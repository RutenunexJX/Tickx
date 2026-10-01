#include "wave/export.h"

#include <QBuffer>
#include <QColor>
#include <QDir>
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPolygonF>
#include <QSaveFile>
#include <QSvgGenerator>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <set>

namespace wave {
namespace {

constexpr int kRulerHeight = 44;
constexpr int kOuterMargin = 14;

struct NormalizedRange {
    Tick start;
    Tick end;
};

std::optional<NormalizedRange> normalizeRange(
    const Scenario& scenario,
    const ExportOptions& options,
    QString* error)
{
    const auto start = options.start.value_or(0);
    const auto end = options.end.value_or(scenario.duration);
    if (start < 0 || end <= start || end > scenario.duration) {
        if (error) {
            *error = QStringLiteral("Export range must satisfy 0 <= start < end <= scenario duration");
        }
        return std::nullopt;
    }
    if (options.width < 480 || options.laneHeight < 28 || options.pngDpi < 72) {
        if (error) *error = QStringLiteral("Export dimensions or DPI are invalid");
        return std::nullopt;
    }
    return NormalizedRange{start, end};
}

QColor parsedColor(const Lane& lane)
{
    const QColor color(QString::fromStdString(lane.color));
    return color.isValid() ? color : QColor(35, 117, 170);
}

Tick niceTickStep(const Tick duration, const int width)
{
    if (duration <= 0 || width <= 0) return 1;
    const auto raw = static_cast<double>(duration) / std::max(1.0, width / 110.0);
    const auto exponent = std::floor(std::log10(std::max(1.0, raw)));
    const auto magnitude = std::pow(10.0, exponent);
    const auto normalized = raw / magnitude;
    const auto multiplier = normalized <= 1.0 ? 1.0
        : normalized <= 2.0             ? 2.0
        : normalized <= 5.0             ? 5.0
                                         : 10.0;
    return std::max<Tick>(1, static_cast<Tick>(std::llround(multiplier * magnitude)));
}

Tick floorToStep(const Tick value, const Tick step)
{
    auto quotient = value / step;
    if (value < 0 && value % step != 0) --quotient;
    return quotient * step;
}

std::vector<const Lane*> visibleLanes(const Scenario& scenario)
{
    std::vector<const Lane*> lanes;
    for (const auto& lane : scenario.lanes) {
        if (lane.visible && lane.kind != LaneKind::Group) lanes.push_back(&lane);
    }
    return lanes;
}

class DocumentRenderer {
public:
    DocumentRenderer(
        const Project& project,
        const Scenario& scenario,
        const ExportOptions& options,
        const NormalizedRange range)
        : project_(project)
        , scenario_(scenario)
        , options_(options)
        , range_(range)
        , lanes_(visibleLanes(scenario))
    {
    }

    [[nodiscard]] int preferredHeight() const
    {
        return kOuterMargin * 2 + kRulerHeight
            + static_cast<int>(lanes_.size()) * options_.laneHeight;
    }

    void render(QPainter& painter, const QSize& size)
    {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(QRect(QPoint(0, 0), size), Qt::white);
        const QFont baseFont(QStringLiteral("Segoe UI"), 9);
        painter.setFont(baseFont);

        const QFontMetrics metrics(baseFont);
        int longestName = 0;
        for (const auto* lane : lanes_) {
            longestName = std::max(
                longestName,
                metrics.horizontalAdvance(QString::fromStdString(lane->name)));
        }
        const auto headerWidth = std::clamp(
            longestName + 34,
            170,
            std::max(170, size.width() / 2));
        const QRect plotRect(
            kOuterMargin + headerWidth,
            kOuterMargin + kRulerHeight,
            std::max(1, size.width() - headerWidth - kOuterMargin * 2),
            std::max(1, size.height() - kRulerHeight - kOuterMargin * 2));
        const auto scale = static_cast<double>(plotRect.width())
            / static_cast<double>(range_.end - range_.start);
        const auto xAt = [&](const Tick tick) {
            return plotRect.left()
                + static_cast<int>(std::llround((tick - range_.start) * scale));
        };

        if (options_.includeMarkers) {
            for (const auto& marker : scenario_.markers) {
                if (marker.end < range_.start || marker.start > range_.end) continue;
                const auto left = xAt(std::max(marker.start, range_.start));
                const auto right = xAt(std::min(marker.end, range_.end));
                QColor color = marker.kind == MarkerKind::Error
                    ? QColor(198, 40, 40)
                    : QColor(245, 166, 35);
                auto fill = color;
                fill.setAlpha(22);
                if (right > left) {
                    painter.fillRect(
                        QRect(left, plotRect.top(), right - left, plotRect.height()),
                        fill);
                }
                painter.setPen(QPen(color, 1.0, Qt::DashLine));
                painter.drawLine(left, plotRect.top(), left, plotRect.bottom());
                if (right > left) painter.drawLine(right, plotRect.top(), right, plotRect.bottom());
                if (options_.includeAnnotations) {
                    painter.setPen(color);
                    const auto annotationFont = painter.font();
                    auto markerFont = annotationFont;
                    markerFont.setPointSize(7);
                    painter.setFont(markerFont);
                    painter.drawText(
                        QRect(
                            left + 3,
                            kOuterMargin + kRulerHeight / 2,
                            180,
                            kRulerHeight / 2 - 2),
                        Qt::AlignLeft | Qt::AlignVCenter,
                        QString::fromStdString(marker.name));
                    painter.setFont(annotationFont);
                }
            }
        }

        const auto major = niceTickStep(range_.end - range_.start, plotRect.width());
        const auto minor = std::max<Tick>(1, major / 5);
        painter.setFont(QFont(QStringLiteral("Segoe UI"), 8));
        for (auto tick = floorToStep(range_.start, minor); tick <= range_.end;) {
            if (tick >= range_.start) {
                const auto x = xAt(tick);
                const auto isMajor = tick % major == 0;
                painter.setPen(isMajor ? QColor(194, 201, 210) : QColor(229, 233, 238));
                painter.drawLine(
                    x,
                    isMajor ? kOuterMargin + kRulerHeight - 9 : kOuterMargin + kRulerHeight - 5,
                    x,
                    plotRect.bottom());
                if (isMajor) {
                    painter.setPen(QColor(65, 73, 86));
                    const auto label = QString::fromStdString(
                        formatTick(tick, project_.timeBase));
                    const auto labelWidth = painter.fontMetrics().horizontalAdvance(label) + 10;
                    const auto labelLeft = std::clamp(
                        x - labelWidth / 2,
                        plotRect.left(),
                        std::max(plotRect.left(), plotRect.right() - labelWidth));
                    painter.drawText(
                        QRect(labelLeft, kOuterMargin, labelWidth, kRulerHeight / 2 + 2),
                        Qt::AlignHCenter | Qt::AlignVCenter,
                        label);
                }
            }
            if (tick > std::numeric_limits<Tick>::max() - minor) break;
            tick += minor;
        }

        painter.setFont(baseFont);
        std::map<std::string, QPoint> eventPoints;
        for (std::size_t laneIndex = 0; laneIndex < lanes_.size(); ++laneIndex) {
            const auto& lane = *lanes_[laneIndex];
            const auto top = plotRect.top() + static_cast<int>(laneIndex) * options_.laneHeight;
            const QRect rowRect(
                kOuterMargin,
                top,
                size.width() - kOuterMargin * 2,
                options_.laneHeight);
            const QRect waveRect(plotRect.left(), top, plotRect.width(), options_.laneHeight);
            painter.fillRect(
                QRect(kOuterMargin, top, headerWidth, options_.laneHeight),
                laneIndex % 2 == 0 ? QColor(247, 249, 251) : QColor(241, 244, 247));
            painter.setPen(QColor(35, 41, 50));
            painter.drawText(
                QRect(kOuterMargin + 8, top, headerWidth - 16, options_.laneHeight),
                Qt::AlignLeft | Qt::AlignVCenter,
                QString::fromStdString(lane.name));
            painter.save();
            painter.setClipRect(waveRect);
            drawLane(painter, lane, waveRect, xAt, scale);
            painter.restore();
            painter.setPen(QColor(218, 223, 229));
            painter.drawLine(rowRect.left(), rowRect.bottom(), rowRect.right(), rowRect.bottom());

            for (const auto& event : scenario_.events) {
                if (event.laneId != lane.id
                    || event.tick < range_.start
                    || event.tick > range_.end) {
                    continue;
                }
                const QPoint point(xAt(event.tick), top + options_.laneHeight / 2);
                eventPoints.emplace(event.id, point);
                const QColor color = event.action == EventAction::Expect
                    ? QColor(245, 124, 0)
                    : QColor(21, 101, 192);
                painter.setBrush(color);
                painter.setPen(Qt::white);
                painter.drawPolygon(QPolygon{
                    QPoint(point.x(), point.y() - 5),
                    QPoint(point.x() + 5, point.y()),
                    QPoint(point.x(), point.y() + 5),
                    QPoint(point.x() - 5, point.y()),
                });
            }
        }

        if (options_.includeRelations) {
            painter.setRenderHint(QPainter::Antialiasing, true);
            for (const auto& relation : scenario_.relations) {
                const auto source = eventPoints.find(relation.sourceEventId);
                const auto target = eventPoints.find(relation.targetEventId);
                if (source == eventPoints.end() || target == eventPoints.end()) continue;
                const QColor color = relation.severity == Severity::Error
                    ? QColor(198, 40, 40)
                    : relation.severity == Severity::Warning
                        ? QColor(239, 108, 0)
                        : QColor(25, 118, 210);
                painter.setPen(QPen(color, 1.5));
                painter.drawLine(source->second, target->second);
                const auto angle = std::atan2(
                    target->second.y() - source->second.y(),
                    target->second.x() - source->second.x());
                constexpr double length = 8.0;
                constexpr double spread = 0.55;
                const QPointF wingA(
                    target->second.x() - length * std::cos(angle - spread),
                    target->second.y() - length * std::sin(angle - spread));
                const QPointF wingB(
                    target->second.x() - length * std::cos(angle + spread),
                    target->second.y() - length * std::sin(angle + spread));
                painter.setBrush(color);
                painter.drawPolygon(QPolygonF{
                    QPointF(target->second),
                    wingA,
                    wingB,
                });
            }
        }

        painter.setPen(QColor(121, 130, 143));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(
            QRect(
                kOuterMargin,
                kOuterMargin,
                size.width() - kOuterMargin * 2 - 1,
                size.height() - kOuterMargin * 2 - 1));
        painter.restore();
    }

private:
    template<typename XAt>
    void drawLane(
        QPainter& painter,
        const Lane& lane,
        const QRect& rect,
        XAt xAt,
        const double scale)
    {
        const auto color = parsedColor(lane);
        if (lane.kind == LaneKind::Clock) {
            const auto* clock = findClock(project_, lane.clockDomainId);
            if (!clock || !clock->isValid()) return;
            const auto highY = rect.top() + 10;
            const auto lowY = rect.bottom() - 10;
            painter.setPen(QPen(color, 1.6));
            const auto drawOverrides = [&] {
                auto iterator = std::lower_bound(
                    lane.segments.begin(),
                    lane.segments.end(),
                    range_.start,
                    [](const Segment& segment, const Tick tick) {
                        return segment.end <= tick;
                    });
                for (; iterator != lane.segments.end() && iterator->start < range_.end; ++iterator) {
                    const auto mode = clockOverrideModeFromString(iterator->value);
                    if (!mode) continue;
                    const auto left = xAt(std::max(iterator->start, range_.start));
                    const auto right = xAt(std::min(iterator->end, range_.end));
                    if (right <= left) continue;
                    const auto gated = *mode == ClockOverrideMode::Gated;
                    painter.fillRect(
                        QRect(left, rect.top() + 1, std::max(1, right - left), rect.height() - 2),
                        gated ? QColor(255, 248, 225) : QColor(255, 235, 238));
                    painter.setPen(QPen(
                        gated ? QColor(239, 108, 0) : QColor(198, 40, 40),
                        1.7,
                        gated ? Qt::SolidLine : Qt::DashLine));
                    const auto y = gated ? lowY : (highY + lowY) / 2;
                    painter.drawLine(left, y, right, y);
                    const auto fullLabel = gated
                        ? QStringLiteral("GATED")
                        : QStringLiteral("DISABLED - X");
                    const auto compactLabel = gated
                        ? QStringLiteral("G")
                        : QStringLiteral("X");
                    const auto availableWidth = right - left - 6;
                    const auto label = painter.fontMetrics().horizontalAdvance(fullLabel)
                            <= availableWidth
                        ? fullLabel
                        : compactLabel;
                    if (painter.fontMetrics().horizontalAdvance(label) <= availableWidth) {
                        painter.drawText(
                            QRect(left + 3, rect.top(), right - left - 6, rect.height()),
                            Qt::AlignCenter,
                            label);
                    }
                    painter.setPen(QPen(
                        gated ? QColor(239, 108, 0) : QColor(198, 40, 40),
                        0.8,
                        Qt::DashLine));
                    painter.drawLine(left, rect.top() + 2, left, rect.bottom() - 2);
                    painter.drawLine(right, rect.top() + 2, right, rect.bottom() - 2);
                }
            };
            if (clock->period * scale < 1.5) {
                painter.drawLine(rect.left(), (highY + lowY) / 2, rect.right(), (highY + lowY) / 2);
                drawOverrides();
                return;
            }
            auto cycle = (range_.start - clock->phase) / clock->period - 1;
            QPainterPath path;
            bool started = false;
            for (std::size_t count = 0; count < 100'000; ++count, ++cycle) {
                const auto rising = tickAtCycle(*clock, cycle, ClockEdge::Rising);
                const auto falling = tickAtCycle(*clock, cycle, ClockEdge::Falling);
                const auto next = tickAtCycle(*clock, cycle + 1, ClockEdge::Rising);
                if (!rising || !falling || !next || *rising > range_.end) break;
                if (*next < range_.start) continue;
                const auto risingX = xAt(*rising);
                const auto fallingX = xAt(*falling);
                const auto nextX = xAt(*next);
                if (!started) {
                    path.moveTo(risingX, lowY);
                    started = true;
                }
                path.lineTo(risingX, highY);
                path.lineTo(fallingX, highY);
                path.lineTo(fallingX, lowY);
                path.lineTo(nextX, lowY);
            }
            painter.drawPath(path);
            drawOverrides();
            return;
        }

        auto iterator = std::lower_bound(
            lane.segments.begin(),
            lane.segments.end(),
            range_.start,
            [](const Segment& segment, const Tick tick) {
                return segment.end <= tick;
            });
        if (lane.kind == LaneKind::Bit) {
            const auto highY = rect.top() + 10;
            const auto lowY = rect.bottom() - 10;
            painter.setPen(QPen(color, 1.7));
            int previousY = lowY;
            bool previous = false;
            for (; iterator != lane.segments.end() && iterator->start < range_.end; ++iterator) {
                const auto left = xAt(std::max(iterator->start, range_.start));
                const auto right = xAt(std::min(iterator->end, range_.end));
                const auto value = iterator->value.empty() ? 'X' : iterator->value.front();
                if (value == 'X' || value == 'Z') {
                    auto fill = color;
                    fill.setAlpha(25);
                    painter.fillRect(QRect(left, highY, std::max(1, right - left), lowY - highY), fill);
                    painter.drawLine(left, (highY + lowY) / 2, right, (highY + lowY) / 2);
                    painter.drawText(
                        QRect(left, highY, right - left, lowY - highY),
                        Qt::AlignCenter,
                        QString(value));
                    previous = false;
                    continue;
                }
                const auto y = value == '1' ? highY : lowY;
                if (previous && previousY != y) painter.drawLine(left, previousY, left, y);
                painter.drawLine(left, y, right, y);
                previousY = y;
                previous = true;
            }
            return;
        }

        const auto top = rect.top() + 9;
        const auto bottom = rect.bottom() - 9;
        const auto middle = (top + bottom) / 2;
        painter.setPen(QPen(color, 1.4));
        for (; iterator != lane.segments.end() && iterator->start < range_.end; ++iterator) {
            const auto left = xAt(std::max(iterator->start, range_.start));
            const auto right = xAt(std::min(iterator->end, range_.end));
            const auto bevel = std::min(6, std::max(0, (right - left) / 3));
            QPainterPath path;
            path.moveTo(left, middle);
            path.lineTo(left + bevel, top);
            path.lineTo(right - bevel, top);
            path.lineTo(right, middle);
            path.lineTo(right - bevel, bottom);
            path.lineTo(left + bevel, bottom);
            path.closeSubpath();
            auto fill = color;
            fill.setAlpha(22);
            painter.fillPath(path, fill);
            painter.drawPath(path);
            if (right - left > 26) {
                painter.setPen(QColor(38, 45, 56));
                const auto label = lane.kind == LaneKind::Bus
                    ? busTextLabel(iterator->value).value_or(iterator->value)
                    : std::string_view(iterator->value);
                painter.drawText(
                    QRect(left + bevel + 3, top, right - left - 2 * bevel - 6, bottom - top),
                    Qt::AlignCenter,
                    QString::fromUtf8(label.data(), static_cast<qsizetype>(label.size())));
                painter.setPen(QPen(color, 1.4));
            }
        }
    }

    const Project& project_;
    const Scenario& scenario_;
    const ExportOptions& options_;
    NormalizedRange range_;
    std::vector<const Lane*> lanes_;
};

bool writeOne(const QString& path, const QByteArray& data, QString* error)
{
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = QStringLiteral("Cannot open %1: %2").arg(path, file.errorString());
        return false;
    }
    if (file.write(data) != data.size() || !file.commit()) {
        if (error) *error = QStringLiteral("Cannot atomically write %1: %2").arg(path, file.errorString());
        file.cancelWriting();
        return false;
    }
    return true;
}

std::string bitAt(const Lane& lane, const Tick tick)
{
    const auto iterator = std::find_if(
        lane.segments.begin(),
        lane.segments.end(),
        [tick](const Segment& segment) {
            return segment.start <= tick && tick < segment.end;
        });
    return iterator == lane.segments.end() ? "x" : iterator->value;
}

} // namespace

bool ArtifactBundleResult::ok() const noexcept
{
    return artifacts.has_value() && error.isEmpty()
        && std::none_of(
            diagnostics.begin(),
            diagnostics.end(),
            [](const GenerationDiagnostic& diagnostic) {
                return diagnostic.severity == Severity::Error;
            });
}

QByteArray renderWaveformSvg(
    const Project& project,
    const Scenario& scenario,
    const ExportOptions& options,
    QString* error)
{
    const auto range = normalizeRange(scenario, options, error);
    if (!range) return {};
    DocumentRenderer renderer(project, scenario, options, *range);
    const QSize size(options.width, std::max(240, renderer.preferredHeight()));
    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly)) {
        if (error) *error = QStringLiteral("Cannot create SVG buffer");
        return {};
    }
    QSvgGenerator generator;
    generator.setOutputDevice(&buffer);
    generator.setSize(size);
    generator.setViewBox(QRect(QPoint(0, 0), size));
    generator.setTitle(QString::fromStdString(scenario.name));
    generator.setDescription(QStringLiteral("Generated by Tickx"));
    QPainter painter(&generator);
    renderer.render(painter, size);
    if (!painter.end()) {
        if (error) *error = QStringLiteral("SVG painter failed");
        return {};
    }
    return bytes;
}

QByteArray renderWaveformPng(
    const Project& project,
    const Scenario& scenario,
    const ExportOptions& options,
    QString* error)
{
    const auto range = normalizeRange(scenario, options, error);
    if (!range) return {};
    DocumentRenderer renderer(project, scenario, options, *range);
    const QSize logicalSize(options.width, std::max(240, renderer.preferredHeight()));
    const auto factor = static_cast<double>(options.pngDpi) / 96.0;
    const QSize pixelSize(
        static_cast<int>(std::ceil(logicalSize.width() * factor)),
        static_cast<int>(std::ceil(logicalSize.height() * factor)));
    QImage image(pixelSize, QImage::Format_ARGB32_Premultiplied);
    image.setDotsPerMeterX(static_cast<int>(options.pngDpi / 0.0254));
    image.setDotsPerMeterY(static_cast<int>(options.pngDpi / 0.0254));
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.scale(factor, factor);
    renderer.render(painter, logicalSize);
    painter.end();

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG")) {
        if (error) *error = QStringLiteral("PNG encoder failed");
        return {};
    }
    return bytes;
}

QByteArray renderWaveformPdf(
    const Project& project,
    const Scenario& scenario,
    const ExportOptions& options,
    QString* error)
{
    const auto fullRange = normalizeRange(scenario, options, error);
    if (!fullRange) return {};
    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly)) {
        if (error) *error = QStringLiteral("Cannot create PDF buffer");
        return {};
    }
    QPdfWriter writer(&buffer);
    writer.setTitle(QString::fromStdString(scenario.name));
    writer.setCreator(QStringLiteral("Tickx"));
    writer.setResolution(144);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageOrientation(QPageLayout::Landscape);
    writer.setPageMargins(QMarginsF(8, 8, 8, 8), QPageLayout::Millimeter);

    QPainter painter(&writer);
    const auto pageRect = writer.pageLayout().paintRectPixels(writer.resolution());
    const auto pageSpan = options.pdfPageSpanTicks > 0
        ? options.pdfPageSpanTicks
        : fullRange->end - fullRange->start;
    auto pageStart = fullRange->start;
    bool first = true;
    while (pageStart < fullRange->end) {
        if (!first && !writer.newPage()) {
            if (error) *error = QStringLiteral("Cannot add a PDF page");
            painter.end();
            return {};
        }
        first = false;
        const auto remaining = fullRange->end - pageStart;
        const auto pageEnd = pageStart + std::min(pageSpan, remaining);
        DocumentRenderer renderer(
            project,
            scenario,
            options,
            NormalizedRange{pageStart, pageEnd});
        renderer.render(painter, pageRect.size());
        pageStart = pageEnd;
    }
    if (!painter.end()) {
        if (error) *error = QStringLiteral("PDF painter failed");
        return {};
    }
    return bytes;
}

QByteArray generateWaveDromJson(
    const Project& project,
    const Scenario& scenario,
    const ExportOptions& options,
    QString* error)
{
    const auto range = normalizeRange(scenario, options, error);
    if (!range) return {};
    Tick grid = range->end - range->start;
    auto includeDelta = [&grid](const Tick delta) {
        if (delta > 0) grid = std::gcd(grid, delta);
    };
    for (const auto& lane : scenario.lanes) {
        for (const auto& segment : lane.segments) {
            if (segment.end <= range->start || segment.start >= range->end) continue;
            includeDelta(std::max(segment.start, range->start) - range->start);
            includeDelta(std::min(segment.end, range->end) - range->start);
        }
    }
    for (const auto& clock : project.clockDomains) includeDelta(clock.period);
    grid = std::max<Tick>(1, grid);
    const auto duration = range->end - range->start;
    auto columns = (duration - 1) / grid + 1;
    if (columns > 4096) {
        const auto multiplier = (columns + 4095) / 4096;
        grid *= multiplier;
        columns = (duration - 1) / grid + 1;
    }

    QJsonArray signalArray;
    for (const auto& lane : scenario.lanes) {
        if (!lane.visible || lane.kind == LaneKind::Group) continue;
        std::string waveString;
        std::vector<std::string> dataValues;
        std::string previous;
        waveString.reserve(static_cast<std::size_t>(columns));
        for (Tick column = 0; column < columns; ++column) {
            const auto tick = std::min(
                range->end - 1,
                range->start + column * grid);
            std::string value;
            if (lane.kind == LaneKind::Clock) {
                const auto* clock = findClock(project, lane.clockDomainId);
                if (!clock || !clock->isValid()) {
                    value = "x";
                } else {
                    value.assign(1, clockValueAt(*clock, lane, tick));
                }
            } else {
                value = bitAt(lane, tick);
            }
            if (value == previous) {
                waveString.push_back('.');
                continue;
            }
            previous = value;
            if (lane.kind == LaneKind::Bit || lane.kind == LaneKind::Clock) {
                const auto lowered = value.empty()
                    ? 'x'
                    : static_cast<char>(std::tolower(static_cast<unsigned char>(value.front())));
                waveString.push_back(
                    lowered == '0' || lowered == '1' || lowered == 'x' || lowered == 'z'
                        ? lowered
                        : 'x');
            } else {
                waveString.push_back('=');
                dataValues.emplace_back(lane.kind == LaneKind::Bus
                    ? busTextLabel(value).value_or(value) : std::string_view(value));
            }
        }
        QJsonObject signal{
            {QStringLiteral("name"), QString::fromStdString(lane.name)},
            {QStringLiteral("wave"), QString::fromStdString(waveString)},
        };
        if (!dataValues.empty()) {
            QJsonArray data;
            for (const auto& value : dataValues) data.append(QString::fromStdString(value));
            signal.insert(QStringLiteral("data"), data);
        }
        signalArray.append(signal);
    }
    QJsonObject root{
        {QStringLiteral("signal"), signalArray},
        {QStringLiteral("head"),
         QJsonObject{{QStringLiteral("text"), QString::fromStdString(scenario.name)}}},
        {QStringLiteral("waveWorkbench"),
         QJsonObject{
             {QStringLiteral("startTick"), QString::number(range->start)},
             {QStringLiteral("endTick"), QString::number(range->end)},
             {QStringLiteral("gridTick"), QString::number(grid)},
             {QStringLiteral("picosecondsPerTick"), QString::number(project.timeBase.picosecondsPerTick)},
         }},
    };
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

ArtifactBundleResult generateArtifactBundle(
    const Project& project,
    const Scenario& scenario,
    const ExportOptions& options)
{
    ArtifactBundleResult result;
    auto planResult = buildGenerationPlan(project, scenario);
    result.diagnostics = planResult.diagnostics;
    if (!planResult.plan) {
        result.error = QStringLiteral("Cannot build generation plan");
        return result;
    }
    auto systemVerilog = generateSystemVerilog(*planResult.plan);
    auto assertions = generateSystemVerilogAssertions(*planResult.plan);
    auto cocotb = generateCocotb(*planResult.plan);
    result.diagnostics.insert(
        result.diagnostics.end(),
        systemVerilog.diagnostics.begin(),
        systemVerilog.diagnostics.end());
    result.diagnostics.insert(
        result.diagnostics.end(),
        assertions.diagnostics.begin(),
        assertions.diagnostics.end());
    result.diagnostics.insert(
        result.diagnostics.end(),
        cocotb.diagnostics.begin(),
        cocotb.diagnostics.end());

    ArtifactBundle artifacts;
    artifacts.systemVerilog = QByteArray::fromStdString(systemVerilog.text);
    artifacts.assertions = QByteArray::fromStdString(assertions.text);
    artifacts.cocotb = QByteArray::fromStdString(cocotb.text);
    artifacts.svg = renderWaveformSvg(project, scenario, options, &result.error);
    if (!result.error.isEmpty()) return result;
    artifacts.png = renderWaveformPng(project, scenario, options, &result.error);
    if (!result.error.isEmpty()) return result;
    artifacts.pdf = renderWaveformPdf(project, scenario, options, &result.error);
    if (!result.error.isEmpty()) return result;
    artifacts.waveDromJson = generateWaveDromJson(project, scenario, options, &result.error);
    if (!result.error.isEmpty()) return result;
    result.artifacts = std::move(artifacts);
    return result;
}

bool writeArtifactBundleAtomic(
    const ArtifactBundle& artifacts,
    const QString& directory,
    const QString& baseName,
    QString* error)
{
    if (!QDir().mkpath(directory)) {
        if (error) *error = QStringLiteral("Cannot create output directory: %1").arg(directory);
        return false;
    }
    const QDir output(directory);
    const std::array<std::pair<QString, const QByteArray*>, 7> files{{
        {baseName + QStringLiteral("_tb.sv"), &artifacts.systemVerilog},
        {baseName + QStringLiteral("_assertions.sv"), &artifacts.assertions},
        {QStringLiteral("test_") + baseName + QStringLiteral(".py"), &artifacts.cocotb},
        {baseName + QStringLiteral(".svg"), &artifacts.svg},
        {baseName + QStringLiteral(".png"), &artifacts.png},
        {baseName + QStringLiteral(".pdf"), &artifacts.pdf},
        {baseName + QStringLiteral(".wavedrom.json"), &artifacts.waveDromJson},
    }};
    for (const auto& [name, data] : files) {
        if (!writeOne(output.filePath(name), *data, error)) return false;
    }
    return true;
}

} // namespace wave
