#include "main_window.h"

#include "wave/module_manifest.h"
#include "wave/project_io.h"
#include "wave/simulation_session.h"
#include "wave/stimulus_scenario.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>

#include <optional>

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
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

bool writeFile(const QString& path, const QByteArray& content)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(content) == content.size();
}

QJsonObject sourceLink(
    const QString& kind,
    const int line,
    const QString& label)
{
    return {
        {QStringLiteral("kind"), kind},
        {QStringLiteral("sourceFile"),
         QStringLiteral("rtl/wave_fixed_counter.sv")},
        {QStringLiteral("sourceLine"), line},
        {QStringLiteral("sourceColumn"), 5},
        {QStringLiteral("label"), label},
    };
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    if (application.arguments().size() != 2) return 2;

    const QDir fixtureRoot(
        QDir(QStringLiteral(WAVE_SOURCE_DIR))
            .filePath(QStringLiteral("tests/fixtures/simulation/fixed-counter")));
    QTemporaryDir temporary;
    if (!temporary.isValid()) return 3;
    QDir root(temporary.path());

    auto manifestRoot = QJsonDocument::fromJson(
        readFile(fixtureRoot.filePath(QStringLiteral("manifest.json"))))
                            .object();
    manifestRoot.insert(QStringLiteral("schemaVersion"), 5);
    manifestRoot.insert(
        QStringLiteral("observationScope"),
        QJsonObject{
            {QStringLiteral("mode"), QStringLiteral("module")},
            {QStringLiteral("label"), QStringLiteral("wave_fixed_counter")},
            {QStringLiteral("sourceFile"),
             QStringLiteral("rtl/wave_fixed_counter.sv")},
            {QStringLiteral("startLine"), 3},
            {QStringLiteral("endLine"), 18},
        });
    manifestRoot.insert(QStringLiteral("observations"), QJsonArray{});
    manifestRoot.insert(
        QStringLiteral("unresolvedDependencies"), QJsonArray{});

    const QString countSemanticId = QStringLiteral("sha256:")
        + QString(64, QLatin1Char('c'));
    auto ports = manifestRoot.value(QStringLiteral("ports")).toArray();
    for (qsizetype index = 0; index < ports.size(); ++index) {
        auto port = ports.at(index).toObject();
        const auto name = port.value(QStringLiteral("name")).toString();
        const int line = port.value(QStringLiteral("sourceLine")).toInt();
        const auto semanticId = name == QStringLiteral("count_o")
            ? countSemanticId
            : QStringLiteral("sha256:")
                + QString(64, QLatin1Char(static_cast<char>('a' + index)));
        port.insert(QStringLiteral("semanticId"), semanticId);
        port.insert(QStringLiteral("structuredLeavesAvailable"), false);
        port.insert(QStringLiteral("editableLeaves"), QJsonArray{});
        port.insert(QStringLiteral("structuredFailureReason"), QString{});
        port.insert(QStringLiteral("sourceColumn"), 5);
        QJsonArray links{
            sourceLink(QStringLiteral("declaration"), line,
                       QStringLiteral("Declaration")),
        };
        if (name == QStringLiteral("count_o")) {
            links.append(sourceLink(
                QStringLiteral("driver"), 12,
                QStringLiteral("Reset assignment")));
        }
        port.insert(QStringLiteral("sourceLinks"), links);
        ports[index] = port;
    }
    manifestRoot.insert(QStringLiteral("ports"), ports);
    const auto manifestPath = root.filePath(QStringLiteral("manifest-v5.json"));
    if (!writeFile(
            manifestPath,
            QJsonDocument(manifestRoot).toJson(QJsonDocument::Indented))) {
        return 4;
    }

    const auto manifest = wave::parseZeroSlackModuleManifest(
        readFile(manifestPath));
    const auto sourceStimulusPath = fixtureRoot.filePath(
        QStringLiteral("stimulus.json"));
    const auto stimulus = wave::parseZeroSlackStimulusScenario(
        readFile(sourceStimulusPath));
    if (!manifest.ok() || !stimulus.ok()) return 5;
    auto restored = wave::restoreZeroSlackStimulusScenario(
        *manifest.manifest, *stimulus.scenario);
    if (!restored.ok() || restored.project->scenarios.empty()) return 6;

    const auto stimulusPath = root.filePath(QStringLiteral("stimulus.json"));
    if (!QFile::copy(sourceStimulusPath, stimulusPath)) return 7;
    wave::SimulationRunRequest request;
    request.manifestPath = manifestPath;
    request.stimulusPath = stimulusPath;
    request.workspaceRoot = fixtureRoot.absolutePath();
    request.artifactDirectory = root.filePath(QStringLiteral("artifacts"));
    request.buildCacheDirectory = root.filePath(QStringLiteral("build-cache"));
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
        return 8;
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
    auto* run = window.findChild<QAction*>(
        QStringLiteral("RunSimulationAction"));
    auto* source = window.findChild<QAction*>(
        QStringLiteral("SimulationSourceNavigationAction"));
    auto* drivers = window.findChild<QToolButton*>(
        QStringLiteral("SimulationDriverNavigationButton"));
    if (!run || !run->isEnabled() || !source || !drivers) return 9;

    QEventLoop loop;
    QTimer watchdog;
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);
    bool current = false;
    QObject::connect(
        &window,
        &wave::MainWindow::simulationSessionStateChanged,
        &loop,
        [&](const QString& state) {
            current = state == QStringLiteral("current");
            if (current || state == QStringLiteral("failed")) loop.quit();
        });
    run->trigger();
    watchdog.start(10'000);
    loop.exec();
    application.processEvents();
    if (!current) return 10;

    if (!window.revealSourceObject(
            countSemanticId,
            QStringLiteral("rtl/wave_fixed_counter.sv"),
            6,
            5,
            QStringLiteral("count_o"),
            QStringLiteral("count_o"))) {
        return 11;
    }
    application.processEvents();
    if (!source->isEnabled() || !drivers->isEnabled()
        || !drivers->menu() || drivers->menu()->actions().size() != 1) {
        return 12;
    }

    QString requestedKind;
    int requestedLine = 0;
    QObject::connect(
        &window,
        &wave::MainWindow::sourceNavigationRequested,
        &window,
        [&](const QString&, const int line, const int,
            const QString&, const QString& kind) {
            requestedKind = kind;
            requestedLine = line;
        });
    source->trigger();
    if (requestedKind != QStringLiteral("declaration")
        || requestedLine != 6) {
        return 13;
    }
    drivers->menu()->actions().front()->trigger();
    if (requestedKind != QStringLiteral("driver")
        || requestedLine != 12) {
        return 14;
    }

    const auto outputPath = application.arguments().at(1);
    QDir().mkpath(QFileInfo(outputPath).absolutePath());
    const auto image = window.grab().toImage();
    if (!image.save(outputPath)) return 15;
    return image.width() >= 1'200 && image.height() >= 700 ? 0 : 16;
}
