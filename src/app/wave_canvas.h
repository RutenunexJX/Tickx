#pragma once

#include "wave/commands.h"
#include "wave/model.h"

#include <QAbstractScrollArea>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QStringList>

#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <vector>

class QContextMenuEvent;
class QComboBox;
class QCompleter;
class QEvent;
class QFrame;
class QKeyEvent;
class QLabel;
class QLineEdit;
class QMenu;
class QStringListModel;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QToolButton;
class QTimer;
class QWheelEvent;

namespace wave {

class WaveCanvas final : public QAbstractScrollArea {
    Q_OBJECT

public:
    enum class Tool {
        Selection,
        Draw,
        WaveEdit,
        Marker,
        Relation,
    };

    explicit WaveCanvas(QWidget* parent = nullptr);

    void setDocument(Project* project, Scenario* scenario, CommandStack* commandStack);
    void setTool(Tool tool);

    [[nodiscard]] Tool tool() const noexcept;
    [[nodiscard]] QString selectedLaneId() const;
    [[nodiscard]] QStringList selectedLaneIds() const;
    [[nodiscard]] Tick cursorTick() const noexcept;
    [[nodiscard]] std::optional<Tick> movableCursorTick() const noexcept;
    [[nodiscard]] std::optional<Tick> temporaryCursorTick() const noexcept;
    [[nodiscard]] QString selectedMarkerId() const;
    [[nodiscard]] std::optional<std::pair<Tick, Tick>> selectedTimeRange() const noexcept;
    [[nodiscard]] bool hasExplicitRangeSelection() const noexcept;
    [[nodiscard]] QString selectedSegmentLaneId() const;
    [[nodiscard]] QString selectedSegmentId() const;
    [[nodiscard]] QString hoveredBitBeatLaneId() const;
    [[nodiscard]] std::optional<std::pair<Tick, Tick>> hoveredBitBeatRange() const noexcept;
    [[nodiscard]] std::optional<std::size_t> laneDropDestinationIndex() const noexcept;
    [[nodiscard]] std::optional<Tick> waveEditTransitionPreviewTick() const noexcept;
    [[nodiscard]] std::optional<std::pair<Tick, Tick>>
    waveEditTransitionPreviewRange() const noexcept;
    [[nodiscard]] bool hasQuickLaneSetup() const noexcept;
    [[nodiscard]] bool hasLaneRename() const noexcept;
    [[nodiscard]] int signalHeaderWidth() const noexcept;
    [[nodiscard]] bool commitLaneRename();
    [[nodiscard]] bool commitPendingInlineEdits();
    [[nodiscard]] QWidget* busEditPaletteWidget() const noexcept;
    [[nodiscard]] QWidget* rangeEditPaletteWidget() const noexcept;
    [[nodiscard]] bool asynchronousEditing() const noexcept;

    void beginQuickLaneSetup(
        const QString& laneId,
        LaneKind kind,
        const QString& name,
        const QString& parameter,
        const QStringList& clockLabels = {},
        const QStringList& clockIds = {},
        const QString& selectedClockId = {});
    void finishQuickLaneSetup();
    void showQuickLaneSetupError(const QString& message, bool focusParameter = false);
    void beginLaneRename(const QString& laneId, const QString& name);
    void finishLaneRename();
    void showLaneRenameError(const QString& message);
    void showDurationEditError(const QString& message);
    void setSignalHeaderWidth(int width);

public slots:
    void zoomIn();
    void zoomOut();
    void fitScenario();
    void fitSelection();
    void refreshModel();
    void revealLocation(const QString& laneId, qint64 tick);
    void revealLane(const QString& laneId);
    void goToTick(qint64 tick);
    void dismissInlineValueEditor();
    void selectEntireTimeline();
    void cutSelection();
    void copySelection();
    void pasteAtCursor();
    void insertPulse();
    void setAsynchronousEditing(bool enabled);

signals:
    void addLaneRequested(LaneKind kind);
    void showHiddenLanesRequested();
    void showHiddenLaneRequested(const QString& laneId);
    void duplicateLaneRequested(const QString& laneId);
    void renameLaneRequested(const QString& laneId);
    void removeLaneRequested(const QString& laneId);
    void editLaneParametersRequested(const QString& laneId, const QPoint& globalPosition);
    void selectionChanged(const QString& laneId, qint64 tick);
    void modelEdited();
    void commandAvailabilityChanged();
    void statusMessage(const QString& message);
    void eventSelected(const QString& eventId);
    void quickLaneSetupAccepted(
        const QString& laneId,
        const QString& name,
        const QString& parameter,
        const QString& clockId);
    void quickLaneSetupCanceled(const QString& laneId);
    void laneRenameAccepted(const QString& laneId, const QString& name);
    void durationEditRequested(const QString& value);
    void measureModeExitRequested();
    void busEditPaletteVisibilityChanged(bool visible);
    void rangeEditPaletteVisibilityChanged(bool visible);
    void signalHeaderWidthCommitted(int width);

protected:
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool viewportEvent(QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    struct LaneLayout {
        std::size_t laneIndex;
        int top;
        int height;
    };

    struct EventHitRegion {
        std::string eventId;
        QRect rect;
    };

    enum class CursorInteraction {
        None,
        MoveActive,
        CreateLocked,
        MoveLocked,
    };
    enum class SegmentBoundary {
        None,
        Start,
        End,
    };
    enum class BusEditScope {
        Beat,
        Segment,
    };
    enum class BusEditCommitAction {
        Close,
        Stay,
        PreviousBeat,
        NextBeat,
    };
    enum class KeyboardRangeTarget {
        Step,
        TimelineBoundary,
        SignalEdge,
    };

    struct SegmentHit {
        const Segment* segment{nullptr};
        SegmentBoundary boundary{SegmentBoundary::None};
    };

    enum class WaveEditInteraction {
        None,
        MoveTransition,
        MoveSegment,
        ToggleBitRange,
        ResizeStart,
        ResizeEnd,
        ResizeRangeStart,
        ResizeRangeEnd,
        SelectRange,
    };


    static constexpr int DefaultHeaderWidth = 190;
    static constexpr int MinimumHeaderWidth = 140;
    static constexpr int MaximumHeaderWidth = 480;
    static constexpr int RulerHeight = 40;
    static constexpr int AddLaneRowHeight = 48;
    static constexpr int LaneDragAutoScrollMargin = 48;
    static constexpr int LaneDragAutoScrollStep = 28;
    static constexpr int LaneDragAutoScrollIntervalMs = 30;
    static constexpr int WaveEditDragAutoScrollMargin = 48;
    static constexpr int WaveEditDragAutoScrollStep = 28;
    static constexpr int WaveEditDragAutoScrollIntervalMs = 30;

    [[nodiscard]] Qt::CursorShape defaultCursorShape() const noexcept;
    void rebuildLaneLayout();
    void rebuildSnapIndex();
    void updateScrollBars();
    void updateAddLaneButtonGeometry();
    [[nodiscard]] bool signalHeaderDividerAt(const QPoint& position) const noexcept;
    [[nodiscard]] int fittedSignalHeaderWidth() const;
    void positionQuickLaneSetup();
    void positionLaneRename();
    void positionDurationEditor();
    void submitQuickLaneSetup();
    void cancelQuickLaneSetup();
    void submitLaneRename();
    void cancelLaneRename();
    void submitBusValue(BusEditCommitAction action = BusEditCommitAction::Close);
    void submitRangeValue();
    void submitDurationEdit(bool preserveMouseFocusTarget = false);
    void clearClockBeat(
        const std::string& laneId,
        Tick start,
        Tick end);
    [[nodiscard]] bool hasPendingBusValueEdit() const noexcept;
    [[nodiscard]] bool hasPendingRangeValueEdit() const noexcept;
    [[nodiscard]] bool hasPendingValueEdit() const noexcept;
    void syncDurationEditor();
    void positionBusPresetPalette();
    void showBusPresetPalette(
        const Lane& lane,
        const QPoint& anchor,
        std::optional<Tick> exactTick = std::nullopt,
        std::optional<std::pair<Tick, Tick>> exactRange = std::nullopt,
        BusEditScope exactRangeScope = BusEditScope::Segment);
    void hideBusPresetPalette();
    bool advanceBusValueEdit(
        const std::string& laneId,
        const std::pair<Tick, Tick>& currentRange,
        bool forward);
    bool stepBusEditorValue(bool upward);
    bool cycleBusRecentValue(bool forward);
    void rememberBusValue(const std::string& laneId, const QString& value);
    [[nodiscard]] QString busEditorValue(const Lane& lane) const;
    void showRangeEditPalette();
    void hideRangeEditPalette();
    void clearExplicitRangeSelection(bool clearLanes = true);
    [[nodiscard]] std::optional<LaneKind> explicitRangeKind() const;
    [[nodiscard]] QStringList explicitRangeEnumSymbols() const;
    [[nodiscard]] SegmentBoundary explicitRangeBoundaryAt(const QPoint& position) const;
    [[nodiscard]] bool hasBitRangeSelection() const;
    bool applyExplicitRangeValue(
        const std::string& value,
        const std::string& presetId = {});
    bool clearExplicitRange(bool cutting = false);
    void applyExplicitRangePreset(const std::string& presetId);
    void applyBusPreset(
        const std::string& laneId,
        const std::string& presetId,
        Tick tick,
        bool useEditorRange = true);
    void promptBusValueAt(const std::string& laneId, Tick tick);
    bool setLaneRangeValue(
        const std::string& laneId,
        Tick start,
        Tick end,
        std::string value,
        JsonExtensions extensions = {});
    bool clearSelectedBitRange();
    void clearSelectedSegment();
    void updateLaneDropTarget(int y);
    void updateLaneDragAutoScroll(int pointerY);
    void advanceLaneDragAutoScroll();
    void stopLaneDragAutoScroll();
    void updateWaveEditDragAutoScroll(
        const QPoint& pointerPosition,
        Qt::KeyboardModifiers modifiers);
    void advanceWaveEditDragAutoScroll();
    void stopWaveEditDragAutoScroll();
    void commitLaneReorder();
    void setScale(double scale, int anchorX);
    [[nodiscard]] double contentWidth() const;
    [[nodiscard]] int waveViewportWidth() const;
    [[nodiscard]] QRect addLaneRowRect() const;
    [[nodiscard]] std::size_t hiddenLaneCount() const noexcept;
    [[nodiscard]] Tick tickAtX(int x) const;
    [[nodiscard]] int xAtTick(Tick tick) const;
    [[nodiscard]] const LaneLayout* layoutAtY(int y) const;
    [[nodiscard]] Lane* laneAtY(int y);
    [[nodiscard]] const Lane* laneAtY(int y) const;
    [[nodiscard]] Marker* markerById(const std::string& markerId);
    [[nodiscard]] const Marker* markerById(const std::string& markerId) const;
    [[nodiscard]] const Marker* markerAtPosition(const QPoint& position) const;
    [[nodiscard]] std::pair<Tick, Tick> markerDisplayRange(const Marker& marker) const;
    [[nodiscard]] QString markerLocationText(const Marker& marker) const;
    [[nodiscard]] std::string nextLockedMarkerName(bool interval) const;
    [[nodiscard]] Tick cursorKeyboardStep() const;
    void adjustTimeRangeByKeyboard(
        bool forward,
        KeyboardRangeTarget target = KeyboardRangeTarget::Step);
    void adjustRangeSignalsByKeyboard(bool downward);
    [[nodiscard]] std::optional<Tick> adjacentEdgeTick(
        const Lane& lane,
        Tick from,
        bool forward) const;
    void selectAdjacentLane(bool downward);
    void ensureLaneVisible(const std::string& laneId);
    [[nodiscard]] QString laneValueAt(const Lane& lane, Tick tick) const;
    [[nodiscard]] QString cursorValue(const Lane& lane) const;
    [[nodiscard]] QString cursorDeltaText(Tick from, Tick to) const;
    [[nodiscard]] QString cursorMeasurementText() const;
    [[nodiscard]] Segment* segmentById(
        const std::string& laneId,
        const std::string& segmentId);
    [[nodiscard]] const Segment* segmentById(
        const std::string& laneId,
        const std::string& segmentId) const;
    [[nodiscard]] const Segment* segmentAtTick(const Lane& lane, Tick tick) const;
    [[nodiscard]] SegmentHit segmentHitAtPosition(
        const Lane& lane,
        const QPoint& position) const;
    [[nodiscard]] std::pair<Tick, Tick> beatGrid(const Lane& lane) const;
    [[nodiscard]] Tick synchronousBoundaryTick(Tick tick, const Lane& lane) const;
    [[nodiscard]] Tick editTick(Tick tick, const Lane& lane) const;
    [[nodiscard]] Tick minimumWaveEditUnit(const Lane& lane) const;
    [[nodiscard]] std::pair<Tick, Tick> beatRangeAt(Tick tick, const Lane& lane) const;
    [[nodiscard]] std::pair<Tick, Tick> editableBeatRangeAt(Tick tick, const Lane& lane) const;
    [[nodiscard]] std::vector<std::pair<Tick, Tick>> beatRangesBetween(
        Tick first,
        Tick second,
        const Lane& lane) const;
    [[nodiscard]] std::vector<std::pair<Tick, Tick>> editableBeatRangesBetween(
        Tick first,
        Tick second,
        const Lane& lane) const;
    [[nodiscard]] std::optional<std::pair<Tick, Tick>>
    adjacentEditableBeatRange(
        const Lane& lane,
        const std::pair<Tick, Tick>& currentRange,
        bool forward) const;
    void navigateSelectedBeat(bool forward);
    void navigateTimelinePage(bool forward);
    void clearWaveEditState();
    void commitWaveEdit(const QPoint& releasePosition);
    [[nodiscard]] Tick constrainedTransitionTick(const Event& event, Tick requested) const;
    void commitBitToggle(const std::vector<std::pair<Tick, Tick>>& beats, const QPoint& position);
    void editSegmentAt(const QPoint& position);
    void ensureCursorVisible(Tick tick);
    void removeSelectedMarker();
    void moveSelectedMarkerBy(Tick delta);
    [[nodiscard]] Tick snappedTick(Tick input, const Lane* lane) const;
    [[nodiscard]] Tick majorTickStep() const;
    [[nodiscard]] std::pair<Tick, Tick> visibleTickRange() const;
    void commitDraw(const QPoint& releasePosition);
    void commitMarker(const QPoint& releasePosition);
    void commitRelation(const QPoint& releasePosition);
    void commitTransition(const QPoint& releasePosition);
    [[nodiscard]] const Event* eventAtPosition(const QPoint& position) const;
    [[nodiscard]] QPoint eventPoint(const Event& event) const;

    void drawRuler(class QPainter& painter);
    void drawAddLaneRow(class QPainter& painter);
    void drawLane(
        class QPainter& painter,
        const Lane& lane,
        const LaneLayout& layout,
        Tick visibleStart,
        Tick visibleEnd);
    void drawClock(
        class QPainter& painter,
        const Lane& lane,
        const QRect& rect,
        Tick visibleStart,
        Tick visibleEnd);
    void drawBitSegments(
        class QPainter& painter,
        const Lane& lane,
        const QRect& rect,
        Tick visibleStart,
        Tick visibleEnd);
    void drawBusSegments(
        class QPainter& painter,
        const Lane& lane,
        const QRect& rect,
        Tick visibleStart,
        Tick visibleEnd);
    void drawScenarioOverlays(
        class QPainter& painter,
        Tick visibleStart,
        Tick visibleEnd);
    void drawCursorOverlays(
        class QPainter& painter,
        Tick visibleStart,
        Tick visibleEnd);
    void drawWaveEditOverlay(class QPainter& painter);
    void drawEditGuide(class QPainter& painter);
    void drawWaveEditTransitionPreview(class QPainter& painter);
    void drawLaneReorderOverlay(class QPainter& painter);

    Project* project_{nullptr};
    Scenario* scenario_{nullptr};
    CommandStack* commandStack_{nullptr};
    std::array<QToolButton*, 3> addLaneButtons_{};
    QToolButton* showHiddenLanesButton_{nullptr};
    QMenu* hiddenLanesMenu_{nullptr};
    QFrame* quickLaneSetupPanel_{nullptr};
    QLineEdit* quickLaneNameEdit_{nullptr};
    QLineEdit* quickLaneParameterEdit_{nullptr};
    QComboBox* quickLaneClockCombo_{nullptr};
    QLabel* quickLaneErrorLabel_{nullptr};
    QString quickLaneSetupLaneId_;
    LaneKind quickLaneSetupKind_{LaneKind::Bit};
    QLineEdit* laneRenameEdit_{nullptr};
    QString laneRenameLaneId_;
    bool laneRenameClosing_{false};
    QLabel* durationLabel_{nullptr};
    QLineEdit* durationEdit_{nullptr};
    bool durationEditSubmitting_{false};
    bool durationEditMouseFocusOut_{false};
    bool durationEditBlurConsumesCanvasInput_{false};
    QFrame* busPresetPalette_{nullptr};
    QLabel* busPresetContextLabel_{nullptr};
    QLineEdit* busValueEdit_{nullptr};
    QComboBox* busRadixCombo_{nullptr};
    QComboBox* busRecentValuesCombo_{nullptr};
    QCompleter* laneValueCompleter_{nullptr};
    QStringListModel* laneValueCompletionModel_{nullptr};
    std::optional<Tick> busPresetAnchorTick_;
    std::optional<std::pair<Tick, Tick>> busEditRange_;
    std::string busPresetLaneId_;
    std::map<std::string, QStringList> busRecentValues_;
    BusEditScope busEditScope_{BusEditScope::Beat};
    bool busEditPaletteVisible_{false};
    QFrame* rangeEditPalette_{nullptr};
    QLabel* rangeEditContextLabel_{nullptr};
    QToolButton* rangeCopyButton_{nullptr};
    QToolButton* rangeCutButton_{nullptr};
    QToolButton* rangePasteButton_{nullptr};
    QToolButton* rangeClearButton_{nullptr};
    QLineEdit* rangeValueEdit_{nullptr};
    QCompleter* rangeValueCompleter_{nullptr};
    QStringListModel* rangeValueCompletionModel_{nullptr};
    QToolButton* rangeZeroButton_{nullptr};
    QToolButton* rangeOneButton_{nullptr};
    QToolButton* rangeXButton_{nullptr};
    QToolButton* rangeZButton_{nullptr};
    QToolButton* rangeDontCareButton_{nullptr};
    bool rangeEditPaletteVisible_{false};
    bool explicitRangeSelection_{false};
    std::vector<LaneLayout> laneLayout_;
    std::vector<Tick> signalEdgeIndex_;
    std::optional<Tick> movableCursorTick_;
    std::optional<Tick> temporaryCursorTick_;
    std::string selectedMarkerId_;
    CursorInteraction cursorInteraction_{CursorInteraction::None};
    std::optional<std::pair<Tick, Tick>> lockedMarkerOriginalRange_;
    Tool tool_{Tool::WaveEdit};
    double pixelsPerTick_{0.003};
    std::string selectedLaneId_;
    std::string selectedSegmentLaneId_;
    std::string selectedSegmentId_;
    WaveEditInteraction waveEditInteraction_{WaveEditInteraction::None};
    std::optional<std::pair<Tick, Tick>> waveEditOriginalRange_;
    std::optional<std::pair<Tick, Tick>> waveEditPreviewRange_;
    std::string waveEditHoverLaneId_;
    std::optional<std::pair<Tick, Tick>> waveEditHoverRange_;
    QPoint waveEditPressPosition_;
    Tick waveEditGrabOffset_{0};
    bool laneHeaderPressed_{false};
    bool laneHeaderDragging_{false};
    bool laneHeaderSelectionActive_{false};
    QPoint laneHeaderPressPosition_;
    int laneDragOriginalVerticalScroll_{0};
    std::string laneDragId_;
    std::optional<std::size_t> laneDropDestinationIndex_;
    std::optional<int> laneDropIndicatorY_;
    QTimer* laneDragAutoScrollTimer_{nullptr};
    int laneDragAutoScrollDirection_{0};
    int laneDragAutoScrollPointerY_{0};
    QTimer* waveEditDragAutoScrollTimer_{nullptr};
    int waveEditDragAutoScrollDirection_{0};
    QPoint waveEditDragAutoScrollPointer_;
    Qt::KeyboardModifiers waveEditDragAutoScrollModifiers_{Qt::NoModifier};
    int waveEditDragOriginalHorizontalScroll_{0};
    bool waveEditDragAutoScrolled_{false};
    bool waveEditCopyDrag_{false};
    bool asynchronousEditing_{false};
    int headerWidth_{DefaultHeaderWidth};
    bool headerResizing_{false};
    int headerResizePressX_{0};
    int headerResizeOriginalWidth_{DefaultHeaderWidth};


    std::vector<std::string> selectedLaneIds_;
    Tick cursorTick_{0};
    bool drawing_{false};
    Tick drawStart_{0};
    Tick drawCurrent_{0};
    std::string drawLaneId_;
    std::string drawValue_;
    std::string activeEventId_;
    QPoint interactionCurrent_;
    std::vector<EventHitRegion> eventHitRegions_;
    bool fitPending_{false};
    std::optional<std::pair<Tick, Tick>> selectionRange_;
    int selectionStartY_{0};
    bool bypassSnap_{false};
    std::optional<Tick> snapGuideTick_;
    bool panning_{false};
    bool spaceHeld_{false};
    QPoint panPressPosition_;
    int panStartHorizontal_{0};
    int panStartVertical_{0};
};

} // namespace wave
