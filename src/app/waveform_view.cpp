#include "waveform_view.h"

#include "waveform_theme.h"
#include "ui_controls.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPolygonF>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSet>
#include <QStyle>
#include <QStyleOptionFocusRect>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

namespace wave {
namespace {

constexpr int kScrollResolution = 1'000'000;
constexpr qsizetype kMaximumPayloadBytes = 8 * 1024 * 1024;
constexpr int kMaximumLanes = 512;
constexpr int kMaximumSegments = 200'000;
constexpr int kMaximumStringLength = 512;
constexpr qint64 kMaximumSafeJsonInteger = 9'007'199'254'740'991LL;

struct PreviewSource {
    QString file;
    int line{0};
    int column{0};
    QString semanticId;

    [[nodiscard]] bool available() const noexcept
    {
        return !file.isEmpty() && line > 0;
    }
};

struct PreviewSegment {
    qint64 start{0};
    qint64 end{0};
    QString value;
    bool unknown{false};
};

struct PreviewLane {
    QString id;
    QString name;
    QString kind;
    QString provenance;
    int width{1};
    PreviewSource source;
    std::vector<PreviewSegment> segments;
};

struct PreviewPayload {
    qulonglong generation{0};
    QString mode;
    QString unit;
    qint64 start{0};
    qint64 end{0};
    std::vector<PreviewLane> lanes;
};

struct ParseResult {
    std::optional<PreviewPayload> payload;
    QString error;
};

bool jsonInteger(const QJsonValue& value,
                 const qint64 minimum,
                 const qint64 maximum,
                 const QString& field,
                 qint64* result,
                 QString* error)
{
    if (!value.isDouble()) {
        if (error) *error = QStringLiteral("%1 must be an integer").arg(field);
        return false;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number)
        || std::trunc(number) != number
        || number < static_cast<double>(minimum)
        || number > static_cast<double>(maximum)) {
        if (error) *error = QStringLiteral("%1 is outside the supported integer range").arg(field);
        return false;
    }
    if (result) *result = static_cast<qint64>(number);
    return true;
}

bool requiredString(const QJsonObject& object,
                    const QString& key,
                    const QString& field,
                    QString* result,
                    QString* error,
                    const int maximumLength = kMaximumStringLength)
{
    const QJsonValue value = object.value(key);
    if (!value.isString()) {
        if (error) *error = QStringLiteral("%1 must be a string").arg(field);
        return false;
    }
    const QString text = value.toString().trimmed();
    if (text.isEmpty() || text.size() > maximumLength) {
        if (error) *error = QStringLiteral("%1 is empty or too long").arg(field);
        return false;
    }
    if (result) *result = text;
    return true;
}

bool onlyKeys(const QJsonObject& object,
              const QSet<QString>& allowed,
              const QString& field,
              QString* error)
{
    for (auto iterator = object.constBegin(); iterator != object.constEnd(); ++iterator) {
        if (!allowed.contains(iterator.key())) {
            if (error) {
                *error = QStringLiteral("%1 contains unsupported field %2")
                             .arg(field, iterator.key());
            }
            return false;
        }
    }
    return true;
}

bool containsUnknownValue(const QString& value)
{
    return value.contains(QLatin1Char('x'), Qt::CaseInsensitive)
        || value.contains(QLatin1Char('z'), Qt::CaseInsensitive)
        || value == QLatin1String("?");
}

ParseResult parsePreviewPayload(const QByteArray& bytes)
{
    ParseResult result;
    if (bytes.isEmpty()) {
        result.error = QStringLiteral("wave-preview/v1 payload is empty");
        return result;
    }
    if (bytes.size() > kMaximumPayloadBytes) {
        result.error = QStringLiteral("wave-preview/v1 payload exceeds the 8 MiB limit");
        return result;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        result.error = QStringLiteral("wave-preview/v1 JSON is invalid at byte %1: %2")
                           .arg(parseError.offset)
                           .arg(parseError.errorString());
        return result;
    }
    const QJsonObject root = document.object();
    if (!onlyKeys(root,
                  {QStringLiteral("contract"), QStringLiteral("generation"),
                   QStringLiteral("mode"), QStringLiteral("timebase"),
                   QStringLiteral("lanes")},
                  QStringLiteral("payload"), &result.error)) {
        return result;
    }
    if (root.value(QStringLiteral("contract")).toString()
        != QLatin1String("wave-preview/v1")) {
        result.error = QStringLiteral("Unsupported waveform payload contract; expected wave-preview/v1");
        return result;
    }

    qint64 generation = 0;
    if (!jsonInteger(root.value(QStringLiteral("generation")),
                     0,
                     kMaximumSafeJsonInteger,
                     QStringLiteral("generation"),
                     &generation,
                     &result.error)) {
        return result;
    }

    PreviewPayload payload;
    payload.generation = static_cast<qulonglong>(generation);
    if (!requiredString(root,
                        QStringLiteral("mode"),
                        QStringLiteral("mode"),
                        &payload.mode,
                        &result.error,
                        32)) {
        return result;
    }
    payload.mode = payload.mode.toLower();
    if (payload.mode != QLatin1String("symbolic")
        && payload.mode != QLatin1String("simulated")) {
        result.error = QStringLiteral("mode must be symbolic or simulated");
        return result;
    }

    const QJsonValue timebaseValue = root.value(QStringLiteral("timebase"));
    if (!timebaseValue.isObject()) {
        result.error = QStringLiteral("timebase must be an object");
        return result;
    }
    const QJsonObject timebase = timebaseValue.toObject();
    if (!onlyKeys(timebase,
                  {QStringLiteral("unit"), QStringLiteral("start"),
                   QStringLiteral("end")},
                  QStringLiteral("timebase"), &result.error)) {
        return result;
    }
    if (!requiredString(timebase,
                        QStringLiteral("unit"),
                        QStringLiteral("timebase.unit"),
                        &payload.unit,
                        &result.error,
                        16)) {
        return result;
    }
    payload.unit = payload.unit.toLower();
    static const QSet<QString> units{
        QStringLiteral("tick"), QStringLiteral("fs"), QStringLiteral("ps"),
        QStringLiteral("ns"), QStringLiteral("us"), QStringLiteral("ms"),
        QStringLiteral("s")};
    if (!units.contains(payload.unit)) {
        result.error = QStringLiteral("timebase.unit is unsupported");
        return result;
    }
    if (!jsonInteger(timebase.value(QStringLiteral("start")),
                     -kMaximumSafeJsonInteger,
                     kMaximumSafeJsonInteger,
                     QStringLiteral("timebase.start"),
                     &payload.start,
                     &result.error)
        || !jsonInteger(timebase.value(QStringLiteral("end")),
                        -kMaximumSafeJsonInteger,
                        kMaximumSafeJsonInteger,
                        QStringLiteral("timebase.end"),
                        &payload.end,
                        &result.error)) {
        return result;
    }
    if (payload.end <= payload.start) {
        result.error = QStringLiteral("timebase.end must be greater than timebase.start");
        return result;
    }

    const QJsonValue lanesValue = root.value(QStringLiteral("lanes"));
    if (!lanesValue.isArray()) {
        result.error = QStringLiteral("lanes must be an array");
        return result;
    }
    const QJsonArray lanes = lanesValue.toArray();
    if (lanes.size() > kMaximumLanes) {
        result.error = QStringLiteral("wave-preview/v1 supports at most %1 lanes")
                           .arg(kMaximumLanes);
        return result;
    }
    QSet<QString> laneIds;
    int segmentCount = 0;
    payload.lanes.reserve(static_cast<std::size_t>(lanes.size()));
    for (int laneIndex = 0; laneIndex < lanes.size(); ++laneIndex) {
        if (!lanes.at(laneIndex).isObject()) {
            result.error = QStringLiteral("lanes[%1] must be an object").arg(laneIndex);
            return result;
        }
        const QJsonObject object = lanes.at(laneIndex).toObject();
        PreviewLane lane;
        const QString prefix = QStringLiteral("lanes[%1]").arg(laneIndex);
        if (!onlyKeys(object,
                      {QStringLiteral("id"), QStringLiteral("name"),
                       QStringLiteral("kind"), QStringLiteral("width"),
                       QStringLiteral("provenance"), QStringLiteral("source"),
                       QStringLiteral("segments")},
                      prefix, &result.error)) {
            return result;
        }
        if (!requiredString(object, QStringLiteral("id"), prefix + QStringLiteral(".id"),
                            &lane.id, &result.error)
            || !requiredString(object, QStringLiteral("name"), prefix + QStringLiteral(".name"),
                               &lane.name, &result.error)
            || !requiredString(object, QStringLiteral("kind"), prefix + QStringLiteral(".kind"),
                               &lane.kind, &result.error, 32)
            || !requiredString(object, QStringLiteral("provenance"),
                               prefix + QStringLiteral(".provenance"),
                               &lane.provenance, &result.error, 96)) {
            return result;
        }
        if (laneIds.contains(lane.id)) {
            result.error = QStringLiteral("Duplicate stable lane id: %1").arg(lane.id);
            return result;
        }
        laneIds.insert(lane.id);
        lane.kind = lane.kind.toLower();
        if (lane.kind != QLatin1String("bit")
            && lane.kind != QLatin1String("bus")
            && lane.kind != QLatin1String("clock")) {
            result.error = prefix + QStringLiteral(".kind must be bit, bus, or clock");
            return result;
        }
        qint64 width = 0;
        if (!jsonInteger(object.value(QStringLiteral("width")),
                         1, 65'536, prefix + QStringLiteral(".width"),
                         &width, &result.error)) {
            return result;
        }
        lane.width = static_cast<int>(width);
        if ((lane.kind == QLatin1String("bit")
             || lane.kind == QLatin1String("clock"))
            && lane.width != 1) {
            result.error = prefix + QStringLiteral(".width must be 1 for bit and clock lanes");
            return result;
        }

        const QJsonValue sourceValue = object.value(QStringLiteral("source"));
        if (!sourceValue.isUndefined() && !sourceValue.isNull()) {
            if (!sourceValue.isObject()) {
                result.error = prefix + QStringLiteral(".source must be an object");
                return result;
            }
            const QJsonObject source = sourceValue.toObject();
            if (!onlyKeys(source,
                          {QStringLiteral("file"), QStringLiteral("line"),
                           QStringLiteral("column"), QStringLiteral("semanticId")},
                          prefix + QStringLiteral(".source"), &result.error)) {
                return result;
            }
            if (!requiredString(source, QStringLiteral("file"),
                                prefix + QStringLiteral(".source.file"),
                                &lane.source.file, &result.error)) {
                return result;
            }
            lane.source.file = QDir::cleanPath(
                QDir::fromNativeSeparators(lane.source.file));
            if (QDir::isAbsolutePath(lane.source.file)
                || lane.source.file == QLatin1String("..")
                || lane.source.file.startsWith(QStringLiteral("../"))) {
                result.error = prefix + QStringLiteral(".source.file must be workspace-relative");
                return result;
            }
            qint64 line = 0;
            qint64 column = 1;
            if (!jsonInteger(source.value(QStringLiteral("line")),
                             1, 10'000'000,
                             prefix + QStringLiteral(".source.line"),
                             &line, &result.error)) {
                return result;
            }
            if (!source.value(QStringLiteral("column")).isUndefined()
                && !jsonInteger(source.value(QStringLiteral("column")),
                                1, 1'000'000,
                                prefix + QStringLiteral(".source.column"),
                                &column, &result.error)) {
                return result;
            }
            lane.source.line = static_cast<int>(line);
            lane.source.column = static_cast<int>(column);
            lane.source.semanticId =
                source.value(QStringLiteral("semanticId")).toString().trimmed();
            if (lane.source.semanticId.size() > kMaximumStringLength) {
                result.error = prefix + QStringLiteral(".source.semanticId is too long");
                return result;
            }
        }

        const QJsonValue segmentsValue = object.value(QStringLiteral("segments"));
        if (!segmentsValue.isArray()) {
            result.error = prefix + QStringLiteral(".segments must be an array");
            return result;
        }
        const QJsonArray segments = segmentsValue.toArray();
        segmentCount += static_cast<int>(segments.size());
        if (segmentCount > kMaximumSegments) {
            result.error = QStringLiteral("wave-preview/v1 supports at most %1 total segments")
                               .arg(kMaximumSegments);
            return result;
        }
        lane.segments.reserve(static_cast<std::size_t>(segments.size()));
        qint64 previousEnd = payload.start;
        for (int segmentIndex = 0; segmentIndex < segments.size(); ++segmentIndex) {
            if (!segments.at(segmentIndex).isObject()) {
                result.error = QStringLiteral("%1.segments[%2] must be an object")
                                   .arg(prefix).arg(segmentIndex);
                return result;
            }
            const QJsonObject segmentObject = segments.at(segmentIndex).toObject();
            const QString segmentPrefix = QStringLiteral("%1.segments[%2]")
                                              .arg(prefix).arg(segmentIndex);
            if (!onlyKeys(segmentObject,
                          {QStringLiteral("start"), QStringLiteral("end"),
                           QStringLiteral("value"), QStringLiteral("unknown")},
                          segmentPrefix, &result.error)) {
                return result;
            }
            PreviewSegment segment;
            if (!jsonInteger(segmentObject.value(QStringLiteral("start")),
                             payload.start, payload.end,
                             segmentPrefix + QStringLiteral(".start"),
                             &segment.start, &result.error)
                || !jsonInteger(segmentObject.value(QStringLiteral("end")),
                                payload.start, payload.end,
                                segmentPrefix + QStringLiteral(".end"),
                                &segment.end, &result.error)
                || !requiredString(segmentObject, QStringLiteral("value"),
                                   segmentPrefix + QStringLiteral(".value"),
                                   &segment.value, &result.error, 128)) {
                return result;
            }
            if (segment.end <= segment.start) {
                result.error = segmentPrefix + QStringLiteral(".end must be greater than start");
                return result;
            }
            if (segment.start < previousEnd) {
                result.error = segmentPrefix + QStringLiteral(" overlaps or is out of order");
                return result;
            }
            const QJsonValue unknown = segmentObject.value(QStringLiteral("unknown"));
            if (!unknown.isUndefined() && !unknown.isBool()) {
                result.error = segmentPrefix + QStringLiteral(".unknown must be a boolean");
                return result;
            }
            segment.unknown = unknown.toBool(false) || containsUnknownValue(segment.value);
            previousEnd = segment.end;
            lane.segments.push_back(std::move(segment));
        }
        payload.lanes.push_back(std::move(lane));
    }

    result.payload = std::move(payload);
    return result;
}

qint64 clampedRound(const long double value)
{
    if (value <= static_cast<long double>(std::numeric_limits<qint64>::min())) {
        return std::numeric_limits<qint64>::min();
    }
    if (value >= static_cast<long double>(std::numeric_limits<qint64>::max())) {
        return std::numeric_limits<qint64>::max();
    }
    return static_cast<qint64>(std::llround(value));
}

qint64 niceGridStep(const double pixelsPerTick)
{
    if (!(pixelsPerTick > 0.0)) return 1;
    const long double raw = 105.0L / static_cast<long double>(pixelsPerTick);
    const long double exponent = std::floor(std::log10(std::max(1.0L, raw)));
    const long double scale = std::pow(10.0L, exponent);
    const long double normalized = raw / scale;
    const long double multiplier = normalized <= 1.0L ? 1.0L
        : normalized <= 2.0L ? 2.0L
        : normalized <= 5.0L ? 5.0L : 10.0L;
    return std::max<qint64>(1, clampedRound(multiplier * scale));
}

qint64 floorToStep(const qint64 value, const qint64 step)
{
    qint64 quotient = value / step;
    if (value < 0 && value % step != 0) --quotient;
    return quotient * step;
}

QString displayTick(const qint64 tick, const QString& unit)
{
    return QStringLiteral("%1 %2").arg(tick).arg(unit);
}

bool highValue(const QString& value)
{
    return value == QLatin1String("1")
        || value.compare(QStringLiteral("H"), Qt::CaseInsensitive) == 0;
}

bool lowValue(const QString& value)
{
    return value == QLatin1String("0")
        || value.compare(QStringLiteral("L"), Qt::CaseInsensitive) == 0;
}

const PreviewSegment* segmentAt(const PreviewLane& lane, const qint64 tick)
{
    const auto iterator = std::upper_bound(
        lane.segments.begin(), lane.segments.end(), tick,
        [](const qint64 value, const PreviewSegment& segment) {
            return value < segment.start;
        });
    if (iterator == lane.segments.begin()) return nullptr;
    const PreviewSegment& segment = *std::prev(iterator);
    return tick >= segment.start && tick < segment.end ? &segment : nullptr;
}

} // namespace

class WaveformView::Data {
public:
    std::optional<PreviewPayload> preview;
    bool hasGeneration{false};
    qulonglong lastGeneration{0};
    QString state{QStringLiteral("empty")};
    QString status{QStringLiteral("No waveform preview")};
    QString error;
    QString requestedTheme{QStringLiteral("system")};
    bool compact{false};
    int nameWidth{220};
    int rulerHeight{40};
    int rowHeight{48};
    double pixelsPerTick{1.0};
    bool fitMode{true};
    QString selectedLane;
    qint64 cursor{0};
};

WaveformView::WaveformView(QWidget* parent)
    : QAbstractScrollArea(parent)
    , data_(std::make_unique<Data>())
{
    setObjectName(QStringLiteral("WaveformView"));
    ui::installScrollBars(this);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setFrameShape(QFrame::NoFrame);
    setAccessibleName(tr("Waveform preview"));
    viewport()->setMouseTracking(true);
    horizontalScrollBar()->setRange(0, 0);
    verticalScrollBar()->setRange(0, 0);
    connect(horizontalScrollBar(), &QScrollBar::valueChanged,
            viewport(), qOverload<>(&QWidget::update));
    connect(verticalScrollBar(), &QScrollBar::valueChanged,
            viewport(), qOverload<>(&QWidget::update));
    connect(this, &WaveformView::themeChanged, this, [this] {
        horizontalScrollBar()->update();
        verticalScrollBar()->update();
    });
    setProperty("wavewidgets.contract", contract());
    setProperty("wavewidgets.capabilities", capabilities());
    setProperty("wavewidgets.previewContract", QStringLiteral("wave-preview/v1"));
    updateAccessibleSummary();
}

WaveformView::~WaveformView() = default;

QString WaveformView::contract() const
{
    return QStringLiteral("wave-workbench.waveform-view/v1");
}

QStringList WaveformView::capabilities() const
{
    return {QStringLiteral("wave-preview/v1"),
            QStringLiteral("generation-replace/v1"),
            QStringLiteral("waveform-theme/v1"),
            QStringLiteral("compact-density/v1"),
            QStringLiteral("source-navigation/v1")};
}

qulonglong WaveformView::previewGeneration() const noexcept
{
    return data_->hasGeneration ? data_->lastGeneration : 0;
}

QString WaveformView::previewMode() const
{
    return data_->preview ? data_->preview->mode : QString{};
}

QString WaveformView::presentationState() const { return data_->state; }
QString WaveformView::statusMessage() const { return data_->status; }
QString WaveformView::lastError() const { return data_->error; }
QString WaveformView::themeName() const { return data_->requestedTheme; }
bool WaveformView::compact() const noexcept { return data_->compact; }

bool WaveformView::replacePreviewPayload(const QByteArray& payload)
{
    ParseResult parsed = parsePreviewPayload(payload);
    if (!parsed.payload) {
        setError(parsed.error);
        setPresentationState(QStringLiteral("failed"), parsed.error);
        return false;
    }
    if (data_->hasGeneration
        && parsed.payload->generation <= data_->lastGeneration) {
        const QString error = QStringLiteral(
            "Stale wave-preview/v1 generation %1 rejected; current generation is %2")
                                  .arg(parsed.payload->generation)
                                  .arg(data_->lastGeneration);
        setError(error);
        setPresentationState(QStringLiteral("stale"), error);
        return false;
    }

    const bool compatibleDomain = data_->preview
        && data_->preview->unit == parsed.payload->unit
        && data_->preview->start == parsed.payload->start
        && data_->preview->end == parsed.payload->end;
    const QString previousLane = data_->selectedLane;
    const qint64 previousCursor = data_->cursor;
    data_->preview = std::move(parsed.payload.value());
    data_->hasGeneration = true;
    data_->lastGeneration = data_->preview->generation;
    data_->selectedLane.clear();
    if (!previousLane.isEmpty()) {
        const auto lane = std::find_if(
            data_->preview->lanes.begin(), data_->preview->lanes.end(),
            [&previousLane](const PreviewLane& candidate) {
                return candidate.id == previousLane;
            });
        if (lane != data_->preview->lanes.end()) data_->selectedLane = previousLane;
    }
    data_->cursor = compatibleDomain
        ? std::clamp(previousCursor, data_->preview->start, data_->preview->end)
        : data_->preview->start;
    if (!compatibleDomain) data_->fitMode = true;
    setError({});
    setPresentationState(
        QStringLiteral("ready"),
        QStringLiteral("%1 · %2 lanes · generation %3")
            .arg(data_->preview->mode == QLatin1String("symbolic")
                     ? QStringLiteral("Symbolic Preview")
                     : QStringLiteral("Simulated Result"))
            .arg(data_->preview->lanes.size())
            .arg(data_->lastGeneration));
    updateScrollBars();
    if (data_->fitMode && viewport()->width() > data_->nameWidth + 30) fitAll();
    updateAccessibleSummary();
    viewport()->update();
    emit previewChanged(data_->lastGeneration);
    return true;
}

bool WaveformView::replacePreviewJson(const QString& payload)
{
    return replacePreviewPayload(payload.toUtf8());
}

bool WaveformView::setPresentationState(const QString& state,
                                        const QString& message)
{
    const QString normalized = state.trimmed().toLower();
    static const QSet<QString> states{
        QStringLiteral("empty"), QStringLiteral("loading"),
        QStringLiteral("ready"), QStringLiteral("stale"),
        QStringLiteral("failed")};
    if (!states.contains(normalized)) {
        setError(QStringLiteral("Unsupported waveform presentation state: %1").arg(state));
        return false;
    }
    const QString normalizedMessage = message.trimmed().isEmpty()
        ? normalized
        : message.trimmed();
    if (data_->state == normalized && data_->status == normalizedMessage) return true;
    data_->state = normalized;
    data_->status = normalizedMessage;
    updateAccessibleSummary();
    viewport()->update();
    emit presentationStateChanged(data_->state, data_->status);
    return true;
}

bool WaveformView::setThemeName(const QString& theme)
{
    const QString normalized = theme.trimmed().toLower();
    if (normalized != QLatin1String("system")
        && normalized != QLatin1String("light")
        && normalized != QLatin1String("dark")) {
        setError(QStringLiteral("Waveform theme must be system, light, or dark"));
        return false;
    }
    if (data_->requestedTheme == normalized) return true;
    data_->requestedTheme = normalized;
    viewport()->update();
    emit themeChanged(normalized);
    return true;
}

void WaveformView::setCompact(const bool compact)
{
    if (data_->compact == compact) return;
    data_->compact = compact;
    updateMetrics();
    updateScrollBars();
    fitAll();
    emit compactChanged(compact);
}

void WaveformView::clearPreview()
{
    data_->preview.reset();
    data_->hasGeneration = false;
    data_->lastGeneration = 0;
    data_->selectedLane.clear();
    data_->cursor = 0;
    data_->fitMode = true;
    setError({});
    setPresentationState(QStringLiteral("empty"), QStringLiteral("No waveform preview"));
    updateScrollBars();
    emit previewChanged(0);
}

void WaveformView::fitAll()
{
    if (!data_->preview) return;
    const int width = std::max(1, viewport()->width() - data_->nameWidth - 12);
    const long double duration = static_cast<long double>(data_->preview->end)
        - static_cast<long double>(data_->preview->start);
    data_->pixelsPerTick = std::clamp(
        static_cast<double>(static_cast<long double>(width) / duration),
        1.0e-9,
        1.0e9);
    data_->fitMode = true;
    updateScrollBars();
    horizontalScrollBar()->setValue(0);
    viewport()->update();
}

void WaveformView::zoomIn()
{
    setZoom(data_->pixelsPerTick * 1.35,
            (viewport()->width() + data_->nameWidth) / 2.0);
}

void WaveformView::zoomOut()
{
    setZoom(data_->pixelsPerTick / 1.35,
            (viewport()->width() + data_->nameWidth) / 2.0);
}

bool WaveformView::selectLane(const QString& stableId)
{
    if (!data_->preview) return false;
    const auto lane = std::find_if(
        data_->preview->lanes.begin(), data_->preview->lanes.end(),
        [&stableId](const PreviewLane& candidate) {
            return candidate.id == stableId;
        });
    if (lane == data_->preview->lanes.end()) return false;
    data_->selectedLane = stableId;
    const int index = static_cast<int>(std::distance(data_->preview->lanes.begin(), lane));
    const int visibleHeight = std::max(0, viewport()->height() - data_->rulerHeight);
    verticalScrollBar()->setValue(std::max(
        0, index * data_->rowHeight
               - std::max(0, visibleHeight - data_->rowHeight) / 2));
    viewport()->update();
    emit selectionChanged(data_->selectedLane, data_->cursor);
    return true;
}

bool WaveformView::revealTick(const qint64 tick)
{
    if (!data_->preview || tick < data_->preview->start || tick > data_->preview->end) {
        return false;
    }
    data_->cursor = tick;
    if (horizontalScrollBar()->maximum() > 0) {
        const long double drawable = std::max(1, viewport()->width() - data_->nameWidth);
        const long double visibleSpan = drawable / data_->pixelsPerTick;
        const long double duration = static_cast<long double>(data_->preview->end)
            - data_->preview->start;
        const long double scrollable = std::max(0.0L, duration - visibleSpan);
        const long double desired = std::clamp(
            static_cast<long double>(tick) - data_->preview->start - visibleSpan / 2.0L,
            0.0L, scrollable);
        horizontalScrollBar()->setValue(scrollable > 0.0L
            ? static_cast<int>(std::llround(desired / scrollable * kScrollResolution))
            : 0);
    }
    viewport()->update();
    emit selectionChanged(data_->selectedLane, data_->cursor);
    return true;
}

void WaveformView::setError(const QString& error)
{
    const QString normalized = error.trimmed();
    if (data_->error == normalized) return;
    data_->error = normalized;
    setProperty("wavewidgets.lastError", data_->error);
    emit lastErrorChanged(data_->error);
}

void WaveformView::updateMetrics()
{
    data_->nameWidth = data_->compact ? 140 : 220;
    data_->rulerHeight = data_->compact ? 30 : 40;
    data_->rowHeight = data_->compact ? 32 : 48;
}

qint64 WaveformView::visibleStart() const noexcept
{
    if (!data_->preview) return 0;
    const long double drawable = std::max(1, viewport()->width() - data_->nameWidth);
    const long double visibleSpan = drawable / data_->pixelsPerTick;
    const long double duration = static_cast<long double>(data_->preview->end)
        - data_->preview->start;
    const long double scrollable = std::max(0.0L, duration - visibleSpan);
    if (scrollable <= 0.0L || horizontalScrollBar()->maximum() <= 0) {
        return data_->preview->start;
    }
    const long double ratio = static_cast<long double>(horizontalScrollBar()->value())
        / horizontalScrollBar()->maximum();
    return clampedRound(static_cast<long double>(data_->preview->start)
                        + ratio * scrollable);
}

qint64 WaveformView::visibleEnd() const noexcept
{
    if (!data_->preview) return 0;
    const long double drawable = std::max(1, viewport()->width() - data_->nameWidth);
    const long double span = drawable / data_->pixelsPerTick;
    return std::min(data_->preview->end,
                    clampedRound(static_cast<long double>(visibleStart()) + span));
}

double WaveformView::tickToX(const qint64 tick) const noexcept
{
    if (!data_->preview) return data_->nameWidth;
    return data_->nameWidth
        + static_cast<double>(static_cast<long double>(tick - visibleStart())
                              * data_->pixelsPerTick);
}

qint64 WaveformView::xToTick(const double x) const noexcept
{
    if (!data_->preview) return 0;
    return clampedRound(static_cast<long double>(visibleStart())
                        + static_cast<long double>(x - data_->nameWidth)
                              / data_->pixelsPerTick);
}

void WaveformView::updateScrollBars()
{
    const int laneCount = data_->preview
        ? static_cast<int>(data_->preview->lanes.size()) : 0;
    const int visibleHeight = std::max(0, viewport()->height() - data_->rulerHeight);
    verticalScrollBar()->setRange(
        0, std::max(0, laneCount * data_->rowHeight - visibleHeight));
    verticalScrollBar()->setPageStep(visibleHeight);
    if (!data_->preview) {
        horizontalScrollBar()->setRange(0, 0);
        return;
    }
    const long double duration = static_cast<long double>(data_->preview->end)
        - data_->preview->start;
    const long double drawable = std::max(1, viewport()->width() - data_->nameWidth);
    const long double visibleSpan = drawable / data_->pixelsPerTick;
    if (duration <= visibleSpan) {
        horizontalScrollBar()->setRange(0, 0);
    } else {
        horizontalScrollBar()->setRange(0, kScrollResolution);
        horizontalScrollBar()->setPageStep(std::clamp(
            static_cast<int>(kScrollResolution * visibleSpan / duration),
            1, kScrollResolution));
    }
}

void WaveformView::setZoom(const double pixelsPerTick, const double anchorX)
{
    if (!data_->preview) return;
    const qint64 anchorTick = xToTick(anchorX);
    data_->pixelsPerTick = std::clamp(pixelsPerTick, 1.0e-9, 1.0e9);
    data_->fitMode = false;
    updateScrollBars();
    if (horizontalScrollBar()->maximum() > 0) {
        const long double visibleSpan = std::max(1, viewport()->width() - data_->nameWidth)
            / data_->pixelsPerTick;
        const long double duration = static_cast<long double>(data_->preview->end)
            - data_->preview->start;
        const long double scrollable = std::max(0.0L, duration - visibleSpan);
        const long double desired = std::clamp(
            static_cast<long double>(anchorTick - data_->preview->start)
                - static_cast<long double>(anchorX - data_->nameWidth)
                      / data_->pixelsPerTick,
            0.0L, scrollable);
        horizontalScrollBar()->setValue(scrollable > 0.0L
            ? static_cast<int>(std::llround(desired / scrollable * kScrollResolution))
            : 0);
    }
    viewport()->update();
}

void WaveformView::updateAccessibleSummary()
{
    if (!data_->preview) {
        setAccessibleDescription(data_->status);
        return;
    }
    setAccessibleDescription(
        QStringLiteral("%1 waveform, %2 lanes, generation %3. %4")
            .arg(data_->preview->mode)
            .arg(data_->preview->lanes.size())
            .arg(data_->lastGeneration)
            .arg(data_->status));
}

void WaveformView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    const WaveformColorScheme scheme = data_->requestedTheme == QLatin1String("dark")
        ? WaveformColorScheme::Dark
        : data_->requestedTheme == QLatin1String("light")
            ? WaveformColorScheme::Light
            : waveformColorScheme(palette());
    const WaveformTheme theme = waveformTheme(scheme);
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(viewport()->rect(), theme.canvas);
    painter.fillRect(QRect(0, 0, data_->nameWidth, viewport()->height()), theme.panel);
    painter.fillRect(QRect(data_->nameWidth, 0,
                           viewport()->width() - data_->nameWidth,
                           data_->rulerHeight), theme.raised);
    painter.setPen(theme.border);
    painter.drawLine(data_->nameWidth, 0, data_->nameWidth, viewport()->height());
    painter.drawLine(0, data_->rulerHeight,
                     viewport()->width(), data_->rulerHeight);

    const QString badge = data_->preview
        ? (data_->preview->mode == QLatin1String("symbolic")
               ? tr("SYMBOLIC PREVIEW") : tr("SIMULATED RESULT"))
        : tr("WAVEFORM VIEW");
    painter.setFont(QFont(painter.font().family(), data_->compact ? 7 : 8,
                          QFont::DemiBold));
    const QRect badgeRect(8, 7, data_->nameWidth - 16,
                          data_->rulerHeight - 14);
    painter.setPen(data_->preview && data_->preview->mode == QLatin1String("symbolic")
                       ? theme.expected : theme.actual);
    painter.drawText(badgeRect, Qt::AlignLeft | Qt::AlignVCenter, badge);

    if (!data_->preview) {
        painter.setPen(theme.mutedText);
        painter.setFont(QFont(painter.font().family(), 10));
        painter.drawText(
            QRect(data_->nameWidth, data_->rulerHeight,
                  viewport()->width() - data_->nameWidth,
                  viewport()->height() - data_->rulerHeight),
            Qt::AlignCenter | Qt::TextWordWrap,
            data_->status);
        return;
    }

    const qint64 start = visibleStart();
    const qint64 end = visibleEnd();
    const qint64 gridStep = niceGridStep(data_->pixelsPerTick);
    const qint64 minorStep = std::max<qint64>(1, gridStep / 5);
    const qint64 firstMinor = floorToStep(start, minorStep);
    painter.setFont(QFont(painter.font().family(), data_->compact ? 7 : 8));
    for (qint64 tick = firstMinor; tick <= end;) {
        const double x = tickToX(tick);
        if (x >= data_->nameWidth) {
            const bool major = tick % gridStep == 0;
            painter.setPen(major ? theme.gridMajor : theme.gridMinor);
            painter.drawLine(QPointF(x, data_->rulerHeight),
                             QPointF(x, viewport()->height()));
            if (major) {
                painter.setPen(theme.mutedText);
                painter.drawText(
                    QRectF(x + 4, 0, 105, data_->rulerHeight - 2),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    displayTick(tick, data_->preview->unit));
            }
        }
        if (tick > std::numeric_limits<qint64>::max() - minorStep) break;
        tick += minorStep;
    }

    const int verticalOffset = verticalScrollBar()->value();
    const int firstRow = std::max(0, verticalOffset / data_->rowHeight);
    const int lastRow = std::min(
        static_cast<int>(data_->preview->lanes.size()),
        firstRow + viewport()->height() / data_->rowHeight + 3);
    for (int row = firstRow; row < lastRow; ++row) {
        const PreviewLane& lane = data_->preview->lanes[static_cast<std::size_t>(row)];
        const int top = data_->rulerHeight + row * data_->rowHeight - verticalOffset;
        const int bottom = top + data_->rowHeight;
        if (bottom < data_->rulerHeight || top > viewport()->height()) continue;
        if (lane.id == data_->selectedLane) {
            painter.fillRect(QRect(0, top, viewport()->width(), data_->rowHeight),
                             theme.selection);
        } else if ((row & 1) != 0) {
            QColor alternate = theme.raised;
            alternate.setAlpha(scheme == WaveformColorScheme::Dark ? 80 : 105);
            painter.fillRect(QRect(0, top, viewport()->width(), data_->rowHeight),
                             alternate);
        }
        painter.setPen(theme.border);
        painter.drawLine(0, bottom, viewport()->width(), bottom);

        painter.setPen(theme.text);
        painter.setFont(QFont(painter.font().family(), data_->compact ? 8 : 9,
                              QFont::DemiBold));
        const int nameBottom = data_->compact ? bottom : top + data_->rowHeight / 2 + 4;
        painter.drawText(
            QRect(8, top + 2, data_->nameWidth - 16, nameBottom - top - 2),
            Qt::AlignLeft | Qt::AlignVCenter,
            painter.fontMetrics().elidedText(lane.name, Qt::ElideMiddle,
                                             data_->nameWidth - 72));
        painter.setFont(QFont(painter.font().family(), 7));
        painter.setPen(theme.mutedText);
        const QString metadata = lane.width > 1
            ? QStringLiteral("%1 · %2-bit · %3")
                  .arg(lane.kind).arg(lane.width).arg(lane.provenance)
            : QStringLiteral("%1 · %2").arg(lane.kind, lane.provenance);
        if (!data_->compact) {
            painter.drawText(
                QRect(8, top + data_->rowHeight / 2 - 2,
                      data_->nameWidth - 16, data_->rowHeight / 2),
                Qt::AlignLeft | Qt::AlignVCenter,
                painter.fontMetrics().elidedText(metadata, Qt::ElideRight,
                                                 data_->nameWidth - 18));
        }

        const QColor laneColor = lane.kind == QLatin1String("clock")
            ? theme.clock : lane.width == 1 ? theme.bit : theme.bus;
        for (const PreviewSegment& segment : lane.segments) {
            if (segment.end < start || segment.start > end) continue;
            const qint64 segmentStart = std::max(start, segment.start);
            const qint64 segmentEnd = std::min(end, segment.end);
            const double left = tickToX(segmentStart);
            const double right = tickToX(segmentEnd);
            const QColor color = segment.unknown ? theme.unknown : laneColor;
            if (lane.width == 1 && !segment.unknown
                && (highValue(segment.value) || lowValue(segment.value))) {
                const double y = highValue(segment.value)
                    ? top + (data_->compact ? 7 : 11)
                    : bottom - (data_->compact ? 7 : 11);
                painter.setPen(QPen(color, data_->compact ? 1.3 : 1.7));
                painter.drawLine(QPointF(left, y), QPointF(right, y));
                continue;
            }

            const QRectF box(left,
                             top + (data_->compact ? 5 : 8),
                             std::max(1.0, right - left),
                             data_->rowHeight - (data_->compact ? 10 : 16));
            QColor fill = color;
            fill.setAlpha(scheme == WaveformColorScheme::Dark ? 82 : 52);
            painter.setPen(QPen(color, 1.3));
            painter.setBrush(fill);
            // Normal preview waveforms use square edges; unknown values retain their hatched boxes.
            painter.drawRect(box);
            if (segment.unknown && box.width() > 10) {
                painter.save();
                painter.setClipRect(box);
                painter.setPen(QPen(color, 0.7));
                for (double x = box.left() - box.height(); x < box.right(); x += 8.0) {
                    painter.drawLine(QPointF(x, box.bottom()),
                                     QPointF(x + box.height(), box.top()));
                }
                painter.restore();
            }
            if (!data_->compact && box.width() > 32) {
                painter.setPen(theme.text);
                painter.setFont(QFont(painter.font().family(), 8));
                painter.drawText(
                    box.adjusted(5, 0, -5, 0),
                    Qt::AlignCenter,
                    painter.fontMetrics().elidedText(segment.value,
                                                     Qt::ElideRight,
                                                     std::max(1, static_cast<int>(box.width()) - 10)));
            }
        }
    }

    if (data_->cursor >= start && data_->cursor <= end) {
        const double x = tickToX(data_->cursor);
        painter.setPen(QPen(theme.cursor, 1.4, Qt::DashLine));
        painter.drawLine(QPointF(x, 0), QPointF(x, viewport()->height()));
    }
    if (data_->state != QLatin1String("ready")) {
        QColor banner = data_->state == QLatin1String("failed")
            ? theme.diagnostic : data_->state == QLatin1String("stale")
                ? theme.unknown : theme.status;
        banner.setAlpha(225);
        const QRect bannerRect(data_->nameWidth + 12, data_->rulerHeight + 10,
                               std::max(80, viewport()->width() - data_->nameWidth - 24),
                               data_->compact ? 28 : 36);
        painter.setPen(Qt::NoPen);
        painter.setBrush(banner);
        painter.drawRoundedRect(bannerRect, 7, 7);
        painter.setPen(scheme == WaveformColorScheme::Dark ? Qt::black : Qt::white);
        painter.drawText(bannerRect.adjusted(10, 0, -10, 0),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         painter.fontMetrics().elidedText(data_->status,
                                                          Qt::ElideRight,
                                                          bannerRect.width() - 20));
    }
    if (hasFocus()) {
        QStyleOptionFocusRect option;
        option.initFrom(this);
        option.rect = viewport()->rect().adjusted(1, 1, -2, -2);
        option.backgroundColor = theme.canvas;
        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &option, &painter, this);
    }
}

void WaveformView::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    if (data_->fitMode && data_->preview) fitAll();
    else updateScrollBars();
}

void WaveformView::wheelEvent(QWheelEvent* event)
{
    if ((event->modifiers() & Qt::ControlModifier) != 0) {
        setZoom(data_->pixelsPerTick
                    * (event->angleDelta().y() > 0 ? 1.25 : 0.8),
                event->position().x());
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

void WaveformView::mousePressEvent(QMouseEvent* event)
{
    if (!data_->preview || event->button() != Qt::LeftButton
        || event->position().y() < data_->rulerHeight) {
        QAbstractScrollArea::mousePressEvent(event);
        return;
    }
    setFocus(Qt::MouseFocusReason);
    const int row = (static_cast<int>(event->position().y()) - data_->rulerHeight
                     + verticalScrollBar()->value()) / data_->rowHeight;
    if (row < 0 || row >= static_cast<int>(data_->preview->lanes.size())) return;
    const PreviewLane& lane = data_->preview->lanes[static_cast<std::size_t>(row)];
    data_->selectedLane = lane.id;
    if (event->position().x() >= data_->nameWidth) {
        data_->cursor = std::clamp(xToTick(event->position().x()),
                                   data_->preview->start, data_->preview->end);
    }
    viewport()->update();
    emit selectionChanged(data_->selectedLane, data_->cursor);
    event->accept();
}

void WaveformView::mouseDoubleClickEvent(QMouseEvent* event)
{
    mousePressEvent(event);
    if (event->button() == Qt::LeftButton) requestSourceNavigation();
}

void WaveformView::mouseMoveEvent(QMouseEvent* event)
{
    if (!data_->preview || event->position().x() < data_->nameWidth
        || event->position().y() < data_->rulerHeight) {
        QAbstractScrollArea::mouseMoveEvent(event);
        return;
    }
    const int row = (static_cast<int>(event->position().y()) - data_->rulerHeight
                     + verticalScrollBar()->value()) / data_->rowHeight;
    if (row >= 0 && row < static_cast<int>(data_->preview->lanes.size())) {
        const PreviewLane& lane = data_->preview->lanes[static_cast<std::size_t>(row)];
        const qint64 tick = std::clamp(xToTick(event->position().x()),
                                      data_->preview->start, data_->preview->end);
        const PreviewSegment* segment = segmentAt(lane, tick);
        emit cursorChanged(tick, lane.id, segment ? segment->value : QString{});
    }
    QAbstractScrollArea::mouseMoveEvent(event);
}

void WaveformView::keyPressEvent(QKeyEvent* event)
{
    if (!data_->preview) {
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }
    if (event->key() == Qt::Key_F) {
        fitAll();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) {
        zoomIn();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Minus) {
        zoomOut();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        requestSourceNavigation();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) {
        const qint64 step = std::max<qint64>(1, niceGridStep(data_->pixelsPerTick) / 5);
        revealTick(std::clamp(
            data_->cursor + (event->key() == Qt::Key_Left ? -step : step),
            data_->preview->start, data_->preview->end));
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Home || event->key() == Qt::Key_End) {
        revealTick(event->key() == Qt::Key_Home
                       ? data_->preview->start : data_->preview->end);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
        int index = 0;
        const auto selected = std::find_if(
            data_->preview->lanes.begin(), data_->preview->lanes.end(),
            [this](const PreviewLane& lane) { return lane.id == data_->selectedLane; });
        if (selected != data_->preview->lanes.end()) {
            index = static_cast<int>(std::distance(data_->preview->lanes.begin(), selected));
        }
        index = std::clamp(index + (event->key() == Qt::Key_Up ? -1 : 1),
                           0, std::max(0, static_cast<int>(data_->preview->lanes.size()) - 1));
        if (!data_->preview->lanes.empty()) {
            selectLane(data_->preview->lanes[static_cast<std::size_t>(index)].id);
        }
        event->accept();
        return;
    }
    QAbstractScrollArea::keyPressEvent(event);
}

void WaveformView::requestSourceNavigation()
{
    if (!data_->preview || data_->selectedLane.isEmpty()) return;
    const auto lane = std::find_if(
        data_->preview->lanes.begin(), data_->preview->lanes.end(),
        [this](const PreviewLane& candidate) {
            return candidate.id == data_->selectedLane;
        });
    if (lane == data_->preview->lanes.end() || !lane->source.available()) return;
    emit sourceNavigationRequested(lane->source.file,
                                   lane->source.line,
                                   lane->source.column,
                                   lane->source.semanticId,
                                   lane->id);
}

} // namespace wave
