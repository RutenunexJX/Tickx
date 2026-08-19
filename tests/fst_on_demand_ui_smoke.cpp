#include "main_window.h"
#include "trace_signal_browser.h"

#include "wave/model.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QLabel>
#include <QPixmap>
#include <QStatusBar>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    if (application.arguments().size() != 2) return 2;

    auto project = wave::makeDemonstrationProject();
    project.name = "FST on-demand trace";
    project.timeBase = {1'000'000'000'000};
    project.importedTraces.clear();
    if (project.scenarios.empty() || project.scenarios.front().lanes.empty()) return 3;
    auto& scenario = project.scenarios.front();
    scenario.duration = 800;

    wave::ImportedTrace trace;
    trace.id = "fst-on-demand";
    trace.path = QDir(QStringLiteral(WAVE_SOURCE_DIR))
                     .filePath(QStringLiteral(
                         "tests/fixtures/traces/wellen-counter.fst"))
                     .toStdString();
    trace.format = "fst";
    trace.signalMapping[scenario.lanes.front().id] = "tb.dut.counter";
    project.importedTraces.push_back(std::move(trace));

    wave::MainWindow window(
        std::move(project),
        {},
        nullptr,
        std::nullopt,
        true,
        QStringLiteral(WAVE_WELLEN_READER));
    window.resize(1'420, 900);
    window.show();

    QElapsedTimer elapsed;
    elapsed.start();
    QEventLoop loop;
    QTimer poll;
    poll.setInterval(25);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (window.property("wavewidgets.fstRequiredSignalsLoaded").toBool()
            || elapsed.elapsed() > 8'000) {
            loop.quit();
        }
    });
    poll.start();
    loop.exec();

    if (!window.property("wavewidgets.fstRequiredSignalsLoaded").toBool()
        || window.property("wavewidgets.fstSignalCount").toULongLong() != 8
        || window.property("wavewidgets.fstLoadedSignalCount").toULongLong() != 1) {
        return 4;
    }
    auto* browser = window.findChild<wave::TraceSignalBrowser*>(
        QStringLiteral("TraceSignalBrowser"));
    auto* tree = window.findChild<QTreeWidget*>(
        QStringLiteral("TraceHierarchyTree"));
    auto* summary = window.findChild<QLabel*>(
        QStringLiteral("TraceHierarchySummary"));
    if (!browser) return 50;
    if (!tree) return 51;
    if (tree->topLevelItemCount() == 0) return 52;
    if (!summary) return 53;
    if (!summary->text().contains(QStringLiteral("1 loaded"))) return 54;

    QTreeWidgetItem* secondSignal = nullptr;
    for (QTreeWidgetItemIterator iterator(tree); *iterator; ++iterator) {
        if ((*iterator)->data(0, Qt::UserRole + 1).toString()
            == QStringLiteral("tb.dut.overflow")) {
            secondSignal = *iterator;
            break;
        }
    }
    if (!secondSignal) return 55;
    secondSignal->setCheckState(0, Qt::Checked);
    elapsed.restart();
    QEventLoop secondLoad;
    QObject::disconnect(&poll, nullptr, &loop, nullptr);
    QObject::connect(&poll, &QTimer::timeout, &secondLoad, [&] {
        if ((window.property("wavewidgets.fstLoadedSignalCount").toULongLong() == 2
             && !window.statusBar()->currentMessage().startsWith(
                 QStringLiteral("Loading")))
            || elapsed.elapsed() > 8'000) {
            secondLoad.quit();
        }
    });
    secondLoad.exec();
    if (window.property("wavewidgets.fstLoadedSignalCount").toULongLong() != 2
        || !summary->text().contains(QStringLiteral("2 loaded"))) {
        return 56;
    }

    const auto screenshot = QFileInfo(application.arguments().at(1)).absoluteFilePath();
    QDir().mkpath(QFileInfo(screenshot).absolutePath());
    if (!window.grab().save(screenshot)) return 6;
    return 0;
}
