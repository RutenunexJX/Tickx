#include "wave/simulation_check.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <utility>

namespace wave {
namespace {

const Lane* laneForId(const Scenario& scenario, const std::string& laneId)
{
    return findLane(scenario, laneId);
}

struct ResolvedSignal {
    const Lane* lane{nullptr};
    const TraceSignal* signal{nullptr};
    std::string error;
};

ResolvedSignal resolveSignal(
    const Scenario& scenario,
    const TraceIndex& trace,
    const ImportedTrace& reference,
    const std::string& laneId)
{
    ResolvedSignal result;
    result.lane = laneForId(scenario, laneId);
    if (!result.lane) {
        result.error = "lane '" + laneId + "' does not exist";
        return result;
    }
    const auto mapping = reference.signalMapping.find(laneId);
    if (mapping == reference.signalMapping.end()) {
        result.error = "lane '" + result.lane->name
            + "' has no configured actual signal mapping";
        return result;
    }
    result.signal = mappedTraceSignal(reference, trace, laneId);
    if (!result.signal) {
        result.error = "mapped actual signal '" + mapping->second
            + "' for lane '" + result.lane->name
            + "' does not exist in the loaded trace";
        return result;
    }
    if (result.signal->width != result.lane->width) {
        result.error = "mapped actual signal for lane '" + result.lane->name
            + "' has width " + std::to_string(result.signal->width)
            + ", expected " + std::to_string(result.lane->width);
        result.signal = nullptr;
    }
    return result;
}

std::optional<std::string> normalizedTraceValue(
    const Lane& sourceLane,
    const std::string& value)
{
    auto lane = sourceLane;
    if (lane.kind == LaneKind::Clock) lane.kind = LaneKind::Bit;
    return laneValueBits(lane, value, LaneValueEncoding::BinaryTrace);
}

std::optional<bool> transitionMatchesEdge(
    const Lane& lane,
    const std::string& before,
    const std::string& after,
    const SimulationCheckEdge edge)
{
    const auto beforeBits = normalizedTraceValue(lane, before);
    const auto afterBits = normalizedTraceValue(lane, after);
    if (!beforeBits || !afterBits) return std::nullopt;
    if (edge == SimulationCheckEdge::AnyChange) {
        return *beforeBits != *afterBits;
    }
    if (beforeBits->size() != 1 || afterBits->size() != 1) {
        return std::nullopt;
    }
    return edge == SimulationCheckEdge::Rising
        ? beforeBits->front() == '0' && afterBits->front() == '1'
        : beforeBits->front() == '1' && afterBits->front() == '0';
}

std::optional<std::vector<Tick>> edgeTicks(
    const Lane& lane,
    const TraceSignal& signal,
    const SimulationCheckEdge edge,
    const Tick start,
    const Tick end,
    std::string& error)
{
    std::vector<Tick> result;
    for (std::size_t index = 1; index < signal.transitions.size(); ++index) {
        const auto& transition = signal.transitions[index];
        if (transition.tick < start) continue;
        if (transition.tick >= end) break;
        const auto matched = transitionMatchesEdge(
            lane,
            signal.transitions[index - 1].value,
            transition.value,
            edge);
        if (!matched) {
            error = "trace values for lane '" + lane.name
                + "' cannot be interpreted for "
                + std::string(toString(edge)) + " edge detection";
            return std::nullopt;
        }
        if (*matched) result.push_back(transition.tick);
    }
    return result;
}

bool checkedAdd(const Tick left, const Tick right, Tick& result)
{
    if (right > 0 && left > std::numeric_limits<Tick>::max() - right) {
        return false;
    }
    result = left + right;
    return true;
}

SimulationCheckOutcome baseOutcome(const SimulationCheckDefinition& check)
{
    SimulationCheckOutcome outcome;
    outcome.checkId = check.id;
    outcome.name = check.name;
    outcome.kind = check.kind;
    outcome.laneId = check.laneId;
    outcome.sourceLaneId = check.sourceLaneId;
    outcome.targetLaneId = check.targetLaneId;
    outcome.start = check.kind == SimulationCheckKind::ValueAtTick
        ? check.tick : check.start;
    outcome.end = check.kind == SimulationCheckKind::ValueAtTick
        ? check.tick : check.end;
    return outcome;
}

SimulationCheckOutcome evaluateValueAtTick(
    const Scenario& scenario,
    const TraceIndex& trace,
    const ImportedTrace& reference,
    const SimulationCheckDefinition& check)
{
    auto outcome = baseOutcome(check);
    outcome.expected = check.expectedValue;
    outcome.focusTick = check.tick;
    const auto resolved = resolveSignal(
        scenario, trace, reference, check.laneId);
    if (!resolved.signal) {
        outcome.status = SimulationCheckStatus::Unavailable;
        outcome.message = resolved.error;
        return outcome;
    }
    outcome.traceSignalId = resolved.signal->id;
    const auto* sampled = resolved.signal->valueAt(check.tick);
    if (!sampled) {
        outcome.status = SimulationCheckStatus::Unavailable;
        outcome.message = "no actual value is available at tick "
            + std::to_string(check.tick);
        return outcome;
    }
    outcome.actual = sampled->value;
    outcome.status = traceValueMatchesLane(
        *resolved.lane,
        check.expectedValue,
        sampled->value)
        ? SimulationCheckStatus::Passed
        : SimulationCheckStatus::Failed;
    outcome.message = outcome.status == SimulationCheckStatus::Passed
        ? "actual value matches the expected value"
        : "actual value does not match the expected value";
    return outcome;
}

SimulationCheckOutcome evaluateStableRange(
    const Scenario& scenario,
    const TraceIndex& trace,
    const ImportedTrace& reference,
    const SimulationCheckDefinition& check)
{
    auto outcome = baseOutcome(check);
    const auto resolved = resolveSignal(
        scenario, trace, reference, check.laneId);
    if (!resolved.signal) {
        outcome.status = SimulationCheckStatus::Unavailable;
        outcome.message = resolved.error;
        return outcome;
    }
    outcome.traceSignalId = resolved.signal->id;
    const auto* sampled = resolved.signal->valueAt(check.start);
    if (!sampled) {
        outcome.status = SimulationCheckStatus::Unavailable;
        outcome.message = "no actual value is available at the stability-window start";
        return outcome;
    }
    auto current = normalizedTraceValue(*resolved.lane, sampled->value);
    if (!current) {
        outcome.status = SimulationCheckStatus::Unavailable;
        outcome.message = "the actual value at the stability-window start is invalid";
        return outcome;
    }
    outcome.expected = sampled->value;
    const auto first = std::lower_bound(
        resolved.signal->transitions.begin(),
        resolved.signal->transitions.end(),
        check.start,
        [](const TraceTransition& transition, const Tick tick) {
            return transition.tick < tick;
        });
    for (auto iterator = first; iterator != resolved.signal->transitions.end(); ++iterator) {
        if (iterator->tick >= check.end) break;
        auto next = normalizedTraceValue(*resolved.lane, iterator->value);
        if (!next) {
            outcome.status = SimulationCheckStatus::Unavailable;
            outcome.focusTick = iterator->tick;
            outcome.message = "an actual value in the stability window is invalid";
            return outcome;
        }
        if (iterator->tick > check.start && *next != *current) {
            outcome.status = SimulationCheckStatus::Failed;
            outcome.focusTick = iterator->tick;
            outcome.actual = iterator->value;
            outcome.message = "value changed inside the required stable range";
            return outcome;
        }
        current = std::move(next);
    }
    outcome.status = SimulationCheckStatus::Passed;
    outcome.message = "value remained stable for the complete range";
    return outcome;
}

SimulationCheckOutcome evaluateEdgeResponse(
    const Scenario& scenario,
    const TraceIndex& trace,
    const ImportedTrace& reference,
    const SimulationCheckDefinition& check)
{
    auto outcome = baseOutcome(check);
    const auto source = resolveSignal(
        scenario, trace, reference, check.sourceLaneId);
    const auto target = resolveSignal(
        scenario, trace, reference, check.targetLaneId);
    if (!source.signal || !target.signal) {
        outcome.status = SimulationCheckStatus::Unavailable;
        outcome.message = !source.signal ? source.error : target.error;
        return outcome;
    }
    outcome.sourceTraceSignalId = source.signal->id;
    outcome.targetTraceSignalId = target.signal->id;
    std::string edgeError;
    const auto sourceTicks = edgeTicks(
        *source.lane,
        *source.signal,
        check.sourceEdge,
        check.start,
        check.end,
        edgeError);
    if (!sourceTicks) {
        outcome.status = SimulationCheckStatus::Unavailable;
        outcome.message = edgeError;
        return outcome;
    }
    Tick targetEnd = trace.endTick;
    if (targetEnd < std::numeric_limits<Tick>::max()) ++targetEnd;
    const auto targetTicks = edgeTicks(
        *target.lane,
        *target.signal,
        check.targetEdge,
        trace.startTick,
        targetEnd,
        edgeError);
    if (!targetTicks) {
        outcome.status = SimulationCheckStatus::Unavailable;
        outcome.message = edgeError;
        return outcome;
    }
    outcome.sourceEventCount = sourceTicks->size();
    if (sourceTicks->empty()) {
        outcome.status = SimulationCheckStatus::Unavailable;
        outcome.message = "the source edge was not exercised in the selected range";
        return outcome;
    }

    std::size_t targetIndex = 0;
    for (const auto sourceTick : *sourceTicks) {
        Tick minimumTick = 0;
        Tick maximumTick = 0;
        if (!checkedAdd(sourceTick, check.minimumDelay, minimumTick)
            || !checkedAdd(sourceTick, check.maximumDelay, maximumTick)) {
            outcome.status = SimulationCheckStatus::Failed;
            outcome.focusTick = sourceTick;
            outcome.message = "the response window exceeds the supported time range";
            return outcome;
        }
        while (targetIndex < targetTicks->size()
               && targetTicks->at(targetIndex) < minimumTick) {
            ++targetIndex;
        }
        if (targetIndex >= targetTicks->size()
            || targetTicks->at(targetIndex) > maximumTick) {
            outcome.status = SimulationCheckStatus::Failed;
            outcome.focusTick = sourceTick;
            outcome.start = sourceTick;
            outcome.end = maximumTick;
            outcome.message = "no matching target edge occurred within the response window";
            return outcome;
        }
        ++outcome.matchedEventCount;
        ++targetIndex;
    }
    outcome.status = SimulationCheckStatus::Passed;
    outcome.message = std::to_string(outcome.matchedEventCount)
        + " source edge(s) received a response within the required delay";
    return outcome;
}

void countStatus(SimulationCheckReport& report, const SimulationCheckStatus status)
{
    switch (status) {
    case SimulationCheckStatus::Disabled: ++report.disabledCount; break;
    case SimulationCheckStatus::Passed: ++report.passedCount; break;
    case SimulationCheckStatus::Failed: ++report.failedCount; break;
    case SimulationCheckStatus::Unavailable: ++report.unavailableCount; break;
    }
}

} // namespace

SimulationCheckReport evaluateSimulationChecks(
    const Project& project,
    const Scenario& scenario,
    const TraceIndex& trace,
    const ImportedTrace& reference,
    const std::vector<SimulationCheckDefinition>& checks)
{
    SimulationCheckReport report;
    report.projectId = project.id;
    report.scenarioId = scenario.id;
    report.traceId = reference.id;
    report.outcomes.reserve(checks.size());
    for (const auto& check : checks) {
        SimulationCheckOutcome outcome;
        if (!check.enabled) {
            outcome = baseOutcome(check);
            outcome.status = SimulationCheckStatus::Disabled;
            outcome.message = "check is disabled";
        } else {
            switch (check.kind) {
            case SimulationCheckKind::ValueAtTick:
                outcome = evaluateValueAtTick(
                    scenario, trace, reference, check);
                break;
            case SimulationCheckKind::StableRange:
                outcome = evaluateStableRange(
                    scenario, trace, reference, check);
                break;
            case SimulationCheckKind::EdgeResponse:
                outcome = evaluateEdgeResponse(
                    scenario, trace, reference, check);
                break;
            }
        }
        countStatus(report, outcome.status);
        report.outcomes.push_back(std::move(outcome));
    }
    return report;
}

std::string_view toString(const SimulationCheckStatus status) noexcept
{
    switch (status) {
    case SimulationCheckStatus::Disabled: return "disabled";
    case SimulationCheckStatus::Passed: return "passed";
    case SimulationCheckStatus::Failed: return "failed";
    case SimulationCheckStatus::Unavailable: return "unavailable";
    }
    return "unavailable";
}

} // namespace wave
