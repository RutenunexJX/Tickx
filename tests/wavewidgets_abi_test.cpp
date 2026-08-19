#include "main_window.h"
#include "trace_canvas.h"
#include "trace_signal_browser.h"
#include "wave_canvas.h"
#include "wave/project_io.h"
#include "wave/widgets.h"

#include <QApplication>
#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QLineEdit>
#include <QMenu>
#include <QTableWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QWidget>

#include <array>
#include <iostream>

namespace {
int failures = 0;

void check(const bool condition, const char* message)
{
    if (condition) return;
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
}
} // namespace

int main(int argc, char** argv)
{
    QApplication application(argc, argv);
    const QString projectPath = QDir(QString::fromUtf8(WAVE_SOURCE_DIR))
        .absoluteFilePath(QStringLiteral("examples/handshake/project.wave.json"));
    const auto loaded = wave::loadProjectFile(projectPath);
    check(loaded.ok(), "reference project contract loads");

    QWidget owner;
    QWidget* workspace = nullptr;
    std::array<char, 2048> error{};
    const int result = wavewidgets_create_simulation_workspace_v1(
        projectPath.toUtf8().constData(),
        &owner,
        &workspace,
        error.data(),
        error.size());
    check(wavewidgets_abi_version() == wave::kWaveWidgetsAbiVersion,
          "runtime ABI matches the public contract");
    check(result == 0 && workspace,
          error.front() ? error.data() : "embedded workspace is created");
    if (workspace) {
        check(!workspace->isWindow(),
              "embedded workspace is a child widget rather than a top-level window");
        check(workspace->property("wavewidgets.contract").toString()
                  == QString::fromLatin1(wave::kSimulationWorkspaceContract),
              "embedded workspace publishes its stable contract");
        check(!loaded.ok()
                  || workspace->property("wavewidgets.projectId").toString()
                         == QString::fromStdString(loaded.project->id),
              "standalone and embedded paths consume the same project identity");
        check(workspace->property("wavewidgets.capabilities")
                  .toStringList()
                  .contains(QStringLiteral("internal-signal-hierarchy/v1")),
              "embedded workspace advertises hierarchy browsing capability");
        check(workspace->property("wavewidgets.capabilities")
                  .toStringList()
                  .contains(QStringLiteral("multi-clock-async-events/v1")),
              "embedded workspace advertises multi-clock asynchronous timing");
        check(workspace->property("wavewidgets.capabilities")
                  .toStringList()
                  .contains(QStringLiteral("expected-actual-compare/v1")),
              "embedded workspace advertises expected/actual comparison");
        check(workspace->findChild<wave::WaveCanvas*>(
                  QStringLiteral("StimulusCanvas")),
              "embedded workspace exposes the shared stimulus canvas");
        check(workspace->findChild<wave::TraceCanvas*>(
                  QStringLiteral("ActualTraceCanvas")),
              "embedded workspace exposes the shared trace canvas");
        check(workspace->findChild<wave::TraceSignalBrowser*>(
                  QStringLiteral("TraceSignalBrowser")),
              "embedded workspace exposes the internal signal hierarchy browser");
        check(workspace->findChild<QAction*>(
                  QStringLiteral("RunSimulationCompareAction"))
                  && workspace->findChild<QTableWidget*>(
                      QStringLiteral("CompareResultTable"))
                  && workspace->findChild<QWidget*>(
                      QStringLiteral("SimulationComparisonPanel")),
              "embedded workspace exposes comparison controls and results");
        auto* stimulusCanvas = workspace->findChild<wave::WaveCanvas*>(
            QStringLiteral("StimulusCanvas"));
        auto* asyncTiming = workspace->findChild<QAction*>(
            QStringLiteral("AsyncTimingAction"));
        auto* clockDomains = workspace->findChild<QToolButton*>(
            QStringLiteral("SimulationClockDomainsButton"));
        check(asyncTiming && stimulusCanvas,
              "embedded workspace exposes asynchronous event timing");
        if (asyncTiming && stimulusCanvas) {
            asyncTiming->setChecked(true);
            check(stimulusCanvas->asynchronousEditing(),
                  "asynchronous timing action enables one-tick editing");
        }
        check(clockDomains && clockDomains->menu()
                  && !clockDomains->menu()->actions().isEmpty(),
              "embedded workspace exposes independent clock-domain editing");
        workspace->close();
        delete workspace;
    }

    wave::TraceIndex trace;
    trace.traceSignals = {
        {"top.u_core.state", "!", "top.u_core", {"top", "u_core"},
         "state", "top.u_core.state", 2, {}},
        {"top.u_io.ready", "\"", "top.u_io", {"top", "u_io"},
         "ready", "top.u_io.ready", 1, {}},
    };
    wave::TraceSignalBrowser browser;
    browser.setTrace(&trace, {"top.u_core.state"});
    auto* tree = browser.findChild<QTreeWidget*>(
        QStringLiteral("TraceHierarchyTree"));
    auto* search = browser.findChild<QLineEdit*>(
        QStringLiteral("TraceHierarchySearch"));
    check(tree && search, "hierarchy browser exposes its tree and search field");
    check(tree && tree->topLevelItemCount() == 1
              && tree->topLevelItem(0)->text(0) == QStringLiteral("top")
              && tree->topLevelItem(0)->childCount() == 2,
          "hierarchy browser groups trace signals by exact scope");

    QTreeWidgetItem* stateItem = nullptr;
    QTreeWidgetItem* readyItem = nullptr;
    if (tree) {
        for (QTreeWidgetItemIterator iterator(tree); *iterator; ++iterator) {
            if ((*iterator)->text(0) == QStringLiteral("state")) {
                stateItem = *iterator;
            } else if ((*iterator)->text(0) == QStringLiteral("ready")) {
                readyItem = *iterator;
            }
        }
    }
    check(stateItem && stateItem->checkState(0) == Qt::Checked
              && readyItem && readyItem->checkState(0) == Qt::Unchecked,
          "hierarchy browser preserves the initial visible signal set");
    QStringList emittedIds;
    QObject::connect(
        &browser,
        &wave::TraceSignalBrowser::visibleSignalIdsChanged,
        [&emittedIds](const QStringList& ids) { emittedIds = ids; });
    if (readyItem) readyItem->setCheckState(0, Qt::Checked);
    check(emittedIds.contains(QStringLiteral("top.u_core.state"))
              && emittedIds.contains(QStringLiteral("top.u_io.ready"))
              && browser.visibleSignalIds().size() == 2,
          "checking an internal signal updates the visible trace selection");
    if (search) search->setText(QStringLiteral("ready"));
    check(stateItem && stateItem->isHidden()
              && readyItem && !readyItem->isHidden(),
          "hierarchy search filters leaves while retaining matching ancestors");

    std::cout << "wavewidgets ABI failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
