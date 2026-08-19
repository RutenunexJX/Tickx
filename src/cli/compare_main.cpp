#include "wave/compare.h"
#include "wave/fst_trace.h"
#include "wave/project_io.h"
#include "wave/trace.h"

#include "scenario_selection.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace {

std::filesystem::path nativePath(const QString& path)
{
#ifdef _WIN32
    return std::filesystem::path(path.toStdWString());
#else
    return std::filesystem::path(path.toUtf8().constData());
#endif
}

QString safeName(QString name)
{
    for (auto& character : name) {
        if (!character.isLetterOrNumber() && character != QLatin1Char('-')
            && character != QLatin1Char('_')) {
            character = QLatin1Char('_');
        }
    }
    while (name.contains(QStringLiteral("__"))) name.replace(QStringLiteral("__"), QStringLiteral("_"));
    name = name.trimmed();
    return name.isEmpty() ? QStringLiteral("scenario") : name;
}

bool writeAtomic(const QString& path, const std::string& content, QString& error)
{
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    const auto bytes = QByteArray::fromStdString(content);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(bytes) != bytes.size()
        || !file.commit()) {
        error = file.errorString();
        return false;
    }
    return true;
}

std::optional<wave::Tick> integerOption(const QString& value)
{
    bool valid = false;
    const auto parsed = value.toLongLong(&valid);
    return valid ? std::optional<wave::Tick>{parsed} : std::nullopt;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    const auto arguments = application.arguments();
    if (arguments.size() < 3) {
        QTextStream(stderr)
            << "Usage: wave-compare <project.wave.json> <output-directory> "
               "[--scenario=SELECTOR] [--trace-id=ID] "
               "[--edge-tolerance-tick=N] "
               "[--x=exact|ignore-x|wildcard] [--mask=VALUE] "
               "[--start-tick=N --end-tick=N] [--relation-only] "
               "[--fail-on-difference]\n";
        return 2;
    }

    const auto projectPath = QFileInfo(arguments[1]).absoluteFilePath();
    const auto outputDirectory = QFileInfo(arguments[2]).absoluteFilePath();
    std::optional<QString> scenarioSelector;
    QString traceId;
    wave::CompareOptions compareOptions;
    bool failOnDifference = false;
    for (int index = 3; index < arguments.size(); ++index) {
        const auto& argument = arguments[index];
        if (argument.startsWith(
                QStringLiteral("--scenario="))) {
            if (scenarioSelector) {
                QTextStream(stderr)
                    << "--scenario may be specified only once.\n";
                return 2;
            }
            scenarioSelector = argument.mid(
                QStringLiteral("--scenario=").size());
        } else if (argument.startsWith(QStringLiteral("--trace-id="))) {
            traceId = argument.mid(QStringLiteral("--trace-id=").size());
        } else if (argument.startsWith(QStringLiteral("--edge-tolerance-tick="))) {
            const auto parsed = integerOption(
                argument.mid(QStringLiteral("--edge-tolerance-tick=").size()));
            if (!parsed || *parsed < 0) {
                QTextStream(stderr) << "Invalid edge tolerance.\n";
                return 2;
            }
            compareOptions.defaultRule.edgeTolerance = *parsed;
        } else if (argument.startsWith(QStringLiteral("--x="))) {
            const auto value = argument.mid(QStringLiteral("--x=").size()).toLower();
            if (value == QStringLiteral("exact")) {
                compareOptions.defaultRule.xHandling = wave::XHandling::Exact;
            } else if (value == QStringLiteral("ignore-x")) {
                compareOptions.defaultRule.xHandling = wave::XHandling::IgnoreAnyX;
            } else if (value == QStringLiteral("wildcard")) {
                compareOptions.defaultRule.xHandling = wave::XHandling::ExpectedXWildcard;
            } else {
                QTextStream(stderr) << "Invalid X handling mode.\n";
                return 2;
            }
        } else if (argument.startsWith(QStringLiteral("--mask="))) {
            compareOptions.defaultRule.busMask =
                argument.mid(QStringLiteral("--mask=").size()).toStdString();
        } else if (argument.startsWith(QStringLiteral("--start-tick="))) {
            compareOptions.start = integerOption(
                argument.mid(QStringLiteral("--start-tick=").size()));
            if (!compareOptions.start) {
                QTextStream(stderr) << "Invalid start tick.\n";
                return 2;
            }
        } else if (argument.startsWith(QStringLiteral("--end-tick="))) {
            compareOptions.end = integerOption(
                argument.mid(QStringLiteral("--end-tick=").size()));
            if (!compareOptions.end) {
                QTextStream(stderr) << "Invalid end tick.\n";
                return 2;
            }
        } else if (argument == QStringLiteral("--relation-only")) {
            compareOptions.relationOnly = true;
        } else if (argument == QStringLiteral("--fail-on-difference")) {
            failOnDifference = true;
        } else {
            QTextStream(stderr) << "Unknown option: " << argument << '\n';
            return 2;
        }
    }

    const auto loaded = wave::loadProjectFile(projectPath);
    if (!loaded.ok()) {
        QTextStream(stderr) << loaded.error << '\n';
        return 2;
    }
    const auto& project = *loaded.project;
    QString scenarioError;
    const auto scenarioIndex =
        wave::cli::resolveScenarioIndex(
            project,
            scenarioSelector,
            scenarioError);
    if (!scenarioIndex) {
        QTextStream(stderr)
            << scenarioError << '\n';
        return 2;
    }
    const auto& scenario =
        project.scenarios.at(*scenarioIndex);
    if (project.importedTraces.empty()) {
        QTextStream(stderr)
            << "Project has no imported trace reference.\n";
        return 2;
    }
    const wave::ImportedTrace* selectedTrace = nullptr;
    if (traceId.isEmpty()) {
        if (project.importedTraces.size() != 1) {
            QTextStream(stderr)
                << "Project has " << project.importedTraces.size()
                << " imported trace references; specify --trace-id=<stable-id>.\n";
            return 2;
        }
        selectedTrace = &project.importedTraces.front();
    } else {
        std::size_t matchCount = 0;
        for (const auto& trace : project.importedTraces) {
            if (QString::fromStdString(trace.id) != traceId) continue;
            selectedTrace = &trace;
            ++matchCount;
        }
        if (matchCount == 0) {
            QTextStream(stderr) << "Requested trace reference does not exist.\n";
            return 2;
        }
        if (matchCount > 1) {
            QTextStream(stderr)
                << "Requested trace stable ID is ambiguous; repair duplicate Imported Trace IDs before comparing.\n";
            return 2;
        }
    }
    if (!selectedTrace || selectedTrace->id.empty()) {
        QTextStream(stderr)
            << "Selected imported trace has an empty stable ID; repair it before comparing.\n";
        return 2;
    }
    const auto& reference = *selectedTrace;
    const auto storedTracePath =
        QString::fromUtf8(reference.path);
    if (storedTracePath.trimmed().isEmpty()) {
        QTextStream(stderr)
            << "Selected imported trace has an empty source path; repair it before comparing.\n";
        return 2;
    }
    const auto format =
        QString::fromStdString(reference.format)
            .trimmed()
            .toLower();
    if (format != QStringLiteral("vcd")
        && format != QStringLiteral("csv")
        && format != QStringLiteral("fst")) {
        QTextStream(stderr)
            << "Selected imported trace format '"
            << QString::fromStdString(reference.format)
            << "' is unsupported; expected VCD, FST, or CSV.\n";
        return 2;
    }
    auto tracePath = storedTracePath;
    if (!QFileInfo(tracePath).isAbsolute()) {
        tracePath = QFileInfo(projectPath).absoluteDir().absoluteFilePath(tracePath);
    }
    if (!QFileInfo(tracePath).isFile()) {
        QTextStream(stderr)
            << "Selected imported trace file is missing or not a file: "
            << tracePath << '\n';
        return 2;
    }
    wave::TraceParseOptions parseOptions;
    parseOptions.projectTimeBase = project.timeBase;
    parseOptions.identity = {project.id, reference.id, 1};
    parseOptions.offset = reference.offset;
    wave::TraceParseResult parsed;
    if (format == QStringLiteral("vcd")) {
        parsed = wave::parseVcdFile(nativePath(tracePath), parseOptions);
    } else if (format == QStringLiteral("csv")) {
        parsed = wave::parseCsvFile(nativePath(tracePath), parseOptions);
    } else {
#ifdef Q_OS_WIN
        constexpr auto readerName = "wave-wellen-reader.exe";
#else
        constexpr auto readerName = "wave-wellen-reader";
#endif
        const auto reader = QDir(QCoreApplication::applicationDirPath())
                                .filePath(QString::fromLatin1(readerName));
        parsed = wave::readFstMetadataFile(
            tracePath, parseOptions, reader);
        if (parsed.ok()) {
            std::set<std::string> required;
            for (const auto& [laneId, signalId] : reference.signalMapping) {
                static_cast<void>(laneId);
                if (parsed.index->findSignal(signalId)) required.insert(signalId);
            }
            while (!required.empty()) {
                std::vector<std::string> batch;
                const auto count = std::min<std::size_t>(64, required.size());
                batch.reserve(count);
                for (auto iterator = required.begin();
                     iterator != required.end() && batch.size() < count;) {
                    batch.push_back(*iterator);
                    iterator = required.erase(iterator);
                }
                auto loaded = wave::loadFstSignalsFile(
                    tracePath, parseOptions, reader, batch);
                std::string mergeError;
                if (!loaded.ok()
                    || !wave::mergeLoadedTraceSignals(
                        *parsed.index, loaded, &mergeError)) {
                    parsed.diagnostics.insert(
                        parsed.diagnostics.end(),
                        loaded.diagnostics.begin(),
                        loaded.diagnostics.end());
                    if (!mergeError.empty()) {
                        parsed.diagnostics.push_back({
                            wave::TraceDiagnosticSeverity::Error,
                            0,
                            std::move(mergeError)});
                    }
                    parsed.index.reset();
                    break;
                }
            }
        }
    }
    if (!parsed.ok()) {
        const auto message = parsed.errorSummary();
        QTextStream(stderr)
            << (message.empty()
                    ? QStringLiteral(
                          "Imported trace parsing failed.")
                    : QString::fromStdString(message))
            << '\n';
        return 2;
    }

    const auto result = wave::compareScenario(
        project,
        scenario,
        *parsed.index,
        reference,
        compareOptions);
    if (!QDir().mkpath(outputDirectory)) {
        QTextStream(stderr) << "Cannot create output directory.\n";
        return 2;
    }
    const auto base = safeName(QString::fromStdString(scenario.name));
    QString error;
    if (!writeAtomic(
            QDir(outputDirectory).filePath(base + QStringLiteral(".compare.json")),
            wave::compareResultJson(result),
            error)
        || !writeAtomic(
            QDir(outputDirectory).filePath(base + QStringLiteral(".compare.csv")),
            wave::compareResultCsv(result),
            error)
        || !writeAtomic(
            QDir(outputDirectory).filePath(base + QStringLiteral(".compare.html")),
            wave::compareResultHtml(result, project, scenario),
            error)) {
        QTextStream(stderr) << "Cannot write compare report: " << error << '\n';
        return 2;
    }
    QTextStream(stdout)
        << "Compared " << scenario.name.c_str()
        << " (" << scenario.id.c_str() << ")"
        << ": " << result.differences.size() << " difference(s)"
        << ", tolerated edges " << result.toleratedEdgeCount << '\n';
    return failOnDifference && !result.matches() ? 3 : 0;
}
