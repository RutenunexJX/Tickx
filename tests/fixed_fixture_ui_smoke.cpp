#include "trace_canvas.h"

#include "wave/module_manifest.h"
#include "wave/simulation_pipeline.h"
#include "wave/stimulus_scenario.h"

#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QTemporaryDir>
#include <QTimer>

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
    const auto stimulusPath = fixtureRoot.filePath(QStringLiteral("stimulus.json"));
    QTemporaryDir artifacts;
    if (!artifacts.isValid()) return 3;

    wave::SimulationRunRequest request;
    request.manifestPath = manifestPath;
    request.stimulusPath = stimulusPath;
    request.workspaceRoot = fixtureRoot.absolutePath();
    request.artifactDirectory = artifacts.path();
    request.toolchain.verilatorProgram = fixtureExecutable(
        QStringLiteral("wave-verilator-fixture"));
    request.toolchain.cxxProgram = fixtureExecutable(
        QStringLiteral("wave-toolchain-fixture-ready"));
    request.toolchain.environment.insert(
        QStringLiteral("WAVE_SIMULATOR_FIXTURE"),
        fixtureExecutable(QStringLiteral("wave-simulator-fixture")));
    request.toolchain.timeoutMs = 1'000;
    request.buildTimeoutMs = 2'000;
    request.runTimeoutMs = 2'000;

    wave::VerilatorSimulationRunner runner;
    QEventLoop loop;
    QTimer watchdog;
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);
    std::optional<wave::SimulationRunReport> report;
    if (!runner.start(std::move(request), [&](wave::SimulationRunReport completed) {
            report = std::move(completed);
            loop.quit();
        })) {
        return 4;
    }
    watchdog.start(8'000);
    loop.exec();
    if (!report || !report->ok() || !report->trace) return 5;

    const auto manifest = wave::parseZeroSlackModuleManifest(readFile(manifestPath));
    const auto stimulus = wave::parseZeroSlackStimulusScenario(readFile(stimulusPath));
    if (!manifest.ok() || !stimulus.ok()) return 6;
    auto restored = wave::restoreZeroSlackStimulusScenario(
        *manifest.manifest, *stimulus.scenario);
    if (!restored.ok() || restored.project->scenarios.empty()) return 7;

    wave::TraceCanvas canvas;
    canvas.resize(1'100, 360);
    canvas.setTrace(
        &*restored.project,
        &restored.project->scenarios.front(),
        &*report->trace,
        nullptr);
    canvas.show();
    application.processEvents();
    canvas.fitTrace();
    application.processEvents();

    QImage image(canvas.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    canvas.render(&painter);
    painter.end();
    const auto outputPath = application.arguments().at(1);
    QDir().mkpath(QFileInfo(outputPath).absolutePath());
    if (!image.save(outputPath)) return 8;

    const auto background = image.pixelColor(0, 0);
    qsizetype differingPixels = 0;
    for (int y = 0; y < image.height(); y += 2) {
        for (int x = 0; x < image.width(); x += 2) {
            if (image.pixelColor(x, y) != background) ++differingPixels;
        }
    }
    return differingPixels > 2'000 ? 0 : 9;
}
