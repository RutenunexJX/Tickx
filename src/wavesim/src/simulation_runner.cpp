#include "wave/simulation_runner.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>
#include <array>
#include <utility>

namespace wave {
namespace {

QString text(const std::string_view value)
{
    return QString::fromLatin1(
        value.data(), static_cast<qsizetype>(value.size()));
}

QString resolvedExecutable(
    const QString& program,
    const QString& workingDirectory,
    const QProcessEnvironment& environment,
    const bool inheritCurrentProcessPath)
{
    if (program.trimmed().isEmpty()) return {};
    const auto hasDirectory = program.contains(QLatin1Char('/'))
        || program.contains(QLatin1Char('\\'));
    if (QFileInfo(program).isAbsolute() || hasDirectory) {
        auto path = program;
        if (QFileInfo(path).isRelative() && !workingDirectory.isEmpty()) {
            path = QDir(workingDirectory).absoluteFilePath(path);
        }
        const QFileInfo information(path);
        return information.exists() && information.isFile()
            ? information.absoluteFilePath()
            : QString{};
    }

    bool hasPathVariable = false;
    QStringList pathValues;
    for (const auto& key : environment.keys()) {
        if (key.compare(QStringLiteral("PATH"), Qt::CaseInsensitive) == 0) {
            hasPathVariable = true;
            pathValues.append(environment.value(key));
        }
    }
    if (inheritCurrentProcessPath) {
        const auto livePath = qEnvironmentVariable("PATH");
        if (!livePath.isEmpty() && !pathValues.contains(livePath)) {
            pathValues.append(livePath);
        }
    }
    QStringList paths;
    for (const auto& value : pathValues) {
        paths.append(value.split(
            QDir::listSeparator(), Qt::SkipEmptyParts));
    }
    paths.removeDuplicates();
    const auto qtResolved = paths.isEmpty() && !hasPathVariable
            && inheritCurrentProcessPath
        ? QStandardPaths::findExecutable(program)
        : QStandardPaths::findExecutable(program, paths);
    if (!qtResolved.isEmpty()) return qtResolved;

    QStringList names{program};
#ifdef Q_OS_WIN
    if (QFileInfo(program).suffix().isEmpty()) {
        names.append(program + QStringLiteral(".exe"));
        names.append(program + QStringLiteral(".com"));
    }
#endif
    for (const auto& directory : paths) {
        for (const auto& name : names) {
            const QFileInfo candidate(QDir(directory).filePath(name));
            if (candidate.exists() && candidate.isFile()) {
                return candidate.absoluteFilePath();
            }
        }
    }
    return {};
}

void appendOutput(
    QByteArray& target,
    const QByteArray& chunk,
    const int maximumBytes,
    bool& truncated)
{
    if (chunk.isEmpty()) return;
    const auto maximum = std::max(0, maximumBytes);
    const auto room = std::max<qsizetype>(0, maximum - target.size());
    if (room > 0) target.append(chunk.left(room));
    if (chunk.size() > room) truncated = true;
}

QString versionString(const ToolVersion& version)
{
    return QStringLiteral("%1.%2.%3")
        .arg(version.major)
        .arg(version.minor)
        .arg(version.patch);
}

bool versionAtLeast(const ToolVersion& actual, const ToolVersion& minimum)
{
    return std::array{actual.major, actual.minor, actual.patch}
        >= std::array{minimum.major, minimum.minor, minimum.patch};
}

std::optional<ToolVersion> capturedVersion(
    const QString& output,
    const QRegularExpression& expression)
{
    const auto match = expression.match(output);
    if (!match.hasMatch()) return std::nullopt;
    bool majorValid = false;
    bool minorValid = false;
    bool patchValid = true;
    const auto major = match.captured(1).toInt(&majorValid);
    const auto minor = match.captured(2).toInt(&minorValid);
    const auto patchText = match.captured(3);
    const auto patch = patchText.isEmpty()
        ? 0
        : patchText.toInt(&patchValid);
    if (!majorValid || !minorValid || !patchValid) return std::nullopt;
    return ToolVersion{major, minor, patch, match.captured(0)};
}

std::optional<ToolVersion> verilatorVersion(const QString& output)
{
    static const QRegularExpression expression(
        QStringLiteral("\\bVerilator\\s+(\\d+)\\.(\\d+)(?:\\.(\\d+))?"),
        QRegularExpression::CaseInsensitiveOption);
    return capturedVersion(output, expression);
}

struct CompilerVersion {
    CxxCompilerFamily family{CxxCompilerFamily::Unknown};
    std::optional<ToolVersion> version;
};

CompilerVersion compilerVersion(const QString& output)
{
    static const QRegularExpression clangExpression(
        QStringLiteral("\\bclang(?:\\+\\+)?\\s+version\\s+(\\d+)\\.(\\d+)(?:\\.(\\d+))?"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression msvcExpression(
        QStringLiteral("\\b(?:compiler\\s+)?version\\s+(\\d+)\\.(\\d+)(?:\\.(\\d+))?"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression gccExpression(
        QStringLiteral("\\b(?:g\\+\\+|gcc)(?:[^\\r\\n]*?)?(\\d+)\\.(\\d+)(?:\\.(\\d+))?"),
        QRegularExpression::CaseInsensitiveOption);

    if (output.contains(QStringLiteral("clang"), Qt::CaseInsensitive)) {
        return {CxxCompilerFamily::Clang,
                capturedVersion(output, clangExpression)};
    }
    if (output.contains(QStringLiteral("Microsoft"), Qt::CaseInsensitive)
        || output.contains(QStringLiteral("MSVC"), Qt::CaseInsensitive)) {
        return {CxxCompilerFamily::Msvc,
                capturedVersion(output, msvcExpression)};
    }
    if (output.contains(QStringLiteral("g++"), Qt::CaseInsensitive)
        || output.contains(QStringLiteral("gcc"), Qt::CaseInsensitive)
        || output.contains(
            QStringLiteral("Free Software Foundation"),
            Qt::CaseInsensitive)) {
        return {CxxCompilerFamily::Gcc,
                capturedVersion(output, gccExpression)};
    }
    return {};
}

ToolVersion minimumCompilerVersion(
    const CxxCompilerFamily family,
    const ToolchainRequirements& requirements)
{
    switch (family) {
    case CxxCompilerFamily::Gcc:
        return requirements.minimumGcc;
    case CxxCompilerFamily::Clang:
        return requirements.minimumClang;
    case CxxCompilerFamily::Msvc:
        return requirements.minimumMsvc;
    case CxxCompilerFamily::NotApplicable:
    case CxxCompilerFamily::Unknown:
        return {};
    }
    return {};
}

struct ToolCommand {
    QString program;
    QStringList arguments;
};

ToolCommand commandFromEnvironment(
    const QString& value,
    const QStringList& defaultArguments)
{
    auto parts = QProcess::splitCommand(value);
    if (parts.isEmpty()) return {};
    ToolCommand command;
    command.program = parts.takeFirst();
    command.arguments = parts.isEmpty() ? defaultArguments : parts;
    return command;
}

QStringList compilerVersionArguments(const QString& program)
{
    const auto base = QFileInfo(program).completeBaseName();
    if (base.compare(QStringLiteral("cl"), Qt::CaseInsensitive) == 0) {
        return {QStringLiteral("/?")};
    }
    return {QStringLiteral("--version")};
}

ToolCommand verilatorCommand(const ToolchainProbeOptions& options)
{
    const auto defaults = QStringList{QStringLiteral("--version")};
    if (!options.verilatorProgram.isEmpty()) {
        return {options.verilatorProgram,
                options.verilatorArguments.isEmpty()
                    ? defaults
                    : options.verilatorArguments};
    }
    const auto configured = options.environment.value(QStringLiteral("VERILATOR"));
    if (!configured.trimmed().isEmpty()) {
        return commandFromEnvironment(configured, defaults);
    }
    const auto root = options.environment.value(QStringLiteral("VERILATOR_ROOT"));
    if (!root.trimmed().isEmpty()) {
        auto executable = QDir(root).filePath(QStringLiteral("bin/verilator"));
#ifdef Q_OS_WIN
        if (QFileInfo(executable + QStringLiteral(".exe")).exists()) {
            executable += QStringLiteral(".exe");
        }
#endif
        return {executable, defaults};
    }
    return {QStringLiteral("verilator"), defaults};
}

ToolCommand compilerCommand(const ToolchainProbeOptions& options)
{
    if (!options.cxxProgram.isEmpty()) {
        return {options.cxxProgram,
                options.cxxArguments.isEmpty()
                    ? compilerVersionArguments(options.cxxProgram)
                    : options.cxxArguments};
    }
    const auto configured = options.environment.value(QStringLiteral("CXX"));
    if (!configured.trimmed().isEmpty()) {
        const auto command = commandFromEnvironment(configured, {});
        if (!command.program.isEmpty()) {
            return {command.program,
                    command.arguments.isEmpty()
                        ? compilerVersionArguments(command.program)
                        : command.arguments};
        }
    }

#ifdef Q_OS_WIN
    const auto candidates = std::array{
        QStringLiteral("g++"),
        QStringLiteral("clang++"),
        QStringLiteral("cl")};
#else
    const auto candidates = std::array{
        QStringLiteral("c++"),
        QStringLiteral("g++"),
        QStringLiteral("clang++")};
#endif
    for (const auto& candidate : candidates) {
        if (!resolvedExecutable(
                candidate,
                {},
                options.environment,
                options.inheritCurrentProcessPath).isEmpty()) {
            return {candidate, compilerVersionArguments(candidate)};
        }
    }
    return {candidates.front(), compilerVersionArguments(candidates.front())};
}

ToolProbeResult evaluateTool(
    const ToolKind kind,
    ProcessRunResult process,
    const ToolchainRequirements& requirements)
{
    ToolProbeResult result;
    result.kind = kind;
    result.compilerFamily = kind == ToolKind::Verilator
        ? CxxCompilerFamily::NotApplicable
        : CxxCompilerFamily::Unknown;
    result.process = std::move(process);

    switch (result.process.state) {
    case ProcessRunState::ProgramNotFound:
        result.status = ToolProbeStatus::NotFound;
        result.diagnostic = QStringLiteral("%1 executable was not found.")
                                .arg(text(toString(kind)));
        return result;
    case ProcessRunState::TimedOut:
        result.status = ToolProbeStatus::TimedOut;
        result.diagnostic = QStringLiteral("%1 version probe timed out.")
                                .arg(text(toString(kind)));
        return result;
    case ProcessRunState::Cancelled:
        result.status = ToolProbeStatus::Cancelled;
        result.diagnostic = QStringLiteral("%1 version probe was cancelled.")
                                .arg(text(toString(kind)));
        return result;
    case ProcessRunState::StartFailed:
    case ProcessRunState::NonZeroExit:
    case ProcessRunState::Crashed:
        result.status = ToolProbeStatus::ExecutionFailed;
        result.diagnostic = result.process.errorMessage;
        return result;
    case ProcessRunState::Succeeded:
        break;
    }

    const auto output = QString::fromUtf8(result.process.standardOutput)
        + QLatin1Char('\n')
        + QString::fromUtf8(result.process.standardError);
    if (kind == ToolKind::Verilator) {
        result.minimumVersion = requirements.minimumVerilator;
        result.version = verilatorVersion(output);
    } else {
        const auto compiler = compilerVersion(output);
        result.compilerFamily = compiler.family;
        result.version = compiler.version;
        result.minimumVersion = minimumCompilerVersion(
            compiler.family, requirements);
    }
    if (!result.version || !result.version->valid()
        || !result.minimumVersion.valid()) {
        result.status = ToolProbeStatus::VersionUnrecognized;
        result.diagnostic = QStringLiteral(
            "%1 ran successfully, but its supported version could not be identified.")
                                .arg(text(toString(kind)));
        return result;
    }
    if (!versionAtLeast(*result.version, result.minimumVersion)) {
        result.status = ToolProbeStatus::IncompatibleVersion;
        result.diagnostic = QStringLiteral("%1 %2 is older than required %3.")
                                .arg(
                                    text(toString(kind)),
                                    versionString(*result.version),
                                    versionString(result.minimumVersion));
        return result;
    }
    result.status = ToolProbeStatus::Ready;
    result.diagnostic = QStringLiteral("%1 %2 is ready.")
                            .arg(
                                text(toString(kind)),
                                versionString(*result.version));
    return result;
}

ToolchainProbeStatus overallStatus(
    const ToolProbeResult& verilator,
    const ToolProbeResult& compiler)
{
    const auto has = [&verilator, &compiler](const ToolProbeStatus status) {
        return verilator.status == status || compiler.status == status;
    };
    if (has(ToolProbeStatus::Cancelled)) return ToolchainProbeStatus::Cancelled;
    if (has(ToolProbeStatus::TimedOut)) return ToolchainProbeStatus::TimedOut;
    if (has(ToolProbeStatus::NotFound)) return ToolchainProbeStatus::Unavailable;
    if (has(ToolProbeStatus::IncompatibleVersion)) {
        return ToolchainProbeStatus::Incompatible;
    }
    if (has(ToolProbeStatus::VersionUnrecognized)
        || has(ToolProbeStatus::ExecutionFailed)) {
        return ToolchainProbeStatus::Failed;
    }
    return ToolchainProbeStatus::Ready;
}

QJsonObject versionJson(const ToolVersion& version)
{
    return {
        {QStringLiteral("major"), version.major},
        {QStringLiteral("minor"), version.minor},
        {QStringLiteral("patch"), version.patch},
        {QStringLiteral("normalized"), versionString(version)},
        {QStringLiteral("sourceText"), version.sourceText},
    };
}

QJsonObject processJson(const ProcessRunResult& process)
{
    QJsonObject object{
        {QStringLiteral("state"), text(toString(process.state))},
        {QStringLiteral("requestedProgram"), process.requestedProgram},
        {QStringLiteral("resolvedProgram"), process.resolvedProgram},
        {QStringLiteral("arguments"), QJsonArray::fromStringList(process.arguments)},
        {QStringLiteral("stdout"), QString::fromUtf8(process.standardOutput)},
        {QStringLiteral("stderr"), QString::fromUtf8(process.standardError)},
        {QStringLiteral("stdoutTruncated"), process.standardOutputTruncated},
        {QStringLiteral("stderrTruncated"), process.standardErrorTruncated},
        {QStringLiteral("durationMs"), process.durationMs},
        {QStringLiteral("error"), process.errorMessage},
    };
    if (process.exitCode >= 0) {
        object.insert(QStringLiteral("exitCode"), process.exitCode);
    } else {
        object.insert(QStringLiteral("exitCode"), QJsonValue::Null);
    }
    return object;
}

QJsonObject toolJson(const ToolProbeResult& tool)
{
    QJsonObject object{
        {QStringLiteral("kind"), text(toString(tool.kind))},
        {QStringLiteral("status"), text(toString(tool.status))},
        {QStringLiteral("compilerFamily"), text(toString(tool.compilerFamily))},
        {QStringLiteral("minimumVersion"),
         tool.minimumVersion.valid()
             ? QJsonValue(versionJson(tool.minimumVersion))
             : QJsonValue(QJsonValue::Null)},
        {QStringLiteral("version"),
         tool.version
             ? QJsonValue(versionJson(*tool.version))
             : QJsonValue(QJsonValue::Null)},
        {QStringLiteral("diagnostic"), tool.diagnostic},
        {QStringLiteral("process"), processJson(tool.process)},
    };
    return object;
}

} // namespace

struct ProcessRunner::Impl {
    QProcess process;
    QTimer timeout;
    QElapsedTimer elapsed;
    ProcessRunRequest request;
    ProcessRunResult result;
    Completion completion;
    std::optional<ProcessRunState> terminalOverride;
    bool active{false};

    Impl()
    {
        timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, [&] {
            if (!active) return;
            terminalOverride = ProcessRunState::TimedOut;
            result.errorMessage = QStringLiteral(
                "Process exceeded the %1 ms timeout.").arg(request.timeoutMs);
            if (process.state() == QProcess::NotRunning) {
                finish(*terminalOverride);
            } else {
                process.kill();
            }
        });
        QObject::connect(
            &process,
            &QProcess::readyReadStandardOutput,
            [&] { captureStandardOutput(); });
        QObject::connect(
            &process,
            &QProcess::readyReadStandardError,
            [&] { captureStandardError(); });
        QObject::connect(
            &process,
            &QProcess::errorOccurred,
            [&](const QProcess::ProcessError error) {
                if (!active || error != QProcess::FailedToStart) return;
                finish(ProcessRunState::StartFailed, process.errorString());
            });
        QObject::connect(
            &process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            [&](const int exitCode, const QProcess::ExitStatus exitStatus) {
                if (!active) return;
                result.exitCode = exitCode;
                if (terminalOverride) {
                    finish(*terminalOverride, result.errorMessage);
                } else if (exitStatus == QProcess::CrashExit) {
                    finish(
                        ProcessRunState::Crashed,
                        QStringLiteral("Process terminated abnormally."));
                } else if (exitCode != 0) {
                    finish(
                        ProcessRunState::NonZeroExit,
                        QStringLiteral("Process exited with code %1.").arg(exitCode));
                } else {
                    finish(ProcessRunState::Succeeded);
                }
            });
    }

    void captureStandardOutput()
    {
        appendOutput(
            result.standardOutput,
            process.readAllStandardOutput(),
            request.maxOutputBytes,
            result.standardOutputTruncated);
    }

    void captureStandardError()
    {
        appendOutput(
            result.standardError,
            process.readAllStandardError(),
            request.maxOutputBytes,
            result.standardErrorTruncated);
    }

    void finish(
        const ProcessRunState state,
        const QString& error = {})
    {
        if (!active) return;
        timeout.stop();
        captureStandardOutput();
        captureStandardError();
        result.state = state;
        result.durationMs = elapsed.isValid() ? elapsed.elapsed() : 0;
        if (!error.isEmpty()) result.errorMessage = error;
        auto callback = std::move(completion);
        auto completed = std::move(result);
        active = false;
        terminalOverride.reset();
        callback(std::move(completed));
    }

    bool start(ProcessRunRequest nextRequest, Completion nextCompletion)
    {
        if (active || nextRequest.program.trimmed().isEmpty()
            || !nextCompletion || nextRequest.timeoutMs <= 0
            || nextRequest.maxOutputBytes < 0) {
            return false;
        }
        request = std::move(nextRequest);
        completion = std::move(nextCompletion);
        result = {};
        result.requestedProgram = request.program;
        result.arguments = request.arguments;
        terminalOverride.reset();
        active = true;
        elapsed.start();
        result.resolvedProgram = resolvedExecutable(
            request.program,
            request.workingDirectory,
            request.environment,
            request.inheritCurrentProcessPath);
        if (result.resolvedProgram.isEmpty()) {
            QTimer::singleShot(0, &process, [&] {
                if (!active) return;
                const auto state = terminalOverride.value_or(
                    ProcessRunState::ProgramNotFound);
                finish(
                    state,
                    state == ProcessRunState::Cancelled
                        ? QStringLiteral("Process was cancelled before start.")
                        : QStringLiteral("Executable was not found: %1")
                              .arg(request.program));
            });
            return true;
        }

        process.setProgram(result.resolvedProgram);
        process.setArguments(request.arguments);
        process.setWorkingDirectory(request.workingDirectory);
        process.setProcessEnvironment(request.environment);
        process.setProcessChannelMode(QProcess::SeparateChannels);
        timeout.start(request.timeoutMs);
        process.start();
        return true;
    }

    bool cancel()
    {
        if (!active) return false;
        terminalOverride = ProcessRunState::Cancelled;
        result.errorMessage = QStringLiteral("Process was cancelled.");
        timeout.stop();
        if (process.state() == QProcess::NotRunning) {
            QTimer::singleShot(0, &process, [&] {
                if (active && terminalOverride) finish(*terminalOverride);
            });
        } else {
            process.kill();
        }
        return true;
    }
};

ProcessRunner::ProcessRunner()
    : impl_(std::make_unique<Impl>())
{
}

ProcessRunner::~ProcessRunner()
{
    if (impl_->active && impl_->process.state() != QProcess::NotRunning) {
        impl_->process.disconnect();
        impl_->process.kill();
    }
}

bool ProcessRunner::start(ProcessRunRequest request, Completion completion)
{
    return impl_->start(std::move(request), std::move(completion));
}

bool ProcessRunner::cancel()
{
    return impl_->cancel();
}

bool ProcessRunner::running() const noexcept
{
    return impl_->active;
}

struct ToolchainProbeRunner::Impl {
    ProcessRunner verilatorRunner;
    ProcessRunner compilerRunner;
    ToolchainProbeOptions options;
    Completion completion;
    QElapsedTimer elapsed;
    std::optional<ToolProbeResult> verilator;
    std::optional<ToolProbeResult> compiler;
    bool active{false};

    void accept(const ToolKind kind, ProcessRunResult process)
    {
        if (!active) return;
        auto result = evaluateTool(kind, std::move(process), options.requirements);
        if (kind == ToolKind::Verilator) {
            verilator = std::move(result);
        } else {
            compiler = std::move(result);
        }
        if (!verilator || !compiler) return;

        ToolchainProbeReport report;
        report.verilator = std::move(*verilator);
        report.cxxCompiler = std::move(*compiler);
        report.status = overallStatus(report.verilator, report.cxxCompiler);
        report.durationMs = elapsed.elapsed();
        auto callback = std::move(completion);
        active = false;
        callback(std::move(report));
    }

    bool start(ToolchainProbeOptions nextOptions, Completion nextCompletion)
    {
        if (active || !nextCompletion || nextOptions.timeoutMs <= 0
            || nextOptions.maxOutputBytes < 0) {
            return false;
        }
        options = std::move(nextOptions);
        completion = std::move(nextCompletion);
        verilator.reset();
        compiler.reset();
        active = true;
        elapsed.start();

        const auto verilatorSpec = verilatorCommand(options);
        const auto compilerSpec = compilerCommand(options);
        ProcessRunRequest verilatorRequest;
        verilatorRequest.program = verilatorSpec.program;
        verilatorRequest.arguments = verilatorSpec.arguments;
        verilatorRequest.environment = options.environment;
        verilatorRequest.inheritCurrentProcessPath =
            options.inheritCurrentProcessPath;
        verilatorRequest.timeoutMs = options.timeoutMs;
        verilatorRequest.maxOutputBytes = options.maxOutputBytes;
        ProcessRunRequest compilerRequest;
        compilerRequest.program = compilerSpec.program;
        compilerRequest.arguments = compilerSpec.arguments;
        compilerRequest.environment = options.environment;
        compilerRequest.inheritCurrentProcessPath =
            options.inheritCurrentProcessPath;
        compilerRequest.timeoutMs = options.timeoutMs;
        compilerRequest.maxOutputBytes = options.maxOutputBytes;

        const auto verilatorStarted = verilatorRunner.start(
            std::move(verilatorRequest),
            [&](ProcessRunResult result) {
                accept(ToolKind::Verilator, std::move(result));
            });
        const auto compilerStarted = compilerRunner.start(
            std::move(compilerRequest),
            [&](ProcessRunResult result) {
                accept(ToolKind::CxxCompiler, std::move(result));
            });
        if (verilatorStarted && compilerStarted) return true;

        active = false;
        completion = {};
        if (verilatorStarted) static_cast<void>(verilatorRunner.cancel());
        if (compilerStarted) static_cast<void>(compilerRunner.cancel());
        return false;
    }

    bool cancel()
    {
        if (!active) return false;
        const auto cancelledVerilator = verilatorRunner.cancel();
        const auto cancelledCompiler = compilerRunner.cancel();
        return cancelledVerilator || cancelledCompiler;
    }
};

ToolchainProbeRunner::ToolchainProbeRunner()
    : impl_(std::make_unique<Impl>())
{
}

ToolchainProbeRunner::~ToolchainProbeRunner() = default;

bool ToolchainProbeRunner::start(
    ToolchainProbeOptions options,
    Completion completion)
{
    return impl_->start(std::move(options), std::move(completion));
}

bool ToolchainProbeRunner::cancel()
{
    return impl_->cancel();
}

bool ToolchainProbeRunner::running() const noexcept
{
    return impl_->active;
}

QJsonObject toolchainProbeReportJson(const ToolchainProbeReport& report)
{
    return {
        {QStringLiteral("schema"), QString::fromLatin1(ToolchainProbeReportSchema)},
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("status"), text(toString(report.status))},
        {QStringLiteral("ok"), report.ready()},
        {QStringLiteral("durationMs"), report.durationMs},
        {QStringLiteral("tools"),
         QJsonObject{
             {QStringLiteral("verilator"), toolJson(report.verilator)},
             {QStringLiteral("cxx"), toolJson(report.cxxCompiler)},
         }},
    };
}

std::string_view toString(const ProcessRunState state) noexcept
{
    switch (state) {
    case ProcessRunState::Succeeded: return "succeeded";
    case ProcessRunState::ProgramNotFound: return "program-not-found";
    case ProcessRunState::StartFailed: return "start-failed";
    case ProcessRunState::NonZeroExit: return "nonzero-exit";
    case ProcessRunState::Crashed: return "crashed";
    case ProcessRunState::TimedOut: return "timed-out";
    case ProcessRunState::Cancelled: return "cancelled";
    }
    return "start-failed";
}

std::string_view toString(const ToolKind kind) noexcept
{
    switch (kind) {
    case ToolKind::Verilator: return "verilator";
    case ToolKind::CxxCompiler: return "cxx";
    }
    return "verilator";
}

std::string_view toString(const CxxCompilerFamily family) noexcept
{
    switch (family) {
    case CxxCompilerFamily::NotApplicable: return "not-applicable";
    case CxxCompilerFamily::Gcc: return "gcc";
    case CxxCompilerFamily::Clang: return "clang";
    case CxxCompilerFamily::Msvc: return "msvc";
    case CxxCompilerFamily::Unknown: return "unknown";
    }
    return "unknown";
}

std::string_view toString(const ToolProbeStatus status) noexcept
{
    switch (status) {
    case ToolProbeStatus::Ready: return "ready";
    case ToolProbeStatus::NotFound: return "not-found";
    case ToolProbeStatus::IncompatibleVersion: return "incompatible-version";
    case ToolProbeStatus::VersionUnrecognized: return "version-unrecognized";
    case ToolProbeStatus::ExecutionFailed: return "execution-failed";
    case ToolProbeStatus::TimedOut: return "timed-out";
    case ToolProbeStatus::Cancelled: return "cancelled";
    }
    return "execution-failed";
}

std::string_view toString(const ToolchainProbeStatus status) noexcept
{
    switch (status) {
    case ToolchainProbeStatus::Ready: return "ready";
    case ToolchainProbeStatus::Unavailable: return "unavailable";
    case ToolchainProbeStatus::Incompatible: return "incompatible";
    case ToolchainProbeStatus::Failed: return "failed";
    case ToolchainProbeStatus::TimedOut: return "timed-out";
    case ToolchainProbeStatus::Cancelled: return "cancelled";
    }
    return "failed";
}

} // namespace wave
