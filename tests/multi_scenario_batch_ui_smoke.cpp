#include "main_window.h"

#include "wave/module_manifest.h"
#include "wave/project_io.h"
#include "wave/simulation_scenario_store.h"
#include "wave/simulation_session.h"
#include "wave/stimulus_scenario.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>

#include <iostream>

namespace {

QString fixtureExecutable(const QString& name)
{
    auto executable = name;
#ifdef Q_OS_WIN
    executable += QStringLiteral(".exe");
#endif
    return QDir(QCoreApplication::applicationDirPath()).filePath(executable);
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    if (application.arguments().size() != 2) return 2;

    const QDir fixtureRoot(
        QDir(QStringLiteral(WAVE_SOURCE_DIR))
            .filePath(QStringLiteral("tests/fixtures/simulation/fixed-counter")));
    const auto manifestPath = fixtureRoot.filePath(QStringLiteral("manifest.json"));
    const auto sourceStimulusPath = fixtureRoot.filePath(
        QStringLiteral("stimulus.json"));
    const auto manifest = wave::parseZeroSlackModuleManifest(
        readFile(manifestPath));
    const auto stimulus = wave::parseZeroSlackStimulusScenario(
        readFile(sourceStimulusPath));
    if (!manifest.ok() || !stimulus.ok()) return 3;
    auto restored = wave::restoreZeroSlackStimulusScenario(
        *manifest.manifest, *stimulus.scenario);
    if (!restored.ok() || restored.project->scenarios.empty()) return 4;

    QTemporaryDir temporary;
    if (!temporary.isValid()) return 5;
    QDir root(temporary.path());
    const auto stimulusPath = root.filePath(QStringLiteral("active-stimulus.json"));
    if (!QFile::copy(sourceStimulusPath, stimulusPath)) return 6;
    const auto scenarioDirectory = root.filePath(QStringLiteral("scenarios"));
    if (!QDir().mkpath(scenarioDirectory)) return 7;

    auto second = restored.project->scenarios.front();
    second.id = "batch-scenario-second";
    second.name = "Second stored scenario";
    restored.project->scenarios.front().name = "Default stored scenario";
    restored.project->scenarios.push_back(std::move(second));
    for (std::size_t index = 0;
         index < restored.project->scenarios.size();
         ++index) {
        const auto& scenario = restored.project->scenarios.at(index);
        const auto exported = wave::exportZeroSlackStimulusScenario(
            *restored.project,
            scenario,
            {});
        if (!exported.ok()) return 8;
        const auto saved = wave::saveSimulationScenario(
            scenarioDirectory, *exported.scenario, index == 0);
        if (!saved.ok()) return 9;
    }

    wave::SimulationRunRequest request;
    request.manifestPath = manifestPath;
    request.stimulusPath = stimulusPath;
    request.workspaceRoot = fixtureRoot.absolutePath();
    request.artifactDirectory = root.filePath(QStringLiteral("artifacts"));
    request.buildCacheDirectory = root.filePath(QStringLiteral("build-cache"));
    request.scenarioDirectory = scenarioDirectory;
    request.resultProjectPath = root.filePath(QStringLiteral("result.wave.json"));
    request.toolchain.verilatorProgram = fixtureExecutable(
        QStringLiteral("wave-verilator-fixture"));
    request.toolchain.cxxProgram = fixtureExecutable(
        QStringLiteral("wave-toolchain-fixture-ready"));
    request.toolchain.environment.insert(
        QStringLiteral("WAVE_SIMULATOR_FIXTURE"),
        fixtureExecutable(QStringLiteral("wave-simulator-fixture")));
    qputenv(
        "WAVE_SIMULATOR_FIXTURE",
        fixtureExecutable(QStringLiteral("wave-simulator-fixture")).toUtf8());
    request.toolchain.timeoutMs = 1'000;
    request.buildTimeoutMs = 2'000;
    request.runTimeoutMs = 2'000;
    wave::attachSimulationSession(*restored.project, request);
    QString saveError;
    if (!wave::saveProjectFileAtomic(
            *restored.project, request.resultProjectPath, &saveError)) {
        return 10;
    }

    wave::MainWindow window(
        *restored.project,
        request.resultProjectPath,
        nullptr,
        std::nullopt,
        true);
    window.resize(1'360, 820);
    window.show();
    application.processEvents();
    auto* runAll = window.findChild<QAction*>(
        QStringLiteral("RunAllSimulationScenariosAction"));
    auto* table = window.findChild<QTableWidget*>(
        QStringLiteral("SimulationBatchResultTable"));
    if (!runAll || !runAll->isEnabled() || !table) return 11;

    QEventLoop loop;
    QTimer watchdog;
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);
    bool completed = false;
    int succeededCount = -1;
    int failedCount = -1;
    int cancelledCount = -1;
    QObject::connect(
        &window,
        &wave::MainWindow::simulationBatchFinished,
        &loop,
        [&](const int succeeded, const int failed, const int cancelled) {
            succeededCount = succeeded;
            failedCount = failed;
            cancelledCount = cancelled;
            completed = succeeded == 2 && failed == 0 && cancelled == 0;
            loop.quit();
        });
    runAll->trigger();
    watchdog.start(20'000);
    loop.exec();
    application.processEvents();
    if (!completed || table->rowCount() != 2) {
        std::cerr << "batch summary: " << succeededCount << " passed, "
                  << failedCount << " failed, " << cancelledCount
                  << " cancelled; rows=" << table->rowCount() << '\n';
        for (int row = 0; row < table->rowCount(); ++row) {
            std::cerr << "row " << row << ": ";
            for (int column = 0; column < table->columnCount(); ++column) {
                if (column) std::cerr << " | ";
                if (const auto* item = table->item(row, column))
                    std::cerr << item->text().toStdString();
            }
            std::cerr << '\n';
        }
        return 12;
    }
    if (!table->item(0, 0) || !table->item(1, 0)
        || table->item(0, 0)->text() != QStringLiteral("Passed")
        || table->item(1, 0)->text() != QStringLiteral("Passed")) {
        return 13;
    }
    if (!table->item(0, 3) || !table->item(1, 3)
        || table->item(0, 3)->text() != QStringLiteral("Built")
        || table->item(1, 3)->text() != QStringLiteral("Cached")) {
        return 14;
    }
    if (!runAll->isEnabled()) return 15;
    const auto reloaded = wave::loadProjectFile(request.resultProjectPath);
    if (!reloaded.ok() || reloaded.project->importedTraces.size() != 1) return 16;
    const auto session = wave::simulationSessionFromProject(*reloaded.project);
    if (!session.ok()
        || QFileInfo(session.request->resultProjectPath).absoluteFilePath()
            != QFileInfo(request.resultProjectPath).absoluteFilePath()) {
        return 17;
    }
    const auto storedTrace = QString::fromStdString(
        reloaded.project->importedTraces.front().path);
    const auto resolvedTrace = QFileInfo(storedTrace).isAbsolute()
        ? QFileInfo(storedTrace).absoluteFilePath()
        : QFileInfo(request.resultProjectPath)
              .absoluteDir()
              .absoluteFilePath(storedTrace);
    if (!QFileInfo::exists(resolvedTrace)) return 18;

    const auto outputPath = application.arguments().at(1);
    QDir().mkpath(QFileInfo(outputPath).absolutePath());
    const auto image = window.grab().toImage();
    if (!image.save(outputPath)) return 19;
    return image.width() >= 1'200 && image.height() >= 700 ? 0 : 20;
}
