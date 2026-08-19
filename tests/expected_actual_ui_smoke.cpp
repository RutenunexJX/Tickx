#include "main_window.h"
#include "wave_canvas.h"

#include "wave/simulation_pipeline.h"
#include "wave/stimulus_scenario.h"
#include "wave/project_io.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>

#include <algorithm>
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

bool writeFile(const QString& path, const QByteArray& document)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(document) == document.size();
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
    const auto parsedStimulus = wave::parseZeroSlackStimulusScenario(
        readFile(fixtureRoot.filePath(QStringLiteral("stimulus.json"))));
    if (!parsedStimulus.ok()) return 3;

    auto expectedScenario = *parsedStimulus.scenario;
    expectedScenario.schemaVersion =
        wave::ZeroSlackStimulusScenario::CurrentSchemaVersion;
    const auto count = std::find_if(
        expectedScenario.ports.begin(),
        expectedScenario.ports.end(),
        [](const wave::StimulusScenarioPort& port) {
            return port.binding.name == "count_o";
        });
    if (count == expectedScenario.ports.end()
        || count->role != wave::StimulusPortRole::Watch) {
        return 4;
    }
    count->expectedSegments = {{0, expectedScenario.duration, "0x0"}};

    QTemporaryDir artifacts;
    if (!artifacts.isValid()) return 5;
    const auto stimulusPath = artifacts.filePath(QStringLiteral("expected.json"));
    const auto resultProjectPath = artifacts.filePath(
        QStringLiteral("expected-actual.wave.json"));
    if (!writeFile(
            stimulusPath,
            wave::serializeZeroSlackStimulusScenario(expectedScenario))) {
        return 6;
    }

    wave::SimulationRunRequest request;
    request.manifestPath = manifestPath;
    request.stimulusPath = stimulusPath;
    request.workspaceRoot = fixtureRoot.absolutePath();
    request.artifactDirectory = artifacts.filePath(QStringLiteral("run"));
    request.resultProjectPath = resultProjectPath;
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
    QEventLoop runLoop;
    QTimer watchdog;
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &runLoop, &QEventLoop::quit);
    std::optional<wave::SimulationRunReport> report;
    if (!runner.start(std::move(request), [&](wave::SimulationRunReport completed) {
            report = std::move(completed);
            runLoop.quit();
        })) {
        return 7;
    }
    watchdog.start(8'000);
    runLoop.exec();
    if (!report || !report->ok()) return 8;

    auto loaded = wave::loadProjectFile(resultProjectPath);
    if (!loaded.ok()) return 9;
    wave::MainWindow window(
        std::move(*loaded.project), resultProjectPath, nullptr, std::nullopt, true);
    window.resize(1'420, 920);

    QEventLoop traceLoop;
    bool traceLoaded = false;
    QObject::connect(
        &window,
        &wave::MainWindow::initialTraceReferenceLoaded,
        &traceLoop,
        [&](const bool success, const QString&) {
            traceLoaded = success;
            traceLoop.quit();
        });
    window.show();
    watchdog.start(5'000);
    traceLoop.exec();
    if (!traceLoaded) return 10;

    auto* compare = window.findChild<QAction*>(
        QStringLiteral("RunSimulationCompareAction"));
    auto* table = window.findChild<QTableWidget*>(
        QStringLiteral("CompareResultTable"));
    auto* expectedCanvas = window.findChild<wave::WaveCanvas*>(
        QStringLiteral("StimulusCanvas"));
    if (!compare || !compare->isEnabled() || !table || !expectedCanvas) return 11;
    compare->trigger();
    application.processEvents();
    if (window.property("wavewidgets.comparisonStatus").toString()
            != QStringLiteral("mismatch")
        || window.property("wavewidgets.compareDifferenceCount").toULongLong() == 0
        || table->rowCount() == 0) {
        return 12;
    }

    QImage image(window.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    window.render(&painter);
    painter.end();
    const auto outputPath = application.arguments().at(1);
    QDir().mkpath(QFileInfo(outputPath).absolutePath());
    if (!image.save(outputPath)) return 13;

    const auto background = image.pixelColor(0, 0);
    qsizetype differingPixels = 0;
    for (int y = 0; y < image.height(); y += 2) {
        for (int x = 0; x < image.width(); x += 2) {
            if (image.pixelColor(x, y) != background) ++differingPixels;
        }
    }
    return differingPixels > 4'000 ? 0 : 14;
}
