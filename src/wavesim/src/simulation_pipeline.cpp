#include "wave/simulation_pipeline.h"

#include "wave/module_manifest.h"
#include "wave/stimulus_scenario.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QSaveFile>
#include <QTextStream>
#include <QTimer>
#include <QUuid>

#include <algorithm>
#include <filesystem>
#include <limits>
#include <optional>
#include <utility>

namespace wave {
namespace {

QString text(const std::string_view value)
{
    return QString::fromLatin1(
        value.data(), static_cast<qsizetype>(value.size()));
}

QString qString(const std::string& value)
{
    return QString::fromUtf8(value);
}

std::filesystem::path filesystemPath(const QString& path)
{
#ifdef Q_OS_WIN
    return std::filesystem::path(path.toStdWString());
#else
    return std::filesystem::path(path.toStdString());
#endif
}

bool readDocument(const QString& path, QByteArray& document, QString& error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("Cannot read %1: %2")
                    .arg(QDir::toNativeSeparators(path), file.errorString());
        return false;
    }
    document = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        error = QStringLiteral("Cannot read %1: %2")
                    .arg(QDir::toNativeSeparators(path), file.errorString());
        return false;
    }
    return true;
}

bool writeDocument(const QString& path, const QByteArray& document, QString& error)
{
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(document) != document.size()
        || !file.commit()) {
        error = QStringLiteral("Cannot write %1: %2")
                    .arg(QDir::toNativeSeparators(path), file.errorString());
        return false;
    }
    return true;
}

bool cppIdentifier(const QString& value)
{
    if (value.isEmpty()) return false;
    const auto first = value.front();
    if (!(first == QLatin1Char('_') || first.isLetter())) return false;
    return std::all_of(
        value.cbegin() + 1,
        value.cend(),
        [](const QChar character) {
            return character == QLatin1Char('_') || character.isLetterOrNumber();
        });
}

struct PreparedSimulation {
    ZeroSlackModuleManifest manifest;
    ZeroSlackStimulusScenario stimulus;
    QStringList sourceFiles;
    QStringList includeDirectories;
};

std::optional<PreparedSimulation> prepareSimulation(
    const SimulationRunRequest& request,
    SimulationRunStatus& status,
    QString& error)
{
    const QFileInfo manifestInfo(request.manifestPath);
    const QFileInfo stimulusInfo(request.stimulusPath);
    const QFileInfo workspaceInfo(request.workspaceRoot);
    if (!manifestInfo.isFile() || !stimulusInfo.isFile()
        || !workspaceInfo.isDir() || request.artifactDirectory.trimmed().isEmpty()) {
        status = SimulationRunStatus::InvalidRequest;
        error = QStringLiteral(
            "Manifest, stimulus, workspace root, and artifact directory are required.");
        return std::nullopt;
    }

    QByteArray manifestDocument;
    if (!readDocument(manifestInfo.absoluteFilePath(), manifestDocument, error)) {
        status = SimulationRunStatus::InvalidManifest;
        return std::nullopt;
    }
    const auto parsedManifest = parseZeroSlackModuleManifest(manifestDocument);
    if (!parsedManifest.ok()) {
        status = SimulationRunStatus::InvalidManifest;
        error = parsedManifest.error;
        return std::nullopt;
    }

    QByteArray stimulusDocument;
    if (!readDocument(stimulusInfo.absoluteFilePath(), stimulusDocument, error)) {
        status = SimulationRunStatus::InvalidStimulus;
        return std::nullopt;
    }
    const auto parsedStimulus = parseZeroSlackStimulusScenario(stimulusDocument);
    if (!parsedStimulus.ok()) {
        status = SimulationRunStatus::InvalidStimulus;
        error = parsedStimulus.error;
        return std::nullopt;
    }

    const auto restored = restoreZeroSlackStimulusScenario(
        *parsedManifest.manifest, *parsedStimulus.scenario);
    if (!restored.ok() || restored.manifestChanged
        || restored.missingSavedPortCount != 0
        || restored.incompatiblePortCount != 0
        || restored.newPortCount != 0) {
        status = SimulationRunStatus::ContractMismatch;
        error = restored.error.isEmpty()
            ? QStringLiteral(
                  "Manifest and stimulus do not describe the same exact module contract.")
            : restored.error;
        return std::nullopt;
    }

    PreparedSimulation prepared;
    prepared.manifest = *parsedManifest.manifest;
    prepared.stimulus = *parsedStimulus.scenario;
    const QDir workspace(workspaceInfo.absoluteFilePath());
    for (const auto& source : prepared.manifest.sources) {
        if (source.role == "header") continue;
        const auto path = QFileInfo(workspace.filePath(qString(source.path)))
                              .absoluteFilePath();
        if (!QFileInfo::exists(path) || !QFileInfo(path).isFile()) {
            status = SimulationRunStatus::InvalidManifest;
            error = QStringLiteral("Manifest source does not exist: %1")
                        .arg(QDir::toNativeSeparators(path));
            return std::nullopt;
        }
        prepared.sourceFiles.append(path);
    }
    if (prepared.sourceFiles.isEmpty()) {
        status = SimulationRunStatus::InvalidManifest;
        error = QStringLiteral("Manifest contains no compilable design source.");
        return std::nullopt;
    }
    for (const auto& include : prepared.manifest.includeDirs) {
        const auto path = QFileInfo(workspace.filePath(qString(include)))
                              .absoluteFilePath();
        if (!QFileInfo(path).isDir()) {
            status = SimulationRunStatus::InvalidManifest;
            error = QStringLiteral("Manifest include directory does not exist: %1")
                        .arg(QDir::toNativeSeparators(path));
            return std::nullopt;
        }
        prepared.includeDirectories.append(path);
    }

    if (!cppIdentifier(qString(prepared.manifest.target.module))) {
        status = SimulationRunStatus::UnsupportedFixture;
        error = QStringLiteral("S5 supports top-module names that map directly to C++ identifiers.");
        return std::nullopt;
    }
    if (prepared.stimulus.duration <= 0
        || prepared.stimulus.timeBase.picosecondsPerTick <= 0
        || prepared.stimulus.duration
               > std::numeric_limits<Tick>::max()
                   / prepared.stimulus.timeBase.picosecondsPerTick) {
        status = SimulationRunStatus::UnsupportedFixture;
        error = QStringLiteral("Stimulus duration or timebase exceeds the S5 runner range.");
        return std::nullopt;
    }

    for (const auto& manifestPort : prepared.manifest.ports) {
        const auto& shape = manifestPort.type.shape;
        if (!cppIdentifier(qString(manifestPort.name))
            || !shape.semanticAvailable || !shape.fixedSize || !shape.integral
            || shape.unpackedArray || shape.interfaceType || shape.bitWidth == 0
            || shape.bitWidth > 64
            || manifestPort.direction == ModulePortDirection::Inout
            || manifestPort.direction == ModulePortDirection::Ref
            || manifestPort.direction == ModulePortDirection::Interface
            || manifestPort.direction == ModulePortDirection::Unknown) {
            status = SimulationRunStatus::UnsupportedFixture;
            error = QStringLiteral(
                "S5 supports fixed integral input/output ports up to 64 bits; unsupported port: %1")
                        .arg(qString(manifestPort.name));
            return std::nullopt;
        }
    }
    status = SimulationRunStatus::Succeeded;
    return prepared;
}

std::optional<quint64> binaryValue(
    const StimulusScenarioPort& port,
    const std::string& value)
{
    Lane lane;
    lane.kind = port.kind == LaneKind::Clock ? LaneKind::Bit : port.kind;
    lane.width = port.binding.width;
    lane.isSigned = port.binding.isSigned;
    lane.radix = port.radix;
    lane.enumMap = port.enumMap;
    const auto bits = laneValueBits(lane, value);
    if (!bits || bits->size() > 64
        || std::any_of(bits->begin(), bits->end(), [](const char bit) {
               return bit != '0' && bit != '1';
           })) {
        return std::nullopt;
    }
    quint64 result = 0;
    for (const auto bit : *bits) result = (result << 1U) | (bit == '1' ? 1U : 0U);
    return result;
}

QString assignmentFunction(
    const StimulusScenarioPort& port,
    QString& error)
{
    QString output;
    QTextStream stream(&output);
    const auto name = qString(port.binding.name);
    stream << "static std::uint64_t drive_" << name
           << "(const std::uint64_t tick)\n{\n";
    if (port.kind == LaneKind::Clock) {
        if (!port.clock) {
            error = QStringLiteral("Clock port %1 has no clock configuration.").arg(name);
            return {};
        }
        const auto& clock = *port.clock;
        const auto highTicks = clock.period * clock.dutyCycle.numerator
            / clock.dutyCycle.denominator;
        stream << "    std::uint64_t value = tick < " << clock.phase
               << "ULL ? 0ULL : (((tick - " << clock.phase << "ULL) % "
               << clock.period << "ULL) < " << highTicks << "ULL ? 1ULL : 0ULL);\n";
        for (const auto& range : port.segments) {
            const auto mode = clockOverrideModeFromString(range.value);
            if (!mode || *mode == ClockOverrideMode::Disabled) {
                error = QStringLiteral(
                    "S5 does not support unknown-valued clock override on %1.").arg(name);
                return {};
            }
            stream << "    if (tick >= " << range.start << "ULL && tick < "
                   << range.end << "ULL) value = 0ULL;\n";
        }
        stream << "    return value;\n}\n\n";
        return output;
    }

    if (port.segments.empty()) {
        error = QStringLiteral("Stimulus input %1 has no ranges.").arg(name);
        return {};
    }
    for (std::size_t index = 0; index < port.segments.size(); ++index) {
        const auto& range = port.segments[index];
        const auto value = binaryValue(port, range.value);
        if (!value) {
            error = QStringLiteral("Stimulus value for %1 is not a known integral value.")
                        .arg(name);
            return {};
        }
        stream << (index == 0 ? "    if" : "    else if")
               << " (tick < " << range.end << "ULL) return 0x"
               << QString::number(*value, 16) << "ULL;\n";
        if (index + 1 == port.segments.size()) {
            stream << "    return 0x" << QString::number(*value, 16) << "ULL;\n";
        }
    }
    stream << "}\n\n";
    return output;
}

std::optional<QByteArray> makeHarness(
    const PreparedSimulation& prepared,
    QString& error)
{
    QString functions;
    QString assignments;
    QTextStream assignmentStream(&assignments);
    for (const auto& port : prepared.stimulus.ports) {
        if (port.binding.direction != ModulePortDirection::Input) continue;
        const auto function = assignmentFunction(port, error);
        if (!error.isEmpty()) return std::nullopt;
        functions += function;
        assignmentStream << "        top->" << qString(port.binding.name)
                         << " = drive_" << qString(port.binding.name) << "(tick);\n";
    }

    QString output;
    QTextStream stream(&output);
    stream
        << "// Generated from Module Manifest " << qString(prepared.manifest.identity) << "\n"
        << "// and Stimulus Scenario " << qString(prepared.stimulus.identity) << "\n"
        << "#include \"Vwave_fixture.h\"\n"
           "#include \"verilated.h\"\n"
           "#include \"verilated_vcd_c.h\"\n\n"
           "#include <cstdint>\n"
           "#include <memory>\n"
           "#include <string>\n\n"
        << functions
        << "int main(int argc, char** argv)\n{\n"
           "    std::string vcdPath;\n"
           "    for (int index = 1; index < argc; ++index) {\n"
           "        const std::string argument(argv[index]);\n"
           "        if (argument.rfind(\"--vcd=\", 0) == 0) vcdPath = argument.substr(6);\n"
           "    }\n"
           "    if (vcdPath.empty()) return 2;\n\n"
           "    auto context = std::make_unique<VerilatedContext>();\n"
           "    context->commandArgs(argc, argv);\n"
           "    context->traceEverOn(true);\n"
           "    auto top = std::make_unique<Vwave_fixture>(context.get());\n"
           "    auto trace = std::make_unique<VerilatedVcdC>();\n"
           "    top->trace(trace.get(), 8);\n"
           "    trace->open(vcdPath.c_str());\n\n"
        << "    constexpr std::uint64_t duration = " << prepared.stimulus.duration << "ULL;\n"
        << "    constexpr std::uint64_t picosecondsPerTick = "
        << prepared.stimulus.timeBase.picosecondsPerTick << "ULL;\n"
           "    for (std::uint64_t tick = 0; tick <= duration; ++tick) {\n"
        << assignments
        << "        context->time(tick * picosecondsPerTick);\n"
           "        top->eval();\n"
           "        trace->dump(context->time());\n"
           "    }\n"
           "    top->final();\n"
           "    trace->close();\n"
           "    return 0;\n"
           "}\n";
    return output.toUtf8();
}

QString simulatorFileName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("wave_fixture_sim.exe");
#else
    return QStringLiteral("wave_fixture_sim");
#endif
}

QJsonObject processJson(const ProcessRunResult& process)
{
    return {
        {QStringLiteral("status"), text(toString(process.state))},
        {QStringLiteral("requestedProgram"), process.requestedProgram},
        {QStringLiteral("resolvedProgram"), process.resolvedProgram},
        {QStringLiteral("arguments"), QJsonArray::fromStringList(process.arguments)},
        {QStringLiteral("exitCode"), process.exitCode},
        {QStringLiteral("durationMs"), process.durationMs},
        {QStringLiteral("stdout"), QString::fromUtf8(process.standardOutput)},
        {QStringLiteral("stderr"), QString::fromUtf8(process.standardError)},
        {QStringLiteral("stdoutTruncated"), process.standardOutputTruncated},
        {QStringLiteral("stderrTruncated"), process.standardErrorTruncated},
        {QStringLiteral("diagnostic"), process.errorMessage},
    };
}

SimulationRunStatus processFailureStatus(
    const ProcessRunResult& process,
    const SimulationRunStatus ordinaryFailure)
{
    if (process.state == ProcessRunState::Cancelled) {
        return SimulationRunStatus::Cancelled;
    }
    if (process.state == ProcessRunState::TimedOut) {
        return SimulationRunStatus::TimedOut;
    }
    return ordinaryFailure;
}

} // namespace

struct VerilatorSimulationRunner::Impl {
    ToolchainProbeRunner probeRunner;
    ProcessRunner processRunner;
    SimulationRunRequest request;
    Completion completion;
    SimulationRunReport report;
    std::optional<PreparedSimulation> prepared;
    QElapsedTimer elapsed;
    bool active{false};
    bool cancelRequested{false};
    QObject deferredContext;

    void finish(const SimulationRunStatus status, QString diagnostic = {})
    {
        if (!active) return;
        report.status = status;
        if (!diagnostic.isEmpty()) report.diagnostic = std::move(diagnostic);
        report.durationMs = elapsed.isValid() ? elapsed.elapsed() : 0;
        if (status == SimulationRunStatus::Succeeded) {
            report.stage = SimulationRunStage::Completed;
        }
        auto callback = std::move(completion);
        auto completed = std::move(report);
        active = false;
        prepared.reset();
        callback(std::move(completed));
    }

    void importTrace()
    {
        report.stage = SimulationRunStage::ImportTrace;
        TraceParseOptions options;
        options.projectTimeBase = prepared->stimulus.timeBase;
        options.identity = {
            prepared->manifest.workspaceId,
            "fixed-fixture-" + prepared->stimulus.identity,
            1,
        };
        auto parsed = parseVcdFile(filesystemPath(report.artifacts.vcdPath), options);
        if (!parsed.ok()) {
            finish(
                SimulationRunStatus::TraceImportFailed,
                QString::fromUtf8(parsed.errorSummary()));
            return;
        }
        report.trace = std::move(*parsed.index);
        finish(SimulationRunStatus::Succeeded);
    }

    void runModel()
    {
        report.stage = SimulationRunStage::RunModel;
        ProcessRunRequest process;
        process.program = report.artifacts.executablePath;
        process.arguments = {
            QStringLiteral("--vcd=%1").arg(report.artifacts.vcdPath),
        };
        process.workingDirectory = report.artifacts.runDirectory;
        process.environment = request.toolchain.environment;
        process.inheritCurrentProcessPath = request.toolchain.inheritCurrentProcessPath;
        process.timeoutMs = request.runTimeoutMs;
        process.maxOutputBytes = request.maxOutputBytes;
        const auto started = processRunner.start(
            std::move(process),
            [this](ProcessRunResult result) {
                report.simulationProcess = std::move(result);
                if (!report.simulationProcess->ok()) {
                    finish(
                        processFailureStatus(
                            *report.simulationProcess,
                            SimulationRunStatus::RunFailed),
                        report.simulationProcess->errorMessage);
                    return;
                }
                if (!QFileInfo(report.artifacts.vcdPath).isFile()) {
                    finish(
                        SimulationRunStatus::RunFailed,
                        QStringLiteral("Simulation completed without producing the VCD artifact."));
                    return;
                }
                importTrace();
            });
        if (!started) {
            finish(
                SimulationRunStatus::RunFailed,
                QStringLiteral("Simulation process could not be started."));
        }
    }

    void buildModel()
    {
        report.stage = SimulationRunStage::BuildModel;
        const auto& probe = *report.toolchain;
        QStringList arguments{
            QStringLiteral("--cc"),
            QStringLiteral("--exe"),
            QStringLiteral("--build"),
            QStringLiteral("--trace"),
            QStringLiteral("--top-module"),
            qString(prepared->manifest.target.module),
            QStringLiteral("--prefix"),
            QStringLiteral("Vwave_fixture"),
            QStringLiteral("--timescale"),
            QStringLiteral("1ps/1ps"),
            QStringLiteral("--Mdir"),
            report.artifacts.objectDirectory,
            QStringLiteral("-o"),
            simulatorFileName(),
        };
        for (const auto& include : prepared->includeDirectories) {
            arguments.append(QStringLiteral("-I%1").arg(include));
        }
        for (const auto& [name, value] : prepared->manifest.defines) {
            arguments.append(
                value.empty()
                    ? QStringLiteral("-D%1").arg(qString(name))
                    : QStringLiteral("-D%1=%2").arg(qString(name), qString(value)));
        }
        for (const auto& parameter : prepared->manifest.parameters) {
            if (parameter.semanticAvailable && !parameter.valueText.empty()) {
                arguments.append(
                    QStringLiteral("-G%1=%2")
                        .arg(qString(parameter.name), qString(parameter.valueText)));
            }
        }
        arguments.append(prepared->sourceFiles);
        arguments.append(report.artifacts.harnessPath);

        auto environment = request.toolchain.environment;
        environment.insert(
            QStringLiteral("CXX"), probe.cxxCompiler.process.resolvedProgram);
        ProcessRunRequest process;
        process.program = probe.verilator.process.resolvedProgram;
        process.arguments = std::move(arguments);
        process.workingDirectory = QFileInfo(request.workspaceRoot).absoluteFilePath();
        process.environment = std::move(environment);
        process.inheritCurrentProcessPath = request.toolchain.inheritCurrentProcessPath;
        process.timeoutMs = request.buildTimeoutMs;
        process.maxOutputBytes = request.maxOutputBytes;
        const auto started = processRunner.start(
            std::move(process),
            [this](ProcessRunResult result) {
                report.buildProcess = std::move(result);
                if (!report.buildProcess->ok()) {
                    finish(
                        processFailureStatus(
                            *report.buildProcess,
                            SimulationRunStatus::BuildFailed),
                        report.buildProcess->errorMessage);
                    return;
                }
                if (!QFileInfo(report.artifacts.executablePath).isFile()) {
                    finish(
                        SimulationRunStatus::BuildFailed,
                        QStringLiteral("Verilator completed without producing the simulator executable."));
                    return;
                }
                runModel();
            });
        if (!started) {
            finish(
                SimulationRunStatus::BuildFailed,
                QStringLiteral("Verilator build process could not be started."));
        }
    }

    void generateHarness()
    {
        report.stage = SimulationRunStage::GenerateHarness;
        QDir artifacts(request.artifactDirectory);
        if (!artifacts.mkpath(QStringLiteral("."))) {
            finish(
                SimulationRunStatus::HarnessGenerationFailed,
                QStringLiteral("Artifact directory could not be created."));
            return;
        }
        const auto runName = QStringLiteral("run-%1").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces));
        if (!artifacts.mkpath(runName)) {
            finish(
                SimulationRunStatus::HarnessGenerationFailed,
                QStringLiteral("Simulation run directory could not be created."));
            return;
        }
        report.artifacts.runDirectory = artifacts.absoluteFilePath(runName);
        QDir run(report.artifacts.runDirectory);
        report.artifacts.harnessPath = run.filePath(QStringLiteral("wave_fixture_main.cpp"));
        report.artifacts.objectDirectory = run.filePath(QStringLiteral("obj_dir"));
        report.artifacts.executablePath = QDir(report.artifacts.objectDirectory)
                                              .filePath(simulatorFileName());
        report.artifacts.vcdPath = run.filePath(QStringLiteral("wave_fixture.vcd"));
        if (!run.mkpath(QStringLiteral("obj_dir"))) {
            finish(
                SimulationRunStatus::HarnessGenerationFailed,
                QStringLiteral("Verilator object directory could not be created."));
            return;
        }
        QString error;
        const auto harness = makeHarness(*prepared, error);
        if (!harness || !writeDocument(report.artifacts.harnessPath, *harness, error)) {
            finish(SimulationRunStatus::HarnessGenerationFailed, error);
            return;
        }
        buildModel();
    }

    void acceptProbe(ToolchainProbeReport completed)
    {
        report.toolchain = std::move(completed);
        if (!report.toolchain->ready()) {
            const auto status = report.toolchain->status;
            if (status == ToolchainProbeStatus::Cancelled) {
                finish(SimulationRunStatus::Cancelled, QStringLiteral("Toolchain probe was cancelled."));
            } else if (status == ToolchainProbeStatus::TimedOut) {
                finish(SimulationRunStatus::TimedOut, QStringLiteral("Toolchain probe timed out."));
            } else {
                finish(
                    SimulationRunStatus::ToolchainUnavailable,
                    QStringLiteral("A compatible Verilator and C++ compiler are required."));
            }
            return;
        }
        generateHarness();
    }

    void begin()
    {
        if (!active) return;
        if (cancelRequested) {
            finish(SimulationRunStatus::Cancelled, QStringLiteral("Simulation was cancelled."));
            return;
        }
        report.stage = SimulationRunStage::ValidateInputs;
        SimulationRunStatus status = SimulationRunStatus::InvalidRequest;
        QString error;
        prepared = prepareSimulation(request, status, error);
        if (!prepared) {
            finish(status, error);
            return;
        }
        report.stage = SimulationRunStage::ProbeToolchain;
        const auto started = probeRunner.start(
            request.toolchain,
            [this](ToolchainProbeReport completed) {
                acceptProbe(std::move(completed));
            });
        if (!started) {
            finish(
                SimulationRunStatus::ToolchainUnavailable,
                QStringLiteral("Toolchain probe could not be started."));
        }
    }

    bool start(SimulationRunRequest nextRequest, Completion nextCompletion)
    {
        if (active || !nextCompletion || nextRequest.buildTimeoutMs <= 0
            || nextRequest.runTimeoutMs <= 0 || nextRequest.maxOutputBytes < 0) {
            return false;
        }
        request = std::move(nextRequest);
        completion = std::move(nextCompletion);
        report = {};
        prepared.reset();
        active = true;
        cancelRequested = false;
        elapsed.start();
        QTimer::singleShot(0, &deferredContext, [this] { begin(); });
        return true;
    }

    bool cancel()
    {
        if (!active) return false;
        cancelRequested = true;
        if (probeRunner.running()) return probeRunner.cancel();
        if (processRunner.running()) return processRunner.cancel();
        return true;
    }
};

VerilatorSimulationRunner::VerilatorSimulationRunner()
    : impl_(std::make_unique<Impl>())
{
}

VerilatorSimulationRunner::~VerilatorSimulationRunner()
{
    if (impl_->active) static_cast<void>(impl_->cancel());
}

bool VerilatorSimulationRunner::start(
    SimulationRunRequest request,
    Completion completion)
{
    return impl_->start(std::move(request), std::move(completion));
}

bool VerilatorSimulationRunner::cancel()
{
    return impl_->cancel();
}

bool VerilatorSimulationRunner::running() const noexcept
{
    return impl_->active;
}

QJsonObject simulationRunReportJson(const SimulationRunReport& report)
{
    QJsonObject trace;
    if (report.trace) {
        QJsonArray signalArray;
        for (const auto& signal : report.trace->traceSignals) {
            signalArray.append(QJsonObject{
                {QStringLiteral("id"), qString(signal.id)},
                {QStringLiteral("name"), qString(signal.fullName)},
                {QStringLiteral("width"), static_cast<qint64>(signal.width)},
                {QStringLiteral("transitionCount"),
                 static_cast<qint64>(signal.transitions.size())},
            });
        }
        trace = {
            {QStringLiteral("format"), QStringLiteral("vcd")},
            {QStringLiteral("startTick"), QString::number(report.trace->startTick)},
            {QStringLiteral("endTick"), QString::number(report.trace->endTick)},
            {QStringLiteral("signalCount"),
             static_cast<qint64>(report.trace->traceSignals.size())},
            {QStringLiteral("transitionCount"),
             static_cast<qint64>(report.trace->transitionCount)},
            {QStringLiteral("signals"), signalArray},
        };
    }
    QJsonObject result{
        {QStringLiteral("schema"), QString::fromLatin1(SimulationRunReportSchema)},
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("status"), text(toString(report.status))},
        {QStringLiteral("stage"), text(toString(report.stage))},
        {QStringLiteral("ok"), report.ok()},
        {QStringLiteral("durationMs"), report.durationMs},
        {QStringLiteral("diagnostic"), report.diagnostic},
        {QStringLiteral("artifacts"),
         QJsonObject{
             {QStringLiteral("runDirectory"), report.artifacts.runDirectory},
             {QStringLiteral("harness"), report.artifacts.harnessPath},
             {QStringLiteral("objectDirectory"), report.artifacts.objectDirectory},
             {QStringLiteral("executable"), report.artifacts.executablePath},
             {QStringLiteral("vcd"), report.artifacts.vcdPath},
         }},
        {QStringLiteral("trace"), trace},
    };
    if (report.toolchain) {
        result.insert(
            QStringLiteral("toolchain"),
            toolchainProbeReportJson(*report.toolchain));
    }
    if (report.buildProcess) {
        result.insert(QStringLiteral("build"), processJson(*report.buildProcess));
    }
    if (report.simulationProcess) {
        result.insert(
            QStringLiteral("simulation"),
            processJson(*report.simulationProcess));
    }
    return result;
}

std::string_view toString(const SimulationRunStage stage) noexcept
{
    switch (stage) {
    case SimulationRunStage::ValidateInputs: return "validate-inputs";
    case SimulationRunStage::ProbeToolchain: return "probe-toolchain";
    case SimulationRunStage::GenerateHarness: return "generate-harness";
    case SimulationRunStage::BuildModel: return "build-model";
    case SimulationRunStage::RunModel: return "run-model";
    case SimulationRunStage::ImportTrace: return "import-trace";
    case SimulationRunStage::Completed: return "completed";
    }
    return "unknown";
}

std::string_view toString(const SimulationRunStatus status) noexcept
{
    switch (status) {
    case SimulationRunStatus::Succeeded: return "succeeded";
    case SimulationRunStatus::InvalidRequest: return "invalid-request";
    case SimulationRunStatus::InvalidManifest: return "invalid-manifest";
    case SimulationRunStatus::InvalidStimulus: return "invalid-stimulus";
    case SimulationRunStatus::ContractMismatch: return "contract-mismatch";
    case SimulationRunStatus::UnsupportedFixture: return "unsupported-fixture";
    case SimulationRunStatus::ToolchainUnavailable: return "toolchain-unavailable";
    case SimulationRunStatus::HarnessGenerationFailed: return "harness-generation-failed";
    case SimulationRunStatus::BuildFailed: return "build-failed";
    case SimulationRunStatus::RunFailed: return "run-failed";
    case SimulationRunStatus::TraceImportFailed: return "trace-import-failed";
    case SimulationRunStatus::TimedOut: return "timed-out";
    case SimulationRunStatus::Cancelled: return "cancelled";
    }
    return "unknown";
}

} // namespace wave
