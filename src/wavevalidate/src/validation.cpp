#include "wave/validation.h"

#include <algorithm>
#include <limits>
#include <sstream>
#include <unordered_map>

namespace wave {
namespace {

void appendUndefinedRegions(
    const Scenario& scenario,
    const Lane& lane,
    std::vector<ValidationIssue>& issues)
{
    if (lane.kind == LaneKind::Clock || lane.kind == LaneKind::Group || !lane.visible) {
        return;
    }
    Tick cursor = 0;
    for (const auto& segment : lane.segments) {
        if (segment.start > cursor) {
            issues.push_back({
                ValidationCode::UndefinedRegion,
                Severity::Warning,
                "Lane '" + lane.name + "' is undefined from "
                    + std::to_string(cursor) + " to " + std::to_string(segment.start) + " ticks",
                lane.id,
                cursor,
                {},
                {},
            });
        }
        cursor = std::max(cursor, segment.end);
    }
    if (cursor < scenario.duration) {
        issues.push_back({
            ValidationCode::UndefinedRegion,
            Severity::Warning,
            "Lane '" + lane.name + "' is undefined from "
                + std::to_string(cursor) + " to " + std::to_string(scenario.duration) + " ticks",
            lane.id,
            cursor,
            {},
            {},
        });
    }
}

void appendLaneClockIssue(
    const Project& project,
    const Lane& lane,
    std::vector<ValidationIssue>& issues)
{
    const auto clockMatches =
        lane.clockDomainId.empty()
        ? std::size_t{0}
        : static_cast<std::size_t>(
              std::count_if(
                  project.clockDomains.begin(),
                  project.clockDomains.end(),
                  [&lane](const ClockDomain& clock) {
                      return clock.id
                          == lane.clockDomainId;
                  }));
    const auto invalidGroupReference =
        lane.kind == LaneKind::Group
        && !lane.clockDomainId.empty();
    const auto invalidClockReference =
        lane.kind == LaneKind::Clock
        && (lane.clockDomainId.empty()
            || clockMatches != 1);
    const auto invalidOptionalReference =
        lane.kind != LaneKind::Clock
        && lane.kind != LaneKind::Group
        && !lane.clockDomainId.empty()
        && clockMatches != 1;
    if (!invalidGroupReference
        && !invalidClockReference
        && !invalidOptionalReference) {
        return;
    }

    std::string message;
    if (invalidGroupReference) {
        message =
            "Group Lane cannot reference a ClockDomain";
    } else if (lane.clockDomainId.empty()) {
        message =
            "Clock Lane requires a ClockDomain";
    } else if (clockMatches == 0) {
        message =
            "Lane ClockDomain does not exist";
    } else {
        message =
            "Lane ClockDomain ID is ambiguous";
    }
    issues.push_back({
        ValidationCode::LaneClockDomainInvalid,
        Severity::Error,
        std::move(message),
        lane.id,
        0,
        {},
        {},
    });
}

void appendLaneGroupIssue(
    const Scenario& scenario,
    const Lane& lane,
    std::vector<ValidationIssue>& issues)
{
    if (lane.groupId.empty()) return;

    const Lane* target = nullptr;
    std::size_t targetMatches = 0;
    for (const auto& candidate : scenario.lanes) {
        if (candidate.id != lane.groupId) continue;
        target = &candidate;
        ++targetMatches;
    }
    const auto valid =
        lane.kind != LaneKind::Group
        && targetMatches == 1
        && target
        && target->kind == LaneKind::Group
        && target->id != lane.id;
    if (valid) return;

    std::string message;
    if (lane.kind == LaneKind::Group) {
        message = "Group Lane cannot belong to another Group";
    } else if (targetMatches == 0) {
        message = "Lane Group does not exist";
    } else if (targetMatches > 1) {
        message = "Lane Group ID is ambiguous";
    } else if (target && target->id == lane.id) {
        message = "Lane cannot use itself as its Group";
    } else {
        message = "Lane groupId does not reference a Group Lane";
    }
    issues.push_back({
        ValidationCode::LaneGroupReferenceInvalid,
        Severity::Error,
        std::move(message),
        lane.id,
        0,
        {},
        {},
    });
}

const Event* uniqueEvent(
    const Scenario& scenario,
    std::string_view id,
    bool& multiple);

void appendRelationClockIssue(
    const Project& project,
    const Scenario& scenario,
    const Relation& relation,
    std::vector<ValidationIssue>& issues)
{
    if (relation.clockDomainId.empty()) return;

    const auto clockMatches =
        static_cast<std::size_t>(
            std::count_if(
                project.clockDomains.begin(),
                project.clockDomains.end(),
                [&relation](const ClockDomain& clock) {
                    return clock.id
                        == relation.clockDomainId;
                }));
    if (clockMatches == 1) return;

    const Event* anchor = nullptr;
    bool multiple = false;
    anchor = uniqueEvent(
        scenario,
        relation.sourceEventId,
        multiple);
    if (multiple) anchor = nullptr;
    if (!anchor) {
        anchor = uniqueEvent(
            scenario,
            relation.targetEventId,
            multiple);
        if (multiple) anchor = nullptr;
    }
    issues.push_back({
        ValidationCode::RelationClockDomainInvalid,
        Severity::Error,
        clockMatches == 0
            ? "Relation ClockDomain does not exist"
            : "Relation ClockDomain ID is ambiguous",
        anchor ? anchor->laneId : std::string{},
        anchor ? anchor->tick : Tick{0},
        anchor ? anchor->id : std::string{},
        relation.id,
    });
}

std::string eventName(const Scenario& scenario, const Event& event)
{
    const auto* lane = findLane(scenario, event.laneId);
    return lane ? lane->name + "@" + std::to_string(event.tick)
                : event.laneId + "@" + std::to_string(event.tick);
}

const Event* uniqueEvent(
    const Scenario& scenario,
    const std::string_view id,
    bool& multiple)
{
    const Event* match = nullptr;
    multiple = false;
    for (const auto& event : scenario.events) {
        if (event.id != id) continue;
        if (match) {
            multiple = true;
            return match;
        }
        match = &event;
    }
    return match;
}

std::string effectiveClockDomain(const Scenario& scenario, const Event& event)
{
    if (!event.clockDomainId.empty()) return event.clockDomainId;
    const auto* lane = findLane(scenario, event.laneId);
    return lane ? lane->clockDomainId : std::string{};
}

bool actionControlsWaveform(const EventAction action)
{
    return action == EventAction::Drive
        || action == EventAction::Expect
        || action == EventAction::Pulse
        || action == EventAction::Toggle;
}

void appendEventCycleIssues(
    const Project& project,
    const Scenario& scenario,
    std::vector<ValidationIssue>& issues)
{
    for (const auto& event : scenario.events) {
        if (!event.cycle) continue;

        const auto clockDomainId =
            effectiveClockDomain(scenario, event);
        const ClockDomain* clock = nullptr;
        std::size_t clockMatches = 0;
        if (!clockDomainId.empty()) {
            for (const auto& candidate :
                 project.clockDomains) {
                if (candidate.id != clockDomainId) {
                    continue;
                }
                clock = &candidate;
                ++clockMatches;
            }
        }

        std::string message;
        if (*event.cycle < 0) {
            message =
                "Event cycle index must be non-negative";
        } else if (clockDomainId.empty()
                   || clockMatches != 1
                   || !clock) {
            message =
                "Cycle-based Event does not resolve one clock domain";
        } else {
            const auto expectedTick =
                tickAtCycle(
                    *clock,
                    *event.cycle,
                    clock->activeEdge);
            if (!expectedTick) {
                message =
                    "Event cycle cannot be represented by its clock domain";
            } else if (*expectedTick != event.tick) {
                message =
                    "Cycle-based Event time does not match its clock active edge";
            } else {
                continue;
            }
        }

        issues.push_back({
            ValidationCode::EventCycleMismatch,
            Severity::Error,
            std::move(message),
            event.laneId,
            event.tick,
            event.id,
            {},
        });
    }
}

void appendEventClockIssues(
    const Project& project,
    const Scenario& scenario,
    std::vector<ValidationIssue>& issues)
{
    for (const auto& event : scenario.events) {
        if (event.clockDomainId.empty()) continue;
        const auto clockMatches =
            std::count_if(
                project.clockDomains.begin(),
                project.clockDomains.end(),
                [&event](const ClockDomain& clock) {
                    return clock.id
                        == event.clockDomainId;
                });
        if (clockMatches == 1) continue;
        issues.push_back({
            ValidationCode::EventClockDomainInvalid,
            Severity::Error,
            clockMatches == 0
                ? "Event ClockDomain does not exist"
                : "Event ClockDomain ID is ambiguous",
            event.laneId,
            event.tick,
            event.id,
            {},
        });
    }
}

bool waveformLinkIdentitiesAreStable(
    const Scenario& scenario)
{
    std::unordered_map<std::string, std::size_t>
        laneIdCounts;
    std::unordered_map<std::string, std::size_t>
        segmentIdCounts;
    std::unordered_map<std::string, std::size_t>
        eventIdCounts;
    for (const auto& lane : scenario.lanes) {
        if (lane.id.empty()
            || ++laneIdCounts[lane.id] != 1) {
            return false;
        }
        for (const auto& segment : lane.segments) {
            if (segment.id.empty()
                || ++segmentIdCounts[segment.id] != 1) {
                return false;
            }
        }
    }
    for (const auto& event : scenario.events) {
        if (event.id.empty()
            || ++eventIdCounts[event.id] != 1) {
            return false;
        }
    }
    return true;
}

void appendWaveformEventIssues(
    const Scenario& scenario,
    std::vector<ValidationIssue>& issues)
{
    if (!waveformLinkIdentitiesAreStable(scenario)) {
        return;
    }
    for (const auto& event : scenario.events) {
        if (!event.waveformLinked) continue;
        const auto* currentLane =
            findLane(scenario, event.laneId);
        if (!currentLane) {
            continue;
        }

        const Lane* linkedLane = nullptr;
        const Segment* linkedSegment = nullptr;
        std::size_t segmentMatches = 0;
        for (const auto& lane : scenario.lanes) {
            for (const auto& segment : lane.segments) {
                if (segment.id
                    != event.linkedSegmentId) {
                    continue;
                }
                linkedLane = &lane;
                linkedSegment = &segment;
                ++segmentMatches;
            }
        }
        const auto linkedEventCount =
            event.linkedSegmentId.empty()
            ? std::size_t{0}
            : static_cast<std::size_t>(
                  std::count_if(
                      scenario.events.begin(),
                      scenario.events.end(),
                      [&event](const Event& candidate) {
                          return candidate.waveformLinked
                              && candidate.linkedSegmentId
                                  == event.linkedSegmentId;
                      }));
        auto mismatch =
            event.linkedSegmentId.empty()
            || segmentMatches != 1
            || !linkedLane
            || !linkedSegment
            || linkedEventCount != 1
            || linkedLane->kind == LaneKind::Clock
            || linkedLane->kind == LaneKind::Group
            || !actionControlsWaveform(event.action);
        if (!mismatch) {
            const auto valueValidation =
                validateLaneValue(
                    *linkedLane,
                    linkedSegment->value);
            if (!valueValidation.valid) {
                continue;
            }
            mismatch =
                event.laneId != linkedLane->id
                || event.tick != linkedSegment->start
                || event.value
                    != valueValidation.normalizedValue;
        }
        if (!mismatch) continue;

        issues.push_back({
            ValidationCode::EventWaveformMismatch,
            Severity::Error,
            "Waveform-linked Event does not match its Segment",
            event.laneId,
            event.tick,
            event.id,
            {},
        });
    }
}

std::optional<std::string> expectedValueAt(
    const Project& project,
    const Lane& lane,
    const Tick tick,
    std::string& error)
{
    if (lane.kind == LaneKind::Clock) {
        const auto* clock = findClock(project, lane.clockDomainId);
        if (!clock || !clock->isValid()) {
            error = "clock domain for lane '" + lane.name + "' is missing or invalid";
            return std::nullopt;
        }
        return std::string(1, clockValueAt(*clock, lane, tick));
    }
    const auto iterator = std::upper_bound(
        lane.segments.begin(),
        lane.segments.end(),
        tick,
        [](const Tick value, const Segment& segment) {
            return value < segment.start;
        });
    if (iterator == lane.segments.begin()) {
        error = "lane '" + lane.name + "' has no value at the source event";
        return std::nullopt;
    }
    const auto& segment = *std::prev(iterator);
    if (tick < segment.start || tick >= segment.end) {
        error = "lane '" + lane.name + "' has no value at the source event";
        return std::nullopt;
    }
    return segment.value;
}

std::optional<bool> expectedValueEquals(
    const Project& project,
    const Lane& lane,
    const Tick tick,
    const std::string_view literal,
    std::string& error)
{
    const auto sampled = expectedValueAt(project, lane, tick, error);
    if (!sampled) return std::nullopt;
    if (lane.kind == LaneKind::Transaction || lane.kind == LaneKind::Event) {
        return *sampled == literal;
    }

    Lane literalLane = lane;
    if (lane.kind == LaneKind::Clock) literalLane.kind = LaneKind::Bit;
    const auto literalValidation = validateLaneValue(literalLane, literal);
    if (!literalValidation.valid) {
        error = "literal '" + std::string(literal) + "' is invalid for lane '"
            + lane.name + "': " + literalValidation.error;
        return std::nullopt;
    }
    const auto sampledBits = laneValueBits(
        lane,
        *sampled,
        LaneValueEncoding::ProjectLiteral);
    const auto literalBits = laneValueBits(
        lane,
        literalValidation.normalizedValue,
        LaneValueEncoding::ProjectLiteral);
    if (!sampledBits || !literalBits) {
        error = "lane '" + lane.name + "' contains a value that cannot be normalized";
        return std::nullopt;
    }
    return *sampledBits == *literalBits;
}

void validateRelation(
    const Project& project,
    const Scenario& scenario,
    const Relation& relation,
    std::vector<ValidationIssue>& issues)
{
    bool multipleSource = false;
    const auto* source = uniqueEvent(scenario, relation.sourceEventId, multipleSource);
    if (!source) {
        issues.push_back({
            ValidationCode::MissingSourceEvent,
            Severity::Error,
            "Relation source event is missing",
            {},
            0,
            relation.sourceEventId,
            relation.id,
        });
        return;
    }
    if (multipleSource) {
        issues.push_back({
            ValidationCode::MultiplePossibleTargets,
            Severity::Error,
            "Relation source ID is not unique",
            source->laneId,
            source->tick,
            source->id,
            relation.id,
        });
        return;
    }

    if (!relation.condition.empty()) {
        const auto condition = evaluateRelationCondition(
            scenario,
            relation.condition,
            [&](const Lane& lane, const std::string_view literal, std::string& error) {
                return expectedValueEquals(project, lane, source->tick, literal, error);
            });
        if (!condition.ok()) {
            issues.push_back({
                ValidationCode::InvalidRelationCondition,
                Severity::Error,
                "Relation condition error at byte "
                    + std::to_string(condition.errorOffset) + ": " + condition.error,
                condition.laneId.empty() ? source->laneId : condition.laneId,
                source->tick,
                source->id,
                relation.id,
            });
            return;
        }
        if (!*condition.value) {
            issues.push_back({
                ValidationCode::RelationNotApplicable,
                Severity::Information,
                "Relation condition is false at the source event; relation is not applicable",
                source->laneId,
                source->tick,
                source->id,
                relation.id,
            });
            return;
        }
    }

    const Event* target = nullptr;
    if (!relation.targetEventId.empty()) {
        bool multipleTarget = false;
        target = uniqueEvent(scenario, relation.targetEventId, multipleTarget);
        if (multipleTarget) {
            issues.push_back({
                ValidationCode::MultiplePossibleTargets,
                Severity::Error,
                "Relation target ID resolves to multiple events",
                source->laneId,
                source->tick,
                source->id,
                relation.id,
            });
            return;
        }
    } else {
        std::vector<const Event*> candidates;
        for (const auto& event : scenario.events) {
            if (event.id == source->id || event.tick < source->tick) continue;
            const auto delay = event.tick - source->tick;
            if (delay >= relation.minimumDelay && delay <= relation.maximumDelay) {
                candidates.push_back(&event);
            }
        }
        if (candidates.size() > 1) {
            issues.push_back({
                ValidationCode::MultiplePossibleTargets,
                Severity::Error,
                "Relation has multiple possible target events",
                source->laneId,
                source->tick,
                source->id,
                relation.id,
            });
            return;
        }
        if (candidates.size() == 1) target = candidates.front();
    }

    if (!target) {
        issues.push_back({
            ValidationCode::MissingTargetEvent,
            relation.severity,
            "Relation target event is missing",
            source->laneId,
            source->tick,
            source->id,
            relation.id,
        });
        return;
    }

    if (relation.maximumDelay >= 0
        && source->tick <= std::numeric_limits<Tick>::max() - relation.maximumDelay
        && source->tick + relation.maximumDelay > scenario.duration) {
        issues.push_back({
            ValidationCode::InsufficientTimeRange,
            Severity::Warning,
            "Scenario ends before the relation observation window closes",
            source->laneId,
            source->tick,
            source->id,
            relation.id,
        });
    }

    const auto sourceClock = effectiveClockDomain(scenario, *source);
    const auto targetClock = effectiveClockDomain(scenario, *target);
    const auto expectedClock = relation.clockDomainId;
    const auto clockMismatch = (!expectedClock.empty()
                                   && ((!sourceClock.empty() && sourceClock != expectedClock)
                                       || (!targetClock.empty() && targetClock != expectedClock)))
        || (!sourceClock.empty() && !targetClock.empty() && sourceClock != targetClock);
    if (clockMismatch) {
        issues.push_back({
            ValidationCode::ClockDomainMismatch,
            Severity::Error,
            "Relation events use incompatible clock domains",
            target->laneId,
            target->tick,
            target->id,
            relation.id,
        });
        return;
    }

    const auto delay = target->tick - source->tick;
    if (delay < relation.minimumDelay || delay > relation.maximumDelay) {
        std::ostringstream message;
        message << "Relation violated: observed " << delay << " ticks, expected "
                << relation.minimumDelay << ".." << relation.maximumDelay;
        issues.push_back({
            ValidationCode::RelationViolated,
            relation.severity,
            message.str(),
            target->laneId,
            target->tick,
            target->id,
            relation.id,
        });
        return;
    }

    issues.push_back({
        ValidationCode::RelationSatisfied,
        Severity::Information,
        "Relation satisfied: " + eventName(scenario, *source)
            + " -> " + eventName(scenario, *target)
            + " in " + std::to_string(delay) + " ticks",
        target->laneId,
        target->tick,
        target->id,
        relation.id,
    });
}

int severityRank(const Severity severity)
{
    switch (severity) {
    case Severity::Error: return 0;
    case Severity::Warning: return 1;
    case Severity::Information: return 2;
    }
    return 2;
}

} // namespace

std::vector<ValidationIssue> validateScenario(
    const Project& project,
    const Scenario& scenario)
{
    std::vector<ValidationIssue> issues;

    for (const auto& lane : scenario.lanes) {
        appendLaneClockIssue(
            project, lane, issues);
        appendLaneGroupIssue(
            scenario, lane, issues);
        appendUndefinedRegions(scenario, lane, issues);
        for (const auto& segment : lane.segments) {
            const auto validation = validateLaneValue(lane, segment.value);
            if (!validation.valid) {
                issues.push_back({
                    ValidationCode::InvalidBusValue,
                    Severity::Error,
                    "Invalid value in lane '" + lane.name + "': " + validation.error,
                    lane.id,
                    segment.start,
                    {},
                    {},
                });
            }
        }
    }

    for (const auto& event : scenario.events) {
        if (event.tick < 0 || event.tick >= scenario.duration) {
            issues.push_back({
                ValidationCode::EventOutsideScenario,
                Severity::Error,
                "Event is outside the scenario duration",
                event.laneId,
                event.tick,
                event.id,
                {},
            });
        }
        if (!event.laneId.empty() && !findLane(scenario, event.laneId)) {
            issues.push_back({
                ValidationCode::MissingLane,
                Severity::Error,
                "Event target lane is missing",
                event.laneId,
                event.tick,
                event.id,
                {},
            });
        }
    }
    appendEventClockIssues(project, scenario, issues);
    appendEventCycleIssues(project, scenario, issues);
    appendWaveformEventIssues(scenario, issues);

    for (const auto& relation : scenario.relations) {
        appendRelationClockIssue(
            project, scenario, relation, issues);
        if (relation.minimumDelay < 0 || relation.maximumDelay < relation.minimumDelay) {
            issues.push_back({
                ValidationCode::RelationViolated,
                Severity::Error,
                "Relation delay range is invalid",
                {},
                0,
                {},
                relation.id,
            });
            continue;
        }
        validateRelation(project, scenario, relation, issues);
    }

    std::stable_sort(
        issues.begin(),
        issues.end(),
        [](const ValidationIssue& left, const ValidationIssue& right) {
            const auto leftRank = severityRank(left.severity);
            const auto rightRank = severityRank(right.severity);
            return leftRank < rightRank
                || (leftRank == rightRank && left.tick < right.tick);
        });
    return issues;
}

std::string_view toString(const ValidationCode code) noexcept
{
    switch (code) {
    case ValidationCode::RelationSatisfied: return "relation-satisfied";
    case ValidationCode::RelationViolated: return "relation-violated";
    case ValidationCode::MissingSourceEvent: return "missing-source-event";
    case ValidationCode::MissingTargetEvent: return "missing-target-event";
    case ValidationCode::MultiplePossibleTargets: return "multiple-possible-targets";
    case ValidationCode::InsufficientTimeRange: return "insufficient-time-range";
    case ValidationCode::ClockDomainMismatch: return "clock-domain-mismatch";
    case ValidationCode::InvalidBusValue: return "invalid-bus-value";
    case ValidationCode::UndefinedRegion: return "undefined-region";
    case ValidationCode::MissingLane: return "missing-lane";
    case ValidationCode::EventOutsideScenario: return "event-outside-scenario";
    case ValidationCode::EventWaveformMismatch: return "event-waveform-mismatch";
    case ValidationCode::EventCycleMismatch: return "event-cycle-mismatch";
    case ValidationCode::EventClockDomainInvalid: return "event-clock-domain-invalid";
    case ValidationCode::LaneClockDomainInvalid: return "lane-clock-domain-invalid";
    case ValidationCode::LaneGroupReferenceInvalid: return "lane-group-reference-invalid";
    case ValidationCode::RelationClockDomainInvalid: return "relation-clock-domain-invalid";
    case ValidationCode::InvalidRelationCondition: return "invalid-relation-condition";
    case ValidationCode::RelationNotApplicable: return "relation-not-applicable";
    }
    return "unknown";
}

} // namespace wave
