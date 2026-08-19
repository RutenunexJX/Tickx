#pragma once

#include "wave/widgets_export.h"

#include "wave/commands.h"
#include "wave/compare.h"
#include "wave/model.h"
#include "wave/simulation_session.h"
#include "wave/stimulus_scenario.h"
#include "wave/trace.h"

#include <QFutureWatcher>
#include <QByteArray>
#include <QMainWindow>
#include <QPair>

#include <QStringList>
#include <QString>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>

class QAction;
class QCloseEvent;
class QComboBox;
class QEvent;
class QCheckBox;
class QPoint;
class QLabel;
class QLineEdit;
class QMenu;
class QProgressBar;
class QSplitter;
class QTableWidget;
class QTabWidget;
class QToolButton;
class QTimer;
class QWidget;
class QTreeWidget;

namespace wave {

class WaveCanvas;
class TraceCanvas;
class TraceSignalBrowser;

[[nodiscard]] WAVEWIDGETS_API QString preferredProjectLoadPath(
    const QString& requestedPath);
[[nodiscard]] WAVEWIDGETS_API QString untitledRecoveryPath();

class WAVEWIDGETS_API MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(
        Project project,
        QString projectFile = {},
        QWidget* parent = nullptr,
        std::optional<std::size_t> initialScenarioIndex = std::nullopt,
        bool loadFirstTrace = false);
    ~MainWindow() override;

    [[nodiscard]] const Project& project() const noexcept;
    void requestCompareMode();
    void revealLocation(const QString& laneId, Tick tick);
    void openLanePropertiesPreview(const QString& laneId);
    void openEditMenuPreview();

signals:
    void initialTraceReferenceLoaded(bool success, const QString& message);
    void simulationSessionStateChanged(const QString& state);

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void newProject();
    void openProject();
    void saveProject();
    void saveProjectAs();
    void undo();
    void redo();
    void markEdited();
    void updateSelection(const QString& laneId, qint64 tick);
    void updateCommandActions();
    void addLane();
    void showHiddenLanes();
    void showHiddenLane(const QString& laneId);
    void addGroup();
    void editSelectedLane();
    void removeSelectedLane();
    void moveSelectedLaneUp();
    void moveSelectedLaneDown();
    void editSelectedClock();
    void addEvent();
    void removeSelectedEvent();
    void eventCellChanged(int row, int column);
    void revealSelectedEvent();
    void selectEventRow(const QString& eventId);
    void revealValidationIssue(int row, int column);
    void relationCellChanged(int row, int column);
    void removeSelectedRelation();
    void exportArtifacts();
    void importTrace();
    void cancelTraceImport();
    void alignImportedTrace();
    void autoMapImportedTrace();
    void traceMappingCellChanged(int row, int column);
    void finishTraceImport();
    void runSimulation();
    void rerunSimulation();
    void stopSimulation();
    void runCompare();
    void revealCompareDifference(int row, int column);
    void exportCompareReport();
    void exportPinloomEntry();
    void startAutosave();
    void finishAutosave();

private:
    struct ProjectFileRevision {
        enum class State {
            Missing,
            Present,
            Unreadable,
        };

        State state{State::Missing};
        QByteArray sha256;
    };

    [[nodiscard]] static ProjectFileRevision projectFileRevision(
        const QString& path);
    void createActions();
    void createToolBars();
    void populateScenarioSelector();
    bool switchActiveScenario(std::size_t index, bool announce = true);
    void switchAdjacentScenario(bool forward);
    bool selectScenarioForHistoryState(std::uint64_t stateId);
    void updateScenarioNavigationActions();
    void rememberActiveScenario();
    void scheduleActiveScenarioLocationMemory();
    void rememberActiveScenarioLocation();
    [[nodiscard]] std::optional<QString> restoreActiveScenarioLocation();
    [[nodiscard]] std::optional<std::size_t>
    rememberedActiveScenarioIndex();
    [[nodiscard]] QString scenarioPreferenceKey() const;
    [[nodiscard]] QString scenarioLocationPreferenceKey() const;
    [[nodiscard]] QString activeScenarioLabel() const;
    void showSignalFind();
    void closeSignalFind(bool announce = true);
    void updateSignalFind();
    void stepSignalFind(int direction);
    [[nodiscard]] QStringList matchingVisibleSignals(const QString& query) const;
    void activateSignalFindMatch(const QStringList& matches, int index, bool wrapped);
    void showGoToTime();
    void closeGoToTime(bool announce = true);
    void submitGoToTime();
    void syncGoToTimeEditor(bool replaceInput);
    void createDocks();
    void populateSignalTree();
    void populateClockTree();
    void populateGroupTree();
    void populateLinkedResources();
    void populateBottomTables();
    void populateRelationTable();
    void populateValidationTable();
    void populateTraceMappingTable();
    void populateCompareTable();
    [[nodiscard]] QString selectedLaneIdForEditing() const;
    [[nodiscard]] bool commitPendingEdits();
    void addQuickLane(LaneKind kind);
    void completeQuickLaneSetup(
        const QString& laneId,
        const QString& name,
        const QString& parameter,
        const QString& clockId);
    void cancelQuickLaneSetup(const QString& laneId);
    void completeLaneRename(const QString& laneId, const QString& name);
    void changeScenarioDuration(const QString& value);
    [[nodiscard]] Tick latestContentTick(const Scenario& scenario) const noexcept;
    void duplicateSelectedLane();
    void duplicateLaneById(const QString& laneId);
    void duplicateLanesById(const QStringList& laneIds);
    void hideSelectedLane();
    void hideLaneById(const QString& laneId);
    void hideLanesById(const QStringList& laneIds);
    void renameLaneById(const QString& laneId);
    void removeLaneById(const QString& laneId);
    void removeLanesById(const QStringList& laneIds);
    void setLaneGroupById(const QString& laneId, const QString& groupId);
    void setLanesGroupById(const QStringList& laneIds, const QString& groupId);
    void createGroupWithLane(const QString& laneId);
    void createGroupWithLanes(const QStringList& laneIds);
    void showLaneContextMenu(const QString& laneId, const QPoint& globalPosition);
    void editLaneKeyParameters(const QString& laneId);
    void moveSelectedLaneBy(int offset);
    [[nodiscard]] std::optional<std::size_t>
    batchLaneStepInsertionSlot(
        const Scenario& scenario,
        const QStringList& laneIds,
        int offset) const;
    void updateLaneOrderActions();
    void updateWaveContext();
    void updateSegmentActions();
    void editLaneById(const QString& laneId);
    void invalidateCompareResult();
    void scheduleAutosave();
    [[nodiscard]] QString synchronizeDirtyState();
    void resetEditTracking(bool clean);
    void updateWindowTitle();
    void updateRecentProjectsMenu();
    void rememberProjectPath(const QString& path);
    void openRecentProject(const QString& path);
    void openProjectPath(const QString& path);
    [[nodiscard]] QString projectDialogDirectory() const;
    bool loadFromPath(const QString& path, bool preferRecovery = true);
    bool writeToPath(const QString& path);
    void isolateAbandonedRecoverySnapshotsAfterReload();
    bool discardRecoverySnapshots();
    bool confirmDiscardChanges();
    [[nodiscard]] Scenario* activeScenario() noexcept;
    [[nodiscard]] const Scenario* activeScenario() const noexcept;
    [[nodiscard]] ImportedTrace* activeTraceReference() noexcept;
    [[nodiscard]] const ImportedTrace* activeTraceReference() const noexcept;
    [[nodiscard]] QString resolvedTracePath(const ImportedTrace& trace) const;
    [[nodiscard]] QString storedTracePath(const QString& absolutePath) const;
    void startTraceImport(
        const QString& path,
        TraceFormat format,
        std::string traceId,
        Tick offset,
        bool addReference);
    void initializeTraceVisibility(
        const std::optional<std::set<std::string>>& preferred = std::nullopt);
    void refreshTraceViews();
    void loadFirstTraceReference();
    void configureSimulationSession();
    void loadStoredSimulationScenarios();
    [[nodiscard]] StimulusScenarioViewState activeSimulationViewState() const;
    [[nodiscard]] bool persistActiveSimulationScenario(QString* error = nullptr);
    void createSimulationScenario();
    void renameSimulationScenario();
    void deleteSimulationScenario();
    void updateSimulationScenarioActions();
    void updateSimulationClockButton();
    void editClockById(const std::string& clockId);
    void updateSimulationControls(const QString& detail = {});
    [[nodiscard]] bool exportSimulationStimulus(QString& error);
    void finishSimulationRun(SimulationRunReport report);
    [[nodiscard]] bool applySimulationResult(
        SimulationRunReport& report,
        QString& error);

    Project project_;
    QString projectFile_;
    std::size_t activeScenarioIndex_{0};
    bool dirty_{false};
    bool recoveryLoaded_{false};
    bool quickLaneDirtyBefore_{false};
    std::size_t pendingQuickCommandSize_{0};
    QString pendingQuickLaneId_;
    CommandStack commandStack_;
    std::optional<std::uint64_t> cleanCommandStateId_;
    std::uint64_t observedCommandStateId_{0};
    std::map<std::uint64_t, std::size_t> commandScenarioIndices_;
    std::uint64_t externalRevision_{0};
    std::uint64_t cleanExternalRevision_{0};
    WaveCanvas* canvas_{nullptr};
    QTreeWidget* signalTree_{nullptr};
    QTreeWidget* clockTree_{nullptr};
    QTreeWidget* groupTree_{nullptr};
    QTreeWidget* resourceTree_{nullptr};
    QTreeWidget* inspectorTree_{nullptr};
    QTableWidget* eventTable_{nullptr};
    QTableWidget* relationTable_{nullptr};
    QTableWidget* validationTable_{nullptr};
    QTableWidget* traceMappingTable_{nullptr};
    TraceCanvas* traceCanvas_{nullptr};
    TraceCanvas* compareTraceCanvas_{nullptr};
    TraceSignalBrowser* traceSignalBrowser_{nullptr};
    QSplitter* simulationResultSplitter_{nullptr};
    QSplitter* simulationActualSplitter_{nullptr};
    QTabWidget* bottomTabs_{nullptr};
    QWidget* tracePanel_{nullptr};
    QWidget* comparePanel_{nullptr};
    QTableWidget* compareTable_{nullptr};
    QComboBox* compareXCombo_{nullptr};
    QLineEdit* compareToleranceEdit_{nullptr};
    QLineEdit* compareMaskEdit_{nullptr};
    QCheckBox* compareRelationOnly_{nullptr};
    QCheckBox* compareSelectionOnly_{nullptr};
    QLabel* compareSummary_{nullptr};
    QProgressBar* traceProgress_{nullptr};
    QLabel* traceSummary_{nullptr};
    QLabel* saveStateLabel_{nullptr};
    QLabel* pointerStatusLabel_{nullptr};
    QLabel* scenarioSelectorLabel_{nullptr};
    QComboBox* scenarioSelector_{nullptr};
    QAction* scenarioSelectorLabelAction_{nullptr};
    QAction* scenarioSelectorAction_{nullptr};
    QAction* scenarioSelectorSeparatorAction_{nullptr};
    QLabel* waveTargetLabel_{nullptr};
    QAction* undoAction_{nullptr};
    QAction* redoAction_{nullptr};
    QAction* previousScenarioAction_{nullptr};
    QAction* nextScenarioAction_{nullptr};
    QAction* cutRangeAction_{nullptr};
    QAction* duplicateLaneAction_{nullptr};
    QAction* hideLaneAction_{nullptr};
    QAction* removeLaneAction_{nullptr};
    QAction* moveLaneUpAction_{nullptr};
    QAction* moveLaneDownAction_{nullptr};
    QAction* showHiddenLanesAction_{nullptr};
    QAction* showRelationsAction_{nullptr};
    QWidget* signalFindWidget_{nullptr};
    QLineEdit* signalFindEdit_{nullptr};
    QLabel* signalFindResultLabel_{nullptr};
    QToolButton* signalFindPreviousButton_{nullptr};
    QToolButton* signalFindNextButton_{nullptr};
    QToolButton* signalFindCloseButton_{nullptr};
    int signalFindMatchIndex_{-1};
    QWidget* goToTimeWidget_{nullptr};
    QLabel* goToTimeLabel_{nullptr};
    QLineEdit* goToTimeEdit_{nullptr};
    QLabel* goToTimeRangeLabel_{nullptr};
    QToolButton* goToTimeOtherEdgeButton_{nullptr};
    QToolButton* goToTimeGoButton_{nullptr};
    QToolButton* goToTimeCloseButton_{nullptr};
    QAction* goToTimeWidgetAction_{nullptr};
    bool goToTimeEditsRange_{false};
    bool goToTimeEditsRangeWidth_{false};
    QAction* selectAction_{nullptr};
    QAction* drawAction_{nullptr};
    QAction* markerAction_{nullptr};
    QAction* asyncTimingAction_{nullptr};
    QAction* waveTargetAction_{nullptr};
    QAction* selectSegmentAtCursorAction_{nullptr};
    QAction* previousSegmentAction_{nullptr};
    QAction* nextSegmentAction_{nullptr};
    QAction* duplicateSegmentBeforeAction_{nullptr};
    QAction* duplicateSegmentAfterAction_{nullptr};
    QAction* moveSegmentEarlierAction_{nullptr};
    QAction* moveSegmentLaterAction_{nullptr};
    QAction* expandSegmentStartAction_{nullptr};
    QAction* trimSegmentStartAction_{nullptr};
    QAction* expandSegmentEndAction_{nullptr};
    QAction* trimSegmentEndAction_{nullptr};
    QAction* rangeEditPaletteAction_{nullptr};
    QAction* relationAction_{nullptr};
    QAction* exportAction_{nullptr};
    QAction* importTraceAction_{nullptr};
    QAction* cancelTraceAction_{nullptr};
    QAction* runSimulationAction_{nullptr};
    QAction* stopSimulationAction_{nullptr};
    QAction* rerunSimulationAction_{nullptr};
    QAction* createSimulationScenarioAction_{nullptr};
    QAction* renameSimulationScenarioAction_{nullptr};
    QAction* deleteSimulationScenarioAction_{nullptr};
    QToolButton* simulationClockButton_{nullptr};
    QAction* compareModeAction_{nullptr};
    QMenu* editMenu_{nullptr};
    QMenu* segmentMenu_{nullptr};
    QMenu* recentProjectsMenu_{nullptr};
    QAction* signalFindAction_{nullptr};
    QAction* signalFindWidgetAction_{nullptr};
    QAction* goToTimeAction_{nullptr};
    QAction* pinloomAction_{nullptr};
    bool populatingTables_{false};
    bool populatingTraceMapping_{false};
    std::optional<TraceIndex> traceIndex_;
    std::optional<CompareResult> compareResult_;
    std::set<std::string> traceVisibleSignalIds_;
    bool traceVisibilityCustomized_{false};
    std::string activeTraceId_;
    QString pendingTracePath_;
    std::string pendingTraceId_;
    TraceFormat pendingTraceFormat_{TraceFormat::Vcd};
    Tick pendingTraceOffset_{0};
    bool pendingTraceAddsReference_{false};
    std::uint64_t traceGeneration_{0};
    std::shared_ptr<std::atomic_bool> traceCancelFlag_;
    QFutureWatcher<std::shared_ptr<TraceParseResult>>* traceWatcher_{nullptr};
    QTimer* scenarioLocationMemoryTimer_{nullptr};
    QTimer* autosaveTimer_{nullptr};
    QFutureWatcher<QPair<quint64, QString>>* autosaveWatcher_{nullptr};
    quint64 autosaveGeneration_{0};
    QString autosaveInFlightPath_;
    std::set<QString> discardedAutosavePaths_;
    bool autosavePending_{false};
    bool reloadTraceAfterCurrent_{false};
    bool compareModeRequested_{false};
    bool simulationResultMode_{false};
    QLabel* simulationStateLabel_{nullptr};
    std::optional<SimulationRunRequest> simulationRequest_;
    SimulationSessionStateMachine simulationStateMachine_;
    std::unique_ptr<VerilatorSimulationRunner> simulationRunner_;
    QString simulationSessionError_;
    QString simulationStateDetail_;
    QString simulationScenarioDirectory_;
    std::string defaultSimulationScenarioId_;
    std::map<std::string, StimulusScenarioViewState> simulationScenarioViews_;
    quint64 simulationGeneration_{0};
    bool simulationStopRequested_{false};
    QString pendingRevealLaneId_;
    std::optional<Tick> pendingRevealTick_;
    ProjectFileRevision loadedProjectRevision_;
};

} // namespace wave
