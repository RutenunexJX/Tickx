#include "wave/compare.h"
#include "wave/project_io.h"
#include "wave/trace.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>

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
               "[--trace-id=ID] [--edge-tolerance-tick=N] "
               "[--x=exact|ignore-x|wildcard] [--mask=VALUE] "
               "[--start-tick=N --end-tick=N] [--relation-only] "
               "[--fail-on-difference]\n";
        return 2;
    }

    const auto projectPath = QFileInfo(arguments[1]).absoluteFilePath();
    const auto outputDirectory = QFileInfo(arguments[2]).absoluteFilePath();
    QString traceId;
    wave::CompareOptions compareOptions;
    bool failOnDifference = false;
    for (int index = 3; index < arguments.size(); ++index) {
        const auto& argument = arguments[index];
        if (argument.startsWith(QStringLiteral("--trace-id="))) {
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
    if (project.scenarios.empty() || project.importedTraces.empty()) {
        QTextStream(stderr) << "Project has no scenario or imported trace reference.\n";
        return 2;
    }
    const auto traceIterator = traceId.isEmpty()
        ? project.importedTraces.begin()
        : std::find_if(
            project.importedTraces.begin(),
            project.importedTraces.end(),
            [&traceId](const wave::ImportedTrace& trace) {
                return QString::fromStdString(trace.id) == traceId;
            });
    if (traceIterator == project.importedTraces.end()) {
        QTextStream(stderr) << "Requested trace reference does not exist.\n";
        return 2;
    }
    const auto& reference = *traceIterator;
    auto tracePath = QString::fromUtf8(reference.path);
    if (!QFileInfo(tracePath).isAbsolute()) {
        tracePath = QFileInfo(projectPath).absoluteDir().absoluteFilePath(tracePath);
    }
    wave::TraceParseOptions parseOptions;
    parseOptions.projectTimeBase = project.timeBase;
    parseOptions.identity = {project.id, reference.id, 1};
    parseOptions.offset = reference.offset;
    const auto format = QString::fromStdString(reference.format).toLower();
    auto parsed = format == QStringLiteral("vcd")
        ? wave::parseVcdFile(nativePath(tracePath), parseOptions)
        : format == QStringLiteral("csv")
            ? wave::parseCsvFile(nativePath(tracePath), parseOptions)
            : wave::TraceParseResult{};
    if (!parsed.ok()) {
        const auto message = parsed.errorSummary();
        QTextStream(stderr)
            << (message.empty() ? QStringLiteral("Unsupported trace format.") : QString::fromStdString(message))
            << '\n';
        return 2;
    }

    const auto& scenario = project.scenarios.front();
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
        << ": " << result.differences.size() << " difference(s)"
        << ", tolerated edges " << result.toleratedEdgeCount << '\n';
    return failOnDifference && !result.matches() ? 3 : 0;
}
