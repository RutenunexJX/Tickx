#pragma once

#include "wave/model.h"
#include "wave/simulation_pipeline.h"

#include <QString>

#include <optional>
#include <string_view>

namespace wave {

inline constexpr auto SimulationSessionSchema =
    "wave-workbench.simulation-session/v2";
inline constexpr auto LegacySimulationSessionSchema =
    "wave-workbench.simulation-session/v1";
inline constexpr auto SimulationSessionExtension =
    "waveSimulation.session";

struct SimulationSessionParseResult {
    bool found{false};
    std::optional<SimulationRunRequest> request;
    QString error;

    [[nodiscard]] bool ok() const noexcept
    {
        return found && request.has_value();
    }
};

void attachSimulationSession(
    Project& project,
    const SimulationRunRequest& request);
[[nodiscard]] SimulationSessionParseResult simulationSessionFromProject(
    const Project& project);

enum class SimulationSessionState {
    Ready,
    Compiling,
    Running,
    Current,
    Stale,
    Failed,
};

struct SimulationSessionActions {
    bool runEnabled{false};
    bool stopEnabled{false};
    bool rerunEnabled{false};
};

class SimulationSessionStateMachine final {
public:
    void configure(bool runnable, bool hasCurrentResult) noexcept;
    void markStimulusEdited() noexcept;
    void markCurrent() noexcept;
    void markFailed() noexcept;
    [[nodiscard]] bool beginRun() noexcept;
    void observeStage(SimulationRunStage stage) noexcept;
    void finish(SimulationRunStatus status) noexcept;

    [[nodiscard]] SimulationSessionState state() const noexcept;
    [[nodiscard]] SimulationSessionActions actions() const noexcept;
    [[nodiscard]] bool runnable() const noexcept;

private:
    SimulationSessionState state_{SimulationSessionState::Ready};
    SimulationSessionState runOriginState_{SimulationSessionState::Ready};
    bool runnable_{false};
    bool staleDuringRun_{false};
};

[[nodiscard]] std::string_view toString(SimulationSessionState state) noexcept;

} // namespace wave
