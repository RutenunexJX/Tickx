#include "wave/integration.h"
#include "wave/project_io.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

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
           "  wave-bridge import-signals <project.wave.json> <signals.json> <output-project.wave.json>\n"
           "  wave-bridge link-frame <project.wave.json> <frame-reference.json> <output-project.wave.json>\n"
           "  wave-bridge pinloom-entry <project.wave.json> <artifact-directory> <entry.json>\n";
    return 2;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    const auto arguments = application.arguments();
    if (arguments.size() < 4) return usage();
    const auto command = arguments[1];
    const auto projectPath = QFileInfo(arguments[2]).absoluteFilePath();
    const auto loaded = wave::loadProjectFile(projectPath);
    if (!loaded.ok()) {
        QTextStream(stderr) << loaded.error << '\n';
        return 2;
    }
    auto project = *loaded.project;
    QString error;

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
        if (arguments.size() != 5 || project.scenarios.empty()) return usage();
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
            project.scenarios.front(),
            *parsed.signalList);
        const auto output = QFileInfo(arguments[4]).absoluteFilePath();
        if (!wave::saveProjectFileAtomic(project, output, &error)) {
            QTextStream(stderr) << "Cannot write output project: " << error << '\n';
            return 2;
        }
        QTextStream(stdout)
            << "Imported ZeroSlack signals: " << imported.added
            << " added, " << imported.existing << " existing\n";
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
        if (arguments.size() != 5 || project.scenarios.empty()) return usage();
        const auto output = QFileInfo(arguments[4]).absoluteFilePath();
        const auto entry = wave::makePinloomEntry(
            project,
            project.scenarios.front(),
            projectPath,
            QFileInfo(arguments[3]).absoluteFilePath(),
            output);
        if (!writeAtomic(output, entry.document, error)) {
            QTextStream(stderr) << "Cannot write Pinloom entry: " << error << '\n';
            return 2;
        }
        QTextStream(stdout)
            << "Pinloom archive URI: " << entry.archiveUri.toString(QUrl::FullyEncoded) << '\n';
        return 0;
    }
    return usage();
}
