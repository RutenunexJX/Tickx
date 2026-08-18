#include "wave/automation.h"
#include "wave/project_io.h"

#include "scenario_selection.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>

#include <algorithm>
#include <array>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace {

enum ExitCode {
    Success = 0,
    UsageError = 2,
    InputError = 3,
    Rejected = 4,
    OutputError = 5,
};

struct CommonOptions {
    bool pretty{false};
    std::optional<std::string> scenarioId;
};

QByteArray jsonBytes(const QJsonObject& object, const bool pretty)
{
    return QJsonDocument(object).toJson(
        pretty ? QJsonDocument::Indented : QJsonDocument::Compact);
}

void writeJson(QTextStream& stream, const QJsonObject& object, const bool pretty)
{
    stream << jsonBytes(object, pretty);
    stream.flush();
}

QJsonObject errorDocument(
    const QString& command,
    const QString& code,
    const QString& message,
    const std::optional<int> operation = std::nullopt)
{
    QJsonObject error{
        {QStringLiteral("code"), code},
        {QStringLiteral("message"), message},
    };
    if (operation && *operation >= 0) {
        error.insert(QStringLiteral("operation"), *operation);
    }
    return {
        {QStringLiteral("schema"),
         QString::fromLatin1(wave::AutomationReportSchema)},
        {QStringLiteral("command"), command},
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), error},
    };
}

int fail(
    const QString& command,
    const QString& code,
    const QString& message,
    const int exitCode,
    const bool pretty,
    const std::optional<int> operation = std::nullopt)
{
    QTextStream stream(stderr);
    writeJson(
        stream,
        errorDocument(command, code, message, operation),
        pretty);
    return exitCode;
}

void printUsage()
{
    QTextStream(stdout)
        << "Wave Workbench headless CLI\n"
           "Usage:\n"
           "  wave-cli capabilities [--pretty]\n"
           "  wave-cli new <output.wave.json> [--name=NAME] "
           "[--scenario-name=NAME] [--duration=TIME] [--timebase-ps=N] "
           "[--project-id=ID] [--scenario-id=ID] "
           "[--operations=FILE|-] [--dry-run] [--pretty]\n"
           "  wave-cli inspect <project.wave.json> [--scenario=SELECTOR] "
           "[--summary] [--pretty]\n"
           "  wave-cli signals <project.wave.json> [--match=TEXT] "
           "[--kind=KIND] [--exact] [--limit=N] "
           "[--scenario=SELECTOR] [--pretty]\n"
           "  wave-cli sample <project.wave.json> --at=TIME "
           "[--scenario=SELECTOR] [--clock=SELECTOR] "
           "[--lane=SELECTOR ...] [--pretty]\n"
           "  wave-cli window <project.wave.json> --start=TIME --end=TIME "
           "[--scenario=SELECTOR] [--clock=SELECTOR] "
           "[--lane=SELECTOR ...] [--pretty]\n"
           "  wave-cli edges <project.wave.json> [--start=TIME] [--end=TIME] "
           "[--edge=initial|rising|falling|change] [--limit=N] "
           "[--scenario=SELECTOR] [--clock=SELECTOR] "
           "[--lane=SELECTOR ...] [--pretty]\n"
           "  wave-cli markers <project.wave.json> [--match=TEXT] "
           "[--exact] [--kind=point|interval|phase|error|note] "
           "[--start=TIME] [--end=TIME] [--limit=N] "
           "[--scenario=SELECTOR] [--clock=SELECTOR] [--pretty]\n"
           "  wave-cli relations <project.wave.json> [--match=TEXT] "
           "[--exact] [--severity=information|warning|error] "
           "[--start=TIME] [--end=TIME] [--limit=N] "
           "[--scenario=SELECTOR] [--clock=SELECTOR] "
           "[--lane=SELECTOR ...] [--pretty]\n"
           "  wave-cli validate <project.wave.json> [--scenario=SELECTOR] "
           "[--fail-on-warning] [--pretty]\n"
           "  wave-cli apply <project.wave.json> <operations.json|-> "
           "(--output=PATH|--in-place|--dry-run) [--scenario=SELECTOR] "
           "[--expect-sha256=HEX] [--backup[=PATH]] [--pretty]\n"
           "\n"
           "Compatibility adapters (legacy text output and exit codes):\n"
           "  wave-cli generate <wave-generate arguments...>\n"
           "  wave-cli compare <wave-compare arguments...>\n"
           "  wave-cli bridge <wave-bridge arguments...>\n"
           "\n"
           "Exit codes: 0 success, 2 usage, 3 input, 4 rejected/invalid, "
           "5 output failure.\n";
}

struct CompatibilityCommand {
    QString name;
    QString executable;
    QString summary;
    bool canWriteProject{false};
};

const std::array<CompatibilityCommand, 3>& compatibilityCommands()
{
    static const std::array<CompatibilityCommand, 3> commands{{
        {
            QStringLiteral("generate"),
            QStringLiteral("wave-generate"),
            QStringLiteral("Generate HDL, verification and documentation artifacts."),
            false,
        },
        {
            QStringLiteral("compare"),
            QStringLiteral("wave-compare"),
            QStringLiteral("Compare expected waveforms with a referenced VCD or CSV trace."),
            false,
        },
        {
            QStringLiteral("bridge"),
            QStringLiteral("wave-bridge"),
            QStringLiteral("Create or consume cross-application integration artifacts."),
            true,
        },
    }};
    return commands;
}

const CompatibilityCommand* compatibilityCommand(const QString& name)
{
    const auto& commands = compatibilityCommands();
    const auto found = std::find_if(
        commands.cbegin(),
        commands.cend(),
        [&name](const CompatibilityCommand& candidate) {
            return candidate.name == name;
        });
    return found == commands.cend() ? nullptr : &*found;
}

void printCompatibilityHelp(const CompatibilityCommand& command)
{
    QTextStream(stdout)
        << "wave-cli " << command.name << " forwards arguments to "
        << command.executable << ".\n"
        << command.summary << "\n\n"
        << "Usage:\n  wave-cli " << command.name
        << " <" << command.executable << " arguments...>\n\n"
        << "This compatibility adapter preserves the legacy command's text "
           "output and exit code. Run the sibling "
        << command.executable << " executable directly for the same behavior.\n";
}

int runCompatibilityCommand(
    const CompatibilityCommand& command,
    const QStringList& arguments)
{
    if (arguments.size() == 1
        && (arguments.front() == QStringLiteral("--help")
            || arguments.front() == QStringLiteral("-h"))) {
        printCompatibilityHelp(command);
        return Success;
    }

    auto executableName = command.executable;
#ifdef Q_OS_WIN
    executableName += QStringLiteral(".exe");
#endif
    const auto executablePath = QDir(
        QCoreApplication::applicationDirPath()).absoluteFilePath(executableName);
    if (!QFileInfo::exists(executablePath)) {
        return fail(
            command.name,
            QStringLiteral("compatibility-command-unavailable"),
            QStringLiteral(
                "The compatibility executable is not installed beside wave-cli: %1")
                .arg(executablePath),
            OutputError,
            false);
    }

    QProcess process;
    process.setProcessChannelMode(QProcess::ForwardedChannels);
    process.start(executablePath, arguments);
    if (!process.waitForStarted()) {
        return fail(
            command.name,
            QStringLiteral("compatibility-command-start-failed"),
            process.errorString(),
            OutputError,
            false);
    }
    if (!process.waitForFinished(-1)) {
        return fail(
            command.name,
            QStringLiteral("compatibility-command-wait-failed"),
            process.errorString(),
            OutputError,
            false);
    }
    if (process.exitStatus() != QProcess::NormalExit) {
        return fail(
            command.name,
            QStringLiteral("compatibility-command-crashed"),
            QStringLiteral("%1 did not exit normally.").arg(command.executable),
            OutputError,
            false);
    }
    return process.exitCode();
}

int runCapabilities(const QStringList& arguments)
{
    auto pretty = false;
    for (const auto& argument : arguments) {
        if (argument == QStringLiteral("--pretty")) {
            pretty = true;
        } else {
            return fail(
                QStringLiteral("capabilities"),
                QStringLiteral("usage"),
                QStringLiteral(
                    "capabilities accepts only --pretty."),
                UsageError,
                pretty);
        }
    }
    const auto capabilities = wave::describeAutomationCapabilities();
    if (!capabilities.ok()) {
        return fail(
            QStringLiteral("capabilities"),
            QStringLiteral("capabilities-failed"),
            capabilities.error,
            Rejected,
            pretty);
    }
    auto document = capabilities.json;
    QJsonArray commands;
    for (const auto& adapter : compatibilityCommands()) {
        commands.append(QJsonObject{
            {QStringLiteral("name"), adapter.name},
            {QStringLiteral("requiresProject"), true},
            {QStringLiteral("canWriteProject"), adapter.canWriteProject},
            {QStringLiteral("compatibilityAdapter"), true},
            {QStringLiteral("legacyExecutable"), adapter.executable},
            {QStringLiteral("structuredOutput"), false},
            {QStringLiteral("argumentsForwardedVerbatim"), true},
        });
    }
    document.insert(QStringLiteral("compatibilityCommands"), commands);
    document.insert(
        QStringLiteral("compatibilityCommandCount"),
        commands.size());
    auto features = document.value(QStringLiteral("features")).toObject();
    features.insert(QStringLiteral("compatibilityAdapters"), true);
    document.insert(QStringLiteral("features"), features);

    QTextStream stream(stdout);
    writeJson(stream, document, pretty);
    return Success;
}

bool readFile(const QString& path, QByteArray& bytes, QString& error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = file.errorString();
        return false;
    }
    bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        error = file.errorString();
        return false;
    }
    return true;
}

bool writeBytesAtomic(
    const QString& path,
    const QByteArray& bytes,
    QString& error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        error = file.errorString();
        return false;
    }
    if (file.write(bytes) != bytes.size()) {
        error = file.errorString();
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        error = file.errorString();
        return false;
    }
    return true;
}

bool samePath(const QString& left, const QString& right)
{
    const auto absoluteLeft = QFileInfo(left).absoluteFilePath();
    const auto absoluteRight = QFileInfo(right).absoluteFilePath();
#ifdef Q_OS_WIN
    return absoluteLeft.compare(absoluteRight, Qt::CaseInsensitive) == 0;
#else
    return absoluteLeft == absoluteRight;
#endif
}

bool readOperations(
    const QString& path,
    QByteArray& bytes,
    QString& error)
{
    if (path != QStringLiteral("-")) return readFile(path, bytes, error);
    QFile input;
    if (!input.open(
            0,
            QIODevice::ReadOnly,
            QFileDevice::DontCloseHandle)) {
        error = QStringLiteral("Cannot read operations from stdin.");
        return false;
    }
    bytes = input.readAll();
    if (input.error() != QFileDevice::NoError) {
        error = input.errorString();
        return false;
    }
    return true;
}

QString sha256(const QByteArray& bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

QJsonArray warningsArray(const QStringList& warnings)
{
    QJsonArray result;
    for (const auto& warning : warnings) result.append(warning);
    return result;
}

bool parseCommonOption(
    const QString& argument,
    CommonOptions& options,
    QString& error)
{
    if (argument == QStringLiteral("--pretty")) {
        options.pretty = true;
        return true;
    }
    if (argument.startsWith(QStringLiteral("--scenario="))) {
        const auto id = argument.mid(QStringLiteral("--scenario=").size()).trimmed();
        if (id.isEmpty()) {
            error = QStringLiteral(
                "--scenario requires a non-empty ID or unique name.");
            return false;
        }
        options.scenarioId = id.toStdString();
        return true;
    }
    return false;
}

std::optional<std::int64_t> jsonCount(
    const QJsonObject& object,
    const QString& name)
{
    const auto value = object.value(name);
    if (value.isString()) {
        bool valid = false;
        const auto parsed = value.toString().toLongLong(&valid);
        return valid ? std::optional<std::int64_t>{parsed} : std::nullopt;
    }
    if (value.isDouble()) return static_cast<std::int64_t>(value.toDouble());
    return std::nullopt;
}

int runNew(const QStringList& arguments)
{
    bool pretty = false;
    bool dryRun = false;
    QString outputArgument;
    QString projectName;
    QString scenarioName = QStringLiteral("Waveform");
    QString durationArgument = QStringLiteral("200 ns");
    QString timebaseArgument = QStringLiteral("1");
    QString projectId;
    QString scenarioId;
    QString operationsArgument;
    std::set<QString> provided;
    const auto setOption =
        [&provided](
            const QString& name,
            const QString& value,
            QString& destination,
            QString& error) {
        if (!provided.insert(name).second) {
            error = QStringLiteral("%1 may be provided only once.").arg(name);
            return false;
        }
        if (value.trimmed().isEmpty()) {
            error = QStringLiteral("%1 requires a non-empty value.").arg(name);
            return false;
        }
        destination = value.trimmed();
        return true;
    };

    QString error;
    for (const auto& argument : arguments) {
        if (argument == QStringLiteral("--pretty")) {
            pretty = true;
        } else if (argument == QStringLiteral("--dry-run")) {
            dryRun = true;
        } else if (argument.startsWith(QStringLiteral("--name="))) {
            if (!setOption(
                    QStringLiteral("--name"),
                    argument.mid(QStringLiteral("--name=").size()),
                    projectName,
                    error)) {
                return fail(
                    QStringLiteral("new"),
                    QStringLiteral("usage"),
                    error,
                    UsageError,
                    pretty);
            }
        } else if (argument.startsWith(QStringLiteral("--scenario-name="))) {
            if (!setOption(
                    QStringLiteral("--scenario-name"),
                    argument.mid(QStringLiteral("--scenario-name=").size()),
                    scenarioName,
                    error)) {
                return fail(
                    QStringLiteral("new"),
                    QStringLiteral("usage"),
                    error,
                    UsageError,
                    pretty);
            }
        } else if (argument.startsWith(QStringLiteral("--duration="))) {
            if (!setOption(
                    QStringLiteral("--duration"),
                    argument.mid(QStringLiteral("--duration=").size()),
                    durationArgument,
                    error)) {
                return fail(
                    QStringLiteral("new"),
                    QStringLiteral("usage"),
                    error,
                    UsageError,
                    pretty);
            }
        } else if (argument.startsWith(QStringLiteral("--timebase-ps="))) {
            if (!setOption(
                    QStringLiteral("--timebase-ps"),
                    argument.mid(QStringLiteral("--timebase-ps=").size()),
                    timebaseArgument,
                    error)) {
                return fail(
                    QStringLiteral("new"),
                    QStringLiteral("usage"),
                    error,
                    UsageError,
                    pretty);
            }
        } else if (argument.startsWith(QStringLiteral("--project-id="))) {
            if (!setOption(
                    QStringLiteral("--project-id"),
                    argument.mid(QStringLiteral("--project-id=").size()),
                    projectId,
                    error)) {
                return fail(
                    QStringLiteral("new"),
                    QStringLiteral("usage"),
                    error,
                    UsageError,
                    pretty);
            }
        } else if (argument.startsWith(QStringLiteral("--scenario-id="))) {
            if (!setOption(
                    QStringLiteral("--scenario-id"),
                    argument.mid(QStringLiteral("--scenario-id=").size()),
                    scenarioId,
                    error)) {
                return fail(
                    QStringLiteral("new"),
                    QStringLiteral("usage"),
                    error,
                    UsageError,
                    pretty);
            }
        } else if (argument.startsWith(QStringLiteral("--operations="))) {
            if (!setOption(
                    QStringLiteral("--operations"),
                    argument.mid(QStringLiteral("--operations=").size()),
                    operationsArgument,
                    error)) {
                return fail(
                    QStringLiteral("new"),
                    QStringLiteral("usage"),
                    error,
                    UsageError,
                    pretty);
            }
        } else if (argument.startsWith(QLatin1Char('-'))) {
            return fail(
                QStringLiteral("new"),
                QStringLiteral("usage"),
                QStringLiteral("Unknown option: %1").arg(argument),
                UsageError,
                pretty);
        } else if (outputArgument.isEmpty()) {
            outputArgument = argument;
        } else {
            return fail(
                QStringLiteral("new"),
                QStringLiteral("usage"),
                QStringLiteral("new accepts one output path."),
                UsageError,
                pretty);
        }
    }
    if (outputArgument.isEmpty()) {
        return fail(
            QStringLiteral("new"),
            QStringLiteral("usage"),
            QStringLiteral("new requires an output path."),
            UsageError,
            pretty);
    }
    if (!outputArgument.endsWith(
            QStringLiteral(".wave.json"), Qt::CaseInsensitive)) {
        outputArgument += QStringLiteral(".wave.json");
    }
    const auto outputPath = QFileInfo(outputArgument).absoluteFilePath();
    if (QFileInfo::exists(outputPath)) {
        return fail(
            QStringLiteral("new"),
            QStringLiteral("output-exists"),
            QStringLiteral(
                "New project output already exists; choose another path."),
            Rejected,
            pretty);
    }
    if (projectName.isEmpty()) {
        projectName = QFileInfo(outputPath).fileName();
        projectName.chop(QStringLiteral(".wave.json").size());
        if (projectName.trimmed().isEmpty()) {
            projectName = QStringLiteral("Untitled");
        }
    }

    bool validTimebase = false;
    const auto timebaseValue =
        timebaseArgument.toLongLong(&validTimebase);
    wave::TimeBase timebase{timebaseValue};
    if (!validTimebase || !timebase.isValid()) {
        return fail(
            QStringLiteral("new"),
            QStringLiteral("usage"),
            QStringLiteral("--timebase-ps must be a positive integer."),
            UsageError,
            pretty);
    }
    wave::Project timeContext;
    timeContext.timeBase = timebase;
    const auto parsedDuration = wave::parseAutomationTime(
        timeContext, durationArgument);
    if (!parsedDuration.ok() || *parsedDuration.tick <= 0) {
        return fail(
            QStringLiteral("new"),
            QStringLiteral("time-invalid"),
            parsedDuration.ok()
                ? QStringLiteral("Scenario duration must be positive.")
                : parsedDuration.error,
            Rejected,
            pretty);
    }

    wave::AutomationNewProjectOptions createOptions;
    createOptions.projectId = projectId.toStdString();
    createOptions.projectName = projectName.toStdString();
    createOptions.scenarioId = scenarioId.toStdString();
    createOptions.scenarioName = scenarioName.toStdString();
    createOptions.timeBase = timebase;
    createOptions.duration = *parsedDuration.tick;
    auto created = wave::createProjectForAutomation(createOptions);
    if (!created.ok()) {
        return fail(
            QStringLiteral("new"),
            QStringLiteral("project-invalid"),
            created.error,
            Rejected,
            pretty);
    }

    QJsonArray operationResults;
    QJsonObject changes{
        {QStringLiteral("projectCreated"), true},
        {QStringLiteral("scenarioCreated"), true},
    };
    auto operationCount = 0;
    auto project = std::move(*created.project);
    if (!operationsArgument.isEmpty()) {
        QByteArray operationsBytes;
        if (!readOperations(operationsArgument, operationsBytes, error)) {
            return fail(
                QStringLiteral("new"),
                QStringLiteral("operations-read-failed"),
                error,
                InputError,
                pretty);
        }
        QJsonParseError parseError;
        const auto document =
            QJsonDocument::fromJson(operationsBytes, &parseError);
        if (parseError.error != QJsonParseError::NoError
            || !document.isObject()) {
            return fail(
                QStringLiteral("new"),
                QStringLiteral("operations-invalid"),
                parseError.error == QJsonParseError::NoError
                    ? QStringLiteral(
                          "Operations document must be a JSON object.")
                    : parseError.errorString(),
                InputError,
                pretty);
        }
        auto applied = wave::applyAutomationBatch(
            project,
            document.object(),
            project.scenarios.front().id);
        if (!applied.ok()) {
            return fail(
                QStringLiteral("new"),
                QStringLiteral("operation-rejected"),
                applied.error,
                Rejected,
                pretty,
                applied.failedOperation >= 0
                    ? std::optional<int>{applied.failedOperation}
                    : std::nullopt);
        }
        project = std::move(*applied.project);
        operationResults =
            applied.json.value(QStringLiteral("operations")).toArray();
        changes = applied.json.value(QStringLiteral("changes")).toObject();
        changes.insert(QStringLiteral("projectCreated"), true);
        changes.insert(QStringLiteral("scenarioCreated"), true);
        operationCount =
            applied.json.value(QStringLiteral("operationCount")).toInt();
    }

    const auto validation = wave::validateProjectForAutomation(
        project, project.scenarios.front().id);
    const auto inspection = wave::inspectProjectForAutomation(
        project,
        project.scenarios.front().id,
        wave::AutomationInspectDetail::Summary);
    if (!validation.ok() || !inspection.ok()) {
        return fail(
            QStringLiteral("new"),
            QStringLiteral("project-invalid"),
            !validation.ok() ? validation.error : inspection.error,
            Rejected,
            pretty);
    }
    const auto resultBytes = wave::serializeProject(project);
    const auto resultHash = sha256(resultBytes);
    auto written = false;
    if (!dryRun) {
        if (QFileInfo::exists(outputPath)) {
            return fail(
                QStringLiteral("new"),
                QStringLiteral("output-exists"),
                QStringLiteral(
                    "New project output appeared before save; no file was replaced."),
                Rejected,
                pretty);
        }
        if (!wave::saveProjectFileAtomic(project, outputPath, &error)) {
            return fail(
                QStringLiteral("new"),
                QStringLiteral("write-failed"),
                error,
                OutputError,
                pretty);
        }
        written = true;
    }

    auto report = created.json;
    report.insert(
        QStringLiteral("project"),
        inspection.json.value(QStringLiteral("project")));
    report.insert(QStringLiteral("operationCount"), operationCount);
    report.insert(QStringLiteral("operations"), operationResults);
    report.insert(QStringLiteral("changes"), changes);
    report.insert(QStringLiteral("validation"), validation.json);
    report.insert(QStringLiteral("resultSha256"), resultHash);
    report.insert(QStringLiteral("dryRun"), dryRun);
    report.insert(QStringLiteral("written"), written);
    report.insert(QStringLiteral("outputPath"), outputPath);
    QTextStream stream(stdout);
    writeJson(stream, report, pretty);
    return Success;
}

int runInspect(const QStringList& arguments)
{
    CommonOptions options;
    bool summary = false;
    QString projectArgument;
    QString error;
    for (const auto& argument : arguments) {
        if (argument == QStringLiteral("--summary")) {
            summary = true;
        } else if (argument.startsWith(QLatin1Char('-'))) {
            if (!parseCommonOption(argument, options, error)) {
                if (error.isEmpty()) {
                    error = QStringLiteral("Unknown option: %1").arg(argument);
                }
                return fail(
                    QStringLiteral("inspect"),
                    QStringLiteral("usage"),
                    error,
                    UsageError,
                    options.pretty);
            }
        } else if (projectArgument.isEmpty()) {
            projectArgument = argument;
        } else {
            return fail(
                QStringLiteral("inspect"),
                QStringLiteral("usage"),
                QStringLiteral("inspect accepts one project path."),
                UsageError,
                options.pretty);
        }
    }
    if (projectArgument.isEmpty()) {
        return fail(
            QStringLiteral("inspect"),
            QStringLiteral("usage"),
            QStringLiteral("Missing project path."),
            UsageError,
            options.pretty);
    }

    const auto projectPath = QFileInfo(projectArgument).absoluteFilePath();
    QByteArray sourceBytes;
    if (!readFile(projectPath, sourceBytes, error)) {
        return fail(
            QStringLiteral("inspect"),
            QStringLiteral("read-failed"),
            error,
            InputError,
            options.pretty);
    }
    const auto loaded = wave::loadProjectFile(projectPath);
    if (!loaded.ok()) {
        return fail(
            QStringLiteral("inspect"),
            QStringLiteral("project-invalid"),
            loaded.error,
            InputError,
            options.pretty);
    }
    auto report = wave::inspectProjectForAutomation(
        *loaded.project,
        options.scenarioId,
        summary
            ? wave::AutomationInspectDetail::Summary
            : wave::AutomationInspectDetail::Full);
    if (!report.ok()) {
        return fail(
            QStringLiteral("inspect"),
            QStringLiteral("scenario-not-found"),
            report.error,
            InputError,
            options.pretty);
    }
    report.json.insert(QStringLiteral("sourcePath"), projectPath);
    report.json.insert(QStringLiteral("sourceSha256"), sha256(sourceBytes));
    report.json.insert(
        QStringLiteral("loadWarnings"), warningsArray(loaded.warnings));
    QTextStream stream(stdout);
    writeJson(stream, report.json, options.pretty);
    return Success;
}

int runSignals(const QStringList& arguments)
{
    CommonOptions common;
    wave::AutomationSignalQueryOptions query;
    QString projectArgument;
    QString matchArgument;
    QString kindArgument;
    QString limitArgument;
    auto exact = false;
    auto matchCount = 0;
    auto kindCount = 0;
    auto limitCount = 0;
    QString error;
    for (const auto& argument : arguments) {
        if (argument == QStringLiteral("--exact")) {
            exact = true;
        } else if (argument.startsWith(QStringLiteral("--match="))) {
            ++matchCount;
            matchArgument =
                argument.mid(QStringLiteral("--match=").size()).trimmed();
        } else if (argument.startsWith(QStringLiteral("--kind="))) {
            ++kindCount;
            kindArgument =
                argument.mid(QStringLiteral("--kind=").size()).trimmed();
        } else if (argument.startsWith(QStringLiteral("--limit="))) {
            ++limitCount;
            limitArgument =
                argument.mid(QStringLiteral("--limit=").size()).trimmed();
        } else if (argument.startsWith(QLatin1Char('-'))) {
            if (!parseCommonOption(argument, common, error)) {
                if (error.isEmpty()) {
                    error = QStringLiteral("Unknown option: %1").arg(argument);
                }
                return fail(
                    QStringLiteral("signals"),
                    QStringLiteral("usage"),
                    error,
                    UsageError,
                    common.pretty);
            }
        } else if (projectArgument.isEmpty()) {
            projectArgument = argument;
        } else {
            return fail(
                QStringLiteral("signals"),
                QStringLiteral("usage"),
                QStringLiteral("signals accepts one project path."),
                UsageError,
                common.pretty);
        }
    }
    if (projectArgument.isEmpty()) {
        return fail(
            QStringLiteral("signals"),
            QStringLiteral("usage"),
            QStringLiteral("signals requires a project path."),
            UsageError,
            common.pretty);
    }
    if (matchCount > 1 || (matchCount == 1 && matchArgument.isEmpty())) {
        return fail(
            QStringLiteral("signals"),
            QStringLiteral("usage"),
            matchCount > 1
                ? QStringLiteral("--match may be provided only once.")
                : QStringLiteral("--match requires non-empty text."),
            UsageError,
            common.pretty);
    }
    if (exact && matchCount == 0) {
        return fail(
            QStringLiteral("signals"),
            QStringLiteral("usage"),
            QStringLiteral("--exact requires --match."),
            UsageError,
            common.pretty);
    }
    if (kindCount > 1 || (kindCount == 1 && kindArgument.isEmpty())) {
        return fail(
            QStringLiteral("signals"),
            QStringLiteral("usage"),
            kindCount > 1
                ? QStringLiteral("--kind may be provided only once.")
                : QStringLiteral("--kind requires a non-empty value."),
            UsageError,
            common.pretty);
    }
    if (limitCount > 1 || (limitCount == 1 && limitArgument.isEmpty())) {
        return fail(
            QStringLiteral("signals"),
            QStringLiteral("usage"),
            limitCount > 1
                ? QStringLiteral("--limit may be provided only once.")
                : QStringLiteral("--limit requires an integer."),
            UsageError,
            common.pretty);
    }
    if (!kindArgument.isEmpty()) {
        query.kind = wave::laneKindFromString(
            kindArgument.toLower().toStdString());
        if (!query.kind) {
            return fail(
                QStringLiteral("signals"),
                QStringLiteral("usage"),
                QStringLiteral("--kind is not a recognized Lane kind."),
                UsageError,
                common.pretty);
        }
    }
    if (!limitArgument.isEmpty()) {
        bool valid = false;
        const auto limit = limitArgument.toULongLong(&valid);
        if (!valid || limit == 0 || limit > 1'000) {
            return fail(
                QStringLiteral("signals"),
                QStringLiteral("usage"),
                QStringLiteral("--limit must be from 1 to 1000."),
                UsageError,
                common.pretty);
        }
        query.limit = static_cast<std::size_t>(limit);
    }
    query.match = matchArgument.toStdString();
    query.exact = exact;

    const auto projectPath = QFileInfo(projectArgument).absoluteFilePath();
    QByteArray sourceBytes;
    if (!readFile(projectPath, sourceBytes, error)) {
        return fail(
            QStringLiteral("signals"),
            QStringLiteral("read-failed"),
            error,
            InputError,
            common.pretty);
    }
    const auto loaded = wave::loadProjectFile(projectPath);
    if (!loaded.ok()) {
        return fail(
            QStringLiteral("signals"),
            QStringLiteral("project-invalid"),
            loaded.error,
            InputError,
            common.pretty);
    }
    auto report = wave::findSignalsForAutomation(
        *loaded.project, query, common.scenarioId);
    if (!report.ok()) {
        return fail(
            QStringLiteral("signals"),
            QStringLiteral("signal-query-rejected"),
            report.error,
            Rejected,
            common.pretty);
    }
    report.json.insert(QStringLiteral("sourcePath"), projectPath);
    report.json.insert(QStringLiteral("sourceSha256"), sha256(sourceBytes));
    report.json.insert(
        QStringLiteral("loadWarnings"), warningsArray(loaded.warnings));
    QTextStream stream(stdout);
    writeJson(stream, report.json, common.pretty);
    return Success;
}

template<typename Item>
std::optional<std::string> resolveCliNamedSelector(
    const std::vector<Item>& items,
    const std::string& selector,
    const QString& kind,
    QString& error)
{
    const auto id = std::find_if(
        items.begin(),
        items.end(),
        [&selector](const Item& item) {
            return item.id == selector;
        });
    if (id != items.end()) return id->id;

    const auto name = QString::fromStdString(selector);
    const Item* matched = nullptr;
    for (const auto& item : items) {
        if (QString::compare(
                QString::fromStdString(item.name),
                name,
                Qt::CaseInsensitive)
            != 0) {
            continue;
        }
        if (matched) {
            error = QStringLiteral(
                "%1 name '%2' is ambiguous; use a stable ID.")
                        .arg(kind, name);
            return std::nullopt;
        }
        matched = &item;
    }
    if (!matched) {
        error = QStringLiteral("%1 '%2' does not exist.").arg(kind, name);
        return std::nullopt;
    }
    return matched->id;
}

std::optional<std::string> resolveCliScenario(
    const wave::Project& project,
    const std::optional<std::string>& selector,
    QString& error)
{
    std::optional<QString> normalizedSelector;
    if (selector) {
        normalizedSelector =
            QString::fromStdString(*selector);
    }
    const auto index =
        wave::cli::resolveScenarioIndex(
            project,
            normalizedSelector,
            error);
    if (!index) return std::nullopt;
    return project.scenarios.at(*index).id;
}

bool resolveCliLaneSelectors(
    const wave::Scenario& scenario,
    const std::vector<std::string>& selectors,
    std::vector<std::string>& laneIds,
    QString& error)
{
    std::set<std::string> unique;
    laneIds.clear();
    laneIds.reserve(selectors.size());
    for (const auto& selector : selectors) {
        const auto id = resolveCliNamedSelector(
            scenario.lanes,
            selector,
            QStringLiteral("Lane"),
            error);
        if (!id) return false;
        if (!unique.insert(*id).second) {
            error = QStringLiteral(
                "Lane selectors resolve to duplicate Lane IDs.");
            return false;
        }
        laneIds.push_back(*id);
    }
    return true;
}

std::optional<std::string> resolveCliClock(
    const wave::Project& project,
    const QString& selector,
    QString& error)
{
    return resolveCliNamedSelector(
        project.clockDomains,
        selector.toStdString(),
        QStringLiteral("Clock domain"),
        error);
}

std::optional<std::string> inferClockFromLanes(
    const wave::Project& project,
    const std::optional<std::string>& scenarioId,
    const std::vector<std::string>& laneIds)
{
    const wave::Scenario* scenario = nullptr;
    if (scenarioId) {
        const auto iterator = std::find_if(
            project.scenarios.begin(),
            project.scenarios.end(),
            [&scenarioId](const wave::Scenario& candidate) {
                return candidate.id == *scenarioId;
            });
        if (iterator != project.scenarios.end()) scenario = &*iterator;
    } else if (project.scenarios.size() == 1) {
        scenario = &project.scenarios.front();
    }
    if (!scenario) return std::nullopt;

    std::set<std::string> clocks;
    for (const auto& laneId : laneIds) {
        const auto* lane = wave::findLane(*scenario, laneId);
        if (lane && !lane->clockDomainId.empty()) {
            clocks.insert(lane->clockDomainId);
        }
    }
    return clocks.size() == 1
        ? std::optional<std::string>{*clocks.begin()}
        : std::nullopt;
}

int runSample(const QStringList& arguments)
{
    CommonOptions options;
    QString projectArgument;
    QString atArgument;
    QString clockArgument;
    std::vector<std::string> laneIds;
    int atCount = 0;
    int clockCount = 0;
    QString error;
    for (const auto& argument : arguments) {
        if (argument.startsWith(QStringLiteral("--at="))) {
            ++atCount;
            atArgument = argument.mid(QStringLiteral("--at=").size()).trimmed();
        } else if (argument.startsWith(QStringLiteral("--clock="))) {
            ++clockCount;
            clockArgument =
                argument.mid(QStringLiteral("--clock=").size()).trimmed();
        } else if (argument.startsWith(QStringLiteral("--lane="))) {
            const auto lane =
                argument.mid(QStringLiteral("--lane=").size()).trimmed();
            if (lane.isEmpty()) {
                return fail(
                    QStringLiteral("sample"),
                    QStringLiteral("usage"),
                    QStringLiteral(
                        "--lane requires a non-empty ID or unique name."),
                    UsageError,
                    options.pretty);
            }
            laneIds.push_back(lane.toStdString());
        } else if (argument.startsWith(QLatin1Char('-'))) {
            if (!parseCommonOption(argument, options, error)) {
                if (error.isEmpty()) {
                    error = QStringLiteral("Unknown option: %1").arg(argument);
                }
                return fail(
                    QStringLiteral("sample"),
                    QStringLiteral("usage"),
                    error,
                    UsageError,
                    options.pretty);
            }
        } else if (projectArgument.isEmpty()) {
            projectArgument = argument;
        } else {
            return fail(
                QStringLiteral("sample"),
                QStringLiteral("usage"),
                QStringLiteral("sample accepts one project path."),
                UsageError,
                options.pretty);
        }
    }
    if (projectArgument.isEmpty() || atArgument.isEmpty()) {
        return fail(
            QStringLiteral("sample"),
            QStringLiteral("usage"),
            QStringLiteral("sample requires a project path and --at=TIME."),
            UsageError,
            options.pretty);
    }
    if (atCount > 1) {
        return fail(
            QStringLiteral("sample"),
            QStringLiteral("usage"),
            QStringLiteral("--at may be provided only once."),
            UsageError,
            options.pretty);
    }
    if (clockCount > 1 || (clockCount == 1 && clockArgument.isEmpty())) {
        return fail(
            QStringLiteral("sample"),
            QStringLiteral("usage"),
            clockCount > 1
                ? QStringLiteral("--clock may be provided only once.")
                : QStringLiteral(
                      "--clock requires a non-empty ID or unique name."),
            UsageError,
            options.pretty);
    }

    const auto projectPath = QFileInfo(projectArgument).absoluteFilePath();
    QByteArray sourceBytes;
    if (!readFile(projectPath, sourceBytes, error)) {
        return fail(
            QStringLiteral("sample"),
            QStringLiteral("read-failed"),
            error,
            InputError,
            options.pretty);
    }
    const auto loaded = wave::loadProjectFile(projectPath);
    if (!loaded.ok()) {
        return fail(
            QStringLiteral("sample"),
            QStringLiteral("project-invalid"),
            loaded.error,
            InputError,
            options.pretty);
    }

    const auto scenarioId =
        resolveCliScenario(*loaded.project, options.scenarioId, error);
    if (!scenarioId) {
        return fail(
            QStringLiteral("sample"),
            QStringLiteral("selector-invalid"),
            error,
            Rejected,
            options.pretty);
    }
    const auto scenario = std::find_if(
        loaded.project->scenarios.begin(),
        loaded.project->scenarios.end(),
        [&scenarioId](const wave::Scenario& candidate) {
            return candidate.id == *scenarioId;
        });
    std::vector<std::string> resolvedLaneIds;
    if (scenario == loaded.project->scenarios.end()
        || !resolveCliLaneSelectors(
            *scenario, laneIds, resolvedLaneIds, error)) {
        return fail(
            QStringLiteral("sample"),
            QStringLiteral("selector-invalid"),
            error.isEmpty()
                ? QStringLiteral("Scenario selection failed.")
                : error,
            Rejected,
            options.pretty);
    }
    std::optional<std::string> clockId;
    if (!clockArgument.isEmpty()) {
        clockId = resolveCliClock(
            *loaded.project, clockArgument, error);
        if (!clockId) {
            return fail(
                QStringLiteral("sample"),
                QStringLiteral("selector-invalid"),
                error,
                Rejected,
                options.pretty);
        }
    } else if (!resolvedLaneIds.empty()) {
        clockId = inferClockFromLanes(
            *loaded.project, scenarioId, resolvedLaneIds);
    }
    const auto parsedTime =
        wave::parseAutomationTime(*loaded.project, atArgument, clockId);
    if (!parsedTime.ok()) {
        return fail(
            QStringLiteral("sample"),
            QStringLiteral("time-invalid"),
            parsedTime.error,
            Rejected,
            options.pretty);
    }
    auto report = wave::sampleProjectForAutomation(
        *loaded.project,
        *parsedTime.tick,
        scenarioId,
        resolvedLaneIds);
    if (!report.ok()) {
        return fail(
            QStringLiteral("sample"),
            QStringLiteral("sample-rejected"),
            report.error,
            Rejected,
            options.pretty);
    }
    report.json.insert(QStringLiteral("atInput"), atArgument);
    if (clockId) {
        report.json.insert(
            QStringLiteral("clockId"),
            QString::fromStdString(*clockId));
    }
    report.json.insert(QStringLiteral("sourcePath"), projectPath);
    report.json.insert(QStringLiteral("sourceSha256"), sha256(sourceBytes));
    report.json.insert(
        QStringLiteral("loadWarnings"), warningsArray(loaded.warnings));
    QTextStream stream(stdout);
    writeJson(stream, report.json, options.pretty);
    return Success;
}

int runWindow(const QStringList& arguments)
{
    CommonOptions options;
    QString projectArgument;
    QString startArgument;
    QString endArgument;
    QString clockArgument;
    std::vector<std::string> laneIds;
    int startCount = 0;
    int endCount = 0;
    int clockCount = 0;
    QString error;
    for (const auto& argument : arguments) {
        if (argument.startsWith(QStringLiteral("--start="))) {
            ++startCount;
            startArgument =
                argument.mid(QStringLiteral("--start=").size()).trimmed();
        } else if (argument.startsWith(QStringLiteral("--end="))) {
            ++endCount;
            endArgument =
                argument.mid(QStringLiteral("--end=").size()).trimmed();
        } else if (argument.startsWith(QStringLiteral("--clock="))) {
            ++clockCount;
            clockArgument =
                argument.mid(QStringLiteral("--clock=").size()).trimmed();
        } else if (argument.startsWith(QStringLiteral("--lane="))) {
            const auto lane =
                argument.mid(QStringLiteral("--lane=").size()).trimmed();
            if (lane.isEmpty()) {
                return fail(
                    QStringLiteral("window"),
                    QStringLiteral("usage"),
                    QStringLiteral(
                        "--lane requires a non-empty ID or unique name."),
                    UsageError,
                    options.pretty);
            }
            laneIds.push_back(lane.toStdString());
        } else if (argument.startsWith(QLatin1Char('-'))) {
            if (!parseCommonOption(argument, options, error)) {
                if (error.isEmpty()) {
                    error = QStringLiteral("Unknown option: %1").arg(argument);
                }
                return fail(
                    QStringLiteral("window"),
                    QStringLiteral("usage"),
                    error,
                    UsageError,
                    options.pretty);
            }
        } else if (projectArgument.isEmpty()) {
            projectArgument = argument;
        } else {
            return fail(
                QStringLiteral("window"),
                QStringLiteral("usage"),
                QStringLiteral("window accepts one project path."),
                UsageError,
                options.pretty);
        }
    }
    if (projectArgument.isEmpty()
        || startArgument.isEmpty()
        || endArgument.isEmpty()) {
        return fail(
            QStringLiteral("window"),
            QStringLiteral("usage"),
            QStringLiteral(
                "window requires a project path, --start=TIME, and --end=TIME."),
            UsageError,
            options.pretty);
    }
    if (startCount != 1 || endCount != 1) {
        return fail(
            QStringLiteral("window"),
            QStringLiteral("usage"),
            QStringLiteral(
                "--start and --end must each be provided exactly once."),
            UsageError,
            options.pretty);
    }
    if (clockCount > 1 || (clockCount == 1 && clockArgument.isEmpty())) {
        return fail(
            QStringLiteral("window"),
            QStringLiteral("usage"),
            clockCount > 1
                ? QStringLiteral("--clock may be provided only once.")
                : QStringLiteral(
                      "--clock requires a non-empty ID or unique name."),
            UsageError,
            options.pretty);
    }

    const auto projectPath = QFileInfo(projectArgument).absoluteFilePath();
    QByteArray sourceBytes;
    if (!readFile(projectPath, sourceBytes, error)) {
        return fail(
            QStringLiteral("window"),
            QStringLiteral("read-failed"),
            error,
            InputError,
            options.pretty);
    }
    const auto loaded = wave::loadProjectFile(projectPath);
    if (!loaded.ok()) {
        return fail(
            QStringLiteral("window"),
            QStringLiteral("project-invalid"),
            loaded.error,
            InputError,
            options.pretty);
    }

    const auto scenarioId =
        resolveCliScenario(*loaded.project, options.scenarioId, error);
    if (!scenarioId) {
        return fail(
            QStringLiteral("window"),
            QStringLiteral("selector-invalid"),
            error,
            Rejected,
            options.pretty);
    }
    const auto scenario = std::find_if(
        loaded.project->scenarios.begin(),
        loaded.project->scenarios.end(),
        [&scenarioId](const wave::Scenario& candidate) {
            return candidate.id == *scenarioId;
        });
    std::vector<std::string> resolvedLaneIds;
    if (scenario == loaded.project->scenarios.end()
        || !resolveCliLaneSelectors(
            *scenario, laneIds, resolvedLaneIds, error)) {
        return fail(
            QStringLiteral("window"),
            QStringLiteral("selector-invalid"),
            error.isEmpty()
                ? QStringLiteral("Scenario selection failed.")
                : error,
            Rejected,
            options.pretty);
    }
    std::optional<std::string> clockId;
    if (!clockArgument.isEmpty()) {
        clockId = resolveCliClock(
            *loaded.project, clockArgument, error);
        if (!clockId) {
            return fail(
                QStringLiteral("window"),
                QStringLiteral("selector-invalid"),
                error,
                Rejected,
                options.pretty);
        }
    } else if (!resolvedLaneIds.empty()) {
        clockId = inferClockFromLanes(
            *loaded.project, scenarioId, resolvedLaneIds);
    }
    const auto parsedStart =
        wave::parseAutomationTime(*loaded.project, startArgument, clockId);
    if (!parsedStart.ok()) {
        return fail(
            QStringLiteral("window"),
            QStringLiteral("time-invalid"),
            QStringLiteral("--start: %1").arg(parsedStart.error),
            Rejected,
            options.pretty);
    }
    const auto parsedEnd =
        wave::parseAutomationTime(*loaded.project, endArgument, clockId);
    if (!parsedEnd.ok()) {
        return fail(
            QStringLiteral("window"),
            QStringLiteral("time-invalid"),
            QStringLiteral("--end: %1").arg(parsedEnd.error),
            Rejected,
            options.pretty);
    }
    auto report = wave::inspectProjectWindowForAutomation(
        *loaded.project,
        *parsedStart.tick,
        *parsedEnd.tick,
        scenarioId,
        resolvedLaneIds);
    if (!report.ok()) {
        return fail(
            QStringLiteral("window"),
            QStringLiteral("window-rejected"),
            report.error,
            Rejected,
            options.pretty);
    }
    report.json.insert(QStringLiteral("startInput"), startArgument);
    report.json.insert(QStringLiteral("endInput"), endArgument);
    if (clockId) {
        report.json.insert(
            QStringLiteral("clockId"),
            QString::fromStdString(*clockId));
    }
    report.json.insert(QStringLiteral("sourcePath"), projectPath);
    report.json.insert(QStringLiteral("sourceSha256"), sha256(sourceBytes));
    report.json.insert(
        QStringLiteral("loadWarnings"), warningsArray(loaded.warnings));
    QTextStream stream(stdout);
    writeJson(stream, report.json, options.pretty);
    return Success;
}

int runEdges(const QStringList& arguments)
{
    CommonOptions options;
    wave::AutomationEdgeQueryOptions query;
    QString projectArgument;
    QString startArgument;
    QString endArgument;
    QString edgeArgument;
    QString limitArgument;
    QString clockArgument;
    std::vector<std::string> laneIds;
    int startCount = 0;
    int endCount = 0;
    int edgeCount = 0;
    int limitCount = 0;
    int clockCount = 0;
    QString error;
    for (const auto& argument : arguments) {
        if (argument.startsWith(QStringLiteral("--start="))) {
            ++startCount;
            startArgument =
                argument.mid(QStringLiteral("--start=").size()).trimmed();
        } else if (argument.startsWith(QStringLiteral("--end="))) {
            ++endCount;
            endArgument =
                argument.mid(QStringLiteral("--end=").size()).trimmed();
        } else if (argument.startsWith(QStringLiteral("--edge="))) {
            ++edgeCount;
            edgeArgument =
                argument.mid(QStringLiteral("--edge=").size()).trimmed();
        } else if (argument.startsWith(QStringLiteral("--limit="))) {
            ++limitCount;
            limitArgument =
                argument.mid(QStringLiteral("--limit=").size()).trimmed();
        } else if (argument.startsWith(QStringLiteral("--clock="))) {
            ++clockCount;
            clockArgument =
                argument.mid(QStringLiteral("--clock=").size()).trimmed();
        } else if (argument.startsWith(QStringLiteral("--lane="))) {
            const auto lane =
                argument.mid(QStringLiteral("--lane=").size()).trimmed();
            if (lane.isEmpty()) {
                return fail(
                    QStringLiteral("edges"),
                    QStringLiteral("usage"),
                    QStringLiteral(
                        "--lane requires a non-empty ID or unique name."),
                    UsageError,
                    options.pretty);
            }
            laneIds.push_back(lane.toStdString());
        } else if (argument.startsWith(QLatin1Char('-'))) {
            if (!parseCommonOption(argument, options, error)) {
                if (error.isEmpty()) {
                    error = QStringLiteral("Unknown option: %1").arg(argument);
                }
                return fail(
                    QStringLiteral("edges"),
                    QStringLiteral("usage"),
                    error,
                    UsageError,
                    options.pretty);
            }
        } else if (projectArgument.isEmpty()) {
            projectArgument = argument;
        } else {
            return fail(
                QStringLiteral("edges"),
                QStringLiteral("usage"),
                QStringLiteral("edges accepts one project path."),
                UsageError,
                options.pretty);
        }
    }
    if (projectArgument.isEmpty()) {
        return fail(
            QStringLiteral("edges"),
            QStringLiteral("usage"),
            QStringLiteral("edges requires a project path."),
            UsageError,
            options.pretty);
    }
    const auto invalidOptional =
        [](const int count, const QString& value) {
            return count > 1 || (count == 1 && value.isEmpty());
        };
    if (invalidOptional(startCount, startArgument)
        || invalidOptional(endCount, endArgument)) {
        return fail(
            QStringLiteral("edges"),
            QStringLiteral("usage"),
            QStringLiteral(
                "--start and --end may each be provided once with a time."),
            UsageError,
            options.pretty);
    }
    if (invalidOptional(edgeCount, edgeArgument)) {
        return fail(
            QStringLiteral("edges"),
            QStringLiteral("usage"),
            QStringLiteral(
                "--edge may be provided once with initial, rising, falling, or change."),
            UsageError,
            options.pretty);
    }
    if (invalidOptional(limitCount, limitArgument)) {
        return fail(
            QStringLiteral("edges"),
            QStringLiteral("usage"),
            QStringLiteral("--limit may be provided once with an integer."),
            UsageError,
            options.pretty);
    }
    if (invalidOptional(clockCount, clockArgument)) {
        return fail(
            QStringLiteral("edges"),
            QStringLiteral("usage"),
            QStringLiteral(
                "--clock may be provided once with an ID or unique name."),
            UsageError,
            options.pretty);
    }

    if (!edgeArgument.isEmpty()) {
        const auto normalized = edgeArgument.toLower();
        if (normalized == QStringLiteral("initial")) {
            query.edge = wave::AutomationEdgeKind::Initial;
        } else if (normalized == QStringLiteral("rising")) {
            query.edge = wave::AutomationEdgeKind::Rising;
        } else if (normalized == QStringLiteral("falling")) {
            query.edge = wave::AutomationEdgeKind::Falling;
        } else if (normalized == QStringLiteral("change")) {
            query.edge = wave::AutomationEdgeKind::Change;
        } else {
            return fail(
                QStringLiteral("edges"),
                QStringLiteral("usage"),
                QStringLiteral(
                    "--edge must be initial, rising, falling, or change."),
                UsageError,
                options.pretty);
        }
    }
    if (!limitArgument.isEmpty()) {
        bool valid = false;
        const auto limit = limitArgument.toULongLong(&valid);
        if (!valid || limit == 0 || limit > 10'000) {
            return fail(
                QStringLiteral("edges"),
                QStringLiteral("usage"),
                QStringLiteral("--limit must be from 1 to 10000."),
                UsageError,
                options.pretty);
        }
        query.limit = static_cast<std::size_t>(limit);
    }

    const auto projectPath = QFileInfo(projectArgument).absoluteFilePath();
    QByteArray sourceBytes;
    if (!readFile(projectPath, sourceBytes, error)) {
        return fail(
            QStringLiteral("edges"),
            QStringLiteral("read-failed"),
            error,
            InputError,
            options.pretty);
    }
    const auto loaded = wave::loadProjectFile(projectPath);
    if (!loaded.ok()) {
        return fail(
            QStringLiteral("edges"),
            QStringLiteral("project-invalid"),
            loaded.error,
            InputError,
            options.pretty);
    }

    const auto scenarioId =
        resolveCliScenario(*loaded.project, options.scenarioId, error);
    if (!scenarioId) {
        return fail(
            QStringLiteral("edges"),
            QStringLiteral("selector-invalid"),
            error,
            Rejected,
            options.pretty);
    }
    const auto scenario = std::find_if(
        loaded.project->scenarios.begin(),
        loaded.project->scenarios.end(),
        [&scenarioId](const wave::Scenario& candidate) {
            return candidate.id == *scenarioId;
        });
    std::vector<std::string> resolvedLaneIds;
    if (scenario == loaded.project->scenarios.end()
        || !resolveCliLaneSelectors(
            *scenario, laneIds, resolvedLaneIds, error)) {
        return fail(
            QStringLiteral("edges"),
            QStringLiteral("selector-invalid"),
            error.isEmpty()
                ? QStringLiteral("Scenario selection failed.")
                : error,
            Rejected,
            options.pretty);
    }

    std::optional<std::string> clockId;
    if (!clockArgument.isEmpty()) {
        clockId = resolveCliClock(
            *loaded.project, clockArgument, error);
        if (!clockId) {
            return fail(
                QStringLiteral("edges"),
                QStringLiteral("selector-invalid"),
                error,
                Rejected,
                options.pretty);
        }
    } else if (!resolvedLaneIds.empty()) {
        clockId = inferClockFromLanes(
            *loaded.project, scenarioId, resolvedLaneIds);
    }
    if (!startArgument.isEmpty()) {
        const auto parsed =
            wave::parseAutomationTime(*loaded.project, startArgument, clockId);
        if (!parsed.ok()) {
            return fail(
                QStringLiteral("edges"),
                QStringLiteral("time-invalid"),
                QStringLiteral("--start: %1").arg(parsed.error),
                Rejected,
                options.pretty);
        }
        query.start = *parsed.tick;
    }
    if (!endArgument.isEmpty()) {
        const auto parsed =
            wave::parseAutomationTime(*loaded.project, endArgument, clockId);
        if (!parsed.ok()) {
            return fail(
                QStringLiteral("edges"),
                QStringLiteral("time-invalid"),
                QStringLiteral("--end: %1").arg(parsed.error),
                Rejected,
                options.pretty);
        }
        query.end = *parsed.tick;
    }

    auto report = wave::findWaveformEdgesForAutomation(
        *loaded.project,
        query,
        scenarioId,
        resolvedLaneIds);
    if (!report.ok()) {
        return fail(
            QStringLiteral("edges"),
            QStringLiteral("edge-query-rejected"),
            report.error,
            Rejected,
            options.pretty);
    }
    report.json.insert(
        QStringLiteral("startInput"),
        startArgument.isEmpty()
            ? QJsonValue{QJsonValue::Null}
            : QJsonValue{startArgument});
    report.json.insert(
        QStringLiteral("endInput"),
        endArgument.isEmpty()
            ? QJsonValue{QJsonValue::Null}
            : QJsonValue{endArgument});
    if (clockId) {
        report.json.insert(
            QStringLiteral("clockId"),
            QString::fromStdString(*clockId));
    }
    report.json.insert(QStringLiteral("sourcePath"), projectPath);
    report.json.insert(QStringLiteral("sourceSha256"), sha256(sourceBytes));
    report.json.insert(
        QStringLiteral("loadWarnings"), warningsArray(loaded.warnings));
    QTextStream stream(stdout);
    writeJson(stream, report.json, options.pretty);
    return Success;
}

int runMarkers(const QStringList& arguments)
{
    CommonOptions options;
    wave::AutomationMarkerQueryOptions query;
    QString projectArgument;
    QString matchArgument;
    QString kindArgument;
    QString startArgument;
    QString endArgument;
    QString limitArgument;
    QString clockArgument;
    int matchCount = 0;
    int exactCount = 0;
    int kindCount = 0;
    int startCount = 0;
    int endCount = 0;
    int limitCount = 0;
    int clockCount = 0;
    QString error;
    for (const auto& argument : arguments) {
        if (argument.startsWith(QStringLiteral("--match="))) {
            ++matchCount;
            matchArgument =
                argument.mid(QStringLiteral("--match=").size()).trimmed();
        } else if (argument == QStringLiteral("--exact")) {
            ++exactCount;
            query.exact = true;
        } else if (argument.startsWith(QStringLiteral("--kind="))) {
            ++kindCount;
            kindArgument =
                argument.mid(QStringLiteral("--kind=").size()).trimmed();
        } else if (argument.startsWith(QStringLiteral("--start="))) {
            ++startCount;
            startArgument =
                argument.mid(QStringLiteral("--start=").size()).trimmed();
        } else if (argument.startsWith(QStringLiteral("--end="))) {
            ++endCount;
            endArgument =
                argument.mid(QStringLiteral("--end=").size()).trimmed();
        } else if (argument.startsWith(QStringLiteral("--limit="))) {
            ++limitCount;
            limitArgument =
                argument.mid(QStringLiteral("--limit=").size()).trimmed();
        } else if (argument.startsWith(QStringLiteral("--clock="))) {
            ++clockCount;
            clockArgument =
                argument.mid(QStringLiteral("--clock=").size()).trimmed();
        } else if (argument.startsWith(QLatin1Char('-'))) {
            if (!parseCommonOption(argument, options, error)) {
                if (error.isEmpty()) {
                    error = QStringLiteral("Unknown option: %1").arg(argument);
                }
                return fail(
                    QStringLiteral("markers"),
                    QStringLiteral("usage"),
                    error,
                    UsageError,
                    options.pretty);
            }
        } else if (projectArgument.isEmpty()) {
            projectArgument = argument;
        } else {
            return fail(
                QStringLiteral("markers"),
                QStringLiteral("usage"),
                QStringLiteral("markers accepts one project path."),
                UsageError,
                options.pretty);
        }
    }
    if (projectArgument.isEmpty()) {
        return fail(
            QStringLiteral("markers"),
            QStringLiteral("usage"),
            QStringLiteral("markers requires a project path."),
            UsageError,
            options.pretty);
    }
    const auto invalidOptional =
        [](const int count, const QString& value) {
            return count > 1 || (count == 1 && value.isEmpty());
        };
    if (invalidOptional(matchCount, matchArgument)) {
        return fail(
            QStringLiteral("markers"),
            QStringLiteral("usage"),
            QStringLiteral("--match may be provided once with non-empty text."),
            UsageError,
            options.pretty);
    }
    if (exactCount > 1 || (query.exact && matchArgument.isEmpty())) {
        return fail(
            QStringLiteral("markers"),
            QStringLiteral("usage"),
            exactCount > 1
                ? QStringLiteral("--exact may be provided only once.")
                : QStringLiteral("--exact requires --match."),
            UsageError,
            options.pretty);
    }
    if (invalidOptional(kindCount, kindArgument)) {
        return fail(
            QStringLiteral("markers"),
            QStringLiteral("usage"),
            QStringLiteral(
                "--kind may be provided once with point, interval, phase, error, or note."),
            UsageError,
            options.pretty);
    }
    if (invalidOptional(startCount, startArgument)
        || invalidOptional(endCount, endArgument)) {
        return fail(
            QStringLiteral("markers"),
            QStringLiteral("usage"),
            QStringLiteral(
                "--start and --end may each be provided once with a time."),
            UsageError,
            options.pretty);
    }
    if (invalidOptional(limitCount, limitArgument)) {
        return fail(
            QStringLiteral("markers"),
            QStringLiteral("usage"),
            QStringLiteral("--limit may be provided once with an integer."),
            UsageError,
            options.pretty);
    }
    if (invalidOptional(clockCount, clockArgument)) {
        return fail(
            QStringLiteral("markers"),
            QStringLiteral("usage"),
            QStringLiteral(
                "--clock may be provided once with an ID or unique name."),
            UsageError,
            options.pretty);
    }

    if (!kindArgument.isEmpty()) {
        query.kind = wave::markerKindFromString(
            kindArgument.toLower().toStdString());
        if (!query.kind) {
            return fail(
                QStringLiteral("markers"),
                QStringLiteral("usage"),
                QStringLiteral(
                    "--kind must be point, interval, phase, error, or note."),
                UsageError,
                options.pretty);
        }
    }
    if (!limitArgument.isEmpty()) {
        bool valid = false;
        const auto limit = limitArgument.toULongLong(&valid);
        if (!valid || limit == 0 || limit > 1'000) {
            return fail(
                QStringLiteral("markers"),
                QStringLiteral("usage"),
                QStringLiteral("--limit must be from 1 to 1000."),
                UsageError,
                options.pretty);
        }
        query.limit = static_cast<std::size_t>(limit);
    }
    query.match = matchArgument.toStdString();

    const auto projectPath = QFileInfo(projectArgument).absoluteFilePath();
    QByteArray sourceBytes;
    if (!readFile(projectPath, sourceBytes, error)) {
        return fail(
            QStringLiteral("markers"),
            QStringLiteral("read-failed"),
            error,
            InputError,
            options.pretty);
    }
    const auto loaded = wave::loadProjectFile(projectPath);
    if (!loaded.ok()) {
        return fail(
            QStringLiteral("markers"),
            QStringLiteral("project-invalid"),
            loaded.error,
            InputError,
            options.pretty);
    }

    const auto scenarioId =
        resolveCliScenario(*loaded.project, options.scenarioId, error);
    if (!scenarioId) {
        return fail(
            QStringLiteral("markers"),
            QStringLiteral("selector-invalid"),
            error,
            Rejected,
            options.pretty);
    }

    std::optional<std::string> clockId;
    if (!clockArgument.isEmpty()) {
        clockId = resolveCliClock(
            *loaded.project, clockArgument, error);
        if (!clockId) {
            return fail(
                QStringLiteral("markers"),
                QStringLiteral("selector-invalid"),
                error,
                Rejected,
                options.pretty);
        }
    } else {
        const auto usesCycleTime =
            [](const QString& value) {
                return value.trimmed().startsWith(
                    QStringLiteral("cycle"),
                    Qt::CaseInsensitive);
            };
        if ((usesCycleTime(startArgument)
             || usesCycleTime(endArgument))
            && loaded.project->clockDomains.size() == 1) {
            clockId = loaded.project->clockDomains.front().id;
        }
    }
    if (!startArgument.isEmpty()) {
        const auto parsed =
            wave::parseAutomationTime(*loaded.project, startArgument, clockId);
        if (!parsed.ok()) {
            return fail(
                QStringLiteral("markers"),
                QStringLiteral("time-invalid"),
                QStringLiteral("--start: %1").arg(parsed.error),
                Rejected,
                options.pretty);
        }
        query.start = *parsed.tick;
    }
    if (!endArgument.isEmpty()) {
        const auto parsed =
            wave::parseAutomationTime(*loaded.project, endArgument, clockId);
        if (!parsed.ok()) {
            return fail(
                QStringLiteral("markers"),
                QStringLiteral("time-invalid"),
                QStringLiteral("--end: %1").arg(parsed.error),
                Rejected,
                options.pretty);
        }
        query.end = *parsed.tick;
    }

    auto report = wave::findMarkersForAutomation(
        *loaded.project,
        query,
        scenarioId);
    if (!report.ok()) {
        return fail(
            QStringLiteral("markers"),
            QStringLiteral("marker-query-rejected"),
            report.error,
            Rejected,
            options.pretty);
    }
    report.json.insert(
        QStringLiteral("startInput"),
        startArgument.isEmpty()
            ? QJsonValue{QJsonValue::Null}
            : QJsonValue{startArgument});
    report.json.insert(
        QStringLiteral("endInput"),
        endArgument.isEmpty()
            ? QJsonValue{QJsonValue::Null}
            : QJsonValue{endArgument});
    if (clockId) {
        report.json.insert(
            QStringLiteral("clockId"),
            QString::fromStdString(*clockId));
    }
    report.json.insert(QStringLiteral("sourcePath"), projectPath);
    report.json.insert(QStringLiteral("sourceSha256"), sha256(sourceBytes));
    report.json.insert(
        QStringLiteral("loadWarnings"), warningsArray(loaded.warnings));
    QTextStream stream(stdout);
    writeJson(stream, report.json, options.pretty);
    return Success;
}

int runRelations(const QStringList& arguments)
{
    CommonOptions options;
    wave::AutomationRelationQueryOptions query;
    QString projectArgument;
    QString matchArgument;
    QString severityArgument;
    QString startArgument;
    QString endArgument;
    QString limitArgument;
    QString clockArgument;
    std::vector<std::string> laneIds;
    int matchCount = 0;
    int exactCount = 0;
    int severityCount = 0;
    int startCount = 0;
    int endCount = 0;
    int limitCount = 0;
    int clockCount = 0;
    QString error;
    for (const auto& argument : arguments) {
        if (argument.startsWith(QStringLiteral("--match="))) {
            ++matchCount;
            matchArgument =
                argument.mid(QStringLiteral("--match=").size()).trimmed();
        } else if (argument == QStringLiteral("--exact")) {
            ++exactCount;
            query.exact = true;
        } else if (argument.startsWith(QStringLiteral("--severity="))) {
            ++severityCount;
            severityArgument =
                argument.mid(QStringLiteral("--severity=").size()).trimmed();
        } else if (argument.startsWith(QStringLiteral("--start="))) {
            ++startCount;
            startArgument =
                argument.mid(QStringLiteral("--start=").size()).trimmed();
        } else if (argument.startsWith(QStringLiteral("--end="))) {
            ++endCount;
            endArgument =
                argument.mid(QStringLiteral("--end=").size()).trimmed();
        } else if (argument.startsWith(QStringLiteral("--limit="))) {
            ++limitCount;
            limitArgument =
                argument.mid(QStringLiteral("--limit=").size()).trimmed();
        } else if (argument.startsWith(QStringLiteral("--clock="))) {
            ++clockCount;
            clockArgument =
                argument.mid(QStringLiteral("--clock=").size()).trimmed();
        } else if (argument.startsWith(QStringLiteral("--lane="))) {
            const auto lane =
                argument.mid(QStringLiteral("--lane=").size()).trimmed();
            if (lane.isEmpty()) {
                return fail(
                    QStringLiteral("relations"),
                    QStringLiteral("usage"),
                    QStringLiteral(
                        "--lane requires a non-empty ID or unique name."),
                    UsageError,
                    options.pretty);
            }
            laneIds.push_back(lane.toStdString());
        } else if (argument.startsWith(QLatin1Char('-'))) {
            if (!parseCommonOption(argument, options, error)) {
                if (error.isEmpty()) {
                    error = QStringLiteral("Unknown option: %1").arg(argument);
                }
                return fail(
                    QStringLiteral("relations"),
                    QStringLiteral("usage"),
                    error,
                    UsageError,
                    options.pretty);
            }
        } else if (projectArgument.isEmpty()) {
            projectArgument = argument;
        } else {
            return fail(
                QStringLiteral("relations"),
                QStringLiteral("usage"),
                QStringLiteral("relations accepts one project path."),
                UsageError,
                options.pretty);
        }
    }
    if (projectArgument.isEmpty()) {
        return fail(
            QStringLiteral("relations"),
            QStringLiteral("usage"),
            QStringLiteral("relations requires a project path."),
            UsageError,
            options.pretty);
    }
    const auto invalidOptional =
        [](const int count, const QString& value) {
            return count > 1 || (count == 1 && value.isEmpty());
        };
    if (invalidOptional(matchCount, matchArgument)) {
        return fail(
            QStringLiteral("relations"),
            QStringLiteral("usage"),
            QStringLiteral("--match may be provided once with non-empty text."),
            UsageError,
            options.pretty);
    }
    if (exactCount > 1 || (query.exact && matchArgument.isEmpty())) {
        return fail(
            QStringLiteral("relations"),
            QStringLiteral("usage"),
            exactCount > 1
                ? QStringLiteral("--exact may be provided only once.")
                : QStringLiteral("--exact requires --match."),
            UsageError,
            options.pretty);
    }
    if (invalidOptional(severityCount, severityArgument)) {
        return fail(
            QStringLiteral("relations"),
            QStringLiteral("usage"),
            QStringLiteral(
                "--severity may be provided once with information, warning, or error."),
            UsageError,
            options.pretty);
    }
    if (invalidOptional(startCount, startArgument)
        || invalidOptional(endCount, endArgument)) {
        return fail(
            QStringLiteral("relations"),
            QStringLiteral("usage"),
            QStringLiteral(
                "--start and --end may each be provided once with a time."),
            UsageError,
            options.pretty);
    }
    if (invalidOptional(limitCount, limitArgument)) {
        return fail(
            QStringLiteral("relations"),
            QStringLiteral("usage"),
            QStringLiteral("--limit may be provided once with an integer."),
            UsageError,
            options.pretty);
    }
    if (invalidOptional(clockCount, clockArgument)) {
        return fail(
            QStringLiteral("relations"),
            QStringLiteral("usage"),
            QStringLiteral(
                "--clock may be provided once with an ID or unique name."),
            UsageError,
            options.pretty);
    }

    if (!severityArgument.isEmpty()) {
        query.severity = wave::severityFromString(
            severityArgument.toLower().toStdString());
        if (!query.severity) {
            return fail(
                QStringLiteral("relations"),
                QStringLiteral("usage"),
                QStringLiteral(
                    "--severity must be information, warning, or error."),
                UsageError,
                options.pretty);
        }
    }
    if (!limitArgument.isEmpty()) {
        bool valid = false;
        const auto limit = limitArgument.toULongLong(&valid);
        if (!valid || limit == 0 || limit > 1'000) {
            return fail(
                QStringLiteral("relations"),
                QStringLiteral("usage"),
                QStringLiteral("--limit must be from 1 to 1000."),
                UsageError,
                options.pretty);
        }
        query.limit = static_cast<std::size_t>(limit);
    }
    query.match = matchArgument.toStdString();

    const auto projectPath = QFileInfo(projectArgument).absoluteFilePath();
    QByteArray sourceBytes;
    if (!readFile(projectPath, sourceBytes, error)) {
        return fail(
            QStringLiteral("relations"),
            QStringLiteral("read-failed"),
            error,
            InputError,
            options.pretty);
    }
    const auto loaded = wave::loadProjectFile(projectPath);
    if (!loaded.ok()) {
        return fail(
            QStringLiteral("relations"),
            QStringLiteral("project-invalid"),
            loaded.error,
            InputError,
            options.pretty);
    }

    const auto scenarioId =
        resolveCliScenario(*loaded.project, options.scenarioId, error);
    if (!scenarioId) {
        return fail(
            QStringLiteral("relations"),
            QStringLiteral("selector-invalid"),
            error,
            Rejected,
            options.pretty);
    }
    const auto scenario = std::find_if(
        loaded.project->scenarios.begin(),
        loaded.project->scenarios.end(),
        [&scenarioId](const wave::Scenario& candidate) {
            return candidate.id == *scenarioId;
        });
    std::vector<std::string> resolvedLaneIds;
    if (scenario == loaded.project->scenarios.end()
        || !resolveCliLaneSelectors(
            *scenario, laneIds, resolvedLaneIds, error)) {
        return fail(
            QStringLiteral("relations"),
            QStringLiteral("selector-invalid"),
            error.isEmpty()
                ? QStringLiteral("Scenario selection failed.")
                : error,
            Rejected,
            options.pretty);
    }

    std::optional<std::string> clockId;
    if (!clockArgument.isEmpty()) {
        clockId = resolveCliClock(
            *loaded.project, clockArgument, error);
        if (!clockId) {
            return fail(
                QStringLiteral("relations"),
                QStringLiteral("selector-invalid"),
                error,
                Rejected,
                options.pretty);
        }
    } else if (!resolvedLaneIds.empty()) {
        clockId = inferClockFromLanes(
            *loaded.project, scenarioId, resolvedLaneIds);
    }
    if (!startArgument.isEmpty()) {
        const auto parsed =
            wave::parseAutomationTime(*loaded.project, startArgument, clockId);
        if (!parsed.ok()) {
            return fail(
                QStringLiteral("relations"),
                QStringLiteral("time-invalid"),
                QStringLiteral("--start: %1").arg(parsed.error),
                Rejected,
                options.pretty);
        }
        query.start = *parsed.tick;
    }
    if (!endArgument.isEmpty()) {
        const auto parsed =
            wave::parseAutomationTime(*loaded.project, endArgument, clockId);
        if (!parsed.ok()) {
            return fail(
                QStringLiteral("relations"),
                QStringLiteral("time-invalid"),
                QStringLiteral("--end: %1").arg(parsed.error),
                Rejected,
                options.pretty);
        }
        query.end = *parsed.tick;
    }

    auto report = wave::findRelationsForAutomation(
        *loaded.project,
        query,
        scenarioId,
        resolvedLaneIds);
    if (!report.ok()) {
        return fail(
            QStringLiteral("relations"),
            QStringLiteral("relation-query-rejected"),
            report.error,
            Rejected,
            options.pretty);
    }
    report.json.insert(
        QStringLiteral("startInput"),
        startArgument.isEmpty()
            ? QJsonValue{QJsonValue::Null}
            : QJsonValue{startArgument});
    report.json.insert(
        QStringLiteral("endInput"),
        endArgument.isEmpty()
            ? QJsonValue{QJsonValue::Null}
            : QJsonValue{endArgument});
    if (clockId) {
        report.json.insert(
            QStringLiteral("clockId"),
            QString::fromStdString(*clockId));
    }
    report.json.insert(QStringLiteral("sourcePath"), projectPath);
    report.json.insert(QStringLiteral("sourceSha256"), sha256(sourceBytes));
    report.json.insert(
        QStringLiteral("loadWarnings"), warningsArray(loaded.warnings));
    QTextStream stream(stdout);
    writeJson(stream, report.json, options.pretty);
    return Success;
}

int runValidate(const QStringList& arguments)
{
    CommonOptions options;
    bool failOnWarning = false;
    QString projectArgument;
    QString error;
    for (const auto& argument : arguments) {
        if (argument == QStringLiteral("--fail-on-warning")) {
            failOnWarning = true;
        } else if (argument.startsWith(QLatin1Char('-'))) {
            if (!parseCommonOption(argument, options, error)) {
                if (error.isEmpty()) {
                    error = QStringLiteral("Unknown option: %1").arg(argument);
                }
                return fail(
                    QStringLiteral("validate"),
                    QStringLiteral("usage"),
                    error,
                    UsageError,
                    options.pretty);
            }
        } else if (projectArgument.isEmpty()) {
            projectArgument = argument;
        } else {
            return fail(
                QStringLiteral("validate"),
                QStringLiteral("usage"),
                QStringLiteral("validate accepts one project path."),
                UsageError,
                options.pretty);
        }
    }
    if (projectArgument.isEmpty()) {
        return fail(
            QStringLiteral("validate"),
            QStringLiteral("usage"),
            QStringLiteral("Missing project path."),
            UsageError,
            options.pretty);
    }

    const auto projectPath = QFileInfo(projectArgument).absoluteFilePath();
    QByteArray sourceBytes;
    if (!readFile(projectPath, sourceBytes, error)) {
        return fail(
            QStringLiteral("validate"),
            QStringLiteral("read-failed"),
            error,
            InputError,
            options.pretty);
    }
    const auto loaded = wave::loadProjectFile(projectPath);
    if (!loaded.ok()) {
        return fail(
            QStringLiteral("validate"),
            QStringLiteral("project-invalid"),
            loaded.error,
            InputError,
            options.pretty);
    }
    auto report = wave::validateProjectForAutomation(
        *loaded.project, options.scenarioId);
    if (!report.ok()) {
        return fail(
            QStringLiteral("validate"),
            QStringLiteral("scenario-not-found"),
            report.error,
            InputError,
            options.pretty);
    }
    report.json.insert(QStringLiteral("sourcePath"), projectPath);
    report.json.insert(QStringLiteral("sourceSha256"), sha256(sourceBytes));
    report.json.insert(
        QStringLiteral("loadWarnings"), warningsArray(loaded.warnings));
    const auto summary = report.json.value(QStringLiteral("summary")).toObject();
    const auto errors = jsonCount(summary, QStringLiteral("errors")).value_or(0);
    const auto warnings =
        jsonCount(summary, QStringLiteral("warnings")).value_or(0)
        + loaded.warnings.size();
    const auto accepted =
        errors == 0 && (!failOnWarning || warnings == 0);
    report.json.insert(QStringLiteral("failOnWarning"), failOnWarning);
    report.json.insert(QStringLiteral("accepted"), accepted);
    report.json.insert(QStringLiteral("ok"), accepted);
    QTextStream stream(stdout);
    writeJson(stream, report.json, options.pretty);
    return accepted ? Success : Rejected;
}

int runApply(const QStringList& arguments)
{
    CommonOptions options;
    QString projectArgument;
    QString operationsArgument;
    QString outputArgument;
    QString backupArgument;
    QString expectedSha256;
    bool inPlace = false;
    bool dryRun = false;
    bool automaticBackup = false;
    int backupCount = 0;
    QString error;
    for (const auto& argument : arguments) {
        if (argument == QStringLiteral("--in-place")) {
            inPlace = true;
        } else if (argument == QStringLiteral("--dry-run")) {
            dryRun = true;
        } else if (argument.startsWith(QStringLiteral("--output="))) {
            outputArgument =
                argument.mid(QStringLiteral("--output=").size()).trimmed();
        } else if (argument == QStringLiteral("--backup")) {
            ++backupCount;
            automaticBackup = true;
        } else if (argument.startsWith(QStringLiteral("--backup="))) {
            ++backupCount;
            backupArgument =
                argument.mid(QStringLiteral("--backup=").size()).trimmed();
        } else if (argument.startsWith(QStringLiteral("--expect-sha256="))) {
            expectedSha256 =
                argument.mid(QStringLiteral("--expect-sha256=").size())
                    .trimmed()
                    .toLower();
        } else if (argument.startsWith(QLatin1Char('-'))
                   && argument != QStringLiteral("-")) {
            if (!parseCommonOption(argument, options, error)) {
                if (error.isEmpty()) {
                    error = QStringLiteral("Unknown option: %1").arg(argument);
                }
                return fail(
                    QStringLiteral("apply"),
                    QStringLiteral("usage"),
                    error,
                    UsageError,
                    options.pretty);
            }
        } else if (projectArgument.isEmpty()) {
            projectArgument = argument;
        } else if (operationsArgument.isEmpty()) {
            operationsArgument = argument;
        } else {
            return fail(
                QStringLiteral("apply"),
                QStringLiteral("usage"),
                QStringLiteral(
                    "apply accepts a project path and one operations path or '-'."),
                UsageError,
                options.pretty);
        }
    }
    if (projectArgument.isEmpty() || operationsArgument.isEmpty()) {
        return fail(
            QStringLiteral("apply"),
            QStringLiteral("usage"),
            QStringLiteral("Missing project or operations path."),
            UsageError,
            options.pretty);
    }
    if (backupCount > 1
        || (backupCount == 1 && !automaticBackup && backupArgument.isEmpty())) {
        return fail(
            QStringLiteral("apply"),
            QStringLiteral("usage"),
            backupCount > 1
                ? QStringLiteral("--backup may be provided only once.")
                : QStringLiteral("--backup=PATH requires a non-empty path."),
            UsageError,
            options.pretty);
    }
    if (backupCount == 1 && !inPlace) {
        return fail(
            QStringLiteral("apply"),
            QStringLiteral("usage"),
            QStringLiteral("--backup is available only with --in-place."),
            UsageError,
            options.pretty);
    }
    const auto destinationCount =
        (inPlace ? 1 : 0) + (!outputArgument.isEmpty() ? 1 : 0);
    if ((!dryRun && destinationCount != 1)
        || (dryRun && destinationCount > 1)) {
        return fail(
            QStringLiteral("apply"),
            QStringLiteral("usage"),
            dryRun
                ? QStringLiteral(
                      "Dry-run accepts at most one of --output or --in-place.")
                : QStringLiteral(
                      "Choose exactly one of --output=PATH or --in-place."),
            UsageError,
            options.pretty);
    }
    static const QRegularExpression shaExpression(
        QStringLiteral(R"(^[0-9a-f]{64}$)"));
    if (!expectedSha256.isEmpty()
        && !shaExpression.match(expectedSha256).hasMatch()) {
        return fail(
            QStringLiteral("apply"),
            QStringLiteral("usage"),
            QStringLiteral("--expect-sha256 requires 64 hexadecimal digits."),
            UsageError,
            options.pretty);
    }

    const auto projectPath = QFileInfo(projectArgument).absoluteFilePath();
    QByteArray sourceBytes;
    if (!readFile(projectPath, sourceBytes, error)) {
        return fail(
            QStringLiteral("apply"),
            QStringLiteral("read-failed"),
            error,
            InputError,
            options.pretty);
    }
    const auto sourceHash = sha256(sourceBytes);
    if (!expectedSha256.isEmpty()
        && sourceHash.compare(expectedSha256, Qt::CaseInsensitive) != 0) {
        return fail(
            QStringLiteral("apply"),
            QStringLiteral("source-conflict"),
            QStringLiteral(
                "Loaded source SHA-256 does not match --expect-sha256."),
            Rejected,
            options.pretty);
    }
    const auto loaded = wave::loadProjectFile(projectPath);
    if (!loaded.ok()) {
        return fail(
            QStringLiteral("apply"),
            QStringLiteral("project-invalid"),
            loaded.error,
            InputError,
            options.pretty);
    }
    QByteArray operationsBytes;
    if (!readOperations(operationsArgument, operationsBytes, error)) {
        return fail(
            QStringLiteral("apply"),
            QStringLiteral("operations-read-failed"),
            error,
            InputError,
            options.pretty);
    }
    QJsonParseError parseError;
    const auto operationsDocument =
        QJsonDocument::fromJson(operationsBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError
        || !operationsDocument.isObject()) {
        return fail(
            QStringLiteral("apply"),
            QStringLiteral("operations-invalid"),
            parseError.error == QJsonParseError::NoError
                ? QStringLiteral("Operations document must be a JSON object.")
                : parseError.errorString(),
            InputError,
            options.pretty);
    }

    auto applied = wave::applyAutomationBatch(
        *loaded.project,
        operationsDocument.object(),
        options.scenarioId);
    if (!applied.ok()) {
        if (!applied.json.isEmpty()) {
            applied.json.insert(QStringLiteral("sourcePath"), projectPath);
            applied.json.insert(QStringLiteral("sourceSha256"), sourceHash);
            applied.json.insert(QStringLiteral("dryRun"), dryRun);
            applied.json.insert(QStringLiteral("written"), false);
            applied.json.insert(QStringLiteral("backupWritten"), false);
            applied.json.insert(QStringLiteral("backupAvailable"), false);
            applied.json.insert(
                QStringLiteral("outputPath"),
                QJsonValue{QJsonValue::Null});
            applied.json.insert(
                QStringLiteral("backupPath"),
                QJsonValue{QJsonValue::Null});
            applied.json.insert(
                QStringLiteral("loadWarnings"),
                warningsArray(loaded.warnings));
            QTextStream stream(stderr);
            writeJson(stream, applied.json, options.pretty);
            return Rejected;
        }
        return fail(
            QStringLiteral("apply"),
            QStringLiteral("operation-rejected"),
            applied.error,
            Rejected,
            options.pretty,
            applied.failedOperation >= 0
                ? std::optional<int>{applied.failedOperation}
                : std::nullopt);
    }

    const auto resultBytes = wave::serializeProject(*applied.project);
    const auto resultHash = sha256(resultBytes);
    auto outputPath = inPlace
        ? projectPath
        : outputArgument.isEmpty()
            ? QString{}
            : QFileInfo(outputArgument).absoluteFilePath();
    if (!inPlace
        && !outputPath.isEmpty()
        && samePath(outputPath, projectPath)) {
        return fail(
            QStringLiteral("apply"),
            QStringLiteral("usage"),
            QStringLiteral(
                "--output must differ from the source project; use --in-place for replacement."),
            UsageError,
            options.pretty);
    }
    auto automaticBackupPath = projectPath;
    if (automaticBackupPath.endsWith(
            QStringLiteral(".wave.json"), Qt::CaseInsensitive)) {
        automaticBackupPath.chop(QStringLiteral(".wave.json").size());
    }
    automaticBackupPath += QStringLiteral(".backup-")
        + sourceHash.left(12) + QStringLiteral(".wave.json");
    auto backupPath = backupCount == 0
        ? QString{}
        : automaticBackup
            ? automaticBackupPath
            : QFileInfo(backupArgument).absoluteFilePath();
    if (!backupPath.isEmpty()) {
        const auto absoluteBackup = QFileInfo(backupPath).absoluteFilePath();
        const auto absoluteProject = QFileInfo(projectPath).absoluteFilePath();
        const auto absoluteOperations =
            operationsArgument == QStringLiteral("-")
            ? QString{}
            : QFileInfo(operationsArgument).absoluteFilePath();
        if (samePath(absoluteBackup, absoluteProject)
            || (!absoluteOperations.isEmpty()
                && samePath(absoluteBackup, absoluteOperations))) {
            return fail(
                QStringLiteral("apply"),
                QStringLiteral("usage"),
                QStringLiteral(
                    "Backup path must differ from the project and operations files."),
                UsageError,
                options.pretty);
        }
        backupPath = absoluteBackup;
    }
    bool written = false;
    bool backupWritten = false;
    bool backupAvailable = false;
    if (!dryRun) {
        const auto sameTarget =
            samePath(outputPath, projectPath);
        if (sameTarget) {
            QByteArray currentBytes;
            if (!readFile(projectPath, currentBytes, error)) {
                return fail(
                    QStringLiteral("apply"),
                    QStringLiteral("source-recheck-failed"),
                    error,
                    InputError,
                    options.pretty);
            }
            if (sha256(currentBytes) != sourceHash) {
                return fail(
                    QStringLiteral("apply"),
                    QStringLiteral("source-conflict"),
                    QStringLiteral(
                        "Project changed after it was loaded; no output was written."),
                    Rejected,
                    options.pretty);
            }
        }
        if (sameTarget && applied.changed && !backupPath.isEmpty()) {
            if (QFileInfo::exists(backupPath)) {
                QByteArray existingBackup;
                if (!readFile(backupPath, existingBackup, error)) {
                    return fail(
                        QStringLiteral("apply"),
                        QStringLiteral("backup-read-failed"),
                        error,
                        OutputError,
                        options.pretty);
                }
                if (sha256(existingBackup) != sourceHash) {
                    return fail(
                        QStringLiteral("apply"),
                        QStringLiteral("backup-conflict"),
                        QStringLiteral(
                            "Backup path already contains different content; no project output was written."),
                        Rejected,
                        options.pretty);
                }
                backupAvailable = true;
            } else {
                if (!writeBytesAtomic(backupPath, sourceBytes, error)) {
                    return fail(
                        QStringLiteral("apply"),
                        QStringLiteral("backup-write-failed"),
                        error,
                        OutputError,
                        options.pretty);
                }
                backupWritten = true;
                backupAvailable = true;
            }
        }
        if (applied.changed || !sameTarget) {
            if (!wave::saveProjectFileAtomic(
                    *applied.project, outputPath, &error)) {
                if (backupAvailable) {
                    error += QStringLiteral(
                        " Original source backup: %1").arg(backupPath);
                }
                return fail(
                    QStringLiteral("apply"),
                    QStringLiteral("write-failed"),
                    error,
                    OutputError,
                    options.pretty);
            }
            written = true;
        }
    }

    applied.json.insert(QStringLiteral("sourcePath"), projectPath);
    applied.json.insert(QStringLiteral("sourceSha256"), sourceHash);
    applied.json.insert(QStringLiteral("resultSha256"), resultHash);
    applied.json.insert(QStringLiteral("dryRun"), dryRun);
    applied.json.insert(QStringLiteral("written"), written);
    applied.json.insert(QStringLiteral("backupWritten"), backupWritten);
    applied.json.insert(QStringLiteral("backupAvailable"), backupAvailable);
    applied.json.insert(
        QStringLiteral("outputPath"),
        dryRun ? QJsonValue{QJsonValue::Null} : QJsonValue{outputPath});
    applied.json.insert(
        QStringLiteral("backupPath"),
        backupPath.isEmpty()
            ? QJsonValue{QJsonValue::Null}
            : QJsonValue{backupPath});
    applied.json.insert(
        QStringLiteral("loadWarnings"), warningsArray(loaded.warnings));
    QTextStream stream(stdout);
    writeJson(stream, applied.json, options.pretty);
    return Success;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("wave-cli"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1"));
    auto arguments = application.arguments();
    arguments.removeFirst();
    if (arguments.isEmpty()
        || arguments.front() == QStringLiteral("--help")
        || arguments.front() == QStringLiteral("-h")) {
        printUsage();
        return arguments.isEmpty() ? UsageError : Success;
    }
    if (arguments.front() == QStringLiteral("--version")) {
        QTextStream(stdout) << "wave-cli 1\n";
        return Success;
    }

    const auto command = arguments.takeFirst();
    if (const auto* adapter = compatibilityCommand(command)) {
        return runCompatibilityCommand(*adapter, arguments);
    }
    if (command == QStringLiteral("capabilities")) {
        return runCapabilities(arguments);
    }
    if (command == QStringLiteral("new")) return runNew(arguments);
    if (command == QStringLiteral("inspect")) return runInspect(arguments);
    if (command == QStringLiteral("signals")) return runSignals(arguments);
    if (command == QStringLiteral("sample")) return runSample(arguments);
    if (command == QStringLiteral("window")) return runWindow(arguments);
    if (command == QStringLiteral("edges")) return runEdges(arguments);
    if (command == QStringLiteral("markers")) return runMarkers(arguments);
    if (command == QStringLiteral("relations")) {
        return runRelations(arguments);
    }
    if (command == QStringLiteral("validate")) return runValidate(arguments);
    if (command == QStringLiteral("apply")) return runApply(arguments);
    return fail(
        command,
        QStringLiteral("usage"),
        QStringLiteral("Unknown command: %1").arg(command),
        UsageError,
        false);
}
