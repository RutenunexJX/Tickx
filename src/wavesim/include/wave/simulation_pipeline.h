#pragma once

#include "wave/simulation_runner.h"
#include "wave/trace.h"

#include <QJsonObject>
#include <QString>

#include <functional>
#include <memory>
#include <optional>
#include <string_view>

namespace wave {

inline constexpr auto SimulationRunReportSchema =
    "wave-workbench.simulation-run/v1";

enum class SimulationRunStage {
    ValidateInputs,
    ProbeToolchain,
    GenerateHarness,
    BuildModel,
    RunModel,
    ImportTrace,
    MaterializeProject,
    Completed,
};

enum class SimulationRunStatus {
    Succeeded,
    InvalidRequest,
    InvalidManifest,
    InvalidStimulus,
    ContractMismatch,
    UnsupportedFixture,
    ToolchainUnavailable,
    HarnessGenerationFailed,
    BuildFailed,
    RunFailed,
    TraceImportFailed,
    ResultProjectFailed,
    TimedOut,
    Cancelled,
};

struct SimulationArtifacts {
    QString runDirectory;
    QString harnessPath;
    QString objectDirectory;
    QString executablePath;
    QString vcdPath;
    QString resultProjectPath;
};

struct SimulationRunRequest {
    QString manifestPath;
    QString stimulusPath;
    QString workspaceRoot;
    QString artifactDirectory;
    QString resultProjectPath;
    ToolchainProbeOptions toolchain;
    int buildTimeoutMs{120'000};
    int runTimeoutMs{30'000};
    int maxOutputBytes{512 * 1024};
};

struct SimulationRunReport {
    SimulationRunStatus status{SimulationRunStatus::InvalidRequest};
    SimulationRunStage stage{SimulationRunStage::ValidateInputs};
    QString diagnostic;
    SimulationArtifacts artifacts;
    std::optional<ToolchainProbeReport> toolchain;
    std::optional<ProcessRunResult> buildProcess;
    std::optional<ProcessRunResult> simulationProcess;
    std::optional<TraceIndex> trace;
    qint64 durationMs{0};

    [[nodiscard]] bool ok() const noexcept
    {
        return status == SimulationRunStatus::Succeeded;
    }
};

class VerilatorSimulationRunner final {
public:
    using Completion = std::function<void(SimulationRunReport)>;

    VerilatorSimulationRunner();
    ~VerilatorSimulationRunner();

    VerilatorSimulationRunner(const VerilatorSimulationRunner&) = delete;
    VerilatorSimulationRunner& operator=(const VerilatorSimulationRunner&) = delete;
    VerilatorSimulationRunner(VerilatorSimulationRunner&&) = delete;
    VerilatorSimulationRunner& operator=(VerilatorSimulationRunner&&) = delete;

    [[nodiscard]] bool start(
        SimulationRunRequest request,
        Completion completion);
    [[nodiscard]] bool cancel();
    [[nodiscard]] bool running() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] QJsonObject simulationRunReportJson(
    const SimulationRunReport& report);

[[nodiscard]] std::string_view toString(SimulationRunStage stage) noexcept;
[[nodiscard]] std::string_view toString(SimulationRunStatus status) noexcept;

} // namespace wave
