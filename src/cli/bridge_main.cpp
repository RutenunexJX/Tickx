#include "wave/integration.h"
#include "wave/project_io.h"

#include "scenario_selection.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

#include <optional>

namespace {

bool readFile(const QString& path, QByteArray& content, QString& error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = file.errorString();
        return false;
    }
    content = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        error = file.errorString();
        return false;
    }
    return true;
}

bool writeAtomic(const QString& path, const QByteArray& content, QString& error)
{
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(content) != content.size()
        || !file.commit()) {
        error = file.errorString();
        return false;
    }
    return true;
}

int usage()
{
    QTextStream(stderr)
        << "Usage:\n"
           "  wave-bridge describe <project.wave.json> <manifest.json>\n"
           "  wave-bridge import-module <module-manifest.json> <output-project.wave.json>\n"
           "  wave-bridge export-stimulus <project.wave.json> <stimulus.json> [--scenario=SELECTOR]\n"
           "  wave-bridge import-stimulus <module-manifest.json> <stimulus.json> <output-project.wave.json>\n"
           "  wave-bridge import-signals <project.wave.json> <signals.json> <output-project.wave.json> [--scenario=SELECTOR]\n"
           "  wave-bridge link-frame <project.wave.json> <frame-reference.json> <output-project.wave.json>\n"
           "  wave-bridge pinloom-entry <project.wave.json> <artifact-directory> <entry.json> [--scenario=SELECTOR]\n";
    return 2;
}

bool parseScenarioOption(
    const QStringList& arguments,
    int requiredSize,
    std::optional<QString>& selector,
    QString& error)
{
    if (arguments.size() == requiredSize) {
        return true;
    }
    if (arguments.size() != requiredSize + 1
        || !arguments.back().startsWith(
            QStringLiteral("--scenario="))) {
        error = QStringLiteral(
            "Expected an optional trailing --scenario=SELECTOR.");
        return false;
    }
    selector =
        arguments.back().mid(
            QStringLiteral("--scenario=").size());
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    const auto arguments = application.arguments();
    if (arguments.size() < 2) return usage();
    const auto command = arguments[1];
    QString error;
    if (command == QStringLiteral("import-module")) {
        if (arguments.size() != 4) return usage();
        QByteArray document;
        if (!readFile(arguments[2], document, error)) {
            QTextStream(stderr) << "Cannot read Module Manifest: " << error << '\n';
            return 2;
        }
        const auto parsed = wave::parseZeroSlackModuleManifest(document);
        if (!parsed.ok()) {
            QTextStream(stderr) << parsed.error << '\n';
            return 2;
        }
        const auto imported = wave::importZeroSlackModuleManifest(*parsed.manifest);
        if (!imported.ok()) {
            QTextStream(stderr) << imported.error << '\n';
            return 2;
        }
        const auto output = QFileInfo(arguments[3]).absoluteFilePath();
        if (!wave::saveProjectFileAtomic(*imported.project, output, &error)) {
            QTextStream(stderr) << "Cannot write output project: " << error << '\n';
            return 2;
        }
        QTextStream(stdout)
            << "Imported ZeroSlack module target: "
            << parsed.manifest->target.module.c_str() << "\n"
            << "Clock suggestion: "
            << wave::toString(imported.clockSuggestion.state).data() << "\n"
            << "Reset suggestion: "
            << wave::toString(imported.resetSuggestion.state).data() << "\n"
            << imported.stimulusLaneIds.size() << " stimulus lane(s), "
            << imported.watchLaneIds.size() << " watch lane(s)\n";
        for (const auto& diagnostic : imported.diagnostics) {
            QTextStream(stdout) << "warning: " << diagnostic << '\n';
        }
        return 0;
    }
    if (command == QStringLiteral("import-stimulus")) {
        if (arguments.size() != 5) return usage();
        QByteArray manifestDocument;
        QByteArray stimulusDocument;
        if (!readFile(arguments[2], manifestDocument, error)) {
            QTextStream(stderr) << "Cannot read Module Manifest: " << error << '\n';
            return 2;
        }
        if (!readFile(arguments[3], stimulusDocument, error)) {
            QTextStream(stderr) << "Cannot read Stimulus Scenario: " << error << '\n';
            return 2;
        }
        const auto manifest = wave::parseZeroSlackModuleManifest(manifestDocument);
        if (!manifest.ok()) {
            QTextStream(stderr) << manifest.error << '\n';
            return 2;
        }
        const auto stimulus = wave::parseZeroSlackStimulusScenario(stimulusDocument);
        if (!stimulus.ok()) {
            QTextStream(stderr) << stimulus.error << '\n';
            return 2;
        }
        const auto restored = wave::restoreZeroSlackStimulusScenario(
            *manifest.manifest, *stimulus.scenario);
        if (!restored.ok()) {
            QTextStream(stderr) << restored.error << '\n';
            return 2;
        }
        const auto output = QFileInfo(arguments[4]).absoluteFilePath();
        if (!wave::saveProjectFileAtomic(*restored.project, output, &error)) {
            QTextStream(stderr) << "Cannot write restored project: " << error << '\n';
            return 2;
        }
        QTextStream(stdout)
            << "Restored ZeroSlack stimulus: " << restored.restoredPortCount
            << " port(s), manifest "
            << (restored.manifestChanged ? "changed" : "unchanged") << '\n';
        for (const auto& diagnostic : restored.diagnostics) {
            QTextStream(stdout) << "warning: " << diagnostic << '\n';
        }
        return 0;
    }
    if (arguments.size() < 4) return usage();
    const auto projectPath = QFileInfo(arguments[2]).absoluteFilePath();
    const auto loaded = wave::loadProjectFile(projectPath);
    if (!loaded.ok()) {
        QTextStream(stderr) << loaded.error << '\n';
        return 2;
    }
    auto project = *loaded.project;

    if (command == QStringLiteral("export-stimulus")) {
        std::optional<QString> scenarioSelector;
        if (!parseScenarioOption(arguments, 4, scenarioSelector, error)) {
            QTextStream(stderr) << error << '\n';
            return usage();
        }
        const auto scenarioIndex = wave::cli::resolveScenarioIndex(
            project, scenarioSelector, error);
        if (!scenarioIndex) {
            QTextStream(stderr) << error << '\n';
            return 2;
        }
        const auto& scenario = project.scenarios.at(*scenarioIndex);
        const auto exported = wave::exportZeroSlackStimulusScenario(project, scenario);
        if (!exported.ok()) {
            QTextStream(stderr) << exported.error << '\n';
            return 2;
        }
        const auto output = QFileInfo(arguments[3]).absoluteFilePath();
        if (!writeAtomic(
                output,
                wave::serializeZeroSlackStimulusScenario(*exported.scenario),
                error)) {
            QTextStream(stderr) << "Cannot write Stimulus Scenario: " << error << '\n';
            return 2;
        }
        QTextStream(stdout)
            << "Exported ZeroSlack stimulus for " << scenario.name.c_str()
            << " (" << scenario.id.c_str() << "): "
            << exported.scenario->ports.size() << " port(s)\n";
        for (const auto& diagnostic : exported.diagnostics) {
            QTextStream(stdout) << "warning: " << diagnostic << '\n';
        }
        return 0;
    }

    if (command == QStringLiteral("describe")) {
        if (arguments.size() != 4) return usage();
        const auto output = QFileInfo(arguments[3]).absoluteFilePath();
        if (!writeAtomic(output, wave::makeWorkspaceManifest(project, projectPath), error)) {
            QTextStream(stderr) << "Cannot write workspace manifest: " << error << '\n';
            return 2;
        }
        QTextStream(stdout) << "Wrote ZeroSlack workspace manifest: " << output << '\n';
        return 0;
    }
    if (command == QStringLiteral("import-signals")) {
        std::optional<QString> scenarioSelector;
        if (!parseScenarioOption(
                arguments,
                5,
                scenarioSelector,
                error)) {
            QTextStream(stderr) << error << '\n';
            return usage();
        }
        const auto scenarioIndex =
            wave::cli::resolveScenarioIndex(
                project,
                scenarioSelector,
                error);
        if (!scenarioIndex) {
            QTextStream(stderr) << error << '\n';
            return 2;
        }
        auto& scenario =
            project.scenarios.at(*scenarioIndex);
        QByteArray document;
        if (!readFile(arguments[3], document, error)) {
            QTextStream(stderr) << "Cannot read signal list: " << error << '\n';
            return 2;
        }
        const auto parsed = wave::parseZeroSlackSignalList(document);
        if (!parsed.ok()) {
            QTextStream(stderr) << parsed.error << '\n';
            return 2;
        }
        const auto imported = wave::applyZeroSlackSignalList(
            project,
            scenario,
            *parsed.signalList);
        const auto output = QFileInfo(arguments[4]).absoluteFilePath();
        if (!wave::saveProjectFileAtomic(project, output, &error)) {
            QTextStream(stderr) << "Cannot write output project: " << error << '\n';
            return 2;
        }
        QTextStream(stdout)
            << "Imported ZeroSlack signals: " << imported.added
            << " added, " << imported.existing << " existing in "
            << scenario.name.c_str() << " ("
            << scenario.id.c_str() << ")\n";
        for (const auto& diagnostic : imported.diagnostics) {
            QTextStream(stdout) << "warning: " << diagnostic << '\n';
        }
        return 0;
    }
    if (command == QStringLiteral("link-frame")) {
        if (arguments.size() != 5) return usage();
        QByteArray document;
        if (!readFile(arguments[3], document, error)) {
            QTextStream(stderr) << "Cannot read frame reference: " << error << '\n';
            return 2;
        }
        const auto parsed = wave::parseFrameSampleReference(document);
        if (!parsed.ok()) {
            QTextStream(stderr) << parsed.error << '\n';
            return 2;
        }
        const auto linked = wave::linkFrameSample(project, *parsed.reference);
        const auto output = QFileInfo(arguments[4]).absoluteFilePath();
        if (!wave::saveProjectFileAtomic(project, output, &error)) {
            QTextStream(stderr) << "Cannot write output project: " << error << '\n';
            return 2;
        }
        QTextStream(stdout)
            << (linked.updated ? "Updated" : "Added")
            << " Private Frame sample " << linked.linkedResourceId.c_str() << '\n';
        for (const auto& diagnostic : linked.diagnostics) {
            QTextStream(stdout) << "warning: " << diagnostic << '\n';
        }
        return 0;
    }
    if (command == QStringLiteral("pinloom-entry")) {
        std::optional<QString> scenarioSelector;
        if (!parseScenarioOption(
                arguments,
                5,
                scenarioSelector,
                error)) {
            QTextStream(stderr) << error << '\n';
            return usage();
        }
        const auto scenarioIndex =
            wave::cli::resolveScenarioIndex(
                project,
                scenarioSelector,
                error);
        if (!scenarioIndex) {
            QTextStream(stderr) << error << '\n';
            return 2;
        }
        const auto& scenario =
            project.scenarios.at(*scenarioIndex);
        const auto output = QFileInfo(arguments[4]).absoluteFilePath();
        const auto entry = wave::makePinloomEntry(
            project,
            scenario,
            projectPath,
            QFileInfo(arguments[3]).absoluteFilePath(),
            output);
        if (!writeAtomic(output, entry.document, error)) {
            QTextStream(stderr) << "Cannot write Pinloom entry: " << error << '\n';
            return 2;
        }
        QTextStream(stdout)
            << "Pinloom archive URI for "
            << scenario.name.c_str() << " ("
            << scenario.id.c_str() << "): "
            << entry.archiveUri.toString(QUrl::FullyEncoded) << '\n';
        return 0;
    }
    return usage();
}
