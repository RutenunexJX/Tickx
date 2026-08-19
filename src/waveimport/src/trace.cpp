#include "wave/trace.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_map>

namespace wave {
namespace {

constexpr std::uint64_t kCancelCheckInterval = 4096;

std::string trim(std::string text)
{
    const auto first = std::find_if_not(text.begin(), text.end(), [](const unsigned char character) {
        return std::isspace(character) != 0;
    });
    const auto last = std::find_if_not(text.rbegin(), text.rend(), [](const unsigned char character) {
        return std::isspace(character) != 0;
    }).base();
    if (first >= last) return {};
    return std::string(first, last);
}

std::string lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return text;
}

std::vector<std::string> splitWhitespace(const std::string& text)
{
    std::istringstream stream(text);
    std::vector<std::string> tokens;
    std::string token;
    while (stream >> token) tokens.push_back(token);
    return tokens;
}

bool parseUnsigned(const std::string_view text, std::uint64_t& value)
{
    if (text.empty()) return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

bool parseSigned(const std::string_view text, std::int64_t& value)
{
    if (text.empty()) return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

bool checkedAdd(const Tick left, const Tick right, Tick& result) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_add_overflow(left, right, &result);
#else
    if ((right > 0 && left > std::numeric_limits<Tick>::max() - right)
        || (right < 0 && left < std::numeric_limits<Tick>::min() - right)) {
        return false;
    }
    result = left + right;
    return true;
#endif
}

bool checkedMultiply(
    const std::uint64_t left,
    const std::uint64_t right,
    std::uint64_t& result) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_mul_overflow(left, right, &result);
#else
    if (right != 0 && left > std::numeric_limits<std::uint64_t>::max() / right) {
        return false;
    }
    result = left * right;
    return true;
#endif
}

void addDiagnostic(
    TraceParseResult& result,
    const TraceDiagnosticSeverity severity,
    const std::size_t line,
    std::string message)
{
    result.diagnostics.push_back({severity, line, std::move(message)});
}

bool hasError(const TraceParseResult& result)
{
    return std::any_of(
        result.diagnostics.begin(),
        result.diagnostics.end(),
        [](const TraceDiagnostic& diagnostic) {
            return diagnostic.severity == TraceDiagnosticSeverity::Error;
        });
}

bool cancelled(const TraceParseOptions& options)
{
    return options.isCancelled && options.isCancelled();
}

void reportProgress(
    const TraceParseOptions& options,
    const std::uint64_t completed,
    const std::uint64_t total)
{
    if (options.progress) options.progress(completed, total);
}

std::string normalizedLogicValue(std::string value)
{
    value = trim(std::move(value));
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        const auto lowered = static_cast<char>(std::tolower(character));
        if (lowered == 'x' || lowered == 'u' || lowered == 'w' || lowered == '-') return 'X';
        if (lowered == 'z') return 'Z';
        if (lowered == 'h') return '1';
        if (lowered == 'l') return '0';
        return static_cast<char>(character);
    });
    return value;
}

void appendTransition(TraceSignal& signal, const Tick tick, std::string value)
{
    value = normalizedLogicValue(std::move(value));
    if (signal.transitions.empty()) {
        signal.transitions.push_back({tick, std::move(value)});
        return;
    }
    auto& last = signal.transitions.back();
    if (last.tick == tick) {
        last.value = std::move(value);
    } else if (last.value != value) {
        signal.transitions.push_back({tick, std::move(value)});
    }
}

std::string joinScope(const std::vector<std::string>& scopes)
{
    std::string joined;
    for (const auto& scope : scopes) {
        if (!joined.empty()) joined += '.';
        joined += scope;
    }
    return joined;
}

std::string uniqueSignalId(
    const std::string& proposed,
    const std::vector<TraceSignal>& traceSignals)
{
    const auto exists = [&traceSignals](const std::string& id) {
        return std::any_of(traceSignals.begin(), traceSignals.end(), [&id](const TraceSignal& signal) {
            return signal.id == id;
        });
    };
    if (!exists(proposed)) return proposed;
    for (std::size_t suffix = 2; ; ++suffix) {
        auto candidate = proposed + "#" + std::to_string(suffix);
        if (!exists(candidate)) return candidate;
    }
}

struct VcdScale {
    std::uint64_t femtosecondsPerUnit{0};
    bool valid{false};
};

VcdScale parseTimescale(
    const std::vector<std::string>& directive,
    TraceParseResult& result,
    const std::size_t line)
{
    std::string combined;
    for (std::size_t index = 1; index < directive.size(); ++index) {
        if (directive[index] == "$end") break;
        combined += directive[index];
    }
    combined = lower(trim(std::move(combined)));
    const auto digitEnd = combined.find_first_not_of("0123456789");
    if (digitEnd == std::string::npos || digitEnd == 0) {
        addDiagnostic(result, TraceDiagnosticSeverity::Error, line, "Invalid VCD $timescale.");
        return {};
    }
    std::uint64_t magnitude = 0;
    if (!parseUnsigned(std::string_view(combined).substr(0, digitEnd), magnitude)
        || (magnitude != 1 && magnitude != 10 && magnitude != 100)) {
        addDiagnostic(
            result,
            TraceDiagnosticSeverity::Error,
            line,
            "VCD timescale magnitude must be 1, 10, or 100.");
        return {};
    }
    const auto unit = combined.substr(digitEnd);
    std::uint64_t unitScale = 0;
    if (unit == "s") unitScale = 1'000'000'000'000'000ULL;
    else if (unit == "ms") unitScale = 1'000'000'000'000ULL;
    else if (unit == "us") unitScale = 1'000'000'000ULL;
    else if (unit == "ns") unitScale = 1'000'000ULL;
    else if (unit == "ps") unitScale = 1'000ULL;
    else if (unit == "fs") unitScale = 1ULL;
    else {
        addDiagnostic(
            result,
            TraceDiagnosticSeverity::Error,
            line,
            "Unsupported VCD timescale unit '" + unit + "'.");
        return {};
    }
    std::uint64_t scale = 0;
    if (!checkedMultiply(magnitude, unitScale, scale)) {
        addDiagnostic(result, TraceDiagnosticSeverity::Error, line, "VCD timescale overflow.");
        return {};
    }
    return {scale, true};
}

std::optional<Tick> convertVcdTime(
    const std::uint64_t raw,
    const VcdScale& scale,
    const TraceParseOptions& options,
    TraceParseResult& result,
    const std::size_t line)
{
    if (!scale.valid || !options.projectTimeBase.isValid()) return std::nullopt;
    std::uint64_t femtoseconds = 0;
    if (!checkedMultiply(raw, scale.femtosecondsPerUnit, femtoseconds)) {
        addDiagnostic(result, TraceDiagnosticSeverity::Error, line, "VCD timestamp overflow.");
        return std::nullopt;
    }
    std::uint64_t femtosecondsPerTick = 0;
    if (!checkedMultiply(
            static_cast<std::uint64_t>(options.projectTimeBase.picosecondsPerTick),
            1'000ULL,
            femtosecondsPerTick)) {
        addDiagnostic(result, TraceDiagnosticSeverity::Error, line, "Project timebase overflow.");
        return std::nullopt;
    }
    if (femtoseconds % femtosecondsPerTick != 0) {
        addDiagnostic(
            result,
            TraceDiagnosticSeverity::Error,
            line,
            "VCD timestamp cannot be represented exactly in the project timebase.");
        return std::nullopt;
    }
    const auto quotient = femtoseconds / femtosecondsPerTick;
    if (quotient > static_cast<std::uint64_t>(std::numeric_limits<Tick>::max())) {
        addDiagnostic(result, TraceDiagnosticSeverity::Error, line, "VCD timestamp exceeds tick range.");
        return std::nullopt;
    }
    Tick shifted = 0;
    if (!checkedAdd(static_cast<Tick>(quotient), options.offset, shifted)) {
        addDiagnostic(result, TraceDiagnosticSeverity::Error, line, "Aligned VCD timestamp overflow.");
        return std::nullopt;
    }
    return shifted;
}

std::vector<std::string> parseCsvRow(const std::string& line, bool& valid)
{
    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;
    valid = true;
    for (std::size_t index = 0; index < line.size(); ++index) {
        const auto character = line[index];
        if (quoted) {
            if (character == '"' && index + 1 < line.size() && line[index + 1] == '"') {
                field += '"';
                ++index;
            } else if (character == '"') {
                quoted = false;
            } else {
                field += character;
            }
        } else if (character == '"') {
            if (!trim(field).empty()) {
                valid = false;
                return {};
            }
            field.clear();
            quoted = true;
        } else if (character == ',') {
            fields.push_back(trim(std::move(field)));
            field.clear();
        } else {
            field += character;
        }
    }
    if (quoted) {
        valid = false;
        return {};
    }
    fields.push_back(trim(std::move(field)));
    return fields;
}

std::optional<CsvTimeUnit> csvUnitFromHeader(std::string text)
{
    text = lower(trim(std::move(text)));
    if (text == "time" || text == "tick" || text == "ticks"
        || text == "time[tick]" || text == "time(tick)" || text == "time_tick") {
        return std::nullopt;
    }
    const std::array<std::pair<std::string_view, CsvTimeUnit>, 4> units{{
        {"ps", CsvTimeUnit::Picosecond},
        {"ns", CsvTimeUnit::Nanosecond},
        {"us", CsvTimeUnit::Microsecond},
        {"ms", CsvTimeUnit::Millisecond},
    }};
    for (const auto& [suffix, unit] : units) {
        if (text == "time[" + std::string(suffix) + "]"
            || text == "time(" + std::string(suffix) + ")"
            || text == "time_" + std::string(suffix)) {
            return unit;
        }
    }
    return std::nullopt;
}

std::optional<Tick> convertCsvTime(
    const std::int64_t raw,
    const CsvTimeUnit unit,
    const TraceParseOptions& options,
    TraceParseResult& result,
    const std::size_t line)
{
    std::optional<Tick> tick;
    switch (unit) {
    case CsvTimeUnit::ProjectTick:
        tick = raw;
        break;
    case CsvTimeUnit::Picosecond:
        tick = toTicks(raw, TimeUnit::Picosecond, options.projectTimeBase);
        break;
    case CsvTimeUnit::Nanosecond:
        tick = toTicks(raw, TimeUnit::Nanosecond, options.projectTimeBase);
        break;
    case CsvTimeUnit::Microsecond:
        tick = toTicks(raw, TimeUnit::Microsecond, options.projectTimeBase);
        break;
    case CsvTimeUnit::Millisecond:
        tick = toTicks(raw, TimeUnit::Millisecond, options.projectTimeBase);
        break;
    }
    if (!tick) {
        addDiagnostic(
            result,
            TraceDiagnosticSeverity::Error,
            line,
            "CSV timestamp cannot be represented exactly in the project timebase.");
        return std::nullopt;
    }
    Tick shifted = 0;
    if (!checkedAdd(*tick, options.offset, shifted)) {
        addDiagnostic(result, TraceDiagnosticSeverity::Error, line, "Aligned CSV timestamp overflow.");
        return std::nullopt;
    }
    return shifted;
}

std::uint32_t inferWidth(const std::string& value)
{
    const auto normalized = lower(trim(value));
    if (normalized.size() > 2 && normalized.starts_with("0b")) {
        return static_cast<std::uint32_t>(normalized.size() - 2);
    }
    if (normalized.size() > 2 && normalized.starts_with("0x")) {
        const auto digits = normalized.size() - 2;
        return static_cast<std::uint32_t>(
            std::min<std::size_t>(digits * 4, std::numeric_limits<std::uint32_t>::max()));
    }
    const auto onlyLogic = !normalized.empty() && std::all_of(
        normalized.begin(),
        normalized.end(),
        [](const char character) {
            return character == '0' || character == '1'
                || character == 'x' || character == 'z';
        });
    return onlyLogic
        ? static_cast<std::uint32_t>(normalized.size())
        : 1U;
}

void finalizeIndex(TraceIndex& index)
{
    bool hasAnyTransition = false;
    Tick minimum = 0;
    Tick maximum = 0;
    index.transitionCount = 0;
    for (auto& signal : index.traceSignals) {
        index.transitionCount += signal.transitions.size();
        if (signal.transitions.empty()) continue;
        if (!hasAnyTransition) {
            minimum = signal.transitions.front().tick;
            maximum = signal.transitions.back().tick;
            hasAnyTransition = true;
        } else {
            minimum = std::min(minimum, signal.transitions.front().tick);
            maximum = std::max(maximum, signal.transitions.back().tick);
        }
    }
    index.startTick = hasAnyTransition ? minimum : 0;
    index.endTick = hasAnyTransition ? maximum : 0;
}

std::string mappingName(std::string text)
{
    text = lower(trim(std::move(text)));
    if (!text.empty() && text.front() == '\\') text.erase(text.begin());
    const auto range = text.find('[');
    if (range != std::string::npos) text.resize(range);
    text.erase(
        std::remove_if(text.begin(), text.end(), [](const unsigned char character) {
            return std::isspace(character) != 0;
        }),
        text.end());
    return text;
}

std::string explicitTraceName(const Lane& lane)
{
    const auto found = lane.extensions.find("waveSimulation.traceName");
    if (found == lane.extensions.end()) return {};
    const auto& encoded = found->second;
    if (encoded.size() < 3 || encoded.front() != '"'
        || encoded.back() != '"') {
        return {};
    }
    const auto value = encoded.substr(1, encoded.size() - 2);
    return std::all_of(value.cbegin(), value.cend(), [](const unsigned char character) {
        return std::isalnum(character) != 0 || character == '_';
    }) ? value : std::string{};
}

} // namespace

std::pair<std::size_t, std::size_t> TraceSignal::visibleRange(
    const Tick start,
    const Tick end,
    const bool includePreceding) const noexcept
{
    if (transitions.empty() || end < start) return {0, 0};
    const auto firstAtOrAfter = std::lower_bound(
        transitions.begin(),
        transitions.end(),
        start,
        [](const TraceTransition& transition, const Tick tick) {
            return transition.tick < tick;
        });
    auto first = static_cast<std::size_t>(firstAtOrAfter - transitions.begin());
    if (includePreceding && first > 0) --first;
    const auto afterEnd = std::upper_bound(
        transitions.begin(),
        transitions.end(),
        end,
        [](const Tick tick, const TraceTransition& transition) {
            return tick < transition.tick;
        });
    const auto last = static_cast<std::size_t>(afterEnd - transitions.begin());
    return {std::min(first, last), last};
}

std::span<const TraceTransition> TraceSignal::visibleTransitions(
    const Tick start,
    const Tick end,
    const bool includePreceding) const noexcept
{
    const auto [first, last] = visibleRange(start, end, includePreceding);
    return std::span<const TraceTransition>(transitions).subspan(first, last - first);
}

const TraceTransition* TraceSignal::valueAt(const Tick tick) const noexcept
{
    const auto iterator = std::upper_bound(
        transitions.begin(),
        transitions.end(),
        tick,
        [](const Tick value, const TraceTransition& transition) {
            return value < transition.tick;
        });
    return iterator == transitions.begin() ? nullptr : &*std::prev(iterator);
}

const TraceSignal* TraceIndex::findSignal(const std::string_view signalId) const noexcept
{
    const auto iterator = std::find_if(
        traceSignals.begin(),
        traceSignals.end(),
        [signalId](const TraceSignal& signal) {
            return signal.id == signalId;
        });
    return iterator == traceSignals.end() ? nullptr : &*iterator;
}

TraceSignal* TraceIndex::findSignal(const std::string_view signalId) noexcept
{
    return const_cast<TraceSignal*>(
        static_cast<const TraceIndex&>(*this).findSignal(signalId));
}

bool TraceIndex::shift(const Tick delta) noexcept
{
    Tick shiftedStart = 0;
    Tick shiftedEnd = 0;
    if (!checkedAdd(startTick, delta, shiftedStart)
        || !checkedAdd(endTick, delta, shiftedEnd)) {
        return false;
    }
    for (const auto& signal : traceSignals) {
        for (const auto& transition : signal.transitions) {
            Tick shifted = 0;
            if (!checkedAdd(transition.tick, delta, shifted)) return false;
        }
    }
    startTick = shiftedStart;
    endTick = shiftedEnd;
    for (auto& signal : traceSignals) {
        for (auto& transition : signal.transitions) transition.tick += delta;
    }
    return true;
}

bool TraceParseResult::ok() const noexcept
{
    return index.has_value() && !cancelled && !hasError(*this);
}

std::string TraceParseResult::errorSummary() const
{
    std::string summary;
    for (const auto& diagnostic : diagnostics) {
        if (diagnostic.severity != TraceDiagnosticSeverity::Error) continue;
        if (!summary.empty()) summary += '\n';
        if (diagnostic.line != 0) {
            summary += "line " + std::to_string(diagnostic.line) + ": ";
        }
        summary += diagnostic.message;
    }
    return summary;
}

TraceParseResult parseVcd(std::istream& input, const TraceParseOptions& options)
{
    TraceParseResult result;
    if (!options.projectTimeBase.isValid()) {
        addDiagnostic(result, TraceDiagnosticSeverity::Error, 0, "Invalid project timebase.");
        return result;
    }

    TraceIndex index;
    index.identity = options.identity;
    index.format = TraceFormat::Vcd;
    index.projectTimeBase = options.projectTimeBase;
    std::vector<std::string> scopes;
    std::unordered_map<std::string, std::vector<std::size_t>> signalsByCode;
    VcdScale scale;
    Tick currentTick = options.offset;
    bool endDefinitions = false;
    bool skippingDirective = false;
    std::size_t lineNumber = 0;
    std::uint64_t processed = 0;
    std::string line;
    while (std::getline(input, line)) {
        ++lineNumber;
        ++processed;
        if ((processed % kCancelCheckInterval) == 0) {
            if (cancelled(options)) {
                result.cancelled = true;
                return result;
            }
            reportProgress(options, processed, 0);
        }
        line = trim(std::move(line));
        if (line.empty()) continue;

        if (!endDefinitions && line.front() == '$') {
            auto directiveText = line;
            while (directiveText.find("$end") == std::string::npos
                   && std::getline(input, line)) {
                ++lineNumber;
                directiveText += ' ';
                directiveText += trim(std::move(line));
            }
            const auto tokens = splitWhitespace(directiveText);
            if (tokens.empty()) continue;
            if (tokens.front() == "$timescale") {
                scale = parseTimescale(tokens, result, lineNumber);
            } else if (tokens.front() == "$scope") {
                if (tokens.size() < 4) {
                    addDiagnostic(
                        result,
                        TraceDiagnosticSeverity::Error,
                        lineNumber,
                        "Malformed VCD $scope.");
                } else {
                    scopes.push_back(tokens[2]);
                }
            } else if (tokens.front() == "$upscope") {
                if (scopes.empty()) {
                    addDiagnostic(
                        result,
                        TraceDiagnosticSeverity::Warning,
                        lineNumber,
                        "VCD $upscope has no matching $scope.");
                } else {
                    scopes.pop_back();
                }
            } else if (tokens.front() == "$var") {
                if (tokens.size() < 6) {
                    addDiagnostic(
                        result,
                        TraceDiagnosticSeverity::Error,
                        lineNumber,
                        "Malformed VCD $var.");
                    continue;
                }
                std::uint64_t parsedWidth = 0;
                if (!parseUnsigned(tokens[2], parsedWidth)
                    || parsedWidth == 0
                    || parsedWidth > std::numeric_limits<std::uint32_t>::max()) {
                    addDiagnostic(
                        result,
                        TraceDiagnosticSeverity::Error,
                        lineNumber,
                        "Invalid VCD signal width.");
                    continue;
                }
                TraceSignal signal;
                signal.identifierCode = tokens[3];
                signal.reference = tokens[4];
                if (tokens.size() > 6 && tokens[5] != "$end") {
                    signal.reference += tokens[5];
                }
                signal.scope = joinScope(scopes);
                signal.scopePath = scopes;
                signal.fullName = signal.scope.empty()
                    ? signal.reference
                    : signal.scope + "." + signal.reference;
                signal.id = uniqueSignalId(signal.fullName, index.traceSignals);
                signal.width = static_cast<std::uint32_t>(parsedWidth);
                const auto signalIndex = index.traceSignals.size();
                index.traceSignals.push_back(std::move(signal));
                signalsByCode[tokens[3]].push_back(signalIndex);
            } else if (tokens.front() == "$enddefinitions") {
                endDefinitions = true;
            }
            continue;
        }

        if (!endDefinitions) continue;
        if (line.front() == '$') {
            const auto directive = lower(line);
            if (directive.starts_with("$dumpvars")
                || directive.starts_with("$dumpall")
                || directive.starts_with("$dumpon")
                || directive.starts_with("$dumpoff")
                || directive.starts_with("$end")) {
                skippingDirective = false;
            } else {
                skippingDirective = line.find("$end") == std::string::npos;
            }
            continue;
        }
        if (skippingDirective) {
            if (line.find("$end") != std::string::npos) skippingDirective = false;
            continue;
        }
        if (line.front() == '#') {
            std::uint64_t rawTime = 0;
            if (!parseUnsigned(std::string_view(line).substr(1), rawTime)) {
                addDiagnostic(result, TraceDiagnosticSeverity::Error, lineNumber, "Invalid VCD timestamp.");
                break;
            }
            const auto converted = convertVcdTime(rawTime, scale, options, result, lineNumber);
            if (!converted) break;
            currentTick = *converted;
            continue;
        }

        std::string code;
        std::string value;
        const auto kind = static_cast<char>(std::tolower(static_cast<unsigned char>(line.front())));
        if (kind == 'b') {
            const auto parts = splitWhitespace(line.substr(1));
            if (parts.size() < 2) {
                addDiagnostic(result, TraceDiagnosticSeverity::Warning, lineNumber, "Malformed VCD vector value.");
                continue;
            }
            value = parts[0];
            code = parts[1];
        } else if (kind == 'r' || kind == 's') {
            addDiagnostic(
                result,
                TraceDiagnosticSeverity::Warning,
                lineNumber,
                "Real/string VCD value ignored by the digital trace importer.");
            continue;
        } else {
            value.assign(1, line.front());
            code = trim(line.substr(1));
        }
        const auto targets = signalsByCode.find(code);
        if (targets == signalsByCode.end()) {
            addDiagnostic(
                result,
                TraceDiagnosticSeverity::Warning,
                lineNumber,
                "VCD value references undeclared identifier code '" + code + "'.");
            continue;
        }
        for (const auto signalIndex : targets->second) {
            appendTransition(index.traceSignals[signalIndex], currentTick, value);
        }
    }

    if (cancelled(options)) {
        result.cancelled = true;
        return result;
    }
    if (!scale.valid) {
        addDiagnostic(result, TraceDiagnosticSeverity::Error, 0, "VCD contains no valid $timescale.");
    }
    if (!endDefinitions) {
        addDiagnostic(result, TraceDiagnosticSeverity::Error, 0, "VCD contains no $enddefinitions.");
    }
    if (index.traceSignals.empty()) {
        addDiagnostic(result, TraceDiagnosticSeverity::Error, 0, "VCD contains no digital signals.");
    }
    if (hasError(result)) return result;
    finalizeIndex(index);
    reportProgress(options, processed, processed);
    result.index = std::move(index);
    return result;
}

TraceParseResult parseCsv(std::istream& input, const TraceParseOptions& options)
{
    TraceParseResult result;
    if (!options.projectTimeBase.isValid()) {
        addDiagnostic(result, TraceDiagnosticSeverity::Error, 0, "Invalid project timebase.");
        return result;
    }
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        line = trim(std::move(line));
        if (!line.empty() && !line.starts_with('#')) break;
    }
    if (line.empty()) {
        addDiagnostic(result, TraceDiagnosticSeverity::Error, lineNumber, "CSV is empty.");
        return result;
    }
    bool validRow = false;
    const auto header = parseCsvRow(line, validRow);
    if (!validRow || header.size() < 2) {
        addDiagnostic(
            result,
            TraceDiagnosticSeverity::Error,
            lineNumber,
            "CSV header must contain time and at least one signal column.");
        return result;
    }

    TraceIndex index;
    index.identity = options.identity;
    index.format = TraceFormat::Csv;
    index.projectTimeBase = options.projectTimeBase;
    for (std::size_t column = 1; column < header.size(); ++column) {
        if (header[column].empty()) {
            addDiagnostic(result, TraceDiagnosticSeverity::Error, lineNumber, "CSV signal name is empty.");
            return result;
        }
        TraceSignal signal;
        signal.id = uniqueSignalId(header[column], index.traceSignals);
        signal.reference = header[column];
        signal.fullName = header[column];
        index.traceSignals.push_back(std::move(signal));
    }
    auto csvTimeUnit = options.csvTimeUnit;
    if (const auto explicitUnit = csvUnitFromHeader(header.front())) {
        csvTimeUnit = *explicitUnit;
    } else {
        const auto normalizedHeader = lower(trim(header.front()));
        const auto known = normalizedHeader == "time"
            || normalizedHeader == "tick"
            || normalizedHeader == "ticks"
            || normalizedHeader == "time[tick]"
            || normalizedHeader == "time(tick)"
            || normalizedHeader == "time_tick";
        if (!known) {
            addDiagnostic(
                result,
                TraceDiagnosticSeverity::Error,
                lineNumber,
                "First CSV column must be time, ticks, or time with an explicit ps/ns/us/ms unit.");
            return result;
        }
    }

    std::uint64_t processed = 0;
    Tick previousTick = std::numeric_limits<Tick>::min();
    while (std::getline(input, line)) {
        ++lineNumber;
        ++processed;
        if ((processed % kCancelCheckInterval) == 0) {
            if (cancelled(options)) {
                result.cancelled = true;
                return result;
            }
            reportProgress(options, processed, 0);
        }
        line = trim(std::move(line));
        if (line.empty() || line.starts_with('#')) continue;
        const auto fields = parseCsvRow(line, validRow);
        if (!validRow || fields.size() != header.size()) {
            addDiagnostic(
                result,
                TraceDiagnosticSeverity::Error,
                lineNumber,
                "CSV row has a different column count from the header.");
            break;
        }
        std::int64_t rawTime = 0;
        if (!parseSigned(fields.front(), rawTime)) {
            addDiagnostic(result, TraceDiagnosticSeverity::Error, lineNumber, "CSV time must be an integer.");
            break;
        }
        const auto tick = convertCsvTime(rawTime, csvTimeUnit, options, result, lineNumber);
        if (!tick) break;
        if (*tick < previousTick) {
            addDiagnostic(
                result,
                TraceDiagnosticSeverity::Error,
                lineNumber,
                "CSV timestamps must be non-decreasing.");
            break;
        }
        previousTick = *tick;
        for (std::size_t column = 1; column < fields.size(); ++column) {
            if (fields[column].empty()) continue;
            auto& signal = index.traceSignals[column - 1];
            signal.width = std::max(signal.width, inferWidth(fields[column]));
            appendTransition(signal, *tick, fields[column]);
        }
    }

    if (cancelled(options)) {
        result.cancelled = true;
        return result;
    }
    if (hasError(result)) return result;
    finalizeIndex(index);
    reportProgress(options, processed, processed);
    result.index = std::move(index);
    return result;
}

TraceParseResult parseVcdFile(
    const std::filesystem::path& path,
    const TraceParseOptions& options)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        TraceParseResult result;
        addDiagnostic(
            result,
            TraceDiagnosticSeverity::Error,
            0,
            "Cannot open VCD file '" + path.string() + "'.");
        return result;
    }
    return parseVcd(input, options);
}

TraceParseResult parseCsvFile(
    const std::filesystem::path& path,
    const TraceParseOptions& options)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        TraceParseResult result;
        addDiagnostic(
            result,
            TraceDiagnosticSeverity::Error,
            0,
            "Cannot open CSV file '" + path.string() + "'.");
        return result;
    }
    return parseCsv(input, options);
}

std::map<std::string, std::string> suggestSignalMapping(
    const Scenario& scenario,
    const TraceIndex& trace)
{
    std::map<std::string, std::string> mapping;
    for (const auto& lane : scenario.lanes) {
        if (lane.kind == LaneKind::Group) continue;
        const auto explicitName = explicitTraceName(lane);
        const auto laneName = mappingName(
            explicitName.empty() ? lane.name : explicitName);
        std::vector<const TraceSignal*> candidates;
        for (const auto& signal : trace.traceSignals) {
            if (mappingName(signal.reference) == laneName) candidates.push_back(&signal);
        }
        if (candidates.size() == 1) {
            mapping.emplace(lane.id, candidates.front()->id);
            continue;
        }
        candidates.clear();
        for (const auto& signal : trace.traceSignals) {
            const auto full = mappingName(signal.fullName);
            if (full == laneName
                || (full.size() > laneName.size()
                    && full.ends_with("." + laneName))) {
                candidates.push_back(&signal);
            }
        }
        if (candidates.size() == 1) mapping.emplace(lane.id, candidates.front()->id);
    }
    return mapping;
}

std::string_view toString(const TraceFormat format) noexcept
{
    switch (format) {
    case TraceFormat::Vcd:
        return "vcd";
    case TraceFormat::Csv:
        return "csv";
    }
    return "unknown";
}

} // namespace wave
