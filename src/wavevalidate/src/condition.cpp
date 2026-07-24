#include "wave/validation.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace wave {
namespace {

enum class TokenKind {
    End,
    Word,
    String,
    LaneReference,
    True,
    False,
    LeftParenthesis,
    RightParenthesis,
    Not,
    And,
    Or,
    Equal,
    NotEqual,
    Invalid,
};

struct Token {
    TokenKind kind{TokenKind::End};
    std::string text;
    std::size_t offset{0};
};

std::string lowerCopy(std::string text)
{
    std::transform(
        text.begin(),
        text.end(),
        text.begin(),
        [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return text;
}

class Lexer {
public:
    explicit Lexer(const std::string_view expression)
        : expression_(expression)
    {
    }

    [[nodiscard]] Token next()
    {
        while (position_ < expression_.size()
               && std::isspace(static_cast<unsigned char>(expression_[position_])) != 0) {
            ++position_;
        }
        if (position_ == expression_.size()) {
            return {TokenKind::End, {}, position_};
        }

        const auto offset = position_;
        const auto character = expression_[position_++];
        switch (character) {
        case '(':
            return {TokenKind::LeftParenthesis, "(", offset};
        case ')':
            return {TokenKind::RightParenthesis, ")", offset};
        case '!':
            if (consume('=')) {
                (void)consume('=');
                return {TokenKind::NotEqual, "!=", offset};
            }
            return {TokenKind::Not, "!", offset};
        case '&':
            return consume('&')
                ? Token{TokenKind::And, "&&", offset}
                : invalid("expected '&' after '&'", offset);
        case '|':
            return consume('|')
                ? Token{TokenKind::Or, "||", offset}
                : invalid("expected '|' after '|'", offset);
        case '=':
            if (!consume('=')) {
                return invalid("expected '=' after '='", offset);
            }
            (void)consume('=');
            return {TokenKind::Equal, "==", offset};
        case '"':
        case '\'':
            return quoted(TokenKind::String, character, offset);
        case '`':
            return quoted(TokenKind::LaneReference, character, offset);
        default:
            break;
        }

        while (position_ < expression_.size()) {
            const auto nextCharacter = expression_[position_];
            if (std::isspace(static_cast<unsigned char>(nextCharacter)) != 0
                || nextCharacter == '('
                || nextCharacter == ')'
                || nextCharacter == '!'
                || nextCharacter == '&'
                || nextCharacter == '|'
                || nextCharacter == '='
                || nextCharacter == '`') {
                break;
            }
            ++position_;
        }
        const auto text = std::string(expression_.substr(offset, position_ - offset));
        const auto keyword = lowerCopy(text);
        if (keyword == "true") return {TokenKind::True, text, offset};
        if (keyword == "false") return {TokenKind::False, text, offset};
        return {TokenKind::Word, text, offset};
    }

private:
    [[nodiscard]] bool consume(const char expected)
    {
        if (position_ >= expression_.size() || expression_[position_] != expected) {
            return false;
        }
        ++position_;
        return true;
    }

    [[nodiscard]] Token quoted(
        const TokenKind kind,
        const char delimiter,
        const std::size_t offset)
    {
        std::string text;
        while (position_ < expression_.size()) {
            const auto character = expression_[position_++];
            if (character == delimiter) return {kind, std::move(text), offset};
            if (character == '\\') {
                if (position_ == expression_.size()) {
                    return invalid("unterminated escape sequence", offset);
                }
                const auto escaped = expression_[position_++];
                switch (escaped) {
                case 'n':
                    text.push_back('\n');
                    break;
                case 'r':
                    text.push_back('\r');
                    break;
                case 't':
                    text.push_back('\t');
                    break;
                default:
                    text.push_back(escaped);
                    break;
                }
            } else {
                text.push_back(character);
            }
        }
        return invalid(
            kind == TokenKind::LaneReference
                ? "unterminated lane reference"
                : "unterminated string literal",
            offset);
    }

    [[nodiscard]] static Token invalid(
        std::string message,
        const std::size_t offset)
    {
        return {TokenKind::Invalid, std::move(message), offset};
    }

    std::string_view expression_;
    std::size_t position_{0};
};

bool isBooleanLane(const Lane& lane) noexcept
{
    return lane.width == 1
        && (lane.kind == LaneKind::Bit
            || lane.kind == LaneKind::Bus
            || lane.kind == LaneKind::Enum
            || lane.kind == LaneKind::Clock);
}

class Parser {
public:
    Parser(
        const Scenario& scenario,
        const std::string_view expression,
        const RelationConditionPredicate& predicate)
        : scenario_(scenario)
        , lexer_(expression)
        , predicate_(predicate)
    {
        advance();
    }

    [[nodiscard]] RelationConditionEvaluation parse()
    {
        if (current_.kind == TokenKind::End) {
            evaluation_.value = true;
            return evaluation_;
        }
        const auto value = parseOr();
        if (!value) return evaluation_;
        if (current_.kind != TokenKind::End) {
            fail(current_.offset, "unexpected token '" + current_.text + "'");
            return evaluation_;
        }
        evaluation_.value = *value;
        return evaluation_;
    }

private:
    void advance()
    {
        current_ = lexer_.next();
        if (current_.kind == TokenKind::Invalid) {
            fail(current_.offset, current_.text);
        }
    }

    void fail(
        const std::size_t offset,
        std::string message,
        std::string laneId = {})
    {
        if (!evaluation_.error.empty()) return;
        evaluation_.errorOffset = offset;
        evaluation_.error = std::move(message);
        evaluation_.laneId = std::move(laneId);
    }

    [[nodiscard]] std::optional<bool> parseOr()
    {
        auto value = parseAnd();
        while (value && current_.kind == TokenKind::Or) {
            advance();
            const auto right = parseAnd();
            if (!right) return std::nullopt;
            *value = *value || *right;
        }
        return value;
    }

    [[nodiscard]] std::optional<bool> parseAnd()
    {
        auto value = parseUnary();
        while (value && current_.kind == TokenKind::And) {
            advance();
            const auto right = parseUnary();
            if (!right) return std::nullopt;
            *value = *value && *right;
        }
        return value;
    }

    [[nodiscard]] std::optional<bool> parseUnary()
    {
        if (!evaluation_.error.empty()) return std::nullopt;
        if (current_.kind == TokenKind::Not) {
            advance();
            const auto value = parseUnary();
            return value ? std::optional<bool>{!*value} : std::nullopt;
        }
        return parsePrimary();
    }

    [[nodiscard]] std::optional<bool> parsePrimary()
    {
        if (!evaluation_.error.empty()) return std::nullopt;
        if (current_.kind == TokenKind::True) {
            advance();
            return true;
        }
        if (current_.kind == TokenKind::False) {
            advance();
            return false;
        }
        if (current_.kind == TokenKind::LeftParenthesis) {
            const auto offset = current_.offset;
            advance();
            const auto value = parseOr();
            if (!value) return std::nullopt;
            if (current_.kind != TokenKind::RightParenthesis) {
                fail(offset, "missing closing parenthesis");
                return std::nullopt;
            }
            advance();
            return value;
        }
        if (current_.kind != TokenKind::Word
            && current_.kind != TokenKind::LaneReference) {
            fail(current_.offset, "expected a lane reference, boolean, or parenthesized expression");
            return std::nullopt;
        }

        const auto laneReference = current_.text;
        const auto laneOffset = current_.offset;
        advance();
        const auto* lane = resolveLane(laneReference, laneOffset);
        if (!lane) return std::nullopt;
        if (lane->kind == LaneKind::Group) {
            fail(
                laneOffset,
                "group lane '" + laneReference + "' has no sample value",
                lane->id);
            return std::nullopt;
        }

        bool negate = false;
        std::string literal;
        if (current_.kind == TokenKind::Equal
            || current_.kind == TokenKind::NotEqual) {
            negate = current_.kind == TokenKind::NotEqual;
            advance();
            switch (current_.kind) {
            case TokenKind::Word:
            case TokenKind::String:
            case TokenKind::LaneReference:
                literal = current_.text;
                break;
            case TokenKind::True:
                literal = "1";
                break;
            case TokenKind::False:
                literal = "0";
                break;
            default:
                fail(current_.offset, "expected a value literal after comparison operator");
                return std::nullopt;
            }
            advance();
        } else {
            if (!isBooleanLane(*lane)) {
                fail(
                    laneOffset,
                    "bare lane reference '" + laneReference
                        + "' requires a one-bit digital lane",
                    lane->id);
                return std::nullopt;
            }
            literal = "1";
        }

        if (!predicate_) {
            fail(laneOffset, "condition value predicate is unavailable", lane->id);
            return std::nullopt;
        }
        std::string predicateError;
        const auto equal = predicate_(*lane, literal, predicateError);
        if (!equal) {
            fail(
                laneOffset,
                predicateError.empty()
                    ? "could not sample lane '" + laneReference + "'"
                    : std::move(predicateError),
                lane->id);
            return std::nullopt;
        }
        return negate ? !*equal : *equal;
    }

    [[nodiscard]] const Lane* resolveLane(
        const std::string& reference,
        const std::size_t offset)
    {
        const Lane* match = nullptr;
        std::size_t count = 0;
        for (const auto& lane : scenario_.lanes) {
            if (lane.id != reference) continue;
            match = &lane;
            ++count;
        }
        if (count == 1) return match;
        if (count > 1) {
            fail(offset, "lane ID '" + reference + "' is not unique");
            return nullptr;
        }

        for (const auto& lane : scenario_.lanes) {
            if (lane.name != reference) continue;
            match = &lane;
            ++count;
        }
        if (count == 1) return match;
        if (count > 1) {
            fail(offset, "lane display name '" + reference + "' is ambiguous");
            return nullptr;
        }
        fail(offset, "lane '" + reference + "' does not exist");
        return nullptr;
    }

    const Scenario& scenario_;
    Lexer lexer_;
    const RelationConditionPredicate& predicate_;
    Token current_;
    RelationConditionEvaluation evaluation_;
};

} // namespace

RelationConditionEvaluation evaluateRelationCondition(
    const Scenario& scenario,
    const std::string_view expression,
    const RelationConditionPredicate& predicate)
{
    return Parser(scenario, expression, predicate).parse();
}

} // namespace wave
