#pragma once

#include "wave/model.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wave {

enum class ValidationCode {
    RelationSatisfied,
    RelationViolated,
    MissingSourceEvent,
    MissingTargetEvent,
    MultiplePossibleTargets,
    InsufficientTimeRange,
    ClockDomainMismatch,
    InvalidBusValue,
    UndefinedRegion,
    MissingLane,
    EventOutsideScenario,
    EventWaveformMismatch,
    EventCycleMismatch,
    EventClockDomainInvalid,
    LaneClockDomainInvalid,
    LaneGroupReferenceInvalid,
    RelationClockDomainInvalid,
    InvalidRelationCondition,
    RelationNotApplicable,
};

struct RelationConditionEvaluation {
    std::optional<bool> value;
    std::string error;
    std::size_t errorOffset{0};
    std::string laneId;

    [[nodiscard]] bool ok() const noexcept { return value.has_value(); }
};

using RelationConditionPredicate = std::function<std::optional<bool>(
    const Lane& lane,
    std::string_view literal,
    std::string& error)>;

struct ValidationIssue {
    ValidationCode code{ValidationCode::RelationSatisfied};
    Severity severity{Severity::Information};
    std::string message;
    std::string laneId;
    Tick tick{0};
    std::string eventId;
    std::string relationId;

    [[nodiscard]] bool operator==(const ValidationIssue&) const = default;
};

[[nodiscard]] std::vector<ValidationIssue> validateScenario(
    const Project& project,
    const Scenario& scenario);

[[nodiscard]] RelationConditionEvaluation evaluateRelationCondition(
    const Scenario& scenario,
    std::string_view expression,
    const RelationConditionPredicate& predicate);

[[nodiscard]] std::string_view toString(ValidationCode code) noexcept;

} // namespace wave
