#pragma once

#include "wave/compare.h"
#include "wave/stimulus_scenario.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wave {

enum class SimulationCheckStatus {
    Disabled,
    Passed,
    Failed,
    Unavailable,
};

struct SimulationCheckOutcome {
    std::string checkId;
    std::string name;
    SimulationCheckKind kind{SimulationCheckKind::ValueAtTick};
    SimulationCheckStatus status{SimulationCheckStatus::Unavailable};
    std::string laneId;
    std::string sourceLaneId;
    std::string targetLaneId;
    std::string traceSignalId;
    std::string sourceTraceSignalId;
    std::string targetTraceSignalId;
    Tick start{0};
    Tick end{0};
    std::optional<Tick> focusTick;
    std::string expected;
    std::string actual;
    std::string message;
    std::uint64_t sourceEventCount{0};
    std::uint64_t matchedEventCount{0};

    [[nodiscard]] bool operator==(const SimulationCheckOutcome&) const = default;
};

struct SimulationCheckReport {
    std::string projectId;
    std::string scenarioId;
    std::string traceId;
    std::vector<SimulationCheckOutcome> outcomes;
    std::uint64_t passedCount{0};
    std::uint64_t failedCount{0};
    std::uint64_t unavailableCount{0};
    std::uint64_t disabledCount{0};

    [[nodiscard]] bool allPassed() const noexcept
    {
        return failedCount == 0 && unavailableCount == 0;
    }
};

[[nodiscard]] SimulationCheckReport evaluateSimulationChecks(
    const Project& project,
    const Scenario& scenario,
    const TraceIndex& trace,
    const ImportedTrace& reference,
    const std::vector<SimulationCheckDefinition>& checks);

[[nodiscard]] std::string_view toString(SimulationCheckStatus status) noexcept;

} // namespace wave
