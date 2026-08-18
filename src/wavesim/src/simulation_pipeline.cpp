#include "wave/simulation_pipeline.h"

#include "wave/module_manifest.h"
#include "wave/project_io.h"
#include "wave/simulation_build_cache.h"
#include "wave/simulation_session.h"
#include "wave/stimulus_scenario.h"

#include <QDir>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>
#include <QTimer>
#include <QUuid>

#include <algorithm>
#include <atomic>
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
    Project project;
    QByteArray manifestDocument;
    std::vector<SimulationBuildSourceInput> buildSources;
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
    prepared.project = *restored.project;
    prepared.manifestDocument = manifestDocument;
    const QDir workspace(workspaceInfo.absoluteFilePath());
    for (const auto& source : prepared.manifest.sources) {
        const auto path = QFileInfo(workspace.filePath(qString(source.path)))
                              .absoluteFilePath();
        if (!QFileInfo::exists(path) || !QFileInfo(path).isFile()) {
            status = SimulationRunStatus::InvalidManifest;
            error = QStringLiteral("Manifest source does not exist: %1")
                        .arg(QDir::toNativeSeparators(path));
            return std::nullopt;
        }
        QByteArray sourceDocument;
        if (!readDocument(path, sourceDocument, error)) {
            status = SimulationRunStatus::InvalidManifest;
            return std::nullopt;
        }
        prepared.buildSources.push_back({
            qString(source.path),
            qString(source.role),
            std::move(sourceDocument),
        });
        if (source.role != "header") prepared.sourceFiles.append(path);
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
        error = QStringLiteral(
            "Wave Simulation requires a top-module name that maps directly to a C++ identifier.");
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
                "Wave Simulation supports fixed integral input/output ports up to 64 bits; unsupported port: %1")
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

std::optional<QByteArray> makeRuntimePlan(
    const PreparedSimulation& prepared,
    QString& error)
{
    QString output;
    QTextStream stream(&output);
    const auto inputCount = static_cast<std::size_t>(std::count_if(
        prepared.stimulus.ports.cbegin(),
        prepared.stimulus.ports.cend(),
        [](const StimulusScenarioPort& port) {
            return port.binding.direction == ModulePortDirection::Input;
        }));
    stream << "wave-runtime-plan-v1\n"
           << "duration " << prepared.stimulus.duration << "\n"
           << "picoseconds-per-tick "
           << prepared.stimulus.timeBase.picosecondsPerTick << "\n"
           << "inputs " << inputCount << "\n";
    for (const auto& port : prepared.stimulus.ports) {
        if (port.binding.direction != ModulePortDirection::Input) continue;
        const auto name = qString(port.binding.name);
        if (port.kind == LaneKind::Clock) {
            if (!port.clock) {
                error = QStringLiteral("Clock port %1 has no clock configuration.")
                            .arg(name);
                return std::nullopt;
            }
            const auto& clock = *port.clock;
            const auto highTicks = clock.period * clock.dutyCycle.numerator
                / clock.dutyCycle.denominator;
            stream << "input " << name << " clock " << clock.period << ' '
                   << clock.phase << ' ' << highTicks << ' '
                   << port.segments.size() << "\n";
            for (const auto& range : port.segments) {
                const auto mode = clockOverrideModeFromString(range.value);
                if (!mode || *mode == ClockOverrideMode::Disabled) {
                    error = QStringLiteral(
                        "Wave Simulation does not support unknown-valued clock override on %1.")
                                .arg(name);
                    return std::nullopt;
                }
                stream << "range " << range.start << ' ' << range.end << " 0\n";
            }
            continue;
        }
        if (port.segments.empty()) {
            error = QStringLiteral("Stimulus input %1 has no ranges.").arg(name);
            return std::nullopt;
        }
        stream << "input " << name << " segments " << port.segments.size()
               << "\n";
        for (const auto& range : port.segments) {
            const auto value = binaryValue(port, range.value);
            if (!value) {
                error = QStringLiteral(
                    "Stimulus value for %1 is not a known integral value.")
                                .arg(name);
                return std::nullopt;
            }
            stream << "range " << range.start << ' ' << range.end << ' '
                   << *value << "\n";
        }
    }
    return output.toUtf8();
}

std::optional<QByteArray> makeHarness(
    const PreparedSimulation& prepared,
    QString& error)
{
    QString output;
    QTextStream stream(&output);
    stream
        << "// Generated from Module Manifest " << qString(prepared.manifest.identity) << "\n"
        << "#include \"Vwave_fixture.h\"\n"
           "#include \"verilated.h\"\n"
           "#include \"verilated_vcd_c.h\"\n\n"
           "#include <cstdint>\n"
           "#include <fstream>\n"
           "#include <memory>\n"
           "#include <string>\n"
           "#include <utility>\n"
           "#include <vector>\n\n"
           "struct RuntimeRange {\n"
           "    std::uint64_t start{};\n"
           "    std::uint64_t end{};\n"
           "    std::uint64_t value{};\n"
           "};\n\n"
           "struct RuntimeInput {\n"
           "    std::string name;\n"
           "    bool clock{};\n"
           "    std::uint64_t period{};\n"
           "    std::uint64_t phase{};\n"
           "    std::uint64_t highTicks{};\n"
           "    std::vector<RuntimeRange> ranges;\n"
           "};\n\n"
           "struct RuntimePlan {\n"
           "    std::uint64_t duration{};\n"
           "    std::uint64_t picosecondsPerTick{};\n"
           "    std::vector<RuntimeInput> inputs;\n"
           "};\n\n"
           "static bool loadPlan(const std::string& path, RuntimePlan& plan)\n"
           "{\n"
           "    std::ifstream stream(path);\n"
           "    std::string token;\n"
           "    std::size_t inputCount = 0;\n"
           "    if (!(stream >> token) || token != \"wave-runtime-plan-v1\") return false;\n"
           "    if (!(stream >> token >> plan.duration) || token != \"duration\") return false;\n"
           "    if (!(stream >> token >> plan.picosecondsPerTick)\n"
           "        || token != \"picoseconds-per-tick\"\n"
           "        || plan.picosecondsPerTick == 0) return false;\n"
           "    if (!(stream >> token >> inputCount) || token != \"inputs\") return false;\n"
           "    plan.inputs.reserve(inputCount);\n"
           "    for (std::size_t index = 0; index < inputCount; ++index) {\n"
           "        RuntimeInput input;\n"
           "        std::string mode;\n"
           "        std::size_t rangeCount = 0;\n"
           "        if (!(stream >> token >> input.name >> mode) || token != \"input\") return false;\n"
           "        if (mode == \"clock\") {\n"
           "            input.clock = true;\n"
           "            if (!(stream >> input.period >> input.phase >> input.highTicks >> rangeCount)\n"
           "                || input.period == 0 || input.highTicks > input.period) return false;\n"
           "        } else if (mode == \"segments\") {\n"
           "            if (!(stream >> rangeCount) || rangeCount == 0) return false;\n"
           "        } else {\n"
           "            return false;\n"
           "        }\n"
           "        input.ranges.reserve(rangeCount);\n"
           "        for (std::size_t rangeIndex = 0; rangeIndex < rangeCount; ++rangeIndex) {\n"
           "            RuntimeRange range;\n"
           "            if (!(stream >> token >> range.start >> range.end >> range.value)\n"
           "                || token != \"range\" || range.end <= range.start) return false;\n"
           "            input.ranges.push_back(range);\n"
           "        }\n"
           "        plan.inputs.push_back(std::move(input));\n"
           "    }\n"
           "    return true;\n"
           "}\n\n"
           "static std::uint64_t valueAt(const RuntimeInput& input, const std::uint64_t tick)\n"
           "{\n"
           "    if (input.clock) {\n"
           "        std::uint64_t value = tick < input.phase\n"
           "            ? 0ULL\n"
           "            : (((tick - input.phase) % input.period) < input.highTicks ? 1ULL : 0ULL);\n"
           "        for (const auto& range : input.ranges) {\n"
           "            if (tick >= range.start && tick < range.end) value = range.value;\n"
           "        }\n"
           "        return value;\n"
           "    }\n"
           "    for (const auto& range : input.ranges) {\n"
           "        if (tick >= range.start && tick < range.end) return range.value;\n"
           "    }\n"
           "    return input.ranges.empty() ? 0ULL : input.ranges.back().value;\n"
           "}\n\n"
        << "int main(int argc, char** argv)\n{\n"
           "    std::string vcdPath;\n"
           "    std::string stimulusPath;\n"
           "    for (int index = 1; index < argc; ++index) {\n"
           "        const std::string argument(argv[index]);\n"
           "        if (argument.rfind(\"--vcd=\", 0) == 0) vcdPath = argument.substr(6);\n"
           "        if (argument.rfind(\"--stimulus=\", 0) == 0) stimulusPath = argument.substr(11);\n"
           "    }\n"
           "    RuntimePlan plan;\n"
           "    if (vcdPath.empty() || stimulusPath.empty() || !loadPlan(stimulusPath, plan)) return 2;\n\n";

    std::size_t inputIndex = 0;
    for (const auto& port : prepared.manifest.ports) {
        if (port.direction != ModulePortDirection::Input) continue;
        const auto name = qString(port.name);
        if (!cppIdentifier(name)) {
            error = QStringLiteral("Input port cannot be emitted in the runtime harness: %1")
                        .arg(name);
            return std::nullopt;
        }
        stream << "    if (plan.inputs.size() <= " << inputIndex
               << " || plan.inputs[" << inputIndex << "].name != \"" << name
               << "\") return 3;\n";
        ++inputIndex;
    }
    stream
        << "    if (plan.inputs.size() != " << inputIndex << ") return 3;\n\n"
           "    auto context = std::make_unique<VerilatedContext>();\n"
           "    context->commandArgs(argc, argv);\n"
           "    context->traceEverOn(true);\n"
           "    auto top = std::make_unique<Vwave_fixture>(context.get());\n"
           "    auto trace = std::make_unique<VerilatedVcdC>();\n"
           "    top->trace(trace.get(), 8);\n"
           "    trace->open(vcdPath.c_str());\n\n"
           "    for (std::uint64_t tick = 0; tick <= plan.duration; ++tick) {\n";
    inputIndex = 0;
    for (const auto& port : prepared.manifest.ports) {
        if (port.direction != ModulePortDirection::Input) continue;
        stream << "        top->" << qString(port.name) << " = valueAt(plan.inputs["
               << inputIndex << "], tick);\n";
        ++inputIndex;
    }
    stream
        << "        context->time(tick * plan.picosecondsPerTick);\n"
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

QString toolVersionText(const std::optional<ToolVersion>& version)
{
    if (!version || !version->valid()) return QStringLiteral("unknown");
    return QStringLiteral("%1.%2.%3")
        .arg(version->major)
        .arg(version->minor)
        .arg(version->patch);
}

QString cacheKey(const QString& fingerprint)
{
    return fingerprint.startsWith(QStringLiteral("sha256:"))
        ? fingerprint.mid(7)
        : fingerprint;
}

QString cacheMetadataPath(const QString& directory)
{
    return QDir(directory).filePath(QStringLiteral("build-cache.json"));
}

QString cacheExecutablePath(const QString& directory)
{
    return QDir(directory).filePath(
        QStringLiteral("obj_dir/%1").arg(simulatorFileName()));
}

QString fileSha256(const QString& path, QString& error)
{
    QByteArray document;
    if (!readDocument(path, document, error)) return {};
    return simulationSha256(document);
}

bool validCachedBuild(
    const QString& directory,
    const SimulationBuildFingerprint& expected,
    QString& diagnostic)
{
    const auto metadataPath = cacheMetadataPath(directory);
    if (!QFileInfo(metadataPath).isFile()) {
        diagnostic = QStringLiteral("No verified build cache entry exists.");
        return false;
    }
    QByteArray metadata;
    if (!readDocument(metadataPath, metadata, diagnostic))
        return false;
    const auto parsed = parseSimulationBuildCacheRecord(metadata);
    if (!parsed.ok()) {
        diagnostic = parsed.error;
        return false;
    }
    if (parsed.record->build.value != expected.value
        || parsed.record->build.evidence != expected.evidence) {
        diagnostic = QStringLiteral("Build cache fingerprint evidence does not match.");
        return false;
    }
    QString error;
    const auto executableDigest = fileSha256(cacheExecutablePath(directory), error);
    if (executableDigest.isEmpty()
        || executableDigest != parsed.record->executableSha256) {
        diagnostic = error.isEmpty()
            ? QStringLiteral("Cached simulator executable digest does not match.")
            : error;
        return false;
    }
    diagnostic.clear();
    return true;
}

QProcessEnvironment effectiveBuildEnvironment(
    const SimulationRunRequest& request,
    const ToolchainProbeReport& toolchain)
{
    auto environment = request.toolchain.environment;
    if (request.toolchain.inheritCurrentProcessPath
        && environment.value(QStringLiteral("PATH")).isEmpty()) {
        environment.insert(
            QStringLiteral("PATH"),
            QProcessEnvironment::systemEnvironment().value(QStringLiteral("PATH")));
    }
    environment.insert(
        QStringLiteral("CXX"),
        toolchain.cxxCompiler.process.resolvedProgram);
    return environment;
}

SimulationBuildFingerprint buildFingerprint(
    const PreparedSimulation& prepared,
    const QByteArray& harness,
    const SimulationRunRequest& request,
    const ToolchainProbeReport& toolchain)
{
    SimulationBuildFingerprintInput input;
    input.manifestDocument = prepared.manifestDocument;
    input.sources = prepared.buildSources;
    input.harnessDocument = harness;
    input.verilatorProgram = toolchain.verilator.process.resolvedProgram;
    input.verilatorVersion = toolVersionText(toolchain.verilator.version);
    input.cxxProgram = toolchain.cxxCompiler.process.resolvedProgram;
    input.cxxVersion = toolVersionText(toolchain.cxxCompiler.version);
    input.cxxFamily = toolchain.cxxCompiler.compilerFamily;
    input.verilatorArguments = request.toolchain.verilatorArguments;
    input.cxxArguments = request.toolchain.cxxArguments;
    input.environment = effectiveBuildEnvironment(request, toolchain);
    return computeSimulationBuildFingerprint(input);
}

QString generationClaimPath(const QString& resultProjectPath)
{
    return resultProjectPath + QStringLiteral(".active-run.json");
}

QString generationLockPath(const QString& resultProjectPath)
{
    return resultProjectPath + QStringLiteral(".active-run.lock");
}

QByteArray generationClaimDocument(
    const quint64 generation,
    const QString& token)
{
    return QJsonDocument(QJsonObject{
        {QStringLiteral("schema"),
         QStringLiteral("wave-workbench.simulation-generation/v1")},
        {QStringLiteral("generation"), QString::number(generation)},
        {QStringLiteral("token"), token},
    }).toJson(QJsonDocument::Compact);
}

bool claimResultGeneration(
    const SimulationRunRequest& request,
    const QString& token,
    bool& superseded,
    QString& error)
{
    superseded = false;
    if (request.resultProjectPath.trimmed().isEmpty()) return true;
    const QFileInfo outputInfo(request.resultProjectPath);
    if (!outputInfo.absoluteDir().mkpath(QStringLiteral("."))) {
        error = QStringLiteral("Result project directory could not be created.");
        return false;
    }
    QLockFile lock(generationLockPath(outputInfo.absoluteFilePath()));
    lock.setStaleLockTime(30'000);
    if (!lock.tryLock(1'000)) {
        error = QStringLiteral("Result generation lock could not be acquired: %1")
                    .arg(static_cast<int>(lock.error()));
        return false;
    }
    const auto claimPath = generationClaimPath(outputInfo.absoluteFilePath());
    if (QFileInfo(claimPath).isFile()) {
        QByteArray existingDocument;
        if (!readDocument(claimPath, existingDocument, error)) return false;
        QJsonParseError parseError;
        const auto existing = QJsonDocument::fromJson(
            existingDocument, &parseError);
        if (parseError.error != QJsonParseError::NoError
            || !existing.isObject()) {
            error = QStringLiteral("Existing result generation claim is invalid.");
            return false;
        }
        const auto object = existing.object();
        bool generationOk = false;
        const auto existingGeneration =
            object.value(QStringLiteral("generation"))
                .toString().toULongLong(&generationOk);
        if (object.value(QStringLiteral("schema")).toString()
                != QStringLiteral("wave-workbench.simulation-generation/v1")
            || !generationOk) {
            error = QStringLiteral("Existing result generation claim is invalid.");
            return false;
        }
        if (existingGeneration > request.generation) {
            superseded = true;
            error = QStringLiteral(
                "A newer simulation generation already owns the result project.");
            return false;
        }
    }
    return writeDocument(
        claimPath,
        generationClaimDocument(request.generation, token),
        error);
}

bool currentResultGeneration(
    const SimulationRunRequest& request,
    const QString& token,
    QString& error)
{
    QByteArray document;
    if (!readDocument(generationClaimPath(request.resultProjectPath), document, error))
        return false;
    QJsonParseError parseError;
    const auto parsed = QJsonDocument::fromJson(document, &parseError);
    if (parseError.error != QJsonParseError::NoError || !parsed.isObject()) {
        error = QStringLiteral("Result generation claim is invalid.");
        return false;
    }
    const auto object = parsed.object();
    return object.value(QStringLiteral("schema")).toString()
            == QStringLiteral("wave-workbench.simulation-generation/v1")
        && object.value(QStringLiteral("generation")).toString()
            == QString::number(request.generation)
        && object.value(QStringLiteral("token")).toString() == token;
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

QString portableDiagnosticPath(
    const QString& rawPath,
    const QString& workspaceRoot)
{
    const QString normalized = QDir::cleanPath(
        QDir::fromNativeSeparators(rawPath.trimmed()));
    if (normalized.isEmpty()) return {};
    if (QDir::isRelativePath(normalized)) {
        if (normalized == QStringLiteral("..")
            || normalized.startsWith(QStringLiteral("../"))) {
            return {};
        }
        return normalized;
    }
    const QString relative = QDir::cleanPath(
        QDir(QFileInfo(workspaceRoot).absoluteFilePath())
            .relativeFilePath(normalized));
    if (relative == QStringLiteral("..")
        || relative.startsWith(QStringLiteral("../"))) {
        return {};
    }
    return QDir::fromNativeSeparators(relative);
}

std::vector<SimulationSourceDiagnostic> processDiagnostics(
    const ProcessRunResult& process,
    const SimulationRunStage stage,
    const QString& workspaceRoot)
{
    std::vector<SimulationSourceDiagnostic> diagnostics;
    const QString output = QString::fromUtf8(process.standardError)
        + QLatin1Char('\n') + QString::fromUtf8(process.standardOutput);
    static const QRegularExpression verilator(
        QStringLiteral(
            "^%(Error|Warning)(?:-([A-Za-z0-9_]+))?:\\s+(.+):(\\d+):(\\d+):\\s*(.*)$"));
    static const QRegularExpression compiler(
        QStringLiteral(
            "^(.+):(\\d+):(\\d+):\\s*(fatal error|error|warning|note):\\s*(.*)$"),
        QRegularExpression::CaseInsensitiveOption);
    const QStringList lines = output.split(QLatin1Char('\n'));
    for (QString line : lines) {
        line.remove(QLatin1Char('\r'));
        SimulationSourceDiagnostic diagnostic;
        const auto verilatorMatch = verilator.match(line.trimmed());
        if (verilatorMatch.hasMatch()) {
            diagnostic.severity = verilatorMatch.captured(1).toLower();
            diagnostic.code = verilatorMatch.captured(2);
            diagnostic.sourceFile = portableDiagnosticPath(
                verilatorMatch.captured(3), workspaceRoot);
            diagnostic.line = verilatorMatch.captured(4).toInt();
            diagnostic.column = verilatorMatch.captured(5).toInt();
            diagnostic.message = verilatorMatch.captured(6).trimmed();
        } else {
            const auto compilerMatch = compiler.match(line.trimmed());
            if (!compilerMatch.hasMatch()) continue;
            diagnostic.sourceFile = portableDiagnosticPath(
                compilerMatch.captured(1), workspaceRoot);
            diagnostic.line = compilerMatch.captured(2).toInt();
            diagnostic.column = compilerMatch.captured(3).toInt();
            diagnostic.severity = compilerMatch.captured(4).toLower();
            if (diagnostic.severity == QStringLiteral("fatal error"))
                diagnostic.severity = QStringLiteral("error");
            diagnostic.message = compilerMatch.captured(5).trimmed();
        }
        if (diagnostic.sourceFile.isEmpty()
            || diagnostic.line <= 0 || diagnostic.column <= 0
            || diagnostic.message.isEmpty()) {
            continue;
        }
        diagnostic.stage = text(toString(stage));
        diagnostics.push_back(std::move(diagnostic));
    }
    return diagnostics;
}

void appendProcessDiagnostics(
    SimulationRunReport& report,
    const ProcessRunResult& process,
    const PreparedSimulation& prepared,
    const SimulationRunRequest& request,
    const SimulationRunStage stage)
{
    auto diagnostics = processDiagnostics(
        process, stage, request.workspaceRoot);
    if (diagnostics.empty()) {
        SimulationSourceDiagnostic fallback;
        fallback.sourceFile = qString(prepared.manifest.target.sourceFile);
        fallback.line = prepared.manifest.target.sourceLine;
        fallback.column = 1;
        fallback.severity = QStringLiteral("error");
        fallback.stage = text(toString(stage));
        fallback.message = process.errorMessage.trimmed();
        if (fallback.message.isEmpty()) {
            fallback.message = QString::fromUtf8(process.standardError)
                                   .trimmed().section(QLatin1Char('\n'), 0, 0);
        }
        if (fallback.message.isEmpty())
            fallback.message = QStringLiteral("Simulation process failed.");
        diagnostics.push_back(std::move(fallback));
    }
    report.diagnostics.insert(
        report.diagnostics.end(),
        std::make_move_iterator(diagnostics.begin()),
        std::make_move_iterator(diagnostics.end()));
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

quint64 nextSimulationGeneration() noexcept
{
    static std::atomic<quint64> next{
        static_cast<quint64>(QDateTime::currentMSecsSinceEpoch()) * 1'000ULL};
    return next.fetch_add(1, std::memory_order_relaxed);
}

struct VerilatorSimulationRunner::Impl {
    ToolchainProbeRunner probeRunner;
    ProcessRunner processRunner;
    SimulationRunRequest request;
    Completion completion;
    StageChanged stageChanged;
    SimulationRunReport report;
    std::optional<PreparedSimulation> prepared;
    QByteArray harnessDocument;
    SimulationBuildFingerprint fingerprint;
    QString buildStagingDirectory;
    QString resultGenerationToken;
    QElapsedTimer elapsed;
    bool active{false};
    bool cancelRequested{false};
    QObject deferredContext;

    void enterStage(const SimulationRunStage stage)
    {
        report.stage = stage;
        if (stageChanged) stageChanged(report.generation, stage);
    }

    void setCachedArtifactPaths(const QString& directory)
    {
        report.artifacts.harnessPath =
            QDir(directory).filePath(QStringLiteral("wave_fixture_main.cpp"));
        report.artifacts.objectDirectory =
            QDir(directory).filePath(QStringLiteral("obj_dir"));
        report.artifacts.executablePath = cacheExecutablePath(directory);
    }

    void cleanupBuildStaging()
    {
        if (buildStagingDirectory.isEmpty()) return;
        QDir directory(buildStagingDirectory);
        if (directory.exists()) directory.removeRecursively();
        buildStagingDirectory.clear();
    }

    void materializeProject()
    {
        if (request.resultProjectPath.trimmed().isEmpty()) {
            finish(SimulationRunStatus::Succeeded);
            return;
        }

        enterStage(SimulationRunStage::MaterializeProject);
        const QFileInfo outputInfo(request.resultProjectPath);
        QDir outputDirectory = outputInfo.absoluteDir();
        if (!outputDirectory.mkpath(QStringLiteral("."))) {
            finish(
                SimulationRunStatus::ResultProjectFailed,
                QStringLiteral("Result project directory could not be created."));
            return;
        }

        ImportedTrace traceReference;
        traceReference.id = makeStableId("zs-simulation-trace");
        const auto relativeVcd = outputDirectory.relativeFilePath(
            report.artifacts.vcdPath);
        traceReference.path = QDir::fromNativeSeparators(relativeVcd)
                                  .toUtf8()
                                  .toStdString();
        traceReference.format = "vcd";
        traceReference.offset = 0;
        if (!prepared->project.scenarios.empty() && report.trace) {
            traceReference.signalMapping = suggestSignalMapping(
                prepared->project.scenarios.front(), *report.trace);
        }
        traceReference.extensions.emplace(
            "waveSimulation.moduleManifestIdentity",
            '"' + prepared->manifest.identity + '"');
        traceReference.extensions.emplace(
            "waveSimulation.stimulusIdentity",
            '"' + prepared->stimulus.identity + '"');
        traceReference.extensions.emplace(
            "waveSimulation.resultState", "\"current\"");
        traceReference.extensions.emplace(
            "waveSimulation.buildFingerprint",
            QJsonDocument(QJsonObject{
                {QStringLiteral("value"), report.buildCache.fingerprint},
            }).toJson(QJsonDocument::Compact).toStdString());
        traceReference.extensions.emplace(
            "waveSimulation.generation",
            QString::number(report.generation).toStdString());
        traceReference.extensions.emplace(
            "waveSimulation.buildCacheHit",
            report.buildCache.hit ? "true" : "false");
        prepared->project.importedTraces.clear();
        prepared->project.importedTraces.push_back(
            std::move(traceReference));
        attachSimulationSession(prepared->project, request);

        const auto outputPath = outputInfo.absoluteFilePath();
        QLockFile lock(generationLockPath(outputPath));
        lock.setStaleLockTime(30'000);
        if (!lock.tryLock(1'000)) {
            finish(
                SimulationRunStatus::ResultProjectFailed,
                QStringLiteral("Result generation lock could not be acquired."));
            return;
        }
        QString error;
        if (!currentResultGeneration(request, resultGenerationToken, error)) {
            lock.unlock();
            finish(
                SimulationRunStatus::Superseded,
                error.isEmpty()
                    ? QStringLiteral(
                          "A newer simulation generation owns the result project.")
                    : error);
            return;
        }
        if (!writeDocument(
                outputPath,
                serializeProject(prepared->project),
                error)) {
            lock.unlock();
            finish(SimulationRunStatus::ResultProjectFailed, error);
            return;
        }
        lock.unlock();
        report.artifacts.resultProjectPath = outputPath;
        finish(SimulationRunStatus::Succeeded);
    }

    void finish(const SimulationRunStatus status, QString diagnostic = {})
    {
        if (!active) return;
        report.status = status;
        if (!diagnostic.isEmpty()) report.diagnostic = std::move(diagnostic);
        report.durationMs = elapsed.isValid() ? elapsed.elapsed() : 0;
        if (status == SimulationRunStatus::Succeeded) {
            enterStage(SimulationRunStage::Completed);
        }
        auto callback = std::move(completion);
        auto completed = std::move(report);
        cleanupBuildStaging();
        active = false;
        prepared.reset();
        harnessDocument.clear();
        fingerprint = {};
        stageChanged = {};
        callback(std::move(completed));
    }

    void importTrace()
    {
        enterStage(SimulationRunStage::ImportTrace);
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
        materializeProject();
    }

    void runModel()
    {
        enterStage(SimulationRunStage::RunModel);
        ProcessRunRequest process;
        process.program = report.artifacts.executablePath;
        process.arguments = {
            QStringLiteral("--vcd=%1").arg(report.artifacts.vcdPath),
            QStringLiteral("--stimulus=%1")
                .arg(report.artifacts.runtimeStimulusPath),
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
                    appendProcessDiagnostics(
                        report,
                        *report.simulationProcess,
                        *prepared,
                        request,
                        SimulationRunStage::RunModel);
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

    bool publishBuildCache(QString& error)
    {
        const auto executableDigest =
            fileSha256(report.artifacts.executablePath, error);
        if (executableDigest.isEmpty()) return false;
        const SimulationBuildCacheRecord record{fingerprint, executableDigest};
        if (!writeDocument(
                cacheMetadataPath(buildStagingDirectory),
                serializeSimulationBuildCacheRecord(record),
                error)) {
            return false;
        }

        const auto finalDirectory = report.buildCache.directory;
        QString existingDiagnostic;
        if (validCachedBuild(finalDirectory, fingerprint, existingDiagnostic)) {
            cleanupBuildStaging();
            setCachedArtifactPaths(finalDirectory);
            report.buildCache.diagnostic = QStringLiteral(
                "A concurrently published verified model was reused.");
            return true;
        }
        if (QDir(finalDirectory).exists()
            && !QDir(finalDirectory).removeRecursively()) {
            error = QStringLiteral("Invalid build cache entry could not be replaced.");
            return false;
        }
        QDir root(request.buildCacheDirectory);
        const auto stagingName = QFileInfo(buildStagingDirectory).fileName();
        const auto finalName = QFileInfo(finalDirectory).fileName();
        if (!root.rename(stagingName, finalName)) {
            if (validCachedBuild(finalDirectory, fingerprint, existingDiagnostic)) {
                cleanupBuildStaging();
                setCachedArtifactPaths(finalDirectory);
                report.buildCache.diagnostic = QStringLiteral(
                    "A concurrently published verified model was reused.");
                return true;
            }
            error = QStringLiteral("Completed simulator model could not be published to cache.");
            return false;
        }
        buildStagingDirectory.clear();
        QString verificationError;
        if (!validCachedBuild(finalDirectory, fingerprint, verificationError)) {
            error = QStringLiteral("Published build cache entry failed verification: %1")
                        .arg(verificationError);
            return false;
        }
        report.buildCache.published = true;
        report.buildCache.diagnostic = QStringLiteral(
            "Built model was verified and published to cache.");
        setCachedArtifactPaths(finalDirectory);
        return true;
    }

    void buildModel()
    {
        enterStage(SimulationRunStage::BuildModel);
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

        ProcessRunRequest process;
        process.program = probe.verilator.process.resolvedProgram;
        process.arguments = std::move(arguments);
        process.workingDirectory = QFileInfo(request.workspaceRoot).absoluteFilePath();
        process.environment = effectiveBuildEnvironment(request, probe);
        process.inheritCurrentProcessPath = request.toolchain.inheritCurrentProcessPath;
        process.timeoutMs = request.buildTimeoutMs;
        process.maxOutputBytes = request.maxOutputBytes;
        const auto started = processRunner.start(
            std::move(process),
            [this](ProcessRunResult result) {
                report.buildProcess = std::move(result);
                if (!report.buildProcess->ok()) {
                    appendProcessDiagnostics(
                        report,
                        *report.buildProcess,
                        *prepared,
                        request,
                        SimulationRunStage::BuildModel);
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
                QString error;
                if (!publishBuildCache(error)) {
                    finish(SimulationRunStatus::BuildFailed, error);
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

    void resolveBuildCache()
    {
        enterStage(SimulationRunStage::ResolveBuildCache);
        if (!QDir().mkpath(request.buildCacheDirectory)) {
            finish(
                SimulationRunStatus::HarnessGenerationFailed,
                QStringLiteral("Build cache directory could not be created."));
            return;
        }
        fingerprint = buildFingerprint(
            *prepared, harnessDocument, request, *report.toolchain);
        if (!fingerprint.valid()) {
            finish(
                SimulationRunStatus::HarnessGenerationFailed,
                QStringLiteral("Simulation build fingerprint could not be computed."));
            return;
        }
        report.buildCache.fingerprint = fingerprint.value;
        report.buildCache.directory = QDir(request.buildCacheDirectory)
                                          .filePath(cacheKey(fingerprint.value));
        QString cacheDiagnostic;
        if (validCachedBuild(
                report.buildCache.directory, fingerprint, cacheDiagnostic)) {
            report.buildCache.hit = true;
            report.buildCache.diagnostic = QStringLiteral("Verified cached model reused.");
            setCachedArtifactPaths(report.buildCache.directory);
            runModel();
            return;
        }
        report.buildCache.diagnostic = cacheDiagnostic;
        const auto stagingName = QStringLiteral("staging-%1-%2")
            .arg(
                cacheKey(fingerprint.value).left(16),
                QUuid::createUuid().toString(QUuid::WithoutBraces));
        buildStagingDirectory = QDir(request.buildCacheDirectory)
                                    .filePath(stagingName);
        if (!QDir().mkpath(
                QDir(buildStagingDirectory).filePath(QStringLiteral("obj_dir")))) {
            finish(
                SimulationRunStatus::HarnessGenerationFailed,
                QStringLiteral("Build cache staging directory could not be created."));
            return;
        }
        setCachedArtifactPaths(buildStagingDirectory);
        QString error;
        if (!writeDocument(
                report.artifacts.harnessPath, harnessDocument, error)) {
            finish(SimulationRunStatus::HarnessGenerationFailed, error);
            return;
        }
        buildModel();
    }

    void generateHarness()
    {
        enterStage(SimulationRunStage::GenerateHarness);
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
        report.artifacts.runtimeStimulusPath =
            run.filePath(QStringLiteral("runtime-stimulus.txt"));
        report.artifacts.vcdPath = run.filePath(QStringLiteral("wave_fixture.vcd"));
        QString error;
        const auto runtimePlan = makeRuntimePlan(*prepared, error);
        if (!runtimePlan
            || !writeDocument(
                report.artifacts.runtimeStimulusPath, *runtimePlan, error)) {
            finish(SimulationRunStatus::HarnessGenerationFailed, error);
            return;
        }
        const auto harness = makeHarness(*prepared, error);
        if (!harness) {
            finish(SimulationRunStatus::HarnessGenerationFailed, error);
            return;
        }
        harnessDocument = *harness;
        resolveBuildCache();
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
        enterStage(SimulationRunStage::ValidateInputs);
        SimulationRunStatus status = SimulationRunStatus::InvalidRequest;
        QString error;
        prepared = prepareSimulation(request, status, error);
        if (!prepared) {
            finish(status, error);
            return;
        }
        bool superseded = false;
        if (!claimResultGeneration(
                request, resultGenerationToken, superseded, error)) {
            finish(
                superseded
                    ? SimulationRunStatus::Superseded
                    : SimulationRunStatus::ResultProjectFailed,
                error);
            return;
        }
        enterStage(SimulationRunStage::ProbeToolchain);
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

    bool start(
        SimulationRunRequest nextRequest,
        Completion nextCompletion,
        StageChanged nextStageChanged)
    {
        if (active || !nextCompletion || nextRequest.buildTimeoutMs <= 0
            || nextRequest.runTimeoutMs <= 0 || nextRequest.maxOutputBytes < 0) {
            return false;
        }
        request = std::move(nextRequest);
        if (!request.artifactDirectory.trimmed().isEmpty()) {
            request.artifactDirectory =
                QDir(request.artifactDirectory).absolutePath();
        }
        if (request.buildCacheDirectory.trimmed().isEmpty()) {
            if (!request.artifactDirectory.isEmpty()) {
                request.buildCacheDirectory = QDir(request.artifactDirectory)
                                                  .filePath(QStringLiteral("build-cache"));
            }
        } else {
            request.buildCacheDirectory =
                QDir(request.buildCacheDirectory).absolutePath();
        }
        if (!request.scenarioDirectory.trimmed().isEmpty()) {
            request.scenarioDirectory =
                QDir(request.scenarioDirectory).absolutePath();
        }
        if (!request.resultProjectPath.trimmed().isEmpty()) {
            request.resultProjectPath =
                QFileInfo(request.resultProjectPath).absoluteFilePath();
        }
        if (request.generation == 0) {
            request.generation = nextSimulationGeneration();
        }
        completion = std::move(nextCompletion);
        stageChanged = std::move(nextStageChanged);
        report = {};
        report.generation = request.generation;
        prepared.reset();
        harnessDocument.clear();
        fingerprint = {};
        cleanupBuildStaging();
        resultGenerationToken =
            QUuid::createUuid().toString(QUuid::WithoutBraces);
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
    Completion completion,
    StageChanged stageChanged)
{
    return impl_->start(
        std::move(request),
        std::move(completion),
        std::move(stageChanged));
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
    QJsonArray diagnostics;
    for (const auto& diagnostic : report.diagnostics) {
        diagnostics.append(QJsonObject{
            {QStringLiteral("sourceFile"), diagnostic.sourceFile},
            {QStringLiteral("line"), diagnostic.line},
            {QStringLiteral("column"), diagnostic.column},
            {QStringLiteral("severity"), diagnostic.severity},
            {QStringLiteral("stage"), diagnostic.stage},
            {QStringLiteral("message"), diagnostic.message},
            {QStringLiteral("code"), diagnostic.code},
        });
    }
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
        {QStringLiteral("generation"), QString::number(report.generation)},
        {QStringLiteral("status"), text(toString(report.status))},
        {QStringLiteral("stage"), text(toString(report.stage))},
        {QStringLiteral("ok"), report.ok()},
        {QStringLiteral("durationMs"), report.durationMs},
        {QStringLiteral("diagnostic"), report.diagnostic},
        {QStringLiteral("diagnostics"), diagnostics},
        {QStringLiteral("artifacts"),
         QJsonObject{
             {QStringLiteral("runDirectory"), report.artifacts.runDirectory},
             {QStringLiteral("harness"), report.artifacts.harnessPath},
             {QStringLiteral("runtimeStimulus"),
              report.artifacts.runtimeStimulusPath},
             {QStringLiteral("objectDirectory"), report.artifacts.objectDirectory},
             {QStringLiteral("executable"), report.artifacts.executablePath},
             {QStringLiteral("vcd"), report.artifacts.vcdPath},
             {QStringLiteral("resultProject"),
              report.artifacts.resultProjectPath},
         }},
        {QStringLiteral("buildCache"),
         QJsonObject{
             {QStringLiteral("fingerprint"), report.buildCache.fingerprint},
             {QStringLiteral("directory"), report.buildCache.directory},
             {QStringLiteral("hit"), report.buildCache.hit},
             {QStringLiteral("published"), report.buildCache.published},
             {QStringLiteral("diagnostic"), report.buildCache.diagnostic},
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
    case SimulationRunStage::ResolveBuildCache: return "resolve-build-cache";
    case SimulationRunStage::BuildModel: return "build-model";
    case SimulationRunStage::RunModel: return "run-model";
    case SimulationRunStage::ImportTrace: return "import-trace";
    case SimulationRunStage::MaterializeProject: return "materialize-project";
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
    case SimulationRunStatus::ResultProjectFailed: return "result-project-failed";
    case SimulationRunStatus::TimedOut: return "timed-out";
    case SimulationRunStatus::Cancelled: return "cancelled";
    case SimulationRunStatus::Superseded: return "superseded";
    }
    return "unknown";
}

} // namespace wave
