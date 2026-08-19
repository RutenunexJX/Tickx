#pragma once

#include "wave/simulation_pipeline.h"

#include <QString>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wave {

enum class SimulationBatchState {
    Idle,
    Running,
    Completed,
    Cancelled,
};

enum class SimulationBatchScenarioState {
    Pending,
    Running,
    Succeeded,
    Failed,
    Cancelled,
};

struct SimulationBatchScenarioDescriptor {
    std::size_t scenarioIndex{0};
    std::string scenarioId;
    QString scenarioName;
};

struct SimulationBatchScenarioResult {
    SimulationBatchScenarioDescriptor scenario;
    SimulationBatchScenarioState state{SimulationBatchScenarioState::Pending};
    std::optional<SimulationRunStatus> runStatus;
    QString diagnostic;
    qint64 durationMs{0};
    bool buildCacheHit{false};
};

struct SimulationBatchSummary {
    std::size_t total{0};
    std::size_t pending{0};
    std::size_t running{0};
    std::size_t succeeded{0};
    std::size_t failed{0};
    std::size_t cancelled{0};

    [[nodiscard]] bool complete() const noexcept
    {
        return total > 0 && pending == 0 && running == 0;
    }
};

class SimulationBatchRun final {
public:
    [[nodiscard]] bool begin(
        std::vector<SimulationBatchScenarioDescriptor> scenarios);
    [[nodiscard]] bool requestCancel() noexcept;
    [[nodiscard]] bool completeCurrent(const SimulationRunReport& report);
    void reset() noexcept;

    [[nodiscard]] SimulationBatchState state() const noexcept;
    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] bool cancelRequested() const noexcept;
    [[nodiscard]] const SimulationBatchScenarioResult* current() const noexcept;
    [[nodiscard]] const std::vector<SimulationBatchScenarioResult>&
    scenarios() const noexcept;
    [[nodiscard]] SimulationBatchSummary summary() const noexcept;

private:
    void cancelPending() noexcept;
    void advance() noexcept;

    SimulationBatchState state_{SimulationBatchState::Idle};
    std::vector<SimulationBatchScenarioResult> scenarios_;
    std::optional<std::size_t> currentIndex_;
    bool cancelRequested_{false};
};

[[nodiscard]] std::string_view toString(SimulationBatchState state) noexcept;
[[nodiscard]] std::string_view toString(
    SimulationBatchScenarioState state) noexcept;

} // namespace wave
