#include "main_window.h"
#include "trace_canvas.h"
#include "trace_signal_browser.h"
#include "wave_canvas.h"
#include "waveform_theme.h"
#include "ui_controls.h"
#include "waveform_view.h"
#include "wave/project_io.h"
#include "wave/widgets.h"

#include <QApplication>
#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QKeyEvent>
#include <QMenu>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
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
        check(workspace->property("wavewidgets.capabilities")
                  .toStringList()
                  .contains(QStringLiteral("lightweight-trace-checks/v1")),
              "embedded workspace advertises lightweight trace checks");
        check(workspace->property("wavewidgets.capabilities")
                  .toStringList()
                  .contains(QStringLiteral("result-source-navigation/v1")),
              "embedded workspace advertises result/source navigation");
        check(workspace->property("wavewidgets.capabilities")
                  .toStringList()
                  .contains(QStringLiteral(
                      "explicit-unresolved-module-stubs/v1")),
              "embedded workspace advertises explicit unresolved-module stubs");
        check(workspace->property("wavewidgets.capabilities")
                  .toStringList()
                  .contains(QStringLiteral(
                      "multi-scenario-batch-run/v1")),
              "embedded workspace advertises multi-scenario batch runs");
#if defined(WAVE_WELLEN_READER)
        check(QFileInfo(QStringLiteral(WAVE_WELLEN_READER)).isFile()
                  && workspace->property("wavewidgets.capabilities")
                         .toStringList()
                         .contains(QStringLiteral("on-demand-fst-trace/v1")),
              "embedded workspace advertises an installed on-demand FST reader");
#else
        check(!workspace->property("wavewidgets.capabilities")
                   .toStringList()
                   .contains(QStringLiteral("on-demand-fst-trace/v1")),
              "embedded workspace does not advertise an unavailable FST reader");
#endif
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
                  && workspace->findChild<QAction*>(
                      QStringLiteral("RunSimulationChecksAction"))
                  && workspace->findChild<QTableWidget*>(
                      QStringLiteral("SimulationCheckResultTable"))
                  && workspace->findChild<QAction*>(
                      QStringLiteral("RunAllSimulationScenariosAction"))
                  && workspace->findChild<QTableWidget*>(
                      QStringLiteral("SimulationBatchResultTable"))
                  && workspace->findChild<QAction*>(
                      QStringLiteral("SimulationSourceNavigationAction"))
                  && workspace->findChild<QToolButton*>(
                      QStringLiteral("SimulationDriverNavigationButton"))
                  && workspace->findChild<QWidget*>(
                      QStringLiteral("SimulationComparisonPanel")),
              "embedded workspace exposes comparison controls and results");
        auto* resultToolbar = workspace->findChild<QToolBar*>(
            QStringLiteral("SimulationResultToolbar"));
        auto* moreButton = workspace->findChild<QToolButton*>(
            QStringLiteral("SimulationMoreButton"));
        auto* resultSplitter = workspace->findChild<QSplitter*>(
            QStringLiteral("SimulationResultSplitter"));
        auto* actualSplitter = workspace->findChild<QSplitter*>(
            QStringLiteral("SimulationActualSplitter"));
        check(resultToolbar && moreButton && moreButton->menu()
                  && moreButton->menu()->objectName()
                         == QStringLiteral("SimulationMoreMenu"),
              "simulation result toolbar exposes a grouped overflow menu");
        if (resultToolbar && moreButton && moreButton->menu()) {
            const auto toolbarActions = resultToolbar->actions();
            const auto onToolbar = [&toolbarActions](QAction* action) {
                return action && toolbarActions.contains(action);
            };
            check(onToolbar(workspace->findChild<QAction*>(
                      QStringLiteral("RunSimulationAction")))
                      && onToolbar(workspace->findChild<QAction*>(
                          QStringLiteral("RunAllSimulationScenariosAction")))
                      && onToolbar(workspace->findChild<QAction*>(
                          QStringLiteral("StopSimulationAction")))
                      && onToolbar(workspace->findChild<QAction*>(
                          QStringLiteral("RunSimulationCompareAction")))
                      && onToolbar(workspace->findChild<QAction*>(
                          QStringLiteral("RunSimulationChecksAction"))),
                  "primary simulation actions remain directly visible");
            check(!onToolbar(workspace->findChild<QAction*>(
                      QStringLiteral("RerunSimulationAction")))
                      && !onToolbar(workspace->findChild<QAction*>(
                          QStringLiteral("CreateSimulationScenarioAction")))
                      && !onToolbar(workspace->findChild<QAction*>(
                          QStringLiteral("SimulationSourceNavigationAction")))
                      && moreButton->menu()->actions().size() >= 10,
                  "secondary simulation actions are confined to overflow");
        }
        check(resultSplitter && actualSplitter
                  && !resultSplitter->childrenCollapsible()
                  && !actualSplitter->childrenCollapsible()
                  && resultSplitter->count() == 3
                  && actualSplitter->count() == 2,
              "simulation pages retain adjustable non-collapsing splitters");
        check(workspace->metaObject()->indexOfMethod(
                  "canRevealSourceObject(QString,QString,int,int,QString,QString)") >= 0
                  && workspace->metaObject()->indexOfMethod(
                      "revealSourceObject(QString,QString,int,int,QString,QString)") >= 0
                  && workspace->metaObject()->indexOfSignal(
                      "sourceNavigationRequested(QString,int,int,QString,QString)") >= 0,
              "embedded workspace exposes the bidirectional source navigation API");
        auto* stimulusCanvas = workspace->findChild<wave::WaveCanvas*>(
            QStringLiteral("StimulusCanvas"));
        auto* asyncTiming = workspace->findChild<QAction*>(
            QStringLiteral("AsyncTimingAction"));
        auto* clockDomains = workspace->findChild<QToolButton*>(
            QStringLiteral("SimulationClockDomainsButton"));
        auto* stubDependencies = workspace->findChild<QToolButton*>(
            QStringLiteral("SimulationStubDependenciesButton"));
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
        check(stubDependencies && stubDependencies->menu()
                  && stubDependencies->text() == QStringLiteral("Stubs (0/0)")
                  && !stubDependencies->isEnabled(),
              "embedded workspace exposes an inert stub selector when all modules resolve");
        workspace->close();
        delete workspace;
    }

    QWidget* waveformWidget = nullptr;
    error.fill('\0');
    const int waveformCreateResult = wavewidgets_create_waveform_view_v1(
        &owner, &waveformWidget, error.data(), error.size());
    check(waveformCreateResult == 0 && waveformWidget,
          error.front() ? error.data() : "waveform-view/v1 is created");
    auto* waveformView = qobject_cast<wave::WaveformView*>(waveformWidget);
    if (waveformView) {
        check(!waveformView->isWindow() && waveformView->parentWidget() == &owner,
              "waveform view is an embeddable child widget");
        check(waveformView->property("wavewidgets.contract").toString()
                  == QString::fromLatin1(wave::kWaveformViewContract)
                  && waveformView->property("wavewidgets.previewContract").toString()
                         == QString::fromLatin1(wave::kWavePreviewPayloadContract),
              "waveform view publishes stable widget and payload contracts");
        check(waveformView->capabilities().contains(QStringLiteral("wave-preview/v1"))
                  && waveformView->capabilities().contains(
                      QStringLiteral("generation-replace/v1"))
                  && waveformView->capabilities().contains(
                      QStringLiteral("source-navigation/v1")),
              "waveform view advertises replace, theme, and navigation capabilities");
        check(waveformView->metaObject()->indexOfMethod(
                  "replacePreviewPayload(QByteArray)") >= 0
                  && waveformView->metaObject()->indexOfMethod(
                      "setThemeName(QString)") >= 0
                  && waveformView->metaObject()->indexOfSignal(
                      "sourceNavigationRequested(QString,int,int,QString,QString)") >= 0,
              "waveform view exposes its stable Qt meta-object contract");

        const QByteArray firstPayload = R"JSON({
          "contract":"wave-preview/v1",
          "generation":7,
          "mode":"symbolic",
          "timebase":{"unit":"ns","start":0,"end":100},
          "lanes":[
            {"id":"top.clk","name":"clk","kind":"clock","width":1,
             "provenance":"zeroslack-symbolic",
             "source":{"file":"rtl/top.sv","line":8,"column":3,"semanticId":"module:top/signal:clk"},
             "segments":[
               {"start":0,"end":10,"value":"0"},
               {"start":10,"end":20,"value":"1"},
               {"start":20,"end":100,"value":"X","unknown":true}]},
            {"id":"top.data","name":"data[7:0]","kind":"bus","width":8,
             "provenance":"zeroslack-symbolic",
             "source":{"file":"rtl/top.sv","line":12,"semanticId":"module:top/signal:data"},
             "segments":[
               {"start":0,"end":40,"value":"0x00"},
               {"start":40,"end":100,"value":"0xA5"}]}
          ]
        })JSON";
        error.fill('\0');
        const int firstSetResult = wavewidgets_set_waveform_preview_v1(
            waveformView,
            firstPayload.constData(),
            static_cast<std::size_t>(firstPayload.size()),
            error.data(),
            error.size());
        check(firstSetResult == 0 && waveformView->previewGeneration() == 7
                  && waveformView->previewMode() == QStringLiteral("symbolic")
                  && waveformView->presentationState() == QStringLiteral("ready")
                  && waveformView->lastError().isEmpty(),
              error.front() ? error.data() : "valid symbolic payload is accepted");
        check(waveformView->selectLane(QStringLiteral("top.data"))
                  && waveformView->revealTick(55),
              "stable lane selection and cursor navigation are accepted");
        check(waveformView->setThemeName(QStringLiteral("dark"))
                  && waveformView->themeName() == QStringLiteral("dark"),
              "embedded waveform theme can be selected explicitly");
        waveformView->setCompact(true);
        check(waveformView->compact(),
              "compact density can be selected for a sidebar host");

        QString sourceFile;
        QString semanticId;
        QObject::connect(
            waveformView,
            &wave::WaveformView::sourceNavigationRequested,
            [&sourceFile, &semanticId](const QString& file,
                                      int,
                                      int,
                                      const QString& semantic,
                                      const QString&) {
                sourceFile = file;
                semanticId = semantic;
            });
        QKeyEvent openSource(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(waveformView, &openSource);
        check(sourceFile == QStringLiteral("rtl/top.sv")
                  && semanticId == QStringLiteral("module:top/signal:data"),
              "selected lane requests navigation through portable source identity");

        const QByteArray replacement = R"JSON({
          "contract":"wave-preview/v1","generation":8,"mode":"symbolic",
          "timebase":{"unit":"ns","start":0,"end":100},
          "lanes":[{"id":"top.data","name":"data[7:0]","kind":"bus","width":8,
            "provenance":"zeroslack-symbolic","segments":[
              {"start":0,"end":50,"value":"0x11"},
              {"start":50,"end":100,"value":"0x22"}]}]
        })JSON";
        error.fill('\0');
        check(wavewidgets_set_waveform_preview_v1(
                  waveformView,
                  replacement.constData(),
                  static_cast<std::size_t>(replacement.size()),
                  error.data(), error.size()) == 0
                  && waveformView->previewGeneration() == 8,
              "newer compatible generation replaces the complete payload");

        error.fill('\0');
        check(wavewidgets_set_waveform_preview_v1(
                  waveformView,
                  firstPayload.constData(),
                  static_cast<std::size_t>(firstPayload.size()),
                  error.data(), error.size()) == 4
                  && waveformView->previewGeneration() == 8
                  && QString::fromUtf8(error.data()).contains(
                      QStringLiteral("Stale")),
              "stale generation is rejected without replacing current data");

        const QByteArray malformed = R"JSON({
          "contract":"wave-preview/v1","generation":9,"mode":"symbolic",
          "timebase":{"unit":"ns","start":0,"end":100},
          "lanes":[{"id":"bad","name":"bad","kind":"bit","width":1,
            "provenance":"test","segments":[
              {"start":0,"end":60,"value":"0"},
              {"start":50,"end":100,"value":"1"}]}]
        })JSON";
        error.fill('\0');
        check(wavewidgets_set_waveform_preview_v1(
                  waveformView,
                  malformed.constData(),
                  static_cast<std::size_t>(malformed.size()),
                  error.data(), error.size()) == 4
                  && waveformView->previewGeneration() == 8
                  && QString::fromUtf8(error.data()).contains(
                      QStringLiteral("overlaps")),
              "overlapping segments are rejected without corrupting current data");

        const QByteArray unknownContract = R"JSON({
          "contract":"wave-preview/v2","generation":9,"mode":"symbolic",
          "timebase":{"unit":"ns","start":0,"end":100},"lanes":[]
        })JSON";
        error.fill('\0');
        check(wavewidgets_set_waveform_preview_v1(
                  waveformView,
                  unknownContract.constData(),
                  static_cast<std::size_t>(unknownContract.size()),
                  error.data(), error.size()) == 4
                  && waveformView->previewGeneration() == 8
                  && QString::fromUtf8(error.data()).contains(
                      QStringLiteral("Unsupported")),
              "unknown preview contract versions are rejected without replacement");

        QByteArray oversizedPayload(8 * 1024 * 1024 + 1, ' ');
        error.fill('\0');
        check(wavewidgets_set_waveform_preview_v1(
                  waveformView,
                  oversizedPayload.constData(),
                  static_cast<std::size_t>(oversizedPayload.size()),
                  error.data(), error.size()) == 2
                  && waveformView->previewGeneration() == 8
                  && QString::fromUtf8(error.data()).contains(
                      QStringLiteral("8 MiB")),
              "oversized preview payloads are rejected before parsing");

        waveformView->resize(960, 720);
        waveformView->show();
        application.processEvents();
        check(!waveformView->grab().isNull(),
              "waveform view renders at the compact 960 by 720 acceptance size");
        waveformView->resize(1440, 900);
        application.processEvents();
        check(!waveformView->grab().isNull(),
              "waveform view renders at the 1440 by 900 acceptance size");
        waveformView->hide();

        check(waveformView->setPresentationState(
                  QStringLiteral("loading"), QStringLiteral("Updating preview"))
                  && waveformView->presentationState()
                         == QStringLiteral("loading")
                  && waveformView->setPresentationState(
                      QStringLiteral("ready"), QStringLiteral("Preview current"))
                  && !waveformView->setPresentationState(
                      QStringLiteral("unsupported")),
              "presentation states are explicit and reject unknown values");

        const auto lightTheme = wave::waveformTheme(wave::WaveformColorScheme::Light);
        const auto darkTheme = wave::waveformTheme(wave::WaveformColorScheme::Dark);
        check(lightTheme.canvas != darkTheme.canvas
                  && lightTheme.gridMajor != darkTheme.gridMajor
                  && lightTheme.unknown != darkTheme.unknown
                  && lightTheme.accent != lightTheme.accentSecondary
                  && darkTheme.accent != darkTheme.accentSecondary
                  && wave::waveApplicationStyleSheet(wave::WaveformColorScheme::Light)
                         != wave::waveApplicationStyleSheet(wave::WaveformColorScheme::Dark),
              "light and dark semantic waveform tokens are distinct");

        const auto metrics = wave::waveformMetrics();
        check(metrics.spacingUnit == 4
                  && metrics.controlHeight == 32
                  && metrics.compactControlHeight == 28
                  && metrics.radius == 6
                  && metrics.panelHeaderHeight == 32
                  && metrics.noticePadding == 8
                  && metrics.focusRingWidth == 2
                  && metrics.controlHeight % metrics.spacingUnit == 0
                  && metrics.compactControlHeight % metrics.spacingUnit == 0
                  && metrics.panelHeaderHeight % metrics.spacingUnit == 0,
              "semantic geometry follows the shared four-pixel spacing grid");

        const auto contrastAcceptable = [](const wave::WaveformTheme& theme) {
            return wave::waveColorContrastRatio(theme.text, theme.canvas) >= 4.5
                && wave::waveColorContrastRatio(theme.text, theme.raised) >= 4.5
                && wave::waveColorContrastRatio(theme.mutedText, theme.canvas) >= 4.5
                && wave::waveColorContrastRatio(
                       theme.selectionText, theme.selection) >= 4.5
                && wave::waveColorContrastRatio(theme.focus, theme.canvas) >= 3.0
                && wave::waveColorContrastRatio(
                       theme.success, theme.successSurface) >= 4.5
                && wave::waveColorContrastRatio(
                       theme.warning, theme.warningSurface) >= 4.5
                && wave::waveColorContrastRatio(
                       theme.error, theme.errorSurface) >= 4.5
                && wave::waveColorContrastRatio(
                       theme.information, theme.informationSurface) >= 4.5;
        };
        check(contrastAcceptable(lightTheme) && contrastAcceptable(darkTheme),
              "light and dark semantic text, status, selection, and focus tokens meet contrast guards");

        for (const auto scheme : {
                 wave::WaveformColorScheme::Light,
                 wave::WaveformColorScheme::Dark}) {
            const auto style = wave::waveApplicationStyleSheet(scheme);
            check(!style.contains(QLatin1Char('@'))
                      && style.contains(QStringLiteral("waveSurface=\"canvas\""))
                      && style.contains(QStringLiteral("wavePanel=\"floating\""))
                      && style.contains(QStringLiteral("waveNotice=\"warning\""))
                      && style.contains(QStringLiteral("waveState=\"empty\""))
                      && style.contains(QStringLiteral("waveState=\"loading\""))
                      && style.contains(QStringLiteral("waveState=\"error\""))
                      && style.contains(QStringLiteral("min-height: 32px"))
                      && !style.contains(QStringLiteral("QLineEdit:focus"))
                      && !style.contains(QStringLiteral("QPushButton, QToolButton")),
                  "semantic stylesheet resolves tokens without overriding Ela control painting");
        }

        application.setStyleSheet(wave::waveApplicationStyleSheet(
            wave::WaveformColorScheme::Dark));
        QWidget semanticHost;
        semanticHost.setObjectName(QStringLiteral("SemanticThemeContractHost"));
        semanticHost.setProperty("waveSurface", QStringLiteral("panel"));
        auto* semanticLayout = new QVBoxLayout(&semanticHost);
        semanticLayout->setContentsMargins(8, 8, 8, 8);
        semanticLayout->setSpacing(metrics.spacingUnit);
        auto* semanticEdit = wave::ui::lineEdit(&semanticHost);
        semanticEdit->setObjectName(QStringLiteral("SemanticFocusEdit"));
        semanticEdit->setAccessibleName(QStringLiteral("Semantic focus editor"));
        auto* semanticButton = wave::ui::button(
            QStringLiteral("Apply"), &semanticHost);
        semanticButton->setObjectName(QStringLiteral("SemanticPrimaryButton"));
        semanticButton->setProperty("waveRole", QStringLiteral("primary"));
        auto* semanticNotice = new QFrame(&semanticHost);
        semanticNotice->setObjectName(QStringLiteral("SemanticWarningNotice"));
        semanticNotice->setProperty("waveNotice", QStringLiteral("warning"));
        auto* semanticNoticeLayout = new QVBoxLayout(semanticNotice);
        auto* semanticNoticeLabel = new QLabel(
            QStringLiteral("External update pending"), semanticNotice);
        semanticNoticeLabel->setProperty("waveState", QStringLiteral("warning"));
        semanticNoticeLayout->addWidget(semanticNoticeLabel);
        semanticLayout->addWidget(semanticEdit);
        semanticLayout->addWidget(semanticButton);
        semanticLayout->addWidget(semanticNotice);
        semanticHost.resize(480, 240);
        semanticHost.show();
        semanticEdit->setFocus(Qt::OtherFocusReason);
        application.processEvents();
        check(semanticEdit->hasFocus()
                  && semanticEdit->height() >= metrics.controlHeight
                  && semanticButton->height() >= metrics.controlHeight
                  && semanticNotice->property("waveNotice").toString()
                         == QStringLiteral("warning")
                  && semanticEdit->accessibleName()
                         == QStringLiteral("Semantic focus editor"),
              "semantic controls retain keyboard focus, accessible naming, and logical geometry");
        semanticHost.hide();

        const auto previousReducedMotion = qgetenv(
            "WAVEWORKBENCH_REDUCED_MOTION");
        qputenv("WAVEWORKBENCH_REDUCED_MOTION", QByteArrayLiteral("1"));
        const auto reducedMotionEnabled = wave::waveReducedMotionEnabled();
        qputenv("WAVEWORKBENCH_REDUCED_MOTION", QByteArrayLiteral("0"));
        const auto reducedMotionDisabled = !wave::waveReducedMotionEnabled();
        if (previousReducedMotion.isNull()) {
            qunsetenv("WAVEWORKBENCH_REDUCED_MOTION");
        } else {
            qputenv("WAVEWORKBENCH_REDUCED_MOTION", previousReducedMotion);
        }
        check(reducedMotionEnabled && reducedMotionDisabled,
              "reduced-motion preference has deterministic environment overrides");
        application.setStyleSheet({});
    }
    QWidget ordinaryWidget;
    const QByteArray trivialPayload("{}");
    error.fill('\0');
    check(wavewidgets_set_waveform_preview_v1(
              &ordinaryWidget,
              trivialPayload.constData(),
              static_cast<std::size_t>(trivialPayload.size()),
              error.data(), error.size()) == 3,
          "preview setter rejects widgets outside the waveform-view/v1 contract");
    delete waveformWidget;

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
