#pragma once

#include "wave/commands.h"
#include "wave/model.h"

#include <QAbstractScrollArea>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QStringList>

#include <cstddef>
#include <optional>
#include <vector>

class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QToolButton;
class QWheelEvent;

namespace wave {

class WaveCanvas final : public QAbstractScrollArea {
    Q_OBJECT

public:
    enum class Tool {
        Selection,
        Draw,
        Transition,
        Marker,
        Relation,
    };

    explicit WaveCanvas(QWidget* parent = nullptr);

    void setDocument(Project* project, Scenario* scenario, CommandStack* commandStack);
    void setTool(Tool tool);
    void setSnapMode(SnapMode mode);

    [[nodiscard]] Tool tool() const noexcept;
    [[nodiscard]] SnapMode snapMode() const noexcept;
    [[nodiscard]] QString selectedLaneId() const;
    [[nodiscard]] QStringList selectedLaneIds() const;
    [[nodiscard]] Tick cursorTick() const noexcept;
    [[nodiscard]] std::optional<Tick> movableCursorTick() const noexcept;
    [[nodiscard]] std::optional<Tick> temporaryCursorTick() const noexcept;
    [[nodiscard]] QString selectedMarkerId() const;
    [[nodiscard]] std::optional<std::pair<Tick, Tick>> selectedTimeRange() const noexcept;

public slots:
    void zoomIn();
    void zoomOut();
    void fitScenario();
    void fitSelection();
    void refreshModel();
    void revealLocation(const QString& laneId, qint64 tick);
    void copySelection();
    void pasteAtCursor();
    void insertPulse();

signals:
    void addLaneRequested();
    void selectionChanged(const QString& laneId, qint64 tick);
    void modelEdited();
    void commandAvailabilityChanged();
    void statusMessage(const QString& message);
    void eventSelected(const QString& eventId);

protected:
    void keyPressEvent(QKeyEvent* event) override;
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

    static constexpr int HeaderWidth = 190;
    static constexpr int RulerHeight = 40;
    static constexpr int AddLaneRowHeight = 48;

    void rebuildLaneLayout();
    void rebuildSnapIndex();
    void updateScrollBars();
    void updateAddLaneButtonGeometry();
    void setScale(double scale, int anchorX);
    [[nodiscard]] double contentWidth() const;
    [[nodiscard]] int waveViewportWidth() const;
    [[nodiscard]] QRect addLaneRowRect() const;
    [[nodiscard]] Tick tickAtX(int x) const;
    [[nodiscard]] int xAtTick(Tick tick) const;
    [[nodiscard]] const LaneLayout* layoutAtY(int y) const;
    [[nodiscard]] Lane* laneAtY(int y);
    [[nodiscard]] const Lane* laneAtY(int y) const;
    [[nodiscard]] Marker* markerById(const std::string& markerId);
    [[nodiscard]] const Marker* markerById(const std::string& markerId) const;
    [[nodiscard]] const Marker* markerAtPosition(const QPoint& position) const;
    [[nodiscard]] std::pair<Tick, Tick> markerDisplayRange(const Marker& marker) const;
    [[nodiscard]] Tick cursorKeyboardStep() const;
    [[nodiscard]] QString cursorValue(const Lane& lane) const;
    [[nodiscard]] QString cursorDeltaText(Tick from, Tick to) const;
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

    Project* project_{nullptr};
    Scenario* scenario_{nullptr};
    CommandStack* commandStack_{nullptr};
    QToolButton* addLaneButton_{nullptr};
    std::vector<LaneLayout> laneLayout_;
    std::vector<Tick> signalEdgeIndex_;
    std::vector<Tick> markerTickIndex_;
    std::optional<Tick> movableCursorTick_;
    std::optional<Tick> temporaryCursorTick_;
    std::string selectedMarkerId_;
    CursorInteraction cursorInteraction_{CursorInteraction::None};
    std::optional<std::pair<Tick, Tick>> lockedMarkerOriginalRange_;
    Tool tool_{Tool::Selection};
    SnapMode snapMode_{SnapMode::FixedGrid};
    double pixelsPerTick_{0.003};
    std::string selectedLaneId_;
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
};

} // namespace wave
