#pragma once

#include "wave/model.h"
#include "wave/timeline_viewport.h"
#include "wave/trace.h"
#include "wave/widgets_export.h"

#include <QAbstractScrollArea>

#include <set>
#include <string>
#include <vector>

namespace wave {

class WAVEWIDGETS_API TraceCanvas final : public QAbstractScrollArea {
    Q_OBJECT

public:
    explicit TraceCanvas(QWidget* parent = nullptr);

    void setTrace(
        const Project* project,
        const Scenario* scenario,
        const TraceIndex* trace,
        const ImportedTrace* reference);
    void setVisibleSignalIds(const std::set<std::string>& signalIds);
    void setDifferenceRanges(std::vector<std::pair<Tick, Tick>> ranges);
    void refreshTrace();
    void revealTick(Tick tick);
    void revealSignal(const QString& signalId);
    void setActiveSignal(const QString& signalId);

public slots:
    void fitTrace();
    void zoomIn();
    void zoomOut();

signals:
    void cursorChanged(qint64 tick, const QString& signalId, const QString& value);
    void signalActivated(const QString& signalId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    [[nodiscard]] Tick visibleStart() const noexcept;
    [[nodiscard]] Tick visibleEnd() const noexcept;
    [[nodiscard]] double tickToX(Tick tick) const noexcept;
    [[nodiscard]] Tick xToTick(double x) const noexcept;
    void updateScrollBars();
    void setViewStart(Tick tick);
    void setZoom(double pixelsPerTick, double anchorX);
    [[nodiscard]] const Lane* mappedLane(const TraceSignal& signal) const noexcept;

    const Project* project_{nullptr};
    const Scenario* scenario_{nullptr};
    const TraceIndex* trace_{nullptr};
    const ImportedTrace* reference_{nullptr};
    std::vector<std::size_t> visibleSignalIndices_;
    std::vector<std::pair<Tick, Tick>> differenceRanges_;
    TimelineViewport timeline_{0, 0, 0.01, 230.0, 0.0};
    bool fitPending_{false};
    std::string activeSignalId_;
};

} // namespace wave
