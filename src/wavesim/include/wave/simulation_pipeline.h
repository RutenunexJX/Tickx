#pragma once

#include "wave/module_manifest.h"
#include "wave/simulation_runner.h"
#include "wave/trace.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace wave {

inline constexpr auto SimulationRunReportSchema =
    "wave-workbench.simulation-run/v1";

enum class SimulationRunStage {
    ValidateInputs,
    ProbeToolchain,
    GenerateHarness,
    ResolveBuildCache,
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
    Superseded,
};

struct SimulationArtifacts {
    QString runDirectory;
    QString harnessPath;
    QString stubPath;
    QString runtimeStimulusPath;
    QString objectDirectory;
    QString executablePath;
    QString vcdPath;
    QString resultProjectPath;
};

struct SimulationBuildCacheReport {
    QString fingerprint;
    QString directory;
    QString diagnostic;
    bool hit{false};
    bool published{false};
};

struct SimulationRunRequest {
    QString manifestPath;
    QString stimulusPath;
    QString workspaceRoot;
    QString artifactDirectory;
    QString buildCacheDirectory;
    QString scenarioDirectory;
    QString resultProjectPath;
    QStringList stubbedModules;
    ToolchainProbeOptions toolchain;
    int buildTimeoutMs{120'000};
    int runTimeoutMs{30'000};
    int maxOutputBytes{512 * 1024};
    quint64 generation{0};
};

struct SimulationSourceDiagnostic {
    QString sourceFile;
    int line{0};
    int column{0};
    QString severity;
    QString stage;
    QString message;
    QString code;
};

struct SimulationRunReport {
    SimulationRunStatus status{SimulationRunStatus::InvalidRequest};
    SimulationRunStage stage{SimulationRunStage::ValidateInputs};
    quint64 generation{0};
    QString diagnostic;
    SimulationArtifacts artifacts;
    SimulationBuildCacheReport buildCache;
    std::optional<ToolchainProbeReport> toolchain;
    std::optional<ProcessRunResult> buildProcess;
    std::optional<ProcessRunResult> simulationProcess;
    std::optional<TraceIndex> trace;
    std::vector<SimulationSourceDiagnostic> diagnostics;
    qint64 durationMs{0};

    [[nodiscard]] bool ok() const noexcept
    {
        return status == SimulationRunStatus::Succeeded;
    }
};

struct StructuredSimulationWrapperResult {
    QByteArray document;
    QString error;

    [[nodiscard]] bool ok() const noexcept
    {
        return !document.isEmpty() && error.isEmpty();
    }
};

struct PassiveSimulationStubResult {
    QByteArray document;
    QString error;

    [[nodiscard]] bool ok() const noexcept
    {
        return !document.isEmpty() && error.isEmpty();
    }
};

class VerilatorSimulationRunner final {
public:
    using Completion = std::function<void(SimulationRunReport)>;
    using StageChanged = std::function<void(quint64, SimulationRunStage)>;

    VerilatorSimulationRunner();
    ~VerilatorSimulationRunner();

    VerilatorSimulationRunner(const VerilatorSimulationRunner&) = delete;
    VerilatorSimulationRunner& operator=(const VerilatorSimulationRunner&) = delete;
    VerilatorSimulationRunner(VerilatorSimulationRunner&&) = delete;
    VerilatorSimulationRunner& operator=(VerilatorSimulationRunner&&) = delete;

    [[nodiscard]] bool start(
        SimulationRunRequest request,
        Completion completion,
        StageChanged stageChanged = {});
    [[nodiscard]] bool cancel();
    [[nodiscard]] bool running() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] QJsonObject simulationRunReportJson(
    const SimulationRunReport& report);

[[nodiscard]] quint64 nextSimulationGeneration() noexcept;

[[nodiscard]] StructuredSimulationWrapperResult
generateStructuredSimulationWrapper(
    const ZeroSlackModuleManifest& manifest);

[[nodiscard]] PassiveSimulationStubResult generatePassiveSimulationStubs(
    const ZeroSlackModuleManifest& manifest,
    const QStringList& selectedModules);

[[nodiscard]] std::string_view toString(SimulationRunStage stage) noexcept;
[[nodiscard]] std::string_view toString(SimulationRunStatus status) noexcept;

} // namespace wave
