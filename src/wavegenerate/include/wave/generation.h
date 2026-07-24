#pragma once

#include "wave/model.h"

#include <optional>
#include <string>
#include <vector>

namespace wave {

enum class GenerationDiagnosticCode {
    InvalidIdentifier,
    IdentifierCollision,
    MissingLane,
    UnsupportedAction,
    InvalidValue,
    TimeOverflow,
    SvaMissingClock,
    SvaMissingDisableCondition,
    SvaNonCycleDelay,
    SvaUnsupportedEvent,
    SvaUnsupportedCondition,
    SvaClockOverrideOverlap,
};

struct GenerationDiagnostic {
    GenerationDiagnosticCode code{GenerationDiagnosticCode::InvalidIdentifier};
    Severity severity{Severity::Error};
    std::string message;
    std::string objectId;

    [[nodiscard]] bool operator==(const GenerationDiagnostic&) const = default;
};

struct GenerationSignal {
    std::string laneId;
    std::string sourceName;
    std::string identifier;
    LaneKind kind{LaneKind::Bit};
    std::uint32_t width{1};
    bool isSigned{false};
    std::map<std::string, std::string> enumMap;
    std::string clockDomainId;
    std::vector<Segment> clockOverrides;
};

struct GenerationStep {
    std::string eventId;
    Tick tick{0};
    EventAction action{EventAction::Drive};
    std::string laneId;
    std::string value;
    std::string expectedResult;
    std::string clockDomainId;
    std::optional<std::int64_t> cycle;
    std::string description;
};

struct GenerationPlan {
    std::string projectId;
    std::string scenarioId;
    std::string scenarioName;
    TimeBase timeBase;
    Tick duration{0};
    std::vector<ClockDomain> clocks;
    std::vector<GenerationSignal> signalDefinitions;
    std::vector<GenerationStep> steps;
    std::vector<Relation> relations;
};

struct GenerationPlanResult {
    std::optional<GenerationPlan> plan;
    std::vector<GenerationDiagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept;
};

struct TextGenerationResult {
    std::string text;
    std::vector<GenerationDiagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept;
};

[[nodiscard]] GenerationPlanResult buildGenerationPlan(
    const Project& project,
    const Scenario& scenario);

[[nodiscard]] TextGenerationResult generateSystemVerilog(const GenerationPlan& plan);
[[nodiscard]] TextGenerationResult generateCocotb(const GenerationPlan& plan);
[[nodiscard]] TextGenerationResult generateSystemVerilogAssertions(const GenerationPlan& plan);

[[nodiscard]] std::string sanitizeIdentifier(std::string_view source);
[[nodiscard]] std::string_view toString(GenerationDiagnosticCode code) noexcept;

} // namespace wave
