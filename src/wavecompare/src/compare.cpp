#include "wave/compare.h"
#include "wave/validation.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace wave {
namespace {

std::string trim(std::string text)
{
    const auto first = std::find_if_not(text.begin(), text.end(), [](const unsigned char character) {
        return std::isspace(character) != 0;
    });
    const auto last = std::find_if_not(text.rbegin(), text.rend(), [](const unsigned char character) {
        return std::isspace(character) != 0;
    }).base();
    return first < last ? std::string(first, last) : std::string{};
}

std::string upper(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](const unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return text;
}

bool checkedAdd(const Tick left, const Tick right, Tick& result)
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

std::optional<std::string> maskBits(
    std::string mask,
    const std::uint32_t width)
{
    mask = trim(std::move(mask));
    if (mask.empty()) return std::string(width, '1');
    Lane lane;
    lane.kind = width > 1 ? LaneKind::Bus : LaneKind::Bit;
    lane.width = width;
    lane.radix = Radix::Binary;
    if (mask.starts_with("0x") || mask.starts_with("0X")) lane.radix = Radix::Hexadecimal;
    return laneValueBits(lane, mask, LaneValueEncoding::ProjectLiteral);
}

bool valuesMatch(
    const Lane& lane,
    const std::string& expected,
    const std::string& actual,
    const CompareRule& rule)
{
    const auto equivalent = rule.enumEquivalence.find(expected);
    if (equivalent != rule.enumEquivalence.end()
        && upper(trim(equivalent->second)) == upper(trim(actual))) {
        return true;
    }
    const auto expectedBits = laneValueBits(
        lane,
        expected,
        LaneValueEncoding::ProjectLiteral);
    const auto actualBits = laneValueBits(
        lane,
        actual,
        LaneValueEncoding::BinaryTrace);
    const auto mask = maskBits(lane.width > 1 ? rule.busMask : std::string{}, lane.width);
    if (!expectedBits || !actualBits || !mask) {
        return upper(trim(expected)) == upper(trim(actual));
    }
    for (std::size_t index = 0; index < expectedBits->size(); ++index) {
        if ((*mask)[index] == '0') continue;
        const auto expectedBit = (*expectedBits)[index];
        const auto actualBit = (*actualBits)[index];
        if (rule.xHandling == XHandling::IgnoreAnyX
            && (expectedBit == 'X' || actualBit == 'X')) {
            continue;
        }
        if (rule.xHandling == XHandling::ExpectedXWildcard && expectedBit == 'X') {
            continue;
        }
        if (expectedBit != actualBit) return false;
    }
    return true;
}

const CompareRule& ruleForLane(
    const CompareOptions& options,
    const std::string& laneId)
{
    const auto iterator = options.laneRules.find(laneId);
    return iterator == options.laneRules.end() ? options.defaultRule : iterator->second;
}

const TraceSignal* mappedSignal(
    const ImportedTrace& reference,
    const TraceIndex& trace,
    const std::string& laneId)
{
    const auto mapping = reference.signalMapping.find(laneId);
    return mapping == reference.signalMapping.end()
        ? nullptr
        : trace.findSignal(mapping->second);
}

std::optional<bool> actualValueEquals(
    const ImportedTrace& reference,
    const TraceIndex& trace,
    const Lane& lane,
    const Tick tick,
    const std::string_view literal,
    std::string& error)
{
    const auto mapping =
        reference.signalMapping.find(lane.id);
    if (mapping == reference.signalMapping.end()) {
        error = "lane '" + lane.name
            + "' has no configured actual signal mapping";
        return std::nullopt;
    }
    const auto* signal =
        trace.findSignal(mapping->second);
    if (!signal) {
        error = "mapped actual signal '"
            + mapping->second + "' for lane '"
            + lane.name
            + "' does not exist in the loaded trace";
        return std::nullopt;
    }
    if (lane.kind != LaneKind::Transaction
        && lane.kind != LaneKind::Event
        && signal->width != lane.width) {
        error = "mapped actual signal for lane '" + lane.name
            + "' has width " + std::to_string(signal->width)
            + ", expected " + std::to_string(lane.width);
        return std::nullopt;
    }
    const auto* sampled = signal->valueAt(tick);
    if (!sampled) {
        error = "lane '" + lane.name + "' has no actual value at source tick "
            + std::to_string(tick);
        return std::nullopt;
    }
    if (lane.kind == LaneKind::Transaction || lane.kind == LaneKind::Event) {
        return sampled->value == literal;
    }

    Lane literalLane = lane;
    if (lane.kind == LaneKind::Clock) literalLane.kind = LaneKind::Bit;
    const auto literalValidation = validateLaneValue(literalLane, literal);
    if (!literalValidation.valid) {
        error = "literal '" + std::string(literal) + "' is invalid for lane '"
            + lane.name + "': " + literalValidation.error;
        return std::nullopt;
    }
    const auto actualBits = laneValueBits(
        lane,
        sampled->value,
        LaneValueEncoding::BinaryTrace);
    const auto literalBits = laneValueBits(
        lane,
        literalValidation.normalizedValue,
        LaneValueEncoding::ProjectLiteral);
    if (!actualBits || !literalBits) {
        error = "actual or literal value for lane '" + lane.name
            + "' cannot be normalized";
        return std::nullopt;
    }
    return *actualBits == *literalBits;
}

bool isDontCareSegment(const Segment& segment)
{
    const auto preset = segment.extensions.find("waveWorkbench.busPreset");
    if (preset == segment.extensions.end()) return false;
    auto value = trim(preset->second);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
    }
    return value == "dont-care";
}

const Segment* expectedSegmentAt(const Lane& lane, const Tick tick)
{
    const auto iterator = std::upper_bound(
        lane.segments.begin(),
        lane.segments.end(),
        tick,
        [](const Tick value, const Segment& segment) {
            return value < segment.start;
        });
    if (iterator == lane.segments.begin()) return nullptr;
    const auto& segment = *std::prev(iterator);
    return segment.start <= tick && tick < segment.end ? &segment : nullptr;
}
std::optional<std::string> expectedValueAt(
    const Project& project,
    const Lane& lane,
    const Tick tick)
{
    if (lane.kind == LaneKind::Clock) {
        const auto* clock = findClock(project, lane.clockDomainId);
        if (!clock || !clock->isValid()) return std::nullopt;
        return std::string(1, clockValueAt(*clock, lane, tick));
    }
    const auto* segment = expectedSegmentAt(lane, tick);
    return segment
        ? std::optional<std::string>{segment->value}
        : std::nullopt;
}

void addClockBoundaries(
    const ClockDomain& clock,
    const Tick start,
    const Tick end,
    std::vector<Tick>& boundaries)
{
    const auto highTicks = clock.period * clock.dutyCycle.numerator
        / clock.dutyCycle.denominator;
    const auto relative = start - clock.phase;
    auto cycle = relative / clock.period;
    if (relative < 0 && relative % clock.period != 0) --cycle;
    --cycle;
    for (;;) {
        if (cycle > 0
            && clock.period > std::numeric_limits<Tick>::max() / cycle) {
            break;
        }
        if (cycle < 0
            && clock.period > std::numeric_limits<Tick>::max() / -cycle) {
            break;
        }
        const auto base = clock.phase + cycle * clock.period;
        Tick falling = 0;
        if (!checkedAdd(base, highTicks, falling)) break;
        if (base > end && falling > end) break;
        if (base > start && base < end) boundaries.push_back(base);
        if (falling > start && falling < end) boundaries.push_back(falling);
        if (cycle == std::numeric_limits<Tick>::max()) break;
        ++cycle;
    }
}

void updateFirstMismatch(CompareResult& result, const Tick tick)
{
    if (!result.firstMismatch || tick < *result.firstMismatch) result.firstMismatch = tick;
}

LaneCompareSummary& laneSummary(
    CompareResult& result,
    const Lane& lane,
    const std::string& signalId)
{
    const auto iterator = std::find_if(
        result.lanes.begin(),
        result.lanes.end(),
        [&lane](const LaneCompareSummary& summary) {
            return summary.laneId == lane.id;
        });
    if (iterator != result.lanes.end()) return *iterator;
    result.lanes.push_back({lane.id, signalId, 0, 0, std::nullopt});
    return result.lanes.back();
}

void appendDifference(
    CompareResult& result,
    LaneCompareSummary& summary,
    CompareDifference difference,
    const std::size_t maximum)
{
    ++summary.differenceCount;
    if (!summary.firstMismatch || difference.start < *summary.firstMismatch) {
        summary.firstMismatch = difference.start;
    }
    if (difference.end > difference.start
        && summary.mismatchDuration <= std::numeric_limits<Tick>::max()
            - (difference.end - difference.start)) {
        summary.mismatchDuration += difference.end - difference.start;
    }
    updateFirstMismatch(result, difference.start);
    if (!result.differences.empty()) {
        auto& previous = result.differences.back();
        if (previous.kind == difference.kind
            && previous.laneId == difference.laneId
            && previous.traceSignalId == difference.traceSignalId
            && previous.end == difference.start
            && previous.expected == difference.expected
            && previous.actual == difference.actual
            && previous.message == difference.message) {
            previous.end = difference.end;
            return;
        }
    }
    if (result.differences.size() >= maximum) {
        result.truncated = true;
        return;
    }
    difference.id = "difference-" + std::to_string(result.differences.size() + 1);
    result.differences.push_back(std::move(difference));
}

std::optional<Tick> nearestMatchingTransition(
    const TraceSignal& signal,
    const Lane& lane,
    const Event& event,
    const CompareRule& rule,
    const Tick start,
    const Tick end)
{
    std::optional<Tick> nearest;
    std::uint64_t nearestDistance = std::numeric_limits<std::uint64_t>::max();
    const auto transitions = signal.visibleTransitions(start, end, false);
    for (const auto& transition : transitions) {
        if (!valuesMatch(lane, event.value, transition.value, rule)) continue;
        const auto distance = transition.tick >= event.tick
            ? static_cast<std::uint64_t>(transition.tick - event.tick)
            : static_cast<std::uint64_t>(event.tick - transition.tick);
        if (!nearest || distance < nearestDistance) {
            nearest = transition.tick;
            nearestDistance = distance;
        }
    }
    return nearest;
}

std::optional<Tick> firstMatchingTransitionAtOrAfter(
    const TraceSignal& signal,
    const Lane& lane,
    const Event& event,
    const CompareRule& rule,
    const Tick start,
    const Tick end)
{
    const auto transitions = signal.visibleTransitions(start, end, false);
    const auto iterator = std::find_if(
        transitions.begin(),
        transitions.end(),
        [&](const TraceTransition& transition) {
            return transition.tick >= start
                && valuesMatch(lane, event.value, transition.value, rule);
        });
    return iterator == transitions.end() ? std::nullopt : std::optional<Tick>{iterator->tick};
}

std::string jsonEscape(const std::string& value)
{
    std::ostringstream output;
    for (const auto character : value) {
        switch (character) {
        case '"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            if (static_cast<unsigned char>(character) < 0x20) {
                output << "\\u"
                       << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<int>(static_cast<unsigned char>(character))
                       << std::dec;
            } else {
                output << character;
            }
        }
    }
    return output.str();
}

std::string csvEscape(const std::string& value)
{
    if (value.find_first_of(",\"\r\n") == std::string::npos) return value;
    std::string escaped{"\""};
    for (const auto character : value) {
        if (character == '"') escaped += '"';
        escaped += character;
    }
    escaped += '"';
    return escaped;
}

std::string htmlEscape(const std::string& value)
{
    std::string escaped;
    for (const auto character : value) {
        switch (character) {
        case '&':
            escaped += "&amp;";
            break;
        case '<':
            escaped += "&lt;";
            break;
        case '>':
            escaped += "&gt;";
            break;
        case '"':
            escaped += "&quot;";
            break;
        default:
            escaped += character;
        }
    }
    return escaped;
}

} // namespace

CompareResult compareScenario(
    const Project& project,
    const Scenario& scenario,
    const TraceIndex& trace,
    const ImportedTrace& reference,
    const CompareOptions& options)
{
    CompareResult result;
    result.projectId = project.id;
    result.scenarioId = scenario.id;
    result.traceId = reference.id;
    result.start = options.start.value_or(0);
    result.end = options.end.value_or(scenario.duration);
    result.traceOffset = reference.offset;
    if (result.end <= result.start) {
        result.diagnostics.push_back("Compare time window is empty.");
        return result;
    }

    if (options.relationOnly) {
        for (const auto& relation : scenario.relations) {
            const auto* sourceEvent = findEvent(scenario, relation.sourceEventId);
            const auto* sourceLane = sourceEvent ? findLane(scenario, sourceEvent->laneId) : nullptr;
            const auto* sourceSignal = sourceLane
                ? mappedSignal(reference, trace, sourceLane->id)
                : nullptr;
            const auto laneId = sourceLane ? sourceLane->id : std::string{};
            auto& summary = sourceLane
                ? laneSummary(result, *sourceLane, sourceSignal ? sourceSignal->id : std::string{})
                : result.lanes.emplace_back();
            if (!sourceEvent || !sourceLane || !sourceSignal) {
                appendDifference(
                    result,
                    summary,
                    {
                        {},
                        CompareDifferenceKind::MissingEvent,
                        laneId,
                        sourceSignal ? sourceSignal->id : std::string{},
                        result.start,
                        result.start,
                        relation.sourceEventId + " -> " + relation.targetEventId,
                        {},
                        "Relation source event, lane, or mapped signal is missing.",
                    },
                    options.maximumDifferences);
                continue;
            }
            const auto& sourceRule = ruleForLane(options, sourceLane->id);
            const auto sourceTick = nearestMatchingTransition(
                *sourceSignal,
                *sourceLane,
                *sourceEvent,
                sourceRule,
                result.start,
                result.end);
            if (!sourceTick) {
                appendDifference(
                    result,
                    summary,
                    {
                        {},
                        CompareDifferenceKind::MissingEvent,
                        sourceLane->id,
                        sourceSignal->id,
                        result.start,
                        result.start,
                        sourceEvent->value,
                        {},
                        "Actual source transition required by relation is missing.",
                    },
                    options.maximumDifferences);
                continue;
            }

            if (!relation.condition.empty()) {
                const auto condition = evaluateRelationCondition(
                    scenario,
                    relation.condition,
                    [&](const Lane& lane, const std::string_view literal, std::string& error) {
                        return actualValueEquals(
                            reference,
                            trace,
                            lane,
                            *sourceTick,
                            literal,
                            error);
                    });
                if (!condition.ok()) {
                    const auto* conditionLane = condition.laneId.empty()
                        ? sourceLane
                        : findLane(scenario, condition.laneId);
                    const auto* conditionSignal = conditionLane
                        ? mappedSignal(reference, trace, conditionLane->id)
                        : nullptr;
                    auto& conditionSummary = conditionLane
                        ? laneSummary(
                            result,
                            *conditionLane,
                            conditionSignal ? conditionSignal->id : std::string{})
                        : summary;
                    appendDifference(
                        result,
                        conditionSummary,
                        {
                            {},
                            CompareDifferenceKind::ConditionEvaluationError,
                            conditionLane ? conditionLane->id : sourceLane->id,
                            conditionSignal ? conditionSignal->id : std::string{},
                            *sourceTick,
                            *sourceTick,
                            relation.condition,
                            condition.error,
                            "Relation condition error at byte "
                                + std::to_string(condition.errorOffset) + ".",
                        },
                        options.maximumDifferences);
                    continue;
                }
                if (!*condition.value) {
                    result.diagnostics.push_back(
                        "Relation '" + relation.id
                        + "' condition is false at actual source tick "
                        + std::to_string(*sourceTick)
                        + "; relation is not applicable.");
                    continue;
                }
            }

            const auto* targetEvent = findEvent(scenario, relation.targetEventId);
            const auto* targetLane = targetEvent
                ? findLane(scenario, targetEvent->laneId)
                : nullptr;
            const auto* targetSignal = targetLane
                ? mappedSignal(reference, trace, targetLane->id)
                : nullptr;
            if (!targetEvent || !targetLane || !targetSignal) {
                appendDifference(
                    result,
                    summary,
                    {
                        {},
                        CompareDifferenceKind::MissingEvent,
                        sourceLane->id,
                        sourceSignal->id,
                        *sourceTick,
                        *sourceTick,
                        relation.targetEventId,
                        {},
                        "Relation target event, lane, or mapped signal is missing.",
                    },
                    options.maximumDifferences);
                continue;
            }
            const auto& targetRule = ruleForLane(options, targetLane->id);
            const auto targetTick = firstMatchingTransitionAtOrAfter(
                *targetSignal,
                *targetLane,
                *targetEvent,
                targetRule,
                *sourceTick,
                result.end);
            if (!targetTick) {
                appendDifference(
                    result,
                    summary,
                    {
                        {},
                        CompareDifferenceKind::MissingEvent,
                        sourceLane->id,
                        sourceSignal->id,
                        *sourceTick,
                        *sourceTick,
                        sourceEvent->value + " -> " + targetEvent->value,
                        {},
                        "Actual target transition required by relation is missing.",
                    },
                    options.maximumDifferences);
                continue;
            }
            const auto delay = *targetTick - *sourceTick;
            if (delay < relation.minimumDelay || delay > relation.maximumDelay) {
                appendDifference(
                    result,
                    summary,
                    {
                        {},
                        CompareDifferenceKind::RelationViolation,
                        sourceLane->id,
                        sourceSignal->id,
                        *sourceTick,
                        *targetTick,
                        std::to_string(relation.minimumDelay) + ".."
                            + std::to_string(relation.maximumDelay) + " tick",
                        std::to_string(delay) + " tick",
                        "Actual relation delay is outside the allowed interval.",
                    },
                    options.maximumDifferences);
            }
        }
        return result;
    }

    for (const auto& lane : scenario.lanes) {
        if (lane.kind == LaneKind::Group
            || lane.kind == LaneKind::Transaction
            || lane.kind == LaneKind::Event) {
            continue;
        }
        const auto mapping =
            reference.signalMapping.find(lane.id);
        if (mapping
            == reference.signalMapping.end()) {
            auto& summary =
                laneSummary(result, lane, {});
            appendDifference(
                result,
                summary,
                {
                    {},
                    CompareDifferenceKind::UnmappedSignal,
                    lane.id,
                    {},
                    result.start,
                    result.end,
                    lane.name,
                    {},
                    "No actual signal mapping is configured for this Lane.",
                },
                options.maximumDifferences);
            continue;
        }
        const auto* signal =
            trace.findSignal(mapping->second);
        auto& summary = laneSummary(
            result, lane, mapping->second);
        if (!signal) {
            appendDifference(
                result,
                summary,
                {
                    {},
                    CompareDifferenceKind::MissingSignal,
                    lane.id,
                    mapping->second,
                    result.start,
                    result.end,
                    lane.name,
                    mapping->second,
                    "Mapped actual signal '"
                        + mapping->second
                        + "' does not exist in the loaded trace.",
                },
                options.maximumDifferences);
            continue;
        }
        if (signal->width != lane.width) {
            appendDifference(
                result,
                summary,
                {
                    {},
                    CompareDifferenceKind::WidthMismatch,
                    lane.id,
                    signal->id,
                    result.start,
                    result.end,
                    std::to_string(lane.width),
                    std::to_string(signal->width),
                    "Expected and actual widths differ.",
                },
                options.maximumDifferences);
            continue;
        }

        std::vector<Tick> boundaries{result.start, result.end};
        if (lane.kind == LaneKind::Clock) {
            const auto* clock = findClock(project, lane.clockDomainId);
            if (clock && clock->isValid()) {
                addClockBoundaries(*clock, result.start, result.end, boundaries);
            }
            const auto begin = std::lower_bound(
                lane.segments.begin(),
                lane.segments.end(),
                result.start,
                [](const Segment& segment, const Tick tick) {
                    return segment.end <= tick;
                });
            for (auto iterator = begin;
                 iterator != lane.segments.end() && iterator->start < result.end;
                 ++iterator) {
                if (iterator->start > result.start) boundaries.push_back(iterator->start);
                if (iterator->end > result.start && iterator->end < result.end) {
                    boundaries.push_back(iterator->end);
                }
            }
        } else {
            const auto begin = std::lower_bound(
                lane.segments.begin(),
                lane.segments.end(),
                result.start,
                [](const Segment& segment, const Tick tick) {
                    return segment.end <= tick;
                });
            for (auto iterator = begin;
                 iterator != lane.segments.end() && iterator->start < result.end;
                 ++iterator) {
                if (iterator->start > result.start) boundaries.push_back(iterator->start);
                if (iterator->end > result.start && iterator->end < result.end) {
                    boundaries.push_back(iterator->end);
                }
            }
        }
        for (const auto& transition : signal->visibleTransitions(result.start, result.end, false)) {
            if (transition.tick > result.start && transition.tick < result.end) {
                boundaries.push_back(transition.tick);
            }
        }
        std::sort(boundaries.begin(), boundaries.end());
        boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());
        const auto& rule = ruleForLane(options, lane.id);
        for (std::size_t index = 0; index + 1 < boundaries.size(); ++index) {
            const auto intervalStart = boundaries[index];
            const auto intervalEnd = boundaries[index + 1];
            const auto* expectedSegment = lane.kind == LaneKind::Bus
                ? expectedSegmentAt(lane, intervalStart)
                : nullptr;
            if (expectedSegment && isDontCareSegment(*expectedSegment)) continue;
            const auto expected = expectedValueAt(project, lane, intervalStart);
            const auto* actualTransition = signal->valueAt(intervalStart);
            if (!expected && !options.compareUndefinedExpected) continue;
            const auto expectedText = expected.value_or("<undefined>");
            const auto actualText = actualTransition
                ? actualTransition->value
                : std::string{"<undefined>"};
            if (expected && actualTransition
                && valuesMatch(lane, *expected, actualTransition->value, rule)) {
                continue;
            }

            bool tolerated = false;
            if (rule.edgeTolerance > 0
                && intervalEnd - intervalStart <= rule.edgeTolerance
                && intervalStart > result.start
                && intervalEnd < result.end) {
                const auto beforeExpected = expectedValueAt(project, lane, intervalStart - 1);
                const auto* beforeActual = signal->valueAt(intervalStart - 1);
                const auto afterExpected = expectedValueAt(project, lane, intervalEnd);
                const auto* afterActual = signal->valueAt(intervalEnd);
                tolerated = beforeExpected && beforeActual && afterExpected && afterActual
                    && valuesMatch(lane, *beforeExpected, beforeActual->value, rule)
                    && valuesMatch(lane, *afterExpected, afterActual->value, rule);
            }
            if (tolerated) {
                ++result.toleratedEdgeCount;
                continue;
            }
            appendDifference(
                result,
                summary,
                {
                    {},
                    CompareDifferenceKind::ValueMismatch,
                    lane.id,
                    signal->id,
                    intervalStart,
                    intervalEnd,
                    expectedText,
                    actualText,
                    "Expected and actual values differ.",
                },
                options.maximumDifferences);
        }
    }
    return result;
}

std::string compareResultJson(const CompareResult& result)
{
    std::ostringstream output;
    output << "{\n"
           << "  \"projectId\": \"" << jsonEscape(result.projectId) << "\",\n"
           << "  \"scenarioId\": \"" << jsonEscape(result.scenarioId) << "\",\n"
           << "  \"traceId\": \"" << jsonEscape(result.traceId) << "\",\n"
           << "  \"startTick\": \"" << result.start << "\",\n"
           << "  \"endTick\": \"" << result.end << "\",\n"
           << "  \"traceOffsetTick\": \"" << result.traceOffset << "\",\n"
           << "  \"firstMismatchTick\": ";
    if (result.firstMismatch) output << '"' << *result.firstMismatch << '"';
    else output << "null";
    output << ",\n"
           << "  \"toleratedEdgeCount\": " << result.toleratedEdgeCount << ",\n"
           << "  \"truncated\": " << (result.truncated ? "true" : "false") << ",\n"
           << "  \"diagnostics\": [";
    for (std::size_t index = 0; index < result.diagnostics.size(); ++index) {
        output << (index == 0 ? "\n" : ",\n")
               << "    \"" << jsonEscape(result.diagnostics[index]) << '"';
    }
    if (!result.diagnostics.empty()) output << '\n';
    output << (result.diagnostics.empty() ? "],\n" : "  ],\n")
           << "  \"differences\": [";
    for (std::size_t index = 0; index < result.differences.size(); ++index) {
        const auto& difference = result.differences[index];
        output << (index == 0 ? "\n" : ",\n")
               << "    {\"id\":\"" << jsonEscape(difference.id)
               << "\",\"kind\":\"" << toString(difference.kind)
               << "\",\"laneId\":\"" << jsonEscape(difference.laneId)
               << "\",\"traceSignalId\":\"" << jsonEscape(difference.traceSignalId)
               << "\",\"startTick\":\"" << difference.start
               << "\",\"endTick\":\"" << difference.end
               << "\",\"expected\":\"" << jsonEscape(difference.expected)
               << "\",\"actual\":\"" << jsonEscape(difference.actual)
               << "\",\"message\":\"" << jsonEscape(difference.message) << "\"}";
    }
    if (!result.differences.empty()) output << '\n';
    output << (result.differences.empty() ? "]\n}\n" : "  ]\n}\n");
    return output.str();
}

std::string compareResultCsv(const CompareResult& result)
{
    std::ostringstream output;
    output << "id,kind,lane_id,trace_signal_id,start_tick,end_tick,expected,actual,message\n";
    for (const auto& difference : result.differences) {
        output << csvEscape(difference.id) << ','
               << toString(difference.kind) << ','
               << csvEscape(difference.laneId) << ','
               << csvEscape(difference.traceSignalId) << ','
               << difference.start << ','
               << difference.end << ','
               << csvEscape(difference.expected) << ','
               << csvEscape(difference.actual) << ','
               << csvEscape(difference.message) << '\n';
    }
    for (const auto& diagnostic : result.diagnostics) {
        output << ",diagnostic,,,,,,,"
               << csvEscape(diagnostic) << '\n';
    }
    return output.str();
}

std::string compareResultHtml(
    const CompareResult& result,
    const Project& project,
    const Scenario& scenario)
{
    std::ostringstream output;
    output << "<!doctype html><html><head><meta charset=\"utf-8\">"
           << "<title>Wave Workbench Compare Report</title>"
           << "<style>body{font:14px system-ui,sans-serif;color:#273142;margin:32px}"
           << "h1{margin:0 0 6px}p{color:#596579}table{border-collapse:collapse;width:100%}"
           << "th,td{border:1px solid #d8dee8;padding:7px;text-align:left}"
           << "th{background:#eef2f7}.ok{color:#16845b}.bad{color:#c62828}</style>"
           << "</head><body><h1>Wave Workbench Compare Report</h1><p>"
           << htmlEscape(project.name) << " / " << htmlEscape(scenario.name)
           << " &middot; trace " << htmlEscape(result.traceId)
           << " &middot; " << result.start << ".." << result.end << " tick</p><h2 class=\""
           << (result.matches() ? "ok\">Match" : "bad\">Differences: " + std::to_string(result.differences.size()))
           << "</h2>";
    if (!result.diagnostics.empty()) {
        output << "<h3>Diagnostics</h3><ul>";
        for (const auto& diagnostic : result.diagnostics) {
            output << "<li>" << htmlEscape(diagnostic) << "</li>";
        }
        output << "</ul>";
    }
    output << "<table><thead><tr><th>Kind</th><th>Lane</th><th>Range</th>"
           << "<th>Expected</th><th>Actual</th><th>Message</th></tr></thead><tbody>";
    for (const auto& difference : result.differences) {
        output << "<tr><td>" << toString(difference.kind)
               << "</td><td>" << htmlEscape(difference.laneId)
               << "</td><td>" << difference.start << ".." << difference.end
               << "</td><td>" << htmlEscape(difference.expected)
               << "</td><td>" << htmlEscape(difference.actual)
               << "</td><td>" << htmlEscape(difference.message) << "</td></tr>";
    }
    output << "</tbody></table></body></html>\n";
    return output.str();
}

std::string_view toString(const XHandling handling) noexcept
{
    switch (handling) {
    case XHandling::Exact:
        return "exact";
    case XHandling::IgnoreAnyX:
        return "ignore-x";
    case XHandling::ExpectedXWildcard:
        return "expected-x-wildcard";
    }
    return "exact";
}

std::string_view toString(const CompareDifferenceKind kind) noexcept
{
    switch (kind) {
    case CompareDifferenceKind::ValueMismatch:
        return "value-mismatch";
    case CompareDifferenceKind::UnmappedSignal:
        return "unmapped-signal";
    case CompareDifferenceKind::MissingSignal:
        return "missing-signal";
    case CompareDifferenceKind::WidthMismatch:
        return "width-mismatch";
    case CompareDifferenceKind::RelationViolation:
        return "relation-violation";
    case CompareDifferenceKind::MissingEvent:
        return "missing-event";
    case CompareDifferenceKind::ConditionEvaluationError:
        return "condition-evaluation-error";
    }
    return "value-mismatch";
}

} // namespace wave
