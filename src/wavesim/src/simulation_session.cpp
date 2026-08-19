#include "wave/simulation_session.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>

namespace wave {
namespace {

bool hasOnlyKeys(
    const QJsonObject& object,
    const QSet<QString>& allowed,
    QString& error,
    const QString& context)
{
    for (auto iterator = object.constBegin(); iterator != object.constEnd(); ++iterator) {
        if (!allowed.contains(iterator.key())) {
            error = QStringLiteral("Unknown %1 property: %2")
                        .arg(context, iterator.key());
            return false;
        }
    }
    return true;
}

std::optional<QString> requiredString(
    const QJsonObject& object,
    const QString& key,
    QString& error)
{
    const auto value = object.value(key);
    if (!value.isString() || value.toString().trimmed().isEmpty()) {
        error = QStringLiteral("Simulation session property '%1' must be a non-empty string.")
                    .arg(key);
        return std::nullopt;
    }
    return value.toString();
}

std::optional<QStringList> stringArray(
    const QJsonObject& object,
    const QString& key,
    QString& error)
{
    const auto value = object.value(key);
    if (!value.isArray()) {
        error = QStringLiteral("Simulation session property '%1' must be an array.")
                    .arg(key);
        return std::nullopt;
    }
    QStringList result;
    for (const auto& entry : value.toArray()) {
        if (!entry.isString()) {
            error = QStringLiteral("Simulation session property '%1' must contain only strings.")
                        .arg(key);
            return std::nullopt;
        }
        result.push_back(entry.toString());
    }
    return result;
}

std::optional<int> integer(
    const QJsonObject& object,
    const QString& key,
    const int minimum,
    QString& error)
{
    const auto value = object.value(key);
    if (!value.isDouble()) {
        error = QStringLiteral("Simulation session property '%1' must be an integer.")
                    .arg(key);
        return std::nullopt;
    }
    const auto number = value.toDouble();
    if (!std::isfinite(number)
        || std::floor(number) != number
        || number < static_cast<double>(minimum)
        || number > static_cast<double>(std::numeric_limits<int>::max())) {
        error = QStringLiteral("Simulation session property '%1' is out of range.")
                    .arg(key);
        return std::nullopt;
    }
    return static_cast<int>(number);
}

QJsonArray strings(const QStringList& values)
{
    QJsonArray result;
    for (const auto& value : values) result.append(value);
    return result;
}

} // namespace

void attachSimulationSession(
    Project& project,
    const SimulationRunRequest& request)
{
    const QJsonObject toolchain{
        {QStringLiteral("verilatorProgram"), request.toolchain.verilatorProgram},
        {QStringLiteral("verilatorArguments"), strings(request.toolchain.verilatorArguments)},
        {QStringLiteral("cxxProgram"), request.toolchain.cxxProgram},
        {QStringLiteral("cxxArguments"), strings(request.toolchain.cxxArguments)},
        {QStringLiteral("inheritCurrentProcessPath"), request.toolchain.inheritCurrentProcessPath},
        {QStringLiteral("probeTimeoutMs"), request.toolchain.timeoutMs},
    };
    const QJsonObject document{
        {QStringLiteral("schema"), QString::fromLatin1(SimulationSessionSchema)},
        {QStringLiteral("manifestPath"), request.manifestPath},
        {QStringLiteral("stimulusPath"), request.stimulusPath},
        {QStringLiteral("workspaceRoot"), request.workspaceRoot},
        {QStringLiteral("artifactDirectory"), request.artifactDirectory},
        {QStringLiteral("buildCacheDirectory"), request.buildCacheDirectory},
        {QStringLiteral("scenarioDirectory"), request.scenarioDirectory},
        {QStringLiteral("resultProjectPath"), request.resultProjectPath},
        {QStringLiteral("stubbedModules"), strings(request.stubbedModules)},
        {QStringLiteral("toolchain"), toolchain},
        {QStringLiteral("buildTimeoutMs"), request.buildTimeoutMs},
        {QStringLiteral("runTimeoutMs"), request.runTimeoutMs},
        {QStringLiteral("maxOutputBytes"), request.maxOutputBytes},
    };
    project.extensions[SimulationSessionExtension] =
        QJsonDocument(document).toJson(QJsonDocument::Compact).toStdString();
}

SimulationSessionParseResult simulationSessionFromProject(const Project& project)
{
    SimulationSessionParseResult result;
    const auto iterator = project.extensions.find(SimulationSessionExtension);
    if (iterator == project.extensions.end()) return result;
    result.found = true;

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(
        QByteArray::fromStdString(iterator->second), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        result.error = QStringLiteral("Simulation session extension is not a JSON object: %1")
                           .arg(parseError.errorString());
        return result;
    }
    const auto object = document.object();
    if (!hasOnlyKeys(
            object,
            {
                QStringLiteral("schema"),
                QStringLiteral("manifestPath"),
                QStringLiteral("stimulusPath"),
                QStringLiteral("workspaceRoot"),
                QStringLiteral("artifactDirectory"),
                QStringLiteral("buildCacheDirectory"),
                QStringLiteral("scenarioDirectory"),
                QStringLiteral("resultProjectPath"),
                QStringLiteral("stubbedModules"),
                QStringLiteral("toolchain"),
                QStringLiteral("buildTimeoutMs"),
                QStringLiteral("runTimeoutMs"),
                QStringLiteral("maxOutputBytes"),
            },
            result.error,
            QStringLiteral("simulation session"))) {
        return result;
    }
    const QString schema = object.value(QStringLiteral("schema")).toString();
    const bool currentSchema =
        schema == QString::fromLatin1(SimulationSessionSchema);
    if (!currentSchema
        && schema != QString::fromLatin1(LegacySimulationSessionSchema)) {
        result.error = QStringLiteral("Unsupported simulation session schema.");
        return result;
    }

    SimulationRunRequest request;
    const auto manifestPath = requiredString(
        object, QStringLiteral("manifestPath"), result.error);
    const auto stimulusPath = requiredString(
        object, QStringLiteral("stimulusPath"), result.error);
    const auto workspaceRoot = requiredString(
        object, QStringLiteral("workspaceRoot"), result.error);
    const auto artifactDirectory = requiredString(
        object, QStringLiteral("artifactDirectory"), result.error);
    const auto resultProjectPath = requiredString(
        object, QStringLiteral("resultProjectPath"), result.error);
    if (!manifestPath || !stimulusPath || !workspaceRoot
        || !artifactDirectory || !resultProjectPath) {
        return result;
    }
    request.manifestPath = *manifestPath;
    request.stimulusPath = *stimulusPath;
    request.workspaceRoot = *workspaceRoot;
    request.artifactDirectory = *artifactDirectory;
    const auto buildCacheValue = object.value(QStringLiteral("buildCacheDirectory"));
    if (!buildCacheValue.isUndefined() && !buildCacheValue.isString()) {
        result.error = QStringLiteral(
            "Simulation session buildCacheDirectory must be a string.");
        return result;
    }
    request.buildCacheDirectory = buildCacheValue.toString().trimmed();
    if (request.buildCacheDirectory.isEmpty()) {
        request.buildCacheDirectory = QDir(request.artifactDirectory)
                                          .filePath(QStringLiteral("build-cache"));
    }
    const auto scenarioDirectoryValue = object.value(
        QStringLiteral("scenarioDirectory"));
    if (!scenarioDirectoryValue.isUndefined()
        && !scenarioDirectoryValue.isString()) {
        result.error = QStringLiteral(
            "Simulation session scenarioDirectory must be a string.");
        return result;
    }
    request.scenarioDirectory = scenarioDirectoryValue.toString().trimmed();
    request.resultProjectPath = *resultProjectPath;
    if (currentSchema) {
        const auto stubbedModules = stringArray(
            object, QStringLiteral("stubbedModules"), result.error);
        if (!stubbedModules) return result;
        QSet<QString> uniqueModules;
        for (const QString& module : *stubbedModules) {
            if (module.trimmed().isEmpty() || uniqueModules.contains(module)) {
                result.error = QStringLiteral(
                    "Simulation session stubbedModules must contain unique non-empty names.");
                return result;
            }
            uniqueModules.insert(module);
        }
        request.stubbedModules = *stubbedModules;
    } else if (object.contains(QStringLiteral("stubbedModules"))) {
        result.error = QStringLiteral(
            "Legacy simulation sessions cannot contain stub selections.");
        return result;
    }

    const auto toolchainValue = object.value(QStringLiteral("toolchain"));
    if (!toolchainValue.isObject()) {
        result.error = QStringLiteral("Simulation session toolchain must be an object.");
        return result;
    }
    const auto toolchain = toolchainValue.toObject();
    if (!hasOnlyKeys(
            toolchain,
            {
                QStringLiteral("verilatorProgram"),
                QStringLiteral("verilatorArguments"),
                QStringLiteral("cxxProgram"),
                QStringLiteral("cxxArguments"),
                QStringLiteral("inheritCurrentProcessPath"),
                QStringLiteral("probeTimeoutMs"),
            },
            result.error,
            QStringLiteral("toolchain"))) {
        return result;
    }
    const auto verilatorArguments = stringArray(
        toolchain, QStringLiteral("verilatorArguments"), result.error);
    const auto cxxArguments = stringArray(
        toolchain, QStringLiteral("cxxArguments"), result.error);
    const auto probeTimeout = integer(
        toolchain, QStringLiteral("probeTimeoutMs"), 1, result.error);
    if (!verilatorArguments || !cxxArguments || !probeTimeout) return result;
    if (!toolchain.value(QStringLiteral("verilatorProgram")).isString()
        || !toolchain.value(QStringLiteral("cxxProgram")).isString()
        || !toolchain.value(QStringLiteral("inheritCurrentProcessPath")).isBool()) {
        result.error = QStringLiteral("Simulation session toolchain properties have invalid types.");
        return result;
    }
    request.toolchain.verilatorProgram =
        toolchain.value(QStringLiteral("verilatorProgram")).toString();
    request.toolchain.verilatorArguments = *verilatorArguments;
    request.toolchain.cxxProgram =
        toolchain.value(QStringLiteral("cxxProgram")).toString();
    request.toolchain.cxxArguments = *cxxArguments;
    request.toolchain.inheritCurrentProcessPath =
        toolchain.value(QStringLiteral("inheritCurrentProcessPath")).toBool();
    request.toolchain.timeoutMs = *probeTimeout;

    const auto buildTimeout = integer(
        object, QStringLiteral("buildTimeoutMs"), 1, result.error);
    const auto runTimeout = integer(
        object, QStringLiteral("runTimeoutMs"), 1, result.error);
    const auto maxOutput = integer(
        object, QStringLiteral("maxOutputBytes"), 0, result.error);
    if (!buildTimeout || !runTimeout || !maxOutput) return result;
    request.buildTimeoutMs = *buildTimeout;
    request.runTimeoutMs = *runTimeout;
    request.maxOutputBytes = *maxOutput;
    result.request = std::move(request);
    return result;
}

void SimulationSessionStateMachine::configure(
    const bool runnable,
    const bool hasCurrentResult) noexcept
{
    runnable_ = runnable;
    staleDuringRun_ = false;
    state_ = hasCurrentResult
        ? SimulationSessionState::Current
        : SimulationSessionState::Ready;
    runOriginState_ = state_;
}

void SimulationSessionStateMachine::markStimulusEdited() noexcept
{
    if (state_ == SimulationSessionState::Compiling
        || state_ == SimulationSessionState::Running) {
        staleDuringRun_ = true;
        return;
    }
    if (state_ != SimulationSessionState::Ready) {
        state_ = SimulationSessionState::Stale;
    }
}

void SimulationSessionStateMachine::markCurrent() noexcept
{
    staleDuringRun_ = false;
    state_ = SimulationSessionState::Current;
}

void SimulationSessionStateMachine::markFailed() noexcept
{
    staleDuringRun_ = false;
    state_ = SimulationSessionState::Failed;
}

bool SimulationSessionStateMachine::beginRun() noexcept
{
    const auto available = actions();
    if (!available.runEnabled && !available.rerunEnabled) return false;
    runOriginState_ = state_;
    staleDuringRun_ = false;
    state_ = SimulationSessionState::Compiling;
    return true;
}

void SimulationSessionStateMachine::observeStage(
    const SimulationRunStage stage) noexcept
{
    if (state_ != SimulationSessionState::Compiling
        && state_ != SimulationSessionState::Running) {
        return;
    }
    if (stage == SimulationRunStage::RunModel
        || stage == SimulationRunStage::ImportTrace
        || stage == SimulationRunStage::MaterializeProject
        || stage == SimulationRunStage::Completed) {
        state_ = SimulationSessionState::Running;
    }
}

void SimulationSessionStateMachine::finish(
    const SimulationRunStatus status) noexcept
{
    if (status == SimulationRunStatus::Succeeded) {
        state_ = staleDuringRun_
            ? SimulationSessionState::Stale
            : SimulationSessionState::Current;
    } else if (status == SimulationRunStatus::Cancelled
               || status == SimulationRunStatus::Superseded) {
        state_ = staleDuringRun_
            ? SimulationSessionState::Stale
            : runOriginState_;
    } else {
        state_ = SimulationSessionState::Failed;
    }
    staleDuringRun_ = false;
}

SimulationSessionState SimulationSessionStateMachine::state() const noexcept
{
    return state_;
}

SimulationSessionActions SimulationSessionStateMachine::actions() const noexcept
{
    if (!runnable_) return {};
    if (state_ == SimulationSessionState::Compiling
        || state_ == SimulationSessionState::Running) {
        return {false, true, false};
    }
    return {
        state_ == SimulationSessionState::Ready
            || state_ == SimulationSessionState::Stale
            || state_ == SimulationSessionState::Failed,
        false,
        state_ == SimulationSessionState::Current
            || state_ == SimulationSessionState::Stale
            || state_ == SimulationSessionState::Failed,
    };
}

bool SimulationSessionStateMachine::runnable() const noexcept
{
    return runnable_;
}

std::string_view toString(const SimulationSessionState state) noexcept
{
    switch (state) {
    case SimulationSessionState::Ready: return "ready";
    case SimulationSessionState::Compiling: return "compiling";
    case SimulationSessionState::Running: return "running";
    case SimulationSessionState::Current: return "current";
    case SimulationSessionState::Stale: return "stale";
    case SimulationSessionState::Failed: return "failed";
    }
    return "failed";
}

} // namespace wave
