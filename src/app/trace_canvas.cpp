#include "trace_canvas.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>

namespace wave {
namespace {

constexpr int kNameWidth = 230;
constexpr int kRulerHeight = 34;
constexpr int kRowHeight = 44;
constexpr int kScrollResolution = 1'000'000;

QColor valueColor(const std::string& value)
{
    if (value.find('X') != std::string::npos || value.find('x') != std::string::npos) {
        return QColor(239, 108, 115);
    }
    if (value.find('Z') != std::string::npos || value.find('z') != std::string::npos) {
        return QColor(255, 183, 77);
    }
    return QColor(38, 166, 154);
}

bool isHigh(const std::string& value)
{
    return value == "1" || value == "H" || value == "h";
}

bool isLow(const std::string& value)
{
    return value == "0" || value == "L" || value == "l";
}

Tick clampedRound(const long double value)
{
    if (value <= static_cast<long double>(std::numeric_limits<Tick>::min())) {
        return std::numeric_limits<Tick>::min();
    }
    if (value >= static_cast<long double>(std::numeric_limits<Tick>::max())) {
        return std::numeric_limits<Tick>::max();
    }
    return static_cast<Tick>(std::llround(value));
}

Tick niceGridStep(const double pixelsPerTick)
{
    if (!(pixelsPerTick > 0.0)) return 1;
    const auto raw = 110.0L / static_cast<long double>(pixelsPerTick);
    const auto exponent = std::floor(std::log10(std::max(1.0L, raw)));
    const auto scale = std::pow(10.0L, exponent);
    const auto normalized = raw / scale;
    const auto multiplier = normalized <= 1.0L ? 1.0L : normalized <= 2.0L ? 2.0L : normalized <= 5.0L ? 5.0L : 10.0L;
    return std::max<Tick>(1, clampedRound(multiplier * scale));
}

Tick floorToStep(const Tick value, const Tick step)
{
    auto quotient = value / step;
    if (value < 0 && value % step != 0) --quotient;
    return quotient * step;
}

} // namespace

TraceCanvas::TraceCanvas(QWidget* parent)
    : QAbstractScrollArea(parent)
{
    setObjectName(QStringLiteral("ActualTraceCanvas"));
    setMouseTracking(true);
    setFrameShape(QFrame::NoFrame);
    horizontalScrollBar()->setRange(0, 0);
    verticalScrollBar()->setRange(0, 0);
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, viewport(), qOverload<>(&QWidget::update));
    connect(verticalScrollBar(), &QScrollBar::valueChanged, viewport(), qOverload<>(&QWidget::update));
}

void TraceCanvas::setTrace(
    const Project* project,
    const Scenario* scenario,
    const TraceIndex* trace,
    const ImportedTrace* reference)
{
    project_ = project;
    scenario_ = scenario;
    trace_ = trace;
    reference_ = reference;
    visibleSignalIndices_.clear();
    if (trace_) {
        timeline_.setDomain(trace_->startTick, trace_->endTick);
        visibleSignalIndices_.reserve(trace_->traceSignals.size());
        for (std::size_t index = 0; index < trace_->traceSignals.size(); ++index) {
            visibleSignalIndices_.push_back(index);
        }
    }
    fitPending_ = trace_ != nullptr;
    updateScrollBars();
    if (viewport()->width() > kNameWidth + 20 && fitPending_) fitTrace();
    viewport()->update();
}

void TraceCanvas::setVisibleSignalIds(const std::set<std::string>& signalIds)
{
    visibleSignalIndices_.clear();
    if (trace_) {
        for (std::size_t index = 0; index < trace_->traceSignals.size(); ++index) {
            if (signalIds.contains(trace_->traceSignals[index].id)) {
                visibleSignalIndices_.push_back(index);
            }
        }
    }
    verticalScrollBar()->setValue(0);
    updateScrollBars();
    viewport()->update();
}

void TraceCanvas::setDifferenceRanges(std::vector<std::pair<Tick, Tick>> ranges)
{
    differenceRanges_ = std::move(ranges);
    viewport()->update();
}

void TraceCanvas::refreshTrace()
{
    updateScrollBars();
    viewport()->update();
}

void TraceCanvas::revealTick(const Tick tick)
{
    if (!trace_) return;
    const auto span = visibleEnd() - visibleStart();
    setViewStart(tick - span / 2);
}

void TraceCanvas::revealSignal(const QString& signalId)
{
    if (!trace_ || signalId.isEmpty()) return;
    const auto row = std::find_if(
        visibleSignalIndices_.begin(),
        visibleSignalIndices_.end(),
        [this, &signalId](const std::size_t index) {
            return QString::fromStdString(trace_->traceSignals[index].id)
                == signalId;
        });
    if (row == visibleSignalIndices_.end()) return;
    const auto rowIndex = static_cast<int>(
        std::distance(visibleSignalIndices_.begin(), row));
    const auto visibleHeight = std::max(0, viewport()->height() - kRulerHeight);
    const auto target = std::max(
        0,
        rowIndex * kRowHeight - std::max(0, visibleHeight - kRowHeight) / 2);
    verticalScrollBar()->setValue(target);
    viewport()->update();
}

void TraceCanvas::fitTrace()
{
    if (!trace_) return;
    const auto width = std::max(1, viewport()->width() - kNameWidth - 12);
    timeline_.setPixelsPerTick(TimelineViewport::fittedPixelsPerTick(
        trace_->startTick, trace_->endTick, width));
    fitPending_ = false;
    updateScrollBars();
    horizontalScrollBar()->setValue(0);
    viewport()->update();
}

void TraceCanvas::zoomIn()
{
    setZoom(timeline_.pixelsPerTick() * 1.35,
            (viewport()->width() + kNameWidth) / 2.0);
}

void TraceCanvas::zoomOut()
{
    setZoom(timeline_.pixelsPerTick() / 1.35,
            (viewport()->width() + kNameWidth) / 2.0);
}

Tick TraceCanvas::visibleStart() const noexcept
{
    if (!trace_) return 0;
    const auto drawable = std::max(1, viewport()->width() - kNameWidth);
    const auto visibleSpan = static_cast<long double>(drawable)
        / timeline_.pixelsPerTick();
    const auto duration = static_cast<long double>(trace_->endTick)
        - static_cast<long double>(trace_->startTick);
    const auto scrollable = std::max(0.0L, duration - visibleSpan);
    if (scrollable <= 0.0L || horizontalScrollBar()->maximum() <= 0) {
        return trace_->startTick;
    }
    const auto ratio = static_cast<long double>(horizontalScrollBar()->value())
        / horizontalScrollBar()->maximum();
    return clampedRound(static_cast<long double>(trace_->startTick) + ratio * scrollable);
}

Tick TraceCanvas::visibleEnd() const noexcept
{
    if (!trace_) return 0;
    const auto drawable = std::max(1, viewport()->width() - kNameWidth);
    const auto span = static_cast<long double>(drawable)
        / timeline_.pixelsPerTick();
    return std::min(
        trace_->endTick,
        clampedRound(static_cast<long double>(visibleStart()) + span));
}

double TraceCanvas::tickToX(const Tick tick) const noexcept
{
    auto viewport = timeline_;
    viewport.setPixelOffset(
        static_cast<double>(visibleStart() - trace_->startTick)
        * timeline_.pixelsPerTick());
    return viewport.pixelForTick(tick);
}

Tick TraceCanvas::xToTick(const double x) const noexcept
{
    if (!trace_) return 0;
    auto viewport = timeline_;
    viewport.setPixelOffset(
        static_cast<double>(visibleStart() - trace_->startTick)
        * timeline_.pixelsPerTick());
    return viewport.tickAtPixel(x);
}

void TraceCanvas::updateScrollBars()
{
    const auto rowCount = trace_ ? static_cast<int>(visibleSignalIndices_.size()) : 0;
    const auto visibleHeight = std::max(0, viewport()->height() - kRulerHeight);
    verticalScrollBar()->setRange(0, std::max(0, rowCount * kRowHeight - visibleHeight));
    verticalScrollBar()->setPageStep(visibleHeight);
    if (!trace_) {
        horizontalScrollBar()->setRange(0, 0);
        return;
    }
    const auto duration = static_cast<long double>(trace_->endTick)
        - static_cast<long double>(trace_->startTick);
    const auto drawable = std::max(1, viewport()->width() - kNameWidth);
    const auto visibleSpan = static_cast<long double>(drawable)
        / timeline_.pixelsPerTick();
    if (duration <= visibleSpan) {
        horizontalScrollBar()->setRange(0, 0);
    } else {
        horizontalScrollBar()->setRange(0, kScrollResolution);
        horizontalScrollBar()->setPageStep(
            std::clamp(
                static_cast<int>(kScrollResolution * visibleSpan / duration),
                1,
                kScrollResolution));
    }
}

void TraceCanvas::setViewStart(const Tick tick)
{
    if (!trace_ || horizontalScrollBar()->maximum() <= 0) return;
    const auto drawable = std::max(1, viewport()->width() - kNameWidth);
    const auto visibleSpan = static_cast<long double>(drawable)
        / timeline_.pixelsPerTick();
    const auto scrollable = std::max(
        0.0L,
        static_cast<long double>(trace_->endTick)
            - static_cast<long double>(trace_->startTick)
            - visibleSpan);
    if (scrollable <= 0.0L) {
        horizontalScrollBar()->setValue(0);
        return;
    }
    const auto relative = std::clamp(
        static_cast<long double>(tick) - static_cast<long double>(trace_->startTick),
        0.0L,
        scrollable);
    horizontalScrollBar()->setValue(
        static_cast<int>(std::llround(relative / scrollable * kScrollResolution)));
}

void TraceCanvas::setZoom(const double pixelsPerTick, const double anchorX)
{
    if (!trace_) return;
    const auto anchorTick = xToTick(anchorX);
    timeline_.setPixelsPerTick(pixelsPerTick);
    updateScrollBars();
    const auto desiredStart = clampedRound(
        static_cast<long double>(anchorTick)
        - static_cast<long double>(anchorX - kNameWidth)
            / timeline_.pixelsPerTick());
    setViewStart(desiredStart);
    viewport()->update();
}

const Lane* TraceCanvas::mappedLane(const TraceSignal& signal) const noexcept
{
    if (!scenario_ || !reference_) return nullptr;
    const auto iterator = std::find_if(
        reference_->signalMapping.begin(),
        reference_->signalMapping.end(),
        [&signal](const auto& mapping) {
            return mapping.second == signal.id;
        });
    return iterator == reference_->signalMapping.end()
        ? nullptr
        : findLane(*scenario_, iterator->first);
}

void TraceCanvas::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(viewport()->rect(), QColor(250, 251, 253));
    painter.fillRect(QRect(0, 0, kNameWidth, viewport()->height()), QColor(240, 243, 247));
    painter.fillRect(QRect(kNameWidth, 0, viewport()->width() - kNameWidth, kRulerHeight), QColor(247, 249, 252));
    painter.setPen(QColor(180, 190, 202));
    painter.drawLine(kNameWidth, 0, kNameWidth, viewport()->height());
    painter.drawLine(0, kRulerHeight, viewport()->width(), kRulerHeight);

    if (!trace_ || !project_) {
        painter.setPen(QColor(100, 110, 125));
        painter.drawText(
            QRect(kNameWidth, kRulerHeight, viewport()->width() - kNameWidth, viewport()->height() - kRulerHeight),
            Qt::AlignCenter,
            tr("Import a standard VCD or CSV trace."));
        return;
    }

    const auto start = visibleStart();
    const auto end = visibleEnd();
    const auto gridStep = niceGridStep(timeline_.pixelsPerTick());
    const auto firstGrid = floorToStep(start, gridStep);
    painter.setFont(QFont(painter.font().family(), 8));
    for (auto tick = firstGrid; tick <= end; ) {
        const auto x = tickToX(tick);
        if (x >= kNameWidth) {
            painter.setPen(QColor(218, 225, 234));
            painter.drawLine(QPointF(x, kRulerHeight), QPointF(x, viewport()->height()));
            painter.setPen(QColor(80, 91, 107));
            painter.drawText(
                QRectF(x + 3, 0, 110, kRulerHeight - 2),
                Qt::AlignLeft | Qt::AlignVCenter,
                QString::fromStdString(formatTick(tick, project_->timeBase)));
        }
        if (tick > std::numeric_limits<Tick>::max() - gridStep) break;
        tick += gridStep;
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(198, 40, 40, 34));
    for (const auto& [differenceStart, differenceEnd] : differenceRanges_) {
        if (differenceEnd < start || differenceStart > end) continue;
        const auto left = tickToX(std::max(start, differenceStart));
        const auto right = tickToX(std::min(end, std::max(differenceStart, differenceEnd)));
        painter.drawRect(
            QRectF(
                left,
                kRulerHeight,
                std::max(2.0, right - left),
                viewport()->height() - kRulerHeight));
    }

    const auto verticalOffset = verticalScrollBar()->value();
    const auto firstRow = std::max(0, verticalOffset / kRowHeight);
    const auto lastRow = std::min(
        static_cast<int>(visibleSignalIndices_.size()),
        firstRow + viewport()->height() / kRowHeight + 3);
    painter.setFont(QFont(painter.font().family(), 9));
    for (int row = firstRow; row < lastRow; ++row) {
        const auto signalIndex = visibleSignalIndices_[static_cast<std::size_t>(row)];
        const auto& signal = trace_->traceSignals[signalIndex];
        const auto top = kRulerHeight + row * kRowHeight - verticalOffset;
        const auto bottom = top + kRowHeight;
        if (bottom < kRulerHeight || top > viewport()->height()) continue;
        if ((row & 1) != 0) {
            painter.fillRect(QRect(0, top, viewport()->width(), kRowHeight), QColor(247, 249, 252));
        }
        painter.setPen(QColor(222, 228, 236));
        painter.drawLine(0, bottom, viewport()->width(), bottom);
        painter.setPen(QColor(39, 50, 66));
        auto name = QString::fromStdString(signal.fullName);
        if (const auto* lane = mappedLane(signal)) {
            name += tr("  →  %1").arg(QString::fromStdString(lane->name));
        }
        painter.drawText(
            QRect(8, top, kNameWidth - 16, kRowHeight),
            Qt::AlignLeft | Qt::AlignVCenter,
            painter.fontMetrics().elidedText(name, Qt::ElideMiddle, kNameWidth - 18));

        const auto transitions = signal.visibleTransitions(start, end, true);
        if (transitions.empty()) continue;
        if (signal.width == 1) {
            for (std::size_t index = 0; index < transitions.size(); ++index) {
                const auto& transition = transitions[index];
                const auto segmentStart = std::max(start, transition.tick);
                const auto segmentEnd = index + 1 < transitions.size()
                    ? std::min(end, transitions[index + 1].tick)
                    : end;
                if (segmentEnd < segmentStart) continue;
                const auto color = valueColor(transition.value);
                painter.setPen(QPen(color, 1.5));
                if (isHigh(transition.value) || isLow(transition.value)) {
                    const auto y = isHigh(transition.value) ? top + 11 : bottom - 11;
                    painter.drawLine(
                        QPointF(tickToX(segmentStart), y),
                        QPointF(tickToX(segmentEnd), y));
                    if (index + 1 < transitions.size()) {
                        const auto nextY = isHigh(transitions[index + 1].value)
                            ? top + 11
                            : isLow(transitions[index + 1].value)
                                ? bottom - 11
                                : top + kRowHeight / 2;
                        painter.drawLine(
                            QPointF(tickToX(transitions[index + 1].tick), y),
                            QPointF(tickToX(transitions[index + 1].tick), nextY));
                    }
                } else {
                    const QRectF box(
                        tickToX(segmentStart),
                        top + 9,
                        std::max(1.0, tickToX(segmentEnd) - tickToX(segmentStart)),
                        kRowHeight - 18);
                    painter.fillRect(box, color.lighter(175));
                    painter.drawRect(box);
                    painter.drawText(box, Qt::AlignCenter, QString::fromStdString(transition.value));
                }
            }
        } else {
            for (std::size_t index = 0; index < transitions.size(); ++index) {
                const auto& transition = transitions[index];
                const auto segmentStart = std::max(start, transition.tick);
                const auto segmentEnd = index + 1 < transitions.size()
                    ? std::min(end, transitions[index + 1].tick)
                    : end;
                if (segmentEnd < segmentStart) continue;
                const auto left = tickToX(segmentStart);
                const auto right = tickToX(segmentEnd);
                const auto middle = top + kRowHeight / 2.0;
                const auto color = valueColor(transition.value);
                painter.setPen(QPen(color, 1.4));
                painter.setBrush(color.lighter(188));
                QPolygonF polygon;
                polygon << QPointF(left + 4, top + 8)
                        << QPointF(right - 4, top + 8)
                        << QPointF(right, middle)
                        << QPointF(right - 4, bottom - 8)
                        << QPointF(left + 4, bottom - 8)
                        << QPointF(left, middle);
                painter.drawPolygon(polygon);
                if (right - left > 34) {
                    painter.setPen(QColor(35, 45, 58));
                    painter.drawText(
                        QRectF(left + 5, top + 7, right - left - 10, kRowHeight - 14),
                        Qt::AlignCenter,
                        QString::fromStdString(transition.value));
                }
            }
        }
    }
}

void TraceCanvas::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    if (fitPending_ && trace_) fitTrace();
    else updateScrollBars();
}

void TraceCanvas::wheelEvent(QWheelEvent* event)
{
    if ((event->modifiers() & Qt::ControlModifier) != 0) {
        const auto factor = event->angleDelta().y() > 0 ? 1.25 : 0.8;
        setZoom(timeline_.pixelsPerTick() * factor, event->position().x());
        event->accept();
        return;
    }
    if ((event->modifiers() & Qt::ShiftModifier) != 0) {
        horizontalScrollBar()->setValue(
            horizontalScrollBar()->value() - event->angleDelta().y() * 500);
        event->accept();
        return;
    }
    QAbstractScrollArea::wheelEvent(event);
}

void TraceCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if (!trace_ || event->position().x() < kNameWidth) {
        QAbstractScrollArea::mouseMoveEvent(event);
        return;
    }
    const auto tick = std::clamp(xToTick(event->position().x()), trace_->startTick, trace_->endTick);
    const auto row = (static_cast<int>(event->position().y()) - kRulerHeight
                      + verticalScrollBar()->value())
        / kRowHeight;
    if (event->position().y() >= kRulerHeight
        && row >= 0
        && row < static_cast<int>(visibleSignalIndices_.size())) {
        const auto signalIndex = visibleSignalIndices_[static_cast<std::size_t>(row)];
        const auto& signal = trace_->traceSignals[signalIndex];
        const auto* transition = signal.valueAt(tick);
        emit cursorChanged(
            tick,
            QString::fromStdString(signal.id),
            transition ? QString::fromStdString(transition->value) : QString{});
    }
    QAbstractScrollArea::mouseMoveEvent(event);
}

} // namespace wave
