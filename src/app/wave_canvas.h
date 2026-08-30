#pragma once

#include "wave/widgets_export.h"

#include "wave/commands.h"
#include "wave/model.h"

#include <QAbstractScrollArea>
#include <QLineF>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QStringList>

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

class QContextMenuEvent;
class QAction;
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
class QShowEvent;
class QToolButton;
class QTimer;
class QWheelEvent;

namespace wave {

class WAVEWIDGETS_API WaveCanvas final : public QAbstractScrollArea {
    Q_OBJECT

public:
    enum class Tool {
        Selection,
        Draw,
        WaveEdit,
        Marker,
        Relation,
    };

    enum class SegmentAction {
        DuplicateBefore,
        DuplicateAfter,
        MoveEarlier,
        MoveLater,
        ExpandStart,
        TrimStart,
        ExpandEnd,
        TrimEnd,
    };

    enum class BusEditAction {
        ApplyDraft,
        PresetZero,
        PresetReserved,
        PresetX,
        PresetZ,
        PresetDontCare,
        Clear,
    };

    struct SegmentActionState {
        bool applicable{false};
        bool valid{false};
        bool modelChanges{false};
        Tick start{0};
        Tick end{0};
        std::size_t relationRemovalCount{0};
        QString summary;
    };

    struct BusEditActionState {
        bool applicable{false};
        bool valid{false};
        bool modelChanges{false};
        Tick start{0};
        Tick end{0};
        std::size_t relationRemovalCount{0};
        std::size_t valueCount{0};
        bool sequence{false};
        bool extendsEnd{false};
        QString displayValue;
        QString summary;
    };

    struct DifferenceRange {
        std::string laneId;
        Tick start{0};
        Tick end{0};
    };

    explicit WaveCanvas(QWidget* parent = nullptr);

    void setDocument(Project* project, Scenario* scenario, CommandStack* commandStack);
    void clearDocumentContexts();
    [[nodiscard]] bool hasDocumentContext(const Scenario* scenario) const noexcept;
    bool cloneDocumentContext(
        std::string_view sourceScenarioId,
        std::string_view targetScenarioId);
    void setTool(Tool tool);

    [[nodiscard]] Tool tool() const noexcept;
    [[nodiscard]] QString selectedLaneId() const;
    [[nodiscard]] QStringList selectedLaneIds() const;
    [[nodiscard]] bool hasLaneHeaderSelection() const noexcept;
    [[nodiscard]] std::pair<bool, QString> selectedLanePasteAvailability() const;
    [[nodiscard]] std::pair<bool, QString>
    selectedRangeClearAvailability() const;
    [[nodiscard]] std::pair<bool, QString>
    selectedRangeRepeatAvailability() const;
    [[nodiscard]] std::size_t
    selectedRangeRepeatRelationRemovalCount() const;
    [[nodiscard]] std::optional<std::pair<Tick, Tick>>
    laneHeaderPastePreviewRange() const;
    [[nodiscard]] QStringList laneHeaderPastePreviewTargetLaneIds() const;
    [[nodiscard]] std::size_t
    laneHeaderPastePreviewRelationRemovalCount() const;
    [[nodiscard]] QStringList
    laneHeaderPastePreviewRelationRemovalSummaries() const;
    [[nodiscard]] std::optional<std::pair<Tick, Tick>>
    pastePreviewRange() const;
    [[nodiscard]] QStringList pastePreviewTargetLaneIds() const;
    [[nodiscard]] std::size_t pastePreviewRelationRemovalCount() const;
    [[nodiscard]] QStringList
    pastePreviewRelationRemovalSummaries() const;
    [[nodiscard]] std::optional<std::pair<Tick, Tick>>
    repeatPreviewRange() const;
    [[nodiscard]] QStringList repeatPreviewTargetLaneIds() const;
    [[nodiscard]] std::size_t repeatPreviewRelationRemovalCount() const;
    [[nodiscard]] QStringList
    repeatPreviewRelationRemovalSummaries() const;
    [[nodiscard]] Tick cursorTick() const noexcept;
    [[nodiscard]] Tick visibleTimeSpan() const noexcept;
    bool restoreVisibleTimeSpan(Tick span, Tick anchorTick);
    [[nodiscard]] std::optional<Tick> movableCursorTick() const noexcept;
    [[nodiscard]] std::optional<Tick> temporaryCursorTick() const noexcept;
    [[nodiscard]] QString selectedMarkerId() const;
    [[nodiscard]] QStringList selectedMarkerIds() const;
    [[nodiscard]] QStringList selectedRelationIds() const;
    [[nodiscard]] std::optional<std::pair<Tick, Tick>> selectedTimeRange() const noexcept;
    [[nodiscard]] bool hasExplicitRangeSelection() const noexcept;
    [[nodiscard]] QString selectedSegmentLaneId() const;
    [[nodiscard]] QString selectedSegmentId() const;
    [[nodiscard]] QString hoveredBitBeatLaneId() const;
    [[nodiscard]] std::optional<std::pair<Tick, Tick>> hoveredBitBeatRange() const noexcept;
    [[nodiscard]] std::optional<std::size_t> laneDropDestinationIndex() const noexcept;
    [[nodiscard]] QString laneDropGroupId() const;
    [[nodiscard]] std::optional<Tick> waveEditTransitionPreviewTick() const noexcept;
    [[nodiscard]] std::optional<std::pair<Tick, Tick>>
    waveEditTransitionPreviewRange() const noexcept;
    [[nodiscard]] std::optional<std::pair<Tick, Tick>>
    waveEditRangeTransferPreviewRange() const noexcept;
    [[nodiscard]] bool waveEditRangeTransferCopies() const noexcept;
    [[nodiscard]] QStringList waveEditRangeTransferTargetLaneIds() const;
    [[nodiscard]] bool waveEditRangeTransferTargetValid() const noexcept;
    [[nodiscard]] QString waveEditRangeTransferTargetError() const;
    [[nodiscard]] bool waveEditRangeTransferChangesModel() const;
    [[nodiscard]] bool waveEditRangeTransferExtendsEnd() const;
    [[nodiscard]] std::size_t
    waveEditRangeTransferRelationRemovalCount() const;
    [[nodiscard]] QStringList
    waveEditRangeTransferRelationRemovalSummaries() const;
    [[nodiscard]] bool waveEditSegmentChangesModel() const;
    [[nodiscard]] std::size_t
    waveEditSegmentRelationRemovalCount() const;
    [[nodiscard]] QStringList
    waveEditSegmentRelationRemovalSummaries() const;
    [[nodiscard]] std::optional<std::pair<Tick, Tick>>
    waveEditSegmentPreviewRange() const;
    [[nodiscard]] SegmentActionState
    selectedSegmentActionState(SegmentAction action) const;
    bool previewSelectedSegmentAction(SegmentAction action);
    void clearSelectedSegmentActionPreview();
    [[nodiscard]] BusEditActionState
    busEditActionState(BusEditAction action) const;
    bool previewBusEditAction(BusEditAction action);
    void clearBusEditActionPreview();
    [[nodiscard]] std::optional<std::pair<Tick, Tick>>
    busEditPreviewRange() const;
    [[nodiscard]] bool busEditPreviewChangesModel() const;
    [[nodiscard]] std::size_t busEditPreviewRelationRemovalCount() const;
    [[nodiscard]] QStringList busEditPreviewRelationRemovalSummaries() const;
    [[nodiscard]] std::optional<std::pair<Tick, Tick>>
    rangeSequencePreviewRange() const;
    [[nodiscard]] bool rangeSequencePreviewChangesModel() const;
    [[nodiscard]] std::size_t
    rangeSequencePreviewRelationRemovalCount() const;
    [[nodiscard]] QString rangeSequenceCaretTargetLaneId() const;
    [[nodiscard]] std::optional<std::pair<Tick, Tick>>
    rangeSequenceCaretTargetRange() const;
    [[nodiscard]] std::size_t rangeSequenceCaretTargetCount() const noexcept;
    [[nodiscard]] bool hasQuickLaneSetup() const noexcept;
    [[nodiscard]] bool hasLaneRename() const noexcept;
    [[nodiscard]] bool isGroupCollapsed(const QString& groupId) const noexcept;
    [[nodiscard]] bool isLaneDisplayed(const QString& laneId) const noexcept;
    bool setGroupCollapsed(const QString& groupId, bool collapsed);
    [[nodiscard]] int signalHeaderWidth() const noexcept;
    [[nodiscard]] bool commitLaneRename();
    [[nodiscard]] bool commitPendingInlineEdits();
    [[nodiscard]] QWidget* busEditPaletteWidget() const noexcept;
    [[nodiscard]] QWidget* rangeEditPaletteWidget() const noexcept;
    [[nodiscard]] bool asynchronousEditing() const noexcept;
    [[nodiscard]] std::optional<Tick> explicitRangeAnchorTick() const noexcept;
    [[nodiscard]] std::optional<Tick> explicitRangeActiveTick() const noexcept;
    [[nodiscard]] QString editTargetSummary() const;
    [[nodiscard]] QString editTargetToolTip() const;
    [[nodiscard]] QString editTimingSummary() const;
    [[nodiscard]] bool relationsVisible() const noexcept;
    [[nodiscard]] QString restoreSelectionForHistoryTransition(
        std::uint64_t fromStateId,
        std::uint64_t toStateId);
    void beginCommandSelectionTransition(std::uint64_t beforeStateId);
    void finishCommandSelectionTransition(std::uint64_t afterStateId);
    void cancelCommandSelectionTransition() noexcept;
    void selectSegmentAtCursor();
    void selectPreviousSegment();
    void selectNextSegment();
    bool duplicateSelectedSegmentBefore();
    bool duplicateSelectedSegmentAfter();
    bool moveSelectedSegmentEarlier();
    bool moveSelectedSegmentLater();
    bool expandSelectedSegmentStart();
    bool trimSelectedSegmentStart();
    bool expandSelectedSegmentEnd();
    bool trimSelectedSegmentEnd();

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
    void setDifferenceRanges(std::vector<DifferenceRange> ranges);

public slots:
    void zoomIn();
    void zoomOut();
    void fitScenario();
    void fitSelection();
    void refreshModel();
    void revealLocation(const QString& laneId, qint64 tick);
    void revealMarker(const QString& markerId);
    void restoreStableObjectSelections(
        const QStringList& markerIds,
        const QStringList& relationIds);
    void revealLane(const QString& laneId);
    void selectLaneHeaders(
        const QStringList& laneIds,
        const QString& activeLaneId);
    void goToTick(qint64 tick);
    void dismissInlineValueEditor();
    void selectEntireTimeline();
    void cutSelection();
    void copySelection();
    void pasteAtCursor();
    void duplicateSelectionAfter();
    void insertPulse();
    void setAsynchronousEditing(bool enabled);
    void setRelationsVisible(bool visible);
    void invalidateRangeSequenceHistoryContext();
    bool setExplicitRangeActiveTick(qint64 tick);

signals:
    void addLaneRequested(LaneKind kind);
    void showHiddenLanesRequested();
    void showHiddenLaneRequested(const QString& laneId);
    void duplicateLaneRequested(const QString& laneId);
    void duplicateLanesRequested(const QStringList& laneIds);
    void renameLaneRequested(const QString& laneId);
    void removeLaneRequested(const QString& laneId);
    void removeLanesRequested(const QStringList& laneIds);
    void editLaneParametersRequested(const QString& laneId, const QPoint& globalPosition);
    void selectionChanged(const QString& laneId, qint64 tick);
    void modelEdited();
    void commandAvailabilityChanged();
    void statusMessage(const QString& message);
    void pointerStatusMessage(const QString& message);
    void eventSelected(const QString& eventId);
    void relationSelected(const QString& relationId);
    void quickLaneSetupAccepted(
        const QString& laneId,
        const QString& name,
        const QString& parameter,
        const QString& clockId);
    void quickLaneSetupCanceled(const QString& laneId);
    void laneRenameAccepted(const QString& laneId, const QString& name);
    void durationEditRequested(const QString& value);
    void measureModeExitRequested();
    void relationModeExitRequested();
    void busEditPaletteVisibilityChanged(bool visible);
    void rangeEditPaletteVisibilityChanged(bool visible);
    void exactRangeTimeEditRequested();
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
    void showEvent(QShowEvent* event) override;
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

    struct RelationHitRegion {
        std::string relationId;
        QLineF line;
        QRect sourceHandle;
        QRect targetHandle;
    };

    enum class RelationEndpoint {
        None,
        Source,
        Target,
    };

    struct RelationHit {
        const Relation* relation{nullptr};
        RelationEndpoint endpoint{RelationEndpoint::None};
    };

    enum class RelationInteraction {
        None,
        Create,
        RetargetSource,
        RetargetTarget,
    };

    struct TimedOverlayIndex {
        std::size_t modelIndex{0};
        Tick start{0};
        Tick end{0};
        Tick prefixMaximumEnd{0};
    };

    enum class CursorInteraction {
        None,
        MoveActive,
        CreateLocked,
        MoveLocked,
        ResizeLockedStart,
        ResizeLockedEnd,
    };
    enum class SegmentBoundary {
        None,
        Start,
        End,
    };

    struct MarkerHit {
        const Marker* marker{nullptr};
        SegmentBoundary boundary{SegmentBoundary::None};
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
        PreviousSegment,
        NextSegment,
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

    struct HistorySelectionSnapshot {
        std::string selectedLaneId;
        std::vector<std::string> selectedLaneIds;
        std::string selectedSegmentLaneId;
        std::string selectedSegmentId;
        std::optional<std::pair<Tick, Tick>> selectionRange;
        Tick cursorTick{0};
        bool explicitRangeSelection{false};
        bool laneHeaderSelectionActive{false};
    };

    struct RangePasteAvailability {
        bool enabled{false};
        QString toolTip;
    };

    struct RelationRemovalImpact {
        std::vector<std::string> ids;
        QStringList endpointSummaries;
        QStringList summaries;
    };

    struct RangeClearAvailability {
        bool enabled{false};
        bool clockRange{false};
        std::size_t affectedLaneCount{0};
        RelationRemovalImpact relationImpact;
        QString toolTip;
    };

    struct RangeValueAvailability {
        bool valid{false};
        bool enabled{false};
        std::size_t affectedLaneCount{0};
        QString displayValue;
        RelationRemovalImpact relationImpact;
        QString toolTip;
    };

    struct BitPatternDraft {
        bool valid{false};
        std::vector<std::string> values;
        QString error;
    };

    struct BitPatternProjection {
        std::vector<std::string> laneIds;
        Tick start{0};
        Tick end{0};
        Tick beatWidth{0};
        std::size_t patternLength{0};
        std::size_t beatCount{0};
        std::size_t changedLaneCount{0};
        bool sharedPattern{true};
        QStringList patternSummaries;
        std::vector<Lane> lanes;
        std::vector<LaneSequenceAssignment> assignments;
        RelationRemovalImpact relationImpact;
        bool waveformChanges{false};
        bool modelChanges{false};
    };

    struct BitPatternAssessment {
        bool applicable{false};
        bool valid{false};
        bool enabled{false};
        QString summary;
        std::optional<BitPatternProjection> projection;
    };

    struct BusRangeSequenceProjection {
        std::vector<std::string> laneIds;
        Tick start{0};
        Tick end{0};
        Tick beatWidth{0};
        std::size_t patternLength{0};
        std::size_t beatCount{0};
        std::size_t changedLaneCount{0};
        bool sharedSequence{true};
        QStringList sequenceSummaries;
        std::vector<Lane> lanes;
        std::vector<LaneSequenceAssignment> assignments;
        RelationRemovalImpact relationImpact;
        bool waveformChanges{false};
        bool modelChanges{false};
    };

    struct RangeSequenceTarget {
        std::string laneId;
        Tick start{0};
        Tick end{0};
        std::size_t firstBeat{0};
        std::size_t beatCount{0};
        std::size_t totalBeatCount{0};
    };

    struct RangeSequenceTextSpan {
        int textStart{0};
        int textLength{0};
        QString token;
        std::vector<RangeSequenceTarget> targets;
    };

    struct RangeSequenceBaseline {
        const Scenario* scenario{nullptr};
        std::vector<std::string> laneIds;
        std::pair<Tick, Tick> range{0, 0};
        std::uint64_t historyStateId{0};
        QString text;
        std::vector<RangeSequenceTextSpan> spans;
        std::vector<std::vector<std::string>> values;
    };

    struct RangeSequenceAnchor {
        std::string laneId;
        Tick tick{0};
    };

    struct BusRangeSequenceAssessment {
        bool applicable{false};
        bool sequence{false};
        bool valid{false};
        bool enabled{false};
        QString summary;
        std::optional<BusRangeSequenceProjection> projection;
        std::vector<RangeSequenceTextSpan> textSpans;
    };

    struct RangeSequenceSeed {
        bool applicable{false};
        bool available{false};
        std::size_t laneCount{0};
        std::size_t beatCount{0};
        QString text;
        QString summary;
        std::vector<RangeSequenceTextSpan> spans;
        std::vector<std::vector<std::string>> values;
    };

    struct RangeRepeatAvailability {
        bool enabled{false};
        Tick destination{0};
        Tick duration{0};
        bool extendsEnd{false};
        RelationRemovalImpact relationImpact;
        QString toolTip;
    };

    enum class RangePreviewOperation {
        Paste,
        Repeat,
    };

    struct LaneHeaderPastePreview {
        RangePreviewOperation operation{RangePreviewOperation::Paste};
        Tick start{0};
        Tick duration{0};
        std::vector<Lane> lanes;
        std::size_t relationRemovalCount{0};
        std::vector<std::string> relationRemovalIds;
        QStringList relationRemovalEndpointSummaries;
        QStringList relationRemovalSummaries;
        bool waveformChanges{false};
        bool modelChanges{false};
    };

    struct RangeTransferProjection {
        bool copy{false};
        Tick sourceStart{0};
        Tick targetStart{0};
        Tick duration{0};
        std::vector<std::string> targetLaneIds;
        std::vector<CopiedLaneRange> copiedLanes;
        std::vector<Lane> lanes;
        RelationRemovalImpact relationImpact;
        bool waveformChanges{false};
        bool extendsEnd{false};
        bool modelChanges{false};
    };

    enum class SegmentEditOperation {
        Move,
        Copy,
        ResizeStart,
        ResizeEnd,
    };

    struct SegmentEditProjection {
        SegmentEditOperation operation{SegmentEditOperation::Move};
        std::string laneId;
        std::string sourceSegmentId;
        Tick originalStart{0};
        Tick originalEnd{0};
        Tick start{0};
        Tick end{0};
        Lane lane;
        RelationRemovalImpact relationImpact;
        bool waveformChanges{false};
        bool modelChanges{false};
    };

    struct SegmentActionAssessment {
        SegmentActionState state;
        std::optional<SegmentEditProjection> projection;
    };

    struct BusEditProjection {
        BusEditAction action{BusEditAction::ApplyDraft};
        std::string laneId;
        Tick start{0};
        Tick end{0};
        Lane lane;
        std::vector<LaneSequenceStep> sequenceSteps;
        RelationRemovalImpact relationImpact;
        bool waveformChanges{false};
        bool modelChanges{false};
        bool extendsEnd{false};
        QString displayValue;
        QString summary;
    };

    struct BusEditActionAssessment {
        BusEditActionState state;
        std::optional<BusEditProjection> projection;
    };

    struct BusDraftToken {
        int textStart{0};
        int textLength{0};
        std::size_t expandedOffset{0};
        std::size_t expandedCount{0};
        QString token;
    };

    struct BusDraftValues {
        bool sequence{false};
        bool valid{false};
        std::size_t valueCount{0};
        std::vector<std::string> normalizedValues;
        std::vector<BusDraftToken> tokens;
        QString error;
    };

    struct DocumentContext {
        HistorySelectionSnapshot selection;
        std::vector<std::string> selectedMarkerIds;
        std::vector<std::string> selectedRelationIds;
        std::map<
            std::pair<std::uint64_t, std::uint64_t>,
            HistorySelectionSnapshot>
            historySelectionTransitions;
        double pixelsPerTick{0.003};
        int horizontalScroll{0};
        int verticalScroll{0};
        std::set<std::string> collapsedGroupIds;
    };

    enum class WaveEditInteraction {
        None,
        MoveTransition,
        MoveSegment,
        MoveRange,
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
    void sanitizeCollapsedGroups();
    void updateScrollBars();
    void updateAddLaneButtonGeometry();
    [[nodiscard]] bool signalHeaderDividerAt(const QPoint& position) const noexcept;
    [[nodiscard]] bool isLaneDisplayed(const Lane& lane) const noexcept;
    [[nodiscard]] const Lane* visibleParentGroup(const Lane& lane) const noexcept;
    [[nodiscard]] std::size_t visibleGroupMemberCount(
        const std::string& groupId) const noexcept;
    [[nodiscard]] QRect groupDisclosureRect(const Lane& group) const;
    [[nodiscard]] int fittedSignalHeaderWidth() const;
    void positionQuickLaneSetup();
    void positionLaneRename();
    void positionDurationEditor();
    void submitQuickLaneSetup();
    void cancelQuickLaneSetup();
    void submitLaneRename();
    void cancelLaneRename();
    void submitBusValue(BusEditCommitAction action = BusEditCommitAction::Close);
    void submitBusSequence(
        BusEditCommitAction action,
        const BusDraftValues& draft,
        const BusEditActionAssessment& assessment);
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
    void cancelBusValueEdit();
    void clearBusEditTarget();
    bool toggleBusEditScope();
    [[nodiscard]] BusEditActionAssessment
    assessBusEditAction(BusEditAction action) const;
    void updateBusEditActionStates(bool announceDraft = false);
    void restoreBusEditDraftPreview(bool announce = false);
    void updateBusEditContextLabel(
        const BusEditActionState* previewState = nullptr);
    void showBusPresetPalette(
        const Lane& lane,
        const QPoint& anchor,
        std::optional<Tick> exactTick = std::nullopt,
        std::optional<std::pair<Tick, Tick>> exactRange = std::nullopt,
        BusEditScope exactRangeScope = BusEditScope::Segment);
    void hideBusPresetPalette();
    bool editSelectedSegmentValue(const QString& seed = {});
    bool advanceBusValueEdit(
        const std::string& laneId,
        const std::pair<Tick, Tick>& currentRange,
        bool forward);
    bool advanceBusSegmentValueEdit(
        const std::string& laneId,
        const std::pair<Tick, Tick>& currentRange,
        bool forward);
    bool cycleEnumEditorSymbol(bool forward);
    bool stepBusEditorValue(bool upward);
    bool cycleBusRecentValue(bool forward);
    void rememberBusValue(const std::string& laneId, const QString& value);
    [[nodiscard]] BusDraftValues parseBusDraftValues(
        const Lane& lane) const;
    [[nodiscard]] BusDraftValues parseBusDraftValues(
        const Lane& lane,
        const QString& entered,
        bool useEditorRadix) const;
    [[nodiscard]] QString busEditorValue(const Lane& lane) const;
    [[nodiscard]] QString busEditorValue(
        const Lane& lane,
        const QString& entered) const;
    void showRangeEditPalette();
    void hideRangeEditPalette();
    void clearExplicitRangeSelection(bool clearLanes = true);
    [[nodiscard]] BitPatternDraft parseBitPatternDraft(
        const QString& entered) const;
    [[nodiscard]] BitPatternAssessment assessBitPatternDraft(
        const QString& entered) const;
    bool submitBitPattern();
    [[nodiscard]] BusRangeSequenceAssessment
    assessBusRangeSequenceDraft(const QString& entered) const;
    [[nodiscard]] RangeSequenceSeed
    currentRangeSequenceSeed() const;
    void loadCurrentRangeSequence();
    void clearRangeSequenceBaseline();
    [[nodiscard]] bool rangeSequenceBaselineMatchesContext() const noexcept;
    bool restoreRangeSequenceBaseline();
    void clearRangeSequenceCaretTarget();
    void clearRangeSequenceAnchor();
    void updateRangeSequenceCaretTarget(bool announce = true);
    bool navigateRangeSequenceTarget(bool forward);
    [[nodiscard]] bool hasActiveRangeSequenceTokenMapping() const noexcept;
    [[nodiscard]] bool rangeSequenceTokenAt(const QPoint& position) const;
    [[nodiscard]] const RangeSequenceTextSpan*
    rangeSequenceTextSpanAt(
        const std::string& laneId,
        Tick tick) const;
    bool selectRangeSequenceTokenAt(const QPoint& position);
    bool beginExplicitRangeMove(
        const QPoint& position,
        Qt::KeyboardModifiers modifiers);
    bool submitBusRangeSequence();
    [[nodiscard]] HistorySelectionSnapshot historySelectionSnapshot() const;
    void rememberHistorySelectionTransition(
        std::uint64_t beforeStateId,
        const HistorySelectionSnapshot& beforeSelection,
        std::uint64_t afterStateId);
    [[nodiscard]] bool explicitRangeContains(const QPoint& position) const;
    [[nodiscard]] std::optional<LaneKind> explicitRangeKind() const;
    [[nodiscard]] QStringList explicitRangeEnumSymbols() const;
    [[nodiscard]] RangePasteAvailability rangePasteAvailability() const;
    [[nodiscard]] RangeClearAvailability rangeClearAvailability() const;
    [[nodiscard]] RangeRepeatAvailability rangeRepeatAvailability() const;
    [[nodiscard]] RangeValueAvailability rangeValueAvailability(
        const std::string& value,
        const std::string& presetId = {}) const;
    [[nodiscard]] RangePasteAvailability pasteAvailabilityForTargets(
        const std::vector<std::string>& targetLaneIds,
        Tick pasteStart,
        std::optional<Tick> selectedWidth = std::nullopt) const;
    [[nodiscard]] std::optional<LaneHeaderPastePreview>
    laneHeaderPastePreview() const;
    [[nodiscard]] std::optional<LaneHeaderPastePreview>
    explicitRangePastePreview() const;
    [[nodiscard]] std::optional<LaneHeaderPastePreview>
    pastePreview() const;
    [[nodiscard]] std::optional<LaneHeaderPastePreview>
    buildRepeatPreview() const;
    [[nodiscard]] std::optional<LaneHeaderPastePreview>
    buildPastePreview(
        const std::vector<std::string>& targetLaneIds,
        Tick pasteStart) const;
    [[nodiscard]] std::optional<LaneHeaderPastePreview>
    buildCopiedRangePreview(
        const std::vector<CopiedLaneRange>& copiedLanes,
        Tick destination,
        Tick duration,
        RangePreviewOperation operation) const;
    [[nodiscard]] RangePasteAvailability pasteAvailabilityWithImpact(
        const std::vector<std::string>& targetLaneIds,
        Tick pasteStart,
        std::optional<Tick> selectedWidth = std::nullopt) const;
    [[nodiscard]] RelationRemovalImpact relationRemovalImpactForProjectedLanes(
        const std::vector<Lane>& projectedLanes) const;
    [[nodiscard]] QString laneHeaderPasteHint() const;
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
        JsonExtensions extensions = {},
        const RelationRemovalImpact* predictedRelationImpact = nullptr);
    bool clearSelectedBeatRange();
    void clearSelectedSegment();
    bool duplicateSelectedSegment(bool after);
    bool nudgeSelectedSegment(bool forward);
    bool resizeSelectedSegmentBoundary(
        SegmentBoundary boundary,
        bool expand);
    void updateLaneDropTarget(int y);
    void updateLaneDragAutoScroll(int pointerY);
    void advanceLaneDragAutoScroll();
    void stopLaneDragAutoScroll();
    void updateWaveEditDragAutoScroll(
        const QPoint& pointerPosition,
        Qt::KeyboardModifiers modifiers);
    void advanceWaveEditDragAutoScroll();
    void stopWaveEditDragAutoScroll();
    void cancelExplicitRangeDrag(const QString& message);
    void updateRangeTransferTargetLanes(const Lane* pointerLane);
    [[nodiscard]] bool rangeTransferTargetOverlapsSource() const;
    [[nodiscard]] QString rangeTransferTargetSummary() const;
    [[nodiscard]] std::optional<RangeTransferProjection>
    buildRangeTransferProjection() const;
    [[nodiscard]] QString rangeTransferPreviewStatus(
        const RangeTransferProjection& projection) const;
    [[nodiscard]] std::optional<SegmentEditProjection>
    buildSegmentEditProjection() const;
    [[nodiscard]] std::optional<SegmentEditProjection>
    buildSegmentEditProjection(
        SegmentEditOperation operation,
        const std::string& laneId,
        const std::string& segmentId,
        Tick originalStart,
        Tick originalEnd,
        Tick start,
        Tick end) const;
    [[nodiscard]] std::optional<SegmentEditProjection>
    activeSegmentEditProjection() const;
    [[nodiscard]] SegmentActionAssessment
    assessSelectedSegmentAction(SegmentAction action) const;
    [[nodiscard]] QString segmentEditPreviewStatus(
        const SegmentEditProjection& projection) const;
    void commitLaneReorder();
    bool applyVisibleTimeSpan(Tick span, Tick anchorTick);
    void schedulePendingVisibleTimeSpanRestore();
    void setScale(double scale, int anchorX);
    [[nodiscard]] int zoomAnchorX() const;
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
    [[nodiscard]] MarkerHit markerHitAtPosition(const QPoint& position) const;
    [[nodiscard]] std::pair<Tick, Tick> markerDisplayRange(const Marker& marker) const;
    [[nodiscard]] QString markerLocationText(const Marker& marker) const;
    [[nodiscard]] std::string nextLockedMarkerName(bool interval) const;
    void clearMarkerSelection();
    void selectOnlyMarker(const std::string& markerId);
    [[nodiscard]] bool markerSelected(const std::string& markerId) const noexcept;
    [[nodiscard]] Tick cursorKeyboardStep() const;
    [[nodiscard]] std::optional<std::pair<Tick, Tick>>
    explicitRangeAnchorAndActive() const noexcept;
    [[nodiscard]] std::optional<Tick> adjacentKeyboardTick(
        Tick from,
        bool forward,
        const Lane* lane) const;
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
    [[nodiscard]] bool navigateSelectedBeat(bool forward);
    void navigateSelectedSegment(bool forward);
    void navigateTimelinePage(bool forward);
    void clearWaveEditState();
    void commitWaveEdit(const QPoint& releasePosition);
    void updateRulerScrub(int x, bool final, bool rangeCleared = false);
    void cancelRulerScrub();
    [[nodiscard]] Tick constrainedTransitionTick(const Event& event, Tick requested) const;
    void commitBitToggle(const std::vector<std::pair<Tick, Tick>>& beats, const QPoint& position);
    void editSegmentAt(const QPoint& position);
    void ensureCursorVisible(Tick tick);
    void removeSelectedMarker();
    void removeSelectedRelations();
    void moveSelectedMarkerBy(Tick delta);
    [[nodiscard]] Tick snappedTick(Tick input, const Lane* lane) const;
    [[nodiscard]] Tick majorTickStep() const;
    [[nodiscard]] std::pair<Tick, Tick> visibleTickRange() const;
    [[nodiscard]] std::pair<std::size_t, std::size_t>
    visibleLaneLayoutRange() const;
    [[nodiscard]] std::pair<std::size_t, std::size_t>
    visibleOverlayRange(
        const std::vector<TimedOverlayIndex>& index,
        Tick visibleStart,
        Tick visibleEnd) const;
    void commitDraw(const QPoint& releasePosition);
    void commitMarker(const QPoint& releasePosition);
    void commitRelation(const QPoint& releasePosition);
    void commitTransition(const QPoint& releasePosition);
    [[nodiscard]] const Event* eventAtPosition(const QPoint& position) const;
    [[nodiscard]] QPoint eventPoint(const Event& event) const;
    [[nodiscard]] RelationHit relationHitAtPosition(
        const QPoint& position) const;
    [[nodiscard]] Relation* relationById(const std::string& relationId);
    [[nodiscard]] const Relation* relationById(
        const std::string& relationId) const;
    void clearRelationSelection();
    void selectOnlyRelation(const std::string& relationId);
    [[nodiscard]] bool isRelationSelected(
        const std::string& relationId) const noexcept;
    void rebuildOverlayIndexes();

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
        Tick visibleEnd,
        bool preview = false);
    void drawBitSegments(
        class QPainter& painter,
        const Lane& lane,
        const QRect& rect,
        Tick visibleStart,
        Tick visibleEnd,
        bool preview = false);
    void drawBusSegments(
        class QPainter& painter,
        const Lane& lane,
        const QRect& rect,
        Tick visibleStart,
        Tick visibleEnd,
        bool preview = false);
    void drawScenarioOverlays(
        class QPainter& painter,
        Tick visibleStart,
        Tick visibleEnd,
        const std::vector<std::string>* relationRemovalIds);
    void drawCursorOverlays(
        class QPainter& painter,
        Tick visibleStart,
        Tick visibleEnd);
    void drawWaveEditOverlay(
        class QPainter& painter,
        const RangeTransferProjection* transferProjection,
        const SegmentEditProjection* segmentProjection,
        const BusEditProjection* busEditProjection);
    void drawLaneHeaderPastePreview(
        class QPainter& painter,
        const LaneHeaderPastePreview* pastePreview);
    void drawRangeTransferWaveformPreview(
        class QPainter& painter,
        const RangeTransferProjection* transferProjection);
    void drawSegmentEditWaveformPreview(
        class QPainter& painter,
        const SegmentEditProjection* segmentProjection);
    void drawProjectedLaneWaveformPreview(
        class QPainter& painter,
        const Lane& lane,
        Tick start,
        Tick end,
        const RelationRemovalImpact& relationImpact,
        bool modelChanges);
    void drawEditGuide(class QPainter& painter);
    void drawWaveEditTransitionPreview(class QPainter& painter);
    void drawLaneReorderOverlay(class QPainter& painter);

    Project* project_{nullptr};
    ScenarioRef scenario_;
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
    std::optional<Tick> durationEditPointerRetargetTick_;
    QPoint durationEditPointerRetargetPressPosition_;
    QFrame* busPresetPalette_{nullptr};
    QLabel* busPresetContextLabel_{nullptr};
    QLineEdit* busValueEdit_{nullptr};
    QComboBox* busRadixCombo_{nullptr};
    QComboBox* busRecentValuesCombo_{nullptr};
    QToolButton* busScopeButton_{nullptr};
    QToolButton* busPreviousButton_{nullptr};
    QToolButton* busNextButton_{nullptr};
    QToolButton* busClearButton_{nullptr};
    QToolButton* busApplyButton_{nullptr};
    QToolButton* busCloseButton_{nullptr};
    std::array<QToolButton*, 5> busPresetButtons_{};
    QCompleter* laneValueCompleter_{nullptr};
    QStringListModel* laneValueCompletionModel_{nullptr};
    std::optional<Tick> busPresetAnchorTick_;
    std::optional<std::pair<Tick, Tick>> busEditRange_;
    std::string busPresetLaneId_;
    std::map<std::string, QStringList> busRecentValues_;
    BusEditScope busEditScope_{BusEditScope::Beat};
    bool busEditPaletteVisible_{false};
    std::optional<BusEditProjection> busEditActionPreview_;
    std::optional<BusEditAction> busEditPreviewAction_;
    QFrame* rangeEditPalette_{nullptr};
    QLabel* rangeEditContextLabel_{nullptr};
    QToolButton* rangeCopyButton_{nullptr};
    QToolButton* rangeRepeatButton_{nullptr};
    QToolButton* rangePasteButton_{nullptr};
    QToolButton* rangeClearButton_{nullptr};
    QLineEdit* rangeValueEdit_{nullptr};
    QAction* rangeLoadValuesAction_{nullptr};
    QCompleter* rangeValueCompleter_{nullptr};
    QStringListModel* rangeValueCompletionModel_{nullptr};
    QToolButton* rangeZeroButton_{nullptr};
    QToolButton* rangeOneButton_{nullptr};
    QToolButton* rangeXButton_{nullptr};
    QToolButton* rangeZButton_{nullptr};
    QToolButton* rangeDontCareButton_{nullptr};
    QToolButton* rangeReservedButton_{nullptr};
    QToolButton* rangeClockGateButton_{nullptr};
    QToolButton* rangeClockDisableButton_{nullptr};
    QToolButton* rangeCloseButton_{nullptr};
    bool rangeEditPaletteVisible_{false};
    bool relationsVisible_{false};
    bool rangeRepeatPreviewActive_{false};
    std::optional<BitPatternProjection> bitPatternPreview_;
    std::optional<BusRangeSequenceProjection>
        busRangeSequencePreview_;
    std::vector<RangeSequenceTextSpan>
        rangeSequenceTextSpans_;
    std::optional<RangeSequenceTextSpan>
        rangeSequenceCaretTarget_;
    std::optional<RangeSequenceBaseline>
        rangeSequenceBaseline_;
    std::optional<RangeSequenceAnchor>
        rangeSequenceAnchor_;
    bool rangeSequenceTokenPress_{false};
    bool rangeSequenceTokenDragRejected_{false};
    QPoint rangeSequenceTokenPressPosition_;
    bool explicitRangeSelection_{false};
    std::vector<LaneLayout> laneLayout_;
    std::map<std::string, std::size_t> laneLayoutIndexById_;
    std::vector<TimedOverlayIndex> markerOverlayIndex_;
    std::vector<TimedOverlayIndex> relationOverlayIndex_;
    std::vector<std::size_t> eventOverlayIndex_;
    std::size_t indexedMarkerCount_{0};
    std::size_t indexedRelationCount_{0};
    std::size_t indexedEventCount_{0};
    std::vector<DifferenceRange> differenceRanges_;
    std::set<std::string> collapsedGroupIds_;
    std::vector<Tick> signalEdgeIndex_;
    std::optional<Tick> movableCursorTick_;
    std::optional<Tick> temporaryCursorTick_;
    std::string selectedMarkerId_;
    std::vector<std::string> selectedMarkerIds_;
    CursorInteraction cursorInteraction_{CursorInteraction::None};
    std::optional<std::pair<Tick, Tick>> lockedMarkerOriginalRange_;
    std::vector<std::string> selectedRelationIds_;
    std::string activeRelationId_;
    RelationInteraction relationInteraction_{RelationInteraction::None};
    Tool tool_{Tool::WaveEdit};
    double pixelsPerTick_{0.003};
    std::string selectedLaneId_;
    std::string selectedSegmentLaneId_;
    std::string selectedSegmentId_;
    std::optional<SegmentEditProjection> segmentActionPreview_;
    WaveEditInteraction waveEditInteraction_{WaveEditInteraction::None};
    std::optional<std::pair<Tick, Tick>> waveEditOriginalRange_;
    std::optional<std::pair<Tick, Tick>> waveEditPreviewRange_;
    std::vector<std::string> waveEditRangeTargetLaneIds_;
    std::size_t waveEditRangeGrabLaneOffset_{0};
    bool waveEditRangeTargetValid_{true};
    QString waveEditRangeTargetError_;
    std::string waveEditHoverLaneId_;
    std::optional<std::pair<Tick, Tick>> waveEditHoverRange_;
    QPoint waveEditPressPosition_;
    Tick waveEditGrabOffset_{0};
    bool laneHeaderPressed_{false};
    bool laneHeaderDragging_{false};
    bool laneHeaderSelectionActive_{false};
    std::string laneHeaderSelectionAnchorId_;
    QPoint laneHeaderPressPosition_;
    int laneDragOriginalVerticalScroll_{0};
    std::string laneDragId_;
    std::vector<std::string> laneDragIds_;
    bool laneHeaderPressPreservesMultiSelection_{false};
    std::optional<std::size_t> laneDropInsertionSlot_;
    std::optional<std::size_t> laneDropDestinationIndex_;
    std::optional<int> laneDropIndicatorY_;
    std::string laneDropGroupId_;
    QTimer* laneDragAutoScrollTimer_{nullptr};
    int laneDragAutoScrollDirection_{0};
    int laneDragAutoScrollPointerY_{0};
    QTimer* waveEditDragAutoScrollTimer_{nullptr};
    int waveEditDragAutoScrollDirection_{0};
    int waveEditDragVerticalAutoScrollDirection_{0};
    QPoint waveEditDragAutoScrollPointer_;
    Qt::KeyboardModifiers waveEditDragAutoScrollModifiers_{Qt::NoModifier};
    int waveEditDragOriginalHorizontalScroll_{0};
    int waveEditDragOriginalVerticalScroll_{0};
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
    std::vector<RelationHitRegion> relationHitRegions_;
    std::uint64_t modelGeneration_{0};
    std::uint64_t viewGeneration_{0};
    std::uint64_t lastPaintViewGeneration_{0};
    bool fitPending_{false};
    std::optional<std::pair<Tick, Tick>> pendingVisibleTimeSpanRestore_;
    bool pendingVisibleTimeSpanRestoreScheduled_{false};
    std::optional<std::pair<Tick, Tick>> selectionRange_;
    int selectionStartY_{0};
    bool bypassSnap_{false};
    std::optional<Tick> snapGuideTick_;
    bool panning_{false};
    bool spaceHeld_{false};
    QPoint panPressPosition_;
    int panStartHorizontal_{0};
    int panStartVertical_{0};
    bool rulerScrubbing_{false};
    bool rulerScrubClearedRange_{false};
    Tick rulerScrubOriginalTick_{0};
    int rulerScrubOriginalHorizontalScroll_{0};
    std::optional<HistorySelectionSnapshot> rulerScrubOriginalSelection_;
    std::map<
        std::pair<std::uint64_t, std::uint64_t>,
        HistorySelectionSnapshot>
        historySelectionTransitions_;
    std::optional<std::pair<std::uint64_t, HistorySelectionSnapshot>>
        pendingCommandSelectionTransition_;
    std::map<std::string, DocumentContext> documentContexts_;
};

} // namespace wave
