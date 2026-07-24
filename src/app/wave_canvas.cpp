#include "wave_canvas.h"

#include <QApplication>
#include <QClipboard>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QScrollBar>
#include <QToolButton>
#include <QToolTip>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

namespace wave {
namespace {

const QColor kBackground(18, 22, 29);
const QColor kHeaderBackground(27, 34, 45);
const QColor kRulerBackground(23, 29, 39);
const QColor kGridMajor(61, 72, 89);
const QColor kGridMinor(39, 47, 60);
const QColor kTextPrimary(224, 229, 237);
const QColor kTextSecondary(151, 160, 176);
const QColor kSelection(68, 138, 255, 62);
const QColor kUndefined(239, 83, 80);
const QColor kMovableCursor(79, 195, 247);
const QColor kTemporaryCursor(186, 104, 200);
const QColor kLockedCursor(255, 202, 40);
const QColor kSelectedLockedCursor(102, 187, 106);
const QString kRangeMimeType = QStringLiteral("application/x-wave-workbench-range+json");

Tick floorToStep(const Tick value, const Tick step)
{
    if (step <= 0) return value;
    auto quotient = value / step;
    if (value < 0 && value % step != 0) --quotient;
    return quotient * step;
}

QColor laneColor(const Lane& lane)
{
    const QColor parsed(QString::fromStdString(lane.color));
    return parsed.isValid() ? parsed : QColor(79, 195, 247);
}

QString laneKindLabel(const LaneKind kind)
{
    return QString::fromLatin1(toString(kind).data(), static_cast<qsizetype>(toString(kind).size()));
}

} // namespace

WaveCanvas::WaveCanvas(QWidget* parent)
    : QAbstractScrollArea(parent)
{
    setFrameShape(QFrame::NoFrame);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    viewport()->setAutoFillBackground(false);
    addLaneButton_ = new QToolButton(viewport());
    addLaneButton_->setObjectName(QStringLiteral("CanvasAddSignalButton"));
    addLaneButton_->setText(tr("+  Add signal"));
    addLaneButton_->setToolTip(tr("Add a signal lane"));
    addLaneButton_->setAccessibleName(tr("Add signal"));
    addLaneButton_->setAutoRaise(true);
    addLaneButton_->setCursor(Qt::PointingHandCursor);
    addLaneButton_->setStyleSheet(QStringLiteral(
        "QToolButton {"
        " color: #e0e5ed;"
        " background: #222b39;"
        " border: 1px solid #3d4859;"
        " border-radius: 5px;"
        " padding-left: 12px;"
        " text-align: left;"
        " font-weight: 600;"
        "}"
        "QToolButton:hover, QToolButton:focus {"
        " background: #2e3c51;"
        " border-color: #6fa8ff;"
        "}"
        "QToolButton:pressed {"
        " background: #244f80;"
        "}"));
    addLaneButton_->hide();
    connect(addLaneButton_, &QToolButton::clicked, this, &WaveCanvas::addLaneRequested);
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, viewport(), qOverload<>(&QWidget::update));
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this] {
        updateAddLaneButtonGeometry();
        viewport()->update();
    });
}

void WaveCanvas::setDocument(
    Project* project,
    Scenario* scenario,
    CommandStack* commandStack)
{
    project_ = project;
    scenario_ = scenario;
    commandStack_ = commandStack;
    selectedLaneId_.clear();
    selectedLaneIds_.clear();
    selectionRange_.reset();
    movableCursorTick_.reset();
    temporaryCursorTick_.reset();
    selectedMarkerId_.clear();
    cursorInteraction_ = CursorInteraction::None;
    lockedMarkerOriginalRange_.reset();
    rebuildLaneLayout();
    fitPending_ = true;
    if (viewport()->width() > HeaderWidth + 40) {
        fitPending_ = false;
        fitScenario();
    }
}

void WaveCanvas::setTool(const Tool tool)
{
    const auto previousTool = tool_;
    tool_ = tool;
    drawing_ = false;
    cursorInteraction_ = CursorInteraction::None;
    lockedMarkerOriginalRange_.reset();
    activeEventId_.clear();
    if (previousTool == Tool::Marker && tool != Tool::Marker) {
        movableCursorTick_.reset();
        temporaryCursorTick_.reset();
        selectedMarkerId_.clear();
    }
    viewport()->setCursor(
        tool == Tool::Selection ? Qt::ArrowCursor : Qt::CrossCursor);
    viewport()->update();
}

void WaveCanvas::setSnapMode(const SnapMode mode)
{
    snapMode_ = mode;
}

WaveCanvas::Tool WaveCanvas::tool() const noexcept
{
    return tool_;
}

SnapMode WaveCanvas::snapMode() const noexcept
{
    return snapMode_;
}

QString WaveCanvas::selectedLaneId() const
{
    return QString::fromStdString(selectedLaneId_);
}

QStringList WaveCanvas::selectedLaneIds() const
{
    QStringList result;
    for (const auto& laneId : selectedLaneIds_) {
        result.append(QString::fromStdString(laneId));
    }
    return result;
}

Tick WaveCanvas::cursorTick() const noexcept
{
    return cursorTick_;
}

std::optional<Tick> WaveCanvas::movableCursorTick() const noexcept
{
    return movableCursorTick_;
}

std::optional<Tick> WaveCanvas::temporaryCursorTick() const noexcept
{
    return temporaryCursorTick_;
}

QString WaveCanvas::selectedMarkerId() const
{
    return QString::fromStdString(selectedMarkerId_);
}

std::optional<std::pair<Tick, Tick>> WaveCanvas::selectedTimeRange() const noexcept
{
    return selectionRange_;
}

void WaveCanvas::zoomIn()
{
    setScale(pixelsPerTick_ * 1.25, HeaderWidth + waveViewportWidth() / 2);
}

void WaveCanvas::zoomOut()
{
    setScale(pixelsPerTick_ / 1.25, HeaderWidth + waveViewportWidth() / 2);
}

void WaveCanvas::fitScenario()
{
    if (!scenario_ || scenario_->duration <= 0 || waveViewportWidth() <= 0) {
        return;
    }
    pixelsPerTick_ = std::clamp(
        static_cast<double>(waveViewportWidth()) / static_cast<double>(scenario_->duration),
        1.0e-9,
        100.0);
    updateScrollBars();
    horizontalScrollBar()->setValue(0);
    viewport()->update();
}

void WaveCanvas::fitSelection()
{
    if (!scenario_) return;
    if (!selectionRange_ || selectionRange_->second <= selectionRange_->first) {
        fitScenario();
        return;
    }
    const auto [start, end] = *selectionRange_;
    const auto duration = std::max<Tick>(1, end - start);
    pixelsPerTick_ = std::clamp(
        static_cast<double>(waveViewportWidth()) / static_cast<double>(duration),
        1.0e-9,
        100.0);
    updateScrollBars();
    const auto scroll = std::clamp(
        static_cast<double>(start) * pixelsPerTick_,
        0.0,
        static_cast<double>(horizontalScrollBar()->maximum()));
    horizontalScrollBar()->setValue(static_cast<int>(std::llround(scroll)));
    viewport()->update();
}

void WaveCanvas::refreshModel()
{
    rebuildLaneLayout();
    if (!selectedMarkerId_.empty() && !markerById(selectedMarkerId_)) {
        selectedMarkerId_.clear();
    }
    updateScrollBars();
    viewport()->update();
}

void WaveCanvas::revealLocation(const QString& laneId, const qint64 tick)
{
    if (!scenario_) return;
    const auto* lane = findLane(*scenario_, laneId.toStdString());
    if (!lane) return;
    selectedLaneId_ = lane->id;
    selectedLaneIds_ = {lane->id};
    cursorTick_ = std::clamp<Tick>(tick, 0, scenario_->duration);

    const auto desiredHorizontal = static_cast<double>(cursorTick_) * pixelsPerTick_
        - waveViewportWidth() / 2.0;
    horizontalScrollBar()->setValue(static_cast<int>(std::clamp(
        std::llround(desiredHorizontal),
        0LL,
        static_cast<long long>(horizontalScrollBar()->maximum()))));
    const auto layout = std::find_if(
        laneLayout_.begin(),
        laneLayout_.end(),
        [this](const LaneLayout& candidate) {
            return scenario_->lanes.at(candidate.laneIndex).id == selectedLaneId_;
        });
    if (layout != laneLayout_.end()) {
        verticalScrollBar()->setValue(std::clamp(
            layout->top - std::max(0, viewport()->height() - RulerHeight) / 2,
            0,
            verticalScrollBar()->maximum()));
    }
    emit selectionChanged(laneId, cursorTick_);
    viewport()->update();
}

void WaveCanvas::copySelection()
{
    if (!scenario_ || !selectionRange_
        || selectionRange_->second <= selectionRange_->first) {
        emit statusMessage(tr("Copy requires a non-empty time selection."));
        return;
    }
    auto laneIds = selectedLaneIds_;
    if (laneIds.empty() && !selectedLaneId_.empty()) laneIds.push_back(selectedLaneId_);
    if (laneIds.empty()) {
        emit statusMessage(tr("Copy requires at least one selected lane."));
        return;
    }
    const auto start = selectionRange_->first;
    const auto end = selectionRange_->second;
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), 1);
    root.insert(QStringLiteral("durationTick"), QString::number(end - start));
    QJsonArray lanes;
    for (const auto& laneId : laneIds) {
        const auto* lane = findLane(*scenario_, laneId);
        if (!lane || lane->kind == LaneKind::Group) continue;
        QJsonObject laneObject;
        laneObject.insert(QStringLiteral("laneId"), QString::fromStdString(laneId));
        QJsonArray segments;
        auto iterator = std::lower_bound(
            lane->segments.begin(),
            lane->segments.end(),
            start,
            [](const Segment& segment, const Tick tick) {
                return segment.end <= tick;
            });
        for (; iterator != lane->segments.end() && iterator->start < end; ++iterator) {
            const auto clippedStart = std::max(start, iterator->start);
            const auto clippedEnd = std::min(end, iterator->end);
            if (clippedEnd <= clippedStart) continue;
            QJsonObject segment;
            segment.insert(
                QStringLiteral("startTick"),
                QString::number(clippedStart - start));
            segment.insert(
                QStringLiteral("endTick"),
                QString::number(clippedEnd - start));
            segment.insert(QStringLiteral("value"), QString::fromStdString(iterator->value));
            QJsonObject extensions;
            for (const auto& [name, encoded] : iterator->extensions) {
                QJsonParseError error;
                const auto wrapper = QJsonDocument::fromJson(
                    QByteArrayLiteral("[") + QByteArray::fromStdString(encoded)
                        + QByteArrayLiteral("]"),
                    &error);
                if (error.error == QJsonParseError::NoError
                    && wrapper.isArray()
                    && !wrapper.array().isEmpty()) {
                    extensions.insert(QString::fromStdString(name), wrapper.array().first());
                }
            }
            if (!extensions.isEmpty()) segment.insert(QStringLiteral("extensions"), extensions);
            segments.append(segment);
        }
        laneObject.insert(QStringLiteral("segments"), segments);
        lanes.append(laneObject);
    }
    root.insert(QStringLiteral("lanes"), lanes);
    const auto document = QJsonDocument(root).toJson(QJsonDocument::Compact);
    auto* mime = new QMimeData;
    mime->setData(kRangeMimeType, document);
    mime->setText(QString::fromUtf8(document));
    QApplication::clipboard()->setMimeData(mime);
    emit statusMessage(
        tr("Copied %1 lane(s), %2.")
            .arg(lanes.size())
            .arg(QString::fromStdString(formatTick(end - start, project_->timeBase))));
}

void WaveCanvas::pasteAtCursor()
{
    if (!scenario_ || !commandStack_) return;
    const auto* mime = QApplication::clipboard()->mimeData();
    const auto content = mime->hasFormat(kRangeMimeType)
        ? mime->data(kRangeMimeType)
        : mime->text().toUtf8();
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(content, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        emit statusMessage(tr("Clipboard does not contain a Wave Workbench range."));
        return;
    }
    const auto root = document.object();
    if (root.value(QStringLiteral("schemaVersion")).toInt(-1) != 1
        || !root.value(QStringLiteral("durationTick")).isString()
        || !root.value(QStringLiteral("lanes")).isArray()) {
        emit statusMessage(tr("Clipboard range schema is invalid."));
        return;
    }
    bool validDuration = false;
    const auto duration = root.value(QStringLiteral("durationTick"))
                              .toString()
                              .toLongLong(&validDuration);
    if (!validDuration || duration <= 0 || cursorTick_ >= scenario_->duration) {
        emit statusMessage(tr("Clipboard range duration or destination is invalid."));
        return;
    }
    std::vector<CopiedLaneRange> copiedLanes;
    for (const auto& laneValue : root.value(QStringLiteral("lanes")).toArray()) {
        if (!laneValue.isObject()) continue;
        const auto laneObject = laneValue.toObject();
        const auto laneId = laneObject.value(QStringLiteral("laneId")).toString().toStdString();
        if (!findLane(*scenario_, laneId)
            || !laneObject.value(QStringLiteral("segments")).isArray()) {
            continue;
        }
        CopiedLaneRange copied;
        copied.laneId = laneId;
        for (const auto& segmentValue : laneObject.value(QStringLiteral("segments")).toArray()) {
            if (!segmentValue.isObject()) continue;
            const auto segmentObject = segmentValue.toObject();
            bool validStart = false;
            bool validEnd = false;
            Segment segment;
            segment.start = segmentObject.value(QStringLiteral("startTick"))
                                .toString()
                                .toLongLong(&validStart);
            segment.end = segmentObject.value(QStringLiteral("endTick"))
                              .toString()
                              .toLongLong(&validEnd);
            segment.value = segmentObject.value(QStringLiteral("value")).toString().toStdString();
            const auto extensions = segmentObject.value(QStringLiteral("extensions"));
            if (extensions.isObject()) {
                const auto extensionObject = extensions.toObject();
                for (auto iterator = extensionObject.begin();
                     iterator != extensionObject.end();
                     ++iterator) {
                    QJsonArray wrapper;
                    wrapper.append(iterator.value());
                    auto encoded = QJsonDocument(wrapper).toJson(QJsonDocument::Compact);
                    encoded.remove(0, 1);
                    encoded.chop(1);
                    segment.extensions.emplace(
                        iterator.key().toStdString(),
                        encoded.toStdString());
                }
            }
            if (validStart && validEnd) copied.relativeSegments.push_back(std::move(segment));
        }
        copiedLanes.push_back(std::move(copied));
    }
    if (copiedLanes.empty()) {
        emit statusMessage(tr("No clipboard lanes exist in this scenario."));
        return;
    }
    try {
        commandStack_->execute(std::make_unique<PasteRangeCommand>(
            *scenario_,
            copiedLanes,
            cursorTick_,
            duration));
    } catch (const std::exception& exception) {
        emit statusMessage(QString::fromUtf8(exception.what()));
        return;
    }
    selectedLaneIds_.clear();
    for (const auto& copied : copiedLanes) selectedLaneIds_.push_back(copied.laneId);
    selectedLaneId_ = selectedLaneIds_.front();
    selectionRange_ = std::pair{
        cursorTick_,
        cursorTick_ + std::min<Tick>(duration, scenario_->duration - cursorTick_),
    };
    emit modelEdited();
    emit commandAvailabilityChanged();
    refreshModel();
}

void WaveCanvas::insertPulse()
{
    if (!scenario_ || !project_ || !commandStack_) return;
    auto* lane = findLane(*scenario_, selectedLaneId_);
    if (!lane || lane->kind != LaneKind::Bit) {
        emit statusMessage(tr("Pulse requires a selected bit lane."));
        return;
    }
    Tick width = majorTickStep();
    if (const auto* clock = findClock(*project_, lane->clockDomainId)) {
        width = clock->period;
    }
    const auto end = std::min(scenario_->duration, cursorTick_ + std::max<Tick>(1, width));
    if (end <= cursorTick_) return;
    auto value = std::string{"1"};
    const auto covering = std::find_if(
        lane->segments.begin(),
        lane->segments.end(),
        [this](const Segment& segment) {
            return segment.start <= cursorTick_ && cursorTick_ < segment.end;
        });
    if (covering != lane->segments.end() && covering->value == "1") value = "0";
    commandStack_->execute(std::make_unique<SetLaneRangeCommand>(
        *scenario_,
        lane->id,
        cursorTick_,
        end,
        value));
    selectionRange_ = std::pair{cursorTick_, end};
    emit modelEdited();
    emit commandAvailabilityChanged();
    refreshModel();
}

void WaveCanvas::keyPressEvent(QKeyEvent* event)
{
    if (tool_ != Tool::Marker || !scenario_) {
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }

    if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
        && !selectedMarkerId_.empty()) {
        removeSelectedMarker();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) {
        const auto direction = event->key() == Qt::Key_Left ? Tick{-1} : Tick{1};
        const auto step = cursorKeyboardStep();
        if (!selectedMarkerId_.empty()) {
            moveSelectedMarkerBy(direction * step);
        } else {
            if (!movableCursorTick_) {
                movableCursorTick_ = std::clamp(cursorTick_, Tick{0}, scenario_->duration);
            }
            const auto current = *movableCursorTick_;
            const auto next = direction < 0
                ? current - std::min(current, step)
                : current + std::min(scenario_->duration - current, step);
            movableCursorTick_ = next;
            cursorTick_ = next;
            ensureCursorVisible(next);
            emit statusMessage(
                tr("Cursor %1").arg(QString::fromStdString(
                    formatTick(next, project_->timeBase))));
            viewport()->update();
        }
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape) {
        drawing_ = false;
        cursorInteraction_ = CursorInteraction::None;
        lockedMarkerOriginalRange_.reset();
        temporaryCursorTick_.reset();
        selectedMarkerId_.clear();
        viewport()->update();
        event->accept();
        return;
    }

    QAbstractScrollArea::keyPressEvent(event);
}

void WaveCanvas::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.fillRect(viewport()->rect(), kBackground);

    if (!project_ || !scenario_) {
        painter.setPen(kTextSecondary);
        painter.drawText(viewport()->rect(), Qt::AlignCenter, tr("No scenario loaded"));
        return;
    }

    drawRuler(painter);
    const auto [visibleStart, visibleEnd] = visibleTickRange();
    const auto verticalOffset = verticalScrollBar()->value();
    for (const auto& layout : laneLayout_) {
        const auto screenTop = RulerHeight + layout.top - verticalOffset;
        if (screenTop + layout.height < RulerHeight || screenTop > viewport()->height()) {
            continue;
        }
        drawLane(
            painter,
            scenario_->lanes.at(layout.laneIndex),
            layout,
            visibleStart,
            visibleEnd);
    }
    drawScenarioOverlays(painter, visibleStart, visibleEnd);
    drawAddLaneRow(painter);

    if (drawing_
        && tool_ == Tool::Marker
        && cursorInteraction_ == CursorInteraction::CreateLocked) {
        const auto left = xAtTick(std::min(drawStart_, drawCurrent_));
        const auto right = xAtTick(std::max(drawStart_, drawCurrent_));
        auto fill = kLockedCursor;
        fill.setAlpha(36);
        painter.fillRect(
            QRect(
                QPoint(left, RulerHeight),
                QPoint(std::max(left + 1, right), viewport()->height())),
            fill);
        painter.setPen(QPen(kLockedCursor, 1.5, Qt::DashLine));
        painter.drawLine(left, RulerHeight, left, viewport()->height());
        painter.drawLine(right, RulerHeight, right, viewport()->height());
    } else if (drawing_ && tool_ == Tool::Relation && !activeEventId_.empty()) {
        const auto* source = findEvent(*scenario_, activeEventId_);
        if (source) {
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(QPen(QColor(255, 183, 77), 2.0, Qt::DashLine));
            painter.drawLine(eventPoint(*source), interactionCurrent_);
            painter.setRenderHint(QPainter::Antialiasing, false);
        }
    } else if (drawing_ && tool_ == Tool::Transition && !activeEventId_.empty()) {
        const auto* source = findEvent(*scenario_, activeEventId_);
        if (source) {
            const auto x = xAtTick(drawCurrent_);
            painter.setPen(QPen(QColor(111, 168, 255), 2.0, Qt::DashLine));
            painter.drawLine(x, RulerHeight, x, viewport()->height());
            painter.drawLine(eventPoint(*source), QPoint(x, eventPoint(*source).y()));
        }
    } else if (drawing_ && !drawLaneId_.empty()) {
        const auto* lane = findLane(*scenario_, drawLaneId_);
        const auto iterator = std::find_if(
            laneLayout_.begin(),
            laneLayout_.end(),
            [this](const LaneLayout& layout) {
                return scenario_->lanes.at(layout.laneIndex).id == drawLaneId_;
            });
        if (lane && iterator != laneLayout_.end()) {
            const auto y = RulerHeight + iterator->top - verticalScrollBar()->value();
            const auto left = xAtTick(std::min(drawStart_, drawCurrent_));
            const auto right = xAtTick(std::max(drawStart_, drawCurrent_));
            const auto top = tool_ == Tool::Selection
                ? std::max(RulerHeight, std::min(selectionStartY_, interactionCurrent_.y()))
                : y;
            const auto bottom = tool_ == Tool::Selection
                ? std::min(
                    viewport()->height(),
                    std::max(selectionStartY_, interactionCurrent_.y()))
                : y + iterator->height;
            painter.fillRect(
                QRect(QPoint(left, top), QPoint(std::max(left + 1, right), bottom)),
                kSelection);
            painter.setPen(QColor(111, 168, 255));
            painter.drawRect(
                QRect(QPoint(left, top), QPoint(std::max(left + 1, right), bottom - 1)));
        }
    }

    painter.fillRect(QRect(0, 0, HeaderWidth, RulerHeight), kHeaderBackground);
    painter.setPen(kTextPrimary);
    QFont titleFont = painter.font();
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.drawText(
        QRect(14, 0, HeaderWidth - 20, RulerHeight),
        Qt::AlignVCenter | Qt::AlignLeft,
        tr("Signals"));
    painter.setPen(kGridMajor);
    painter.drawLine(HeaderWidth - 1, 0, HeaderWidth - 1, viewport()->height());
}

void WaveCanvas::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    if (fitPending_ && viewport()->width() > HeaderWidth + 40) {
        fitPending_ = false;
        fitScenario();
    } else {
        updateScrollBars();
    }
    updateAddLaneButtonGeometry();
}

void WaveCanvas::mousePressEvent(QMouseEvent* event)
{
    if (!scenario_ || event->button() != Qt::LeftButton) {
        QAbstractScrollArea::mousePressEvent(event);
        return;
    }
    const auto position = event->position().toPoint();
    if (addLaneRowRect().contains(position)) {
        emit addLaneRequested();
        return;
    }
    auto* lane = laneAtY(position.y());
    bypassSnap_ = event->modifiers().testFlag(Qt::AltModifier);

    if (tool_ == Tool::Marker) {
        setFocus(Qt::MouseFocusReason);
        if (position.x() < HeaderWidth) return;
        drawing_ = true;
        activeEventId_.clear();
        drawLaneId_.clear();
        drawStart_ = snappedTick(tickAtX(position.x()), lane);
        drawCurrent_ = drawStart_;
        cursorTick_ = drawStart_;
        interactionCurrent_ = position;
        if (lane) {
            selectedLaneId_ = lane->id;
            selectedLaneIds_ = {lane->id};
            emit selectionChanged(QString::fromStdString(lane->id), cursorTick_);
        }

        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            drawing_ = false;
            cursorInteraction_ = CursorInteraction::None;
            selectedMarkerId_.clear();
            lockedMarkerOriginalRange_.reset();
            if (movableCursorTick_) {
                temporaryCursorTick_ = drawStart_;
            } else {
                movableCursorTick_ = drawStart_;
                temporaryCursorTick_.reset();
            }
            viewport()->update();
            return;
        }

        if (const auto* marker = markerAtPosition(position)) {
            selectedMarkerId_ = marker->id;
            cursorInteraction_ = CursorInteraction::MoveLocked;
            lockedMarkerOriginalRange_ = std::pair{marker->start, marker->end};
            temporaryCursorTick_.reset();
            viewport()->update();
            return;
        }

        selectedMarkerId_.clear();
        lockedMarkerOriginalRange_.reset();
        temporaryCursorTick_.reset();
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            cursorInteraction_ = CursorInteraction::CreateLocked;
        } else {
            cursorInteraction_ = CursorInteraction::MoveActive;
            movableCursorTick_ = drawStart_;
        }
        viewport()->update();
        return;
    }

    if (tool_ == Tool::Relation || tool_ == Tool::Transition) {
        const auto* hitEvent = eventAtPosition(position);
        if (!hitEvent) {
            QToolTip::showText(
                event->globalPosition().toPoint(),
                tr("Start from a visible event marker"),
                viewport());
            return;
        }
        drawing_ = true;
        activeEventId_ = hitEvent->id;
        drawLaneId_ = hitEvent->laneId;
        drawStart_ = hitEvent->tick;
        drawCurrent_ = hitEvent->tick;
        interactionCurrent_ = position;
        selectedLaneId_ = hitEvent->laneId;
        selectedLaneIds_ = {hitEvent->laneId};
        cursorTick_ = hitEvent->tick;
        emit eventSelected(QString::fromStdString(hitEvent->id));
        emit selectionChanged(QString::fromStdString(hitEvent->laneId), hitEvent->tick);
        viewport()->update();
        return;
    }

    if (!lane) return;

    selectedLaneId_ = lane->id;
    cursorTick_ = snappedTick(tickAtX(position.x()), lane);
    emit selectionChanged(QString::fromStdString(selectedLaneId_), cursorTick_);

    if (const auto* hitEvent = eventAtPosition(position)) {
        emit eventSelected(QString::fromStdString(hitEvent->id));
    }

    if (tool_ == Tool::Selection
        && event->modifiers().testFlag(Qt::ControlModifier)) {
        const auto selected = std::find(
            selectedLaneIds_.begin(),
            selectedLaneIds_.end(),
            lane->id);
        if (selected == selectedLaneIds_.end()) {
            selectedLaneIds_.push_back(lane->id);
        } else if (selectedLaneIds_.size() > 1) {
            selectedLaneIds_.erase(selected);
        }
        drawing_ = false;
        viewport()->update();
    } else if (tool_ == Tool::Draw
        && (lane->kind == LaneKind::Clock
            || lane->kind == LaneKind::Bit
            || lane->kind == LaneKind::Bus
            || lane->kind == LaneKind::Enum
            || lane->kind == LaneKind::Transaction
            || lane->kind == LaneKind::Event)) {
        selectedLaneIds_ = {lane->id};
        drawing_ = true;
        drawLaneId_ = lane->id;
        drawStart_ = cursorTick_;
        drawCurrent_ = cursorTick_;
        if (lane->kind == LaneKind::Bit) {
            const auto* layout = layoutAtY(position.y());
            const auto laneTop = layout
                ? RulerHeight + layout->top - verticalScrollBar()->value()
                : position.y();
            drawValue_ = position.y() < laneTop + lane->height / 2 ? "1" : "0";
        } else {
            drawValue_.clear();
        }
        viewport()->update();
    } else if (tool_ == Tool::Selection) {
        selectedLaneIds_ = {lane->id};
        drawing_ = true;
        drawLaneId_ = lane->id;
        drawStart_ = cursorTick_;
        drawCurrent_ = cursorTick_;
        selectionStartY_ = position.y();
        viewport()->update();
    } else {
        viewport()->update();
    }
}

void WaveCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if (!scenario_) return;
    const auto position = event->position().toPoint();
    bypassSnap_ = event->modifiers().testFlag(Qt::AltModifier);
    interactionCurrent_ = position;
    const auto* lane = laneAtY(position.y());
    const auto rawTick = tickAtX(position.x());
    cursorTick_ = snappedTick(rawTick, lane);
    if (drawing_) {
        const auto* drawLane = drawLaneId_.empty()
            ? lane
            : findLane(*scenario_, drawLaneId_);
        drawCurrent_ = std::clamp(
            snappedTick(rawTick, drawLane),
            Tick{0},
            scenario_->duration);
        if (tool_ == Tool::Marker
            && cursorInteraction_ == CursorInteraction::MoveActive) {
            movableCursorTick_ = drawCurrent_;
            if (drawCurrent_ == drawStart_) {
                temporaryCursorTick_.reset();
            } else {
                temporaryCursorTick_ = drawStart_;
            }
        }
        viewport()->update();
    }

    if (tool_ == Tool::Marker) {
        auto message = tr("Cursor %1").arg(QString::fromStdString(
            formatTick(cursorTick_, project_->timeBase)));
        if (movableCursorTick_ && temporaryCursorTick_) {
            message += QStringLiteral("  |  ")
                + cursorDeltaText(*temporaryCursorTick_, *movableCursorTick_);
        }
        emit statusMessage(message);
    } else {
        emit statusMessage(
            tr("%1  |  %2")
                .arg(QString::fromStdString(formatTick(cursorTick_, project_->timeBase)))
                .arg(lane ? QString::fromStdString(lane->name) : tr("No lane")));
    }
}

void WaveCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && drawing_) {
        bypassSnap_ = event->modifiers().testFlag(Qt::AltModifier);
        switch (tool_) {
        case Tool::Draw:
            commitDraw(event->position().toPoint());
            break;
        case Tool::Marker:
            commitMarker(event->position().toPoint());
            break;
        case Tool::Relation:
            commitRelation(event->position().toPoint());
            break;
        case Tool::Transition:
            commitTransition(event->position().toPoint());
            break;
        case Tool::Selection:
            drawCurrent_ = snappedTick(
                tickAtX(event->position().toPoint().x()),
                laneAtY(event->position().toPoint().y()));
            selectionRange_ = std::pair{
                std::min(drawStart_, drawCurrent_),
                std::max(drawStart_, drawCurrent_),
            };
            if (const auto* releaseLane = laneAtY(event->position().toPoint().y())) {
                const auto first = std::find_if(
                    laneLayout_.begin(),
                    laneLayout_.end(),
                    [this](const LaneLayout& layout) {
                        return scenario_->lanes.at(layout.laneIndex).id == drawLaneId_;
                    });
                const auto last = std::find_if(
                    laneLayout_.begin(),
                    laneLayout_.end(),
                    [this, releaseLane](const LaneLayout& layout) {
                        return scenario_->lanes.at(layout.laneIndex).id == releaseLane->id;
                    });
                if (first != laneLayout_.end() && last != laneLayout_.end()) {
                    const auto firstIndex = static_cast<std::size_t>(
                        std::min(first - laneLayout_.begin(), last - laneLayout_.begin()));
                    const auto lastIndex = static_cast<std::size_t>(
                        std::max(first - laneLayout_.begin(), last - laneLayout_.begin()));
                    selectedLaneIds_.clear();
                    for (auto index = firstIndex; index <= lastIndex; ++index) {
                        selectedLaneIds_.push_back(
                            scenario_->lanes.at(laneLayout_[index].laneIndex).id);
                    }
                }
            }
            drawing_ = false;
            bypassSnap_ = false;
            viewport()->update();
            break;
        }
        bypassSnap_ = false;
        return;
    }
    QAbstractScrollArea::mouseReleaseEvent(event);
}

void WaveCanvas::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (!scenario_ || !commandStack_) return;
    bypassSnap_ = event->modifiers().testFlag(Qt::AltModifier);
    auto* lane = laneAtY(event->position().toPoint().y());
    if (!lane || (lane->kind != LaneKind::Bus
                  && lane->kind != LaneKind::Enum
                  && lane->kind != LaneKind::Transaction
                  && lane->kind != LaneKind::Event)) {
        return;
    }
    const auto start = snappedTick(tickAtX(event->position().toPoint().x()), lane);
    const auto end = std::min(scenario_->duration, start + std::max<Tick>(1, majorTickStep()));
    bool accepted = false;
    const auto value = QInputDialog::getText(
        this,
        tr("Set bus value"),
        tr("Value for %1:").arg(QString::fromStdString(lane->name)),
        QLineEdit::Normal,
        {},
        &accepted);
    if (!accepted || value.isEmpty() || end <= start) return;

    const auto validation = validateLaneValue(*lane, value.toStdString());
    if (!validation.valid) {
        QToolTip::showText(
            event->globalPosition().toPoint(),
            QString::fromStdString(validation.error),
            viewport());
        return;
    }
    try {
        commandStack_->execute(std::make_unique<SetLaneRangeCommand>(
            *scenario_,
            lane->id,
            start,
            end,
            validation.normalizedValue));
    } catch (const std::exception& exception) {
        QToolTip::showText(
            event->globalPosition().toPoint(),
            QString::fromUtf8(exception.what()),
            viewport());
        return;
    }
    emit modelEdited();
    emit commandAvailabilityChanged();
    bypassSnap_ = false;
    viewport()->update();
}

void WaveCanvas::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        const auto steps = event->angleDelta().y() / 120.0;
        setScale(
            pixelsPerTick_ * std::pow(1.18, steps),
            static_cast<int>(event->position().x()));
        event->accept();
        return;
    }
    if (event->modifiers().testFlag(Qt::ShiftModifier)) {
        horizontalScrollBar()->setValue(
            horizontalScrollBar()->value() - event->angleDelta().y());
        event->accept();
        return;
    }
    QAbstractScrollArea::wheelEvent(event);
}

void WaveCanvas::rebuildLaneLayout()
{
    laneLayout_.clear();
    rebuildSnapIndex();
    if (!scenario_) {
        updateScrollBars();
        return;
    }
    int top = 0;
    for (std::size_t index = 0; index < scenario_->lanes.size(); ++index) {
        const auto& lane = scenario_->lanes[index];
        if (!lane.visible) continue;
        const auto height = std::clamp(lane.height, 30, 240);
        laneLayout_.push_back({index, top, height});
        top += height;
    }
    updateScrollBars();
}

void WaveCanvas::rebuildSnapIndex()
{
    signalEdgeIndex_.clear();
    markerTickIndex_.clear();
    if (!scenario_) return;

    for (const auto& lane : scenario_->lanes) {
        if (lane.kind == LaneKind::Group) continue;
        signalEdgeIndex_.reserve(signalEdgeIndex_.size() + lane.segments.size() * 2);
        for (const auto& segment : lane.segments) {
            signalEdgeIndex_.push_back(segment.start);
            signalEdgeIndex_.push_back(segment.end);
        }
    }
    std::sort(signalEdgeIndex_.begin(), signalEdgeIndex_.end());
    signalEdgeIndex_.erase(
        std::unique(signalEdgeIndex_.begin(), signalEdgeIndex_.end()),
        signalEdgeIndex_.end());

    markerTickIndex_.reserve(scenario_->markers.size() * 2);
    for (const auto& marker : scenario_->markers) {
        markerTickIndex_.push_back(marker.start);
        markerTickIndex_.push_back(marker.end);
    }
    std::sort(markerTickIndex_.begin(), markerTickIndex_.end());
    markerTickIndex_.erase(
        std::unique(markerTickIndex_.begin(), markerTickIndex_.end()),
        markerTickIndex_.end());
}

void WaveCanvas::updateScrollBars()
{
    const auto horizontalMaximum = static_cast<int>(std::clamp(
        contentWidth() - static_cast<double>(waveViewportWidth()),
        0.0,
        static_cast<double>(std::numeric_limits<int>::max())));
    horizontalScrollBar()->setRange(0, horizontalMaximum);
    horizontalScrollBar()->setPageStep(std::max(1, waveViewportWidth()));
    horizontalScrollBar()->setSingleStep(48);

    const auto laneContentHeight = laneLayout_.empty()
        ? 0
        : laneLayout_.back().top + laneLayout_.back().height;
    const auto contentHeight = scenario_
        ? laneContentHeight + AddLaneRowHeight
        : 0;
    const auto visibleHeight = std::max(0, viewport()->height() - RulerHeight);
    verticalScrollBar()->setRange(0, std::max(0, contentHeight - visibleHeight));
    verticalScrollBar()->setPageStep(std::max(1, visibleHeight));
    verticalScrollBar()->setSingleStep(40);
    updateAddLaneButtonGeometry();
}

QRect WaveCanvas::addLaneRowRect() const
{
    if (!scenario_) return {};
    const auto contentTop = laneLayout_.empty()
        ? 0
        : laneLayout_.back().top + laneLayout_.back().height;
    return {
        0,
        RulerHeight + contentTop - verticalScrollBar()->value(),
        viewport()->width(),
        AddLaneRowHeight,
    };
}

void WaveCanvas::updateAddLaneButtonGeometry()
{
    if (!addLaneButton_) return;
    const auto row = addLaneRowRect();
    const auto visible = scenario_
        && row.bottom() >= RulerHeight
        && row.top() < viewport()->height();
    addLaneButton_->setVisible(visible);
    if (!visible) return;
    addLaneButton_->setGeometry(
        10,
        row.top() + 6,
        HeaderWidth - 20,
        row.height() - 12);
    addLaneButton_->raise();
}

void WaveCanvas::setScale(const double scale, const int anchorX)
{
    if (!scenario_) return;
    const auto clampedAnchor = std::clamp(anchorX, HeaderWidth, viewport()->width());
    const auto anchorTick = tickAtX(clampedAnchor);
    pixelsPerTick_ = std::clamp(scale, 1.0e-9, 100.0);
    updateScrollBars();
    const auto newScroll = static_cast<double>(anchorTick) * pixelsPerTick_
        - static_cast<double>(clampedAnchor - HeaderWidth);
    horizontalScrollBar()->setValue(static_cast<int>(std::clamp(
        std::llround(newScroll),
        0LL,
        static_cast<long long>(horizontalScrollBar()->maximum()))));
    viewport()->update();
}

double WaveCanvas::contentWidth() const
{
    return scenario_
        ? std::max(0.0, static_cast<double>(scenario_->duration) * pixelsPerTick_)
        : 0.0;
}

int WaveCanvas::waveViewportWidth() const
{
    return std::max(1, viewport()->width() - HeaderWidth);
}

Tick WaveCanvas::tickAtX(const int x) const
{
    if (!scenario_) return 0;
    const auto contentX = static_cast<double>(horizontalScrollBar()->value())
        + static_cast<double>(x - HeaderWidth);
    const auto tick = static_cast<long double>(contentX) / pixelsPerTick_;
    return std::clamp<Tick>(
        static_cast<Tick>(std::llround(tick)),
        0,
        scenario_->duration);
}

int WaveCanvas::xAtTick(const Tick tick) const
{
    const auto x = static_cast<double>(HeaderWidth)
        + static_cast<double>(tick) * pixelsPerTick_
        - horizontalScrollBar()->value();
    return static_cast<int>(std::clamp(
        x,
        static_cast<double>(std::numeric_limits<int>::min()),
        static_cast<double>(std::numeric_limits<int>::max())));
}

const WaveCanvas::LaneLayout* WaveCanvas::layoutAtY(const int y) const
{
    if (y < RulerHeight || laneLayout_.empty()) return nullptr;
    const auto contentY = y - RulerHeight + verticalScrollBar()->value();
    const auto iterator = std::upper_bound(
        laneLayout_.begin(),
        laneLayout_.end(),
        contentY,
        [](const int value, const LaneLayout& layout) { return value < layout.top; });
    if (iterator == laneLayout_.begin()) return nullptr;
    const auto& candidate = *std::prev(iterator);
    return contentY < candidate.top + candidate.height ? &candidate : nullptr;
}

Lane* WaveCanvas::laneAtY(const int y)
{
    const auto* layout = layoutAtY(y);
    return layout && scenario_ ? &scenario_->lanes.at(layout->laneIndex) : nullptr;
}

const Lane* WaveCanvas::laneAtY(const int y) const
{
    const auto* layout = layoutAtY(y);
    return layout && scenario_ ? &scenario_->lanes.at(layout->laneIndex) : nullptr;
}

Marker* WaveCanvas::markerById(const std::string& markerId)
{
    if (!scenario_) return nullptr;
    const auto marker = std::find_if(
        scenario_->markers.begin(),
        scenario_->markers.end(),
        [&markerId](const Marker& candidate) {
            return candidate.id == markerId;
        });
    return marker == scenario_->markers.end() ? nullptr : &*marker;
}

const Marker* WaveCanvas::markerById(const std::string& markerId) const
{
    if (!scenario_) return nullptr;
    const auto marker = std::find_if(
        scenario_->markers.begin(),
        scenario_->markers.end(),
        [&markerId](const Marker& candidate) {
            return candidate.id == markerId;
        });
    return marker == scenario_->markers.end() ? nullptr : &*marker;
}

const Marker* WaveCanvas::markerAtPosition(const QPoint& position) const
{
    if (!scenario_
        || position.x() < HeaderWidth
        || position.y() < RulerHeight) {
        return nullptr;
    }

    constexpr int HitRadius = 7;
    for (auto marker = scenario_->markers.rbegin();
         marker != scenario_->markers.rend();
         ++marker) {
        const auto [start, end] = markerDisplayRange(*marker);
        const auto left = xAtTick(start);
        const auto right = xAtTick(end);
        if (std::abs(position.x() - left) <= HitRadius
            || (start != end && std::abs(position.x() - right) <= HitRadius)
            || (start != end
                && position.y() <= RulerHeight + 24
                && position.x() >= std::min(left, right)
                && position.x() <= std::max(left, right))) {
            return &*marker;
        }
    }
    return nullptr;
}

std::pair<Tick, Tick> WaveCanvas::markerDisplayRange(const Marker& marker) const
{
    if (cursorInteraction_ != CursorInteraction::MoveLocked
        || marker.id != selectedMarkerId_
        || !lockedMarkerOriginalRange_
        || !scenario_) {
        return {marker.start, marker.end};
    }
    const auto [originalStart, originalEnd] = *lockedMarkerOriginalRange_;
    const auto requested = drawCurrent_ - drawStart_;
    const auto offset = std::clamp(
        requested,
        -originalStart,
        scenario_->duration - originalEnd);
    return {originalStart + offset, originalEnd + offset};
}

Tick WaveCanvas::cursorKeyboardStep() const
{
    if (!project_) return 1;
    return std::max<Tick>(
        1,
        toTicks(10, TimeUnit::Nanosecond, project_->timeBase).value_or(1));
}

QString WaveCanvas::cursorValue(const Lane& lane) const
{
    if (!project_ || !movableCursorTick_ || lane.kind == LaneKind::Group) {
        return {};
    }
    const auto tick = *movableCursorTick_;
    if (lane.kind == LaneKind::Clock) {
        const auto* clock = findClock(*project_, lane.clockDomainId);
        return clock && clock->isValid()
            ? QString(QChar::fromLatin1(clockValueAt(*clock, lane, tick)))
            : QStringLiteral("X");
    }

    const auto segment = std::upper_bound(
        lane.segments.begin(),
        lane.segments.end(),
        tick,
        [](const Tick value, const Segment& candidate) {
            return value < candidate.start;
        });
    if (segment == lane.segments.begin()) return QStringLiteral("?");
    const auto& candidate = *std::prev(segment);
    return candidate.start <= tick && tick < candidate.end
        ? QString::fromStdString(candidate.value)
        : QStringLiteral("?");
}

QString WaveCanvas::cursorDeltaText(const Tick from, const Tick to) const
{
    const auto delta = to - from;
    auto text = project_
        ? QString::fromStdString(formatTick(delta, project_->timeBase))
        : QString::number(delta) + tr(" ticks");
    if (delta > 0) text.prepend(QLatin1Char('+'));
    return QStringLiteral("\u0394 %1").arg(text);
}

void WaveCanvas::ensureCursorVisible(const Tick tick)
{
    const auto x = xAtTick(tick);
    if (x >= HeaderWidth + 24 && x <= viewport()->width() - 24) return;
    const auto desired = static_cast<double>(tick) * pixelsPerTick_
        - waveViewportWidth() / 2.0;
    horizontalScrollBar()->setValue(static_cast<int>(std::clamp(
        std::llround(desired),
        0LL,
        static_cast<long long>(horizontalScrollBar()->maximum()))));
}

void WaveCanvas::removeSelectedMarker()
{
    if (!scenario_ || !commandStack_ || selectedMarkerId_.empty()) return;
    try {
        commandStack_->execute(std::make_unique<RemoveMarkerCommand>(
            *scenario_,
            selectedMarkerId_));
    } catch (const std::exception& exception) {
        emit statusMessage(QString::fromUtf8(exception.what()));
        return;
    }
    selectedMarkerId_.clear();
    emit modelEdited();
    emit commandAvailabilityChanged();
    refreshModel();
}

void WaveCanvas::moveSelectedMarkerBy(const Tick delta)
{
    if (!scenario_ || !commandStack_ || selectedMarkerId_.empty()) return;
    const auto* marker = markerById(selectedMarkerId_);
    if (!marker) {
        selectedMarkerId_.clear();
        return;
    }
    const auto offset = std::clamp(
        delta,
        -marker->start,
        scenario_->duration - marker->end);
    if (offset == 0) return;
    auto replacement = *marker;
    replacement.start += offset;
    replacement.end += offset;
    try {
        commandStack_->execute(std::make_unique<ChangeMarkerCommand>(
            *scenario_,
            selectedMarkerId_,
            replacement));
    } catch (const std::exception& exception) {
        emit statusMessage(QString::fromUtf8(exception.what()));
        return;
    }
    cursorTick_ = replacement.start;
    emit modelEdited();
    emit commandAvailabilityChanged();
    refreshModel();
    ensureCursorVisible(cursorTick_);
}

Tick WaveCanvas::snappedTick(const Tick input, const Lane* lane) const
{
    if (!project_) return input;
    if (bypassSnap_ && scenario_) {
        return std::clamp(input, Tick{0}, scenario_->duration);
    }
    const auto grid = std::max<Tick>(
        1,
        toTicks(10, TimeUnit::Nanosecond, project_->timeBase).value_or(1));
    const ClockDomain* clock = nullptr;
    if (lane && !lane->clockDomainId.empty()) {
        clock = findClock(*project_, lane->clockDomainId);
    }
    if (!clock && !project_->clockDomains.empty()) {
        clock = &project_->clockDomains.front();
    }

    const SnapContext context{
        grid,
        majorTickStep(),
        clock,
        signalEdgeIndex_,
        markerTickIndex_,
    };
    return std::clamp(snapTick(input, snapMode_, context), Tick{0}, scenario_->duration);
}

Tick WaveCanvas::majorTickStep() const
{
    if (pixelsPerTick_ <= 0.0) return 1;
    const auto raw = 110.0 / pixelsPerTick_;
    const auto exponent = std::floor(std::log10(std::max(raw, 1.0)));
    const auto magnitude = std::pow(10.0, exponent);
    const auto normalized = raw / magnitude;
    const auto multiplier = normalized <= 1.0 ? 1.0 : normalized <= 2.0 ? 2.0 : normalized <= 5.0 ? 5.0 : 10.0;
    return std::max<Tick>(1, static_cast<Tick>(std::llround(multiplier * magnitude)));
}

std::pair<Tick, Tick> WaveCanvas::visibleTickRange() const
{
    if (!scenario_) return {0, 0};
    const auto start = tickAtX(HeaderWidth);
    const auto end = tickAtX(viewport()->width());
    const auto padding = std::max<Tick>(1, majorTickStep());
    return {
        std::max<Tick>(0, start - std::min(start, padding)),
        std::min<Tick>(scenario_->duration, end + padding),
    };
}

void WaveCanvas::commitDraw(const QPoint& releasePosition)
{
    drawing_ = false;
    if (!scenario_ || !commandStack_) return;
    auto* lane = findLane(*scenario_, drawLaneId_);
    if (!lane) return;
    drawCurrent_ = snappedTick(tickAtX(releasePosition.x()), lane);
    auto start = std::min(drawStart_, drawCurrent_);
    auto end = std::max(drawStart_, drawCurrent_);
    if (start == end) {
        end = std::min(scenario_->duration, start + std::max<Tick>(1, majorTickStep()));
    }
    if (end <= start) return;
    selectionRange_ = std::pair{start, end};

    auto value = drawValue_;
    if (lane->kind == LaneKind::Clock) {
        bool accepted = false;
        const auto choice = QInputDialog::getItem(
            this,
            tr("Clock interval"),
            tr("Mode for %1:").arg(QString::fromStdString(lane->name)),
            {
                tr("Gated — hold low"),
                tr("Disabled — drive X"),
                tr("Run — clear override"),
            },
            0,
            false,
            &accepted);
        if (!accepted) {
            viewport()->update();
            return;
        }
        try {
            if (choice.startsWith(tr("Run"))) {
                commandStack_->execute(std::make_unique<ClearLaneRangeCommand>(
                    *scenario_,
                    lane->id,
                    start,
                    end));
            } else {
                value = choice.startsWith(tr("Gated")) ? "gated" : "disabled";
                commandStack_->execute(std::make_unique<SetLaneRangeCommand>(
                    *scenario_,
                    lane->id,
                    start,
                    end,
                    value));
            }
        } catch (const std::exception& exception) {
            QToolTip::showText(
                viewport()->mapToGlobal(releasePosition),
                QString::fromUtf8(exception.what()),
                viewport());
            viewport()->update();
            return;
        }
        emit modelEdited();
        emit commandAvailabilityChanged();
        refreshModel();
        return;
    }
    if (lane->kind == LaneKind::Bus
        || lane->kind == LaneKind::Enum
        || lane->kind == LaneKind::Transaction
        || lane->kind == LaneKind::Event) {
        bool accepted = false;
        const auto entered = QInputDialog::getText(
            this,
            tr("Set range value"),
            tr("Value for %1:").arg(QString::fromStdString(lane->name)),
            QLineEdit::Normal,
            {},
            &accepted);
        if (!accepted) {
            viewport()->update();
            return;
        }
        value = entered.toStdString();
    }

    const auto validation = validateLaneValue(*lane, value);
    if (!validation.valid) {
        QToolTip::showText(
            viewport()->mapToGlobal(releasePosition),
            QString::fromStdString(validation.error),
            viewport());
        viewport()->update();
        return;
    }
    try {
        commandStack_->execute(std::make_unique<SetLaneRangeCommand>(
            *scenario_,
            lane->id,
            start,
            end,
            validation.normalizedValue));
    } catch (const std::exception& exception) {
        QToolTip::showText(
            viewport()->mapToGlobal(releasePosition),
            QString::fromUtf8(exception.what()),
            viewport());
        viewport()->update();
        return;
    }
    emit modelEdited();
    emit commandAvailabilityChanged();
    viewport()->update();
}

void WaveCanvas::commitMarker(const QPoint& releasePosition)
{
    if (!scenario_ || !commandStack_) {
        drawing_ = false;
        cursorInteraction_ = CursorInteraction::None;
        lockedMarkerOriginalRange_.reset();
        return;
    }
    drawCurrent_ = snappedTick(
        tickAtX(releasePosition.x()),
        laneAtY(releasePosition.y()));
    const auto interaction = cursorInteraction_;
    drawing_ = false;
    cursorInteraction_ = CursorInteraction::None;

    if (interaction == CursorInteraction::MoveActive) {
        movableCursorTick_ = drawCurrent_;
        cursorTick_ = drawCurrent_;
        if (drawCurrent_ == drawStart_) {
            temporaryCursorTick_.reset();
        } else {
            temporaryCursorTick_ = drawStart_;
        }
        lockedMarkerOriginalRange_.reset();
        viewport()->update();
        return;
    }

    if (interaction == CursorInteraction::MoveLocked) {
        const auto* marker = markerById(selectedMarkerId_);
        if (!marker || !lockedMarkerOriginalRange_) {
            selectedMarkerId_.clear();
            lockedMarkerOriginalRange_.reset();
            viewport()->update();
            return;
        }
        const auto [originalStart, originalEnd] = *lockedMarkerOriginalRange_;
        const auto requested = drawCurrent_ - drawStart_;
        const auto offset = std::clamp(
            requested,
            -originalStart,
            scenario_->duration - originalEnd);
        if (offset != 0) {
            auto replacement = *marker;
            replacement.start = originalStart + offset;
            replacement.end = originalEnd + offset;
            try {
                commandStack_->execute(std::make_unique<ChangeMarkerCommand>(
                    *scenario_,
                    selectedMarkerId_,
                    replacement));
            } catch (const std::exception& exception) {
                QToolTip::showText(
                    viewport()->mapToGlobal(releasePosition),
                    QString::fromUtf8(exception.what()),
                    viewport());
                lockedMarkerOriginalRange_.reset();
                viewport()->update();
                return;
            }
            cursorTick_ = replacement.start;
            emit modelEdited();
            emit commandAvailabilityChanged();
            refreshModel();
        }
        lockedMarkerOriginalRange_.reset();
        viewport()->update();
        return;
    }

    lockedMarkerOriginalRange_.reset();
    if (interaction != CursorInteraction::CreateLocked) {
        viewport()->update();
        return;
    }

    const auto start = std::min(drawStart_, drawCurrent_);
    const auto end = std::max(drawStart_, drawCurrent_);
    Marker marker;
    marker.id = makeStableId("marker");
    marker.name = start == end
        ? "Locked cursor " + std::to_string(scenario_->markers.size() + 1)
        : "Locked range " + std::to_string(scenario_->markers.size() + 1);
    marker.start = start;
    marker.end = end;
    marker.kind = start == end ? MarkerKind::Point : MarkerKind::Interval;
    try {
        commandStack_->execute(std::make_unique<AddMarkerCommand>(*scenario_, marker));
    } catch (const std::exception& exception) {
        QToolTip::showText(
            viewport()->mapToGlobal(releasePosition),
            QString::fromUtf8(exception.what()),
            viewport());
        viewport()->update();
        return;
    }
    selectedMarkerId_ = marker.id;
    cursorTick_ = drawCurrent_;
    emit modelEdited();
    emit commandAvailabilityChanged();
    refreshModel();
}

void WaveCanvas::commitRelation(const QPoint& releasePosition)
{
    drawing_ = false;
    if (!scenario_ || !commandStack_ || activeEventId_.empty()) return;
    const auto* source = findEvent(*scenario_, activeEventId_);
    const auto* target = eventAtPosition(releasePosition);
    if (!source || !target || source->id == target->id) {
        QToolTip::showText(
            viewport()->mapToGlobal(releasePosition),
            tr("Finish on a different visible event marker"),
            viewport());
        viewport()->update();
        return;
    }

    const auto* sourceLane = findLane(*scenario_, source->laneId);
    const auto* sourceClock = sourceLane && project_
        ? findClock(*project_, sourceLane->clockDomainId)
        : nullptr;
    const auto baseWindow = sourceClock ? sourceClock->period : majorTickStep();
    const auto window = baseWindow > std::numeric_limits<Tick>::max() / 4
        ? std::numeric_limits<Tick>::max()
        : std::max<Tick>(1, baseWindow * 4);
    Relation relation;
    relation.id = makeStableId("relation");
    relation.sourceEventId = source->id;
    relation.targetEventId = target->id;
    relation.minimumDelay = 0;
    relation.maximumDelay = window;
    relation.clockDomainId = sourceLane ? sourceLane->clockDomainId : std::string{};
    relation.severity = Severity::Error;
    relation.description = "Timing relation";
    try {
        commandStack_->execute(std::make_unique<AddRelationCommand>(*scenario_, relation));
    } catch (const std::exception& exception) {
        QToolTip::showText(
            viewport()->mapToGlobal(releasePosition),
            QString::fromUtf8(exception.what()),
            viewport());
        viewport()->update();
        return;
    }
    activeEventId_.clear();
    emit modelEdited();
    emit commandAvailabilityChanged();
    viewport()->update();
}

void WaveCanvas::commitTransition(const QPoint& releasePosition)
{
    drawing_ = false;
    if (!scenario_ || !commandStack_ || activeEventId_.empty()) return;
    const auto* existing = findEvent(*scenario_, activeEventId_);
    if (!existing) return;
    auto replacement = *existing;
    const auto* lane = findLane(*scenario_, existing->laneId);
    replacement.tick = snappedTick(tickAtX(releasePosition.x()), lane);
    if (replacement.tick == existing->tick) {
        activeEventId_.clear();
        viewport()->update();
        return;
    }
    try {
        commandStack_->execute(std::make_unique<ChangeEventCommand>(
            *scenario_,
            existing->id,
            std::move(replacement)));
    } catch (const std::exception& exception) {
        QToolTip::showText(
            viewport()->mapToGlobal(releasePosition),
            QString::fromUtf8(exception.what()),
            viewport());
        viewport()->update();
        return;
    }
    activeEventId_.clear();
    emit modelEdited();
    emit commandAvailabilityChanged();
    viewport()->update();
}

const Event* WaveCanvas::eventAtPosition(const QPoint& position) const
{
    if (!scenario_) return nullptr;
    const auto iterator = std::find_if(
        eventHitRegions_.begin(),
        eventHitRegions_.end(),
        [&position](const EventHitRegion& region) {
            return region.rect.adjusted(-3, -3, 3, 3).contains(position);
        });
    return iterator == eventHitRegions_.end()
        ? nullptr
        : findEvent(*scenario_, iterator->eventId);
}

QPoint WaveCanvas::eventPoint(const Event& event) const
{
    if (!scenario_) return {};
    const auto layout = std::find_if(
        laneLayout_.begin(),
        laneLayout_.end(),
        [this, &event](const LaneLayout& candidate) {
            return scenario_->lanes.at(candidate.laneIndex).id == event.laneId;
        });
    if (layout == laneLayout_.end()) return {};
    return {
        xAtTick(event.tick),
        RulerHeight + layout->top - verticalScrollBar()->value() + layout->height / 2,
    };
}

void WaveCanvas::drawRuler(QPainter& painter)
{
    painter.fillRect(QRect(HeaderWidth, 0, waveViewportWidth(), RulerHeight), kRulerBackground);
    painter.setClipRect(QRect(HeaderWidth, 0, waveViewportWidth(), viewport()->height()));
    const auto [visibleStart, visibleEnd] = visibleTickRange();
    const auto major = majorTickStep();
    const auto minor = std::max<Tick>(1, major / 5);
    auto first = floorToStep(visibleStart, minor);

    QFont labelFont = painter.font();
    labelFont.setPointSizeF(std::max(7.5, labelFont.pointSizeF() - 1.0));
    painter.setFont(labelFont);
    for (auto tick = first; tick <= visibleEnd;) {
        const auto x = xAtTick(tick);
        const auto isMajor = tick % major == 0;
        painter.setPen(isMajor ? kGridMajor : kGridMinor);
        painter.drawLine(
            x,
            isMajor ? RulerHeight - 13 : RulerHeight - 7,
            x,
            viewport()->height());
        if (isMajor && tick >= 0) {
            painter.setPen(kTextSecondary);
            painter.drawText(
                QRect(x + 5, 2, 130, RulerHeight - 12),
                Qt::AlignLeft | Qt::AlignVCenter,
                QString::fromStdString(formatTick(tick, project_->timeBase)));
        }
        if (tick > std::numeric_limits<Tick>::max() - minor) break;
        tick += minor;
    }
    painter.setClipping(false);
    painter.setPen(kGridMajor);
    painter.drawLine(HeaderWidth, RulerHeight - 1, viewport()->width(), RulerHeight - 1);
}

void WaveCanvas::drawLane(
    QPainter& painter,
    const Lane& lane,
    const LaneLayout& layout,
    const Tick visibleStart,
    const Tick visibleEnd)
{
    const auto y = RulerHeight + layout.top - verticalScrollBar()->value();
    const QRect rowRect(0, y, viewport()->width(), layout.height);
    const QRect waveformRect(HeaderWidth, y, waveViewportWidth(), layout.height);

    const auto selected = std::find(
        selectedLaneIds_.begin(),
        selectedLaneIds_.end(),
        lane.id) != selectedLaneIds_.end();
    painter.fillRect(
        QRect(0, y, HeaderWidth, layout.height),
        selected ? QColor(46, 60, 81) : kHeaderBackground);
    if (selected) painter.fillRect(waveformRect, QColor(34, 43, 57));
    painter.setPen(kTextPrimary);
    QFont nameFont = painter.font();
    nameFont.setBold(selected);
    painter.setFont(nameFont);
    const auto sampledValue = tool_ == Tool::Marker
        ? cursorValue(lane)
        : QString{};
    const auto valueWidth = sampledValue.isEmpty() ? 0 : 78;
    const QRect nameRect(
        14,
        y + 4,
        HeaderWidth - 28 - valueWidth,
        layout.height / 2);
    painter.drawText(
        nameRect,
        Qt::AlignLeft | Qt::AlignVCenter,
        painter.fontMetrics().elidedText(
            QString::fromStdString(lane.name),
            Qt::ElideRight,
            nameRect.width()));
    if (!sampledValue.isEmpty()) {
        painter.setPen(kMovableCursor);
        nameFont.setBold(true);
        painter.setFont(nameFont);
        const QRect valueRect(
            HeaderWidth - 88,
            y + 4,
            74,
            layout.height / 2);
        painter.drawText(
            valueRect,
            Qt::AlignRight | Qt::AlignVCenter,
            painter.fontMetrics().elidedText(
                sampledValue,
                Qt::ElideLeft,
                valueRect.width()));
    }
    QFont detailFont = painter.font();
    detailFont.setBold(false);
    detailFont.setPointSizeF(std::max(7.0, detailFont.pointSizeF() - 1.5));
    painter.setFont(detailFont);
    painter.setPen(kTextSecondary);
    auto detail = laneKindLabel(lane.kind);
    if (lane.kind == LaneKind::Bus || lane.kind == LaneKind::Enum) {
        detail += tr(" · %1-bit").arg(lane.width);
    }
    painter.drawText(
        QRect(14, y + layout.height / 2 - 2, HeaderWidth - 28, layout.height / 2),
        Qt::AlignLeft | Qt::AlignVCenter,
        detail);

    painter.save();
    painter.setClipRect(waveformRect.adjusted(0, 1, 0, -1));
    switch (lane.kind) {
    case LaneKind::Clock:
        drawClock(painter, lane, waveformRect, visibleStart, visibleEnd);
        break;
    case LaneKind::Bit:
        drawBitSegments(painter, lane, waveformRect, visibleStart, visibleEnd);
        break;
    case LaneKind::Bus:
    case LaneKind::Enum:
    case LaneKind::Transaction:
    case LaneKind::Event:
        drawBusSegments(painter, lane, waveformRect, visibleStart, visibleEnd);
        break;
    case LaneKind::Group:
        break;
    }
    painter.restore();
    painter.setPen(kGridMinor);
    painter.drawLine(0, rowRect.bottom(), viewport()->width(), rowRect.bottom());
}

void WaveCanvas::drawAddLaneRow(QPainter& painter)
{
    const auto row = addLaneRowRect();
    if (row.isEmpty()
        || row.bottom() < RulerHeight
        || row.top() >= viewport()->height()) {
        return;
    }

    painter.fillRect(row, QColor(21, 27, 36));
    painter.fillRect(
        QRect(0, row.top(), HeaderWidth, row.height()),
        kHeaderBackground);
    painter.setPen(QPen(kGridMinor, 1.0, Qt::DashLine));
    painter.drawLine(
        HeaderWidth + 14,
        row.center().y(),
        viewport()->width() - 14,
        row.center().y());
    painter.setPen(kGridMinor);
    painter.drawLine(0, row.top(), viewport()->width(), row.top());
    painter.drawLine(0, row.bottom(), viewport()->width(), row.bottom());
}

void WaveCanvas::drawClock(
    QPainter& painter,
    const Lane& lane,
    const QRect& rect,
    const Tick visibleStart,
    const Tick visibleEnd)
{
    const auto* clock = project_ ? findClock(*project_, lane.clockDomainId) : nullptr;
    if (!clock || !clock->isValid()) {
        painter.setPen(kUndefined);
        painter.drawText(rect.adjusted(12, 0, -8, 0), Qt::AlignVCenter, tr("Unresolved clock domain"));
        return;
    }

    const auto highY = rect.top() + 12;
    const auto lowY = rect.bottom() - 12;
    QPen pen(laneColor(lane), 2.0);
    painter.setPen(pen);

    const auto drawOverrides = [&] {
        auto iterator = std::lower_bound(
            lane.segments.begin(),
            lane.segments.end(),
            visibleStart,
            [](const Segment& segment, const Tick tick) {
                return segment.end <= tick;
            });
        for (; iterator != lane.segments.end() && iterator->start < visibleEnd; ++iterator) {
            const auto mode = clockOverrideModeFromString(iterator->value);
            if (!mode) continue;
            const auto left = xAtTick(std::max(iterator->start, visibleStart));
            const auto right = xAtTick(std::min(iterator->end, visibleEnd));
            if (right <= left) continue;
            const auto gated = *mode == ClockOverrideMode::Gated;
            painter.fillRect(
                QRect(left, rect.top() + 1, std::max(1, right - left), rect.height() - 2),
                gated ? QColor(61, 49, 25) : QColor(68, 31, 40));
            painter.setPen(QPen(
                gated ? QColor(255, 183, 77) : kUndefined,
                2.0,
                gated ? Qt::SolidLine : Qt::DashLine));
            const auto y = gated ? lowY : (highY + lowY) / 2;
            painter.drawLine(left, y, right, y);
            painter.setPen(gated ? QColor(255, 202, 40) : QColor(255, 138, 128));
            const auto fullLabel = gated ? tr("GATED") : tr("DISABLED · X");
            const auto compactLabel = gated ? tr("G") : tr("X");
            const auto availableWidth = right - left - 8;
            const auto label = painter.fontMetrics().horizontalAdvance(fullLabel)
                    <= availableWidth
                ? fullLabel
                : compactLabel;
            if (painter.fontMetrics().horizontalAdvance(label) <= availableWidth) {
                painter.drawText(
                    QRect(left + 4, rect.top(), right - left - 8, rect.height()),
                    Qt::AlignCenter,
                    label);
            }
            painter.setPen(QPen(
                gated ? QColor(255, 183, 77) : kUndefined,
                1.0,
                Qt::DashLine));
            painter.drawLine(left, rect.top() + 2, left, rect.bottom() - 2);
            painter.drawLine(right, rect.top() + 2, right, rect.bottom() - 2);
        }
    };

    if (static_cast<double>(clock->period) * pixelsPerTick_ < 2.0) {
        painter.setPen(QPen(laneColor(lane), 1.0));
        painter.drawLine(rect.left(), (highY + lowY) / 2, rect.right(), (highY + lowY) / 2);
        painter.setPen(kTextSecondary);
        painter.drawText(rect.adjusted(8, 0, -8, 0), Qt::AlignVCenter, tr("clock density"));
        drawOverrides();
        return;
    }

    auto cycle = (visibleStart - clock->phase) / clock->period - 1;
    if (visibleStart < clock->phase && (visibleStart - clock->phase) % clock->period != 0) --cycle;
    QPainterPath path;
    bool started = false;
    for (std::size_t count = 0; count < 100'000; ++count, ++cycle) {
        const auto rising = tickAtCycle(*clock, cycle, ClockEdge::Rising);
        const auto falling = tickAtCycle(*clock, cycle, ClockEdge::Falling);
        const auto nextRising = tickAtCycle(*clock, cycle + 1, ClockEdge::Rising);
        if (!rising || !falling || !nextRising) break;
        if (*rising > visibleEnd) break;
        if (*nextRising < visibleStart) continue;
        const auto risingX = xAtTick(*rising);
        const auto fallingX = xAtTick(*falling);
        const auto nextX = xAtTick(*nextRising);
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
}

void WaveCanvas::drawBitSegments(
    QPainter& painter,
    const Lane& lane,
    const QRect& rect,
    const Tick visibleStart,
    const Tick visibleEnd)
{
    const auto highY = rect.top() + 12;
    const auto lowY = rect.bottom() - 12;
    auto iterator = std::lower_bound(
        lane.segments.begin(),
        lane.segments.end(),
        visibleStart,
        [](const Segment& segment, const Tick tick) { return segment.end <= tick; });
    painter.setPen(QPen(laneColor(lane), 2.0));
    int previousY = lowY;
    bool havePrevious = false;
    for (; iterator != lane.segments.end() && iterator->start < visibleEnd; ++iterator) {
        const auto left = xAtTick(iterator->start);
        const auto right = xAtTick(iterator->end);
        const auto value = iterator->value.empty() ? 'X' : iterator->value.front();
        if (value == 'X' || value == 'Z') {
            painter.save();
            QColor fill = laneColor(lane);
            fill.setAlpha(44);
            painter.fillRect(QRect(left, highY, std::max(1, right - left), lowY - highY), fill);
            painter.setPen(QPen(laneColor(lane), 1.0, value == 'Z' ? Qt::DashLine : Qt::SolidLine));
            painter.drawLine(left, (highY + lowY) / 2, right, (highY + lowY) / 2);
            painter.drawText(
                QRect(left + 4, highY, std::max(0, right - left - 8), lowY - highY),
                Qt::AlignCenter,
                QString(value));
            painter.restore();
            havePrevious = false;
            continue;
        }
        const auto y = value == '1' ? highY : lowY;
        if (havePrevious && left >= rect.left() && previousY != y) {
            painter.drawLine(left, previousY, left, y);
        }
        painter.drawLine(left, y, right, y);
        previousY = y;
        havePrevious = true;
    }
}

void WaveCanvas::drawBusSegments(
    QPainter& painter,
    const Lane& lane,
    const QRect& rect,
    const Tick visibleStart,
    const Tick visibleEnd)
{
    const auto top = rect.top() + 10;
    const auto bottom = rect.bottom() - 10;
    const auto middle = (top + bottom) / 2;
    auto iterator = std::lower_bound(
        lane.segments.begin(),
        lane.segments.end(),
        visibleStart,
        [](const Segment& segment, const Tick tick) { return segment.end <= tick; });
    painter.setPen(QPen(laneColor(lane), 1.5));
    for (; iterator != lane.segments.end() && iterator->start < visibleEnd; ++iterator) {
        const auto left = xAtTick(iterator->start);
        const auto right = xAtTick(iterator->end);
        const auto bevel = std::min(7, std::max(0, (right - left) / 3));
        QPainterPath path;
        path.moveTo(left, middle);
        path.lineTo(left + bevel, top);
        path.lineTo(right - bevel, top);
        path.lineTo(right, middle);
        path.lineTo(right - bevel, bottom);
        path.lineTo(left + bevel, bottom);
        path.closeSubpath();
        QColor fill = laneColor(lane);
        fill.setAlpha(35);
        painter.fillPath(path, fill);
        painter.drawPath(path);
        if (right - left > 28) {
            painter.setPen(kTextPrimary);
            painter.drawText(
                QRect(left + bevel + 4, top, right - left - 2 * bevel - 8, bottom - top),
                Qt::AlignCenter,
                QString::fromStdString(iterator->value));
            painter.setPen(QPen(laneColor(lane), 1.5));
        }
    }
}

void WaveCanvas::drawScenarioOverlays(
    QPainter& painter,
    const Tick visibleStart,
    const Tick visibleEnd)
{
    if (!scenario_) return;
    eventHitRegions_.clear();
    painter.save();
    painter.setClipRect(QRect(HeaderWidth, RulerHeight, waveViewportWidth(), viewport()->height() - RulerHeight));

    for (const auto& marker : scenario_->markers) {
        const auto [start, end] = markerDisplayRange(marker);
        if (end < visibleStart || start > visibleEnd) continue;
        const auto left = xAtTick(start);
        const auto right = xAtTick(end);
        const auto selected = tool_ == Tool::Marker
            && marker.id == selectedMarkerId_;
        QColor color = selected
            ? kSelectedLockedCursor
            : marker.kind == MarkerKind::Error
            ? QColor(239, 83, 80)
            : kLockedCursor;
        if (start != end) {
            auto fill = color;
            fill.setAlpha(selected ? 42 : 24);
            painter.fillRect(
                QRect(
                    QPoint(left, RulerHeight),
                    QPoint(std::max(left + 1, right), viewport()->height())),
                fill);
        }
        painter.setPen(QPen(
            color,
            selected || marker.kind == MarkerKind::Error ? 2.0 : 1.25,
            selected ? Qt::SolidLine : Qt::DashLine));
        painter.drawLine(left, RulerHeight, left, viewport()->height());
        if (start != end) {
            painter.drawLine(right, RulerHeight, right, viewport()->height());
        }
        if (selected) {
            painter.setBrush(color);
            painter.drawPolygon(QPolygon{
                QPoint(left - 5, RulerHeight),
                QPoint(left + 5, RulerHeight),
                QPoint(left, RulerHeight + 7),
            });
        }
        painter.setPen(color);
        auto label = QString::fromStdString(marker.name);
        if (start != end) {
            label += QStringLiteral("  ") + cursorDeltaText(start, end);
        }
        painter.drawText(
            QRect(left + 5, RulerHeight + 2, 190, 18),
            Qt::AlignLeft | Qt::AlignVCenter,
            label);
    }

    painter.setRenderHint(QPainter::Antialiasing, true);

    for (const auto& relation : scenario_->relations) {
        const auto* source = findEvent(*scenario_, relation.sourceEventId);
        const auto* target = findEvent(*scenario_, relation.targetEventId);
        if (!source || !target) continue;
        if ((source->tick < visibleStart && target->tick < visibleStart)
            || (source->tick > visibleEnd && target->tick > visibleEnd)) {
            continue;
        }
        const auto sourcePoint = eventPoint(*source);
        const auto targetPoint = eventPoint(*target);
        if (sourcePoint.isNull() || targetPoint.isNull()) continue;
        const QColor color = relation.severity == Severity::Error
            ? QColor(239, 108, 115)
            : relation.severity == Severity::Warning
                ? QColor(255, 183, 77)
                : QColor(100, 181, 246);
        painter.setPen(QPen(color, 1.5));
        painter.drawLine(sourcePoint, targetPoint);
        const auto angle = std::atan2(
            static_cast<double>(targetPoint.y() - sourcePoint.y()),
            static_cast<double>(targetPoint.x() - sourcePoint.x()));
        constexpr double arrowLength = 9.0;
        constexpr double arrowSpread = 0.55;
        const QPointF leftWing(
            targetPoint.x() - arrowLength * std::cos(angle - arrowSpread),
            targetPoint.y() - arrowLength * std::sin(angle - arrowSpread));
        const QPointF rightWing(
            targetPoint.x() - arrowLength * std::cos(angle + arrowSpread),
            targetPoint.y() - arrowLength * std::sin(angle + arrowSpread));
        painter.setBrush(color);
        painter.drawPolygon(QPolygonF{QPointF(targetPoint), leftWing, rightWing});
    }

    for (const auto& event : scenario_->events) {
        if (event.tick < visibleStart || event.tick > visibleEnd || event.laneId.empty()) {
            continue;
        }
        const auto point = eventPoint(event);
        if (point.isNull()) continue;
        const QColor color = event.action == EventAction::Expect
            ? QColor(255, 183, 77)
            : QColor(126, 200, 255);
        const QPolygon marker{
            QPoint(point.x(), point.y() - 6),
            QPoint(point.x() + 6, point.y()),
            QPoint(point.x(), point.y() + 6),
            QPoint(point.x() - 6, point.y()),
        };
        painter.setPen(QPen(kBackground, 1.0));
        painter.setBrush(color);
        painter.drawPolygon(marker);
        eventHitRegions_.push_back({
            event.id,
            marker.boundingRect().adjusted(-2, -2, 2, 2),
        });
    }
    drawCursorOverlays(painter, visibleStart, visibleEnd);

    painter.restore();
}

void WaveCanvas::drawCursorOverlays(
    QPainter& painter,
    const Tick visibleStart,
    const Tick visibleEnd)
{
    if (tool_ != Tool::Marker || !movableCursorTick_) return;

    painter.save();
    const auto activeTick = *movableCursorTick_;
    const auto activeX = xAtTick(activeTick);
    const auto drawTag = [&](const int anchorX,
                             const int top,
                             const QString& text,
                             const QColor& color,
                             const bool centered) {
        const auto width = std::clamp(
            painter.fontMetrics().horizontalAdvance(text) + 14,
            52,
            190);
        const auto proposedLeft = centered
            ? anchorX - width / 2
            : anchorX + 6;
        const auto left = std::clamp(
            proposedLeft,
            HeaderWidth + 3,
            std::max(HeaderWidth + 3, viewport()->width() - width - 3));
        const QRect rect(left, top, width, 20);
        painter.setPen(QPen(color, 1.0));
        painter.setBrush(QColor(18, 22, 29, 232));
        painter.drawRoundedRect(rect, 4, 4);
        painter.drawText(
            rect.adjusted(6, 0, -6, 0),
            Qt::AlignCenter,
            painter.fontMetrics().elidedText(
                text,
                Qt::ElideRight,
                rect.width() - 12));
    };

    if (temporaryCursorTick_) {
        const auto temporaryTick = *temporaryCursorTick_;
        const auto temporaryX = xAtTick(temporaryTick);
        const auto left = std::min(activeX, temporaryX);
        const auto right = std::max(activeX, temporaryX);
        auto fill = kTemporaryCursor;
        fill.setAlpha(22);
        painter.fillRect(
            QRect(
                QPoint(left, RulerHeight),
                QPoint(std::max(left + 1, right), viewport()->height())),
            fill);

        painter.setPen(QPen(kTemporaryCursor, 1.5, Qt::DashLine));
        painter.drawLine(
            temporaryX,
            RulerHeight,
            temporaryX,
            viewport()->height());
        painter.setBrush(kTemporaryCursor);
        painter.drawPolygon(QPolygon{
            QPoint(temporaryX - 5, RulerHeight),
            QPoint(temporaryX + 5, RulerHeight),
            QPoint(temporaryX, RulerHeight + 7),
        });

        const auto measureY = RulerHeight + 28;
        painter.setPen(QPen(kTemporaryCursor, 1.25));
        painter.drawLine(temporaryX, measureY, activeX, measureY);
        painter.drawLine(temporaryX, measureY - 4, temporaryX, measureY + 4);
        painter.drawLine(activeX, measureY - 4, activeX, measureY + 4);
        drawTag(
            (temporaryX + activeX) / 2,
            RulerHeight + 34,
            cursorDeltaText(temporaryTick, activeTick),
            kTemporaryCursor,
            true);
    }

    if (activeTick >= visibleStart && activeTick <= visibleEnd) {
        painter.setPen(QPen(kMovableCursor, 2.0, Qt::SolidLine));
        painter.drawLine(activeX, RulerHeight, activeX, viewport()->height());
        painter.setBrush(kMovableCursor);
        painter.drawPolygon(QPolygon{
            QPoint(activeX - 6, RulerHeight),
            QPoint(activeX + 6, RulerHeight),
            QPoint(activeX, RulerHeight + 8),
        });
        drawTag(
            activeX,
            RulerHeight + 3,
            tr("Cursor %1").arg(QString::fromStdString(
                formatTick(activeTick, project_->timeBase))),
            kMovableCursor,
            false);
    }
    painter.restore();
}

} // namespace wave
