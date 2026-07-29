#pragma once

#include "wave/commands.h"
#include "wave/compare.h"
#include "wave/model.h"
#include "wave/trace.h"

#include <QFutureWatcher>
#include <QMainWindow>
#include <QPair>

#include <QStringList>
#include <QString>

#include <atomic>
#include <cstddef>
#include <cstdint>
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
class QTableWidget;
class QTabWidget;
class QToolButton;
class QTimer;
class QWidget;
class QTreeWidget;

namespace wave {

class WaveCanvas;
class TraceCanvas;

[[nodiscard]] QString preferredProjectLoadPath(const QString& requestedPath);
[[nodiscard]] QString untitledRecoveryPath();

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(Project project, QString projectFile = {}, QWidget* parent = nullptr);

    [[nodiscard]] const Project& project() const noexcept;
    void requestCompareMode();
    void revealLocation(const QString& laneId, Tick tick);
    void openLanePropertiesPreview(const QString& laneId);
    void openEditMenuPreview();

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
    void runCompare();
    void revealCompareDifference(int row, int column);
    void exportCompareReport();
    void exportPinloomEntry();
    void startAutosave();
    void finishAutosave();

private:
    void createActions();
    void createToolBars();
    void showSignalFind();
    void closeSignalFind(bool announce = true);
    void updateSignalFind();
    void stepSignalFind(int direction);
    [[nodiscard]] QStringList matchingVisibleSignals(const QString& query) const;
    void activateSignalFindMatch(const QStringList& matches, int index, bool wrapped);
    void showGoToTime();
    void closeGoToTime(bool announce = true);
    void submitGoToTime();
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
    void renameLaneById(const QString& laneId);
    void removeLaneById(const QString& laneId);
    void showLaneContextMenu(const QString& laneId, const QPoint& globalPosition);
    void editLaneKeyParameters(const QString& laneId);
    void moveSelectedLaneBy(int offset);
    void updateLaneOrderActions();
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
    bool loadFromPath(const QString& path);
    bool writeToPath(const QString& path);
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
    void loadFirstTraceReference();

    Project project_;
    QString projectFile_;
    bool dirty_{false};
    bool recoveryLoaded_{false};
    bool quickLaneDirtyBefore_{false};
    std::size_t pendingQuickCommandSize_{0};
    QString pendingQuickLaneId_;
    CommandStack commandStack_;
    std::optional<std::uint64_t> cleanCommandStateId_;
    std::uint64_t observedCommandStateId_{0};
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
    QAction* undoAction_{nullptr};
    QAction* redoAction_{nullptr};
    QAction* duplicateLaneAction_{nullptr};
    QAction* moveLaneUpAction_{nullptr};
    QAction* moveLaneDownAction_{nullptr};
    QAction* showHiddenLanesAction_{nullptr};
    QWidget* signalFindWidget_{nullptr};
    QLineEdit* signalFindEdit_{nullptr};
    QLabel* signalFindResultLabel_{nullptr};
    QToolButton* signalFindPreviousButton_{nullptr};
    QToolButton* signalFindNextButton_{nullptr};
    QToolButton* signalFindCloseButton_{nullptr};
    int signalFindMatchIndex_{-1};
    QWidget* goToTimeWidget_{nullptr};
    QLineEdit* goToTimeEdit_{nullptr};
    QLabel* goToTimeRangeLabel_{nullptr};
    QToolButton* goToTimeGoButton_{nullptr};
    QToolButton* goToTimeCloseButton_{nullptr};
    QAction* goToTimeWidgetAction_{nullptr};
    QAction* selectAction_{nullptr};
    QAction* drawAction_{nullptr};
    QAction* markerAction_{nullptr};
    QAction* rangeEditPaletteAction_{nullptr};
    QAction* relationAction_{nullptr};
    QAction* exportAction_{nullptr};
    QAction* importTraceAction_{nullptr};
    QAction* cancelTraceAction_{nullptr};
    QAction* compareModeAction_{nullptr};
    QMenu* editMenu_{nullptr};
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
    std::string activeTraceId_;
    QString pendingTracePath_;
    std::string pendingTraceId_;
    TraceFormat pendingTraceFormat_{TraceFormat::Vcd};
    Tick pendingTraceOffset_{0};
    bool pendingTraceAddsReference_{false};
    std::uint64_t traceGeneration_{0};
    std::shared_ptr<std::atomic_bool> traceCancelFlag_;
    QFutureWatcher<std::shared_ptr<TraceParseResult>>* traceWatcher_{nullptr};
    QTimer* autosaveTimer_{nullptr};
    QFutureWatcher<QPair<quint64, QString>>* autosaveWatcher_{nullptr};
    quint64 autosaveGeneration_{0};
    QString autosaveInFlightPath_;
    std::set<QString> discardedAutosavePaths_;
    bool autosavePending_{false};
    bool reloadTraceAfterCurrent_{false};
    bool compareModeRequested_{false};
    QString pendingRevealLaneId_;
    std::optional<Tick> pendingRevealTick_;
};

} // namespace wave
