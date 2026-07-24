#include "wave/generation.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace wave {
namespace {

bool hasErrors(const std::vector<GenerationDiagnostic>& diagnostics)
{
    return std::any_of(
        diagnostics.begin(),
        diagnostics.end(),
        [](const GenerationDiagnostic& diagnostic) {
            return diagnostic.severity == Severity::Error;
        });
}

bool checkedMultiply(
    const std::int64_t left,
    const std::int64_t right,
    std::int64_t& result)
{
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_mul_overflow(left, right, &result);
#else
    if (left == 0 || right == 0) {
        result = 0;
        return true;
    }
    if (left == -1 && right == std::numeric_limits<std::int64_t>::min()) return false;
    if (right == -1 && left == std::numeric_limits<std::int64_t>::min()) return false;
    if (left > 0) {
        if ((right > 0 && left > std::numeric_limits<std::int64_t>::max() / right)
            || (right < 0 && right < std::numeric_limits<std::int64_t>::min() / left)) {
            return false;
        }
    } else if (left < 0) {
        if ((right > 0 && left < std::numeric_limits<std::int64_t>::min() / right)
            || (right < 0 && left < std::numeric_limits<std::int64_t>::max() / right)) {
            return false;
        }
    }
    result = left * right;
    return true;
#endif
}

std::optional<std::int64_t> toPicoseconds(
    const Tick tick,
    const TimeBase& timeBase)
{
    std::int64_t result = 0;
    if (!timeBase.isValid()
        || !checkedMultiply(tick, timeBase.picosecondsPerTick, result)) {
        return std::nullopt;
    }
    return result;
}

const GenerationSignal* findSignal(
    const GenerationPlan& plan,
    const std::string_view laneId)
{
    const auto iterator = std::find_if(
        plan.signalDefinitions.begin(),
        plan.signalDefinitions.end(),
        [laneId](const GenerationSignal& signal) {
            return signal.laneId == laneId;
        });
    return iterator == plan.signalDefinitions.end() ? nullptr : &*iterator;
}

const ClockDomain* findPlanClock(
    const GenerationPlan& plan,
    const std::string_view clockId)
{
    const auto iterator = std::find_if(
        plan.clocks.begin(),
        plan.clocks.end(),
        [clockId](const ClockDomain& clock) {
            return clock.id == clockId;
        });
    return iterator == plan.clocks.end() ? nullptr : &*iterator;
}

std::string clockInternalBase(const GenerationSignal& signal)
{
    return "__ww_clock_" + sanitizeIdentifier(signal.clockDomainId);
}

std::string clockRawIdentifier(const GenerationSignal& signal)
{
    return clockInternalBase(signal) + "_raw";
}

std::string clockGateIdentifier(const GenerationSignal& signal)
{
    return clockInternalBase(signal) + "_gated";
}

std::string clockDisableIdentifier(const GenerationSignal& signal)
{
    return clockInternalBase(signal) + "_disabled";
}

const GenerationStep* findStep(
    const GenerationPlan& plan,
    const std::string_view eventId)
{
    const auto iterator = std::find_if(
        plan.steps.begin(),
        plan.steps.end(),
        [eventId](const GenerationStep& step) {
            return step.eventId == eventId;
        });
    return iterator == plan.steps.end() ? nullptr : &*iterator;
}

std::string escapeComment(std::string text)
{
    std::replace(text.begin(), text.end(), '\n', ' ');
    std::replace(text.begin(), text.end(), '\r', ' ');
    return text;
}

std::string escapePython(std::string_view text)
{
    std::string escaped;
    escaped.reserve(text.size() + 8);
    for (const auto character : text) {
        switch (character) {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        default: escaped.push_back(character); break;
        }
    }
    return escaped;
}

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string removeSeparators(std::string value)
{
    value.erase(
        std::remove_if(
            value.begin(),
            value.end(),
            [](const unsigned char character) {
                return character == '_' || std::isspace(character);
            }),
        value.end());
    return value;
}

std::string mappedValue(
    const GenerationSignal& signal,
    const std::string& input)
{
    if (signal.kind == LaneKind::Enum) {
        const auto iterator = signal.enumMap.find(input);
        if (iterator != signal.enumMap.end()) return iterator->second;
    }
    return input;
}

std::optional<std::string> systemVerilogLiteral(
    const GenerationSignal& signal,
    const std::string& input)
{
    auto value = removeSeparators(mappedValue(signal, input));
    if (value.empty()) return std::nullopt;
    auto lower = lowerCopy(value);
    if (signal.kind == LaneKind::Bit) {
        if (lower == "0" || lower == "1" || lower == "x" || lower == "z") {
            return "1'b" + lower;
        }
        return std::nullopt;
    }

    const auto width = std::to_string(signal.width);
    const auto signedMarker = signal.isSigned ? "s" : "";
    if (lower.starts_with("0x")) {
        return width + "'" + signedMarker + "h" + lower.substr(2);
    }
    if (lower.starts_with("0b")) {
        return width + "'" + signedMarker + "b" + lower.substr(2);
    }
    if (lower.starts_with("0o")) {
        return width + "'" + signedMarker + "o" + lower.substr(2);
    }
    if (lower.front() == '-') {
        if (!signal.isSigned || lower.size() == 1) return std::nullopt;
        return "-" + width + "'sd" + lower.substr(1);
    }
    if (std::all_of(lower.begin(), lower.end(), [](const unsigned char character) {
            return std::isdigit(character);
        })) {
        return width + "'" + signedMarker + "d" + lower;
    }
    if (std::all_of(lower.begin(), lower.end(), [](const unsigned char character) {
            return character == '0' || character == '1'
                || character == 'x' || character == 'z';
        })) {
        return width + "'" + signedMarker + "b" + lower;
    }
    return std::nullopt;
}

std::optional<std::string> cocotbDriveLiteral(
    const GenerationSignal& signal,
    const std::string& input)
{
    auto value = removeSeparators(mappedValue(signal, input));
    if (value.empty()) return std::nullopt;
    auto lower = lowerCopy(value);
    const auto expandRadixPattern = [](const std::string_view digits, const int bitsPerDigit)
        -> std::optional<std::string> {
        std::string expanded;
        expanded.reserve(digits.size() * static_cast<std::size_t>(bitsPerDigit));
        for (const auto character : digits) {
            if (character == 'x' || character == 'z') {
                expanded.append(static_cast<std::size_t>(bitsPerDigit), character);
                continue;
            }
            const auto numeric = character >= '0' && character <= '9'
                ? character - '0'
                : character >= 'a' && character <= 'f'
                    ? character - 'a' + 10
                    : -1;
            if (numeric < 0 || numeric >= (1 << bitsPerDigit)) return std::nullopt;
            for (auto bit = bitsPerDigit - 1; bit >= 0; --bit) {
                expanded.push_back((numeric & (1 << bit)) != 0 ? '1' : '0');
            }
        }
        return expanded;
    };
    if (lower.starts_with("0b")) {
        const auto digits = lower.substr(2);
        if (digits.find('x') != std::string::npos || digits.find('z') != std::string::npos) {
            return "\"" + digits + "\"";
        }
        return "0b" + digits;
    }
    if (lower.starts_with("0x")) {
        const auto digits = lower.substr(2);
        if (digits.find('x') != std::string::npos || digits.find('z') != std::string::npos) {
            const auto expanded = expandRadixPattern(digits, 4);
            return expanded ? std::optional<std::string>{"\"" + *expanded + "\""} : std::nullopt;
        }
        return "0x" + digits;
    }
    if (lower.starts_with("0o")) {
        const auto digits = lower.substr(2);
        if (digits.find('x') != std::string::npos || digits.find('z') != std::string::npos) {
            const auto expanded = expandRadixPattern(digits, 3);
            return expanded ? std::optional<std::string>{"\"" + *expanded + "\""} : std::nullopt;
        }
        return "0o" + digits;
    }
    if (lower.find('x') != std::string::npos || lower.find('z') != std::string::npos) {
        return "\"" + lower + "\"";
    }
    if (signal.kind == LaneKind::Bit && (lower == "0" || lower == "1")) {
        return lower;
    }
    if (lower.front() == '-' || std::isdigit(static_cast<unsigned char>(lower.front()))) {
        return lower;
    }
    return std::nullopt;
}

std::string stepLabel(const GenerationStep& step, const std::size_t index)
{
    return sanitizeIdentifier(
        step.eventId.empty() ? "step_" + std::to_string(index) : step.eventId);
}

void appendDiagnostic(
    std::vector<GenerationDiagnostic>& diagnostics,
    const GenerationDiagnosticCode code,
    const Severity severity,
    std::string message,
    std::string objectId)
{
    diagnostics.push_back({
        code,
        severity,
        std::move(message),
        std::move(objectId),
    });
}

std::optional<std::string> edgeExpression(
    const GenerationSignal& signal,
    const GenerationStep& step)
{
    if (signal.kind != LaneKind::Bit) return std::nullopt;
    const auto value = lowerCopy(step.value);
    if (value == "1") return "$rose(" + signal.identifier + ")";
    if (value == "0") return "$fell(" + signal.identifier + ")";
    return std::nullopt;
}

std::string systemVerilogAction(
    const GenerationPlan& plan,
    const GenerationStep& step,
    std::vector<GenerationDiagnostic>& diagnostics)
{
    const auto* signal = findSignal(plan, step.laneId);
    switch (step.action) {
    case EventAction::Drive:
    case EventAction::Pulse: {
        if (!signal) {
            appendDiagnostic(
                diagnostics,
                GenerationDiagnosticCode::MissingLane,
                Severity::Error,
                "Drive event has no generated signal",
                step.eventId);
            return "$error(\"missing drive signal\");";
        }
        const auto literal = systemVerilogLiteral(*signal, step.value);
        if (!literal) {
            appendDiagnostic(
                diagnostics,
                GenerationDiagnosticCode::InvalidValue,
                Severity::Error,
                "Cannot encode value '" + step.value + "' for " + signal->sourceName,
                step.eventId);
            return "$error(\"invalid generated value\");";
        }
        if (step.action == EventAction::Pulse) {
            appendDiagnostic(
                diagnostics,
                GenerationDiagnosticCode::UnsupportedAction,
                Severity::Warning,
                "Pulse duration is not represented; generated as a drive transition",
                step.eventId);
        }
        return signal->identifier + " = " + *literal + ";";
    }
    case EventAction::Expect: {
        if (!signal) {
            appendDiagnostic(
                diagnostics,
                GenerationDiagnosticCode::MissingLane,
                Severity::Error,
                "Expect event has no generated signal",
                step.eventId);
            return "$error(\"missing expected signal\");";
        }
        const auto literal = systemVerilogLiteral(*signal, step.value);
        if (!literal) {
            appendDiagnostic(
                diagnostics,
                GenerationDiagnosticCode::InvalidValue,
                Severity::Error,
                "Cannot encode expected value '" + step.value + "'",
                step.eventId);
            return "$error(\"invalid expected value\");";
        }
        return "if (" + signal->identifier + " !== " + *literal
            + ") $error(\"" + escapeComment(signal->sourceName)
            + " mismatch at %0t: expected " + escapeComment(step.value)
            + ", got %b\", $time, " + signal->identifier + ");";
    }
    case EventAction::Toggle:
        if (!signal) return "$error(\"missing toggle signal\");";
        return signal->identifier + " = ~" + signal->identifier + ";";
    case EventAction::WaitCondition:
        if (step.value.empty()) {
            appendDiagnostic(
                diagnostics,
                GenerationDiagnosticCode::UnsupportedAction,
                Severity::Error,
                "Wait condition has no expression",
                step.eventId);
            return "$error(\"empty wait condition\");";
        }
        return "wait (" + step.value + ");";
    case EventAction::Marker:
    case EventAction::Note:
        return "// " + escapeComment(
            step.description.empty() ? step.value : step.description);
    case EventAction::SendFrame:
    case EventAction::ReceiveFrame:
        appendDiagnostic(
            diagnostics,
            GenerationDiagnosticCode::UnsupportedAction,
            Severity::Error,
            "Frame events require a protocol adapter and were not generated",
            step.eventId);
        return "$error(\"frame adapter required\");";
    }
    return {};
}

std::string cocotbAction(
    const GenerationPlan& plan,
    const GenerationStep& step,
    std::vector<GenerationDiagnostic>& diagnostics,
    const std::string& indentation)
{
    const auto* signal = findSignal(plan, step.laneId);
    switch (step.action) {
    case EventAction::Drive:
    case EventAction::Pulse: {
        if (!signal) {
            appendDiagnostic(
                diagnostics,
                GenerationDiagnosticCode::MissingLane,
                Severity::Error,
                "Drive event has no generated signal",
                step.eventId);
            return indentation + "raise AssertionError(\"missing drive signal\")\n";
        }
        const auto literal = cocotbDriveLiteral(*signal, step.value);
        if (!literal) {
            appendDiagnostic(
                diagnostics,
                GenerationDiagnosticCode::InvalidValue,
                Severity::Error,
                "Cannot encode value '" + step.value + "' for cocotb",
                step.eventId);
            return indentation + "raise AssertionError(\"invalid generated value\")\n";
        }
        if (step.action == EventAction::Pulse) {
            appendDiagnostic(
                diagnostics,
                GenerationDiagnosticCode::UnsupportedAction,
                Severity::Warning,
                "Pulse duration is not represented; generated as a drive transition",
                step.eventId);
        }
        return indentation + "dut." + signal->identifier + ".value = " + *literal + "\n";
    }
    case EventAction::Expect: {
        if (!signal) {
            appendDiagnostic(
                diagnostics,
                GenerationDiagnosticCode::MissingLane,
                Severity::Error,
                "Expect event has no generated signal",
                step.eventId);
            return indentation + "raise AssertionError(\"missing expected signal\")\n";
        }
        const auto literal = cocotbDriveLiteral(*signal, step.value);
        if (!literal) {
            appendDiagnostic(
                diagnostics,
                GenerationDiagnosticCode::InvalidValue,
                Severity::Error,
                "Cannot encode expected value '" + step.value + "' for cocotb",
                step.eventId);
            return indentation + "raise AssertionError(\"invalid expected value\")\n";
        }
        const auto lowered = lowerCopy(*literal);
        if (lowered.find('x') != std::string::npos || lowered.find('z') != std::string::npos) {
            auto expected = lowerCopy(mappedValue(*signal, step.value));
            if (expected.starts_with("0b")) expected = expected.substr(2);
            return indentation + "assert str(dut." + signal->identifier
                + ".value).lower() == \"" + expected + "\", "
                + "\"expected " + escapePython(step.value) + "\"\n";
        }
        return indentation + "assert int(dut." + signal->identifier
            + ".value) == " + *literal + ", "
            + "\"expected " + escapePython(step.value) + "\"\n";
    }
    case EventAction::Toggle:
        if (!signal) return indentation + "raise AssertionError(\"missing toggle signal\")\n";
        return indentation + "dut." + signal->identifier
            + ".value = 0 if int(dut." + signal->identifier + ".value) else 1\n";
    case EventAction::Marker:
    case EventAction::Note:
        return indentation + "dut._log.info(\""
            + escapePython(step.description.empty() ? step.value : step.description) + "\")\n";
    case EventAction::WaitCondition:
        appendDiagnostic(
            diagnostics,
            GenerationDiagnosticCode::UnsupportedAction,
            Severity::Error,
            "Free-form wait conditions cannot be translated reliably to cocotb",
            step.eventId);
        return indentation + "raise NotImplementedError(\"manual wait-condition adapter required\")\n";
    case EventAction::SendFrame:
    case EventAction::ReceiveFrame:
        appendDiagnostic(
            diagnostics,
            GenerationDiagnosticCode::UnsupportedAction,
            Severity::Error,
            "Frame events require a protocol adapter and were not generated",
            step.eventId);
        return indentation + "raise NotImplementedError(\"frame adapter required\")\n";
    }
    return {};
}

} // namespace

bool GenerationPlanResult::ok() const noexcept
{
    return plan.has_value() && !hasErrors(diagnostics);
}

bool TextGenerationResult::ok() const noexcept
{
    return !text.empty() && !hasErrors(diagnostics);
}

GenerationPlanResult buildGenerationPlan(
    const Project& project,
    const Scenario& scenario)
{
    GenerationPlanResult result;
    GenerationPlan plan;
    plan.projectId = project.id;
    plan.scenarioId = scenario.id;
    plan.scenarioName = scenario.name;
    plan.timeBase = project.timeBase;
    plan.duration = scenario.duration;
    plan.clocks = project.clockDomains;
    plan.relations = scenario.relations;
    std::sort(plan.clocks.begin(), plan.clocks.end(), [](const ClockDomain& left, const ClockDomain& right) {
        return left.id < right.id;
    });
    std::sort(plan.relations.begin(), plan.relations.end(), [](const Relation& left, const Relation& right) {
        return left.id < right.id;
    });

    std::vector<const Lane*> lanes;
    lanes.reserve(scenario.lanes.size());
    for (const auto& lane : scenario.lanes) {
        if (lane.kind != LaneKind::Group
            && lane.kind != LaneKind::Transaction
            && lane.kind != LaneKind::Event) {
            lanes.push_back(&lane);
        }
    }
    std::sort(lanes.begin(), lanes.end(), [](const Lane* left, const Lane* right) {
        return left->id < right->id;
    });

    std::unordered_map<std::string, int> identifierCounts;
    for (const auto* lane : lanes) {
        GenerationSignal signal;
        signal.laneId = lane->id;
        signal.sourceName = lane->name;
        auto identifierSource = lane->name;
        if ((lane->kind == LaneKind::Bus || lane->kind == LaneKind::Enum)
            && identifierSource.ends_with(']')) {
            const auto bracket = identifierSource.rfind('[');
            if (bracket != std::string::npos) {
                const auto range = identifierSource.substr(
                    bracket + 1,
                    identifierSource.size() - bracket - 2);
                if (!range.empty()
                    && std::all_of(
                        range.begin(),
                        range.end(),
                        [](const unsigned char character) {
                            return std::isdigit(character) || character == ':';
                        })) {
                    identifierSource.erase(bracket);
                }
            }
        }
        signal.identifier = sanitizeIdentifier(identifierSource);
        signal.kind = lane->kind;
        signal.width = lane->width;
        signal.isSigned = lane->isSigned;
        signal.enumMap = lane->enumMap;
        signal.clockDomainId = lane->clockDomainId;
        if (lane->kind == LaneKind::Clock) {
            signal.clockOverrides = lane->segments;
        }
        if (signal.identifier.empty()) {
            signal.identifier = "signal";
            appendDiagnostic(
                result.diagnostics,
                GenerationDiagnosticCode::InvalidIdentifier,
                Severity::Warning,
                "Signal name '" + lane->name + "' required a generated identifier",
                lane->id);
        }
        const auto count = identifierCounts[signal.identifier]++;
        if (count > 0) {
            const auto base = signal.identifier;
            signal.identifier += "_" + std::to_string(count + 1);
            appendDiagnostic(
                result.diagnostics,
                GenerationDiagnosticCode::IdentifierCollision,
                Severity::Warning,
                "Identifier collision for '" + base + "'; generated '" + signal.identifier + "'",
                lane->id);
        }
        plan.signalDefinitions.push_back(std::move(signal));
    }

    std::unordered_set<std::string> linkedSegments;
    for (const auto& event : scenario.events) {
        if (!event.linkedSegmentId.empty()) linkedSegments.insert(event.linkedSegmentId);
        plan.steps.push_back({
            event.id,
            event.tick,
            event.action,
            event.laneId,
            event.value,
            event.expectedResult,
            event.clockDomainId,
            event.cycle,
            event.description,
        });
    }
    for (const auto& lane : scenario.lanes) {
        if (lane.kind == LaneKind::Clock
            || lane.kind == LaneKind::Group
            || lane.kind == LaneKind::Transaction
            || lane.kind == LaneKind::Event) {
            continue;
        }
        for (const auto& segment : lane.segments) {
            if (linkedSegments.contains(segment.id)) continue;
            plan.steps.push_back({
                "derived-" + segment.id,
                segment.start,
                EventAction::Drive,
                lane.id,
                segment.value,
                {},
                lane.clockDomainId,
                std::nullopt,
                "Derived from waveform segment",
            });
        }
    }
    std::stable_sort(plan.steps.begin(), plan.steps.end(), [](const GenerationStep& left, const GenerationStep& right) {
        return left.tick < right.tick
            || (left.tick == right.tick && left.eventId < right.eventId);
    });

    for (const auto& step : plan.steps) {
        if (!step.laneId.empty() && !findSignal(plan, step.laneId)
            && step.action != EventAction::Marker
            && step.action != EventAction::Note
            && step.action != EventAction::WaitCondition) {
            appendDiagnostic(
                result.diagnostics,
                GenerationDiagnosticCode::MissingLane,
                Severity::Error,
                "Event references a lane that cannot be generated",
                step.eventId);
        }
    }
    if (!toPicoseconds(plan.duration, plan.timeBase)) {
        appendDiagnostic(
            result.diagnostics,
            GenerationDiagnosticCode::TimeOverflow,
            Severity::Error,
            "Scenario duration overflows picosecond generation time",
            scenario.id);
    }

    plan.relations.shrink_to_fit();
    result.plan = std::move(plan);
    return result;
}

TextGenerationResult generateSystemVerilogAssertionBody(const GenerationPlan& plan)
{
    TextGenerationResult result;
    std::ostringstream output;
    output << "// Assertions generated from losslessly representable Wave Workbench relations.\n";
    for (const auto& relation : plan.relations) {
        const auto* source = findStep(plan, relation.sourceEventId);
        const auto* target = findStep(plan, relation.targetEventId);
        if (!source || !target) {
            appendDiagnostic(
                result.diagnostics,
                GenerationDiagnosticCode::SvaUnsupportedEvent,
                Severity::Warning,
                "Relation event is missing; no SVA was generated",
                relation.id);
            continue;
        }
        const auto* sourceSignal = findSignal(plan, source->laneId);
        const auto* targetSignal = findSignal(plan, target->laneId);
        const auto sourceExpression = sourceSignal
            ? edgeExpression(*sourceSignal, *source)
            : std::nullopt;
        const auto targetExpression = targetSignal
            ? edgeExpression(*targetSignal, *target)
            : std::nullopt;
        if (!sourceExpression || !targetExpression) {
            appendDiagnostic(
                result.diagnostics,
                GenerationDiagnosticCode::SvaUnsupportedEvent,
                Severity::Warning,
                "Relation endpoints are not unambiguous bit edges; no SVA was generated",
                relation.id);
            continue;
        }
        if (!relation.condition.empty()) {
            appendDiagnostic(
                result.diagnostics,
                GenerationDiagnosticCode::SvaUnsupportedCondition,
                Severity::Warning,
                "Relation condition has no lossless SVA translation; no approximate SVA was generated",
                relation.id);
            continue;
        }
        const auto* clock = findPlanClock(plan, relation.clockDomainId);
        const auto clockSignal = clock
            ? std::find_if(
                  plan.signalDefinitions.begin(),
                  plan.signalDefinitions.end(),
                  [clock](const GenerationSignal& signal) {
                      return signal.kind == LaneKind::Clock
                          && signal.clockDomainId == clock->id;
                  })
            : plan.signalDefinitions.end();
        if (!clock || clockSignal == plan.signalDefinitions.end()) {
            appendDiagnostic(
                result.diagnostics,
                GenerationDiagnosticCode::SvaMissingClock,
                Severity::Warning,
                "Relation has no resolvable clock lane; no SVA was generated",
                relation.id);
            continue;
        }
        if (clock->resetRelation.empty()) {
            appendDiagnostic(
                result.diagnostics,
                GenerationDiagnosticCode::SvaMissingDisableCondition,
                Severity::Warning,
                "Clock domain has no explicit disable/reset condition; no SVA was generated",
                relation.id);
            continue;
        }
        if (relation.minimumDelay < 0
            || relation.maximumDelay < relation.minimumDelay
            || relation.minimumDelay % clock->period != 0
            || relation.maximumDelay % clock->period != 0) {
            appendDiagnostic(
                result.diagnostics,
                GenerationDiagnosticCode::SvaNonCycleDelay,
                Severity::Warning,
                "Relation delay is not an exact number of clock cycles; no approximate SVA was generated",
                relation.id);
            continue;
        }
        const auto relationWindowOverflows =
            relation.maximumDelay > 0
            && source->tick
                > std::numeric_limits<Tick>::max() - relation.maximumDelay;
        const auto relationWindowEnd = relationWindowOverflows
            ? std::numeric_limits<Tick>::max()
            : source->tick + relation.maximumDelay;
        const auto overrideOverlapsWindow = std::any_of(
            clockSignal->clockOverrides.begin(),
            clockSignal->clockOverrides.end(),
            [source, relationWindowEnd](const Segment& segment) {
                return segment.end > source->tick
                    && segment.start <= relationWindowEnd;
            });
        if (relationWindowOverflows || overrideOverlapsWindow) {
            appendDiagnostic(
                result.diagnostics,
                GenerationDiagnosticCode::SvaClockOverrideOverlap,
                Severity::Warning,
                "Clock override intersects the relation evaluation window; "
                "no approximate SVA was generated",
                relation.id);
            continue;
        }

        const auto minimumCycles = relation.minimumDelay / clock->period;
        const auto maximumCycles = relation.maximumDelay / clock->period;
        const auto propertyName = "p_" + sanitizeIdentifier(relation.id);
        auto disableExpression = "(" + clock->resetRelation + ")";
        if (std::any_of(
                clockSignal->clockOverrides.begin(),
                clockSignal->clockOverrides.end(),
                [](const Segment& segment) {
                    return clockOverrideModeFromString(segment.value)
                        == ClockOverrideMode::Disabled;
                })) {
            disableExpression += " || $isunknown(" + clockSignal->identifier + ")";
        }
        output << "\n// " << escapeComment(relation.description) << "\n"
               << "property " << propertyName << ";\n"
               << "  @(" << (clock->activeEdge == ClockEdge::Rising ? "posedge " : "negedge ")
               << clockSignal->identifier << ") disable iff (" << disableExpression << ")\n"
               << "    " << *sourceExpression << " |-> ##[" << minimumCycles
               << ":" << maximumCycles << "] " << *targetExpression << ";\n"
               << "endproperty\n"
               << "assert property (" << propertyName << ") else $error(\"Relation "
               << escapeComment(relation.id) << " failed\");\n";
    }
    result.text = output.str();
    return result;
}

TextGenerationResult generateSystemVerilogAssertions(const GenerationPlan& plan)
{
    auto result = generateSystemVerilogAssertionBody(plan);
    std::ostringstream output;
    output << "`timescale 1ps/1ps\n\n"
           << "module wave_workbench_assertions_"
           << sanitizeIdentifier(plan.scenarioId) << "(\n";
    for (std::size_t index = 0; index < plan.signalDefinitions.size(); ++index) {
        const auto& signal = plan.signalDefinitions[index];
        output << "  input logic ";
        if (signal.isSigned) output << "signed ";
        if (signal.width > 1) output << "[" << signal.width - 1 << ":0] ";
        output << signal.identifier;
        if (index + 1 < plan.signalDefinitions.size()) output << ",";
        output << "\n";
    }
    output << ");\n"
           << "  timeunit 1ps;\n"
           << "  timeprecision 1ps;\n\n";
    std::istringstream assertionStream(result.text);
    std::string line;
    while (std::getline(assertionStream, line)) {
        output << "  " << line << "\n";
    }
    output << "\nendmodule\n";
    result.text = output.str();
    return result;
}

TextGenerationResult generateSystemVerilog(const GenerationPlan& plan)
{
    TextGenerationResult result;
    std::ostringstream output;
    output << "`timescale 1ps/1ps\n\n"
           << "module wave_workbench_tb;\n"
           << "  timeunit 1ps;\n"
           << "  timeprecision 1ps;\n\n";

    for (const auto& signal : plan.signalDefinitions) {
        output << "  logic ";
        if (signal.isSigned) output << "signed ";
        if (signal.width > 1) output << "[" << signal.width - 1 << ":0] ";
        output << signal.identifier << "; // " << escapeComment(signal.sourceName) << "\n";
    }
    for (const auto& signal : plan.signalDefinitions) {
        if (signal.kind != LaneKind::Clock || signal.clockOverrides.empty()) continue;
        output << "  logic " << clockRawIdentifier(signal) << ";\n"
               << "  logic " << clockGateIdentifier(signal) << ";\n"
               << "  logic " << clockDisableIdentifier(signal) << ";\n"
               << "  assign " << signal.identifier << " = "
               << clockDisableIdentifier(signal) << " ? 1'bx : "
               << clockGateIdentifier(signal) << " ? 1'b0 : "
               << clockRawIdentifier(signal) << ";\n";
    }
    output << "\n";

    for (const auto& clock : plan.clocks) {
        const auto signal = std::find_if(
            plan.signalDefinitions.begin(),
            plan.signalDefinitions.end(),
            [&clock](const GenerationSignal& candidate) {
                return candidate.kind == LaneKind::Clock
                    && candidate.clockDomainId == clock.id;
            });
        if (signal == plan.signalDefinitions.end()) continue;
        const auto period = toPicoseconds(clock.period, plan.timeBase);
        const auto phase = toPicoseconds(clock.phase, plan.timeBase);
        std::int64_t highTickProduct = 0;
        if (!period || !phase
            || !checkedMultiply(clock.period, clock.dutyCycle.numerator, highTickProduct)) {
            appendDiagnostic(
                result.diagnostics,
                GenerationDiagnosticCode::TimeOverflow,
                Severity::Error,
                "Clock time overflows generated picoseconds",
                clock.id);
            continue;
        }
        const auto highTicks = highTickProduct / clock.dutyCycle.denominator;
        const auto high = toPicoseconds(highTicks, plan.timeBase);
        const auto low = high ? *period - *high : 0;
        if (!high || *high <= 0 || low <= 0) {
            appendDiagnostic(
                result.diagnostics,
                GenerationDiagnosticCode::TimeOverflow,
                Severity::Error,
                "Clock duty cycle cannot be represented in integer picoseconds",
                clock.id);
            continue;
        }
        const auto clockTarget = signal->clockOverrides.empty()
            ? signal->identifier
            : clockRawIdentifier(*signal);
        output << "  initial begin : clock_" << sanitizeIdentifier(clock.id) << "\n"
               << "    " << clockTarget << " = 1'b0;\n"
               << "    #" << *phase << ";\n"
               << "    forever begin\n"
               << "      " << clockTarget << " = 1'b1; #" << *high << ";\n"
               << "      " << clockTarget << " = 1'b0; #" << low << ";\n"
               << "    end\n"
               << "  end\n\n";
        if (!signal->clockOverrides.empty()) {
            output << "  initial begin : clock_overrides_"
                   << sanitizeIdentifier(clock.id) << "\n"
                   << "    " << clockGateIdentifier(*signal) << " = 1'b0;\n"
                   << "    " << clockDisableIdentifier(*signal) << " = 1'b0;\n";
            Tick cursor = 0;
            for (const auto& segment : signal->clockOverrides) {
                const auto mode = clockOverrideModeFromString(segment.value);
                const auto delay = toPicoseconds(segment.start - cursor, plan.timeBase);
                const auto duration = toPicoseconds(segment.end - segment.start, plan.timeBase);
                if (!mode || !delay || !duration) {
                    appendDiagnostic(
                        result.diagnostics,
                        GenerationDiagnosticCode::TimeOverflow,
                        Severity::Error,
                        "Clock override time cannot be represented in picoseconds",
                        segment.id);
                    continue;
                }
                if (*delay > 0) output << "    #" << *delay << ";\n";
                output << "    " << clockGateIdentifier(*signal) << " = 1'b"
                       << (*mode == ClockOverrideMode::Gated ? "1" : "0") << ";\n"
                       << "    " << clockDisableIdentifier(*signal) << " = 1'b"
                       << (*mode == ClockOverrideMode::Disabled ? "1" : "0") << ";\n";
                if (*duration > 0) output << "    #" << *duration << ";\n";
                output << "    " << clockGateIdentifier(*signal) << " = 1'b0;\n"
                       << "    " << clockDisableIdentifier(*signal) << " = 1'b0;\n";
                cursor = segment.end;
            }
            output << "  end\n\n";
        }
    }

    output << "  initial begin : execute_" << sanitizeIdentifier(plan.scenarioId) << "\n"
           << "    $display(\"Wave Workbench scenario: "
           << escapeComment(plan.scenarioName) << "\");\n"
           << "    fork : scenario_and_timeout\n"
           << "      begin : scenario_steps\n"
           << "        fork\n";
    for (std::size_t index = 0; index < plan.steps.size(); ++index) {
        const auto& step = plan.steps[index];
        output << "          begin : " << stepLabel(step, index) << "\n";
        if (!step.description.empty()) {
            output << "            // " << escapeComment(step.description) << "\n";
        }
        if (step.cycle) {
            const auto* clock = findPlanClock(plan, step.clockDomainId);
            const auto clockSignal = clock
                ? std::find_if(
                      plan.signalDefinitions.begin(),
                      plan.signalDefinitions.end(),
                      [clock](const GenerationSignal& candidate) {
                          return candidate.kind == LaneKind::Clock
                              && candidate.clockDomainId == clock->id;
                      })
                : plan.signalDefinitions.end();
            if (!clock || clockSignal == plan.signalDefinitions.end() || *step.cycle < 0) {
                appendDiagnostic(
                    result.diagnostics,
                    GenerationDiagnosticCode::SvaMissingClock,
                    Severity::Error,
                    "Cycle-based event has no valid clock domain",
                    step.eventId);
                output << "            $error(\"invalid cycle-based event\");\n";
            } else if (std::none_of(
                           clockSignal->clockOverrides.begin(),
                           clockSignal->clockOverrides.end(),
                           [&step](const Segment& segment) {
                               return segment.start <= step.tick;
                           })) {
                output << "            repeat (" << *step.cycle + 1 << ") @("
                       << (clock->activeEdge == ClockEdge::Rising ? "posedge " : "negedge ")
                       << clockSignal->identifier << ");\n";
            } else {
                const auto time = toPicoseconds(step.tick, plan.timeBase);
                if (!time || *time < 0) {
                    appendDiagnostic(
                        result.diagnostics,
                        GenerationDiagnosticCode::TimeOverflow,
                        Severity::Error,
                        "Cycle-based event time cannot be generated after a clock override",
                        step.eventId);
                    output << "            $error(\"cycle event time overflow\");\n";
                } else if (*time > 0) {
                    output << "            #" << *time << ";\n";
                }
            }
        } else {
            const auto time = toPicoseconds(step.tick, plan.timeBase);
            if (!time) {
                appendDiagnostic(
                    result.diagnostics,
                    GenerationDiagnosticCode::TimeOverflow,
                    Severity::Error,
                    "Event time overflows generated picoseconds",
                    step.eventId);
                output << "            $error(\"event time overflow\");\n";
            } else if (*time > 0) {
                output << "            #" << *time << ";\n";
            }
        }
        output << "            "
               << systemVerilogAction(plan, step, result.diagnostics) << "\n"
               << "          end\n";
    }
    const auto duration = toPicoseconds(plan.duration, plan.timeBase).value_or(0);
    output << "          begin : scenario_duration\n"
           << "            #" << duration << ";\n"
           << "          end\n"
           << "        join\n"
           << "        $display(\"Wave Workbench scenario completed\");\n"
           << "        $finish;\n"
           << "      end\n"
           << "      begin : timeout_guard\n"
           << "        #" << (duration > std::numeric_limits<std::int64_t>::max() / 2
                                 ? duration
                                 : std::max<std::int64_t>(duration + 1, duration * 2))
           << ";\n"
           << "        $fatal(1, \"Wave Workbench scenario timeout\");\n"
           << "      end\n"
           << "    join_any\n"
           << "    disable scenario_and_timeout;\n"
           << "  end\n\n";

    auto assertions = generateSystemVerilogAssertionBody(plan);
    result.diagnostics.insert(
        result.diagnostics.end(),
        assertions.diagnostics.begin(),
        assertions.diagnostics.end());
    std::istringstream assertionStream(assertions.text);
    std::string line;
    while (std::getline(assertionStream, line)) {
        output << "  " << line << "\n";
    }
    output << "\nendmodule\n";
    result.text = output.str();
    return result;
}

TextGenerationResult generateCocotb(const GenerationPlan& plan)
{
    TextGenerationResult result;
    std::ostringstream output;
    output << "import cocotb\n"
           << "from cocotb.triggers import ClockCycles, Combine, Timer, with_timeout\n\n\n";

    for (const auto& clock : plan.clocks) {
        const auto signal = std::find_if(
            plan.signalDefinitions.begin(),
            plan.signalDefinitions.end(),
            [&clock](const GenerationSignal& candidate) {
                return candidate.kind == LaneKind::Clock
                    && candidate.clockDomainId == clock.id;
            });
        if (signal == plan.signalDefinitions.end()) continue;
        const auto period = toPicoseconds(clock.period, plan.timeBase);
        const auto phase = toPicoseconds(clock.phase, plan.timeBase);
        std::int64_t highTickProduct = 0;
        if (!period || !phase
            || !checkedMultiply(clock.period, clock.dutyCycle.numerator, highTickProduct)) {
            appendDiagnostic(
                result.diagnostics,
                GenerationDiagnosticCode::TimeOverflow,
                Severity::Error,
                "Clock time overflows generated picoseconds",
                clock.id);
            continue;
        }
        const auto high = toPicoseconds(
            highTickProduct / clock.dutyCycle.denominator,
            plan.timeBase);
        if (!high || *high <= 0 || *period <= *high) {
            appendDiagnostic(
                result.diagnostics,
                GenerationDiagnosticCode::TimeOverflow,
                Severity::Error,
                "Clock duty cycle cannot be represented in integer picoseconds",
                clock.id);
            continue;
        }
        const auto clockName = sanitizeIdentifier(clock.id);
        if (signal->clockOverrides.empty()) {
            output << "async def clock_" << clockName << "(dut):\n"
                   << "    dut." << signal->identifier << ".value = 0\n";
            if (*phase > 0) output << "    await Timer(" << *phase << ", unit=\"ps\")\n";
            output << "    while True:\n"
                   << "        dut." << signal->identifier << ".value = 1\n"
                   << "        await Timer(" << *high << ", unit=\"ps\")\n"
                   << "        dut." << signal->identifier << ".value = 0\n"
                   << "        await Timer(" << *period - *high << ", unit=\"ps\")\n\n\n";
            continue;
        }

        const auto stateName = "_ww_clock_state_" + clockName;
        const auto applyName = "_ww_apply_clock_" + clockName;
        output << stateName << " = {\"raw\": 0, \"mode\": \"run\"}\n\n\n"
               << "def " << applyName << "(dut):\n"
               << "    mode = " << stateName << "[\"mode\"]\n"
               << "    if mode == \"disabled\":\n"
               << "        dut." << signal->identifier << ".value = \"X\"\n"
               << "    elif mode == \"gated\":\n"
               << "        dut." << signal->identifier << ".value = 0\n"
               << "    else:\n"
               << "        dut." << signal->identifier << ".value = "
               << stateName << "[\"raw\"]\n\n\n"
               << "async def clock_" << clockName << "(dut):\n"
               << "    " << stateName << "[\"raw\"] = 0\n"
               << "    " << applyName << "(dut)\n";
        if (*phase > 0) output << "    await Timer(" << *phase << ", unit=\"ps\")\n";
        output << "    while True:\n"
               << "        " << stateName << "[\"raw\"] = 1\n"
               << "        " << applyName << "(dut)\n"
               << "        await Timer(" << *high << ", unit=\"ps\")\n"
               << "        " << stateName << "[\"raw\"] = 0\n"
               << "        " << applyName << "(dut)\n"
               << "        await Timer(" << *period - *high << ", unit=\"ps\")\n\n\n"
               << "async def clock_overrides_" << clockName << "(dut):\n";
        Tick cursor = 0;
        bool emittedOverride = false;
        for (const auto& segment : signal->clockOverrides) {
            const auto mode = clockOverrideModeFromString(segment.value);
            const auto delay = toPicoseconds(segment.start - cursor, plan.timeBase);
            const auto duration = toPicoseconds(segment.end - segment.start, plan.timeBase);
            if (!mode || !delay || !duration) {
                appendDiagnostic(
                    result.diagnostics,
                    GenerationDiagnosticCode::TimeOverflow,
                    Severity::Error,
                    "Clock override time cannot be represented in picoseconds",
                    segment.id);
                continue;
            }
            if (*delay > 0) {
                output << "    await Timer(" << *delay << ", unit=\"ps\")\n";
            }
            output << "    " << stateName << "[\"mode\"] = \""
                   << toString(*mode) << "\"\n"
                   << "    " << applyName << "(dut)\n";
            if (*duration > 0) {
                output << "    await Timer(" << *duration << ", unit=\"ps\")\n";
            }
            output << "    " << stateName << "[\"mode\"] = \"run\"\n"
                   << "    " << applyName << "(dut)\n";
            cursor = segment.end;
            emittedOverride = true;
        }
        if (!emittedOverride) output << "    pass\n";
        output << "\n\n";
    }

    for (std::size_t index = 0; index < plan.steps.size(); ++index) {
        const auto& step = plan.steps[index];
        output << "async def " << stepLabel(step, index) << "(dut):\n";
        if (!step.description.empty()) {
            output << "    # " << escapeComment(step.description) << "\n";
        }
        if (step.cycle) {
            const auto* clock = findPlanClock(plan, step.clockDomainId);
            const auto signal = clock
                ? std::find_if(
                      plan.signalDefinitions.begin(),
                      plan.signalDefinitions.end(),
                      [clock](const GenerationSignal& candidate) {
                          return candidate.kind == LaneKind::Clock
                              && candidate.clockDomainId == clock->id;
                      })
                : plan.signalDefinitions.end();
            if (!clock || signal == plan.signalDefinitions.end() || *step.cycle < 0) {
                appendDiagnostic(
                    result.diagnostics,
                    GenerationDiagnosticCode::SvaMissingClock,
                    Severity::Error,
                    "Cycle-based event has no valid clock domain",
                    step.eventId);
                output << "    raise AssertionError(\"invalid cycle-based event\")\n";
            } else if (std::none_of(
                           signal->clockOverrides.begin(),
                           signal->clockOverrides.end(),
                           [&step](const Segment& segment) {
                               return segment.start <= step.tick;
                           })) {
                output << "    await ClockCycles(dut." << signal->identifier
                       << ", " << *step.cycle + 1
                       << ", rising=" << (clock->activeEdge == ClockEdge::Rising ? "True" : "False")
                       << ")\n";
            } else {
                const auto time = toPicoseconds(step.tick, plan.timeBase);
                if (!time || *time < 0) {
                    appendDiagnostic(
                        result.diagnostics,
                        GenerationDiagnosticCode::TimeOverflow,
                        Severity::Error,
                        "Cycle-based event time cannot be generated after a clock override",
                        step.eventId);
                    output << "    raise AssertionError(\"cycle event time overflow\")\n";
                } else if (*time > 0) {
                    output << "    await Timer(" << *time << ", unit=\"ps\")\n";
                }
            }
        } else {
            const auto time = toPicoseconds(step.tick, plan.timeBase);
            if (!time) {
                appendDiagnostic(
                    result.diagnostics,
                    GenerationDiagnosticCode::TimeOverflow,
                    Severity::Error,
                    "Event time overflows generated picoseconds",
                    step.eventId);
                output << "    raise AssertionError(\"event time overflow\")\n";
            } else if (*time > 0) {
                output << "    await Timer(" << *time << ", unit=\"ps\")\n";
            }
        }
        output << cocotbAction(plan, step, result.diagnostics, "    ") << "\n";
    }

    const auto duration = toPicoseconds(plan.duration, plan.timeBase).value_or(0);
    const auto timeout = duration > std::numeric_limits<std::int64_t>::max() / 2
        ? duration
        : std::max<std::int64_t>(duration + 1, duration * 2);
    output << "async def scenario_duration():\n"
           << "    await Timer(" << std::max<std::int64_t>(1, duration)
           << ", unit=\"ps\")\n\n\n"
           << "@cocotb.test()\n"
           << "async def test_" << sanitizeIdentifier(plan.scenarioId) << "(dut):\n"
           << "    \"\"\"" << escapePython(plan.scenarioName) << "\"\"\"\n"
           << "    dut._log.info(\"Wave Workbench scenario: "
           << escapePython(plan.scenarioName) << "\")\n";
    for (const auto& clock : plan.clocks) {
        const auto signal = std::find_if(
            plan.signalDefinitions.begin(),
            plan.signalDefinitions.end(),
            [&clock](const GenerationSignal& candidate) {
                return candidate.kind == LaneKind::Clock
                    && candidate.clockDomainId == clock.id;
            });
        if (signal != plan.signalDefinitions.end()) {
            output << "    cocotb.start_soon(clock_" << sanitizeIdentifier(clock.id)
                   << "(dut))\n";
            if (!signal->clockOverrides.empty()) {
                output << "    cocotb.start_soon(clock_overrides_"
                       << sanitizeIdentifier(clock.id) << "(dut))\n";
            }
        }
    }
    output << "    tasks = [\n";
    for (std::size_t index = 0; index < plan.steps.size(); ++index) {
        output << "        cocotb.start_soon(" << stepLabel(plan.steps[index], index)
               << "(dut)),\n";
    }
    output << "        cocotb.start_soon(scenario_duration()),\n";
    output << "    ]\n"
           << "    await with_timeout(Combine(*tasks), " << timeout << ", \"ps\")\n"
           << "    dut._log.info(\"Wave Workbench scenario completed\")\n";
    result.text = output.str();
    return result;
}

std::string sanitizeIdentifier(const std::string_view source)
{
    std::string result;
    result.reserve(source.size() + 1);
    for (const auto character : source) {
        const auto unsignedCharacter = static_cast<unsigned char>(character);
        if (std::isalnum(unsignedCharacter) || character == '_') {
            if (character != '_' || result.empty() || result.back() != '_') {
                result.push_back(character);
            }
        } else {
            if (result.empty() || result.back() != '_') {
                result.push_back('_');
            }
        }
    }
    while (!result.empty() && result.back() == '_') result.pop_back();
    if (result.empty()) return {};
    if (std::isdigit(static_cast<unsigned char>(result.front()))) {
        result.insert(result.begin(), '_');
    }
    static const std::set<std::string> keywords{
        "always", "assign", "begin", "end", "if", "else", "logic", "module",
        "property", "repeat", "wait", "wire",
    };
    if (keywords.contains(result)) result += "_signal";
    return result;
}

std::string_view toString(const GenerationDiagnosticCode code) noexcept
{
    switch (code) {
    case GenerationDiagnosticCode::InvalidIdentifier: return "invalid-identifier";
    case GenerationDiagnosticCode::IdentifierCollision: return "identifier-collision";
    case GenerationDiagnosticCode::MissingLane: return "missing-lane";
    case GenerationDiagnosticCode::UnsupportedAction: return "unsupported-action";
    case GenerationDiagnosticCode::InvalidValue: return "invalid-value";
    case GenerationDiagnosticCode::TimeOverflow: return "time-overflow";
    case GenerationDiagnosticCode::SvaMissingClock: return "sva-missing-clock";
    case GenerationDiagnosticCode::SvaMissingDisableCondition: return "sva-missing-disable-condition";
    case GenerationDiagnosticCode::SvaNonCycleDelay: return "sva-non-cycle-delay";
    case GenerationDiagnosticCode::SvaUnsupportedEvent: return "sva-unsupported-event";
    case GenerationDiagnosticCode::SvaUnsupportedCondition: return "sva-unsupported-condition";
    case GenerationDiagnosticCode::SvaClockOverrideOverlap: return "sva-clock-override-overlap";
    }
    return "unknown";
}

} // namespace wave
