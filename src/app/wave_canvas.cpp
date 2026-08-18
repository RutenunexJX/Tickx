#include "wave_canvas.h"

#include "wave/timeline_viewport.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCompleter>
#include <QContextMenuEvent>
#include <QEvent>
#include <QFrame>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QRegularExpression>
#include <QScrollBar>
#include <QShowEvent>
#include <QStyle>
#include <QStringListModel>
#include <QToolButton>
#include <QToolTip>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace wave {
namespace {

const QColor kBackground(32, 39, 49);
const QColor kHeaderBackground(42, 51, 65);
const QColor kRulerBackground(37, 45, 57);
const QColor kGridMajor(91, 105, 124);
const QColor kGridMinor(57, 68, 84);
const QColor kTextPrimary(235, 239, 245);
const QColor kTextSecondary(177, 187, 202);
const QColor kSelection(68, 138, 255, 62);
const QColor kUndefined(239, 83, 80);
const QColor kMovableCursor(79, 195, 247);
const QColor kTemporaryCursor(186, 104, 200);
const QColor kLockedCursor(255, 202, 40);
const QColor kSelectedLockedCursor(102, 187, 106);
const QString kRangeMimeType = QStringLiteral("application/x-wave-workbench-range+json");
constexpr std::string_view kBusPresetExtension = "waveWorkbench.busPreset";
constexpr int kSoftSnapRadiusPixels = 7;

std::string busPresetValue(const std::string_view presetId, const std::uint32_t width)
{
    const auto count = static_cast<std::size_t>(std::max<std::uint32_t>(1, width));
    if (presetId == "zero" || presetId == "reserved") return "0b" + std::string(count, '0');
    if (presetId == "dont-care") return "0B" + std::string(count, 'X');
    if (presetId == "x") return "0b" + std::string(count, 'x');
    if (presetId == "z") return "0b" + std::string(count, 'z');
    return {};
}

QString busPresetDisplayLabel(const std::string_view presetId)
{
    if (presetId == "zero") return QStringLiteral("0");
    if (presetId == "reserved") return QStringLiteral("RESERVED");
    if (presetId == "dont-care") return QStringLiteral("DON'T CARE");
    if (presetId == "x") return QStringLiteral("X");
    if (presetId == "z") return QStringLiteral("Z");
    return QStringLiteral("BUS VALUE");
}

std::string busPresetId(const Segment& segment)
{
    const auto preset = segment.extensions.find(std::string(kBusPresetExtension));
    if (preset == segment.extensions.end()) return {};
    auto value = preset->second;
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
    }
    return value;
}


QString appendRelationAwareUndo(
    QString message,
    const std::size_t relationCountBefore,
    const std::size_t relationCountAfter)
{
    const auto removedRelationCount = relationCountBefore
        - std::min(relationCountBefore, relationCountAfter);
    if (removedRelationCount > 0) {
        message += QObject::tr(
            " · removed %1 relation(s) because referenced edges disappeared"
            " · Ctrl+Z restores waveform and relations")
                       .arg(static_cast<qulonglong>(removedRelationCount));
    } else {
        message += QObject::tr(" · Ctrl+Z to undo");
    }
    return message;
}

std::vector<CopiedLaneRange> captureLaneRanges(
    const Scenario& scenario,
    const std::vector<std::string>& laneIds,
    const Tick start,
    const Tick end,
    const bool preserveSegmentIds)
{
    std::vector<CopiedLaneRange> copiedLanes;
    if (start < 0 || end <= start || end > scenario.duration) return copiedLanes;
    copiedLanes.reserve(laneIds.size());
    for (const auto& laneId : laneIds) {
        const auto* lane = findLane(scenario, laneId);
        if (!lane || !lane->visible || lane->kind == LaneKind::Group) continue;
        CopiedLaneRange copied;
        copied.laneId = lane->id;
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
            auto relative = *iterator;
            if (!preserveSegmentIds
                || iterator->start < start
                || iterator->end > end) {
                relative.id.clear();
            }
            relative.start = clippedStart - start;
            relative.end = clippedEnd - start;
            copied.relativeSegments.push_back(std::move(relative));
        }
        copiedLanes.push_back(std::move(copied));
    }
    return copiedLanes;
}

bool sameProjectedWaveform(const Lane& left, const Lane& right)
{
    return left.segments.size() == right.segments.size()
        && std::equal(
            left.segments.begin(),
            left.segments.end(),
            right.segments.begin(),
            [](const Segment& leftSegment,
               const Segment& rightSegment) {
                return leftSegment.start == rightSegment.start
                    && leftSegment.end == rightSegment.end
                    && leftSegment.value == rightSegment.value
                    && leftSegment.extensions
                        == rightSegment.extensions;
            });
}

bool projectedRangeAlreadyEquals(
    const Lane& lane,
    const Tick start,
    const Tick end,
    const std::string_view value,
    const JsonExtensions& extensions)
{
    auto coveredUntil = start;
    for (const auto& segment : lane.segments) {
        if (segment.end <= coveredUntil) continue;
        if (segment.start > coveredUntil
            || segment.value != value
            || segment.extensions != extensions) {
            return false;
        }
        coveredUntil = std::min(end, segment.end);
        if (coveredUntil >= end) return true;
    }
    return false;
}

void normalizeProjectedSegments(Lane& lane)
{
    std::sort(
        lane.segments.begin(),
        lane.segments.end(),
        [](const Segment& left, const Segment& right) {
            return left.start == right.start
                ? left.end < right.end
                : left.start < right.start;
        });
    std::vector<Segment> normalized;
    normalized.reserve(lane.segments.size());
    for (auto& candidate : lane.segments) {
        if (!normalized.empty()
            && normalized.back().end == candidate.start
            && normalized.back().value == candidate.value
            && normalized.back().extensions == candidate.extensions) {
            normalized.back().end = candidate.end;
        } else {
            normalized.push_back(std::move(candidate));
        }
    }
    lane.segments = std::move(normalized);
}

template<typename NextSegmentId>
void clearProjectedSegmentRange(
    Lane& lane,
    const Tick start,
    const Tick end,
    NextSegmentId& nextSegmentId)
{
    std::vector<Segment> retained;
    retained.reserve(lane.segments.size() + 1);
    for (const auto& existing : lane.segments) {
        if (existing.end <= start || existing.start >= end) {
            retained.push_back(existing);
            continue;
        }
        if (existing.start < start) {
            auto left = existing;
            left.end = start;
            retained.push_back(std::move(left));
        }
        if (existing.end > end) {
            auto right = existing;
            right.id = nextSegmentId();
            right.start = end;
            retained.push_back(std::move(right));
        }
    }
    lane.segments = std::move(retained);
    normalizeProjectedSegments(lane);
}

template<typename NextSegmentId>
void setProjectedSegmentRange(
    Lane& lane,
    const Tick start,
    const Tick end,
    std::string value,
    std::string stableId,
    JsonExtensions extensions,
    NextSegmentId& nextSegmentId)
{
    std::vector<Segment> replaced;
    replaced.reserve(lane.segments.size() + 2);
    for (const auto& existing : lane.segments) {
        if (existing.end <= start || existing.start >= end) {
            replaced.push_back(existing);
            continue;
        }
        if (existing.start < start) {
            auto left = existing;
            left.end = start;
            replaced.push_back(std::move(left));
        }
        if (existing.end > end) {
            auto right = existing;
            right.id = nextSegmentId();
            right.start = end;
            replaced.push_back(std::move(right));
        }
    }
    if (stableId.empty()) stableId = nextSegmentId();
    replaced.push_back({
        std::move(stableId),
        start,
        end,
        std::move(value),
        std::move(extensions),
    });
    lane.segments = std::move(replaced);
    normalizeProjectedSegments(lane);
}

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
    setObjectName(QStringLiteral("StimulusCanvas"));
    setFrameShape(QFrame::NoFrame);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    viewport()->setAutoFillBackground(false);
    laneDragAutoScrollTimer_ = new QTimer(this);
    laneDragAutoScrollTimer_->setInterval(LaneDragAutoScrollIntervalMs);
    connect(
        laneDragAutoScrollTimer_,
        &QTimer::timeout,
        this,
        &WaveCanvas::advanceLaneDragAutoScroll);
    waveEditDragAutoScrollTimer_ = new QTimer(this);
    waveEditDragAutoScrollTimer_->setInterval(WaveEditDragAutoScrollIntervalMs);
    connect(
        waveEditDragAutoScrollTimer_,
        &QTimer::timeout,
        this,
        &WaveCanvas::advanceWaveEditDragAutoScroll);
    const std::array<LaneKind, 3> quickKinds{
        LaneKind::Clock,
        LaneKind::Bit,
        LaneKind::Bus,
    };
    const std::array<QString, 3> quickLabels{
        tr("+ CLK"),
        tr("+ BIT"),
        tr("+ BUS"),
    };
    const std::array<QString, 3> quickObjectNames{
        QStringLiteral("CanvasAddClockButton"),
        QStringLiteral("CanvasAddBitButton"),
        QStringLiteral("CanvasAddBusButton"),
    };
    for (std::size_t index = 0; index < addLaneButtons_.size(); ++index) {
        auto* button = new QToolButton(viewport());
        button->setObjectName(quickObjectNames.at(index));
        button->setText(quickLabels.at(index));
        button->setToolTip(tr("Add a %1 lane immediately").arg(
            laneKindLabel(quickKinds.at(index))));
        button->setAccessibleName(quickLabels.at(index));
        button->setAutoRaise(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet(QStringLiteral(
            "QToolButton {"
            " color: #edf1f7; background: #344052;"
            " border: 1px solid #59677c; border-radius: 5px;"
            " font-weight: 600;"
            "}"
            "QToolButton:hover, QToolButton:focus {"
            " background: #43536a; border-color: #7eb5ff;"
            "}"
            "QToolButton:pressed { background: #315f91; }"));
        button->hide();
        addLaneButtons_.at(index) = button;
        connect(button, &QToolButton::clicked, this, [this, kind = quickKinds.at(index)] {
            emit addLaneRequested(kind);
        });
    }

    showHiddenLanesButton_ = new QToolButton(viewport());
    showHiddenLanesButton_->setObjectName(QStringLiteral("CanvasShowHiddenLanesButton"));
    hiddenLanesMenu_ = new QMenu(showHiddenLanesButton_);
    hiddenLanesMenu_->setObjectName(QStringLiteral("HiddenLanesMenu"));
    hiddenLanesMenu_->setTitle(tr("Restore one hidden item"));
    showHiddenLanesButton_->setText(tr("Show hidden items"));
    showHiddenLanesButton_->setToolTip(
        tr("Restore every hidden signal or group as one undoable edit"));
    showHiddenLanesButton_->setAccessibleName(tr("Show hidden items"));
    showHiddenLanesButton_->setCursor(Qt::PointingHandCursor);
    showHiddenLanesButton_->setStyleSheet(QStringLiteral(
        "QToolButton {"
        " color: #e7f1ff; background: #2f4d69;"
        " border: 1px solid #6e9cc5; border-radius: 5px;"
        " font-weight: 600; padding: 3px 10px;"
        "}"
        "QToolButton:hover, QToolButton:focus {"
        " background: #3c6285; border-color: #9acbfa;"
        "}"
        "QToolButton:pressed { background: #29445d; }"));
    showHiddenLanesButton_->hide();
    connect(showHiddenLanesButton_, &QToolButton::clicked, this, [this] {
        emit showHiddenLanesRequested();
    });

    quickLaneSetupPanel_ = new QFrame(viewport());
    quickLaneSetupPanel_->setObjectName(QStringLiteral("QuickLaneSetupPanel"));
    quickLaneSetupPanel_->setAttribute(Qt::WA_StyledBackground, true);
    quickLaneSetupPanel_->setStyleSheet(QStringLiteral(
        "QFrame#QuickLaneSetupPanel {"
        " background: rgba(45, 56, 72, 248);"
        " border: 1px solid #7eb5ff; border-radius: 6px;"
        "}"
        "QFrame#QuickLaneSetupPanel QLabel { color: #e9eff7; }"
        "QFrame#QuickLaneSetupPanel QLineEdit,"
        "QFrame#QuickLaneSetupPanel QComboBox {"
        " color: #f4f7fb; background: #222b38;"
        " border: 1px solid #65768e; border-radius: 4px; padding: 3px 6px;"
        "}"
        "QFrame#QuickLaneSetupPanel QLineEdit:focus,"
        "QFrame#QuickLaneSetupPanel QComboBox:focus { border-color: #8fc3ff; }"));
    auto* quickLayout = new QHBoxLayout(quickLaneSetupPanel_);
    quickLayout->setContentsMargins(8, 5, 8, 5);
    quickLayout->setSpacing(6);
    auto* quickPrompt = new QLabel(tr("New signal"), quickLaneSetupPanel_);
    quickPrompt->setObjectName(QStringLiteral("QuickLaneSetupPrompt"));
    quickLayout->addWidget(quickPrompt);
    quickLaneNameEdit_ = new QLineEdit(quickLaneSetupPanel_);
    quickLaneNameEdit_->setObjectName(QStringLiteral("QuickLaneNameEdit"));
    quickLaneNameEdit_->setPlaceholderText(tr("Signal name"));
    quickLaneNameEdit_->setMinimumWidth(130);
    quickLayout->addWidget(quickLaneNameEdit_, 1);
    quickLaneParameterEdit_ = new QLineEdit(quickLaneSetupPanel_);
    quickLaneParameterEdit_->setObjectName(QStringLiteral("QuickLaneParameterEdit"));
    quickLaneParameterEdit_->setMaximumWidth(120);
    quickLayout->addWidget(quickLaneParameterEdit_);
    quickLaneClockCombo_ = new QComboBox(quickLaneSetupPanel_);
    quickLaneClockCombo_->setObjectName(QStringLiteral("QuickLaneClockCombo"));
    quickLaneClockCombo_->setMinimumWidth(120);
    quickLayout->addWidget(quickLaneClockCombo_);
    quickLaneErrorLabel_ = new QLabel(quickLaneSetupPanel_);
    quickLaneErrorLabel_->setObjectName(QStringLiteral("QuickLaneSetupError"));
    quickLaneErrorLabel_->setStyleSheet(QStringLiteral("color: #ff9b98;"));
    quickLaneErrorLabel_->setMaximumWidth(230);
    quickLayout->addWidget(quickLaneErrorLabel_);
    auto* quickHint = new QLabel(
        tr("Enter or click the next target to apply · Esc to cancel"),
        quickLaneSetupPanel_);
    quickHint->setObjectName(QStringLiteral("QuickLaneSetupHint"));
    quickHint->setStyleSheet(QStringLiteral("color: #aebbd0;"));
    quickLayout->addWidget(quickHint);
    for (auto* editor : {quickLaneNameEdit_, quickLaneParameterEdit_}) {
        editor->installEventFilter(this);
    }
    quickLaneClockCombo_->installEventFilter(this);
    quickLaneSetupPanel_->hide();

    laneRenameEdit_ = new QLineEdit(viewport());
    laneRenameEdit_->setObjectName(QStringLiteral("LaneRenameEdit"));
    laneRenameEdit_->setAccessibleName(tr("Signal name"));
    laneRenameEdit_->setPlaceholderText(tr("Signal name"));
    laneRenameEdit_->setToolTip(tr("Enter or click elsewhere to apply · Esc to cancel"));
    laneRenameEdit_->setStyleSheet(QStringLiteral(
        "QLineEdit { color: #f4f7fb; background: #222b38;"
        " border: 1px solid #8fc3ff; border-radius: 4px; padding: 3px 6px;"
        " font-weight: 600; }"));
    laneRenameEdit_->installEventFilter(this);
    connect(laneRenameEdit_, &QLineEdit::textEdited, this, [this] {
        laneRenameEdit_->setStyleSheet(QStringLiteral(
            "QLineEdit { color: #f4f7fb; background: #222b38;"
            " border: 1px solid #8fc3ff; border-radius: 4px; padding: 3px 6px;"
            " font-weight: 600; }"));
        laneRenameEdit_->setToolTip(tr("Enter or click elsewhere to apply · Esc to cancel"));
    });
    laneRenameEdit_->hide();

    durationLabel_ = new QLabel(tr("End"), viewport());
    durationLabel_->setObjectName(QStringLiteral("TimelineDurationLabel"));
    durationLabel_->setStyleSheet(QStringLiteral("color: #b9c6d8; background: transparent;"));
    durationEdit_ = new QLineEdit(viewport());
    durationEdit_->setObjectName(QStringLiteral("TimelineDurationEdit"));
    durationEdit_->setAccessibleName(tr("Timeline end"));
    durationEdit_->setToolTip(
        tr("Edit the timeline end, for example 500 ns or 500.5 ns, then press Enter"));
    durationEdit_->setAlignment(Qt::AlignCenter);
    durationEdit_->setStyleSheet(QStringLiteral(
        "QLineEdit { color: #edf2f8; background: #2d3949;"
        " border: 1px solid #65758b; border-radius: 4px; padding: 2px 5px; }"
        "QLineEdit:focus { border-color: #8fc3ff; }"));
    durationEdit_->installEventFilter(this);
    connect(durationEdit_, &QLineEdit::editingFinished, this, [this] {
        durationEditMouseFocusOut_ = false;
        if (!durationEdit_->isModified() || durationEditSubmitting_) return;
        submitDurationEdit(true);
    });

    busPresetPalette_ = new QFrame(viewport());
    busPresetPalette_->setObjectName(QStringLiteral("BusPresetPalette"));
    busPresetPalette_->setFrameShape(QFrame::StyledPanel);
    busPresetPalette_->setAttribute(Qt::WA_StyledBackground, true);
    busPresetPalette_->setStyleSheet(QStringLiteral(
        "QFrame#BusPresetPalette {"
        " background: #2d3949; border: 1px solid #78aef0; border-radius: 7px;"
        "}"
        "QFrame#BusPresetPalette QLabel { color: #e4ecf7; font-weight: 600; }"
        "QFrame#BusPresetPalette QLineEdit, QFrame#BusPresetPalette QComboBox {"
        " color: #f3f6fa; background: #263241;"
        " border: 1px solid #7588a2; border-radius: 4px; padding: 3px 6px;"
        "}"
        "QFrame#BusPresetPalette QLineEdit:focus { border-color: #8dc0ff; }"
        "QFrame#BusPresetPalette QLineEdit[relationRisk=\"true\"] {"
        " color: #fff3e0; background: #4f3c25; border-color: #ffb74d;"
        "}"
        "QFrame#BusPresetPalette QLineEdit[noEffect=\"true\"] {"
        " color: #aeb8c6; background: #303b49; border-color: #5d6a7c;"
        "}"
        "QFrame#BusPresetPalette QLineEdit[invalidDraft=\"true\"] {"
        " color: #fff1f1; background: #4b2d35; border-color: #ef7773;"
        "}"
        "QFrame#BusPresetPalette QToolButton {"
        " color: #f3f6fa; background: #46566c;"
        " border: 1px solid #6f8099; border-radius: 4px; padding: 3px 5px;"
        "}"
        "QFrame#BusPresetPalette QToolButton:hover {"
        " background: #56708f; border-color: #8dc0ff;"
        "}"
        "QFrame#BusPresetPalette QToolButton[relationRisk=\"true\"] {"
        " color: #fff3e0; background: #5a4528; border-color: #ffb74d;"
        "}"
        "QFrame#BusPresetPalette QToolButton[relationRisk=\"true\"]:hover {"
        " background: #735a31; border-color: #ffd180;"
        "}"
        "QFrame#BusPresetPalette QToolButton:disabled {"
        " color: #8894a5; background: #374352; border-color: #526074;"
        "}"));
    auto* presetLayout = new QVBoxLayout(busPresetPalette_);
    presetLayout->setContentsMargins(7, 5, 7, 5);
    presetLayout->setSpacing(4);
    auto* presetHeaderLayout = new QHBoxLayout;
    presetHeaderLayout->setContentsMargins(0, 0, 0, 0);
    presetHeaderLayout->setSpacing(4);
    auto* presetControlLayout = new QHBoxLayout;
    presetControlLayout->setContentsMargins(0, 0, 0, 0);
    presetControlLayout->setSpacing(5);
    presetLayout->addLayout(presetHeaderLayout);
    presetLayout->addLayout(presetControlLayout);
    busPresetContextLabel_ = new QLabel(tr("Bus · Beat"), busPresetPalette_);
    busPresetContextLabel_->setObjectName(QStringLiteral("BusPresetContextLabel"));
    busPresetContextLabel_->setMinimumWidth(180);
    busPresetContextLabel_->setMaximumWidth(390);
    presetHeaderLayout->addWidget(busPresetContextLabel_, 1);

    const auto makeBusControl = [this](
                                    const QString& text,
                                    const QString& objectName,
                                    const QString& accessibleName,
                                    const QString& toolTip) {
        auto* button = new QToolButton(busPresetPalette_);
        button->setText(text);
        button->setObjectName(objectName);
        button->setAccessibleName(accessibleName);
        button->setToolTip(toolTip);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::StrongFocus);
        button->setCursor(Qt::PointingHandCursor);
        return button;
    };
    busScopeButton_ = makeBusControl(
        tr("Beat"),
        QStringLiteral("BusEditScopeButton"),
        tr("Bus edit scope"),
        tr("Switch between one beat and the complete explicit Segment"));
    presetHeaderLayout->addWidget(busScopeButton_);
    connect(busScopeButton_, &QToolButton::clicked, this, [this] {
        static_cast<void>(toggleBusEditScope());
    });

    busPreviousButton_ = makeBusControl(
        QStringLiteral("◀"),
        QStringLiteral("BusEditPreviousButton"),
        tr("Previous Bus edit target"),
        tr("Apply the draft and move to the previous target (Shift+Tab)"));
    presetHeaderLayout->addWidget(busPreviousButton_);
    connect(busPreviousButton_, &QToolButton::clicked, this, [this] {
        submitBusValue(
            busEditScope_ == BusEditScope::Segment
                ? BusEditCommitAction::PreviousSegment
                : BusEditCommitAction::PreviousBeat);
    });

    busNextButton_ = makeBusControl(
        QStringLiteral("▶"),
        QStringLiteral("BusEditNextButton"),
        tr("Next Bus edit target"),
        tr("Apply the draft and move to the next target (Tab)"));
    presetHeaderLayout->addWidget(busNextButton_);
    connect(busNextButton_, &QToolButton::clicked, this, [this] {
        submitBusValue(
            busEditScope_ == BusEditScope::Segment
                ? BusEditCommitAction::NextSegment
                : BusEditCommitAction::NextBeat);
    });

    busCloseButton_ = makeBusControl(
        QStringLiteral("×"),
        QStringLiteral("BusEditCloseButton"),
        tr("Close Bus editor"),
        tr("Discard an uncommitted draft and keep the waveform target (Esc)"));
    presetHeaderLayout->addWidget(busCloseButton_);
    connect(busCloseButton_, &QToolButton::clicked, this, &WaveCanvas::cancelBusValueEdit);

    busRadixCombo_ = new QComboBox(busPresetPalette_);
    busRadixCombo_->setObjectName(QStringLiteral("BusEditRadixCombo"));
    busRadixCombo_->setAccessibleName(tr("Bus input radix"));
    busRadixCombo_->setToolTip(tr("Interpret values without a prefix using this radix"));
    busRadixCombo_->addItem(QStringLiteral("HEX"), static_cast<int>(Radix::Hexadecimal));
    busRadixCombo_->addItem(QStringLiteral("BIN"), static_cast<int>(Radix::Binary));
    busRadixCombo_->addItem(QStringLiteral("DEC"), static_cast<int>(Radix::Decimal));
    busRadixCombo_->addItem(QStringLiteral("OCT"), static_cast<int>(Radix::Octal));
    busRadixCombo_->setMaximumWidth(72);
    presetControlLayout->addWidget(busRadixCombo_);
    connect(
        busRadixCombo_,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        [this] {
            if (busValueEdit_ && busValueEdit_->isModified()) {
                updateBusEditActionStates(true);
            }
        });
    busValueEdit_ = new QLineEdit(busPresetPalette_);
    busValueEdit_->setObjectName(QStringLiteral("BusPresetValueEdit"));
    busValueEdit_->setPlaceholderText(tr("Value + Enter"));
    busValueEdit_->setAccessibleName(tr("Bus value"));
    busValueEdit_->setMinimumWidth(160);
    busValueEdit_->setMaximumWidth(240);
    laneValueCompletionModel_ = new QStringListModel(this);
    laneValueCompleter_ = new QCompleter(laneValueCompletionModel_, this);
    laneValueCompleter_->setCaseSensitivity(Qt::CaseInsensitive);
    laneValueCompleter_->setCompletionMode(QCompleter::PopupCompletion);
    busValueEdit_->setCompleter(laneValueCompleter_);
    busValueEdit_->installEventFilter(this);
    connect(
        busValueEdit_,
        &QLineEdit::textEdited,
        this,
        [this] {
            updateBusEditActionStates(true);
        });
    presetControlLayout->addWidget(busValueEdit_, 1);
    busRecentValuesCombo_ = new QComboBox(busPresetPalette_);
    busRecentValuesCombo_->setObjectName(QStringLiteral("BusEditRecentValuesCombo"));
    busRecentValuesCombo_->setAccessibleName(tr("Recent Bus values"));
    busRecentValuesCombo_->setToolTip(tr("Reuse a recent value for this signal"));
    busRecentValuesCombo_->addItem(tr("Recent"));
    busRecentValuesCombo_->setEnabled(false);
    busRecentValuesCombo_->setMinimumWidth(82);
    busRecentValuesCombo_->setMaximumWidth(120);
    presetControlLayout->addWidget(busRecentValuesCombo_);
    connect(
        busRecentValuesCombo_,
        qOverload<int>(&QComboBox::activated),
        this,
        [this](const int index) {
            if (index <= 0 || !busValueEdit_) return;
            busValueEdit_->setText(busRecentValuesCombo_->itemText(index));
            busValueEdit_->setModified(true);
            busValueEdit_->setFocus(Qt::OtherFocusReason);
            busValueEdit_->selectAll();
            updateBusEditActionStates(true);
        });
    const std::array<std::tuple<QString, QString, QString>, 5> presets{{
        {QStringLiteral("zero"), QStringLiteral("0"), QStringLiteral("BusPresetZeroButton")},
        {QStringLiteral("reserved"), tr("Reserved"), QStringLiteral("BusPresetReservedButton")},
        {QStringLiteral("x"), QStringLiteral("X"), QStringLiteral("BusPresetXButton")},
        {QStringLiteral("z"), QStringLiteral("Z"), QStringLiteral("BusPresetZButton")},
        {QStringLiteral("dont-care"), tr("Don't care"), QStringLiteral("BusPresetDontCareButton")},
    }};
    std::size_t presetIndex = 0;
    for (const auto& [presetId, label, objectName] : presets) {
        auto* button = new QToolButton(busPresetPalette_);
        button->setText(label);
        button->setObjectName(objectName);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::StrongFocus);
        button->setCursor(Qt::PointingHandCursor);
        button->installEventFilter(this);
        button->setToolTip(
            presetId == QStringLiteral("reserved")
                ? tr("Mark this target as reserved while preserving an explicit zero-valued waveform")
            : presetId == QStringLiteral("dont-care")
                ? tr("Ignore this target range during Expected/Actual comparison")
                : presetId == QStringLiteral("x")
                    ? tr("Drive an unknown value; X remains significant unless compare rules ignore it")
                    : tr("Apply %1 to the current Bus target").arg(label));
        presetControlLayout->addWidget(button);
        connect(button, &QToolButton::clicked, this, [this, presetId] {
            if (!busPresetLaneId_.empty() && busPresetAnchorTick_) {
                applyBusPreset(busPresetLaneId_, presetId.toStdString(), *busPresetAnchorTick_);
            }
        });
        busPresetButtons_.at(presetIndex++) = button;
    }
    busClearButton_ = makeBusControl(
        tr("Clear"),
        QStringLiteral("BusEditClearButton"),
        tr("Clear Bus or Enum target"),
        tr("Restore the current target to implicit X and keep editing"));
    busClearButton_->installEventFilter(this);
    presetControlLayout->addWidget(busClearButton_);
    connect(
        busClearButton_,
        &QToolButton::clicked,
        this,
        &WaveCanvas::clearBusEditTarget);
    busApplyButton_ = makeBusControl(
        tr("Apply"),
        QStringLiteral("BusEditApplyButton"),
        tr("Apply Bus value"),
        tr("Apply the current value and close the editor (Enter)"));
    busApplyButton_->installEventFilter(this);
    presetControlLayout->addWidget(busApplyButton_);
    connect(busApplyButton_, &QToolButton::clicked, this, [this] {
        submitBusValue(BusEditCommitAction::Close);
    });
    busPresetPalette_->adjustSize();
    busPresetPalette_->hide();

    rangeEditPalette_ = new QFrame(this);
    rangeEditPalette_->setObjectName(QStringLiteral("RangeEditPalette"));
    rangeEditPalette_->setFrameShape(QFrame::StyledPanel);
    rangeEditPalette_->setAttribute(Qt::WA_StyledBackground, true);
    rangeEditPalette_->setStyleSheet(QStringLiteral(
        "QFrame#RangeEditPalette {"
        " background: rgba(45, 57, 73, 246);"
        " border: 1px solid #78aef0; border-radius: 7px;"
        "}"
        "QFrame#RangeEditPalette QLabel { color: #e4ecf7; font-weight: 600; }"
        "QLabel#RangeEditContextLabel {"
        " border-bottom: 1px dotted #8dc0ff;"
        "}"
        "QLabel#RangeEditContextLabel:hover { background: #35465d; }"
        "QFrame#RangeEditPalette QLineEdit {"
        " color: #f3f6fa; background: #263241;"
        " border: 1px solid #7588a2; border-radius: 4px; padding: 3px 6px;"
        "}"
        "QFrame#RangeEditPalette QLineEdit[loadedExisting=\"true\"] {"
        " color: #c4d1e2; background: #2b3746; border-style: dashed;"
        "}"
        "QFrame#RangeEditPalette QLineEdit:focus { border-color: #8dc0ff; }"
        "QFrame#RangeEditPalette QLineEdit[relationRisk=\"true\"] {"
        " color: #fff3e0; background: #4f3c25; border-color: #ffb74d;"
        "}"
        "QFrame#RangeEditPalette QLineEdit[noEffect=\"true\"] {"
        " color: #aeb8c6; background: #303b49; border-color: #5d6a7c;"
        "}"
        "QFrame#RangeEditPalette QLineEdit[invalidDraft=\"true\"] {"
        " color: #fff1f1; background: #4b2d35; border-color: #ef7773;"
        "}"
        "QFrame#RangeEditPalette QToolButton {"
        " color: #f3f6fa; background: #46566c;"
        " border: 1px solid #6f8099; border-radius: 4px; padding: 3px 7px;"
        "}"
        "QFrame#RangeEditPalette QToolButton:hover {"
        " background: #56708f; border-color: #8dc0ff;"
        "}"
        "QFrame#RangeEditPalette QToolButton[relationRisk=\"true\"] {"
        " color: #fff3e0; background: #5a4528; border-color: #ffb74d;"
        "}"
        "QFrame#RangeEditPalette QToolButton[relationRisk=\"true\"]:hover {"
        " background: #735a31; border-color: #ffd180;"
        "}"
        "QFrame#RangeEditPalette QToolButton:disabled {"
        " color: #8894a5; background: #374352; border-color: #526074;"
        "}"));
    auto* rangeLayout = new QHBoxLayout(rangeEditPalette_);
    rangeLayout->setContentsMargins(5, 4, 5, 4);
    rangeLayout->setSpacing(3);
    rangeEditContextLabel_ = new QLabel(tr("Selected range"), rangeEditPalette_);
    rangeEditContextLabel_->setObjectName(QStringLiteral("RangeEditContextLabel"));
    rangeEditContextLabel_->setMinimumWidth(100);
    rangeEditContextLabel_->setMaximumWidth(130);
    rangeEditContextLabel_->setAccessibleName(
        tr("Selected range; click to edit the active time edge"));
    rangeEditContextLabel_->setCursor(Qt::PointingHandCursor);
    rangeEditContextLabel_->installEventFilter(this);
    rangeLayout->addWidget(rangeEditContextLabel_);
    rangeCopyButton_ = new QToolButton(rangeEditPalette_);
    rangeCopyButton_->setText(tr("Copy"));
    rangeCopyButton_->setObjectName(QStringLiteral("RangeEditCopyButton"));
    rangeCopyButton_->setAutoRaise(true);
    rangeCopyButton_->setFocusPolicy(Qt::StrongFocus);
    rangeCopyButton_->setCursor(Qt::PointingHandCursor);
    rangeCopyButton_->setToolTip(tr("Copy the selected signals and time range (Ctrl+C)"));
    rangeCopyButton_->setAccessibleName(tr("Copy selected range"));
    rangeCopyButton_->setIcon(QIcon::fromTheme(
        QStringLiteral("edit-copy"),
        style()->standardIcon(QStyle::SP_FileDialogListView)));
    rangeCopyButton_->setText({});
    rangeLayout->addWidget(rangeCopyButton_);
    connect(rangeCopyButton_, &QToolButton::clicked, this, &WaveCanvas::copySelection);
    rangeRepeatButton_ = new QToolButton(rangeEditPalette_);
    rangeRepeatButton_->setText(tr("Repeat"));
    rangeRepeatButton_->setObjectName(QStringLiteral("RangeEditRepeatButton"));
    rangeRepeatButton_->setAutoRaise(true);
    rangeRepeatButton_->setFocusPolicy(Qt::StrongFocus);
    rangeRepeatButton_->setCursor(Qt::PointingHandCursor);
    rangeRepeatButton_->setToolTip(
        tr("Repeat the complete selected range immediately after it without changing the clipboard (Ctrl+D)"));
    rangeRepeatButton_->setAccessibleName(tr("Repeat selected range after"));
    rangeRepeatButton_->setIcon(
        style()->standardIcon(QStyle::SP_BrowserReload));
    rangeRepeatButton_->setText({});
    rangeRepeatButton_->installEventFilter(this);
    rangeLayout->addWidget(rangeRepeatButton_);
    connect(
        rangeRepeatButton_,
        &QToolButton::clicked,
        this,
        &WaveCanvas::duplicateSelectionAfter);
    rangePasteButton_ = new QToolButton(rangeEditPalette_);
    rangePasteButton_->setText(tr("Paste"));
    rangePasteButton_->setObjectName(QStringLiteral("RangeEditPasteButton"));
    rangePasteButton_->setAutoRaise(true);
    rangePasteButton_->setFocusPolicy(Qt::StrongFocus);
    rangePasteButton_->setCursor(Qt::PointingHandCursor);
    rangePasteButton_->setToolTip(tr("Paste a copied range at the selected start (Ctrl+V)"));
    rangePasteButton_->setAccessibleName(tr("Paste copied range into selected signals"));
    rangePasteButton_->setIcon(QIcon::fromTheme(
        QStringLiteral("edit-paste"),
        style()->standardIcon(QStyle::SP_DialogOpenButton)));
    rangePasteButton_->setText({});
    rangeLayout->addWidget(rangePasteButton_);
    connect(rangePasteButton_, &QToolButton::clicked, this, &WaveCanvas::pasteAtCursor);
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, [this] {
        if (rangeEditPaletteVisible_) showRangeEditPalette();
        viewport()->update();
    });
    rangeClearButton_ = new QToolButton(rangeEditPalette_);
    rangeClearButton_->setText(tr("Clear"));
    rangeClearButton_->setObjectName(QStringLiteral("RangeEditClearButton"));
    rangeClearButton_->setAutoRaise(true);
    rangeClearButton_->setFocusPolicy(Qt::StrongFocus);
    rangeClearButton_->setCursor(Qt::PointingHandCursor);
    rangeClearButton_->setToolTip(tr("Clear values in the selected range (Delete)"));
    rangeClearButton_->setAccessibleName(tr("Clear selected range"));
    rangeClearButton_->setIcon(
        style()->standardIcon(QStyle::SP_TrashIcon));
    rangeClearButton_->setText({});
    rangeLayout->addWidget(rangeClearButton_);
    connect(rangeClearButton_, &QToolButton::clicked, this, [this] {
        static_cast<void>(clearExplicitRange());
    });
    rangeValueEdit_ = new QLineEdit(rangeEditPalette_);
    rangeValueEdit_->setObjectName(QStringLiteral("RangeEditValueEdit"));
    rangeValueEdit_->setPlaceholderText(tr("Value + Enter"));
    rangeValueEdit_->setAccessibleName(tr("Selected bus range value"));
    rangeValueEdit_->setMinimumWidth(90);
    rangeValueEdit_->setMaximumWidth(120);
    rangeValueCompletionModel_ = new QStringListModel(this);
    rangeValueCompleter_ = new QCompleter(rangeValueCompletionModel_, this);
    rangeValueCompleter_->setCaseSensitivity(Qt::CaseInsensitive);
    rangeValueCompleter_->setCompletionMode(QCompleter::PopupCompletion);
    rangeValueEdit_->setCompleter(rangeValueCompleter_);
    rangeValueEdit_->installEventFilter(this);
    rangeLoadValuesAction_ = rangeValueEdit_->addAction(
        style()->standardIcon(QStyle::SP_BrowserReload),
        QLineEdit::TrailingPosition);
    rangeLoadValuesAction_->setObjectName(
        QStringLiteral("RangeEditLoadValuesAction"));
    rangeLoadValuesAction_->setText(
        tr("Load current beat values"));
    rangeLoadValuesAction_->setToolTip(
        tr("Load the selected range's current beat values for editing"));
    rangeLoadValuesAction_->setVisible(false);
    connect(
        rangeLoadValuesAction_,
        &QAction::triggered,
        this,
        &WaveCanvas::loadCurrentRangeSequence);
    connect(
        rangeValueEdit_,
        &QLineEdit::textEdited,
        this,
        [this] {
            if (rangeSequenceBaseline_
                && !rangeSequenceBaselineMatchesContext()) {
                clearRangeSequenceBaseline();
            }
            if (rangeValueEdit_
                && rangeSequenceBaseline_
                && rangeSequenceBaselineMatchesContext()
                && rangeValueEdit_->text()
                    == rangeSequenceBaseline_->text) {
                rangeValueEdit_->setProperty(
                    "loadedExisting",
                    true);
                rangeValueEdit_->setModified(false);
                rangeSequenceTextSpans_ =
                    rangeSequenceBaseline_->spans;
                busRangeSequencePreview_.reset();
                rangeValueEdit_->setStyleSheet({});
                rangeValueEdit_->style()->unpolish(
                    rangeValueEdit_);
                rangeValueEdit_->style()->polish(
                    rangeValueEdit_);
                if (rangeEditPaletteVisible_) {
                    showRangeEditPalette();
                }
                updateRangeSequenceCaretTarget();
                emit statusMessage(
                    tr("Loaded current values restored · no waveform or history change"));
                return;
            }
            if (rangeValueEdit_
                && rangeValueEdit_->property(
                    "loadedExisting").toBool()) {
                rangeValueEdit_->setProperty(
                    "loadedExisting",
                    false);
                rangeValueEdit_->style()->unpolish(
                    rangeValueEdit_);
                rangeValueEdit_->style()->polish(
                    rangeValueEdit_);
            }
            rangeSequenceTextSpans_.clear();
            clearRangeSequenceCaretTarget();
            if (rangeEditPaletteVisible_) showRangeEditPalette();
            const auto kind = explicitRangeKind();
            if (kind
                && *kind == LaneKind::Bit
                && rangeValueEdit_
                && rangeValueEdit_->isModified()) {
                auto message =
                    assessBitPatternDraft(
                        rangeValueEdit_->text()).summary;
                message.replace(
                    QLatin1Char('\n'),
                    QStringLiteral(" · "));
                emit statusMessage(message);
            } else if (kind
                       && (*kind == LaneKind::Bus
                           || *kind == LaneKind::Enum)
                       && rangeValueEdit_
                       && rangeValueEdit_->isModified()) {
                auto assessment =
                    assessBusRangeSequenceDraft(
                        rangeValueEdit_->text());
                if (assessment.sequence) {
                    assessment.summary.replace(
                        QLatin1Char('\n'),
                        QStringLiteral(" · "));
                    emit statusMessage(
                        assessment.summary);
                }
            }
            updateRangeSequenceCaretTarget();
        });
    connect(
        rangeValueEdit_,
        &QLineEdit::cursorPositionChanged,
        this,
        [this](const int, const int) {
            updateRangeSequenceCaretTarget();
        });
    connect(
        rangeValueEdit_,
        &QLineEdit::selectionChanged,
        this,
        [this] {
            updateRangeSequenceCaretTarget();
        });
    rangeLayout->addWidget(rangeValueEdit_);
    const auto makeRangeButton = [this, rangeLayout](
                                     const QString& text,
                                     const QString& objectName,
                                     const std::string& presetId) {
        auto* button = new QToolButton(rangeEditPalette_);
        button->setText(text);
        button->setObjectName(objectName);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::StrongFocus);
        button->setCursor(Qt::PointingHandCursor);
        button->setToolTip(tr("Set the whole selected range to %1").arg(text));
        rangeLayout->addWidget(button);
        connect(button, &QToolButton::clicked, this, [this, presetId] {
            applyExplicitRangePreset(presetId);
        });
        return button;
    };
    rangeZeroButton_ = makeRangeButton(
        QStringLiteral("0"), QStringLiteral("RangeEditZeroButton"), "zero");
    rangeOneButton_ = makeRangeButton(
        QStringLiteral("1"), QStringLiteral("RangeEditOneButton"), "one");
    rangeXButton_ = makeRangeButton(
        QStringLiteral("X"), QStringLiteral("RangeEditXButton"), "x");
    rangeZButton_ = makeRangeButton(
        QStringLiteral("Z"), QStringLiteral("RangeEditZButton"), "z");
    rangeDontCareButton_ = makeRangeButton(
        tr("Don't care"),
        QStringLiteral("RangeEditDontCareButton"),
        "dont-care");
    rangeReservedButton_ = makeRangeButton(
        tr("Reserved"),
        QStringLiteral("RangeEditReservedButton"),
        "reserved");
    rangeClockGateButton_ = makeRangeButton(
        tr("Gate"),
        QStringLiteral("RangeEditClockGateButton"),
        "clock-gate");
    rangeClockDisableButton_ = makeRangeButton(
        tr("Disable"),
        QStringLiteral("RangeEditClockDisableButton"),
        "clock-disable");
    rangeCloseButton_ = new QToolButton(rangeEditPalette_);
    rangeCloseButton_->setText(QStringLiteral("×"));
    rangeCloseButton_->setObjectName(QStringLiteral("RangeEditCloseButton"));
    rangeCloseButton_->setAccessibleName(tr("Close range selection"));
    rangeCloseButton_->setToolTip(
        tr("Close the range while keeping the active signal and edit cursor (Esc)"));
    rangeCloseButton_->setAutoRaise(true);
    rangeCloseButton_->setFocusPolicy(Qt::StrongFocus);
    rangeCloseButton_->setCursor(Qt::PointingHandCursor);
    rangeLayout->addWidget(rangeCloseButton_);
    connect(rangeCloseButton_, &QToolButton::clicked, this, [this] {
        const auto keptLaneId = selectedLaneId_;
        clearExplicitRangeSelection(false);
        if (!keptLaneId.empty()) {
            selectedLaneId_ = keptLaneId;
            selectedLaneIds_ = {keptLaneId};
            emit selectionChanged(QString::fromStdString(keptLaneId), cursorTick_);
        }
        emit statusMessage(
            keptLaneId.empty()
                ? tr("Range selection closed")
                : tr("Range selection closed · active signal and edit cursor kept"));
    });
    rangeEditPalette_->adjustSize();

    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, [this] {
        positionBusPresetPalette();
        viewport()->update();
    });
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this] {
        updateAddLaneButtonGeometry();
        positionQuickLaneSetup();
        positionLaneRename();
        positionBusPresetPalette();
        viewport()->update();
    });
    positionDurationEditor();
}

bool WaveCanvas::hasQuickLaneSetup() const noexcept
{
    return quickLaneSetupPanel_ && quickLaneSetupPanel_->isVisible();
}

bool WaveCanvas::hasLaneRename() const noexcept
{
    return laneRenameEdit_ && laneRenameEdit_->isVisible();
}

bool WaveCanvas::isGroupCollapsed(const QString& groupId) const noexcept
{
    if (!scenario_ || groupId.isEmpty()) return false;
    const auto* group = findLane(*scenario_, groupId.toStdString());
    return group
        && group->visible
        && group->kind == LaneKind::Group
        && collapsedGroupIds_.contains(group->id);
}

bool WaveCanvas::isLaneDisplayed(const QString& laneId) const noexcept
{
    if (!scenario_ || laneId.isEmpty()) return false;
    const auto* lane = findLane(*scenario_, laneId.toStdString());
    return lane && isLaneDisplayed(*lane);
}

bool WaveCanvas::setGroupCollapsed(
    const QString& groupId,
    const bool collapsed)
{
    if (!scenario_ || groupId.isEmpty()) return false;
    auto* group = findLane(*scenario_, groupId.toStdString());
    if (!group || !group->visible || group->kind != LaneKind::Group) {
        emit statusMessage(tr("The group is not visible"));
        return false;
    }
    if (drawing_ || laneHeaderPressed_ || laneHeaderDragging_) {
        emit statusMessage(
            tr("Finish or cancel the current drag before changing a group"));
        return false;
    }
    if (!commitPendingInlineEdits()) return false;

    const auto memberCount = visibleGroupMemberCount(group->id);
    if (memberCount == 0) {
        emit statusMessage(
            tr("Group %1 has no visible member signals")
                .arg(QString::fromStdString(group->name)));
        return false;
    }
    const auto currentlyCollapsed = collapsedGroupIds_.contains(group->id);
    if (currentlyCollapsed == collapsed) {
        emit statusMessage(
            collapsed
                ? tr("Group %1 is already collapsed · Right expands")
                      .arg(QString::fromStdString(group->name))
                : tr("Group %1 is already expanded · Left collapses")
                      .arg(QString::fromStdString(group->name)));
        return true;
    }

    const auto belongsToGroup = [this, group](const std::string& laneId) {
        const auto* lane = findLane(*scenario_, laneId);
        return lane && lane->groupId == group->id;
    };
    const auto selectionAffected = collapsed
        && (belongsToGroup(selectedLaneId_)
            || std::any_of(
                selectedLaneIds_.begin(),
                selectedLaneIds_.end(),
                belongsToGroup));
    if (collapsed) {
        collapsedGroupIds_.insert(group->id);
    } else {
        collapsedGroupIds_.erase(group->id);
    }

    if (selectionAffected) {
        clearWaveEditState();
        hideBusPresetPalette();
        selectedLaneId_ = group->id;
        selectedLaneIds_ = {group->id};
        laneHeaderSelectionActive_ = false;
        selectedMarkerId_.clear();
        cursorInteraction_ = CursorInteraction::None;
        lockedMarkerOriginalRange_.reset();
    }
    rebuildLaneLayout();
    ensureLaneVisible(group->id);
    positionBusPresetPalette();
    if (selectionAffected) {
        emit selectionChanged(
            QString::fromStdString(group->id),
            cursorTick_);
    }
    emit statusMessage(
        collapsed
            ? tr("Collapsed group %1 · %2 signal(s) hidden from view · data unchanged · Right expands")
                  .arg(QString::fromStdString(group->name))
                  .arg(static_cast<qulonglong>(memberCount))
            : tr("Expanded group %1 · %2 signal(s) shown · data unchanged · Left collapses")
                  .arg(QString::fromStdString(group->name))
                  .arg(static_cast<qulonglong>(memberCount)));
    viewport()->setCursor(Qt::PointingHandCursor);
    viewport()->update();
    return true;
}

int WaveCanvas::signalHeaderWidth() const noexcept
{
    return headerWidth_;
}

void WaveCanvas::setSignalHeaderWidth(const int width)
{
    const auto clamped = std::clamp(width, MinimumHeaderWidth, MaximumHeaderWidth);
    if (headerWidth_ == clamped) return;
    headerWidth_ = clamped;
    updateScrollBars();
    positionQuickLaneSetup();
    positionLaneRename();
    positionDurationEditor();
    positionBusPresetPalette();
    viewport()->update();
}

QWidget* WaveCanvas::busEditPaletteWidget() const noexcept
{
    return busPresetPalette_;
}

QWidget* WaveCanvas::rangeEditPaletteWidget() const noexcept
{
    return rangeEditPalette_;
}

bool WaveCanvas::asynchronousEditing() const noexcept
{
    return asynchronousEditing_;
}

bool WaveCanvas::relationsVisible() const noexcept
{
    return relationsVisible_;
}

void WaveCanvas::setRelationsVisible(const bool visible)
{
    if (relationsVisible_ == visible) return;
    relationsVisible_ = visible;
    viewport()->update();
    emit statusMessage(
        visible
            ? tr("Relation constraints shown")
            : tr("Relation constraints hidden; edit-impact warnings remain visible"));
}

void WaveCanvas::invalidateRangeSequenceHistoryContext()
{
    if (rangeValueEdit_) {
        rangeValueEdit_->clear();
        rangeValueEdit_->setModified(false);
        rangeValueEdit_->setProperty("loadedExisting", false);
        rangeValueEdit_->setStyleSheet({});
        rangeValueEdit_->setToolTip({});
    }
    busRangeSequencePreview_.reset();
    rangeSequenceTextSpans_.clear();
    clearRangeSequenceBaseline();
    clearRangeSequenceAnchor();
    clearRangeSequenceCaretTarget();
}

std::optional<Tick> WaveCanvas::explicitRangeAnchorTick() const noexcept
{
    const auto endpoints = explicitRangeAnchorAndActive();
    return endpoints
        ? std::optional<Tick>{endpoints->first}
        : std::nullopt;
}

std::optional<Tick> WaveCanvas::explicitRangeActiveTick() const noexcept
{
    const auto endpoints = explicitRangeAnchorAndActive();
    return endpoints
        ? std::optional<Tick>{endpoints->second}
        : std::nullopt;
}

QString WaveCanvas::editTargetSummary() const
{
    const auto formatTime = [this](const Tick tick) {
        return project_
            ? QString::fromStdString(formatTick(tick, project_->timeBase))
            : tr("%1 ticks").arg(tick);
    };
    const auto compactValue = [](QString value) {
        constexpr auto maximumLength = qsizetype{18};
        if (value.size() <= maximumLength) return value;
        return value.left(maximumLength - 1) + QChar(0x2026);
    };

    const auto* headerLane = scenario_
        ? findLane(*scenario_, selectedLaneId_)
        : nullptr;
    if (laneHeaderSelectionActive_ && headerLane) {
        const auto laneName = QString::fromStdString(headerLane->name);
        if (headerLane->kind == LaneKind::Group) {
            return tr("Target: group · %1 · %2")
                .arg(laneName)
                .arg(
                    collapsedGroupIds_.contains(headerLane->id)
                        ? QStringLiteral("collapsed")
                        : QStringLiteral("expanded"));
        }
        if (selectedLaneIds_.size() > 1) {
            return tr("Target: %1 signals · active %2 · %3")
                .arg(static_cast<qulonglong>(selectedLaneIds_.size()))
                .arg(laneName)
                .arg(formatTime(cursorTick_));
        }
        return tr("Target: signal · %1 · %2")
            .arg(laneName)
            .arg(formatTime(cursorTick_));
    }

    if (tool_ == Tool::Marker) {
        if (const auto* marker = markerById(selectedMarkerId_)) {
            if (marker->end > marker->start) {
                return tr("Target: locked range · %1–%2")
                    .arg(formatTime(marker->start), formatTime(marker->end));
            }
            return tr("Target: locked cursor · %1").arg(formatTime(marker->start));
        }
        if (movableCursorTick_ && temporaryCursorTick_) {
            return tr("Target: temporary measure · %1 to %2")
                .arg(formatTime(*movableCursorTick_), formatTime(*temporaryCursorTick_));
        }
        if (temporaryCursorTick_) {
            return tr("Target: temporary cursor · %1")
                .arg(formatTime(*temporaryCursorTick_));
        }
        if (movableCursorTick_) {
            return tr("Target: movable cursor · %1")
                .arg(formatTime(*movableCursorTick_));
        }
        return tr("Target: Measure · click the waveform");
    }

    const auto laneId = !selectedSegmentLaneId_.empty()
        ? selectedSegmentLaneId_
        : selectedLaneId_;
    const auto* lane = scenario_ ? findLane(*scenario_, laneId) : nullptr;
    if (!lane) return tr("Target: none");

    const auto laneName = QString::fromStdString(lane->name);
    if (lane->kind == LaneKind::Group) {
        const auto groupState = collapsedGroupIds_.contains(lane->id)
            ? QStringLiteral("collapsed")
            : QStringLiteral("expanded");
        return tr("Target: group · %1 · %2")
            .arg(laneName)
            .arg(groupState);
    }
    if (explicitRangeSelection_ && selectionRange_) {
        const auto count = std::max<std::size_t>(1, selectedLaneIds_.size());
        return tr("Target: range · %1–%2 · %3 signal(s)")
            .arg(formatTime(selectionRange_->first))
            .arg(formatTime(selectionRange_->second))
            .arg(static_cast<qulonglong>(count));
    }
    if (const auto* segment = segmentById(
            selectedSegmentLaneId_,
            selectedSegmentId_)) {
        return tr("Target: Segment · %1 · %2–%3 · %4")
            .arg(laneName)
            .arg(formatTime(segment->start))
            .arg(formatTime(segment->end))
            .arg(compactValue(QString::fromStdString(segment->value)));
    }
    if (selectionRange_ && selectionRange_->second > selectionRange_->first) {
        return tr("Target: beat · %1 · %2–%3 · %4")
            .arg(laneName)
            .arg(formatTime(selectionRange_->first))
            .arg(formatTime(selectionRange_->second))
            .arg(compactValue(laneValueAt(*lane, selectionRange_->first)));
    }
    return tr("Target: %1 · %2 · %3")
        .arg(laneName)
        .arg(formatTime(cursorTick_))
        .arg(compactValue(laneValueAt(*lane, cursorTick_)));
}

QString WaveCanvas::editTargetToolTip() const
{
    const auto laneId = !selectedSegmentLaneId_.empty()
        ? selectedSegmentLaneId_
        : selectedLaneId_;
    const auto* lane = scenario_ ? findLane(*scenario_, laneId) : nullptr;
    if (laneHeaderSelectionActive_ && lane) {
        if (lane->kind == LaneKind::Group) {
            return tr("Group header selected. Left collapses, Right expands, F2 renames, and Delete removes after confirmation.");
        }
        if (selectedLaneIds_.size() > 1) {
            return tr("%1 signal headers selected. Ctrl+G positions the edit cursor and Ctrl+V pastes copied signals into this selection in visible order; a compatible clipboard range appears as a dashed Paste preview before commit. Ctrl+D duplicates the complete signals as one block. Drag any selected name to move them together or drop them onto a Group; Alt+Up/Down moves the selection one visible row; right-click for Group and cleanup actions. Ctrl+click toggles; Shift+click extends; Esc keeps only the active signal.")
                .arg(static_cast<qulonglong>(selectedLaneIds_.size()));
        }
        auto help = tr("Signal header selected. F2 renames, Ctrl+D duplicates, and Delete removes after confirmation.");
        if (tool_ == Tool::WaveEdit) {
            help.append(
                tr("\nLeft/Right follows the visible Timing step; Ctrl+Left/Right jumps edges; Shift+Left/Right selects time with the Timing step; Shift+Home/End selects to a timeline boundary."));
            help.append(
                tr("\nCtrl+Shift+Left/Right selects to edges; Up/Down selects signals; Ctrl+A selects the full signal range; Ctrl+G positions the cursor and Ctrl+V pastes a copied range here. A compatible clipboard range appears as a dashed Paste preview before commit."));
            if (lane->kind == LaneKind::Bus || lane->kind == LaneKind::Enum) {
                help.append(tr(" Enter edits the value at the edit cursor."));
            } else if (lane->kind == LaneKind::Clock) {
                help.append(tr(" G gates, X drives unknown, and R restores the clock."));
            }
        }
        return help;
    }
    if (tool_ == Tool::Marker) {
        return tr("Measure mode is active. Click or drag to measure, Ctrl locks the result, and Esc returns to waveform editing.");
    }
    if (!lane) {
        return tr("No edit target. Click a signal name, beat, or Segment; click or drag the ruler to place the edit cursor.");
    }
    if (lane->kind == LaneKind::Group) {
        return tr("Group view target. Left collapses, Right expands, and the disclosure arrow changes only the view; click the group name to enable rename or removal.");
    }
    if (explicitRangeSelection_) {
        return tr("Waveform range selected. Ctrl+D repeats it immediately after the selection. Click the visible range/time label or press Ctrl+G to type the exact active edge. Drag inside the selection in time or onto compatible signals to move; Ctrl+drag copies. Press or release Ctrl while dragging to switch between Move and Copy. Delete clears and the × button closes it.");
    }
    if (!selectedSegmentId_.empty()) {
        return tr("Segment selected. Drag its body or edges directly; Enter edits the value; Delete clears it. Edit > Segment contains exact commands; the Bus editor can switch between Segment and Beat.");
    }
    if (selectionRange_) {
        return tr("One beat is selected. Type a value where supported, use the visible Apply/previous/next controls, or Delete to clear the beat.");
    }
    return tr("Signal and edit cursor selected. Click or drag the ruler to place the cursor; Left/Right follows the visible Timing step; F6 selects the explicit Segment at the cursor.");
}

QString WaveCanvas::editTimingSummary() const
{
    if (asynchronousEditing_) return tr("Async · 1 tick/step");

    const auto* lane = scenario_
        ? findLane(*scenario_, selectedLaneId_)
        : nullptr;
    if (lane && project_ && !lane->clockDomainId.empty()) {
        if (const auto* clock = findClock(*project_, lane->clockDomainId);
            clock && clock->isValid()) {
            const auto name = QString::fromStdString(clock->name);
            const auto period = QString::fromStdString(
                formatTick(clock->period, project_->timeBase));
            return name.isEmpty()
                ? tr("Sync · %1/step").arg(period)
                : tr("Sync · %1 · %2/step").arg(name, period);
        }
    }

    const auto step = lane ? beatGrid(*lane).first : cursorKeyboardStep();
    const auto stepText = project_
        ? QString::fromStdString(formatTick(step, project_->timeBase))
        : tr("%1 ticks").arg(step);
    return tr("Grid · %1/step").arg(stepText);
}

WaveCanvas::HistorySelectionSnapshot
WaveCanvas::historySelectionSnapshot() const
{
    return {
        selectedLaneId_,
        selectedLaneIds_,
        selectedSegmentLaneId_,
        selectedSegmentId_,
        selectionRange_,
        cursorTick_,
        explicitRangeSelection_,
        laneHeaderSelectionActive_,
    };
}

void WaveCanvas::rememberHistorySelectionTransition(
    const std::uint64_t beforeStateId,
    const HistorySelectionSnapshot& beforeSelection,
    const std::uint64_t afterStateId)
{
    if (beforeStateId == afterStateId) return;
    const auto afterSelection = historySelectionSnapshot();
    historySelectionTransitions_[{afterStateId, beforeStateId}] =
        beforeSelection;
    historySelectionTransitions_[{beforeStateId, afterStateId}] =
        afterSelection;
}

QString WaveCanvas::restoreSelectionForHistoryTransition(
    const std::uint64_t fromStateId,
    const std::uint64_t toStateId)
{
    if (!scenario_ || fromStateId == toStateId) return {};
    invalidateRangeSequenceHistoryContext();
    segmentActionPreview_.reset();
    const auto transition =
        historySelectionTransitions_.find({fromStateId, toStateId});
    if (transition == historySelectionTransitions_.end()) return {};
    const auto& snapshot = transition->second;
    const auto emptySelection = snapshot.selectedLaneId.empty()
        && snapshot.selectedLaneIds.empty();
    if (snapshot.selectedLaneId.empty()
        != snapshot.selectedLaneIds.empty()) {
        return {};
    }
    if (emptySelection) {
        stopWaveEditDragAutoScroll();
        drawing_ = false;
        waveEditInteraction_ = WaveEditInteraction::None;
        waveEditCopyDrag_ = false;
        waveEditOriginalRange_.reset();
        waveEditPreviewRange_.reset();
        waveEditRangeTargetLaneIds_.clear();
        waveEditRangeGrabLaneOffset_ = 0;
        waveEditRangeTargetValid_ = true;
        waveEditRangeTargetError_.clear();
        waveEditHoverLaneId_.clear();
        waveEditHoverRange_.reset();
        snapGuideTick_.reset();
        selectedLaneId_.clear();
        selectedLaneIds_.clear();
        selectedSegmentLaneId_.clear();
        selectedSegmentId_.clear();
        selectionRange_.reset();
        cursorTick_ = std::clamp<Tick>(
            snapshot.cursorTick,
            0,
            scenario_->duration);
        explicitRangeSelection_ = false;
        laneHeaderSelectionActive_ = false;
        laneHeaderSelectionAnchorId_.clear();
        hideBusPresetPalette();
        hideRangeEditPalette();
        if (rangeValueEdit_) rangeValueEdit_->setModified(false);
        viewport()->setCursor(defaultCursorShape());
        viewport()->update();
        emit selectionChanged({}, cursorTick_);
        return tr("edit target cleared");
    }

    const auto validLane = [this](const std::string& laneId) {
        const auto* lane = findLane(*scenario_, laneId);
        return lane
            && isLaneDisplayed(*lane)
            && lane->kind != LaneKind::Group;
    };
    if (!validLane(snapshot.selectedLaneId)
        || !std::all_of(
            snapshot.selectedLaneIds.begin(),
            snapshot.selectedLaneIds.end(),
            validLane)) {
        return {};
    }
    const auto rangeValid = !snapshot.selectionRange
        || (snapshot.selectionRange->first >= 0
            && snapshot.selectionRange->second
                > snapshot.selectionRange->first
            && snapshot.selectionRange->second <= scenario_->duration);
    if (!rangeValid
        || (snapshot.explicitRangeSelection && !snapshot.selectionRange)) {
        return {};
    }

    stopWaveEditDragAutoScroll();
    drawing_ = false;
    waveEditInteraction_ = WaveEditInteraction::None;
    waveEditCopyDrag_ = false;
    waveEditOriginalRange_.reset();
    waveEditPreviewRange_.reset();
    waveEditRangeTargetLaneIds_.clear();
    waveEditRangeGrabLaneOffset_ = 0;
    waveEditRangeTargetValid_ = true;
    waveEditRangeTargetError_.clear();
    waveEditHoverLaneId_.clear();
    waveEditHoverRange_.reset();
    snapGuideTick_.reset();
    selectedLaneId_ = snapshot.selectedLaneId;
    selectedLaneIds_ = snapshot.selectedLaneIds;
    selectedSegmentLaneId_ = snapshot.selectedSegmentLaneId;
    selectedSegmentId_ = snapshot.selectedSegmentId;
    selectionRange_ = snapshot.selectionRange;
    cursorTick_ = std::clamp<Tick>(
        snapshot.cursorTick,
        0,
        scenario_->duration);
    explicitRangeSelection_ = snapshot.explicitRangeSelection;
    laneHeaderSelectionActive_ = snapshot.laneHeaderSelectionActive;
    laneHeaderSelectionAnchorId_ = laneHeaderSelectionActive_
        ? selectedLaneId_
        : std::string{};
    hideBusPresetPalette();
    if (rangeValueEdit_) rangeValueEdit_->setModified(false);

    if (!selectedSegmentId_.empty()
        && !segmentById(selectedSegmentLaneId_, selectedSegmentId_)) {
        selectedSegmentLaneId_.clear();
        selectedSegmentId_.clear();
    }
    if (explicitRangeSelection_) {
        selectedSegmentLaneId_.clear();
        selectedSegmentId_.clear();
        showRangeEditPalette();
    } else {
        hideRangeEditPalette();
    }

    ensureLaneVisible(selectedLaneId_);
    if (selectionRange_) {
        const auto left = xAtTick(selectionRange_->first);
        const auto right = xAtTick(selectionRange_->second);
        const auto available = std::max(1, waveViewportWidth() - 48);
        if (right - left <= available) {
            auto desiredScroll = horizontalScrollBar()->value();
            if (left < headerWidth_ + 24) {
                desiredScroll += left - (headerWidth_ + 24);
            } else if (right > viewport()->width() - 24) {
                desiredScroll += right - (viewport()->width() - 24);
            }
            horizontalScrollBar()->setValue(std::clamp(
                desiredScroll,
                horizontalScrollBar()->minimum(),
                horizontalScrollBar()->maximum()));
        } else {
            ensureCursorVisible(cursorTick_);
        }
    } else {
        ensureCursorVisible(cursorTick_);
    }
    viewport()->setCursor(defaultCursorShape());
    viewport()->update();
    emit selectionChanged(
        QString::fromStdString(selectedLaneId_),
        cursorTick_);

    const auto format = [this](const Tick tick) {
        return project_
            ? QString::fromStdString(formatTick(tick, project_->timeBase))
            : QString::number(tick);
    };
    if (explicitRangeSelection_ && selectionRange_) {
        return tr("selection restored to %1–%2 on %3 signal(s)")
            .arg(format(selectionRange_->first))
            .arg(format(selectionRange_->second))
            .arg(static_cast<qulonglong>(selectedLaneIds_.size()));
    }
    if (!selectedSegmentId_.empty() && selectionRange_) {
        return tr("Segment target restored to %1–%2 on %3")
            .arg(format(selectionRange_->first))
            .arg(format(selectionRange_->second))
            .arg(QString::fromStdString(
                findLane(*scenario_, selectedLaneId_)->name));
    }
    if (laneHeaderSelectionActive_ && selectedLaneIds_.size() > 1) {
        return tr("signal selection restored to %1 signals · active %2 at %3")
            .arg(static_cast<qulonglong>(selectedLaneIds_.size()))
            .arg(QString::fromStdString(
                findLane(*scenario_, selectedLaneId_)->name))
            .arg(format(cursorTick_));
    }
    return tr("target restored to %1 at %2")
        .arg(QString::fromStdString(
            findLane(*scenario_, selectedLaneId_)->name))
        .arg(format(cursorTick_));
}

void WaveCanvas::beginCommandSelectionTransition(
    const std::uint64_t beforeStateId)
{
    pendingCommandSelectionTransition_ = std::pair{
        beforeStateId,
        historySelectionSnapshot(),
    };
}

void WaveCanvas::finishCommandSelectionTransition(
    const std::uint64_t afterStateId)
{
    if (!pendingCommandSelectionTransition_) return;
    rememberHistorySelectionTransition(
        pendingCommandSelectionTransition_->first,
        pendingCommandSelectionTransition_->second,
        afterStateId);
    pendingCommandSelectionTransition_.reset();
}

void WaveCanvas::cancelCommandSelectionTransition() noexcept
{
    pendingCommandSelectionTransition_.reset();
}

void WaveCanvas::setAsynchronousEditing(const bool enabled)
{
    if (asynchronousEditing_ == enabled) return;
    const auto cancelledDrag = drawing_ && tool_ == Tool::WaveEdit;
    clearWaveEditState();
    hideBusPresetPalette();
    asynchronousEditing_ = enabled;
    snapGuideTick_.reset();
    viewport()->update();
    auto message = enabled
        ? tr("Async editing · arbitrary tick offsets with light snapping · Alt bypasses snapping")
        : tr("Sync editing · waveform changes use one associated-clock beat");
    if (cancelledDrag) message.append(tr(" · waveform drag cancelled"));
    emit statusMessage(message);
}

bool WaveCanvas::commitLaneRename()
{
    if (!hasLaneRename()) return true;
    submitLaneRename();
    return !hasLaneRename();
}

bool WaveCanvas::commitPendingInlineEdits()
{
    durationEditBlurConsumesCanvasInput_ = false;
    if (hasQuickLaneSetup()) {
        submitQuickLaneSetup();
        if (hasQuickLaneSetup()) return false;
    }
    if (!commitLaneRename()) return false;
    if (durationEdit_ && durationEdit_->isVisible() && durationEdit_->isModified()) {
        submitDurationEdit();
        if (durationEdit_->isModified()
            || hasPendingBusValueEdit()
            || hasPendingRangeValueEdit()) {
            return false;
        }
    } else if (hasPendingBusValueEdit()) {
        submitBusValue();
        if (hasPendingBusValueEdit()) return false;
    }
    if (hasPendingRangeValueEdit()) {
        submitRangeValue();
        if (hasPendingRangeValueEdit()) return false;
    }
    return true;
}

void WaveCanvas::beginLaneRename(const QString& laneId, const QString& name)
{
    if (!laneRenameEdit_ || laneId.isEmpty() || hasQuickLaneSetup() || hasLaneRename()) return;
    const auto* lane = scenario_ ? findLane(*scenario_, laneId.toStdString()) : nullptr;
    const auto renamingGroup = lane && lane->kind == LaneKind::Group;
    laneRenameLaneId_ = laneId;
    laneRenameEdit_->setAccessibleName(
        renamingGroup ? tr("Group name") : tr("Signal name"));
    laneRenameEdit_->setPlaceholderText(
        renamingGroup ? tr("Group name") : tr("Signal name"));
    laneRenameEdit_->setText(name);
    laneRenameEdit_->setModified(false);
    laneRenameEdit_->setToolTip(tr("Enter or click elsewhere to apply · Esc to cancel"));
    laneRenameEdit_->setStyleSheet(QStringLiteral(
        "QLineEdit { color: #f4f7fb; background: #222b38;"
        " border: 1px solid #8fc3ff; border-radius: 4px; padding: 3px 6px;"
        " font-weight: 600; }"));
    laneRenameEdit_->show();
    positionLaneRename();
    laneRenameEdit_->raise();
    laneRenameEdit_->setFocus(Qt::OtherFocusReason);
    laneRenameEdit_->selectAll();
    QTimer::singleShot(0, laneRenameEdit_, [this, laneId] {
        if (hasLaneRename() && laneRenameLaneId_ == laneId) {
            laneRenameEdit_->setFocus(Qt::OtherFocusReason);
            laneRenameEdit_->selectAll();
        }
    });
    emit statusMessage(
        renamingGroup
            ? tr("Rename group %1 · Enter or click elsewhere to apply · Esc to cancel")
                  .arg(name)
            : tr("Rename signal %1 · Enter or click elsewhere to apply · Esc to cancel")
                  .arg(name));
    viewport()->update();
}

void WaveCanvas::finishLaneRename()
{
    if (!laneRenameEdit_) return;
    laneRenameClosing_ = true;
    laneRenameEdit_->hide();
    laneRenameEdit_->clearFocus();
    laneRenameLaneId_.clear();
    laneRenameClosing_ = false;
    viewport()->setFocus(Qt::OtherFocusReason);
    viewport()->update();
}

void WaveCanvas::showLaneRenameError(const QString& message)
{
    if (!hasLaneRename()) return;
    laneRenameEdit_->setStyleSheet(QStringLiteral(
        "QLineEdit { color: #fff1f1; background: #4b2d35;"
        " border: 1px solid #ef7773; border-radius: 4px; padding: 3px 6px;"
        " font-weight: 600; }"));
    laneRenameEdit_->setToolTip(message);
    laneRenameEdit_->setFocus(Qt::OtherFocusReason);
    laneRenameEdit_->selectAll();
    emit statusMessage(message);
}

void WaveCanvas::beginQuickLaneSetup(
    const QString& laneId,
    const LaneKind kind,
    const QString& name,
    const QString& parameter,
    const QStringList& clockLabels,
    const QStringList& clockIds,
    const QString& selectedClockId)
{
    if (!quickLaneSetupPanel_ || laneId.isEmpty()) return;
    hideBusPresetPalette();
    quickLaneSetupLaneId_ = laneId;
    quickLaneSetupKind_ = kind;
    quickLaneNameEdit_->setText(name);
    quickLaneNameEdit_->setModified(false);
    quickLaneParameterEdit_->setText(parameter);
    quickLaneParameterEdit_->setModified(false);
    quickLaneErrorLabel_->clear();
    quickLaneClockCombo_->clear();
    const auto count = std::min(clockLabels.size(), clockIds.size());
    for (qsizetype index = 0; index < count; ++index) {
        quickLaneClockCombo_->addItem(clockLabels.at(index), clockIds.at(index));
    }
    const auto selectedIndex = quickLaneClockCombo_->findData(selectedClockId);
    if (selectedIndex >= 0) quickLaneClockCombo_->setCurrentIndex(selectedIndex);
    quickLaneClockCombo_->setVisible(kind != LaneKind::Clock && count > 1);
    quickLaneParameterEdit_->setVisible(kind == LaneKind::Clock || kind == LaneKind::Bus);
    quickLaneParameterEdit_->setPlaceholderText(
        kind == LaneKind::Clock ? tr("Period, e.g. 2.5 ns") : tr("Width, e.g. 8"));
    if (auto* prompt = quickLaneSetupPanel_->findChild<QLabel*>(
            QStringLiteral("QuickLaneSetupPrompt"))) {
        prompt->setText(
            kind == LaneKind::Clock ? tr("New clock")
            : kind == LaneKind::Bus ? tr("New bus")
                                    : tr("New bit"));
    }
    quickLaneSetupPanel_->show();
    quickLaneSetupPanel_->raise();
    positionQuickLaneSetup();
    quickLaneNameEdit_->setFocus(Qt::OtherFocusReason);
    quickLaneNameEdit_->selectAll();
}

void WaveCanvas::finishQuickLaneSetup()
{
    if (!quickLaneSetupPanel_) return;
    quickLaneSetupPanel_->hide();
    quickLaneSetupLaneId_.clear();
    quickLaneErrorLabel_->clear();
    viewport()->setFocus(Qt::OtherFocusReason);
    viewport()->update();
}

void WaveCanvas::showQuickLaneSetupError(
    const QString& message,
    const bool focusParameter)
{
    if (!hasQuickLaneSetup()) return;
    quickLaneErrorLabel_->setText(message);
    quickLaneErrorLabel_->setToolTip(message);
    positionQuickLaneSetup();
    auto* target = focusParameter && quickLaneParameterEdit_->isVisible()
        ? quickLaneParameterEdit_
        : quickLaneNameEdit_;
    target->setFocus(Qt::OtherFocusReason);
    target->selectAll();
}

void WaveCanvas::showDurationEditError(const QString& message)
{
    if (!durationEdit_) return;
    durationEdit_->setStyleSheet(QStringLiteral(
        "QLineEdit { color: #fff2f2; background: #452d34;"
        " border: 1px solid #ef7773; border-radius: 4px; padding: 2px 5px; }"));
    durationEdit_->setToolTip(message);
    durationEdit_->setModified(true);
    durationEdit_->setFocus(Qt::OtherFocusReason);
    durationEdit_->selectAll();
    QTimer::singleShot(0, durationEdit_, [this] {
        if (!durationEdit_ || !durationEdit_->isModified()) return;
        durationEdit_->setFocus(Qt::OtherFocusReason);
        durationEdit_->selectAll();
    });
    emit statusMessage(message);
}

bool WaveCanvas::event(QEvent* event)
{
    if (event->type() == QEvent::KeyPress && tool_ == Tool::WaveEdit) {
        const auto* keyEvent = static_cast<QKeyEvent*>(event);
        const auto forward = keyEvent->key() == Qt::Key_Tab
            && keyEvent->modifiers() == Qt::NoModifier;
        const auto backward = (keyEvent->key() == Qt::Key_Backtab
            && keyEvent->modifiers() == Qt::ShiftModifier)
            || (keyEvent->key() == Qt::Key_Tab
                && keyEvent->modifiers() == Qt::ShiftModifier);
        if ((forward || backward)
            && navigateSelectedBeat(forward)) {
            event->accept();
            return true;
        }
    }
    return QAbstractScrollArea::event(event);
}

bool WaveCanvas::eventFilter(QObject* watched, QEvent* event)
{
    auto busEditAction = std::optional<BusEditAction>{};
    if (watched == busApplyButton_) {
        busEditAction = BusEditAction::ApplyDraft;
    } else if (watched == busClearButton_) {
        busEditAction = BusEditAction::Clear;
    } else if (watched == busPresetButtons_.at(0)) {
        busEditAction = BusEditAction::PresetZero;
    } else if (watched == busPresetButtons_.at(1)) {
        busEditAction = BusEditAction::PresetReserved;
    } else if (watched == busPresetButtons_.at(2)) {
        busEditAction = BusEditAction::PresetX;
    } else if (watched == busPresetButtons_.at(3)) {
        busEditAction = BusEditAction::PresetZ;
    } else if (watched == busPresetButtons_.at(4)) {
        busEditAction = BusEditAction::PresetDontCare;
    }
    if (busEditAction
        && (event->type() == QEvent::Enter
            || event->type() == QEvent::Leave)) {
        if (event->type() == QEvent::Enter) {
            static_cast<void>(previewBusEditAction(*busEditAction));
        } else {
            restoreBusEditDraftPreview();
        }
    }
    if (watched == rangeRepeatButton_
        && (event->type() == QEvent::Enter
            || event->type() == QEvent::Leave)) {
        const auto active = event->type() == QEvent::Enter
            && rangeEditPaletteVisible_
            && explicitRangeSelection_;
        if (rangeRepeatPreviewActive_ != active) {
            rangeRepeatPreviewActive_ = active;
            viewport()->update();
        }
    }
    if (watched == rangeEditContextLabel_
        && event->type() == QEvent::MouseButtonRelease) {
        const auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton
            && explicitRangeSelection_) {
            emit exactRangeTimeEditRequested();
            return true;
        }
    }
    if (event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        const auto acceptKey = keyEvent->key() == Qt::Key_Return
            || keyEvent->key() == Qt::Key_Enter;
        const auto quickEditor = watched == quickLaneNameEdit_
            || watched == quickLaneParameterEdit_
            || watched == quickLaneClockCombo_;
        if (quickEditor && acceptKey) {
            submitQuickLaneSetup();
            return true;
        }
        if (quickEditor && keyEvent->key() == Qt::Key_Escape) {
            cancelQuickLaneSetup();
            return true;
        }
        if (watched == laneRenameEdit_ && acceptKey) {
            submitLaneRename();
            return true;
        }
        if (watched == laneRenameEdit_ && keyEvent->key() == Qt::Key_Escape) {
            cancelLaneRename();
            return true;
        }
        if (watched == durationEdit_ && acceptKey) {
            submitDurationEdit();
            return true;
        }
        if (watched == durationEdit_ && keyEvent->key() == Qt::Key_Escape) {
            durationEditMouseFocusOut_ = false;
            durationEditBlurConsumesCanvasInput_ = false;
            durationEdit_->setModified(false);
            durationEdit_->clearFocus();
            syncDurationEditor();
            return true;
        }
        const auto busNextBeat = watched == busValueEdit_
            && busEditScope_ == BusEditScope::Beat
            && keyEvent->key() == Qt::Key_Tab
            && keyEvent->modifiers() == Qt::NoModifier;
        const auto busPreviousBeat = watched == busValueEdit_
            && busEditScope_ == BusEditScope::Beat
            && (keyEvent->key() == Qt::Key_Backtab
                || (keyEvent->key() == Qt::Key_Tab
                    && keyEvent->modifiers() == Qt::ShiftModifier));
        const auto busNextSegment = watched == busValueEdit_
            && busEditScope_ == BusEditScope::Segment
            && keyEvent->key() == Qt::Key_Tab
            && keyEvent->modifiers() == Qt::NoModifier;
        const auto busPreviousSegment = watched == busValueEdit_
            && busEditScope_ == BusEditScope::Segment
            && (keyEvent->key() == Qt::Key_Backtab
                || (keyEvent->key() == Qt::Key_Tab
                    && keyEvent->modifiers() == Qt::ShiftModifier));
        if (watched == busValueEdit_
            && keyEvent->modifiers() == Qt::ControlModifier
            && (keyEvent->key() == Qt::Key_Up
                || keyEvent->key() == Qt::Key_Down)
            && cycleBusRecentValue(keyEvent->key() == Qt::Key_Down)) {
            return true;
        }
        if (watched == busValueEdit_
            && keyEvent->modifiers() == Qt::NoModifier
            && (keyEvent->key() == Qt::Key_Up
                || keyEvent->key() == Qt::Key_Down)
            && cycleEnumEditorSymbol(keyEvent->key() == Qt::Key_Down)) {
            return true;
        }
        if (watched == busValueEdit_
            && keyEvent->modifiers() == Qt::NoModifier
            && (keyEvent->key() == Qt::Key_Up
                || keyEvent->key() == Qt::Key_Down)
            && stepBusEditorValue(keyEvent->key() == Qt::Key_Up)) {
            return true;
        }
        if (watched == busValueEdit_
            && acceptKey
            && keyEvent->modifiers() == Qt::ControlModifier) {
            submitBusValue(BusEditCommitAction::Stay);
            return true;
        }
        if (busNextBeat || busPreviousBeat) {
            submitBusValue(
                busNextBeat
                    ? BusEditCommitAction::NextBeat
                    : BusEditCommitAction::PreviousBeat);
            return true;
        }
        if (busNextSegment || busPreviousSegment) {
            submitBusValue(
                busNextSegment
                    ? BusEditCommitAction::NextSegment
                    : BusEditCommitAction::PreviousSegment);
            return true;
        }
        if (watched == busValueEdit_ && acceptKey) {
            submitBusValue();
            return true;
        }
        if (watched == busValueEdit_ && keyEvent->key() == Qt::Key_Escape) {
            cancelBusValueEdit();
            viewport()->setFocus(Qt::OtherFocusReason);
            return true;
        }
        const auto rangeNextTarget = watched == rangeValueEdit_
            && keyEvent->key() == Qt::Key_Tab
            && keyEvent->modifiers() == Qt::NoModifier;
        const auto rangePreviousTarget = watched == rangeValueEdit_
            && ((keyEvent->key() == Qt::Key_Backtab
                 && keyEvent->modifiers() == Qt::ShiftModifier)
                || (keyEvent->key() == Qt::Key_Tab
                    && keyEvent->modifiers() == Qt::ShiftModifier));
        if ((rangeNextTarget || rangePreviousTarget)
            && hasActiveRangeSequenceTokenMapping()) {
            static_cast<void>(
                navigateRangeSequenceTarget(
                    rangeNextTarget));
            return true;
        }
        if (watched == rangeValueEdit_ && acceptKey) {
            submitRangeValue();
            return true;
        }
        if (watched == rangeValueEdit_ && keyEvent->key() == Qt::Key_Escape) {
            if (!rangeValueEdit_->isModified()
                && rangeValueEdit_->property(
                    "loadedExisting").toBool()) {
                rangeValueEdit_->setText({});
                rangeValueEdit_->setModified(false);
                rangeValueEdit_->setProperty(
                    "loadedExisting",
                    false);
                rangeSequenceTextSpans_.clear();
                clearRangeSequenceCaretTarget();
                clearRangeSequenceBaseline();
                clearRangeSequenceAnchor();
                rangeValueEdit_->style()->unpolish(
                    rangeValueEdit_);
                rangeValueEdit_->style()->polish(
                    rangeValueEdit_);
                showRangeEditPalette();
                rangeValueEdit_->setFocus(
                    Qt::OtherFocusReason);
                emit statusMessage(
                    tr("Current beat values hidden · selection kept · Esc again closes the range"));
                return true;
            }
            if (rangeValueEdit_->isModified()) {
                if (restoreRangeSequenceBaseline()) {
                    emit statusMessage(
                        tr("Draft discarded · loaded current values restored · selection kept · Esc again hides them"));
                    return true;
                }
                rangeValueEdit_->setText({});
                rangeValueEdit_->setModified(false);
                rangeValueEdit_->setProperty(
                    "loadedExisting",
                    false);
                rangeSequenceTextSpans_.clear();
                clearRangeSequenceCaretTarget();
                clearRangeSequenceBaseline();
                clearRangeSequenceAnchor();
                rangeValueEdit_->setStyleSheet({});
                showRangeEditPalette();
                rangeValueEdit_->setFocus(Qt::OtherFocusReason);
                emit statusMessage(
                    explicitRangeKind() == LaneKind::Bit
                        ? tr("Bit pattern draft discarded · selection kept · Esc again closes the range")
                        : tr("Range value draft discarded · selection kept · Esc again closes the range"));
                return true;
            }
            if (rangeCloseButton_) rangeCloseButton_->click();
            viewport()->setFocus(Qt::OtherFocusReason);
            return true;
        }
    }
    if (watched == laneRenameEdit_
        && event->type() == QEvent::FocusOut
        && hasLaneRename()
        && !laneRenameClosing_) {
        QTimer::singleShot(0, this, [this] {
            if (hasLaneRename() && !laneRenameEdit_->hasFocus()) submitLaneRename();
        });
    }
    return QAbstractScrollArea::eventFilter(watched, event);
}

void WaveCanvas::submitQuickLaneSetup()
{
    if (!hasQuickLaneSetup()) return;
    const auto laneId = quickLaneSetupLaneId_;
    const auto name = quickLaneNameEdit_->text().trimmed();
    const auto parameter = quickLaneParameterEdit_->isVisible()
        ? quickLaneParameterEdit_->text().trimmed()
        : QString{};
    const auto clockId = quickLaneClockCombo_->isVisible()
        ? quickLaneClockCombo_->currentData().toString()
        : QString{};
    emit quickLaneSetupAccepted(laneId, name, parameter, clockId);
}

void WaveCanvas::cancelQuickLaneSetup()
{
    if (!hasQuickLaneSetup()) return;
    const auto laneId = quickLaneSetupLaneId_;
    emit quickLaneSetupCanceled(laneId);
}

void WaveCanvas::submitLaneRename()
{
    if (!hasLaneRename()) return;
    const auto laneId = laneRenameLaneId_;
    const auto name = laneRenameEdit_->text().trimmed();
    emit laneRenameAccepted(laneId, name);
}

void WaveCanvas::cancelLaneRename()
{
    if (!hasLaneRename()) return;
    QString laneName;
    auto renamingGroup = false;
    if (scenario_) {
        if (const auto* lane = findLane(*scenario_, laneRenameLaneId_.toStdString())) {
            laneName = QString::fromStdString(lane->name);
            renamingGroup = lane->kind == LaneKind::Group;
        }
    }
    finishLaneRename();
    emit statusMessage(
        laneName.isEmpty()
            ? tr("Rename cancelled")
            : renamingGroup
                ? tr("Rename cancelled · selected group %1")
                      .arg(laneName)
                : tr("Rename cancelled · selected signal %1")
                      .arg(laneName));
}

void WaveCanvas::submitDurationEdit(const bool preserveMouseFocusTarget)
{
    if (!durationEdit_ || durationEditSubmitting_) return;
    durationEditSubmitting_ = true;
    if (hasQuickLaneSetup()) {
        submitQuickLaneSetup();
        if (hasQuickLaneSetup()) {
            durationEditSubmitting_ = false;
            return;
        }
    }
    const auto busWasOutsideTimeline = hasPendingBusValueEdit()
        && scenario_
        && (scenario_->duration <= 0
            || *busPresetAnchorTick_ < 0
            || *busPresetAnchorTick_ >= scenario_->duration);
    if (hasPendingBusValueEdit() && !busWasOutsideTimeline) {
        submitBusValue();
        if (hasPendingBusValueEdit()) {
            durationEditSubmitting_ = false;
            return;
        }
        durationEdit_->setFocus(Qt::OtherFocusReason);
    }
    const auto value = durationEdit_->text();
    durationEdit_->setModified(false);
    emit durationEditRequested(value);
    const auto durationAccepted = !durationEdit_->isModified();
    if (durationAccepted && busWasOutsideTimeline && hasPendingBusValueEdit()) {
        submitBusValue();
    }
    const auto fullyAccepted = durationAccepted && !hasPendingBusValueEdit();
    if (fullyAccepted && !preserveMouseFocusTarget) {
        viewport()->setFocus(Qt::OtherFocusReason);
    }
    durationEditSubmitting_ = false;
    if (!durationAccepted) return;
    if (preserveMouseFocusTarget && fullyAccepted) {
        QTimer::singleShot(0, this, [this] {
            if (durationEdit_ && !durationEdit_->isModified()) syncDurationEditor();
        });
    } else {
        syncDurationEditor();
    }
}

bool WaveCanvas::hasPendingBusValueEdit() const noexcept
{
    return busValueEdit_
        && busValueEdit_->isModified()
        && !busPresetLaneId_.empty()
        && busPresetAnchorTick_;
}

bool WaveCanvas::hasPendingRangeValueEdit() const noexcept
{
    const auto kind = explicitRangeKind();
    return rangeValueEdit_
        && rangeValueEdit_->isModified()
        && rangeEditPaletteVisible_
        && explicitRangeSelection_
        && kind
        && (*kind == LaneKind::Bus
            || *kind == LaneKind::Enum
            || *kind == LaneKind::Bit);
}

bool WaveCanvas::hasPendingValueEdit() const noexcept
{
    return (durationEdit_ && durationEdit_->isModified())
        || hasPendingBusValueEdit()
        || hasPendingRangeValueEdit();
}

void WaveCanvas::syncDurationEditor()
{
    if (!durationEdit_ || !durationLabel_) return;
    const auto available = project_ && scenario_;
    durationEdit_->setVisible(available);
    durationLabel_->setVisible(available);
    if (!available || durationEdit_->hasFocus() || durationEdit_->isModified()) return;
    durationEdit_->setText(QString::fromStdString(
        formatTick(scenario_->duration, project_->timeBase)));
    durationEdit_->setModified(false);
    durationEdit_->setToolTip(
        tr("Edit the timeline end, for example 500 ns or 500.5 ns, then press Enter"));
    durationEdit_->setStyleSheet(QStringLiteral(
        "QLineEdit { color: #edf2f8; background: #2d3949;"
        " border: 1px solid #65758b; border-radius: 4px; padding: 2px 5px; }"
        "QLineEdit:focus { border-color: #8fc3ff; }"));
}

void WaveCanvas::setDocument(
    Project* project,
    Scenario* scenario,
    CommandStack* commandStack)
{
    pendingVisibleTimeSpanRestore_.reset();
    pendingVisibleTimeSpanRestoreScheduled_ = false;
    if (scenario_ && scenario_ != scenario) {
        documentContexts_[scenario_] = {
            historySelectionSnapshot(),
            historySelectionTransitions_,
            pixelsPerTick_,
            horizontalScrollBar()->value(),
            verticalScrollBar()->value(),
            collapsedGroupIds_,
        };
    }
    const auto savedContext = scenario
        ? documentContexts_.find(scenario)
        : documentContexts_.end();
    const auto restoreContext = savedContext != documentContexts_.end();

    stopLaneDragAutoScroll();
    stopWaveEditDragAutoScroll();
    if (headerResizing_) setSignalHeaderWidth(headerResizeOriginalWidth_);
    if (quickLaneSetupPanel_) quickLaneSetupPanel_->hide();
    quickLaneSetupLaneId_.clear();
    laneRenameClosing_ = true;
    if (laneRenameEdit_) laneRenameEdit_->hide();
    laneRenameLaneId_.clear();
    laneRenameClosing_ = false;
    durationEditMouseFocusOut_ = false;
    durationEditBlurConsumesCanvasInput_ = false;
    project_ = project;
    scenario_ = scenario;
    commandStack_ = commandStack;
    pendingCommandSelectionTransition_.reset();
    historySelectionTransitions_.clear();
    selectedLaneId_.clear();
    selectedLaneIds_.clear();
    collapsedGroupIds_.clear();
    rangeSequenceTextSpans_.clear();
    clearRangeSequenceCaretTarget();
    selectionRange_.reset();
    movableCursorTick_.reset();
    clearWaveEditState();
    temporaryCursorTick_.reset();
    selectedMarkerId_.clear();
    cursorInteraction_ = CursorInteraction::None;
    laneHeaderPressed_ = false;
    laneHeaderDragging_ = false;
    laneHeaderPressPreservesMultiSelection_ = false;
    laneHeaderSelectionActive_ = false;
    rulerScrubbing_ = false;
    rulerScrubClearedRange_ = false;
    rulerScrubOriginalSelection_.reset();
    headerResizing_ = false;
    laneDragId_.clear();
    laneDragIds_.clear();
    laneDropInsertionSlot_.reset();
    laneDropDestinationIndex_.reset();
    laneDropIndicatorY_.reset();
    laneDropGroupId_.clear();
    lockedMarkerOriginalRange_.reset();
    hideBusPresetPalette();
    if (restoreContext) {
        const auto& context = savedContext->second;
        pixelsPerTick_ = std::clamp(context.pixelsPerTick, 1.0e-9, 100.0);
        collapsedGroupIds_ = context.collapsedGroupIds;
        sanitizeCollapsedGroups();
        historySelectionTransitions_ = context.historySelectionTransitions;
        selectedLaneId_ = context.selection.selectedLaneId;
        selectedLaneIds_ = context.selection.selectedLaneIds;
        selectedSegmentLaneId_ = context.selection.selectedSegmentLaneId;
        selectedSegmentId_ = context.selection.selectedSegmentId;
        selectionRange_ = context.selection.selectionRange;
        cursorTick_ = scenario_
            ? std::clamp<Tick>(
                  context.selection.cursorTick,
                  0,
                  scenario_->duration)
            : Tick{0};
        explicitRangeSelection_ = context.selection.explicitRangeSelection;
        laneHeaderSelectionActive_ =
            context.selection.laneHeaderSelectionActive;
        laneHeaderSelectionAnchorId_ = laneHeaderSelectionActive_
            ? selectedLaneId_
            : std::string{};

        const auto validLane = [this](const std::string& laneId) {
            const auto* lane = scenario_ ? findLane(*scenario_, laneId) : nullptr;
            return lane && isLaneDisplayed(*lane);
        };
        if ((!selectedLaneId_.empty() && !validLane(selectedLaneId_))
            || !std::all_of(
                selectedLaneIds_.begin(),
                selectedLaneIds_.end(),
                validLane)) {
            selectedLaneId_.clear();
            selectedLaneIds_.clear();
            selectedSegmentLaneId_.clear();
            selectedSegmentId_.clear();
            selectionRange_.reset();
            explicitRangeSelection_ = false;
            laneHeaderSelectionActive_ = false;
            laneHeaderSelectionAnchorId_.clear();
        }
        if (selectionRange_
            && (!scenario_
                || selectionRange_->first < 0
                || selectionRange_->second <= selectionRange_->first
                || selectionRange_->second > scenario_->duration)) {
            selectionRange_.reset();
            explicitRangeSelection_ = false;
        }
        if (!selectedSegmentId_.empty()
            && !segmentById(selectedSegmentLaneId_, selectedSegmentId_)) {
            selectedSegmentLaneId_.clear();
            selectedSegmentId_.clear();
        }
    }
    rebuildLaneLayout();
    syncDurationEditor();
    positionDurationEditor();
    fitPending_ = !restoreContext;
    if (restoreContext) {
        horizontalScrollBar()->setValue(savedContext->second.horizontalScroll);
        verticalScrollBar()->setValue(savedContext->second.verticalScroll);
        if (explicitRangeSelection_ && selectionRange_) {
            showRangeEditPalette();
        } else {
            hideRangeEditPalette();
        }
        positionBusPresetPalette();
        viewport()->update();
    } else if (viewport()->width() > headerWidth_ + 40) {
        fitPending_ = false;
        fitScenario();
    }
}

void WaveCanvas::clearDocumentContexts()
{
    documentContexts_.clear();
    collapsedGroupIds_.clear();
    rangeSequenceTextSpans_.clear();
    clearRangeSequenceCaretTarget();
    pendingVisibleTimeSpanRestore_.reset();
    pendingVisibleTimeSpanRestoreScheduled_ = false;
    pendingCommandSelectionTransition_.reset();
    project_ = nullptr;
    scenario_ = nullptr;
    commandStack_ = nullptr;
}

bool WaveCanvas::hasDocumentContext(const Scenario* scenario) const noexcept
{
    return scenario && documentContexts_.contains(scenario);
}

void WaveCanvas::setTool(const Tool tool)
{
    const auto restoreWaveEditViewport = tool_ == Tool::WaveEdit
        && drawing_
        && waveEditDragAutoScrolled_;
    stopLaneDragAutoScroll();
    stopWaveEditDragAutoScroll();
    if (laneHeaderPressed_ || laneHeaderDragging_) {
        verticalScrollBar()->setValue(laneDragOriginalVerticalScroll_);
    }
    if (restoreWaveEditViewport) {
        horizontalScrollBar()->setValue(waveEditDragOriginalHorizontalScroll_);
        verticalScrollBar()->setValue(waveEditDragOriginalVerticalScroll_);
    }
    if (headerResizing_) setSignalHeaderWidth(headerResizeOriginalWidth_);
    const auto previousTool = tool_;
    tool_ = tool;
    drawing_ = false;
    panning_ = false;
    rulerScrubbing_ = false;
    rulerScrubClearedRange_ = false;
    rulerScrubOriginalSelection_.reset();
    spaceHeld_ = false;
    bypassSnap_ = false;
    snapGuideTick_.reset();
    cursorInteraction_ = CursorInteraction::None;
    lockedMarkerOriginalRange_.reset();
    laneHeaderPressed_ = false;
    laneHeaderDragging_ = false;
    laneHeaderPressPreservesMultiSelection_ = false;
    headerResizing_ = false;
    laneDragId_.clear();
    laneDragIds_.clear();
    laneDropInsertionSlot_.reset();
    laneDropDestinationIndex_.reset();
    laneDropIndicatorY_.reset();
    laneDropGroupId_.clear();
    activeEventId_.clear();
    if (previousTool == Tool::Marker && tool != Tool::Marker) {
        movableCursorTick_.reset();
        temporaryCursorTick_.reset();
        selectedMarkerId_.clear();
    }
    if (previousTool == Tool::WaveEdit && tool != Tool::WaveEdit) {
        clearWaveEditState();
        hideBusPresetPalette();
    }
    viewport()->setCursor(defaultCursorShape());
    viewport()->update();
}

Qt::CursorShape WaveCanvas::defaultCursorShape() const noexcept
{
    if (tool_ == Tool::Selection) return Qt::ArrowCursor;
    if (tool_ == Tool::WaveEdit) return Qt::PointingHandCursor;
    return Qt::CrossCursor;
}

WaveCanvas::Tool WaveCanvas::tool() const noexcept
{
    return tool_;
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

bool WaveCanvas::hasLaneHeaderSelection() const noexcept
{
    return laneHeaderSelectionActive_;
}

std::pair<bool, QString> WaveCanvas::selectedLanePasteAvailability() const
{
    if (!laneHeaderSelectionActive_ || selectedLaneIds_.empty()) {
        return {
            false,
            tr("Paste unavailable · select one or more signal names first"),
        };
    }
    const auto availability = pasteAvailabilityWithImpact(
        selectedLaneIds_,
        cursorTick_);
    return {availability.enabled, availability.toolTip};
}

std::pair<bool, QString> WaveCanvas::selectedRangeClearAvailability() const
{
    const auto availability = rangeClearAvailability();
    return {availability.enabled, availability.toolTip};
}

std::pair<bool, QString> WaveCanvas::selectedRangeRepeatAvailability() const
{
    const auto availability = rangeRepeatAvailability();
    return {availability.enabled, availability.toolTip};
}

std::size_t
WaveCanvas::selectedRangeRepeatRelationRemovalCount() const
{
    return rangeRepeatAvailability().relationImpact.ids.size();
}

std::optional<std::pair<Tick, Tick>>
WaveCanvas::laneHeaderPastePreviewRange() const
{
    const auto preview = laneHeaderPastePreview();
    if (!preview) return std::nullopt;
    return std::pair{
        preview->start,
        preview->start + preview->duration,
    };
}

QStringList WaveCanvas::laneHeaderPastePreviewTargetLaneIds() const
{
    const auto preview = laneHeaderPastePreview();
    QStringList result;
    if (!preview) return result;
    result.reserve(static_cast<qsizetype>(preview->lanes.size()));
    for (const auto& lane : preview->lanes) {
        result.append(QString::fromStdString(lane.id));
    }
    return result;
}

std::size_t WaveCanvas::laneHeaderPastePreviewRelationRemovalCount() const
{
    const auto preview = laneHeaderPastePreview();
    return preview ? preview->relationRemovalCount : 0;
}

QStringList
WaveCanvas::laneHeaderPastePreviewRelationRemovalSummaries() const
{
    const auto preview = laneHeaderPastePreview();
    return preview ? preview->relationRemovalSummaries : QStringList{};
}

std::optional<std::pair<Tick, Tick>>
WaveCanvas::pastePreviewRange() const
{
    const auto preview = pastePreview();
    if (!preview) return std::nullopt;
    return std::pair{
        preview->start,
        preview->start + preview->duration,
    };
}

QStringList WaveCanvas::pastePreviewTargetLaneIds() const
{
    const auto preview = pastePreview();
    if (!preview) return {};
    QStringList result;
    result.reserve(static_cast<qsizetype>(preview->lanes.size()));
    for (const auto& lane : preview->lanes) {
        result.append(QString::fromStdString(lane.id));
    }
    return result;
}

std::size_t WaveCanvas::pastePreviewRelationRemovalCount() const
{
    const auto preview = pastePreview();
    return preview ? preview->relationRemovalCount : 0;
}

QStringList WaveCanvas::pastePreviewRelationRemovalSummaries() const
{
    const auto preview = pastePreview();
    return preview ? preview->relationRemovalSummaries : QStringList{};
}

std::optional<std::pair<Tick, Tick>>
WaveCanvas::repeatPreviewRange() const
{
    const auto preview = buildRepeatPreview();
    if (!preview) return std::nullopt;
    return std::pair{
        preview->start,
        preview->start + preview->duration,
    };
}

QStringList WaveCanvas::repeatPreviewTargetLaneIds() const
{
    const auto preview = buildRepeatPreview();
    if (!preview) return {};
    QStringList result;
    result.reserve(static_cast<qsizetype>(preview->lanes.size()));
    for (const auto& lane : preview->lanes) {
        result.append(QString::fromStdString(lane.id));
    }
    return result;
}

std::size_t WaveCanvas::repeatPreviewRelationRemovalCount() const
{
    const auto preview = buildRepeatPreview();
    return preview ? preview->relationRemovalCount : 0;
}

QStringList WaveCanvas::repeatPreviewRelationRemovalSummaries() const
{
    const auto preview = buildRepeatPreview();
    return preview ? preview->relationRemovalSummaries : QStringList{};
}

Tick WaveCanvas::cursorTick() const noexcept
{
    return cursorTick_;
}

Tick WaveCanvas::visibleTimeSpan() const noexcept
{
    if (!scenario_ || scenario_->duration <= 0
        || !std::isfinite(pixelsPerTick_) || pixelsPerTick_ <= 0.0) {
        return 0;
    }
    if (pendingVisibleTimeSpanRestore_) {
        return std::clamp<Tick>(
            pendingVisibleTimeSpanRestore_->first,
            1,
            scenario_->duration);
    }
    const auto rawSpan = static_cast<long double>(waveViewportWidth())
        / static_cast<long double>(pixelsPerTick_);
    return std::clamp<Tick>(
        static_cast<Tick>(std::llround(rawSpan)),
        1,
        scenario_->duration);
}

bool WaveCanvas::restoreVisibleTimeSpan(
    const Tick span,
    const Tick anchorTick)
{
    if (!scenario_ || scenario_->duration <= 0 || span <= 0) return false;
    const auto clampedSpan = std::clamp<Tick>(
        span,
        1,
        scenario_->duration);
    const auto clampedAnchor = std::clamp<Tick>(
        anchorTick,
        0,
        scenario_->duration);
    pendingVisibleTimeSpanRestore_ = std::pair{
        clampedSpan,
        clampedAnchor,
    };
    fitPending_ = false;
    const auto applied = applyVisibleTimeSpan(clampedSpan, clampedAnchor);
    if (isVisible()) schedulePendingVisibleTimeSpanRestore();
    return applied;
}

bool WaveCanvas::applyVisibleTimeSpan(
    const Tick span,
    const Tick anchorTick)
{
    if (!scenario_ || scenario_->duration <= 0 || span <= 0) return false;
    const auto clampedSpan = std::clamp<Tick>(
        span,
        1,
        scenario_->duration);
    const auto clampedAnchor = std::clamp<Tick>(
        anchorTick,
        0,
        scenario_->duration);
    pixelsPerTick_ = std::clamp(
        static_cast<double>(waveViewportWidth())
            / static_cast<double>(clampedSpan),
        1.0e-9,
        100.0);
    updateScrollBars();
    const auto desiredScroll =
        static_cast<double>(clampedAnchor) * pixelsPerTick_
        - static_cast<double>(waveViewportWidth()) / 2.0;
    horizontalScrollBar()->setValue(static_cast<int>(std::clamp(
        std::llround(desiredScroll),
        0LL,
        static_cast<long long>(horizontalScrollBar()->maximum()))));
    positionBusPresetPalette();
    viewport()->update();
    return true;
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

bool WaveCanvas::hasExplicitRangeSelection() const noexcept
{
    return explicitRangeSelection_;
}

QString WaveCanvas::selectedSegmentLaneId() const
{
    return QString::fromStdString(selectedSegmentLaneId_);
}

QString WaveCanvas::selectedSegmentId() const
{
    return QString::fromStdString(selectedSegmentId_);
}

QString WaveCanvas::hoveredBitBeatLaneId() const
{
    return QString::fromStdString(waveEditHoverLaneId_);
}

std::optional<std::pair<Tick, Tick>> WaveCanvas::hoveredBitBeatRange() const noexcept
{
    return waveEditHoverRange_;
}

std::optional<std::size_t> WaveCanvas::laneDropDestinationIndex() const noexcept
{
    return laneDropDestinationIndex_;
}

QString WaveCanvas::laneDropGroupId() const
{
    return laneHeaderDragging_
        ? QString::fromStdString(laneDropGroupId_)
        : QString{};
}

std::optional<Tick> WaveCanvas::waveEditTransitionPreviewTick() const noexcept
{
    return waveEditInteraction_ == WaveEditInteraction::MoveTransition && drawing_
        ? std::optional<Tick>{drawCurrent_}
        : std::nullopt;
}

std::optional<std::pair<Tick, Tick>>
WaveCanvas::waveEditTransitionPreviewRange() const noexcept
{
    if (!scenario_
        || !drawing_
        || waveEditInteraction_ != WaveEditInteraction::MoveTransition
        || activeEventId_.empty()) {
        return std::nullopt;
    }
    const auto* event = findEvent(*scenario_, activeEventId_);
    const auto* lane = event ? findLane(*scenario_, event->laneId) : nullptr;
    if (!event || !lane || lane->kind != LaneKind::Bit) return std::nullopt;
    const auto segment = std::find_if(
        lane->segments.begin(),
        lane->segments.end(),
        [event](const Segment& candidate) {
            return candidate.id == event->linkedSegmentId;
        });
    if (segment == lane->segments.end()) return std::nullopt;
    const auto start = std::min(event->tick, drawCurrent_);
    return segment->end > start
        ? std::optional<std::pair<Tick, Tick>>{{start, segment->end}}
        : std::nullopt;
}

std::optional<std::pair<Tick, Tick>>
WaveCanvas::waveEditRangeTransferPreviewRange() const noexcept
{
    return drawing_
            && waveEditInteraction_ == WaveEditInteraction::MoveRange
        ? waveEditPreviewRange_
        : std::nullopt;
}

bool WaveCanvas::waveEditRangeTransferCopies() const noexcept
{
    return drawing_
        && waveEditInteraction_ == WaveEditInteraction::MoveRange
        && waveEditCopyDrag_;
}

QStringList WaveCanvas::waveEditRangeTransferTargetLaneIds() const
{
    QStringList result;
    result.reserve(
        static_cast<qsizetype>(waveEditRangeTargetLaneIds_.size()));
    for (const auto& laneId : waveEditRangeTargetLaneIds_) {
        result.push_back(QString::fromStdString(laneId));
    }
    return result;
}

bool WaveCanvas::waveEditRangeTransferTargetValid() const noexcept
{
    return !drawing_
        || waveEditInteraction_ != WaveEditInteraction::MoveRange
        || waveEditRangeTargetValid_;
}

QString WaveCanvas::waveEditRangeTransferTargetError() const
{
    return drawing_
            && waveEditInteraction_ == WaveEditInteraction::MoveRange
        ? waveEditRangeTargetError_
        : QString{};
}

bool WaveCanvas::waveEditRangeTransferChangesModel() const
{
    const auto projection = buildRangeTransferProjection();
    return projection && projection->modelChanges;
}

bool WaveCanvas::waveEditRangeTransferExtendsEnd() const
{
    const auto projection = buildRangeTransferProjection();
    return projection && projection->extendsEnd;
}

std::size_t
WaveCanvas::waveEditRangeTransferRelationRemovalCount() const
{
    const auto projection = buildRangeTransferProjection();
    return projection ? projection->relationImpact.ids.size() : 0;
}

QStringList
WaveCanvas::waveEditRangeTransferRelationRemovalSummaries() const
{
    const auto projection = buildRangeTransferProjection();
    return projection
        ? projection->relationImpact.summaries
        : QStringList{};
}

bool WaveCanvas::waveEditSegmentChangesModel() const
{
    const auto projection = activeSegmentEditProjection();
    return projection && projection->modelChanges;
}

std::size_t
WaveCanvas::waveEditSegmentRelationRemovalCount() const
{
    const auto projection = activeSegmentEditProjection();
    return projection ? projection->relationImpact.ids.size() : 0;
}

QStringList
WaveCanvas::waveEditSegmentRelationRemovalSummaries() const
{
    const auto projection = activeSegmentEditProjection();
    return projection
        ? projection->relationImpact.summaries
        : QStringList{};
}

std::optional<std::pair<Tick, Tick>>
WaveCanvas::waveEditSegmentPreviewRange() const
{
    const auto projection = activeSegmentEditProjection();
    return projection
        ? std::optional<std::pair<Tick, Tick>>{
              std::pair{projection->start, projection->end}}
        : std::nullopt;
}

WaveCanvas::SegmentActionState
WaveCanvas::selectedSegmentActionState(const SegmentAction action) const
{
    return assessSelectedSegmentAction(action).state;
}

bool WaveCanvas::previewSelectedSegmentAction(const SegmentAction action)
{
    const auto assessment = assessSelectedSegmentAction(action);
    segmentActionPreview_ = assessment.projection;
    emit statusMessage(assessment.state.summary);
    viewport()->update();
    return assessment.state.valid;
}

void WaveCanvas::clearSelectedSegmentActionPreview()
{
    if (!segmentActionPreview_) return;
    segmentActionPreview_.reset();
    viewport()->update();
}

WaveCanvas::BusEditActionState
WaveCanvas::busEditActionState(const BusEditAction action) const
{
    return assessBusEditAction(action).state;
}

bool WaveCanvas::previewBusEditAction(const BusEditAction action)
{
    const auto assessment = assessBusEditAction(action);
    busEditActionPreview_ = assessment.projection;
    busEditPreviewAction_ = assessment.projection
        ? std::optional<BusEditAction>{action}
        : std::nullopt;
    updateBusEditContextLabel(&assessment.state);
    emit statusMessage(assessment.state.summary);
    viewport()->update();
    return assessment.state.valid;
}

void WaveCanvas::clearBusEditActionPreview()
{
    if (!busEditActionPreview_ && !busEditPreviewAction_) return;
    busEditActionPreview_.reset();
    busEditPreviewAction_.reset();
    updateBusEditContextLabel();
    viewport()->update();
}

std::optional<std::pair<Tick, Tick>>
WaveCanvas::busEditPreviewRange() const
{
    return busEditActionPreview_
        ? std::optional<std::pair<Tick, Tick>>{
              std::pair{
                  busEditActionPreview_->start,
                  busEditActionPreview_->end}}
        : std::nullopt;
}

bool WaveCanvas::busEditPreviewChangesModel() const
{
    return busEditActionPreview_
        && busEditActionPreview_->modelChanges;
}

std::size_t WaveCanvas::busEditPreviewRelationRemovalCount() const
{
    return busEditActionPreview_
        ? busEditActionPreview_->relationImpact.ids.size()
        : 0;
}

QStringList WaveCanvas::busEditPreviewRelationRemovalSummaries() const
{
    return busEditActionPreview_
        ? busEditActionPreview_->relationImpact.summaries
        : QStringList{};
}

std::optional<std::pair<Tick, Tick>>
WaveCanvas::rangeSequencePreviewRange() const
{
    return busRangeSequencePreview_
        ? std::optional<std::pair<Tick, Tick>>{
              std::pair{
                  busRangeSequencePreview_->start,
                  busRangeSequencePreview_->end}}
        : std::nullopt;
}

bool WaveCanvas::rangeSequencePreviewChangesModel() const
{
    return busRangeSequencePreview_
        && busRangeSequencePreview_->modelChanges;
}

std::size_t WaveCanvas::rangeSequencePreviewRelationRemovalCount() const
{
    return busRangeSequencePreview_
        ? busRangeSequencePreview_->relationImpact.ids.size()
        : 0;
}

QString WaveCanvas::rangeSequenceCaretTargetLaneId() const
{
    return rangeSequenceCaretTarget_
            && !rangeSequenceCaretTarget_
                    ->targets.empty()
        ? QString::fromStdString(
              rangeSequenceCaretTarget_
                  ->targets.front().laneId)
        : QString{};
}

std::optional<std::pair<Tick, Tick>>
WaveCanvas::rangeSequenceCaretTargetRange() const
{
    return rangeSequenceCaretTarget_
            && !rangeSequenceCaretTarget_
                    ->targets.empty()
        ? std::optional<std::pair<Tick, Tick>>{
              std::pair{
                  rangeSequenceCaretTarget_
                      ->targets.front().start,
                  rangeSequenceCaretTarget_
                      ->targets.front().end}}
        : std::nullopt;
}

std::size_t
WaveCanvas::rangeSequenceCaretTargetCount() const noexcept
{
    return rangeSequenceCaretTarget_
        ? rangeSequenceCaretTarget_->targets.size()
        : 0;
}


void WaveCanvas::zoomIn()
{
    if (!scenario_) return;
    const auto anchor = zoomAnchorX();
    const auto cursorX = xAtTick(cursorTick_);
    const auto cursorVisible =
        cursorX >= headerWidth_ && cursorX <= viewport()->width();
    setScale(pixelsPerTick_ * 1.25, anchor);
    emit statusMessage(
        cursorVisible
            ? tr("Zoomed in around edit cursor at %1")
                  .arg(project_
                           ? QString::fromStdString(
                                 formatTick(cursorTick_, project_->timeBase))
                           : QString::number(cursorTick_))
            : tr("Zoomed in around viewport center"));
}

void WaveCanvas::zoomOut()
{
    if (!scenario_) return;
    const auto anchor = zoomAnchorX();
    const auto cursorX = xAtTick(cursorTick_);
    const auto cursorVisible =
        cursorX >= headerWidth_ && cursorX <= viewport()->width();
    setScale(pixelsPerTick_ / 1.25, anchor);
    emit statusMessage(
        cursorVisible
            ? tr("Zoomed out around edit cursor at %1")
                  .arg(project_
                           ? QString::fromStdString(
                                 formatTick(cursorTick_, project_->timeBase))
                           : QString::number(cursorTick_))
            : tr("Zoomed out around viewport center"));
}

void WaveCanvas::fitScenario()
{
    pendingVisibleTimeSpanRestore_.reset();
    if (!scenario_ || scenario_->duration <= 0 || waveViewportWidth() <= 0) {
        return;
    }
    pixelsPerTick_ = std::clamp(
        static_cast<double>(waveViewportWidth()) / static_cast<double>(scenario_->duration),
        1.0e-9,
        100.0);
    updateScrollBars();
    horizontalScrollBar()->setValue(0);
    positionBusPresetPalette();
    viewport()->update();
}

void WaveCanvas::fitSelection()
{
    pendingVisibleTimeSpanRestore_.reset();
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
    positionBusPresetPalette();
    viewport()->update();
}

void WaveCanvas::updateRulerScrub(
    const int x,
    const bool final,
    const bool rangeCleared)
{
    if (!scenario_ || !project_) return;
    const auto raw = std::clamp<Tick>(
        tickAtX(x),
        0,
        scenario_->duration);
    cursorTick_ = snappedTick(raw, nullptr);
    snapGuideTick_ = !final && cursorTick_ != raw
        ? std::optional<Tick>{cursorTick_}
        : std::nullopt;
    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    selectionRange_.reset();
    explicitRangeSelection_ = false;
    hideRangeEditPalette();
    ensureCursorVisible(cursorTick_);

    auto message = tr("Edit cursor %1")
                       .arg(QString::fromStdString(
                           formatTick(cursorTick_, project_->timeBase)));
    if (const auto* lane = findLane(*scenario_, selectedLaneId_);
        lane && lane->kind != LaneKind::Group) {
        const auto sampleTick = scenario_->duration > 0
            ? std::min(cursorTick_, scenario_->duration - 1)
            : Tick{0};
        message.append(
            tr(" · %1 = %2")
                .arg(QString::fromStdString(lane->name))
                .arg(laneValueAt(*lane, sampleTick)));
    }
    message.append(
        final
            ? tr(" · ruler move finished")
            : tr(" · drag to scrub · Esc cancels"));
    if (rangeCleared) message.append(tr(" · range cleared"));
    if (laneHeaderSelectionActive_ && !selectedLaneIds_.empty()) {
        message.append(
            tr(" · %1 selected signal target(s) kept")
                .arg(static_cast<qulonglong>(selectedLaneIds_.size())));
        const auto pasteHint = laneHeaderPasteHint();
        if (!pasteHint.isEmpty()) {
            message.append(tr(" · %1").arg(pasteHint));
        }
    }

    emit selectionChanged(
        QString::fromStdString(selectedLaneId_),
        cursorTick_);
    emit statusMessage(message);
    viewport()->update();
}

void WaveCanvas::cancelRulerScrub()
{
    if (!rulerScrubbing_) return;

    rulerScrubbing_ = false;
    rulerScrubClearedRange_ = false;
    horizontalScrollBar()->setValue(rulerScrubOriginalHorizontalScroll_);
    snapGuideTick_.reset();
    waveEditHoverLaneId_.clear();
    waveEditHoverRange_.reset();
    hideBusPresetPalette();

    if (rulerScrubOriginalSelection_) {
        const auto& snapshot = *rulerScrubOriginalSelection_;
        selectedLaneId_ = snapshot.selectedLaneId;
        selectedLaneIds_ = snapshot.selectedLaneIds;
        selectedSegmentLaneId_ = snapshot.selectedSegmentLaneId;
        selectedSegmentId_ = snapshot.selectedSegmentId;
        selectionRange_ = snapshot.selectionRange;
        cursorTick_ = std::clamp<Tick>(
            snapshot.cursorTick,
            0,
            scenario_ ? scenario_->duration : Tick{0});
        explicitRangeSelection_ = snapshot.explicitRangeSelection;
        laneHeaderSelectionActive_ = snapshot.laneHeaderSelectionActive;
        laneHeaderSelectionAnchorId_ = laneHeaderSelectionActive_
            ? selectedLaneId_
            : std::string{};
    } else {
        cursorTick_ = rulerScrubOriginalTick_;
    }
    rulerScrubOriginalSelection_.reset();

    if (explicitRangeSelection_ && selectionRange_) {
        showRangeEditPalette();
    } else {
        hideRangeEditPalette();
    }
    viewport()->setCursor(defaultCursorShape());
    emit selectionChanged(
        QString::fromStdString(selectedLaneId_),
        cursorTick_);

    const auto format = [this](const Tick tick) {
        return project_
            ? QString::fromStdString(formatTick(tick, project_->timeBase))
            : QString::number(tick);
    };
    auto message = tr("Ruler move cancelled · edit cursor restored to %1")
                       .arg(format(cursorTick_));
    if (explicitRangeSelection_ && selectionRange_) {
        message.append(
            tr(" · selection restored to %1–%2 on %3 signal(s)")
                .arg(format(selectionRange_->first))
                .arg(format(selectionRange_->second))
                .arg(static_cast<qulonglong>(selectedLaneIds_.size())));
    } else if (laneHeaderSelectionActive_ && !selectedLaneIds_.empty()) {
        message.append(
            tr(" · %1 selected signal target(s) restored")
                .arg(static_cast<qulonglong>(selectedLaneIds_.size())));
        const auto pasteHint = laneHeaderPasteHint();
        if (!pasteHint.isEmpty()) {
            message.append(tr(" · %1").arg(pasteHint));
        }
    }
    emit statusMessage(message);
    viewport()->update();
}

void WaveCanvas::selectEntireTimeline()
{
    if (tool_ != Tool::WaveEdit) {
        emit statusMessage(
            tr("Enter Wave Edit before selecting a waveform range"));
        return;
    }
    if (!scenario_ || scenario_->duration <= 0) {
        emit statusMessage(tr("Timeline has no editable time range"));
        return;
    }
    if (drawing_ || laneHeaderPressed_ || laneHeaderDragging_) {
        emit statusMessage(
            tr("Finish or cancel the current drag before selecting the full timeline"));
        return;
    }
    if (hasPendingRangeValueEdit()) {
        if (rangeValueEdit_) {
            rangeValueEdit_->setFocus(Qt::OtherFocusReason);
            rangeValueEdit_->selectAll();
        }
        emit statusMessage(
            tr("Finish the selected range value or press Esc before changing its time"));
        return;
    }

    const auto* lane = findLane(*scenario_, selectedLaneId_);
    if (!lane || !isLaneDisplayed(*lane) || lane->kind == LaneKind::Group) {
        emit statusMessage(tr("Select a signal before using Ctrl+A"));
        return;
    }

    const auto validTargets = !selectedLaneIds_.empty()
        && std::find(
               selectedLaneIds_.begin(),
               selectedLaneIds_.end(),
               selectedLaneId_)
            != selectedLaneIds_.end()
        && std::all_of(
            selectedLaneIds_.begin(),
            selectedLaneIds_.end(),
            [this](const std::string& laneId) {
                const auto* selected = findLane(*scenario_, laneId);
                return selected
                    && isLaneDisplayed(*selected)
                    && selected->kind != LaneKind::Group;
            });
    if (!validTargets) {
        selectedLaneIds_ = {selectedLaneId_};
    }

    laneHeaderSelectionActive_ = false;
    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    waveEditHoverLaneId_.clear();
    waveEditHoverRange_.reset();
    hideBusPresetPalette();
    snapGuideTick_.reset();
    selectionRange_ = std::pair<Tick, Tick>{0, scenario_->duration};
    explicitRangeSelection_ = true;
    showRangeEditPalette();

    const auto format = [this](const Tick tick) {
        return project_
            ? QString::fromStdString(formatTick(tick, project_->timeBase))
            : QString::number(tick);
    };
    emit selectionChanged(
        QString::fromStdString(selectedLaneId_),
        cursorTick_);
    emit statusMessage(
        tr("Ctrl+A selected full timeline %1 to %2 · %3 signal(s) · "
           "Ctrl+D repeats · Ctrl+C copies · Delete clears · Esc cancels")
            .arg(format(0))
            .arg(format(scenario_->duration))
            .arg(static_cast<qulonglong>(selectedLaneIds_.size())));
    viewport()->update();
}

void WaveCanvas::refreshModel()
{
    segmentActionPreview_.reset();
    busEditActionPreview_.reset();
    busEditPreviewAction_.reset();
    if (rangeSequenceBaseline_
        && !rangeSequenceBaselineMatchesContext()) {
        invalidateRangeSequenceHistoryContext();
    }
    rebuildLaneLayout();
    if (scenario_) {
        std::erase_if(
            selectedLaneIds_,
            [this](const std::string& laneId) {
                const auto* lane = findLane(*scenario_, laneId);
                return !lane || !isLaneDisplayed(*lane);
            });
        if (!selectedLaneId_.empty()) {
            const auto* lane = findLane(*scenario_, selectedLaneId_);
            if (!lane || !isLaneDisplayed(*lane)) {
                selectedLaneId_ = selectedLaneIds_.empty()
                    ? std::string{}
                    : selectedLaneIds_.front();
                laneHeaderSelectionActive_ = false;
                clearWaveEditState();
            }
        }
    }
    if (!selectedMarkerId_.empty() && !markerById(selectedMarkerId_)) {
        selectedMarkerId_.clear();
    }
    if (!selectedSegmentId_.empty()
        && !segmentById(selectedSegmentLaneId_, selectedSegmentId_)) {
        clearWaveEditState();
    }
    updateScrollBars();
    syncDurationEditor();
    positionQuickLaneSetup();
    positionLaneRename();
    positionDurationEditor();
    if (!busPresetLaneId_.empty()) {
        const auto* lane = scenario_ ? findLane(*scenario_, busPresetLaneId_) : nullptr;
        if (!lane || !isLaneDisplayed(*lane)) hideBusPresetPalette();
    }
    if (!busPresetLaneId_.empty()) {
        positionBusPresetPalette();
        updateBusEditActionStates();
        positionBusPresetPalette();
    }
    if (explicitRangeSelection_) {
        const auto validRange = scenario_
            && selectionRange_
            && selectionRange_->first >= 0
            && selectionRange_->second > selectionRange_->first
            && selectionRange_->second <= scenario_->duration;
        const auto validLanes = validRange
            && !selectedLaneIds_.empty()
            && std::all_of(
                selectedLaneIds_.begin(),
                selectedLaneIds_.end(),
                [this](const std::string& laneId) {
                    const auto* lane = findLane(*scenario_, laneId);
                    return lane && isLaneDisplayed(*lane);
                });
        if (validLanes) {
            showRangeEditPalette();
        } else {
            clearExplicitRangeSelection();
        }
    }
    viewport()->update();
}

void WaveCanvas::revealLocation(const QString& laneId, const qint64 tick)
{
    if (!scenario_) return;
    if (tool_ == Tool::WaveEdit) {
        clearWaveEditState();
    } else if (explicitRangeSelection_) {
        clearExplicitRangeSelection(false);
    }
    const auto* lane = findLane(*scenario_, laneId.toStdString());
    if (!lane || !lane->visible) return;
    if (const auto* group = visibleParentGroup(*lane);
        group && collapsedGroupIds_.contains(group->id)) {
        collapsedGroupIds_.erase(group->id);
        rebuildLaneLayout();
    }
    selectedLaneId_ = lane->id;
    selectedLaneIds_ = {lane->id};
    cursorTick_ = std::clamp<Tick>(tick, 0, scenario_->duration);
    if (pendingVisibleTimeSpanRestore_) {
        pendingVisibleTimeSpanRestore_->second = cursorTick_;
    }

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

void WaveCanvas::revealLane(const QString& laneId)
{
    if (!scenario_) return;
    const auto* lane = findLane(*scenario_, laneId.toStdString());
    if (!lane || !lane->visible) return;
    if (const auto* group = visibleParentGroup(*lane);
        group && collapsedGroupIds_.contains(group->id)) {
        collapsedGroupIds_.erase(group->id);
        rebuildLaneLayout();
    }
    if (explicitRangeSelection_) clearExplicitRangeSelection();
    if (tool_ == Tool::WaveEdit) {
        clearWaveEditState();
    } else if (tool_ == Tool::Marker) {
        selectedMarkerId_.clear();
        cursorInteraction_ = CursorInteraction::None;
        lockedMarkerOriginalRange_.reset();
    }
    selectedLaneId_ = lane->id;
    selectedLaneIds_ = {lane->id};
    laneHeaderSelectionAnchorId_ = lane->id;
    laneHeaderSelectionActive_ = true;
    snapGuideTick_.reset();
    ensureLaneVisible(lane->id);
    emit selectionChanged(laneId, cursorTick_);
    viewport()->update();
}

void WaveCanvas::selectLaneHeaders(
    const QStringList& laneIds,
    const QString& activeLaneId)
{
    if (!scenario_ || laneIds.isEmpty()) return;

    std::vector<std::string> requested;
    requested.reserve(static_cast<std::size_t>(laneIds.size()));
    for (const auto& laneId : laneIds) {
        const auto id = laneId.toStdString();
        const auto* lane = findLane(*scenario_, id);
        if (!lane || !lane->visible || lane->kind == LaneKind::Group) continue;
        if (std::find(requested.begin(), requested.end(), id) == requested.end()) {
            requested.push_back(id);
        }
        if (const auto* group = visibleParentGroup(*lane);
            group && collapsedGroupIds_.contains(group->id)) {
            collapsedGroupIds_.erase(group->id);
        }
    }
    if (requested.empty()) return;

    rebuildLaneLayout();
    std::vector<std::string> ordered;
    ordered.reserve(requested.size());
    for (const auto& lane : scenario_->lanes) {
        if (lane.kind == LaneKind::Group || !isLaneDisplayed(lane)) continue;
        if (std::find(requested.begin(), requested.end(), lane.id)
            != requested.end()) {
            ordered.push_back(lane.id);
        }
    }
    if (ordered.empty()) return;

    clearWaveEditState();
    hideBusPresetPalette();
    selectedMarkerId_.clear();
    cursorInteraction_ = CursorInteraction::None;
    lockedMarkerOriginalRange_.reset();
    selectedLaneIds_ = std::move(ordered);
    const auto requestedActive = activeLaneId.toStdString();
    selectedLaneId_ =
        std::find(
            selectedLaneIds_.begin(),
            selectedLaneIds_.end(),
            requestedActive)
            != selectedLaneIds_.end()
        ? requestedActive
        : selectedLaneIds_.front();
    laneHeaderSelectionActive_ = true;
    laneHeaderSelectionAnchorId_ = selectedLaneId_;
    snapGuideTick_.reset();
    ensureLaneVisible(selectedLaneId_);
    emit selectionChanged(
        QString::fromStdString(selectedLaneId_),
        cursorTick_);
    viewport()->setCursor(Qt::PointingHandCursor);
    viewport()->update();
}

void WaveCanvas::goToTick(const qint64 tick)
{
    if (!scenario_) return;
    cursorTick_ = std::clamp<Tick>(tick, 0, scenario_->duration);
    if (pendingVisibleTimeSpanRestore_) {
        pendingVisibleTimeSpanRestore_->second = cursorTick_;
    }
    ensureCursorVisible(cursorTick_);
    snapGuideTick_.reset();
    positionBusPresetPalette();
    viewport()->update();
}

bool WaveCanvas::setExplicitRangeActiveTick(const qint64 tick)
{
    if (!scenario_ || selectedLaneIds_.empty()) return false;
    const auto endpoints = explicitRangeAnchorAndActive();
    if (!endpoints || tick < 0 || tick > scenario_->duration
        || tick == endpoints->first) {
        return false;
    }

    cursorTick_ = tick;
    selectionRange_ = std::pair{
        std::min(endpoints->first, static_cast<Tick>(tick)),
        std::max(endpoints->first, static_cast<Tick>(tick)),
    };
    laneHeaderSelectionActive_ = false;
    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    waveEditHoverLaneId_.clear();
    waveEditHoverRange_.reset();
    hideBusPresetPalette();
    snapGuideTick_.reset();
    ensureCursorVisible(cursorTick_);
    showRangeEditPalette();
    emit selectionChanged(
        QString::fromStdString(selectedLaneId_),
        cursorTick_);
    viewport()->update();
    return true;
}

void WaveCanvas::dismissInlineValueEditor()
{
    hideBusPresetPalette();
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
    root.insert(QStringLiteral("schemaVersion"), 2);
    root.insert(QStringLiteral("durationTick"), QString::number(end - start));
    QJsonArray lanes;
    for (const auto& laneId : laneIds) {
        const auto* lane = findLane(*scenario_, laneId);
        if (!lane || lane->kind == LaneKind::Group) continue;
        QJsonObject laneObject;
        laneObject.insert(QStringLiteral("laneId"), QString::fromStdString(laneId));
        laneObject.insert(QStringLiteral("name"), QString::fromStdString(lane->name));
        laneObject.insert(
            QStringLiteral("kind"),
            QString::fromLatin1(
                toString(lane->kind).data(),
                static_cast<qsizetype>(toString(lane->kind).size())));
        laneObject.insert(QStringLiteral("width"), QString::number(lane->width));
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

void WaveCanvas::cutSelection()
{
    if (!explicitRangeSelection_ || !selectionRange_
        || selectionRange_->second <= selectionRange_->first) {
        emit statusMessage(tr("Cut requires a non-empty explicit time selection."));
        return;
    }
    copySelection();
    static_cast<void>(clearExplicitRange(true));
}

void WaveCanvas::pasteAtCursor()
{
    if (!commitPendingInlineEdits()) return;
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
    const auto schemaVersion =
        root.value(QStringLiteral("schemaVersion")).toInt(-1);
    if ((schemaVersion != 1 && schemaVersion != 2)
        || !root.value(QStringLiteral("durationTick")).isString()
        || !root.value(QStringLiteral("lanes")).isArray()) {
        emit statusMessage(tr("Clipboard range schema is invalid."));
        return;
    }
    bool validDuration = false;
    const auto duration = root.value(QStringLiteral("durationTick"))
                              .toString()
                              .toLongLong(&validDuration);
    if (explicitRangeSelection_
        && (!selectionRange_
            || selectionRange_->second <= selectionRange_->first)) {
        emit statusMessage(tr("Selected paste target is no longer valid."));
        return;
    }
    const auto pasteStart = explicitRangeSelection_
        ? selectionRange_->first
        : cursorTick_;
    if (!validDuration
        || duration <= 0
        || pasteStart < 0
        || pasteStart > scenario_->duration) {
        emit statusMessage(tr("Clipboard range duration or destination is invalid."));
        return;
    }
    std::vector<CopiedLaneRange> copiedLanes;
    struct CopiedLaneMetadata {
        QString name;
        LaneKind kind{LaneKind::Bit};
        std::size_t width{1};
    };
    std::vector<CopiedLaneMetadata> copiedMetadata;
    for (const auto& laneValue : root.value(QStringLiteral("lanes")).toArray()) {
        if (!laneValue.isObject()) continue;
        const auto laneObject = laneValue.toObject();
        const auto laneId = laneObject.value(QStringLiteral("laneId")).toString().toStdString();
        if (laneId.empty() || !laneObject.value(QStringLiteral("segments")).isArray()) {
            continue;
        }
        CopiedLaneMetadata metadata;
        if (schemaVersion == 2) {
            bool validWidth = false;
            const auto kind = laneKindFromString(
                laneObject.value(QStringLiteral("kind")).toString().toStdString());
            const auto width = laneObject.value(QStringLiteral("width"))
                                   .toString()
                                   .toULongLong(&validWidth);
            if (!laneObject.value(QStringLiteral("name")).isString()
                || !laneObject.value(QStringLiteral("kind")).isString()
                || !laneObject.value(QStringLiteral("width")).isString()
                || !kind
                || *kind == LaneKind::Group
                || !validWidth
                || width == 0) {
                emit statusMessage(tr("Clipboard range signal metadata is invalid."));
                return;
            }
            metadata.name = laneObject.value(QStringLiteral("name")).toString();
            metadata.kind = *kind;
            metadata.width = static_cast<std::size_t>(width);
        } else {
            const auto* sourceLane = findLane(*scenario_, laneId);
            if (!sourceLane || sourceLane->kind == LaneKind::Group) continue;
            metadata.name = QString::fromStdString(sourceLane->name);
            metadata.kind = sourceLane->kind;
            metadata.width = sourceLane->width;
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
        copiedMetadata.push_back(std::move(metadata));
    }
    if (copiedLanes.empty()) {
        emit statusMessage(tr("No clipboard lanes exist in this scenario."));
        return;
    }

    auto targetSummary = tr("%1 lane(s)")
                             .arg(static_cast<qulonglong>(copiedLanes.size()));
    std::vector<std::string> requestedTargetIds;
    bool explicitTargetMapping = false;
    if (explicitRangeSelection_) {
        explicitTargetMapping = true;
        if (selectedLaneIds_.size() != copiedLanes.size()) {
            emit statusMessage(
                tr("Cannot paste %1 copied signal(s) into %2 selected signal(s) · select the same number of targets")
                    .arg(static_cast<qulonglong>(copiedLanes.size()))
                    .arg(static_cast<qulonglong>(selectedLaneIds_.size())));
            return;
        }
        requestedTargetIds = selectedLaneIds_;
    } else if (laneHeaderSelectionActive_ && !selectedLaneIds_.empty()) {
        explicitTargetMapping = true;
        if (selectedLaneIds_.size() != copiedLanes.size()) {
            emit statusMessage(
                tr("Cannot paste %1 copied signal(s) into %2 selected signal(s) · select the same number of targets")
                    .arg(static_cast<qulonglong>(copiedLanes.size()))
                    .arg(static_cast<qulonglong>(selectedLaneIds_.size())));
            return;
        }
        requestedTargetIds = selectedLaneIds_;
    } else if (copiedLanes.size() == 1 && !selectedLaneId_.empty()) {
        requestedTargetIds.push_back(selectedLaneId_);
    } else {
        requestedTargetIds.reserve(copiedLanes.size());
        for (const auto& copied : copiedLanes) {
            requestedTargetIds.push_back(copied.laneId);
        }
    }

    if (!requestedTargetIds.empty()) {
        const auto kindLabel = [](const LaneKind kind) {
            const auto label = toString(kind);
            return QString::fromLatin1(
                label.data(),
                static_cast<qsizetype>(label.size()));
        };
        bool remapped = false;
        for (std::size_t index = 0; index < copiedLanes.size(); ++index) {
            auto& copied = copiedLanes.at(index);
            const auto& source = copiedMetadata.at(index);
            const auto* targetLane = findLane(*scenario_, requestedTargetIds.at(index));
            if (!targetLane) {
                emit statusMessage(
                    requestedTargetIds.at(index) == copied.laneId
                        ? tr("Cannot paste %1 · original signal no longer exists; select %2 target signal(s)")
                              .arg(source.name)
                              .arg(static_cast<qulonglong>(copiedLanes.size()))
                        : tr("Cannot paste %1 · selected target is unavailable")
                              .arg(source.name));
                return;
            }
            if (targetLane->kind == LaneKind::Group) {
                emit statusMessage(
                    tr("Cannot paste %1 · selected target is not a signal")
                        .arg(source.name));
                return;
            }
            const auto sourceName = source.name;
            const auto targetName = QString::fromStdString(targetLane->name);
            if (source.kind != targetLane->kind) {
                emit statusMessage(
                    tr("Cannot paste %1 (%2) into %3 (%4) · signal types must match")
                        .arg(
                            sourceName,
                            kindLabel(source.kind),
                            targetName,
                            kindLabel(targetLane->kind)));
                return;
            }
            if ((source.kind == LaneKind::Bus
                 || source.kind == LaneKind::Enum)
                && source.width != targetLane->width) {
                emit statusMessage(
                    tr("Cannot paste %1 (%2-bit) into %3 (%4-bit) · signal widths must match")
                        .arg(sourceName)
                        .arg(static_cast<qulonglong>(source.width))
                        .arg(targetName)
                        .arg(static_cast<qulonglong>(targetLane->width)));
                return;
            }
            for (auto& segment : copied.relativeSegments) {
                const auto validation = validateLaneValue(*targetLane, segment.value);
                if (!validation.valid) {
                    emit statusMessage(
                        tr("Cannot paste %1 into %2 · %3")
                            .arg(
                                sourceName,
                                targetName,
                                QString::fromStdString(validation.error)));
                    return;
                }
                segment.value = validation.normalizedValue;
            }
            remapped = remapped || copied.laneId != targetLane->id;
            copied.laneId = targetLane->id;
            if (copiedLanes.size() == 1 && remapped) {
                targetSummary = tr("%1 → %2").arg(sourceName, targetName);
            }
        }
        if (explicitTargetMapping && copiedLanes.size() > 1) {
            targetSummary = tr("%1 copied signals → %2 selected signals")
                                .arg(static_cast<qulonglong>(copiedLanes.size()))
                                .arg(static_cast<qulonglong>(requestedTargetIds.size()));
        }
    }

    std::vector<std::string> projectedTargetIds;
    projectedTargetIds.reserve(copiedLanes.size());
    for (const auto& copied : copiedLanes) {
        projectedTargetIds.push_back(copied.laneId);
    }
    const auto projectedPaste = buildPastePreview(
        projectedTargetIds,
        pasteStart);
    if (projectedPaste && !projectedPaste->modelChanges) {
        const auto startLabel = project_
            ? QString::fromStdString(
                  formatTick(pasteStart, project_->timeBase))
            : QString::number(pasteStart);
        emit statusMessage(
            tr("Paste skipped · %1 at %2 already matches copied range · no values changed")
                .arg(targetSummary, startLabel));
        return;
    }

    const auto durationBeforePaste = scenario_->duration;
    const auto relationCountBefore = scenario_->relations.size();
    const auto historyStateBefore = commandStack_->stateId();
    const auto historySelectionBefore = historySelectionSnapshot();
    bool changed = false;
    try {
        changed = commandStack_->execute(std::make_unique<PasteRangeCommand>(
            *scenario_,
            copiedLanes,
            pasteStart,
            duration));
    } catch (const std::exception& exception) {
        emit statusMessage(tr("Paste failed · %1").arg(QString::fromUtf8(exception.what())));
        return;
    }
    explicitRangeSelection_ = false;
    hideRangeEditPalette();
    if (rangeValueEdit_) rangeValueEdit_->setModified(false);
    selectedLaneIds_.clear();
    for (const auto& copied : copiedLanes) selectedLaneIds_.push_back(copied.laneId);
    selectedLaneId_ = selectedLaneIds_.front();
    selectionRange_ = std::pair{
        pasteStart,
        pasteStart + std::min<Tick>(duration, scenario_->duration - pasteStart),
    };
    explicitRangeSelection_ = true;
    laneHeaderSelectionActive_ = false;
    laneHeaderSelectionAnchorId_.clear();
    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    waveEditHoverLaneId_.clear();
    waveEditHoverRange_.reset();
    hideBusPresetPalette();
    snapGuideTick_.reset();
    if (changed) {
        if (historySelectionBefore.explicitRangeSelection
            || historySelectionBefore.laneHeaderSelectionActive) {
            rememberHistorySelectionTransition(
                historyStateBefore,
                historySelectionBefore,
                commandStack_->stateId());
        }
        emit modelEdited();
        emit commandAvailabilityChanged();
    }
    refreshModel();
    const auto endExtended = scenario_->duration > durationBeforePaste;
    if (endExtended && selectionRange_) {
        const auto desiredScroll = static_cast<int>(std::clamp(
            std::ceil(
                static_cast<double>(selectionRange_->second) * pixelsPerTick_
                - static_cast<double>(std::max(1, waveViewportWidth() - 20))),
            0.0,
            static_cast<double>(horizontalScrollBar()->maximum())));
        if (desiredScroll > horizontalScrollBar()->value()) {
            horizontalScrollBar()->setValue(desiredScroll);
        }
    }
    const auto pastedDuration = selectionRange_->second - selectionRange_->first;
    const auto startLabel = project_
        ? QString::fromStdString(formatTick(pasteStart, project_->timeBase))
        : QString::number(pasteStart);
    const auto durationLabel = project_
        ? QString::fromStdString(formatTick(pastedDuration, project_->timeBase))
        : QString::number(pastedDuration);
    auto message = changed
        ? tr("Pasted %1 at %2 · %3")
              .arg(targetSummary, startLabel, durationLabel)
        : tr("%1 at %2 · %3 already matches copied range · no values changed")
              .arg(targetSummary, startLabel, durationLabel);
    if (endExtended) {
        message += tr(" · End extended to %1").arg(
            project_
                ? QString::fromStdString(formatTick(scenario_->duration, project_->timeBase))
                : QString::number(scenario_->duration));
    }
    emit statusMessage(
        changed
            ? appendRelationAwareUndo(
                  message,
                  relationCountBefore,
                  scenario_->relations.size())
            : message);
}

void WaveCanvas::duplicateSelectionAfter()
{
    if (!commitPendingInlineEdits()) return;
    if (!scenario_ || !commandStack_) return;
    if (!explicitRangeSelection_
        || !selectionRange_
        || selectionRange_->second <= selectionRange_->first) {
        emit statusMessage(tr("Duplicate requires a non-empty explicit time selection."));
        return;
    }

    auto laneIds = selectedLaneIds_;
    if (laneIds.empty() && !selectedLaneId_.empty()) laneIds.push_back(selectedLaneId_);
    if (laneIds.empty()) {
        emit statusMessage(tr("Duplicate requires at least one selected signal."));
        return;
    }

    const auto repeatPreflight = rangeRepeatAvailability();
    if (!repeatPreflight.enabled) {
        auto reason = repeatPreflight.toolTip;
        reason.replace(QLatin1Char('\n'), QStringLiteral(" · "));
        emit statusMessage(
            tr("Repeat skipped · %1 · source selection kept · no values changed · clipboard unchanged")
                .arg(reason));
        return;
    }

    const auto sourceStart = selectionRange_->first;
    const auto sourceEnd = selectionRange_->second;
    const auto duration = sourceEnd - sourceStart;
    const auto destination = sourceEnd;
    auto copiedLanes = captureLaneRanges(
        *scenario_,
        laneIds,
        sourceStart,
        sourceEnd,
        false);
    if (copiedLanes.empty()) {
        emit statusMessage(tr("Selected range does not contain an available signal."));
        return;
    }

    const auto durationBeforeDuplicate = scenario_->duration;
    const auto relationCountBefore = scenario_->relations.size();
    const auto historyStateBefore = commandStack_->stateId();
    const auto historySelectionBefore = historySelectionSnapshot();
    bool changed = false;
    try {
        changed = commandStack_->execute(std::make_unique<PasteRangeCommand>(
            *scenario_,
            copiedLanes,
            destination,
            duration,
            "Duplicate range"));
    } catch (const std::exception& exception) {
        emit statusMessage(
            tr("Duplicate failed · %1").arg(QString::fromUtf8(exception.what())));
        return;
    }
    if (!changed) {
        emit statusMessage(
            tr("Repeat skipped · following range already matches the selection · source selection kept · no values changed · clipboard unchanged"));
        return;
    }

    explicitRangeSelection_ = false;
    hideRangeEditPalette();
    if (rangeValueEdit_) rangeValueEdit_->setModified(false);
    selectedLaneIds_.clear();
    for (const auto& copied : copiedLanes) selectedLaneIds_.push_back(copied.laneId);
    selectedLaneId_ = selectedLaneIds_.front();
    selectionRange_ = std::pair{destination, destination + duration};
    explicitRangeSelection_ = true;
    cursorTick_ = destination;
    laneHeaderSelectionActive_ = false;
    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    waveEditHoverLaneId_.clear();
    waveEditHoverRange_.reset();
    hideBusPresetPalette();
    snapGuideTick_.reset();
    rememberHistorySelectionTransition(
        historyStateBefore,
        historySelectionBefore,
        commandStack_->stateId());
    emit modelEdited();
    emit commandAvailabilityChanged();
    refreshModel();
    emit selectionChanged(
        QString::fromStdString(selectedLaneId_),
        cursorTick_);

    if (selectionRange_) {
        const auto desiredScroll = static_cast<int>(std::clamp(
            std::ceil(
                static_cast<double>(selectionRange_->second) * pixelsPerTick_
                - static_cast<double>(std::max(1, waveViewportWidth() - 20))),
            0.0,
            static_cast<double>(horizontalScrollBar()->maximum())));
        if (desiredScroll > horizontalScrollBar()->value()) {
            horizontalScrollBar()->setValue(desiredScroll);
        }
    }

    const auto format = [this](const Tick tick) {
        return project_
            ? QString::fromStdString(formatTick(tick, project_->timeBase))
            : QString::number(tick);
    };
    auto message =
        tr("Duplicated %1 signal(s) to %2–%3 · clipboard unchanged")
            .arg(static_cast<qulonglong>(copiedLanes.size()))
            .arg(format(destination))
            .arg(format(destination + duration));
    if (scenario_->duration > durationBeforeDuplicate) {
        message += tr(" · End extended to %1").arg(format(scenario_->duration));
    }
    if (!repeatPreflight.relationImpact.summaries.isEmpty()) {
        message += tr(" · removed: %1")
                       .arg(
                           repeatPreflight.relationImpact.summaries.join(
                               QStringLiteral("; ")));
    }
    emit statusMessage(appendRelationAwareUndo(
        message,
        relationCountBefore,
        scenario_->relations.size()));
}

void WaveCanvas::insertPulse()
{
    if (!commitPendingInlineEdits()) return;
    if (!scenario_ || !project_ || !commandStack_) return;
    auto* lane = findLane(*scenario_, selectedLaneId_);
    if (!lane || lane->kind != LaneKind::Bit) {
        emit statusMessage(tr("Pulse requires a selected bit lane."));
        return;
    }
    const auto [start, end] = editableBeatRangeAt(cursorTick_, *lane);

    if (end <= start) {
        emit statusMessage(tr("Pulse not inserted · cursor is at the scenario end"));
        return;
    }
    const auto laneId = lane->id;
    const auto laneName = lane->name;
    auto value = std::string{"1"};
    const auto covering = std::find_if(
        lane->segments.begin(),
        lane->segments.end(),
        [start](const Segment& segment) {
            return segment.start <= start && start < segment.end;
        });
    if (covering != lane->segments.end() && covering->value == "1") value = "0";
    const auto relationCountBefore = scenario_->relations.size();
    try {
        commandStack_->execute(std::make_unique<SetLaneRangeCommand>(
            *scenario_,
            laneId,
            start,
            end,
            value));
    } catch (const std::exception& exception) {
        emit statusMessage(
            tr("Pulse not inserted · %1").arg(QString::fromUtf8(exception.what())));
        return;
    }
    selectedLaneId_ = laneId;
    selectedLaneIds_ = {laneId};
    laneHeaderSelectionActive_ = false;
    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    selectionRange_ = std::pair{start, end};
    waveEditHoverLaneId_ = laneId;
    waveEditHoverRange_ = selectionRange_;
    cursorTick_ = start;
    emit modelEdited();
    emit commandAvailabilityChanged();
    emit selectionChanged(QString::fromStdString(laneId), start);
    refreshModel();
    emit statusMessage(appendRelationAwareUndo(
        tr("%1 · %2–%3 pulse = %4")
            .arg(QString::fromStdString(laneName))
            .arg(QString::fromStdString(formatTick(start, project_->timeBase)))
            .arg(QString::fromStdString(formatTick(end, project_->timeBase)))
            .arg(QString::fromStdString(value)),
        relationCountBefore,
        scenario_->relations.size()));
}

bool WaveCanvas::viewportEvent(QEvent* event)
{
    if (event && event->type() == QEvent::ToolTip) {
        const auto* help = static_cast<QHelpEvent*>(event);
        if (signalHeaderDividerAt(help->pos())) {
            QToolTip::showText(
                help->globalPos(),
                tr("Drag to resize signal names · double-click to fit"),
                viewport(),
                QRect(headerWidth_ - 5, 0, 11, viewport()->height()));
            return true;
        }
        if (help->pos().y() < RulerHeight
            && help->pos().x() >= headerWidth_) {
            auto rulerHelp = tr(
                "Click or drag to place the edit cursor\n"
                "Space+drag or middle-drag pans · Ctrl+wheel zooms · "
                "Shift+wheel scrolls horizontally");
            if (laneHeaderSelectionActive_ && !selectedLaneIds_.empty()) {
                rulerHelp = tr(
                    "Click or drag to place the edit cursor while keeping %1 selected signal target(s)\n"
                    "Right-click a selected signal name or press Ctrl+V to paste at that time\n"
                    "Space+drag or middle-drag pans · Ctrl+wheel zooms · "
                    "Shift+wheel scrolls horizontally")
                                 .arg(static_cast<qulonglong>(
                                     selectedLaneIds_.size()));
            }
            QToolTip::showText(
                help->globalPos(),
                rulerHelp,
                viewport(),
                QRect(
                    headerWidth_,
                    0,
                    waveViewportWidth(),
                    RulerHeight));
            return true;
        }
        if (help->pos().x() < headerWidth_) {
            if (const auto* lane = laneAtY(help->pos().y())) {
                if (lane->kind == LaneKind::Group) {
                    const auto memberCount =
                        visibleGroupMemberCount(lane->id);
                    if (memberCount == 0) {
                        QToolTip::showText(
                            help->globalPos(),
                            tr("%1\nGroup · no visible member signals")
                                .arg(QString::fromStdString(lane->name)),
                            viewport());
                        return true;
                    }
                    const auto collapsed =
                        collapsedGroupIds_.contains(lane->id);
                    QToolTip::showText(
                        help->globalPos(),
                        tr("%1\n%2 · %3 visible member signal(s)\n"
                           "Click the arrow or use Left/Right to %4")
                            .arg(QString::fromStdString(lane->name))
                            .arg(collapsed ? tr("Collapsed") : tr("Expanded"))
                            .arg(static_cast<qulonglong>(memberCount))
                            .arg(collapsed ? tr("expand") : tr("collapse")),
                        viewport(),
                        groupDisclosureRect(*lane));
                    return true;
                }
                auto label = QString::fromStdString(lane->name);
                if (const auto* group = visibleParentGroup(*lane)) {
                    label.append(
                        tr("\nMember of %1")
                            .arg(QString::fromStdString(group->name)));
                }
                QToolTip::showText(
                    help->globalPos(),
                    label,
                    viewport());
                return true;
            }
        }
        if (tool_ == Tool::WaveEdit
            && scenario_
            && project_
            && help->pos().x() >= headerWidth_) {
            if (const auto* lane = laneAtY(help->pos().y());
                lane && lane->kind != LaneKind::Group) {
                const auto tick = scenario_->duration > 0
                    ? std::clamp<Tick>(
                          tickAtX(help->pos().x()),
                          0,
                          scenario_->duration - 1)
                    : Tick{0};
                if (lane->kind == LaneKind::Bit) {
                    const auto range = editableBeatRangeAt(tick, *lane);
                    const auto details =
                        tr("%1 · beat value %2\n%3–%4 · width %5\n"
                           "Click toggles one beat · drag toggles several beats\n"
                           "Press 0, 1, X, or Z to set an exact value")
                            .arg(
                                QString::fromStdString(lane->name),
                                laneValueAt(*lane, tick),
                                QString::fromStdString(formatTick(
                                    range.first,
                                    project_->timeBase)),
                                QString::fromStdString(formatTick(
                                    range.second,
                                    project_->timeBase)),
                                QString::fromStdString(formatTick(
                                    range.second - range.first,
                                    project_->timeBase)));
                    QToolTip::showText(help->globalPos(), details, viewport());
                    return true;
                }
                if (const auto* segment = segmentAtTick(*lane, tick)) {
                    const auto details =
                        tr("%1 · value %2\n%3–%4 · width %5\nClick selects · drag body moves · drag edges resizes\nEdit > Segment provides exact commands · Esc keeps signal/time")
                            .arg(
                                QString::fromStdString(lane->name),
                                QString::fromStdString(segment->value),
                                QString::fromStdString(formatTick(
                                    segment->start,
                                    project_->timeBase)),
                                QString::fromStdString(formatTick(
                                    segment->end,
                                    project_->timeBase)),
                                QString::fromStdString(formatTick(
                                    segment->end - segment->start,
                                    project_->timeBase)));
                    QToolTip::showText(
                        help->globalPos(),
                        lane->kind == LaneKind::Clock
                            ? details
                            : details
                                + tr("\nDouble-click or Enter edits value · Ctrl+drag copies · press or release Ctrl while dragging to switch Move/Copy"),
                        viewport());
                    return true;
                }
                const auto range = editableBeatRangeAt(tick, *lane);
                const auto details =
                    lane->kind == LaneKind::Clock
                    ? tr("%1 · normal clock · %2–%3\n"
                         "Right-click or press G/X/R to gate, drive X, or run this period")
                          .arg(
                              QString::fromStdString(lane->name),
                              QString::fromStdString(formatTick(
                                  range.first,
                                  project_->timeBase)),
                              QString::fromStdString(formatTick(
                                  range.second,
                                  project_->timeBase)))
                    : tr("%1 · implicit value %2\n%3–%4 · width %5\n"
                         "Click selects one beat · double-click edits that beat")
                          .arg(
                              QString::fromStdString(lane->name),
                              laneValueAt(*lane, tick),
                              QString::fromStdString(formatTick(
                                  range.first,
                                  project_->timeBase)),
                              QString::fromStdString(formatTick(
                                  range.second,
                                  project_->timeBase)),
                              QString::fromStdString(formatTick(
                                  range.second - range.first,
                                  project_->timeBase)));
                QToolTip::showText(help->globalPos(), details, viewport());
                return true;
            }
        }
        if (help->pos().x() >= headerWidth_) {
            QToolTip::showText(
                help->globalPos(),
                tr("Space+drag or middle-drag pans · Ctrl+wheel zooms · "
                   "Shift+wheel scrolls horizontally"),
                viewport());
            return true;
        }
        QToolTip::hideText();
        event->ignore();
        return true;
    }
    if (event && event->type() == QEvent::Leave) {
        const auto changed = waveEditHoverRange_.has_value()
            || !waveEditHoverLaneId_.empty()
            || snapGuideTick_.has_value();
        waveEditHoverLaneId_.clear();
        waveEditHoverRange_.reset();
        snapGuideTick_.reset();
        QToolTip::hideText();
        if (changed) viewport()->update();
    }
    return QAbstractScrollArea::viewportEvent(event);
}

void WaveCanvas::contextMenuEvent(QContextMenuEvent* event)
{
    if (!scenario_) {
        QAbstractScrollArea::contextMenuEvent(event);
        return;
    }
    if (durationEdit_
        && durationEdit_->isVisible()
        && durationEdit_->isModified()) {
        if (!commitPendingInlineEdits()) {
            event->accept();
            return;
        }
    }
    if (!commitPendingInlineEdits()) {
        event->accept();
        return;
    }
    if (explicitRangeContains(event->pos())) {
        setFocus(Qt::MouseFocusReason);
        QMenu menu(this);
        menu.setObjectName(QStringLiteral("WaveformRangeContextMenu"));
        menu.setToolTipsVisible(true);
        const auto clearAvailability = rangeClearAvailability();
        const auto repeatAvailability = rangeRepeatAvailability();
        const auto rangeKind = explicitRangeKind();

        auto* copyRange = menu.addAction(tr("Copy selected range"));
        copyRange->setObjectName(QStringLiteral("RangeContextCopyAction"));
        copyRange->setShortcut(QKeySequence::Copy);
        auto* cutRange = menu.addAction(tr("Cut selected range"));
        cutRange->setObjectName(QStringLiteral("RangeContextCutAction"));
        cutRange->setShortcut(QKeySequence::Cut);
        cutRange->setToolTip(
            clearAvailability.enabled
                ? tr("Copy the selected range, then clear its source as one undo command\n%1")
                      .arg(clearAvailability.toolTip)
                : tr("Copy the selected range; the source already uses implicit values, so Cut will not remove waveform values\n%1")
                      .arg(clearAvailability.toolTip));
        auto cutStatusTip = cutRange->toolTip();
        cutStatusTip.replace(
            QLatin1Char('\n'),
            QStringLiteral(" · "));
        cutRange->setStatusTip(cutStatusTip);
        auto* pasteRange = menu.addAction(tr("Paste into selected range"));
        pasteRange->setObjectName(QStringLiteral("RangeContextPasteAction"));
        pasteRange->setShortcut(QKeySequence::Paste);
        const auto pasteAvailability = rangePasteAvailability();
        pasteRange->setEnabled(pasteAvailability.enabled);
        pasteRange->setToolTip(pasteAvailability.toolTip);
        auto pasteStatusTip = pasteAvailability.toolTip;
        pasteStatusTip.replace(
            QLatin1Char('\n'),
            QStringLiteral(" · "));
        pasteRange->setStatusTip(pasteStatusTip);
        const auto repeatRelationCount =
            repeatAvailability.relationImpact.ids.size();
        auto* duplicateRange = menu.addAction(
            repeatRelationCount > 0
                ? tr("Duplicate selected range after · removes %1 relation(s)")
                      .arg(static_cast<qulonglong>(
                          repeatRelationCount))
                : tr("Duplicate selected range after"));
        duplicateRange->setObjectName(
            QStringLiteral("RangeContextDuplicateAfterAction"));
        duplicateRange->setEnabled(repeatAvailability.enabled);
        duplicateRange->setToolTip(repeatAvailability.toolTip);
        auto repeatStatusTip = repeatAvailability.toolTip;
        repeatStatusTip.replace(
            QLatin1Char('\n'),
            QStringLiteral(" · "));
        duplicateRange->setStatusTip(repeatStatusTip);
        const auto clearRelationCount =
            clearAvailability.relationImpact.ids.size();
        auto* clearRange = menu.addAction(
            clearAvailability.clockRange
                ? clearRelationCount > 0
                    ? tr("Restore selected Clock range to Run · removes %1 relation(s)")
                          .arg(static_cast<qulonglong>(
                              clearRelationCount))
                    : tr("Restore selected Clock range to Run")
                : clearRelationCount > 0
                    ? tr("Clear selected range · removes %1 relation(s)")
                          .arg(static_cast<qulonglong>(
                              clearRelationCount))
                    : tr("Clear selected range"));
        clearRange->setObjectName(QStringLiteral("RangeContextClearAction"));
        clearRange->setShortcut(QKeySequence::Delete);
        clearRange->setEnabled(clearAvailability.enabled);
        clearRange->setToolTip(clearAvailability.toolTip);
        auto clearStatusTip = clearAvailability.toolTip;
        clearStatusTip.replace(
            QLatin1Char('\n'),
            QStringLiteral(" · "));
        clearRange->setStatusTip(clearStatusTip);

        menu.addSeparator();
        auto* fitRange = menu.addAction(tr("Fit selected range"));
        fitRange->setObjectName(QStringLiteral("RangeContextFitAction"));
        fitRange->setToolTip(tr("Fill the waveform viewport with the selected time range"));

        QAction* editRangeValue = nullptr;
        if (rangeValueEdit_ && rangeValueEdit_->isVisible()) {
            editRangeValue = menu.addAction(
                rangeKind == LaneKind::Bit
                    ? tr("Edit selected Bit pattern")
                    : tr("Edit selected range value"));
            editRangeValue->setObjectName(
                QStringLiteral("RangeContextEditValueAction"));
            editRangeValue->setToolTip(
                rangeKind == LaneKind::Bit
                    ? tr("Focus the 0/1/X/Z pattern field without changing the selection")
                    : tr("Focus the range value field without changing the selection"));
        }

        menu.addSeparator();
        auto* closeRange = menu.addAction(tr("Close range selection"));
        closeRange->setObjectName(QStringLiteral("RangeContextCloseAction"));
        closeRange->setShortcut(QKeySequence(Qt::Key_Escape));
        closeRange->setToolTip(
            tr("Keep the active signal and edit cursor, but leave range editing"));

        const auto* chosen = menu.exec(event->globalPos());
        if (chosen == copyRange) {
            copySelection();
        } else if (chosen == cutRange) {
            cutSelection();
        } else if (chosen == pasteRange) {
            pasteAtCursor();
        } else if (chosen == duplicateRange) {
            duplicateSelectionAfter();
        } else if (chosen == clearRange) {
            static_cast<void>(clearExplicitRange());
        } else if (chosen == fitRange) {
            fitSelection();
            emit statusMessage(
                tr("Fitted selected range · right-click or use the range toolbar to continue"));
        } else if (chosen == editRangeValue) {
            rangeValueEdit_->setFocus(Qt::OtherFocusReason);
            rangeValueEdit_->selectAll();
            emit statusMessage(
                rangeKind == LaneKind::Bit
                    ? tr("Edit selected Bit pattern · Enter applies as one Undo")
                    : tr("Edit selected range value · Enter applies"));
        } else if (chosen == closeRange) {
            if (rangeCloseButton_) rangeCloseButton_->click();
        }
        event->accept();
        return;
    }
    const auto rangeClearedForRetarget = explicitRangeSelection_;
    if (explicitRangeSelection_) {
        clearExplicitRangeSelection();
        emit statusMessage(tr("Range selection cleared"));
    }
    auto* lane = laneAtY(event->pos().y());
    if (!lane) {
        QAbstractScrollArea::contextMenuEvent(event);
        return;
    }
    setFocus(Qt::MouseFocusReason);
    if (event->pos().x() < headerWidth_) {
        const auto preservesSignalSelection =
            lane->kind != LaneKind::Group
            && laneHeaderSelectionActive_
            && selectedLaneIds_.size() > 1
            && std::find(
                   selectedLaneIds_.begin(),
                   selectedLaneIds_.end(),
                   lane->id)
                != selectedLaneIds_.end();
        selectedLaneId_ = lane->id;
        if (!preservesSignalSelection) {
            selectedLaneIds_ = {lane->id};
            laneHeaderSelectionAnchorId_ =
                lane->kind == LaneKind::Group ? std::string{} : lane->id;
        }
        const auto lockedMarkerDeselected =
            tool_ == Tool::Marker && !selectedMarkerId_.empty();
        if (tool_ == Tool::Marker) {
            selectedMarkerId_.clear();
            cursorInteraction_ = CursorInteraction::None;
            lockedMarkerOriginalRange_.reset();
        }
        if (tool_ == Tool::WaveEdit) clearWaveEditState();
        laneHeaderSelectionActive_ = true;
        emit selectionChanged(QString::fromStdString(lane->id), cursorTick_);
        auto selectionMessage = preservesSignalSelection
            ? tr("Selected %1 signals · active %2 · choose one Group action for the selection")
                  .arg(static_cast<qulonglong>(selectedLaneIds_.size()))
                  .arg(QString::fromStdString(lane->name))
            : lane->kind == LaneKind::Group
            ? tr("Selected group %1").arg(QString::fromStdString(lane->name))
            : tr("Selected signal %1").arg(QString::fromStdString(lane->name));
        if (lane->kind == LaneKind::Group) {
            selectionMessage.append(
                collapsedGroupIds_.contains(lane->id)
                    ? tr(" · Right expands")
                    : tr(" · Left collapses"));
        } else if (tool_ == Tool::WaveEdit && !preservesSignalSelection) {
            selectionMessage.append(
                tr(" · value %1 at %2")
                    .arg(laneValueAt(*lane, cursorTick_))
                    .arg(QString::fromStdString(
                        formatTick(cursorTick_, project_->timeBase))));
        }
        if (lane->kind != LaneKind::Group) {
            const auto pasteHint = laneHeaderPasteHint();
            if (!pasteHint.isEmpty()) {
                selectionMessage.append(tr(" · %1").arg(pasteHint));
            }
        }
        if (lockedMarkerDeselected) {
            selectionMessage.append(tr(" · locked cursor/range deselected"));
        }
        if (rangeClearedForRetarget) {
            selectionMessage.append(tr(" · range cleared"));
        }
        emit statusMessage(selectionMessage);
        emit editLaneParametersRequested(
            QString::fromStdString(lane->id),
            event->globalPos());
        viewport()->update();
        event->accept();
        return;
    }
    if (lane->kind == LaneKind::Group) {
        event->accept();
        return;
    }
    if (tool_ == Tool::Marker) {
        emit statusMessage(
            tr("Measure active · waveform actions are unavailable · press Esc to edit"));
        event->accept();
        return;
    }

    selectedLaneId_ = lane->id;
    selectedLaneIds_ = {lane->id};
    laneHeaderSelectionActive_ = false;
    const auto rawTick = scenario_->duration > 0
        ? std::clamp<Tick>(tickAtX(event->pos().x()), 0, scenario_->duration - 1)
        : Tick{0};
    cursorTick_ = editTick(rawTick, *lane);
    const auto* segment = segmentAtTick(*lane, rawTick);
    if (lane->kind == LaneKind::Bit) {
        const auto beatRange = editableBeatRangeAt(cursorTick_, *lane);
        selectedSegmentLaneId_.clear();
        selectedSegmentId_.clear();
        selectionRange_ = beatRange;
        waveEditHoverLaneId_ = lane->id;
        waveEditHoverRange_ = beatRange;
    } else if (segment) {
        selectedSegmentLaneId_ = lane->id;
        selectedSegmentId_ = segment->id;
        selectionRange_ = std::pair{segment->start, segment->end};
    } else {
        selectedSegmentLaneId_.clear();
        selectedSegmentId_.clear();
        selectionRange_.reset();
    }
    emit selectionChanged(QString::fromStdString(lane->id), cursorTick_);
    viewport()->update();

    QMenu menu(this);
    menu.setObjectName(QStringLiteral("WaveformContextMenu"));
    menu.setToolTipsVisible(true);
    QAction* pasteRange = nullptr;
    const auto* clipboardMime = QApplication::clipboard()->mimeData();
    if (clipboardMime && clipboardMime->hasFormat(kRangeMimeType)) {
        pasteRange = menu.addAction(tr("Paste copied range here"));
        pasteRange->setObjectName(QStringLiteral("PasteRangeHereAction"));
        pasteRange->setShortcut(QKeySequence::Paste);
        auto contextPasteTargetIds = selectedLaneIds_;
        QJsonParseError contextParseError;
        const auto contextDocument = QJsonDocument::fromJson(
            clipboardMime->data(kRangeMimeType),
            &contextParseError);
        if (contextParseError.error == QJsonParseError::NoError
            && contextDocument.isObject()) {
            const auto copiedLaneValues =
                contextDocument.object()
                    .value(QStringLiteral("lanes"))
                    .toArray();
            if (copiedLaneValues.size() > 1) {
                contextPasteTargetIds.clear();
                contextPasteTargetIds.reserve(
                    static_cast<std::size_t>(
                        copiedLaneValues.size()));
                for (const auto& copiedLaneValue : copiedLaneValues) {
                    contextPasteTargetIds.push_back(
                        copiedLaneValue.toObject()
                            .value(QStringLiteral("laneId"))
                            .toString()
                            .toStdString());
                }
            }
        }
        const auto pasteAvailability = pasteAvailabilityWithImpact(
            contextPasteTargetIds,
            cursorTick_);
        pasteRange->setEnabled(pasteAvailability.enabled);
        pasteRange->setToolTip(pasteAvailability.toolTip);
        auto pasteStatusTip = pasteAvailability.toolTip;
        pasteStatusTip.replace(
            QLatin1Char('\n'),
            QStringLiteral(" · "));
        pasteRange->setStatusTip(pasteStatusTip);
        menu.addSeparator();
    }
    QAction* editValue = nullptr;
    QAction* clearValue = nullptr;
    QAction* setZero = nullptr;
    QAction* setOne = nullptr;
    QAction* setX = nullptr;
    QAction* setZ = nullptr;
    QAction* pulse = nullptr;
    QAction* zeroBus = nullptr;
    QAction* dontCare = nullptr;
    QAction* customBus = nullptr;
    QAction* clockGated = nullptr;
    QAction* clockDisabled = nullptr;
    QAction* clockRun = nullptr;
    QAction* duplicateBefore = nullptr;
    QAction* duplicateAfter = nullptr;

    if (lane->kind == LaneKind::Bit) {
        setZero = menu.addAction(tr("Set beat to 0"));
        setOne = menu.addAction(tr("Set beat to 1"));
        setX = menu.addAction(tr("Set beat to X"));
        setZ = menu.addAction(tr("Set beat to Z"));
        menu.addSeparator();
        pulse = menu.addAction(tr("Insert one-beat pulse"));
    } else if (lane->kind == LaneKind::Bus) {
        zeroBus = menu.addAction(tr("Set beat to 0"));
        dontCare = menu.addAction(tr("Don't care"));
        setX = menu.addAction(tr("Set beat to X"));
        setZ = menu.addAction(tr("Set beat to Z"));
        customBus = menu.addAction(tr("Set beat value…"));
        if (segment) {
            menu.addSeparator();
            editValue = menu.addAction(tr("Edit complete segment value…"));
        }
    } else if (lane->kind == LaneKind::Clock) {
        clockGated = menu.addAction(tr("Gate for one period"));
        clockDisabled = menu.addAction(tr("Drive X for one period"));
        clockRun = menu.addAction(tr("Run for one period"));
        if (segment) {
            menu.addSeparator();
            editValue = menu.addAction(tr("Edit complete override…"));
        }
    } else if (segment) {
        editValue = menu.addAction(tr("Edit segment value…"));
    }
    if (segment
        && (lane->kind == LaneKind::Bus || lane->kind == LaneKind::Enum)) {
        duplicateBefore = menu.addAction(tr("Duplicate segment before"));
        duplicateBefore->setObjectName(QStringLiteral("DuplicateSegmentBeforeAction"));
        duplicateAfter = menu.addAction(tr("Duplicate segment after"));
        duplicateAfter->setObjectName(QStringLiteral("DuplicateSegmentAfterAction"));
        const auto configureDuplicate =
            [this](
                QAction* action,
                const SegmentAction segmentAction,
                const QString& baseText) {
                const auto state =
                    selectedSegmentActionState(segmentAction);
                auto text = baseText;
                if (state.valid
                    && state.relationRemovalCount > 0) {
                    text += tr(" ⚠%1").arg(
                        static_cast<qulonglong>(
                            state.relationRemovalCount));
                } else if (state.valid
                           && !state.modelChanges) {
                    text += tr(" · no change");
                } else if (state.applicable
                           && !state.valid) {
                    text += tr(" · unavailable");
                }
                action->setText(text);
                action->setEnabled(
                    state.valid && state.modelChanges);
                action->setToolTip(state.summary);
                action->setStatusTip(state.summary);
                connect(
                    action,
                    &QAction::hovered,
                    this,
                    [this, segmentAction] {
                        static_cast<void>(
                            previewSelectedSegmentAction(
                                segmentAction));
                    });
            };
        configureDuplicate(
            duplicateBefore,
            SegmentAction::DuplicateBefore,
            tr("Duplicate segment before"));
        configureDuplicate(
            duplicateAfter,
            SegmentAction::DuplicateAfter,
            tr("Duplicate segment after"));
        connect(
            &menu,
            &QMenu::aboutToHide,
            this,
            &WaveCanvas::clearSelectedSegmentActionPreview);
        connect(
            &menu,
            &QMenu::hovered,
            this,
            [this, duplicateBefore, duplicateAfter](QAction* hovered) {
                if (hovered != duplicateBefore
                    && hovered != duplicateAfter) {
                    clearSelectedSegmentActionPreview();
                }
            });
    }
    if (segment) {
        menu.addSeparator();
        clearValue = menu.addAction(
            lane->kind == LaneKind::Bit
                ? tr("Clear beat to implicit 0")
                : lane->kind == LaneKind::Bus
                    ? tr("Clear segment to implicit X")
                    : tr("Clear segment"));
    }

    const auto* chosen = menu.exec(event->globalPos());
    if (!chosen) return;
    const auto [beatStart, beatEnd] = editableBeatRangeAt(cursorTick_, *lane);
    if (chosen == pasteRange) {
        pasteAtCursor();
    } else if (chosen == editValue) {
        editSegmentAt(event->pos());
    } else if (chosen == duplicateBefore) {
        static_cast<void>(duplicateSelectedSegmentBefore());
    } else if (chosen == duplicateAfter) {
        static_cast<void>(duplicateSelectedSegmentAfter());
    } else if (chosen == clearValue) {
        if (lane->kind == LaneKind::Bit) {
            static_cast<void>(clearSelectedBeatRange());
        } else {
            clearSelectedSegment();
        }
    } else if (chosen == setZero) {
        setLaneRangeValue(lane->id, beatStart, beatEnd, "0");
    } else if (chosen == setOne) {
        setLaneRangeValue(lane->id, beatStart, beatEnd, "1");
    } else if (chosen == setX) {
        if (lane->kind == LaneKind::Bus) applyBusPreset(lane->id, "x", cursorTick_, false);
        else setLaneRangeValue(lane->id, beatStart, beatEnd, "X");
    } else if (chosen == setZ) {
        if (lane->kind == LaneKind::Bus) applyBusPreset(lane->id, "z", cursorTick_, false);
        else setLaneRangeValue(lane->id, beatStart, beatEnd, "Z");
    } else if (chosen == pulse) {
        insertPulse();
    } else if (chosen == zeroBus) {
        applyBusPreset(lane->id, "zero", cursorTick_, false);
    } else if (chosen == dontCare) {
        applyBusPreset(lane->id, "dont-care", cursorTick_, false);
    } else if (chosen == customBus) {
        promptBusValueAt(lane->id, cursorTick_);
    } else if (chosen == clockGated) {
        setLaneRangeValue(lane->id, beatStart, beatEnd, "gated");
    } else if (chosen == clockDisabled) {
        setLaneRangeValue(lane->id, beatStart, beatEnd, "disabled");
    } else if (chosen == clockRun) {
        clearClockBeat(lane->id, beatStart, beatEnd);
    }
    event->accept();
}

void WaveCanvas::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Control
        && !event->isAutoRepeat()
        && drawing_
        && tool_ == Tool::WaveEdit
        && (waveEditInteraction_ == WaveEditInteraction::MoveRange
            || waveEditInteraction_ == WaveEditInteraction::MoveSegment)) {
        const auto* lane = scenario_
            ? findLane(*scenario_, drawLaneId_)
            : nullptr;
        if (waveEditInteraction_ == WaveEditInteraction::MoveRange
            || (lane && lane->kind != LaneKind::Clock)) {
            waveEditCopyDrag_ = true;
            const auto overlap = waveEditInteraction_
                    == WaveEditInteraction::MoveRange
                && rangeTransferTargetOverlapsSource();
            const auto invalidTarget =
                waveEditInteraction_ == WaveEditInteraction::MoveRange
                && !waveEditRangeTargetValid_;
            viewport()->setCursor(
                invalidTarget || overlap
                    ? Qt::ForbiddenCursor
                    : Qt::DragCopyCursor);
            viewport()->update();
            auto message =
                waveEditInteraction_ == WaveEditInteraction::MoveRange
                ? invalidTarget
                    ? tr("Cannot drop · %1").arg(waveEditRangeTargetError_)
                    : overlap
                        ? tr("Copy target overlaps source signal(s) · move time or signals")
                        : [&] {
                              const auto projection =
                                  buildRangeTransferProjection();
                              return projection
                                  ? rangeTransferPreviewStatus(*projection)
                                  : tr("Copy range preview · source remains · release to copy");
                          }()
                : [&] {
                      const auto projection =
                          buildSegmentEditProjection();
                      return projection
                          ? segmentEditPreviewStatus(*projection)
                          : tr("Copy Segment preview · source remains · release to copy");
                  }();
            emit statusMessage(message);
            event->accept();
            return;
        }
    }

    if (event->key() == Qt::Key_Space
        && event->modifiers() == Qt::NoModifier
        && !event->isAutoRepeat()) {
        spaceHeld_ = true;
        viewport()->setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape && rulerScrubbing_) {
        cancelRulerScrub();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape && headerResizing_) {
        setSignalHeaderWidth(headerResizeOriginalWidth_);
        headerResizing_ = false;
        viewport()->setCursor(defaultCursorShape());
        emit statusMessage(tr("Signal names resize cancelled"));
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape
        && (laneHeaderPressed_ || laneHeaderDragging_)) {
        auto cancellation = laneDragIds_.size() > 1
            ? tr("Move cancelled · %1 selected signals remain in place")
                  .arg(static_cast<qulonglong>(laneDragIds_.size()))
            : tr("Move cancelled");
        if (scenario_ && laneDragIds_.size() <= 1) {
            const auto lane = std::find_if(
                scenario_->lanes.begin(),
                scenario_->lanes.end(),
                [this](const Lane& candidate) { return candidate.id == laneDragId_; });
            if (lane != scenario_->lanes.end()) {
                cancellation = tr("Move cancelled · %1 remains at position %2")
                    .arg(QString::fromStdString(lane->name))
                    .arg(std::distance(scenario_->lanes.begin(), lane) + 1);
            }
        }
        stopLaneDragAutoScroll();
        verticalScrollBar()->setValue(laneDragOriginalVerticalScroll_);
        laneHeaderPressed_ = false;
        laneHeaderDragging_ = false;
        laneHeaderPressPreservesMultiSelection_ = false;
        laneDragId_.clear();
        laneDragIds_.clear();
        laneDropInsertionSlot_.reset();
        laneDropDestinationIndex_.reset();
        laneDropIndicatorY_.reset();
        laneDropGroupId_.clear();
        viewport()->setCursor(defaultCursorShape());
        viewport()->update();
        emit statusMessage(cancellation);
        event->accept();
        return;
    }

    if (scenario_
        && explicitRangeSelection_
        && event->key() == Qt::Key_D
        && event->modifiers() == Qt::ControlModifier) {
        duplicateSelectionAfter();
        event->accept();
        return;
    }

    if (scenario_
        && laneHeaderSelectionActive_
        && selectedLaneIds_.size() > 1) {
        if (event->key() == Qt::Key_Escape) {
            selectedLaneIds_ = {selectedLaneId_};
            laneHeaderSelectionAnchorId_ = selectedLaneId_;
            const auto* lane = findLane(*scenario_, selectedLaneId_);
            emit statusMessage(
                tr("Kept active signal %1 · multi-signal selection cleared")
                    .arg(
                        lane
                            ? QString::fromStdString(lane->name)
                            : QString::fromStdString(selectedLaneId_)));
            viewport()->update();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_F2) {
            emit statusMessage(
                tr("%1 signals selected · rename requires one signal · Ctrl+D duplicates and Delete removes the selection")
                    .arg(static_cast<qulonglong>(selectedLaneIds_.size())));
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_D
            && event->modifiers() == Qt::ControlModifier) {
            emit duplicateLanesRequested(selectedLaneIds());
            event->accept();
            return;
        }
    }

    if (scenario_
        && event->key() == Qt::Key_D
        && event->modifiers() == Qt::ControlModifier) {
        if (!selectedLaneId_.empty()) {
            const auto* lane = findLane(*scenario_, selectedLaneId_);
            if (lane && lane->kind != LaneKind::Group) {
                emit duplicateLaneRequested(QString::fromStdString(lane->id));
                event->accept();
                return;
            }
        }
    }

    if (scenario_ && event->key() == Qt::Key_F2 && !selectedLaneId_.empty()) {
        const auto* lane = findLane(*scenario_, selectedLaneId_);
        if (lane) {
            emit renameLaneRequested(QString::fromStdString(lane->id));
            event->accept();
            return;
        }
    }

    if (scenario_
        && laneHeaderSelectionActive_
        && (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
        && !selectedLaneId_.empty()) {
        if (selectedLaneIds_.size() > 1) {
            emit removeLanesRequested(selectedLaneIds());
        } else {
            emit removeLaneRequested(QString::fromStdString(selectedLaneId_));
        }
        event->accept();
        return;
    }

    if (scenario_
        && (event->key() == Qt::Key_Left
            || event->key() == Qt::Key_Right)
        && event->modifiers() == Qt::NoModifier
        && !selectedLaneId_.empty()) {
        const auto* lane = findLane(*scenario_, selectedLaneId_);
        if (lane && lane->kind == LaneKind::Group) {
            static_cast<void>(setGroupCollapsed(
                QString::fromStdString(lane->id),
                event->key() == Qt::Key_Left));
            event->accept();
            return;
        }
    }

    if (tool_ == Tool::WaveEdit && scenario_) {
        if (event->key() == Qt::Key_Escape
            && drawing_
            && (waveEditInteraction_ == WaveEditInteraction::MoveRange
                || waveEditInteraction_ == WaveEditInteraction::ResizeRangeStart
                || waveEditInteraction_ == WaveEditInteraction::ResizeRangeEnd)) {
            cancelExplicitRangeDrag(
                waveEditDragAutoScrolled_
                    ? tr("Range drag cancelled · selection and view restored")
                    : tr("Range drag cancelled · selection kept"));
            event->accept();
            return;
        }
        if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
            && explicitRangeSelection_) {
            static_cast<void>(clearExplicitRange());
            event->accept();
            return;
        }
        if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
            && selectionRange_
            && selectedSegmentId_.empty()) {
            const auto* lane = findLane(*scenario_, selectedLaneId_);
            if (lane && lane->kind != LaneKind::Group) {
                static_cast<void>(clearSelectedBeatRange());
                event->accept();
                return;
            }
        }
        if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
            && !selectedSegmentId_.empty()) {
            clearSelectedSegment();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Escape && explicitRangeSelection_) {
            if (rangeCloseButton_) rangeCloseButton_->click();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Escape) {
            const auto cancelledWaveEditDrag = drawing_;
            const auto restoredViewport = cancelledWaveEditDrag
                && waveEditDragAutoScrolled_;
            const auto keepWaveTarget = !selectedSegmentId_.empty();
            const auto keptLaneId = selectedLaneId_;
            const auto keptCursorTick = cursorTick_;
            const auto* keptLane = keepWaveTarget
                ? findLane(*scenario_, keptLaneId)
                : nullptr;
            const auto keptLaneName = keptLane
                ? QString::fromStdString(keptLane->name)
                : QString{};
            stopWaveEditDragAutoScroll();
            if (restoredViewport) {
                horizontalScrollBar()->setValue(
                    waveEditDragOriginalHorizontalScroll_);
                verticalScrollBar()->setValue(
                    waveEditDragOriginalVerticalScroll_);
            }
            panning_ = false;
            clearWaveEditState();
            if (keepWaveTarget && !keptLaneId.empty()) {
                selectedLaneId_ = keptLaneId;
                selectedLaneIds_ = {keptLaneId};
                cursorTick_ = keptCursorTick;
                emit selectionChanged(
                    QString::fromStdString(keptLaneId),
                    cursorTick_);
            } else {
                selectedLaneId_.clear();
                selectedLaneIds_.clear();
            }
            laneHeaderSelectionActive_ = false;
            hideBusPresetPalette();
            snapGuideTick_.reset();
            viewport()->setCursor(Qt::PointingHandCursor);
            viewport()->update();
            if (cancelledWaveEditDrag) {
                auto message = restoredViewport
                    ? tr("Waveform drag cancelled · view restored")
                    : tr("Waveform drag cancelled");
                if (keepWaveTarget && !keptLaneName.isEmpty()) {
                    message.append(
                        tr(" · %1 and edit cursor %2 kept")
                            .arg(keptLaneName)
                            .arg(QString::fromStdString(formatTick(
                                cursorTick_,
                                project_->timeBase))));
                }
                emit statusMessage(message);
            } else if (keepWaveTarget && !keptLaneName.isEmpty()) {
                emit statusMessage(
                    tr("Segment selection cleared · signal %1 and edit cursor %2 kept · F6 reselects the Segment here")
                        .arg(keptLaneName)
                        .arg(QString::fromStdString(formatTick(
                            cursorTick_,
                            project_->timeBase))));
            }
            event->accept();
            return;
        }
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
            && event->modifiers() == Qt::NoModifier) {
            const auto* lane = findLane(*scenario_, selectedLaneId_);
            if (drawing_ || laneHeaderPressed_ || laneHeaderDragging_) {
                emit statusMessage(
                    tr("Finish or cancel the current drag before editing a value"));
            } else if (explicitRangeSelection_) {
                const auto rangeKind = explicitRangeKind();
                if (rangeKind && *rangeKind == LaneKind::Clock) {
                    emit statusMessage(
                        tr("Clock range selected · use Run, Gate, or Disable in the range toolbar · R/G/X"));
                } else if (rangeValueEdit_ && rangeValueEdit_->isVisible()) {
                    rangeValueEdit_->setFocus(Qt::OtherFocusReason);
                    rangeValueEdit_->selectAll();
                    emit statusMessage(
                        rangeKind && *rangeKind == LaneKind::Bit
                            ? selectedLaneIds_.size() > 1
                                ? tr("Edit selected Bit range · type one shared 0/1/X/Z pattern, or separate one pattern per signal with / in top-to-bottom order · symbol*N repeats one symbol · Enter applies once · Esc discards the draft")
                                : tr("Edit selected Bit range · type one 0/1/X/Z pattern; symbol*N repeats one symbol · Enter applies once · Esc discards the draft")
                            : tr("Edit selected %1 range · type a value and press Enter · Esc clears the range")
                                  .arg(rangeKind && *rangeKind == LaneKind::Enum
                                           ? tr("Enum")
                                           : tr("Bus")));
                } else {
                    emit statusMessage(
                        tr("Esc clears the selected range before editing one signal beat"));
                }
            } else if (!selectedSegmentId_.empty()
                       && editSelectedSegmentValue()) {
            } else if (!lane
                       || (lane->kind != LaneKind::Bus
                           && lane->kind != LaneKind::Enum)) {
                emit statusMessage(
                    tr("Select a Bus or Enum signal before pressing Enter to edit its value"));
            } else if (scenario_->duration <= 0 || cursorTick_ >= scenario_->duration) {
                emit statusMessage(
                    tr("Timeline End has no editable beat · press Left or Ctrl+Left first"));
            } else {
                promptBusValueAt(lane->id, cursorTick_);
                emit statusMessage(
                    tr("Edit %1 at %2 · current value %3 · Tab applies and advances · Shift+Tab goes back · Enter finishes · Esc cancels")
                        .arg(QString::fromStdString(lane->name))
                        .arg(QString::fromStdString(formatTick(
                            cursorTick_,
                            project_->timeBase)))
                        .arg(laneValueAt(*lane, cursorTick_)));
            }
            event->accept();
            return;
        }
        if ((event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)
            && event->modifiers() == Qt::ShiftModifier) {
            const auto* focusedEditor = qobject_cast<QLineEdit*>(
                QApplication::focusWidget());
            if (focusedEditor && isAncestorOf(focusedEditor)) {
                event->accept();
                return;
            }
            if (drawing_ || laneHeaderPressed_ || laneHeaderDragging_) {
                emit statusMessage(
                    tr("Finish or cancel the current drag before changing range signals"));
            } else {
                adjustRangeSignalsByKeyboard(event->key() == Qt::Key_Down);
            }
            event->accept();
            return;
        }
        if ((event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)
            && event->modifiers() == Qt::NoModifier) {
            const auto* focusedEditor = qobject_cast<QLineEdit*>(
                QApplication::focusWidget());
            if (focusedEditor && isAncestorOf(focusedEditor)) {
                event->accept();
                return;
            }
            if (drawing_ || laneHeaderPressed_ || laneHeaderDragging_) {
                emit statusMessage(
                    tr("Finish or cancel the current drag before changing signals"));
            } else if (explicitRangeSelection_) {
                emit statusMessage(
                    tr("Esc clears the selected range before changing signals"));
            } else {
                selectAdjacentLane(event->key() == Qt::Key_Down);
            }
            event->accept();
            return;
        }
        if ((event->key() == Qt::Key_PageUp
             || event->key() == Qt::Key_PageDown)
            && event->modifiers() == Qt::NoModifier) {
            if (drawing_ || laneHeaderPressed_ || laneHeaderDragging_) {
                emit statusMessage(
                    tr("Finish or cancel the current drag before navigating a page"));
            } else if (explicitRangeSelection_) {
                emit statusMessage(
                    tr("Esc clears the selected range before navigating a page"));
            } else {
                navigateTimelinePage(event->key() == Qt::Key_PageDown);
            }
            event->accept();
            return;
        }
        if ((event->key() == Qt::Key_Home || event->key() == Qt::Key_End)
            && event->modifiers() == Qt::ShiftModifier) {
            const auto* focusedEditor = qobject_cast<QLineEdit*>(
                QApplication::focusWidget());
            if (focusedEditor && isAncestorOf(focusedEditor)) {
                event->accept();
                return;
            }
            if (drawing_ || laneHeaderPressed_ || laneHeaderDragging_) {
                emit statusMessage(
                    tr("Finish or cancel the current drag before selecting to a timeline boundary"));
            } else {
                adjustTimeRangeByKeyboard(
                    event->key() == Qt::Key_End,
                    KeyboardRangeTarget::TimelineBoundary);
            }
            event->accept();
            return;
        }
        if (!drawing_
            && (event->key() == Qt::Key_Home
                || event->key() == Qt::Key_End)
            && (event->modifiers() == Qt::NoModifier
                || event->modifiers() == Qt::ControlModifier)) {
            const auto atStart = event->key() == Qt::Key_Home;
            cursorTick_ = atStart ? Tick{0} : scenario_->duration;
            ensureCursorVisible(cursorTick_);
            snapGuideTick_.reset();
            const auto format = [this](const Tick tick) {
                return QString::fromStdString(
                    formatTick(tick, project_->timeBase));
            };
            emit statusMessage(
                atStart
                    ? tr("Timeline start · edit cursor %1 · End jumps to %2")
                          .arg(format(cursorTick_))
                          .arg(format(scenario_->duration))
                    : tr("Timeline end · edit cursor %1 · Home jumps to %2")
                          .arg(format(cursorTick_))
                          .arg(format(0)));
            viewport()->update();
            event->accept();
            return;
        }
        if ((event->key() == Qt::Key_Left
             || event->key() == Qt::Key_Right)
            && event->modifiers()
                == (Qt::ControlModifier | Qt::ShiftModifier)) {
            const auto* focusedEditor = qobject_cast<QLineEdit*>(
                QApplication::focusWidget());
            if (focusedEditor && isAncestorOf(focusedEditor)) {
                event->accept();
                return;
            }
            if (drawing_ || laneHeaderPressed_ || laneHeaderDragging_) {
                emit statusMessage(
                    tr("Finish or cancel the current drag before selecting to a signal edge"));
            } else {
                adjustTimeRangeByKeyboard(
                    event->key() == Qt::Key_Right,
                    KeyboardRangeTarget::SignalEdge);
            }
            event->accept();
            return;
        }
        if ((event->key() == Qt::Key_Left
             || event->key() == Qt::Key_Right)
            && event->modifiers() == Qt::ControlModifier) {
            const auto forward = event->key() == Qt::Key_Right;
            const auto* lane = findLane(*scenario_, selectedLaneId_);
            if (drawing_) {
                emit statusMessage(
                    tr("Finish or cancel the current drag before navigating edges"));
            } else if (!lane || lane->kind == LaneKind::Group) {
                emit statusMessage(
                    tr("Select a signal before using Ctrl+Left or Ctrl+Right"));
            } else if (const auto edge = adjacentEdgeTick(
                           *lane,
                           cursorTick_,
                           forward)) {
                cursorTick_ = *edge;
                ensureCursorVisible(cursorTick_);
                snapGuideTick_.reset();
                auto message =
                    forward
                        ? tr("Next edge on %1 · %2 · value %3 · Ctrl+Left goes back")
                              .arg(QString::fromStdString(lane->name))
                              .arg(QString::fromStdString(formatTick(
                                  cursorTick_,
                                  project_->timeBase)))
                              .arg(laneValueAt(*lane, cursorTick_))
                        : tr("Previous edge on %1 · %2 · value %3 · Ctrl+Right goes forward")
                              .arg(QString::fromStdString(lane->name))
                              .arg(QString::fromStdString(formatTick(
                                  cursorTick_,
                                  project_->timeBase)))
                              .arg(laneValueAt(*lane, cursorTick_));
                if (lane->kind == LaneKind::Bus || lane->kind == LaneKind::Enum) {
                    message.append(tr(" · Enter edits value"));
                } else if (lane->kind == LaneKind::Clock) {
                    message.append(tr(" · G gates · X drives unknown · R runs"));
                }
                emit statusMessage(message);
                viewport()->update();
            } else {
                emit statusMessage(
                    forward
                        ? tr("No later edge on %1 · End jumps to %2")
                              .arg(QString::fromStdString(lane->name))
                              .arg(QString::fromStdString(formatTick(
                                  scenario_->duration,
                                  project_->timeBase)))
                        : tr("No earlier edge on %1 · Home jumps to 0")
                              .arg(QString::fromStdString(lane->name)));
            }
            event->accept();
            return;
        }
        if ((event->key() == Qt::Key_Left
             || event->key() == Qt::Key_Right)
            && event->modifiers() == Qt::ShiftModifier) {
            if (drawing_ || laneHeaderPressed_ || laneHeaderDragging_) {
                emit statusMessage(
                    tr("Finish or cancel the current drag before selecting a time range"));
            } else {
                adjustTimeRangeByKeyboard(event->key() == Qt::Key_Right);
            }
            event->accept();
            return;
        }
        if ((event->key() == Qt::Key_Left || event->key() == Qt::Key_Right)
            && event->modifiers() == Qt::NoModifier) {
            const auto forward = event->key() == Qt::Key_Right;
            const auto* lane = findLane(*scenario_, selectedLaneId_);
            const auto next = adjacentKeyboardTick(
                cursorTick_,
                forward,
                lane);
            if (!next) {
                emit statusMessage(
                    forward
                        ? tr("Timeline end reached · edit cursor unchanged · Left moves back")
                        : tr("Timeline start reached · edit cursor unchanged · Right moves forward"));
                event->accept();
                return;
            }
            cursorTick_ = *next;
            ensureCursorVisible(cursorTick_);
            snapGuideTick_.reset();
            emit statusMessage(
                tr("Edit cursor %1 · %2")
                    .arg(QString::fromStdString(
                        formatTick(cursorTick_, project_->timeBase)))
                    .arg(editTimingSummary()));
            viewport()->update();
            event->accept();
            return;
        }
        if (event->modifiers() == Qt::NoModifier
            && (event->key() == Qt::Key_G
                || event->key() == Qt::Key_X
                || event->key() == Qt::Key_R)) {
            const auto* lane = findLane(*scenario_, selectedLaneId_);
            if (explicitRangeSelection_
                && lane
                && lane->kind == LaneKind::Clock) {
                const auto kind = explicitRangeKind();
                if (kind && *kind == LaneKind::Clock) {
                    if (event->key() == Qt::Key_G) {
                        applyExplicitRangePreset("clock-gate");
                    } else if (event->key() == Qt::Key_X) {
                        applyExplicitRangePreset("clock-disable");
                    } else {
                        static_cast<void>(clearExplicitRange());
                    }
                } else {
                    emit statusMessage(
                        tr("No values changed · select only Clock signals before using R/G/X on a range"));
                }
                event->accept();
                return;
            }
            if (lane && lane->kind == LaneKind::Clock) {
                const auto [start, end] = editableBeatRangeAt(cursorTick_, *lane);
                if (event->key() == Qt::Key_G) {
                    static_cast<void>(
                        setLaneRangeValue(lane->id, start, end, "gated"));
                } else if (event->key() == Qt::Key_X) {
                    static_cast<void>(
                        setLaneRangeValue(lane->id, start, end, "disabled"));
                } else {
                    clearClockBeat(lane->id, start, end);
                }
                event->accept();
                return;
            }
        }
        if (event->modifiers() == Qt::NoModifier
            && (event->key() == Qt::Key_0
                || event->key() == Qt::Key_1
                || event->key() == Qt::Key_X
                || event->key() == Qt::Key_Z)) {
            const auto value = event->key() == Qt::Key_0
                ? std::string{"0"}
                : event->key() == Qt::Key_1
                    ? std::string{"1"}
                    : event->key() == Qt::Key_X
                        ? std::string{"X"}
                        : std::string{"Z"};
            if (explicitRangeSelection_) {
                const auto kind = explicitRangeKind();
                if (!kind) {
                    emit statusMessage(
                        tr("No values changed · select only Clock, only Bit, only Bus, or only Enum signals"));
                } else if (*kind == LaneKind::Bit) {
                    static_cast<void>(applyExplicitRangeValue(value));
                } else if (*kind == LaneKind::Enum) {
                    if (event->key() == Qt::Key_0
                        || event->key() == Qt::Key_1) {
                        static_cast<void>(applyExplicitRangeValue(value));
                    } else {
                        emit statusMessage(
                            tr("No values changed · type a declared symbol or numeric value in the Enum range field"));
                    }
                } else if (*kind == LaneKind::Clock) {
                    emit statusMessage(
                        tr("No values changed · use R/G/X or Run/Gate/Disable for Clock ranges"));
                } else if (event->key() == Qt::Key_0) {
                    applyExplicitRangePreset("zero");
                } else if (event->key() == Qt::Key_X) {
                    applyExplicitRangePreset("x");
                } else if (event->key() == Qt::Key_Z) {
                    applyExplicitRangePreset("z");
                } else {
                    emit statusMessage(
                        tr("No values changed · use the Bus value field to assign 1"));
                }
                event->accept();
                return;
            }
            const auto* lane = findLane(*scenario_, selectedLaneId_);
            if (lane
                && (lane->kind == LaneKind::Bus || lane->kind == LaneKind::Enum)
                && !selectedSegmentId_.empty()
                && editSelectedSegmentValue(QString::fromLatin1(
                    event->key() == Qt::Key_0
                        ? "0"
                        : event->key() == Qt::Key_1
                            ? "1"
                            : event->key() == Qt::Key_X
                                ? "X"
                                : "Z"))) {
                event->accept();
                return;
            }
            if (lane && lane->kind == LaneKind::Bit) {
                const auto [start, end] = editableBeatRangeAt(cursorTick_, *lane);
                setLaneRangeValue(lane->id, start, end, value);
                event->accept();
                return;
            }
            if (lane && lane->kind == LaneKind::Bus) {
                if (event->key() == Qt::Key_0) {
                    applyBusPreset(lane->id, "zero", cursorTick_, false);
                } else if (event->key() == Qt::Key_X) {
                    applyBusPreset(lane->id, "x", cursorTick_, false);
                } else if (event->key() == Qt::Key_Z) {
                    applyBusPreset(lane->id, "z", cursorTick_, false);
                } else {
                    promptBusValueAt(lane->id, cursorTick_);
                    busValueEdit_->setText(QStringLiteral("1"));
                    busValueEdit_->setModified(true);
                    busValueEdit_->setFocus(Qt::OtherFocusReason);
                    updateBusEditActionStates(true);
                }
                event->accept();
                return;
            }
            if (lane && lane->kind == LaneKind::Enum
                && (event->key() == Qt::Key_0 || event->key() == Qt::Key_1)) {
                promptBusValueAt(lane->id, cursorTick_);
                busValueEdit_->setText(QString::fromLatin1(event->key() == Qt::Key_0 ? "0" : "1"));
                busValueEdit_->setModified(true);
                busValueEdit_->setFocus(Qt::OtherFocusReason);
                updateBusEditActionStates(true);
                event->accept();
                return;
            }
        }
        if (event->modifiers() == Qt::NoModifier) {
            const auto* lane = findLane(*scenario_, selectedLaneId_);
            const auto text = event->text();
            if (lane
                && (lane->kind == LaneKind::Bus || lane->kind == LaneKind::Enum)
                && !selectedSegmentId_.empty()
                && text.size() == 1
                && !text.front().isSpace()
                && text.front().isPrint()
                && editSelectedSegmentValue(text)) {
                event->accept();
                return;
            }
            if (lane && lane->kind == LaneKind::Bus
                && event->key() == Qt::Key_Question) {
                applyBusPreset(lane->id, "dont-care", cursorTick_, false);
                event->accept();
                return;
            }
            if (lane
                && (lane->kind == LaneKind::Bus || lane->kind == LaneKind::Enum)
                && text.size() == 1
                && !text.front().isSpace()
                && text.front().isPrint()) {
                promptBusValueAt(lane->id, cursorTick_);
                busValueEdit_->setText(text);
                busValueEdit_->setModified(true);
                busValueEdit_->setFocus(Qt::OtherFocusReason);
                updateBusEditActionStates(true);
                event->accept();
                return;
            }
        }
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }

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
            emit statusMessage(cursorMeasurementText());
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
        emit measureModeExitRequested();
        event->accept();
        return;
    }

    QAbstractScrollArea::keyPressEvent(event);
}

void WaveCanvas::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Control
        && !event->isAutoRepeat()
        && drawing_
        && tool_ == Tool::WaveEdit
        && (waveEditInteraction_ == WaveEditInteraction::MoveRange
            || waveEditInteraction_ == WaveEditInteraction::MoveSegment)) {
        waveEditCopyDrag_ = false;
        viewport()->setCursor(
            waveEditInteraction_ == WaveEditInteraction::MoveRange
                    && !waveEditRangeTargetValid_
                ? Qt::ForbiddenCursor
                : Qt::SizeAllCursor);
        viewport()->update();
        auto message =
            waveEditInteraction_ == WaveEditInteraction::MoveRange
            ? !waveEditRangeTargetValid_
                ? tr("Cannot drop · %1").arg(waveEditRangeTargetError_)
                : [&] {
                      const auto projection =
                          buildRangeTransferProjection();
                      return projection
                          ? rangeTransferPreviewStatus(*projection)
                          : tr("Move range preview · source clears on release");
                  }()
            : [&] {
                  const auto projection =
                      buildSegmentEditProjection();
                  return projection
                      ? segmentEditPreviewStatus(*projection)
                      : tr("Move Segment preview · source moves on release");
              }();
        emit statusMessage(message);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Space
        && event->modifiers() == Qt::NoModifier
        && !event->isAutoRepeat()) {
        spaceHeld_ = false;
        if (!panning_) {
            viewport()->setCursor(defaultCursorShape());
        }
        event->accept();
        return;
    }
    QAbstractScrollArea::keyReleaseEvent(event);
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
    const auto rangeTransferActive =
        drawing_
        && waveEditInteraction_ == WaveEditInteraction::MoveRange;
    const auto activeRangeTransferProjection =
        rangeTransferActive
        ? buildRangeTransferProjection()
        : std::nullopt;
    const auto currentSegmentEditProjection =
        activeSegmentEditProjection();
    const auto segmentEditActive =
        currentSegmentEditProjection.has_value();
    const auto* currentBusEditProjection =
        busEditActionPreview_
        && busPresetPalette_
        && busPresetPalette_->isVisible()
        && busEditActionPreview_->laneId == busPresetLaneId_
        && busEditRange_
        && busEditActionPreview_->start == busEditRange_->first
        && (busEditActionPreview_->sequenceSteps.size() > 1
            || busEditActionPreview_->end == busEditRange_->second)
        ? &*busEditActionPreview_
        : nullptr;
    const auto busEditPreviewActive =
        currentBusEditProjection != nullptr;
    const auto* currentBitPatternProjection =
        bitPatternPreview_
        && rangeEditPalette_
        && rangeEditPalette_->isVisible()
        && rangeValueEdit_
        && rangeValueEdit_->isModified()
        && explicitRangeSelection_
        && selectionRange_
        && bitPatternPreview_->laneIds
            == selectedLaneIds_
        && bitPatternPreview_->start == selectionRange_->first
        && bitPatternPreview_->end == selectionRange_->second
        ? &*bitPatternPreview_
        : nullptr;
    const auto bitPatternPreviewActive =
        currentBitPatternProjection != nullptr;
    const auto* currentBusRangeSequenceProjection =
        busRangeSequencePreview_
        && rangeEditPalette_
        && rangeEditPalette_->isVisible()
        && rangeValueEdit_
        && rangeValueEdit_->isModified()
        && explicitRangeSelection_
        && selectionRange_
        && busRangeSequencePreview_->laneIds
            == selectedLaneIds_
        && busRangeSequencePreview_->start
            == selectionRange_->first
        && busRangeSequencePreview_->end
            == selectionRange_->second
        ? &*busRangeSequencePreview_
        : nullptr;
    const auto busRangeSequencePreviewActive =
        currentBusRangeSequenceProjection != nullptr;
    const auto activeRepeatPreview =
        !rangeTransferActive
            && !segmentEditActive
            && !busEditPreviewActive
            && !bitPatternPreviewActive
            && !busRangeSequencePreviewActive
            && rangeRepeatPreviewActive_
        ? buildRepeatPreview()
        : std::nullopt;
    const auto activePastePreview = activeRepeatPreview
            || rangeTransferActive
            || segmentEditActive
            || busEditPreviewActive
            || bitPatternPreviewActive
            || busRangeSequencePreviewActive
        ? std::nullopt
        : pastePreview();
    const auto* pastePreviewData = activeRepeatPreview
        ? &*activeRepeatPreview
        : activePastePreview
            ? &*activePastePreview
            : nullptr;
    drawLaneHeaderPastePreview(painter, pastePreviewData);
    if (currentBusEditProjection) {
        drawProjectedLaneWaveformPreview(
            painter,
            currentBusEditProjection->lane,
            currentBusEditProjection->start,
            currentBusEditProjection->end,
            currentBusEditProjection->relationImpact,
            currentBusEditProjection->modelChanges);
    }
    if (currentBitPatternProjection) {
        for (const auto& projectedLane
             : currentBitPatternProjection->lanes) {
            drawProjectedLaneWaveformPreview(
                painter,
                projectedLane,
                currentBitPatternProjection->start,
                currentBitPatternProjection->end,
                currentBitPatternProjection->relationImpact,
                currentBitPatternProjection->modelChanges);
        }
    }
    if (currentBusRangeSequenceProjection) {
        for (const auto& projectedLane
             : currentBusRangeSequenceProjection->lanes) {
            drawProjectedLaneWaveformPreview(
                painter,
                projectedLane,
                currentBusRangeSequenceProjection->start,
                currentBusRangeSequenceProjection->end,
                currentBusRangeSequenceProjection->relationImpact,
                currentBusRangeSequenceProjection->modelChanges);
        }
    }
    drawScenarioOverlays(
        painter,
        visibleStart,
        visibleEnd,
        currentSegmentEditProjection
            ? &currentSegmentEditProjection->relationImpact.ids
            : activeRangeTransferProjection
                ? &activeRangeTransferProjection->relationImpact.ids
            : currentBusEditProjection
                ? &currentBusEditProjection->relationImpact.ids
            : currentBitPatternProjection
                ? &currentBitPatternProjection->relationImpact.ids
            : currentBusRangeSequenceProjection
                ? &currentBusRangeSequenceProjection->relationImpact.ids
            : pastePreviewData
                ? &pastePreviewData->relationRemovalIds
                : nullptr);
    drawWaveEditOverlay(
        painter,
        activeRangeTransferProjection
            ? &*activeRangeTransferProjection
            : nullptr,
        currentSegmentEditProjection
            ? &*currentSegmentEditProjection
            : nullptr,
        currentBusEditProjection);
    drawEditGuide(painter);
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
    } else if (drawing_
               && (tool_ != Tool::WaveEdit
                   || waveEditInteraction_ == WaveEditInteraction::SelectRange)
               && !drawLaneId_.empty()) {
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
            const auto rangeSelecting = tool_ == Tool::Selection
                || waveEditInteraction_ == WaveEditInteraction::SelectRange;
            const auto top = rangeSelecting
                ? std::max(RulerHeight, std::min(selectionStartY_, interactionCurrent_.y()))
                : y;
            const auto bottom = rangeSelecting
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
            if (rangeSelecting && right > left) {
                const auto rangeStart = std::min(drawStart_, drawCurrent_);
                const auto rangeEnd = std::max(drawStart_, drawCurrent_);
                const auto timingLabel = tr("%1–%2 · width %3")
                    .arg(
                        QString::fromStdString(formatTick(
                            rangeStart,
                            project_->timeBase)),
                        QString::fromStdString(formatTick(
                            rangeEnd,
                            project_->timeBase)),
                        QString::fromStdString(formatTick(
                            rangeEnd - rangeStart,
                            project_->timeBase)));
                const auto labelWidth = std::min(
                    painter.fontMetrics().horizontalAdvance(timingLabel) + 16,
                    std::max(0, viewport()->width() - headerWidth_ - 8));
                const auto maximumLeft = std::max(
                    headerWidth_ + 4,
                    viewport()->width() - labelWidth - 4);
                const auto labelLeft = std::clamp(
                    left + 8,
                    headerWidth_ + 4,
                    maximumLeft);
                const QRect labelRect(
                    labelLeft,
                    std::max(RulerHeight + 3, top + 3),
                    labelWidth,
                    22);
                painter.fillRect(labelRect, QColor(16, 25, 39, 224));
                painter.setPen(QColor(225, 239, 255));
                painter.drawText(
                    labelRect.adjusted(8, 0, -8, 0),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    timingLabel);
            }
        }
    }

    drawLaneReorderOverlay(painter);

    painter.fillRect(QRect(0, 0, headerWidth_, RulerHeight), kHeaderBackground);
    painter.setPen(kTextPrimary);
    QFont titleFont = painter.font();
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.drawText(
        QRect(14, 0, headerWidth_ - 20, RulerHeight),
        Qt::AlignVCenter | Qt::AlignLeft,
        tr("Signals"));
    painter.setPen(kGridMajor);
    painter.drawLine(headerWidth_ - 1, 0, headerWidth_ - 1, viewport()->height());
}

void WaveCanvas::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    if (isVisible() && pendingVisibleTimeSpanRestore_) {
        const auto [span, anchorTick] = *pendingVisibleTimeSpanRestore_;
        applyVisibleTimeSpan(span, anchorTick);
        schedulePendingVisibleTimeSpanRestore();
    } else if (fitPending_ && viewport()->width() > headerWidth_ + 40) {
        fitPending_ = false;
        fitScenario();
    } else {
        updateScrollBars();
    }
    updateAddLaneButtonGeometry();
    positionQuickLaneSetup();
    positionLaneRename();
    positionDurationEditor();
    positionBusPresetPalette();
}

void WaveCanvas::showEvent(QShowEvent* event)
{
    QAbstractScrollArea::showEvent(event);
    if (!pendingVisibleTimeSpanRestore_) return;
    const auto [span, anchorTick] = *pendingVisibleTimeSpanRestore_;
    applyVisibleTimeSpan(span, anchorTick);
    schedulePendingVisibleTimeSpanRestore();
}

bool WaveCanvas::beginExplicitRangeMove(
    const QPoint& position,
    const Qt::KeyboardModifiers modifiers)
{
    if (!scenario_
        || tool_ != Tool::WaveEdit
        || !explicitRangeSelection_
        || !selectionRange_
        || spaceHeld_
        || modifiers.testFlag(Qt::ShiftModifier)
        || explicitRangeBoundaryAt(position)
            != SegmentBoundary::None) {
        return false;
    }
    const auto* rangeLane = laneAtY(position.y());
    if (!rangeLane
        || !explicitRangeContains(position)) {
        return false;
    }
    const auto rawTick = std::clamp<Tick>(
        tickAtX(position.x()),
        selectionRange_->first,
        selectionRange_->second);
    bypassSnap_ = modifiers.testFlag(
        Qt::AltModifier);
    viewport()->setFocus(Qt::MouseFocusReason);
    clearRangeSequenceCaretTarget();
    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    waveEditHoverLaneId_.clear();
    waveEditHoverRange_.reset();
    waveEditOriginalRange_ = selectionRange_;
    waveEditPreviewRange_ = selectionRange_;
    waveEditInteraction_ =
        WaveEditInteraction::MoveRange;
    const auto pressedLane = std::find(
        selectedLaneIds_.begin(),
        selectedLaneIds_.end(),
        rangeLane->id);
    waveEditRangeGrabLaneOffset_ =
        pressedLane == selectedLaneIds_.end()
        ? 0
        : static_cast<std::size_t>(
              std::distance(
                  selectedLaneIds_.begin(),
                  pressedLane));
    updateRangeTransferTargetLanes(rangeLane);
    waveEditCopyDrag_ = modifiers.testFlag(
        Qt::ControlModifier);
    waveEditGrabOffset_ =
        rawTick - selectionRange_->first;
    drawLaneId_ = rangeLane->id;
    drawStart_ = selectionRange_->first;
    drawCurrent_ = drawStart_;
    waveEditPressPosition_ = position;
    stopWaveEditDragAutoScroll();
    waveEditDragOriginalHorizontalScroll_ =
        horizontalScrollBar()->value();
    waveEditDragOriginalVerticalScroll_ =
        verticalScrollBar()->value();
    waveEditDragAutoScrolled_ = false;
    drawing_ = true;
    cursorTick_ = selectionRange_->first;
    snapGuideTick_.reset();
    viewport()->setCursor(
        waveEditCopyDrag_
            ? Qt::DragCopyCursor
            : Qt::SizeAllCursor);
    viewport()->update();
    return true;
}

void WaveCanvas::mousePressEvent(QMouseEvent* event)
{
    if (!scenario_) {
        QAbstractScrollArea::mousePressEvent(event);
        return;
    }
    auto position = event->position().toPoint();
    if (event->button() == Qt::LeftButton) {
        rangeSequenceTokenPress_ = false;
        rangeSequenceTokenDragRejected_ = false;
        rangeSequenceTokenPressPosition_ = {};
        durationEditPointerRetargetTick_.reset();
    }
    clearSelectedSegmentActionPreview();
    if (event->button() == Qt::LeftButton
        && durationEdit_
        && durationEdit_->isVisible()
        && durationEdit_->isModified()) {
        const auto retargetTick = tool_ == Tool::WaveEdit
                && position.x() >= headerWidth_
                && position.y() >= RulerHeight
            ? std::optional<Tick>{std::clamp<Tick>(
                  tickAtX(position.x()),
                  0,
                  std::max<Tick>(0, scenario_->duration - 1))}
            : std::nullopt;
        if (!commitPendingInlineEdits()) {
            event->accept();
            return;
        }
        if (retargetTick) {
            durationEditPointerRetargetTick_ = *retargetTick;
            durationEditPointerRetargetPressPosition_ =
                event->position().toPoint();
            position.setX(xAtTick(*retargetTick));
        }
    }
    auto rangeClearedForRetarget = false;
    std::optional<HistorySelectionSnapshot> rulerSelectionBeforeRetarget;
    if (event->button() == Qt::LeftButton && hasLaneRename()) {
        submitLaneRename();
        if (hasLaneRename()) {
            event->accept();
            return;
        }
    }
    if (event->button() == Qt::LeftButton && hasQuickLaneSetup()) {
        if (!commitPendingInlineEdits()) {
            event->accept();
            return;
        }
    }
    if (event->button() == Qt::LeftButton
        && event->modifiers() == Qt::NoModifier
        && rangeSequenceTokenAt(position)) {
        rangeSequenceTokenPress_ = true;
        rangeSequenceTokenPressPosition_ = position;
        QToolTip::hideText();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && hasPendingValueEdit()) {
        if (!commitPendingInlineEdits()) {
            event->accept();
            return;
        }
    }
    if (event->button() == Qt::LeftButton && signalHeaderDividerAt(position)) {
        setFocus(Qt::MouseFocusReason);
        headerResizing_ = true;
        headerResizePressX_ = position.x();
        headerResizeOriginalWidth_ = headerWidth_;
        viewport()->setCursor(Qt::SplitHCursor);
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton
        && tool_ == Tool::WaveEdit
        && position.y() < RulerHeight
        && position.x() >= headerWidth_) {
        rulerSelectionBeforeRetarget = historySelectionSnapshot();
    }
    if (event->button() == Qt::LeftButton
        && tool_ == Tool::WaveEdit
        && explicitRangeSelection_
        && !spaceHeld_
        && !event->modifiers().testFlag(Qt::ShiftModifier)) {
        const auto boundary = explicitRangeBoundaryAt(position);
        const auto* rangeLane = laneAtY(position.y());
        if (boundary != SegmentBoundary::None && selectionRange_ && rangeLane) {
            bypassSnap_ = event->modifiers().testFlag(Qt::AltModifier);
            viewport()->setFocus(Qt::MouseFocusReason);
            selectedSegmentLaneId_.clear();
            selectedSegmentId_.clear();
            waveEditHoverLaneId_.clear();
            waveEditHoverRange_.reset();
            waveEditOriginalRange_ = selectionRange_;
            waveEditPreviewRange_ = selectionRange_;
            waveEditRangeTargetLaneIds_.clear();
            waveEditRangeTargetValid_ = true;
            waveEditRangeTargetError_.clear();
            waveEditInteraction_ = boundary == SegmentBoundary::Start
                ? WaveEditInteraction::ResizeRangeStart
                : WaveEditInteraction::ResizeRangeEnd;
            drawLaneId_ = rangeLane->id;
            drawStart_ = boundary == SegmentBoundary::Start
                ? selectionRange_->first
                : selectionRange_->second;
            drawCurrent_ = drawStart_;
            waveEditPressPosition_ = position;
            stopWaveEditDragAutoScroll();
            waveEditDragOriginalHorizontalScroll_ =
                horizontalScrollBar()->value();
            waveEditDragOriginalVerticalScroll_ =
                verticalScrollBar()->value();
            waveEditDragAutoScrolled_ = false;
            drawing_ = true;
            cursorTick_ = drawStart_;
            snapGuideTick_.reset();
            viewport()->setCursor(Qt::SplitHCursor);
            viewport()->update();
            event->accept();
            return;
        }
        if (beginExplicitRangeMove(
                position,
                event->modifiers())) {
            event->accept();
            return;
        }
        const auto rulerRetarget =
            position.y() < RulerHeight && position.x() >= headerWidth_;
        clearExplicitRangeSelection(!rulerRetarget);
        if (rulerRetarget && !selectedLaneIds_.empty()) {
            laneHeaderSelectionActive_ = true;
            laneHeaderSelectionAnchorId_ = selectedLaneId_;
        }
        snapGuideTick_.reset();
        viewport()->setFocus(Qt::MouseFocusReason);
        const auto* retargetLane = laneAtY(position.y());
        const auto retargetsWithoutEditing = position.y() < RulerHeight
            || position.x() < headerWidth_
            || (position.x() >= headerWidth_
                && retargetLane
                && retargetLane->kind != LaneKind::Bit
                && retargetLane->kind != LaneKind::Group);
        if (retargetsWithoutEditing) {
            rangeClearedForRetarget = true;
        } else {
            emit statusMessage(tr("Range selection cleared"));
            event->accept();
            return;
        }
    }
    if (event->button() == Qt::MiddleButton
        || (event->button() == Qt::LeftButton && spaceHeld_)) {
        setFocus(Qt::MouseFocusReason);
        panning_ = true;
        panPressPosition_ = position;
        panStartHorizontal_ = horizontalScrollBar()->value();
        panStartVertical_ = verticalScrollBar()->value();
        viewport()->setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    if (event->button() != Qt::LeftButton) {
        QAbstractScrollArea::mousePressEvent(event);
        return;
    }
    if (tool_ == Tool::WaveEdit
        && position.y() < RulerHeight
        && position.x() >= headerWidth_) {
        setFocus(Qt::MouseFocusReason);
        rulerScrubbing_ = true;
        rulerScrubClearedRange_ = rangeClearedForRetarget;
        rulerScrubOriginalTick_ = cursorTick_;
        rulerScrubOriginalHorizontalScroll_ = horizontalScrollBar()->value();
        rulerScrubOriginalSelection_ = rulerSelectionBeforeRetarget
            ? rulerSelectionBeforeRetarget
            : std::optional<HistorySelectionSnapshot>{
                  historySelectionSnapshot()};
        hideBusPresetPalette();
        updateRulerScrub(position.x(), false, rangeClearedForRetarget);
        viewport()->setCursor(Qt::SizeHorCursor);
        event->accept();
        return;
    }
    if (addLaneRowRect().contains(position)) return;
    auto* lane = laneAtY(position.y());
    bypassSnap_ = event->modifiers().testFlag(Qt::AltModifier);
    const auto startingExplicitRange = tool_ == Tool::WaveEdit
        && event->modifiers().testFlag(Qt::ShiftModifier);
    if (!startingExplicitRange) hideBusPresetPalette();

    if (position.x() < headerWidth_
        && lane
        && lane->kind == LaneKind::Group
        && groupDisclosureRect(*lane).contains(position)) {
        setFocus(Qt::MouseFocusReason);
        laneHeaderPressed_ = false;
        laneHeaderDragging_ = false;
        laneHeaderPressPreservesMultiSelection_ = false;
        laneDragId_.clear();
        laneDragIds_.clear();
        laneDropInsertionSlot_.reset();
        laneDropDestinationIndex_.reset();
        laneDropIndicatorY_.reset();
        laneDropGroupId_.clear();
        static_cast<void>(setGroupCollapsed(
            QString::fromStdString(lane->id),
            !collapsedGroupIds_.contains(lane->id)));
        event->accept();
        return;
    }

    if (position.x() < headerWidth_ && lane) {
        setFocus(Qt::MouseFocusReason);
        if (tool_ == Tool::WaveEdit) clearWaveEditState();
        const auto lockedMarkerDeselected =
            tool_ == Tool::Marker && !selectedMarkerId_.empty();
        if (tool_ == Tool::Marker) {
            selectedMarkerId_.clear();
            cursorInteraction_ = CursorInteraction::None;
            lockedMarkerOriginalRange_.reset();
        }

        const auto modifiesSignalSelection =
            lane->kind != LaneKind::Group
            && (event->modifiers().testFlag(Qt::ControlModifier)
                || event->modifiers().testFlag(Qt::ShiftModifier));
        const auto preservesMultiSelectionForPotentialDrag =
            lane->kind != LaneKind::Group
            && event->modifiers() == Qt::NoModifier
            && laneHeaderSelectionActive_
            && selectedLaneIds_.size() > 1
            && std::find(
                   selectedLaneIds_.begin(),
                   selectedLaneIds_.end(),
                   lane->id)
                != selectedLaneIds_.end();
        if (modifiesSignalSelection) {
            std::vector<const Lane*> visibleSignals;
            visibleSignals.reserve(laneLayout_.size());
            for (const auto& layout : laneLayout_) {
                const auto& candidate = scenario_->lanes.at(layout.laneIndex);
                if (candidate.kind != LaneKind::Group) {
                    visibleSignals.push_back(&candidate);
                }
            }
            const auto visibleIndex =
                [&visibleSignals](const std::string& laneId)
                    -> std::optional<std::size_t> {
                const auto found = std::find_if(
                    visibleSignals.begin(),
                    visibleSignals.end(),
                    [&laneId](const Lane* candidate) {
                        return candidate->id == laneId;
                    });
                if (found == visibleSignals.end()) return std::nullopt;
                return static_cast<std::size_t>(
                    std::distance(visibleSignals.begin(), found));
            };
            const auto validExistingSelection =
                laneHeaderSelectionActive_
                && !selectedLaneIds_.empty()
                && std::all_of(
                    selectedLaneIds_.begin(),
                    selectedLaneIds_.end(),
                    [&visibleIndex](const std::string& laneId) {
                        return visibleIndex(laneId).has_value();
                    });
            if (!validExistingSelection) {
                selectedLaneIds_ = {lane->id};
                selectedLaneId_ = lane->id;
                laneHeaderSelectionAnchorId_ = lane->id;
            } else if (event->modifiers().testFlag(Qt::ShiftModifier)) {
                auto anchorIndex = visibleIndex(laneHeaderSelectionAnchorId_);
                if (!anchorIndex) anchorIndex = visibleIndex(selectedLaneId_);
                const auto activeIndex = visibleIndex(lane->id);
                if (!anchorIndex || !activeIndex) {
                    selectedLaneIds_ = {lane->id};
                    laneHeaderSelectionAnchorId_ = lane->id;
                } else {
                    const auto first = std::min(*anchorIndex, *activeIndex);
                    const auto last = std::max(*anchorIndex, *activeIndex);
                    selectedLaneIds_.clear();
                    selectedLaneIds_.reserve(last - first + 1);
                    for (auto index = first; index <= last; ++index) {
                        selectedLaneIds_.push_back(
                            visibleSignals.at(index)->id);
                    }
                }
                selectedLaneId_ = lane->id;
            } else {
                const auto selected = std::find(
                    selectedLaneIds_.begin(),
                    selectedLaneIds_.end(),
                    lane->id);
                if (selected == selectedLaneIds_.end()) {
                    selectedLaneIds_.push_back(lane->id);
                    selectedLaneId_ = lane->id;
                    laneHeaderSelectionAnchorId_ = lane->id;
                } else if (selectedLaneIds_.size() > 1) {
                    selectedLaneIds_.erase(selected);
                    selectedLaneId_ = selectedLaneIds_.back();
                    laneHeaderSelectionAnchorId_ = selectedLaneId_;
                }

                std::vector<std::string> ordered;
                ordered.reserve(selectedLaneIds_.size());
                for (const auto* visible : visibleSignals) {
                    if (std::find(
                            selectedLaneIds_.begin(),
                            selectedLaneIds_.end(),
                            visible->id)
                        != selectedLaneIds_.end()) {
                        ordered.push_back(visible->id);
                    }
                }
                selectedLaneIds_ = std::move(ordered);
            }
        } else if (preservesMultiSelectionForPotentialDrag) {
            selectedLaneId_ = lane->id;
            std::vector<std::string> ordered;
            ordered.reserve(selectedLaneIds_.size());
            for (const auto& candidate : scenario_->lanes) {
                if (std::find(
                        selectedLaneIds_.begin(),
                        selectedLaneIds_.end(),
                        candidate.id)
                    != selectedLaneIds_.end()) {
                    ordered.push_back(candidate.id);
                }
            }
            if (ordered.size() == selectedLaneIds_.size()) {
                selectedLaneIds_ = std::move(ordered);
            }
        } else {
            selectedLaneId_ = lane->id;
            selectedLaneIds_ = {lane->id};
            laneHeaderSelectionAnchorId_ =
                lane->kind == LaneKind::Group ? std::string{} : lane->id;
        }
        laneHeaderSelectionActive_ = true;
        laneHeaderPressed_ = !modifiesSignalSelection;
        laneHeaderDragging_ = false;
        laneHeaderPressPreservesMultiSelection_ =
            preservesMultiSelectionForPotentialDrag;
        laneHeaderPressPosition_ = position;
        laneDragOriginalVerticalScroll_ = verticalScrollBar()->value();
        laneDragId_ = laneHeaderPressed_ ? lane->id : std::string{};
        laneDragIds_ = laneHeaderPressed_
            ? preservesMultiSelectionForPotentialDrag
                ? selectedLaneIds_
                : std::vector<std::string>{lane->id}
            : std::vector<std::string>{};
        laneDropInsertionSlot_.reset();
        laneDropDestinationIndex_.reset();
        laneDropIndicatorY_.reset();
        laneDropGroupId_.clear();
        emit selectionChanged(
            QString::fromStdString(selectedLaneId_),
            cursorTick_);
        const auto* activeHeaderLane = findLane(*scenario_, selectedLaneId_);
        const auto* messageLane = activeHeaderLane ? activeHeaderLane : lane;
        auto selectionMessage = selectedLaneIds_.size() > 1
            ? tr("Selected %1 signals · active %2 · drag to move together · Alt+Up/Down moves one row · right-click for actions")
                  .arg(static_cast<qulonglong>(selectedLaneIds_.size()))
                  .arg(
                      activeHeaderLane
                          ? QString::fromStdString(activeHeaderLane->name)
                          : QString::fromStdString(selectedLaneId_))
            : messageLane->kind == LaneKind::Group
            ? tr("Selected group %1").arg(QString::fromStdString(messageLane->name))
            : tr("Selected signal %1").arg(QString::fromStdString(messageLane->name));
        if (messageLane->kind == LaneKind::Group) {
            selectionMessage.append(
                collapsedGroupIds_.contains(messageLane->id)
                    ? tr(" · Right expands")
                    : tr(" · Left collapses"));
        } else if (tool_ == Tool::WaveEdit && selectedLaneIds_.size() == 1) {
            selectionMessage.append(
                tr(" · value %1 at %2")
                    .arg(laneValueAt(*messageLane, cursorTick_))
                    .arg(QString::fromStdString(
                        formatTick(cursorTick_, project_->timeBase))));
        }
        if (messageLane->kind != LaneKind::Group) {
            const auto pasteHint = laneHeaderPasteHint();
            if (!pasteHint.isEmpty()) {
                selectionMessage.append(tr(" · %1").arg(pasteHint));
            }
        }
        if (lockedMarkerDeselected) {
            selectionMessage.append(tr(" · locked cursor/range deselected"));
        }
        if (rangeClearedForRetarget) {
            selectionMessage.append(tr(" · range cleared"));
        }
        emit statusMessage(selectionMessage);
        viewport()->setCursor(
            modifiesSignalSelection ? Qt::PointingHandCursor : Qt::OpenHandCursor);
        viewport()->update();
        return;
    }

    laneHeaderSelectionActive_ = false;
    laneHeaderPressed_ = false;
    laneHeaderDragging_ = false;
    laneHeaderPressPreservesMultiSelection_ = false;
    laneDragId_.clear();
    laneDragIds_.clear();
    laneDropInsertionSlot_.reset();
    laneDropDestinationIndex_.reset();
    laneDropIndicatorY_.reset();
    laneDropGroupId_.clear();

    if (tool_ == Tool::WaveEdit) {
        setFocus(Qt::MouseFocusReason);
        if (position.x() < headerWidth_ || !lane || lane->kind == LaneKind::Group) {
            clearWaveEditState();
            if (!lane) {
                selectedLaneId_.clear();
                selectedLaneIds_.clear();
                emit selectionChanged(QString{}, cursorTick_);
            }
            viewport()->update();
            return;
        }
        selectedLaneId_ = lane->id;
        selectedLaneIds_ = {lane->id};
        const auto rawTick = scenario_->duration > 0
            ? std::clamp<Tick>(tickAtX(position.x()), 0, scenario_->duration - 1)
            : Tick{0};
        cursorTick_ = editTick(rawTick, *lane);
        emit selectionChanged(QString::fromStdString(lane->id), cursorTick_);
        stopWaveEditDragAutoScroll();
        waveEditDragOriginalHorizontalScroll_ = horizontalScrollBar()->value();
        waveEditDragOriginalVerticalScroll_ = verticalScrollBar()->value();
        waveEditDragAutoScrolled_ = false;
        waveEditPressPosition_ = position;
        waveEditCopyDrag_ = false;

        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            explicitRangeSelection_ = false;
            hideRangeEditPalette();
            if (rangeValueEdit_) rangeValueEdit_->setModified(false);
            selectedSegmentLaneId_.clear();
            selectedSegmentId_.clear();
            waveEditOriginalRange_.reset();
            waveEditPreviewRange_.reset();
            waveEditInteraction_ = WaveEditInteraction::SelectRange;
            drawLaneId_ = lane->id;
            drawStart_ = cursorTick_;
            drawCurrent_ = cursorTick_;
            selectionStartY_ = position.y();
            interactionCurrent_ = position;
            selectionRange_ = std::pair{cursorTick_, cursorTick_};
            drawing_ = true;
            viewport()->setCursor(Qt::CrossCursor);
            viewport()->update();
            return;
        }

        if (lane->kind == LaneKind::Bit) {
            const auto* hitEvent = eventAtPosition(position);
            if (hitEvent
                && hitEvent->laneId == lane->id
                && hitEvent->waveformLinked) {
                selectedSegmentLaneId_.clear();
                selectedSegmentId_.clear();
                waveEditOriginalRange_.reset();
                waveEditPreviewRange_.reset();
                waveEditHoverLaneId_.clear();
                waveEditHoverRange_.reset();
                selectionRange_.reset();
                waveEditInteraction_ = WaveEditInteraction::MoveTransition;
                activeEventId_ = hitEvent->id;
                drawLaneId_ = lane->id;
                drawStart_ = hitEvent->tick;
                drawCurrent_ = hitEvent->tick;
                waveEditPressPosition_ = position;
                drawing_ = true;
                emit eventSelected(QString::fromStdString(hitEvent->id));
                viewport()->setCursor(Qt::SizeHorCursor);
                viewport()->update();
                return;
            }
        }

        if (lane->kind == LaneKind::Bit) {
            selectedSegmentLaneId_.clear();
            selectedSegmentId_.clear();
            waveEditOriginalRange_.reset();
            waveEditInteraction_ = WaveEditInteraction::ToggleBitRange;
            drawLaneId_ = lane->id;
            drawStart_ = cursorTick_;
            drawCurrent_ = cursorTick_;
            waveEditPressPosition_ = position;
            waveEditPreviewRange_ = editableBeatRangeAt(rawTick, *lane);
            waveEditHoverLaneId_ = lane->id;
            waveEditHoverRange_ = waveEditPreviewRange_;
            selectionRange_ = waveEditPreviewRange_;
            drawing_ = true;
            viewport()->setCursor(Qt::PointingHandCursor);
            viewport()->update();
            return;
        }

        waveEditHoverLaneId_.clear();
        waveEditHoverRange_.reset();
        const auto hit = segmentHitAtPosition(*lane, position);
        if (hit.segment && hit.boundary != SegmentBoundary::None) {
            selectedSegmentLaneId_ = lane->id;
            selectedSegmentId_ = hit.segment->id;
            waveEditOriginalRange_ = std::pair{hit.segment->start, hit.segment->end};
            waveEditPreviewRange_ = waveEditOriginalRange_;
            waveEditInteraction_ = hit.boundary == SegmentBoundary::Start
                ? WaveEditInteraction::ResizeStart
                : WaveEditInteraction::ResizeEnd;
            drawLaneId_ = lane->id;
            drawStart_ = hit.boundary == SegmentBoundary::Start
                ? hit.segment->start
                : hit.segment->end;
            drawCurrent_ = drawStart_;
            waveEditPressPosition_ = position;
            drawing_ = true;
            selectionRange_ = waveEditPreviewRange_;
            viewport()->setCursor(Qt::SplitHCursor);
            viewport()->update();
            return;
        }

        if (hit.segment) {
            selectedSegmentLaneId_ = lane->id;
            selectedSegmentId_ = hit.segment->id;
            waveEditOriginalRange_ = std::pair{hit.segment->start, hit.segment->end};
            waveEditPreviewRange_ = waveEditOriginalRange_;
            waveEditInteraction_ = WaveEditInteraction::MoveSegment;
            waveEditCopyDrag_ = event->modifiers().testFlag(Qt::ControlModifier)
                && lane->kind != LaneKind::Clock;
            waveEditGrabOffset_ = rawTick - hit.segment->start;
            drawLaneId_ = lane->id;
            drawStart_ = rawTick;
            drawCurrent_ = rawTick;
            waveEditPressPosition_ = position;
            selectionRange_ = waveEditOriginalRange_;
            drawing_ = true;
            viewport()->setCursor(waveEditCopyDrag_ ? Qt::DragCopyCursor : Qt::SizeAllCursor);
        } else {
            drawing_ = false;
            waveEditInteraction_ = WaveEditInteraction::None;
            waveEditOriginalRange_.reset();
            waveEditPreviewRange_.reset();
            selectedSegmentLaneId_.clear();
            selectedSegmentId_.clear();
            if (lane->kind == LaneKind::Bus || lane->kind == LaneKind::Enum) {
                selectionRange_ = editableBeatRangeAt(rawTick, *lane);
            } else {
                selectionRange_.reset();
            }
        }
        viewport()->update();
        return;
    }

    if (tool_ == Tool::Marker) {
        setFocus(Qt::MouseFocusReason);
        if (position.x() < headerWidth_) return;
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
                cursorTick_ = *movableCursorTick_;
            } else {
                movableCursorTick_ = drawStart_;
                temporaryCursorTick_.reset();
            }
            emit statusMessage(cursorMeasurementText());
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

    if (tool_ == Tool::Relation) {
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
    if (rangeSequenceTokenPress_) {
        if (!event->buttons().testFlag(
                Qt::LeftButton)) {
            rangeSequenceTokenPress_ = false;
            rangeSequenceTokenDragRejected_ = false;
            rangeSequenceTokenPressPosition_ = {};
            viewport()->setCursor(
                defaultCursorShape());
            event->accept();
            return;
        }
        const auto dragDistance =
            (position
             - rangeSequenceTokenPressPosition_)
                .manhattanLength();
        if (dragDistance
            < QApplication::startDragDistance()) {
            viewport()->setCursor(
                Qt::PointingHandCursor);
            event->accept();
            return;
        }
        const auto pressPosition =
            rangeSequenceTokenPressPosition_;
        rangeSequenceTokenPress_ = false;
        rangeSequenceTokenPressPosition_ = {};
        if (rangeValueEdit_
            && rangeValueEdit_->isModified()) {
            rangeSequenceTokenDragRejected_ = true;
            viewport()->setCursor(
                Qt::ForbiddenCursor);
            emit statusMessage(
                tr("Range drag not started · apply or discard the sequence draft first"));
            event->accept();
            return;
        }
        if (!beginExplicitRangeMove(
                pressPosition,
                event->modifiers())) {
            event->accept();
            return;
        }
    }
    if (headerResizing_) {
        setSignalHeaderWidth(
            headerResizeOriginalWidth_ + position.x() - headerResizePressX_);
        viewport()->setCursor(Qt::SplitHCursor);
        event->accept();
        return;
    }
    if (rulerScrubbing_) {
        if (!event->buttons().testFlag(Qt::LeftButton)) {
            cancelRulerScrub();
            event->accept();
            return;
        }
        updateRulerScrub(position.x(), false);
        viewport()->setCursor(Qt::SizeHorCursor);
        event->accept();
        return;
    }
    if (panning_) {
        const auto delta = position - panPressPosition_;
        horizontalScrollBar()->setValue(panStartHorizontal_ - delta.x());
        verticalScrollBar()->setValue(panStartVertical_ - delta.y());
        viewport()->setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    if (laneHeaderPressed_) {
        if (!event->buttons().testFlag(Qt::LeftButton)) {
            stopLaneDragAutoScroll();
            verticalScrollBar()->setValue(laneDragOriginalVerticalScroll_);
            laneHeaderPressed_ = false;
            laneHeaderDragging_ = false;
            laneHeaderPressPreservesMultiSelection_ = false;
            laneDragId_.clear();
            laneDragIds_.clear();
            laneDropInsertionSlot_.reset();
            laneDropDestinationIndex_.reset();
            laneDropIndicatorY_.reset();
            laneDropGroupId_.clear();
            viewport()->setCursor(defaultCursorShape());
            viewport()->update();
            return;
        }
        if (!laneHeaderDragging_
            && (position - laneHeaderPressPosition_).manhattanLength()
                >= QApplication::startDragDistance()) {
            laneHeaderDragging_ = true;
        }
        if (laneHeaderDragging_) {
            updateLaneDropTarget(position.y());
            updateLaneDragAutoScroll(position.y());
            viewport()->setCursor(
                laneDropGroupId_.empty()
                    ? Qt::ClosedHandCursor
                    : Qt::DragMoveCursor);
            viewport()->update();
        }
        return;
    }
    if (tool_ == Tool::WaveEdit
        && drawing_
        && !event->buttons().testFlag(Qt::LeftButton)) {
        if (waveEditInteraction_ == WaveEditInteraction::MoveRange
            || waveEditInteraction_ == WaveEditInteraction::ResizeRangeStart
            || waveEditInteraction_ == WaveEditInteraction::ResizeRangeEnd) {
            cancelExplicitRangeDrag(
                waveEditDragAutoScrolled_
                    ? tr("Range drag cancelled · selection and view restored")
                    : tr("Range drag cancelled · selection kept"));
            event->accept();
            return;
        }
        const auto restoredViewport = waveEditDragAutoScrolled_;
        stopWaveEditDragAutoScroll();
        if (restoredViewport) {
            horizontalScrollBar()->setValue(
                waveEditDragOriginalHorizontalScroll_);
            verticalScrollBar()->setValue(
                waveEditDragOriginalVerticalScroll_);
        }
        clearWaveEditState();
        snapGuideTick_.reset();
        viewport()->setCursor(defaultCursorShape());
        viewport()->update();
        emit statusMessage(
            restoredViewport
                ? tr("Waveform drag cancelled · view restored")
                : tr("Waveform drag cancelled"));
        event->accept();
        return;
    }
    if (event->buttons() == Qt::NoButton && signalHeaderDividerAt(position)) {
        const auto hadHover = waveEditHoverRange_.has_value()
            || !waveEditHoverLaneId_.empty()
            || snapGuideTick_.has_value();
        waveEditHoverLaneId_.clear();
        waveEditHoverRange_.reset();
        snapGuideTick_.reset();
        viewport()->setCursor(Qt::SplitHCursor);
        if (hadHover) viewport()->update();
        event->accept();
        return;
    }
    if (event->buttons() == Qt::NoButton
        && position.x() < headerWidth_) {
        const auto* headerLane = laneAtY(position.y());
        if (headerLane
            && headerLane->kind == LaneKind::Group
            && groupDisclosureRect(*headerLane).contains(position)) {
            const auto hadHover = waveEditHoverRange_.has_value()
                || !waveEditHoverLaneId_.empty()
                || snapGuideTick_.has_value();
            waveEditHoverLaneId_.clear();
            waveEditHoverRange_.reset();
            snapGuideTick_.reset();
            viewport()->setCursor(Qt::PointingHandCursor);
            if (hadHover) viewport()->update();
            event->accept();
            return;
        }
    }
    bypassSnap_ = event->modifiers().testFlag(Qt::AltModifier);
    interactionCurrent_ = position;
    const auto* lane = laneAtY(position.y());
    const auto rawTick = tickAtX(position.x());
    const auto boundedRaw = std::clamp<Tick>(rawTick, 0, scenario_->duration);
    const auto pointerEditTick =
        tool_ == Tool::WaveEdit && lane && lane->kind != LaneKind::Group
        ? editTick(rawTick, *lane)
        : snappedTick(rawTick, lane);
    const auto* rangeSequenceHoverSpan =
        tool_ == Tool::WaveEdit
            && event->buttons() == Qt::NoButton
            && position.x() >= headerWidth_
            && lane
        ? rangeSequenceTextSpanAt(
              lane->id,
              rawTick)
        : nullptr;
    if (tool_ != Tool::WaveEdit || drawing_) {
        cursorTick_ = pointerEditTick;
        snapGuideTick_ = cursorTick_ == boundedRaw
            ? std::optional<Tick>{}
            : std::optional<Tick>{cursorTick_};
    } else {
        snapGuideTick_.reset();
    }

    if (tool_ == Tool::WaveEdit) {
        if (drawing_) {
            updateWaveEditDragAutoScroll(position, event->modifiers());
            const auto* editLane = findLane(*scenario_, drawLaneId_);
            if (waveEditInteraction_ == WaveEditInteraction::MoveRange) {
                waveEditCopyDrag_ =
                    event->modifiers().testFlag(Qt::ControlModifier);
            } else if (waveEditInteraction_
                       == WaveEditInteraction::MoveSegment) {
                waveEditCopyDrag_ =
                    event->modifiers().testFlag(Qt::ControlModifier)
                    && editLane
                    && editLane->kind != LaneKind::Clock;
            }
            const auto meaningfulDrag =
                (position - waveEditPressPosition_).manhattanLength()
                >= QApplication::startDragDistance();
            if (meaningfulDrag
                && busEditPaletteVisible_
                && busPresetLaneId_ == drawLaneId_
                && (waveEditInteraction_ == WaveEditInteraction::MoveSegment
                    || waveEditInteraction_ == WaveEditInteraction::ResizeStart
                    || waveEditInteraction_ == WaveEditInteraction::ResizeEnd)) {
                hideBusPresetPalette();
            }
            drawCurrent_ = std::clamp<Tick>(rawTick, 0, scenario_->duration);
            if (waveEditInteraction_ == WaveEditInteraction::MoveTransition) {
                if (const auto* event = findEvent(*scenario_, activeEventId_)) {
                    drawCurrent_ = constrainedTransitionTick(
                        *event,
                        editLane ? editTick(rawTick, *editLane) : snappedTick(rawTick, nullptr));
                    cursorTick_ = drawCurrent_;
                }
            } else if (waveEditInteraction_ == WaveEditInteraction::SelectRange) {
                drawCurrent_ = editLane ? editTick(rawTick, *editLane) : snappedTick(rawTick, nullptr);
                selectionRange_ = std::pair{
                    std::min(drawStart_, drawCurrent_),
                    std::max(drawStart_, drawCurrent_),
                };
                interactionCurrent_ = position;
            } else if (waveEditOriginalRange_
                       && (waveEditInteraction_ == WaveEditInteraction::ResizeRangeStart
                           || waveEditInteraction_ == WaveEditInteraction::ResizeRangeEnd)) {
                const auto [originalStart, originalEnd] = *waveEditOriginalRange_;
                const auto snapped = editLane
                    ? editTick(rawTick, *editLane)
                    : snappedTick(rawTick, nullptr);
                const auto unit = editLane ? minimumWaveEditUnit(*editLane) : Tick{1};
                if (waveEditInteraction_ == WaveEditInteraction::ResizeRangeStart) {
                    const auto upper = originalEnd - std::min(unit, originalEnd);
                    const auto start = std::clamp<Tick>(snapped, 0, upper);
                    waveEditPreviewRange_ = std::pair{start, originalEnd};
                    cursorTick_ = start;
                } else {
                    const auto lower = std::min(
                        scenario_->duration,
                        originalStart + std::min(unit, scenario_->duration - originalStart));
                    const auto end = std::clamp<Tick>(
                        snapped,
                        lower,
                        scenario_->duration);
                    waveEditPreviewRange_ = std::pair{originalStart, end};
                    cursorTick_ = end;
                }
                selectionRange_ = waveEditPreviewRange_;
            } else if (editLane
                       && waveEditInteraction_ == WaveEditInteraction::MoveRange
                       && waveEditOriginalRange_) {
                const auto [originalStart, originalEnd] = *waveEditOriginalRange_;
                const auto width = originalEnd - originalStart;
                if (!meaningfulDrag) {
                    waveEditPreviewRange_ = waveEditOriginalRange_;
                    waveEditRangeTargetLaneIds_ = selectedLaneIds_;
                    waveEditRangeTargetValid_ = true;
                    waveEditRangeTargetError_.clear();
                } else {
                    updateRangeTransferTargetLanes(lane);
                    const auto maximumStart = std::min(
                        scenario_->duration,
                        std::numeric_limits<Tick>::max() - width);
                    const auto* snapLane =
                        lane && lane->kind != LaneKind::Group
                        ? lane
                        : editLane;
                    const auto requestedStart = editTick(
                        rawTick - waveEditGrabOffset_,
                        *snapLane);
                    const auto start = std::clamp<Tick>(
                        requestedStart,
                        0,
                        maximumStart);
                    waveEditPreviewRange_ =
                        std::pair{start, start + width};
                }
                cursorTick_ = waveEditPreviewRange_->first;
                const auto copyOverlap = waveEditCopyDrag_
                    && rangeTransferTargetOverlapsSource();
                viewport()->setCursor(
                    !waveEditRangeTargetValid_ || copyOverlap
                        ? Qt::ForbiddenCursor
                        : waveEditCopyDrag_
                            ? Qt::DragCopyCursor
                            : Qt::SizeAllCursor);
            } else if (editLane
                       && waveEditInteraction_ == WaveEditInteraction::MoveSegment
                       && waveEditOriginalRange_) {
                const auto* selected = segmentById(
                    selectedSegmentLaneId_,
                    selectedSegmentId_);
                if (selected) {
                    const auto index = static_cast<std::size_t>(
                        selected - editLane->segments.data());
                    const auto [originalStart, originalEnd] = *waveEditOriginalRange_;
                    const auto width = originalEnd - originalStart;
                    const auto unit = minimumWaveEditUnit(*editLane);
                    auto lower = Tick{0};
                    auto upper = scenario_->duration - width;
                    if (!waveEditCopyDrag_) {
                        if (index > 0) {
                            const auto& previous = editLane->segments.at(index - 1);
                            lower = previous.end == originalStart
                                ? previous.start + unit
                                : previous.end;
                        }
                        if (index + 1 < editLane->segments.size()) {
                            const auto& next = editLane->segments.at(index + 1);
                            upper = next.start == originalEnd
                                ? next.end - width - unit
                                : next.start - width;
                        }
                    }
                    if ((position - waveEditPressPosition_).manhattanLength()
                        < QApplication::startDragDistance()
                        || upper < lower) {
                        waveEditPreviewRange_ = waveEditOriginalRange_;
                    } else {
                        const auto requestedStart = editTick(
                            rawTick - waveEditGrabOffset_,
                            *editLane);
                        const auto start = std::clamp(requestedStart, lower, upper);
                        waveEditPreviewRange_ = std::pair{start, start + width};
                    }
                    selectionRange_ = waveEditPreviewRange_;
                    viewport()->setCursor(
                        waveEditCopyDrag_ ? Qt::DragCopyCursor : Qt::SizeAllCursor);
                }
            } else if (editLane
                && waveEditInteraction_ == WaveEditInteraction::ToggleBitRange) {
                const auto beats = editableBeatRangesBetween(drawStart_, drawCurrent_, *editLane);
                if (!beats.empty()) {
                    waveEditPreviewRange_ = std::pair{
                        beats.front().first,
                        beats.back().second,
                    };
                    selectionRange_ = waveEditPreviewRange_;
                } else {
                    waveEditPreviewRange_.reset();
                }
            } else if (editLane
                       && waveEditOriginalRange_
                       && (waveEditInteraction_ == WaveEditInteraction::ResizeStart
                           || waveEditInteraction_ == WaveEditInteraction::ResizeEnd)) {
                const auto* selected = segmentById(
                    selectedSegmentLaneId_,
                    selectedSegmentId_);
                if (selected) {
                    const auto index = static_cast<std::size_t>(
                        selected - editLane->segments.data());
                    const auto [originalStart, originalEnd] = *waveEditOriginalRange_;
                    const auto snapped = editTick(rawTick, *editLane);
                    const auto unit = minimumWaveEditUnit(*editLane);
                    if (waveEditInteraction_ == WaveEditInteraction::ResizeStart) {
                        auto lower = Tick{0};
                        if (index > 0) {
                            const auto& previous = editLane->segments.at(index - 1);
                            lower = previous.end == originalStart
                                ? previous.start + unit
                                : previous.end;
                        }
                        const auto upper = originalEnd - std::min(unit, originalEnd);
                        const auto start = upper >= lower
                            ? std::clamp(snapped, lower, upper)
                            : originalStart;
                        waveEditPreviewRange_ = std::pair{start, originalEnd};
                    } else {
                        auto upper = scenario_->duration;
                        if (index + 1 < editLane->segments.size()) {
                            const auto& next = editLane->segments.at(index + 1);
                            upper = next.start == originalEnd
                                ? next.end - unit
                                : next.start;
                        }
                        const auto lower = originalStart
                            + std::min(unit, scenario_->duration - originalStart);
                        const auto end = upper >= lower
                            ? std::clamp(snapped, lower, upper)
                            : originalEnd;
                        waveEditPreviewRange_ = std::pair{originalStart, end};
                    }
                    selectionRange_ = waveEditPreviewRange_;
                }
            }
            viewport()->update();
        } else if (explicitRangeSelection_
                   && explicitRangeBoundaryAt(position) != SegmentBoundary::None) {
            const auto hadHover = waveEditHoverRange_.has_value()
                || !waveEditHoverLaneId_.empty();
            waveEditHoverLaneId_.clear();
            waveEditHoverRange_.reset();
            QToolTip::hideText();
            viewport()->setCursor(Qt::SplitHCursor);
            if (hadHover) viewport()->update();
        } else if (rangeSequenceHoverSpan) {
            const auto hadHover =
                waveEditHoverRange_.has_value()
                || !waveEditHoverLaneId_.empty();
            waveEditHoverLaneId_.clear();
            waveEditHoverRange_.reset();
            viewport()->setCursor(
                Qt::PointingHandCursor);
            QToolTip::showText(
                event->globalPosition().toPoint(),
                tr("Click to select token %1 in the range editor\nEditing it affects %2 mapped target(s)")
                    .arg(
                        rangeSequenceHoverSpan
                            ->token)
                    .arg(
                        static_cast<qulonglong>(
                            rangeSequenceHoverSpan
                                ->targets.size())),
                viewport());
            if (hadHover) viewport()->update();
        } else if (explicitRangeSelection_
                   && explicitRangeContains(position)) {
            const auto hadHover = waveEditHoverRange_.has_value()
                || !waveEditHoverLaneId_.empty();
            waveEditHoverLaneId_.clear();
            waveEditHoverRange_.reset();
            QToolTip::hideText();
            viewport()->setCursor(
                event->modifiers().testFlag(Qt::ControlModifier)
                    ? Qt::DragCopyCursor
                    : Qt::SizeAllCursor);
            if (hadHover) viewport()->update();
        } else if (position.x() >= headerWidth_
                   && lane
                   && lane->kind == LaneKind::Bit) {
            const auto* hitEvent = eventAtPosition(position);
            if (hitEvent
                && hitEvent->laneId == lane->id
                && hitEvent->waveformLinked) {
                const auto hadHover = waveEditHoverRange_.has_value()
                    || !waveEditHoverLaneId_.empty();
                waveEditHoverLaneId_.clear();
                waveEditHoverRange_.reset();
                viewport()->setCursor(Qt::SizeHorCursor);
                if (hadHover) viewport()->update();
            } else {
                const auto boundedTick = scenario_->duration > 0
                    ? std::clamp<Tick>(rawTick, 0, scenario_->duration - 1)
                    : Tick{0};
                const auto hoverRange = editableBeatRangeAt(boundedTick, *lane);
                const auto changed = waveEditHoverLaneId_ != lane->id
                    || !waveEditHoverRange_
                    || *waveEditHoverRange_ != hoverRange;
                waveEditHoverLaneId_ = lane->id;
                waveEditHoverRange_ = hoverRange;
                viewport()->setCursor(Qt::PointingHandCursor);
                if (changed) viewport()->update();
            }
        } else if (lane && lane->kind != LaneKind::Group) {
            const auto hit = segmentHitAtPosition(*lane, position);
            std::optional<std::pair<Tick, Tick>> hoverRange;
            if (hit.segment) {
                hoverRange = std::pair{
                    hit.segment->start,
                    hit.segment->end,
                };
            } else if (scenario_->duration > 0) {
                const auto hoverTick = std::clamp<Tick>(
                    rawTick,
                    0,
                    scenario_->duration - 1);
                hoverRange = editableBeatRangeAt(hoverTick, *lane);
            }
            const auto changed = waveEditHoverLaneId_ != lane->id
                || waveEditHoverRange_ != hoverRange;
            waveEditHoverLaneId_ = lane->id;
            waveEditHoverRange_ = hoverRange;
            viewport()->setCursor(
                hit.boundary == SegmentBoundary::None
                    ? (hit.segment ? Qt::SizeAllCursor : Qt::PointingHandCursor)
                    : Qt::SplitHCursor);
            if (changed) viewport()->update();
        } else {
            const auto hadHover = waveEditHoverRange_.has_value()
                || !waveEditHoverLaneId_.empty();
            waveEditHoverLaneId_.clear();
            waveEditHoverRange_.reset();
            viewport()->setCursor(Qt::PointingHandCursor);
            if (hadHover) viewport()->update();
        }
        auto message = tr("Wave Edit %1").arg(QString::fromStdString(
            formatTick(cursorTick_, project_->timeBase)));
        if (!drawing_
            && position.x() >= headerWidth_
            && lane
            && lane->kind != LaneKind::Group) {
            message += tr("  |  pointer %1 · value %2")
                           .arg(
                               QString::fromStdString(lane->name),
                               laneValueAt(*lane, boundedRaw));
        }
        if (drawing_
            && waveEditInteraction_ == WaveEditInteraction::MoveTransition) {
            message += tr("  |  Move edge to %1").arg(QString::fromStdString(
                formatTick(drawCurrent_, project_->timeBase)));
        } else if (drawing_
                   && (waveEditInteraction_ == WaveEditInteraction::MoveSegment
                       || waveEditInteraction_ == WaveEditInteraction::ResizeStart
                       || waveEditInteraction_ == WaveEditInteraction::ResizeEnd)) {
            const auto projection = buildSegmentEditProjection();
            message += tr("  |  %1")
                           .arg(
                               projection
                                   ? segmentEditPreviewStatus(*projection)
                                   : waveEditCopyDrag_
                                       ? tr("Copy Segment preview · source remains")
                                       : tr("Segment preview"));
        } else if (drawing_
                   && waveEditInteraction_ == WaveEditInteraction::MoveRange) {
            const auto copyOverlap = waveEditCopyDrag_
                && rangeTransferTargetOverlapsSource();
            if (!waveEditRangeTargetValid_) {
                message += tr("  |  Cannot drop · %1")
                               .arg(waveEditRangeTargetError_);
            } else if (copyOverlap) {
                message +=
                    tr("  |  Copy target overlaps source signal(s) · move time or signals");
            } else {
                const auto projection = buildRangeTransferProjection();
                message += tr("  |  %1")
                               .arg(
                                   projection
                                       ? rangeTransferPreviewStatus(
                                             *projection)
                                       : waveEditCopyDrag_
                                           ? tr("Copy range preview · source remains")
                                           : tr("Move range preview · source clears on release"));
            }
        }
        const auto displayedRange = waveEditPreviewRange_
            ? waveEditPreviewRange_
            : drawing_
                    && waveEditInteraction_ == WaveEditInteraction::SelectRange
                    && selectionRange_
                ? selectionRange_
            : waveEditHoverRange_;
        if (displayedRange) {
            message += tr("  |  %1 to %2 · width %3")
                           .arg(
                               QString::fromStdString(formatTick(
                                   displayedRange->first,
                                   project_->timeBase)),
                               QString::fromStdString(formatTick(
                                   displayedRange->second,
                                   project_->timeBase)),
                               QString::fromStdString(formatTick(
                                   displayedRange->second - displayedRange->first,
                                   project_->timeBase)));
        }
        if (waveEditDragAutoScrollDirection_ != 0) {
            message += waveEditDragAutoScrollDirection_ < 0
                ? tr("  |  Auto-scroll left")
                : tr("  |  Auto-scroll right");
        }
        if (waveEditDragVerticalAutoScrollDirection_ != 0) {
            message += waveEditDragVerticalAutoScrollDirection_ < 0
                ? tr("  |  Auto-scroll up")
                : tr("  |  Auto-scroll down");
        }
        if (drawing_) {
            emit statusMessage(message);
        } else {
            emit pointerStatusMessage(message);
        }
        return;
    }

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
        emit statusMessage(
            movableCursorTick_
                ? cursorMeasurementText()
                : tr("Pointer %1 · click to place cursor")
                      .arg(QString::fromStdString(
                          formatTick(cursorTick_, project_->timeBase))));
    } else {
        emit statusMessage(
            tr("%1  |  %2")
                .arg(QString::fromStdString(formatTick(cursorTick_, project_->timeBase)))
                .arg(lane ? QString::fromStdString(lane->name) : tr("No lane")));
    }
}

void WaveCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (rangeSequenceTokenDragRejected_
        && event->button() == Qt::LeftButton) {
        rangeSequenceTokenDragRejected_ = false;
        viewport()->setCursor(
            rangeSequenceTokenAt(
                event->position().toPoint())
                ? Qt::PointingHandCursor
                : defaultCursorShape());
        if (rangeValueEdit_
            && rangeSequenceCaretTarget_) {
            rangeValueEdit_->setFocus(
                Qt::MouseFocusReason);
        }
        event->accept();
        return;
    }
    if (rangeSequenceTokenPress_
        && event->button() == Qt::LeftButton) {
        const auto pressPosition =
            rangeSequenceTokenPressPosition_;
        rangeSequenceTokenPress_ = false;
        rangeSequenceTokenPressPosition_ = {};
        const auto selected =
            selectRangeSequenceTokenAt(
                pressPosition);
        if (selected && rangeValueEdit_) {
            rangeValueEdit_->setFocus(
                Qt::MouseFocusReason);
            updateRangeSequenceCaretTarget();
            QTimer::singleShot(
                0,
                rangeValueEdit_,
                [this] {
                    if (rangeValueEdit_
                        && rangeSequenceCaretTarget_) {
                        rangeValueEdit_->setFocus(
                            Qt::MouseFocusReason);
                    }
                });
        }
        viewport()->setCursor(
            selected
                ? Qt::PointingHandCursor
                : defaultCursorShape());
        event->accept();
        return;
    }
    if (rulerScrubbing_ && event->button() == Qt::LeftButton) {
        updateRulerScrub(
            event->position().toPoint().x(),
            true,
            rulerScrubClearedRange_);
        rulerScrubbing_ = false;
        rulerScrubClearedRange_ = false;
        rulerScrubOriginalSelection_.reset();
        snapGuideTick_.reset();
        viewport()->setCursor(defaultCursorShape());
        viewport()->update();
        event->accept();
        return;
    }
    if (headerResizing_ && event->button() == Qt::LeftButton) {
        const auto changed = headerWidth_ != headerResizeOriginalWidth_;
        headerResizing_ = false;
        viewport()->setCursor(
            signalHeaderDividerAt(event->position().toPoint())
                ? Qt::SplitHCursor
                : defaultCursorShape());
        if (changed) emit signalHeaderWidthCommitted(headerWidth_);
        emit statusMessage(
            changed
                ? tr("Signal names width %1 px").arg(headerWidth_)
                : tr("Signal names width unchanged"));
        event->accept();
        return;
    }
    if (panning_
        && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
        panning_ = false;
        viewport()->setCursor(
            spaceHeld_ ? Qt::OpenHandCursor : defaultCursorShape());
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && laneHeaderPressed_) {
        stopLaneDragAutoScroll();
        if (laneHeaderDragging_) {
            commitLaneReorder();
        } else if (laneHeaderPressPreservesMultiSelection_
                   && !laneDragId_.empty()) {
            selectedLaneId_ = laneDragId_;
            selectedLaneIds_ = {laneDragId_};
            laneHeaderSelectionAnchorId_ = laneDragId_;
            const auto* lane = scenario_
                ? findLane(*scenario_, laneDragId_)
                : nullptr;
            emit selectionChanged(
                QString::fromStdString(selectedLaneId_),
                cursorTick_);
            emit statusMessage(
                lane
                    ? tr("Selected signal %1 · multi-signal selection cleared")
                          .arg(QString::fromStdString(lane->name))
                    : tr("Multi-signal selection cleared"));
        }
        laneHeaderPressed_ = false;
        laneHeaderDragging_ = false;
        laneHeaderPressPreservesMultiSelection_ = false;
        laneDragId_.clear();
        laneDragIds_.clear();
        laneDropInsertionSlot_.reset();
        laneDropDestinationIndex_.reset();
        laneDropIndicatorY_.reset();
        laneDropGroupId_.clear();
        viewport()->setCursor(defaultCursorShape());
        viewport()->update();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && drawing_) {
        bypassSnap_ = event->modifiers().testFlag(Qt::AltModifier);
        auto releasePosition = event->position().toPoint();
        if (durationEditPointerRetargetTick_) {
            if ((releasePosition - durationEditPointerRetargetPressPosition_)
                    .manhattanLength()
                < QApplication::startDragDistance()) {
                releasePosition.setX(
                    xAtTick(*durationEditPointerRetargetTick_));
            }
            durationEditPointerRetargetTick_.reset();
        }
        switch (tool_) {
        case Tool::Draw:
            commitDraw(releasePosition);
            break;
        case Tool::WaveEdit:
            stopWaveEditDragAutoScroll();
            if (waveEditInteraction_ == WaveEditInteraction::MoveRange) {
                waveEditCopyDrag_ =
                    event->modifiers().testFlag(Qt::ControlModifier);
            } else if (waveEditInteraction_
                       == WaveEditInteraction::MoveSegment) {
                const auto* lane = findLane(*scenario_, drawLaneId_);
                waveEditCopyDrag_ =
                    event->modifiers().testFlag(Qt::ControlModifier)
                    && lane
                    && lane->kind != LaneKind::Clock;
            }
            commitWaveEdit(releasePosition);
            waveEditDragAutoScrolled_ = false;
            break;
        case Tool::Marker:
            commitMarker(event->position().toPoint());
            break;
        case Tool::Relation:
            commitRelation(event->position().toPoint());
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
    if (durationEdit_
        && durationEdit_->isVisible()
        && durationEdit_->isModified()) {
        static_cast<void>(commitPendingInlineEdits());
        event->accept();
        return;
    }
    if (!commitPendingInlineEdits()) {
        event->accept();
        return;
    }
    const auto position = event->position().toPoint();
    if (event->button() == Qt::LeftButton && signalHeaderDividerAt(position)) {
        headerResizing_ = false;
        const auto originalWidth = headerWidth_;
        setSignalHeaderWidth(fittedSignalHeaderWidth());
        viewport()->setCursor(
            signalHeaderDividerAt(position)
                ? Qt::SplitHCursor
                : defaultCursorShape());
        if (headerWidth_ != originalWidth) {
            emit signalHeaderWidthCommitted(headerWidth_);
            emit statusMessage(tr("Signal names fitted to %1 px").arg(headerWidth_));
        } else {
            emit statusMessage(tr("Signal names already fit at %1 px").arg(headerWidth_));
        }
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && position.x() < headerWidth_) {
        const auto* lane = laneAtY(position.y());
        if (lane) {
            if (lane->kind == LaneKind::Group
                && groupDisclosureRect(*lane).contains(position)) {
                event->accept();
                return;
            }
            selectedLaneId_ = lane->id;
            selectedLaneIds_ = {lane->id};
            laneHeaderSelectionAnchorId_ = lane->id;
            if (tool_ == Tool::Marker) {
                selectedMarkerId_.clear();
                cursorInteraction_ = CursorInteraction::None;
                lockedMarkerOriginalRange_.reset();
            }
            laneHeaderSelectionActive_ = true;
            laneHeaderPressed_ = false;
            laneHeaderDragging_ = false;
            laneDragId_.clear();
            laneDropDestinationIndex_.reset();
            laneDropIndicatorY_.reset();
            laneDropGroupId_.clear();
            emit selectionChanged(QString::fromStdString(lane->id), cursorTick_);
            emit renameLaneRequested(QString::fromStdString(lane->id));
            event->accept();
            viewport()->update();
        }
        return;
    }
    if (tool_ == Tool::WaveEdit) {
        auto* editLane = laneAtY(position.y());
        if (editLane
            && (editLane->kind == LaneKind::Bus
                || editLane->kind == LaneKind::Enum)
            && !segmentAtTick(*editLane, tickAtX(position.x()))) {
            promptBusValueAt(editLane->id, editTick(tickAtX(position.x()), *editLane));
        } else {
            editSegmentAt(position);
        }
        return;
    }

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
    sanitizeCollapsedGroups();
    rebuildSnapIndex();
    if (!scenario_) {
        updateScrollBars();
        return;
    }
    int top = 0;
    for (std::size_t index = 0; index < scenario_->lanes.size(); ++index) {
        const auto& lane = scenario_->lanes[index];
        if (!isLaneDisplayed(lane)) continue;
        const auto height = std::clamp(lane.height, 30, 240);
        laneLayout_.push_back({index, top, height});
        top += height;
    }
    updateScrollBars();
}

void WaveCanvas::sanitizeCollapsedGroups()
{
    if (!scenario_) {
        collapsedGroupIds_.clear();
        return;
    }
    std::erase_if(
        collapsedGroupIds_,
        [this](const std::string& groupId) {
            const auto* group = findLane(*scenario_, groupId);
            return !group
                || !group->visible
                || group->kind != LaneKind::Group
                || visibleGroupMemberCount(groupId) == 0;
        });
}

void WaveCanvas::rebuildSnapIndex()
{
    signalEdgeIndex_.clear();
    if (!scenario_) return;

    for (const auto& lane : scenario_->lanes) {
        if (!isLaneDisplayed(lane) || lane.kind == LaneKind::Group) continue;
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

std::size_t WaveCanvas::hiddenLaneCount() const noexcept
{
    if (!scenario_) return 0;
    return static_cast<std::size_t>(std::count_if(
        scenario_->lanes.begin(),
        scenario_->lanes.end(),
        [](const Lane& lane) { return !lane.visible; }));
}

bool WaveCanvas::signalHeaderDividerAt(const QPoint& position) const noexcept
{
    return std::abs(position.x() - headerWidth_) <= 5;
}

bool WaveCanvas::isLaneDisplayed(const Lane& lane) const noexcept
{
    if (!lane.visible) return false;
    const auto* group = visibleParentGroup(lane);
    return !group || !collapsedGroupIds_.contains(group->id);
}

const Lane* WaveCanvas::visibleParentGroup(const Lane& lane) const noexcept
{
    if (!scenario_
        || lane.kind == LaneKind::Group
        || lane.groupId.empty()) {
        return nullptr;
    }
    const auto* group = findLane(*scenario_, lane.groupId);
    return group
        && group->visible
        && group->kind == LaneKind::Group
        ? group
        : nullptr;
}

std::size_t WaveCanvas::visibleGroupMemberCount(
    const std::string& groupId) const noexcept
{
    if (!scenario_ || groupId.empty()) return 0;
    return static_cast<std::size_t>(std::count_if(
        scenario_->lanes.begin(),
        scenario_->lanes.end(),
        [&groupId](const Lane& lane) {
            return lane.visible
                && lane.kind != LaneKind::Group
                && lane.groupId == groupId;
        }));
}

QRect WaveCanvas::groupDisclosureRect(const Lane& group) const
{
    if (!scenario_
        || group.kind != LaneKind::Group
        || visibleGroupMemberCount(group.id) == 0) {
        return {};
    }
    const auto layout = std::find_if(
        laneLayout_.begin(),
        laneLayout_.end(),
        [this, &group](const LaneLayout& candidate) {
            return scenario_->lanes.at(candidate.laneIndex).id == group.id;
        });
    if (layout == laneLayout_.end()) return {};
    const auto y = RulerHeight + layout->top
        - verticalScrollBar()->value();
    constexpr int DisclosureSize = 20;
    return {
        7,
        y + (layout->height - DisclosureSize) / 2,
        DisclosureSize,
        DisclosureSize,
    };
}

int WaveCanvas::fittedSignalHeaderWidth() const
{
    QFont nameFont = viewport()->font();
    nameFont.setBold(true);
    const QFontMetrics nameMetrics(nameFont);
    const QFontMetrics detailMetrics(viewport()->font());
    auto fitted = DefaultHeaderWidth;
    if (scenario_) {
        for (const auto& lane : scenario_->lanes) {
            if (!isLaneDisplayed(lane)) continue;
            const auto memberIndent = visibleParentGroup(lane) ? 20 : 0;
            fitted = std::max(
                fitted,
                nameMetrics.horizontalAdvance(QString::fromStdString(lane.name))
                    + 32
                    + memberIndent);
            auto detail = laneKindLabel(lane.kind);
            if (lane.kind == LaneKind::Group) {
                detail = tr("Group · %1 signal(s) · %2")
                             .arg(static_cast<qulonglong>(
                                 visibleGroupMemberCount(lane.id)))
                             .arg(
                                 collapsedGroupIds_.contains(lane.id)
                                     ? tr("Collapsed")
                                     : tr("Expanded"));
            } else if (lane.kind == LaneKind::Bus || lane.kind == LaneKind::Enum) {
                detail += tr(" · %1-bit").arg(lane.width);
            }
            fitted = std::max(
                fitted,
                detailMetrics.horizontalAdvance(detail) + 28 + memberIndent);
        }
    }
    return std::clamp(fitted, MinimumHeaderWidth, MaximumHeaderWidth);
}

void WaveCanvas::updateAddLaneButtonGeometry()
{
    const auto row = addLaneRowRect();
    const auto visible = scenario_
        && row.bottom() >= RulerHeight
        && row.top() < viewport()->height();
    for (auto* button : addLaneButtons_) {
        if (button) button->setVisible(visible);
    }
    const auto hiddenCount = hiddenLaneCount();
    if (showHiddenLanesButton_) {
        showHiddenLanesButton_->setVisible(visible && hiddenCount > 0);
    }
    if (!visible) return;

    const auto empty = std::none_of(
        scenario_->lanes.begin(),
        scenario_->lanes.end(),
        [](const Lane& lane) { return lane.visible && lane.kind != LaneKind::Group; });
    const auto spacing = empty ? 10 : 4;
    const auto buttonWidth = empty ? 92 : (headerWidth_ - 20 - spacing * 2) / 3;
    const auto buttonHeight = empty ? 38 : row.height() - 12;
    const auto totalWidth = buttonWidth * 3 + spacing * 2;
    const auto startX = empty
        ? headerWidth_ + std::max(16, (waveViewportWidth() - totalWidth) / 2)
        : 10;
    const auto startY = empty
        ? RulerHeight + std::max(94, (viewport()->height() - RulerHeight - buttonHeight) / 3)
        : row.top() + 6;
    for (std::size_t index = 0; index < addLaneButtons_.size(); ++index) {
        auto* button = addLaneButtons_.at(index);
        if (!button) continue;
        button->setGeometry(
            startX + static_cast<int>(index) * (buttonWidth + spacing),
            startY,
            buttonWidth,
            buttonHeight);
        button->raise();
    }

    if (!showHiddenLanesButton_ || hiddenCount == 0) return;
    const auto label = hiddenCount == 1
        ? tr("Show 1 hidden item")
        : tr("Show %1 hidden items").arg(static_cast<qulonglong>(hiddenCount));
    showHiddenLanesButton_->setText(label);
    showHiddenLanesButton_->setAccessibleName(label);
    if (hiddenLanesMenu_) {
        hiddenLanesMenu_->clear();
        showHiddenLanesButton_->setMenu(nullptr);
        showHiddenLanesButton_->setPopupMode(QToolButton::DelayedPopup);
        if (hiddenCount > 1) {
            for (const auto& lane : scenario_->lanes) {
                if (lane.visible) continue;
                const auto name = QString::fromStdString(lane.name);
                auto escapedName = name;
                escapedName.replace(QLatin1Char('&'), QStringLiteral("&&"));
                auto* action = hiddenLanesMenu_->addAction(
                    lane.kind == LaneKind::Group
                        ? tr("Show group %1").arg(escapedName)
                        : tr("Show signal %1").arg(escapedName));
                action->setObjectName(QStringLiteral("ShowHiddenLaneAction"));
                action->setData(QString::fromStdString(lane.id));
                action->setToolTip(
                    tr("Restore only %1 in its original position").arg(name));
                connect(
                    action,
                    &QAction::triggered,
                    this,
                    [this, laneId = QString::fromStdString(lane.id)] {
                        emit showHiddenLaneRequested(laneId);
                    },
                    Qt::QueuedConnection);
            }
            hiddenLanesMenu_->addSeparator();
            auto* showAll = hiddenLanesMenu_->addAction(
                tr("Show all %1 hidden items").arg(
                    static_cast<qulonglong>(hiddenCount)));
            showAll->setObjectName(QStringLiteral("ShowAllHiddenLanesMenuAction"));
            connect(
                showAll,
                &QAction::triggered,
                this,
                [this] { emit showHiddenLanesRequested(); },
                Qt::QueuedConnection);
            showHiddenLanesButton_->setMenu(hiddenLanesMenu_);
            showHiddenLanesButton_->setPopupMode(QToolButton::MenuButtonPopup);
        }
    }
    showHiddenLanesButton_->setToolTip(
        hiddenCount > 1
            ? tr("Click to restore all; use the arrow to restore one by name")
            : tr("Restore the hidden signal or group as one undoable edit"));
    const auto hiddenWidth = std::clamp(
        showHiddenLanesButton_->sizeHint().width() + 12,
        142,
        std::max(142, waveViewportWidth() - 28));
    const auto hiddenX = empty
        ? headerWidth_ + std::max(14, (waveViewportWidth() - hiddenWidth) / 2)
        : headerWidth_ + 14;
    const auto hiddenY = empty
        ? startY + buttonHeight + 10
        : row.top() + 6;
    showHiddenLanesButton_->setGeometry(hiddenX, hiddenY, hiddenWidth, buttonHeight);
    showHiddenLanesButton_->raise();
}

void WaveCanvas::positionQuickLaneSetup()
{
    if (!hasQuickLaneSetup() || !scenario_) return;
    const auto laneId = quickLaneSetupLaneId_.toStdString();
    const auto layout = std::find_if(
        laneLayout_.begin(),
        laneLayout_.end(),
        [this, &laneId](const LaneLayout& candidate) {
            return candidate.laneIndex < scenario_->lanes.size()
                && scenario_->lanes.at(candidate.laneIndex).id == laneId;
        });
    if (layout == laneLayout_.end()) {
        quickLaneSetupPanel_->hide();
        quickLaneSetupLaneId_.clear();
        return;
    }
    quickLaneSetupPanel_->adjustSize();
    const auto hint = quickLaneSetupPanel_->sizeHint();
    const auto width = std::clamp(
        hint.width(),
        std::min(420, std::max(1, viewport()->width() - 16)),
        std::max(1, viewport()->width() - 16));
    const auto height = std::min(
        std::max(36, hint.height()),
        std::max(36, layout->height - 8));
    const auto laneTop = RulerHeight + layout->top - verticalScrollBar()->value();
    const auto y = std::clamp(
        laneTop + (layout->height - height) / 2,
        RulerHeight + 2,
        std::max(RulerHeight + 2, viewport()->height() - height - 2));
    quickLaneSetupPanel_->setGeometry(8, y, width, height);
    quickLaneSetupPanel_->raise();
}

void WaveCanvas::positionLaneRename()
{
    if (!hasLaneRename() || !scenario_) return;
    const auto laneId = laneRenameLaneId_.toStdString();
    const auto layout = std::find_if(
        laneLayout_.begin(),
        laneLayout_.end(),
        [this, &laneId](const LaneLayout& candidate) {
            return candidate.laneIndex < scenario_->lanes.size()
                && scenario_->lanes.at(candidate.laneIndex).id == laneId;
        });
    if (layout == laneLayout_.end()) {
        finishLaneRename();
        return;
    }
    const auto laneTop = RulerHeight + layout->top - verticalScrollBar()->value();
    const auto height = std::clamp(layout->height / 2 + 2, 24, 32);
    laneRenameEdit_->setGeometry(10, laneTop + 3, headerWidth_ - 20, height);
    laneRenameEdit_->raise();
}

void WaveCanvas::positionDurationEditor()
{
    if (!durationEdit_ || !durationLabel_) return;
    constexpr int editWidth = 112;
    constexpr int editHeight = 27;
    const auto editX = std::max(headerWidth_ + 62, viewport()->width() - editWidth - 8);
    durationEdit_->setGeometry(editX, 6, editWidth, editHeight);
    durationLabel_->setGeometry(std::max(headerWidth_ + 4, editX - 38), 6, 34, editHeight);
    durationLabel_->raise();
    durationEdit_->raise();
}

WaveCanvas::BusEditActionAssessment
WaveCanvas::assessBusEditAction(const BusEditAction action) const
{
    BusEditActionAssessment assessment;
    auto& state = assessment.state;
    const auto actionName = [this, action] {
        switch (action) {
        case BusEditAction::ApplyDraft:
            return tr("Apply value");
        case BusEditAction::PresetZero:
            return tr("Set 0");
        case BusEditAction::PresetReserved:
            return tr("Set Reserved");
        case BusEditAction::PresetX:
            return tr("Set X");
        case BusEditAction::PresetZ:
            return tr("Set Z");
        case BusEditAction::PresetDontCare:
            return tr("Set Don't care");
        case BusEditAction::Clear:
            return tr("Clear");
        }
        return tr("Edit value");
    }();
    state.summary =
        tr("%1 unavailable · open a Bus or Enum Beat/Segment editor first")
            .arg(actionName);
    if (!scenario_ || !project_ || !busPresetPalette_
        || !busPresetPalette_->isVisible()
        || busPresetLaneId_.empty() || !busPresetAnchorTick_
        || !busEditRange_) {
        return assessment;
    }

    const auto* lane = findLane(*scenario_, busPresetLaneId_);
    if (!lane
        || (lane->kind != LaneKind::Bus
            && lane->kind != LaneKind::Enum)) {
        state.summary =
            tr("%1 unavailable · the edited signal no longer exists")
                .arg(actionName);
        return assessment;
    }
    const auto presetAction =
        action == BusEditAction::PresetZero
        || action == BusEditAction::PresetReserved
        || action == BusEditAction::PresetX
        || action == BusEditAction::PresetZ
        || action == BusEditAction::PresetDontCare;
    if (presetAction && lane->kind != LaneKind::Bus) {
        state.summary =
            tr("%1 unavailable · Enum targets use declared symbols or numeric values")
                .arg(actionName);
        return assessment;
    }
    state.applicable = true;

    const auto [start, end] = *busEditRange_;
    state.start = start;
    state.end = end;
    if (start < 0 || end <= start || end > scenario_->duration) {
        state.summary =
            tr("%1 unavailable · target range is outside the current End")
                .arg(actionName);
        return assessment;
    }

    const auto clear = action == BusEditAction::Clear;
    std::string presetId;
    if (action == BusEditAction::PresetZero) {
        presetId = "zero";
    } else if (action == BusEditAction::PresetReserved) {
        presetId = "reserved";
    } else if (action == BusEditAction::PresetX) {
        presetId = "x";
    } else if (action == BusEditAction::PresetZ) {
        presetId = "z";
    } else if (action == BusEditAction::PresetDontCare) {
        presetId = "dont-care";
    }

    BusDraftValues draft;
    bool sequence = false;
    std::string normalizedValue;
    if (clear) {
        state.valueCount = 1;
        state.displayValue = tr("implicit X");
    } else if (!presetAction) {
        draft = parseBusDraftValues(*lane);
        sequence = draft.sequence;
        state.valueCount = draft.valueCount;
        state.sequence = sequence;
        if (!draft.valid) {
            auto reason = draft.error;
            if (lane->kind == LaneKind::Enum
                && !lane->enumMap.empty()) {
                QStringList symbols;
                for (const auto& [symbol, mappedValue] : lane->enumMap) {
                    Q_UNUSED(mappedValue);
                    symbols.append(QString::fromStdString(symbol));
                }
                reason += tr(" · symbols: %1")
                              .arg(symbols.join(QStringLiteral(", ")));
            }
            state.summary =
                tr("%1 unavailable · %2").arg(actionName, reason);
            return assessment;
        }
        if (sequence && busEditScope_ != BusEditScope::Beat) {
            state.summary =
                tr("Sequence unavailable for Segment · click Segment to switch to Beat; the draft will be preserved");
            return assessment;
        }
        normalizedValue = draft.normalizedValues.front();
        state.displayValue = sequence
            ? tr("%1 values").arg(static_cast<qulonglong>(
                  draft.normalizedValues.size()))
            : QString::fromStdString(normalizedValue);
    } else {
        state.valueCount = 1;
        const auto targetValue =
            busPresetValue(presetId, lane->width);
        const auto validation = validateLaneValue(*lane, targetValue);
        if (targetValue.empty() || !validation.valid) {
            state.summary =
                tr("%1 unavailable · %2")
                    .arg(
                        actionName,
                        QString::fromStdString(validation.error));
            return assessment;
        }
        normalizedValue = validation.normalizedValue;
        state.displayValue = busPresetDisplayLabel(presetId);
    }

    auto projectedEnd = end;
    Tick sequenceStep = 0;
    if (sequence) {
        sequenceStep = beatGrid(*lane).first;
        const auto count = static_cast<Tick>(
            draft.normalizedValues.size());
        if (sequenceStep <= 0
            || count <= 0
            || sequenceStep
                > (std::numeric_limits<Tick>::max() - start) / count) {
            state.summary =
                tr("Sequence unavailable · its final beat exceeds the supported timeline range");
            return assessment;
        }
        projectedEnd = start + sequenceStep * count;
        state.end = projectedEnd;
        state.extendsEnd = projectedEnd > scenario_->duration;
    }

    BusEditProjection projection;
    projection.action = action;
    projection.laneId = lane->id;
    projection.start = start;
    projection.end = projectedEnd;
    projection.lane = *lane;
    projection.extendsEnd = state.extendsEnd;
    projection.displayValue = state.displayValue;

    std::unordered_set<std::string> reservedSegmentIds;
    for (const auto& existingLane : scenario_->lanes) {
        for (const auto& segment : existingLane.segments) {
            reservedSegmentIds.insert(segment.id);
        }
    }
    std::size_t previewSequence = 0;
    const auto nextPreviewSegmentId =
        [&reservedSegmentIds, &previewSequence] {
            std::string candidate;
            do {
                candidate = std::string{"__bus_edit_preview_segment_"}
                    + std::to_string(++previewSequence);
            } while (reservedSegmentIds.contains(candidate));
            reservedSegmentIds.insert(candidate);
            return candidate;
        };

    if (clear) {
        auto nextId = nextPreviewSegmentId;
        clearProjectedSegmentRange(
            projection.lane,
            start,
            end,
            nextId);
    } else if (sequence) {
        auto cellStart = start;
        projection.sequenceSteps.reserve(
            draft.normalizedValues.size());
        for (const auto& value : draft.normalizedValues) {
            const auto cellEnd = cellStart + sequenceStep;
            projection.sequenceSteps.push_back({
                cellStart,
                cellEnd,
                value,
                {},
            });
            if (!projectedRangeAlreadyEquals(
                    projection.lane,
                    cellStart,
                    cellEnd,
                    value,
                    {})) {
                auto nextId = nextPreviewSegmentId;
                setProjectedSegmentRange(
                    projection.lane,
                    cellStart,
                    cellEnd,
                    value,
                    {},
                    {},
                    nextId);
            }
            cellStart = cellEnd;
        }
    } else if (busEditScope_ == BusEditScope::Segment) {
        const auto probe = start + (end - start) / 2;
        const auto segment = std::find_if(
            projection.lane.segments.begin(),
            projection.lane.segments.end(),
            [probe](const Segment& candidate) {
                return candidate.start <= probe
                    && candidate.end > probe;
            });
        if (segment == projection.lane.segments.end()
            || segment->start != start || segment->end != end) {
            state.summary =
                tr("%1 unavailable · the selected Segment changed; select it again")
                    .arg(actionName);
            return assessment;
        }
        auto extensions = segment->extensions;
        if (presetAction) {
            extensions[std::string(kBusPresetExtension)] =
                "\"" + presetId + "\"";
        } else if (segment->value != normalizedValue) {
            extensions.erase(std::string(kBusPresetExtension));
        }
        segment->value = normalizedValue;
        segment->extensions = std::move(extensions);
        normalizeProjectedSegments(projection.lane);
    } else {
        JsonExtensions extensions;
        if (presetAction) {
            extensions.emplace(
                std::string(kBusPresetExtension),
                "\"" + presetId + "\"");
        }
        if (!projectedRangeAlreadyEquals(
                projection.lane,
                start,
                end,
                normalizedValue,
                extensions)) {
            auto nextId = nextPreviewSegmentId;
            setProjectedSegmentRange(
                projection.lane,
                start,
                end,
                normalizedValue,
                {},
                std::move(extensions),
                nextId);
        }
    }

    projection.waveformChanges =
        !sameProjectedWaveform(projection.lane, *lane);
    projection.relationImpact =
        relationRemovalImpactForProjectedLanes({projection.lane});
    projection.modelChanges =
        projection.waveformChanges
        || !projection.relationImpact.ids.empty()
        || projection.extendsEnd;
    state.valid = true;
    state.modelChanges = projection.modelChanges;
    state.relationRemovalCount =
        projection.relationImpact.ids.size();

    const auto effectiveActionName = sequence
        ? tr("Apply %1 values").arg(static_cast<qulonglong>(
              draft.normalizedValues.size()))
        : actionName;
    const auto scope = sequence
        ? tr("Sequence")
        : busEditScope_ == BusEditScope::Segment
            ? tr("Segment")
            : tr("Beat");
    const auto range = tr("%1–%2")
        .arg(
            QString::fromStdString(
                formatTick(start, project_->timeBase)),
            QString::fromStdString(
                formatTick(projectedEnd, project_->timeBase)));
    QString result;
    if (clear) {
        result = tr("→ implicit X");
    } else if (sequence) {
        QStringList sample;
        const auto shown = std::min<std::size_t>(
            draft.normalizedValues.size(),
            4);
        for (std::size_t index = 0; index < shown; ++index) {
            sample.append(QString::fromStdString(
                draft.normalizedValues.at(index)));
        }
        if (shown < draft.normalizedValues.size()) {
            sample.append(QStringLiteral("…"));
        }
        result = tr("= %1 [%2]")
                     .arg(
                         state.displayValue,
                         sample.join(QStringLiteral(", ")));
    } else {
        result = tr("= %1").arg(state.displayValue);
    }
    if (!state.modelChanges) {
        state.summary =
            tr("%1 · no change · %2 %3 %4 %5 already matches · no command will run")
                .arg(
                    effectiveActionName,
                    QString::fromStdString(lane->name),
                    scope,
                    range,
                    result);
    } else if (!projection.relationImpact.ids.empty()) {
        state.summary =
            tr("%1 preview ⚠%2 · %3 %4 %5 %6 · removes %2 relation(s) · Ctrl+Z restores waveform and relations")
                .arg(effectiveActionName)
                .arg(static_cast<qulonglong>(
                    projection.relationImpact.ids.size()))
                .arg(
                    QString::fromStdString(lane->name),
                    scope,
                    range,
                    result);
        if (!projection.relationImpact.summaries.isEmpty()) {
            state.summary += tr(" · affected: %1").arg(
                projection.relationImpact.summaries.join(
                    QStringLiteral("; ")));
        }
    } else {
        state.summary =
            tr("%1 preview · %2 %3 %4 %5 · no Relation will be removed")
                .arg(
                    effectiveActionName,
                    QString::fromStdString(lane->name),
                    scope,
                    range,
                    result);
    }
    if (projection.extendsEnd) {
        state.summary += tr(" · End → %1")
                             .arg(QString::fromStdString(
                                 formatTick(
                                     projection.end,
                                     project_->timeBase)));
    }
    projection.summary = state.summary;
    assessment.projection = std::move(projection);
    return assessment;
}

void WaveCanvas::updateBusEditContextLabel(
    const BusEditActionState* previewState)
{
    if (!busPresetContextLabel_ || !scenario_ || !project_
        || busPresetLaneId_.empty() || !busEditRange_) {
        return;
    }
    const auto* lane = findLane(*scenario_, busPresetLaneId_);
    if (!lane) return;
    const auto sequence =
        previewState && previewState->sequence;
    const auto start =
        previewState ? previewState->start : busEditRange_->first;
    const auto end =
        previewState ? previewState->end : busEditRange_->second;
    const auto scope = sequence
        ? previewState->valid
            ? tr("Sequence · %1 values")
                  .arg(static_cast<qulonglong>(
                      previewState->valueCount))
            : tr("Sequence")
        : busEditScope_ == BusEditScope::Segment
            ? tr("Segment")
            : tr("Beat");
    auto text = sequence && !previewState->valid
        ? tr("%1 · %2 · starts %3")
              .arg(
                  QString::fromStdString(lane->name),
                  scope,
                  QString::fromStdString(
                      formatTick(start, project_->timeBase)))
        : tr("%1 · %2 · %3–%4")
              .arg(
                  QString::fromStdString(lane->name),
                  scope,
                  QString::fromStdString(
                      formatTick(start, project_->timeBase)),
                  QString::fromStdString(
                      formatTick(end, project_->timeBase)));
    if (previewState) {
        if (!previewState->valid) {
            text += tr(" · invalid");
        } else if (!previewState->modelChanges) {
            text += tr(" · no change");
        } else if (previewState->relationRemovalCount > 0) {
            if (!sequence && !previewState->displayValue.isEmpty()) {
                text += tr(" · %1")
                            .arg(previewState->displayValue);
            }
            text += tr(" · ⚠%1")
                        .arg(static_cast<qulonglong>(
                            previewState->relationRemovalCount));
        } else if (!sequence
                   && !previewState->displayValue.isEmpty()) {
            text += tr(" · %1").arg(previewState->displayValue);
        }
        if (previewState->extendsEnd) {
            text += tr(" · End → %1")
                        .arg(QString::fromStdString(
                            formatTick(
                                previewState->end,
                                project_->timeBase)));
        }
        busPresetContextLabel_->setToolTip(previewState->summary);
        busPresetContextLabel_->setAccessibleDescription(
            previewState->summary);
    } else {
        auto contextHelp = tr("Applies to %1 from %2 to %3")
                               .arg(
                                   scope.toLower(),
                                   QString::fromStdString(
                                       formatTick(
                                           start,
                                           project_->timeBase)),
                                   QString::fromStdString(
                                       formatTick(
                                           end,
                                           project_->timeBase)));
        contextHelp.append(
            asynchronousEditing_
                ? tr("\nAsync mode: this beat may start away from a clock edge")
                : tr("\nSync mode: one beat follows the associated clock"));
        contextHelp.append(
            busEditScope_ == BusEditScope::Beat
                ? tr("\nTab applies and advances · Shift+Tab applies and goes back"
                     "\nPaste comma-, space-, or line-separated values to fill consecutive beats in one Undo"
                     "\nUse value*N to repeat one value, for example 0x00*8")
                : tr("\nTab applies and opens the next Segment · Shift+Tab opens the previous Segment"));
        if (lane->kind == LaneKind::Enum && !lane->enumMap.empty()) {
            QStringList symbols;
            for (const auto& [symbol, value] : lane->enumMap) {
                Q_UNUSED(value);
                symbols.append(QString::fromStdString(symbol));
            }
            contextHelp.append(
                tr("\nSymbols: %1")
                    .arg(symbols.join(QStringLiteral(", "))));
        }
        busPresetContextLabel_->setToolTip(contextHelp);
        busPresetContextLabel_->setAccessibleDescription(contextHelp);
    }
    busPresetContextLabel_->setText(text);
}

void WaveCanvas::updateBusEditActionStates(const bool announceDraft)
{
    if (!busPresetPalette_ || !busPresetPalette_->isVisible()
        || !scenario_ || busPresetLaneId_.empty()) {
        busEditActionPreview_.reset();
        busEditPreviewAction_.reset();
        return;
    }
    const auto setDynamicProperty =
        [](QWidget* widget, const char* name, const bool value) {
            if (!widget || widget->property(name).toBool() == value) return;
            widget->setProperty(name, value);
            widget->style()->unpolish(widget);
            widget->style()->polish(widget);
            widget->update();
        };
    const std::array<BusEditAction, 5> presetActions{{
        BusEditAction::PresetZero,
        BusEditAction::PresetReserved,
        BusEditAction::PresetX,
        BusEditAction::PresetZ,
        BusEditAction::PresetDontCare,
    }};
    const std::array<QString, 5> presetLabels{{
        QStringLiteral("0"),
        tr("Reserved"),
        QStringLiteral("X"),
        QStringLiteral("Z"),
        tr("Don't care"),
    }};
    for (std::size_t index = 0;
         index < busPresetButtons_.size();
         ++index) {
        auto* button = busPresetButtons_.at(index);
        if (!button) continue;
        const auto state =
            assessBusEditAction(presetActions.at(index)).state;
        const auto risk = state.relationRemovalCount > 0;
        button->setEnabled(state.valid && state.modelChanges);
        button->setText(
            risk
                ? tr("%1 ⚠%2")
                      .arg(presetLabels.at(index))
                      .arg(static_cast<qulonglong>(
                          state.relationRemovalCount))
                : state.valid && !state.modelChanges
                    ? tr("%1 · same").arg(presetLabels.at(index))
                    : presetLabels.at(index));
        button->setToolTip(state.summary);
        button->setAccessibleDescription(state.summary);
        setDynamicProperty(button, "relationRisk", risk);
    }

    const auto clearState =
        assessBusEditAction(BusEditAction::Clear).state;
    if (busClearButton_) {
        const auto risk = clearState.relationRemovalCount > 0;
        busClearButton_->setEnabled(
            clearState.valid && clearState.modelChanges);
        busClearButton_->setText(
            risk
                ? tr("Clear ⚠%1")
                      .arg(static_cast<qulonglong>(
                          clearState.relationRemovalCount))
                : clearState.valid && !clearState.modelChanges
                    ? tr("Clear · empty")
                    : tr("Clear"));
        busClearButton_->setToolTip(clearState.summary);
        busClearButton_->setAccessibleDescription(clearState.summary);
        setDynamicProperty(busClearButton_, "relationRisk", risk);
    }

    const auto draftModified =
        busValueEdit_ && busValueEdit_->isModified();
    if (!draftModified) {
        if (busApplyButton_) {
            busApplyButton_->setText(tr("Apply"));
            busApplyButton_->setEnabled(true);
            busApplyButton_->setToolTip(
                tr("Apply the current value and close the editor (Enter)"));
            busApplyButton_->setAccessibleDescription(
                busApplyButton_->toolTip());
            setDynamicProperty(busApplyButton_, "relationRisk", false);
        }
        if (busPreviousButton_) busPreviousButton_->setEnabled(true);
        if (busNextButton_) busNextButton_->setEnabled(true);
        if (busValueEdit_) {
            setDynamicProperty(busValueEdit_, "relationRisk", false);
            setDynamicProperty(busValueEdit_, "noEffect", false);
            setDynamicProperty(busValueEdit_, "invalidDraft", false);
        }
        busEditActionPreview_.reset();
        busEditPreviewAction_.reset();
        updateBusEditContextLabel();
        viewport()->update();
        return;
    }

    const auto draftAssessment =
        assessBusEditAction(BusEditAction::ApplyDraft);
    const auto& draftState = draftAssessment.state;
    const auto draftRisk =
        draftState.relationRemovalCount > 0;
    const auto noEffect =
        draftState.valid && !draftState.modelChanges;
    if (busScopeButton_ && busPresetAnchorTick_) {
        if (draftState.sequence) {
            const auto canRecoverToBeat =
                busEditScope_ == BusEditScope::Segment;
            busScopeButton_->setEnabled(canRecoverToBeat);
            busScopeButton_->setToolTip(
                canRecoverToBeat
                    ? tr("Switch to Beat and preserve this sequence draft")
                    : tr("Apply or discard the sequence before switching to Segment"));
        } else {
            const auto probe = std::clamp<Tick>(
                *busPresetAnchorTick_,
                0,
                std::max<Tick>(0, scenario_->duration - 1));
            const auto* lane =
                findLane(*scenario_, busPresetLaneId_);
            const auto* segment = lane
                ? segmentAtTick(*lane, probe)
                : nullptr;
            busScopeButton_->setEnabled(
                busEditScope_ == BusEditScope::Segment
                || segment);
            busScopeButton_->setToolTip(
                busEditScope_ == BusEditScope::Segment
                    ? tr("Switch to one beat at the current target")
                    : segment
                        ? tr("Switch to the complete explicit Segment containing this beat")
                        : tr("This beat is implicit X; no complete explicit Segment exists"));
        }
        busScopeButton_->setAccessibleDescription(
            busScopeButton_->toolTip());
    }
    if (busApplyButton_) {
        const auto applyLabel =
            draftState.valid && draftState.sequence
            ? tr("Apply %1").arg(static_cast<qulonglong>(
                  draftState.valueCount))
            : tr("Apply");
        busApplyButton_->setEnabled(draftState.valid);
        busApplyButton_->setText(
            !draftState.valid
                ? applyLabel
                : noEffect
                    ? tr("Done")
                    : draftRisk
                        ? tr("%1 ⚠%2")
                              .arg(applyLabel)
                              .arg(static_cast<qulonglong>(
                                  draftState.relationRemovalCount))
                        : applyLabel);
        busApplyButton_->setToolTip(draftState.summary);
        busApplyButton_->setAccessibleDescription(
            draftState.summary);
        setDynamicProperty(
            busApplyButton_,
            "relationRisk",
            draftRisk);
    }
    if (busPreviousButton_) {
        busPreviousButton_->setEnabled(
            draftState.valid && !draftState.sequence);
        const auto tooltip = draftState.sequence
            ? tr("Sequence applies all values at once; navigation resumes after it is applied")
            : !draftState.valid
                ? draftState.summary
                : busEditScope_ == BusEditScope::Segment
                    ? tr("Apply the draft and open the previous Segment (Shift+Tab)")
                    : tr("Apply the draft and move to the previous beat (Shift+Tab)");
        busPreviousButton_->setToolTip(tooltip);
        busPreviousButton_->setAccessibleDescription(tooltip);
    }
    if (busNextButton_) {
        busNextButton_->setEnabled(
            draftState.valid && !draftState.sequence);
        const auto tooltip = draftState.sequence
            ? tr("Sequence applies all values at once; navigation resumes after it is applied")
            : !draftState.valid
                ? draftState.summary
                : busEditScope_ == BusEditScope::Segment
                    ? tr("Apply the draft and open the next Segment (Tab)")
                    : tr("Apply the draft and move to the next beat (Tab)");
        busNextButton_->setToolTip(tooltip);
        busNextButton_->setAccessibleDescription(tooltip);
    }
    if (busValueEdit_) {
        setDynamicProperty(
            busValueEdit_,
            "relationRisk",
            draftRisk);
        setDynamicProperty(
            busValueEdit_,
            "noEffect",
            noEffect);
        setDynamicProperty(
            busValueEdit_,
            "invalidDraft",
            !draftState.valid);
        busValueEdit_->setToolTip(draftState.summary);
        busValueEdit_->setAccessibleDescription(
            draftState.summary);
    }
    busEditActionPreview_ = draftAssessment.projection;
    busEditPreviewAction_ = draftAssessment.projection
        ? std::optional<BusEditAction>{
              BusEditAction::ApplyDraft}
        : std::nullopt;
    updateBusEditContextLabel(&draftState);
    if (announceDraft) emit statusMessage(draftState.summary);
    viewport()->update();
}

void WaveCanvas::restoreBusEditDraftPreview(const bool announce)
{
    updateBusEditActionStates(announce);
}

void WaveCanvas::positionBusPresetPalette()
{
    if (!busPresetPalette_ || !scenario_ || busPresetLaneId_.empty()
        || !busPresetAnchorTick_ || !busEditRange_) {
        hideBusPresetPalette();
        return;
    }
    const auto* lane = findLane(*scenario_, busPresetLaneId_);
    if (!lane
        || !isLaneDisplayed(*lane)
        || (lane->kind != LaneKind::Bus && lane->kind != LaneKind::Enum)) {
        hideBusPresetPalette();
        return;
    }
    busPresetPalette_->adjustSize();
    busPresetPalette_->updateGeometry();
    const auto layout = std::find_if(
        laneLayout_.begin(),
        laneLayout_.end(),
        [this](const LaneLayout& candidate) {
            return scenario_->lanes.at(candidate.laneIndex).id
                == busPresetLaneId_;
        });
    if (layout == laneLayout_.end()) {
        hideBusPresetPalette();
        return;
    }

    const auto paletteSize = busPresetPalette_->sizeHint();
    const auto maximumWidth = std::max(180, viewport()->width() - headerWidth_ - 16);
    const auto width = std::min(paletteSize.width(), maximumWidth);
    const auto height = paletteSize.height();
    const auto targetCenter = xAtTick(
        busEditRange_->first
        + (busEditRange_->second - busEditRange_->first) / 2);
    const auto minimumX = headerWidth_ + 8;
    const auto maximumX = std::max(
        minimumX,
        viewport()->width() - width - 8);
    const auto x = std::clamp(targetCenter - width / 2, minimumX, maximumX);
    const auto laneTop =
        RulerHeight + layout->top - verticalScrollBar()->value();
    const auto above = laneTop - height - 6;
    const auto below = laneTop + layout->height + 6;
    const auto preferredY = above >= RulerHeight + 4 ? above : below;
    const auto y = std::clamp(
        preferredY,
        RulerHeight + 4,
        std::max(RulerHeight + 4, viewport()->height() - height - 8));
    busPresetPalette_->setGeometry(x, y, width, height);
    if (!busEditPaletteVisible_) {
        busEditPaletteVisible_ = true;
        emit busEditPaletteVisibilityChanged(true);
    }
    busPresetPalette_->show();
    busPresetPalette_->raise();
}

void WaveCanvas::cancelBusValueEdit()
{
    if (!busPresetPalette_ || !busPresetPalette_->isVisible()) return;
    const auto discarded = busValueEdit_ && busValueEdit_->isModified();
    const auto laneId = busPresetLaneId_;
    const auto* lane = scenario_ ? findLane(*scenario_, laneId) : nullptr;
    const auto laneName = lane
        ? QString::fromStdString(lane->name)
        : tr("Bus");
    hideBusPresetPalette();
    emit statusMessage(
        discarded
            ? tr("%1 value draft discarded · waveform unchanged · target kept")
                  .arg(laneName)
            : tr("%1 value editor closed · target kept").arg(laneName));
    viewport()->update();
}

void WaveCanvas::clearBusEditTarget()
{
    if (!scenario_ || !commandStack_ || busPresetLaneId_.empty()
        || !busPresetAnchorTick_ || !busEditRange_) {
        return;
    }
    const auto* lane = findLane(*scenario_, busPresetLaneId_);
    if (!lane
        || (lane->kind != LaneKind::Bus && lane->kind != LaneKind::Enum)) {
        hideBusPresetPalette();
        return;
    }
    const auto [start, end] = *busEditRange_;
    if (start < 0 || end <= start || end > scenario_->duration) return;
    const auto preflight =
        assessBusEditAction(BusEditAction::Clear);
    if (!preflight.state.valid) {
        updateBusEditActionStates();
        emit statusMessage(preflight.state.summary);
        return;
    }
    if (!preflight.state.modelChanges) {
        if (busValueEdit_) {
            busValueEdit_->setModified(false);
            busValueEdit_->setStyleSheet({});
        }
        updateBusEditActionStates();
        emit statusMessage(preflight.state.summary);
        return;
    }

    const auto laneId = lane->id;
    const auto laneName = lane->name;
    const auto clearedScope = busEditScope_;
    const auto discardedDraft = busValueEdit_ && busValueEdit_->isModified();
    const auto historyStateBefore = commandStack_->stateId();
    const auto historySelectionBefore = historySelectionSnapshot();
    if (busValueEdit_) {
        busValueEdit_->setModified(false);
        busValueEdit_->setStyleSheet({});
    }

    const auto relationCountBefore = scenario_->relations.size();
    bool changed = false;
    try {
        changed = commandStack_->execute(
            std::make_unique<ClearLaneRangeCommand>(
                *scenario_,
                laneId,
                start,
                end));
    } catch (const std::exception& exception) {
        emit statusMessage(
            tr("No values changed · %1")
                .arg(QString::fromUtf8(exception.what())));
        return;
    }

    selectedLaneId_ = laneId;
    selectedLaneIds_ = {laneId};
    laneHeaderSelectionActive_ = false;
    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    cursorTick_ = start;
    if (changed) {
        rememberHistorySelectionTransition(
            historyStateBefore,
            historySelectionBefore,
            commandStack_->stateId());
        emit modelEdited();
        emit commandAvailabilityChanged();
    }
    emit selectionChanged(QString::fromStdString(laneId), cursorTick_);
    refreshModel();

    if (const auto* refreshed = findLane(*scenario_, laneId)) {
        const auto nextBeat = clearedScope == BusEditScope::Segment
            ? editableBeatRangeAt(start, *refreshed)
            : std::pair{start, end};
        showBusPresetPalette(
            *refreshed,
            QPoint(xAtTick(nextBeat.first), 0),
            nextBeat.first,
            nextBeat,
            BusEditScope::Beat);
    }

    const auto scope = clearedScope == BusEditScope::Segment
        ? tr("Segment")
        : tr("Beat");
    auto message = changed
        ? tr("%1 %2 · %3–%4 cleared to implicit X · Beat target kept")
              .arg(QString::fromStdString(laneName))
              .arg(scope)
              .arg(QString::fromStdString(formatTick(
                  start,
                  project_->timeBase)))
              .arg(QString::fromStdString(formatTick(
                  end,
                  project_->timeBase)))
        : tr("%1 %2 · %3–%4 already uses implicit X · no values changed")
              .arg(QString::fromStdString(laneName))
              .arg(scope)
              .arg(QString::fromStdString(formatTick(
                  start,
                  project_->timeBase)))
              .arg(QString::fromStdString(formatTick(
                  end,
                  project_->timeBase)));
    if (discardedDraft) message.append(tr(" · draft discarded"));
    if (changed) {
        message = appendRelationAwareUndo(
            message,
            relationCountBefore,
            scenario_->relations.size());
        const auto removedRelationCount = relationCountBefore
            - std::min(
                relationCountBefore,
                scenario_->relations.size());
        if (preflight.state.relationRemovalCount
                == removedRelationCount
            && removedRelationCount > 0
            && preflight.projection
            && !preflight.projection->relationImpact
                    .summaries.isEmpty()) {
            message += tr(" · affected: %1").arg(
                preflight.projection->relationImpact
                    .summaries.join(QStringLiteral("; ")));
        }
    }
    emit statusMessage(message);
    viewport()->update();
}

bool WaveCanvas::toggleBusEditScope()
{
    if (!scenario_ || busPresetLaneId_.empty() || !busPresetAnchorTick_
        || !busEditRange_) {
        return false;
    }

    auto preserveSequenceDraft = false;
    auto sequenceDraftText = QString{};
    if (hasPendingBusValueEdit()) {
        const auto* pendingLane =
            findLane(*scenario_, busPresetLaneId_);
        const auto pendingDraft = pendingLane
            ? parseBusDraftValues(*pendingLane)
            : BusDraftValues{};
        if (pendingDraft.sequence) {
            if (busEditScope_ == BusEditScope::Beat) {
                emit statusMessage(
                    tr("Apply or discard the sequence before switching from Beat to Segment"));
                return false;
            }
            preserveSequenceDraft = true;
            sequenceDraftText = busValueEdit_
                ? busValueEdit_->text()
                : QString{};
        } else {
            submitBusValue(BusEditCommitAction::Stay);
            if (hasPendingBusValueEdit()) return false;
        }
    }

    const auto laneId = busPresetLaneId_;
    const auto* lane = findLane(*scenario_, laneId);
    if (!lane
        || (lane->kind != LaneKind::Bus && lane->kind != LaneKind::Enum)) {
        hideBusPresetPalette();
        return false;
    }

    const auto focusTick = std::clamp<Tick>(
        *busPresetAnchorTick_,
        0,
        std::max<Tick>(0, scenario_->duration - 1));
    if (busEditScope_ == BusEditScope::Beat) {
        const auto* segment = segmentAtTick(*lane, focusTick);
        if (!segment) {
            emit statusMessage(
                tr("%1 · current beat is implicit X, so no complete Segment exists")
                    .arg(QString::fromStdString(lane->name)));
            return false;
        }
        selectedSegmentLaneId_ = lane->id;
        selectedSegmentId_ = segment->id;
        showBusPresetPalette(
            *lane,
            QPoint(xAtTick(focusTick), 0),
            focusTick,
            std::pair{segment->start, segment->end},
            BusEditScope::Segment);
        emit statusMessage(
            tr("%1 · editing complete Segment %2–%3")
                .arg(QString::fromStdString(lane->name))
                .arg(QString::fromStdString(formatTick(
                    segment->start,
                    project_->timeBase)))
                .arg(QString::fromStdString(formatTick(
                    segment->end,
                    project_->timeBase))));
    } else {
        selectedSegmentLaneId_.clear();
        selectedSegmentId_.clear();
        const auto beat = editableBeatRangeAt(focusTick, *lane);
        showBusPresetPalette(
            *lane,
            QPoint(xAtTick(focusTick), 0),
            focusTick,
            beat,
            BusEditScope::Beat);
        if (preserveSequenceDraft && busValueEdit_) {
            busValueEdit_->setText(sequenceDraftText);
            busValueEdit_->setModified(true);
            updateBusEditActionStates(true);
            emit statusMessage(
                tr("%1 · switched to Beat · sequence preserved and previewed from %2")
                    .arg(QString::fromStdString(lane->name))
                    .arg(QString::fromStdString(formatTick(
                        beat.first,
                        project_->timeBase))));
        } else {
            emit statusMessage(
                tr("%1 · editing one beat %2–%3")
                    .arg(QString::fromStdString(lane->name))
                    .arg(QString::fromStdString(formatTick(
                        beat.first,
                        project_->timeBase)))
                    .arg(QString::fromStdString(formatTick(
                        beat.second,
                        project_->timeBase))));
        }
    }
    emit selectionChanged(QString::fromStdString(laneId), cursorTick_);
    if (busValueEdit_) {
        busValueEdit_->setFocus(Qt::OtherFocusReason);
        busValueEdit_->selectAll();
    }
    viewport()->update();
    return true;
}

void WaveCanvas::showBusPresetPalette(
    const Lane& lane,
    const QPoint& anchor,
    const std::optional<Tick> exactTick,
    const std::optional<std::pair<Tick, Tick>> exactRange,
    const BusEditScope exactRangeScope)
{
    const auto enumLane = lane.kind == LaneKind::Enum;
    if ((lane.kind != LaneKind::Bus && !enumLane)
        || !scenario_
        || scenario_->duration <= 0) {
        hideBusPresetPalette();
        return;
    }
    hideRangeEditPalette();
    busPresetLaneId_ = lane.id;
    const auto requested = exactTick.value_or(
        anchor.x() >= headerWidth_ ? tickAtX(anchor.x()) : cursorTick_);
    busPresetAnchorTick_ = std::clamp<Tick>(requested, 0, scenario_->duration - 1);
    if (exactRange
        && exactRange->first >= 0
        && exactRange->second > exactRange->first
        && exactRange->second <= scenario_->duration) {
        busEditRange_ = exactRange;
        busEditScope_ = exactRangeScope;
    } else {
        busEditRange_ = editableBeatRangeAt(*busPresetAnchorTick_, lane);
        busEditScope_ = BusEditScope::Beat;
    }
    const auto [start, end] = *busEditRange_;
    selectedLaneId_ = lane.id;
    selectedLaneIds_ = {lane.id};
    laneHeaderSelectionActive_ = false;
    selectionRange_ = *busEditRange_;
    cursorTick_ = std::clamp<Tick>(
        *busPresetAnchorTick_,
        start,
        std::max(start, end - 1));
    waveEditHoverLaneId_.clear();
    waveEditHoverRange_.reset();
    if (busEditScope_ == BusEditScope::Beat) {
        selectedSegmentLaneId_.clear();
        selectedSegmentId_.clear();
    }
    QStringList enumSymbols;
    if (enumLane) {
        for (const auto& [symbol, value] : lane.enumMap) {
            Q_UNUSED(value);
            enumSymbols.append(QString::fromStdString(symbol));
        }
    }
    if (laneValueCompletionModel_) {
        laneValueCompletionModel_->setStringList(enumSymbols);
    }
    if (busPresetPalette_) {
        for (auto* button : busPresetPalette_->findChildren<QToolButton*>()) {
            if (button->objectName().startsWith(QStringLiteral("BusPreset"))) {
                button->setVisible(!enumLane);
            }
        }
    }
    if (busRadixCombo_) {
        busRadixCombo_->setVisible(!enumLane);
        const auto index = busRadixCombo_->findData(static_cast<int>(lane.radix));
        if (index >= 0) busRadixCombo_->setCurrentIndex(index);
    }
    if (busRecentValuesCombo_) {
        busRecentValuesCombo_->clear();
        busRecentValuesCombo_->addItem(tr("Recent"));
        const auto recent = busRecentValues_.find(lane.id);
        if (recent != busRecentValues_.end()) {
            busRecentValuesCombo_->addItems(recent->second);
        }
        busRecentValuesCombo_->setCurrentIndex(0);
        busRecentValuesCombo_->setEnabled(busRecentValuesCombo_->count() > 1);
    }
    if (busPresetContextLabel_) {
        const auto scope = busEditScope_ == BusEditScope::Segment
            ? tr("Segment")
            : tr("Beat");
        busPresetContextLabel_->setText(
            tr("%1 · %2 · %3–%4")
                .arg(QString::fromStdString(lane.name))
                .arg(scope)
                .arg(QString::fromStdString(formatTick(start, project_->timeBase)))
                .arg(QString::fromStdString(formatTick(end, project_->timeBase))));
        auto contextHelp = tr("Applies to %1 from %2 to %3")
                               .arg(scope.toLower())
                               .arg(QString::fromStdString(formatTick(
                                   start,
                                   project_->timeBase)))
                               .arg(QString::fromStdString(formatTick(
                                   end,
                                   project_->timeBase)));
        contextHelp.append(
            asynchronousEditing_
                ? tr("\nAsync mode: this beat may start away from a clock edge")
                : tr("\nSync mode: one beat follows the associated clock"));
        if (busEditScope_ == BusEditScope::Beat) {
            contextHelp.append(
                tr("\nTab applies and advances · Shift+Tab applies and goes back"
                   "\nPaste comma-, space-, or line-separated values to fill consecutive beats in one Undo"
                   "\nUse value*N to repeat one value, for example 0x00*8"));
        } else {
            contextHelp.append(
                tr("\nTab applies and opens the next Segment · Shift+Tab opens the previous Segment"));
        }
        if (!enumSymbols.isEmpty()) {
            contextHelp.append(
                tr("\nSymbols: %1").arg(enumSymbols.join(QStringLiteral(", "))));
        }
        busPresetContextLabel_->setToolTip(contextHelp);
    }
    const auto scopeProbe = std::clamp<Tick>(
        *busPresetAnchorTick_,
        0,
        std::max<Tick>(0, scenario_->duration - 1));
    const auto* scopeSegment = segmentAtTick(lane, scopeProbe);
    if (busScopeButton_) {
        busScopeButton_->setText(
            busEditScope_ == BusEditScope::Segment
                ? tr("Segment")
                : tr("Beat"));
        busScopeButton_->setEnabled(
            busEditScope_ == BusEditScope::Segment || scopeSegment);
        busScopeButton_->setToolTip(
            busEditScope_ == BusEditScope::Segment
                ? tr("Switch to one beat at the current target")
                : scopeSegment
                    ? tr("Switch to the complete explicit Segment containing this beat")
                    : tr("This beat is implicit X; no complete explicit Segment exists"));
    }
    if (busPreviousButton_) {
        busPreviousButton_->setToolTip(
            busEditScope_ == BusEditScope::Segment
                ? tr("Apply the draft and open the previous Segment (Shift+Tab)")
                : tr("Apply the draft and move to the previous beat (Shift+Tab)"));
    }
    if (busNextButton_) {
        busNextButton_->setToolTip(
            busEditScope_ == BusEditScope::Segment
                ? tr("Apply the draft and open the next Segment (Tab)")
                : tr("Apply the draft and move to the next beat (Tab)"));
    }
    if (busClearButton_) {
        busClearButton_->setToolTip(
            busEditScope_ == BusEditScope::Segment
                ? tr("Clear the complete Segment to implicit X and continue at its first beat")
                : tr("Clear only this beat to implicit X and keep the beat selected (Delete)"));
    }
    if (busValueEdit_) {
        const auto probe = start + (end - start) / 2;
        const auto* existing = segmentAtTick(lane, probe);
        busValueEdit_->setText(existing ? QString::fromStdString(existing->value) : QString{});
        busValueEdit_->setModified(false);
        busValueEdit_->setPlaceholderText(
            busEditScope_ == BusEditScope::Beat
                ? existing
                    ? enumLane
                        ? tr("Symbol/list · *N repeat")
                        : tr("Value/list · *N repeat")
                    : tr("Value/list · X (implicit) · *N")
                : enumLane
                    ? tr("Symbol or value · Tab next · Shift+Tab previous")
                    : tr("Value · Tab next · Shift+Tab previous"));
        busValueEdit_->setAccessibleName(
            enumLane ? tr("Enum value") : tr("Bus value"));
        busValueEdit_->setToolTip(
            enumLane && !enumSymbols.isEmpty()
                ? busEditScope_ == BusEditScope::Beat
                    ? tr("Type a symbol or numeric value · Tab applies and advances · Shift+Tab goes back · Enter finishes · symbols: %1")
                          .arg(enumSymbols.join(QStringLiteral(", ")))
                    : tr("Type a symbol or numeric value · Tab opens the next Segment · Shift+Tab opens the previous Segment · Enter finishes · symbols: %1")
                      .arg(enumSymbols.join(QStringLiteral(", ")))
                : enumLane
                    ? busEditScope_ == BusEditScope::Beat
                        ? tr("Type a value · Tab applies and advances · Shift+Tab goes back · Enter finishes")
                        : tr("Type a value · Tab opens the next Segment · Shift+Tab opens the previous Segment · Enter finishes")
                    : busEditScope_ == BusEditScope::Beat
                        ? tr("Type a value · Tab applies and advances · Shift+Tab goes back · Enter finishes · bare input uses the selected radix · width %1 bit(s)")
                              .arg(lane.width)
                        : tr("Type a value · Tab opens the next Segment · Shift+Tab opens the previous Segment · Enter finishes · bare input uses the selected radix · width %1 bit(s)")
                          .arg(lane.width));
        if (busEditScope_ == BusEditScope::Beat && !existing) {
            busValueEdit_->setToolTip(
                busValueEdit_->toolTip()
                + tr("\nCurrent value is implicit X · Tab or Shift+Tab skips it without writing"));
        }
        if (busEditScope_ == BusEditScope::Beat) {
            busValueEdit_->setToolTip(
                busValueEdit_->toolTip()
                + tr("\nPaste comma-, semicolon-, space-, or line-separated values to fill consecutive beats with one Apply and one Undo")
                + tr("\nUse value*N to repeat one value, for example 0x00*8 or IDLE*4"));
        } else {
            busValueEdit_->setToolTip(
                busValueEdit_->toolTip()
                + tr("\nValue sequences apply to Beat scope; switch from Segment before pasting a list"));
        }
        if (!enumLane) {
            busValueEdit_->setToolTip(
                busValueEdit_->toolTip()
                + tr("\nUp/Down adjusts a known numeric draft by one"));
        }
        busValueEdit_->setToolTip(
            busValueEdit_->toolTip()
            + tr("\nCtrl+Up/Down cycles this signal's recent values")
            + tr("\nCtrl+Enter applies and keeps the current target open")
            + tr("\nClicking another target applies this draft and continues there"));
        busValueEdit_->setStyleSheet({});
    }
    positionBusPresetPalette();
    updateBusEditActionStates();
    positionBusPresetPalette();
    viewport()->update();
}

void WaveCanvas::hideBusPresetPalette()
{
    const auto restoreCanvasFocus = busValueEdit_ && busValueEdit_->hasFocus();
    busEditActionPreview_.reset();
    busEditPreviewAction_.reset();
    busPresetLaneId_.clear();
    busPresetAnchorTick_.reset();
    busEditRange_.reset();
    busEditScope_ = BusEditScope::Beat;
    if (busValueEdit_) busValueEdit_->setModified(false);
    if (busEditPaletteVisible_) {
        busEditPaletteVisible_ = false;
        emit busEditPaletteVisibilityChanged(false);
    }
    if (busPresetPalette_) busPresetPalette_->hide();
    if (restoreCanvasFocus) viewport()->setFocus(Qt::OtherFocusReason);
}

bool WaveCanvas::editSelectedSegmentValue(const QString& seed)
{
    if (!scenario_ || selectedSegmentLaneId_.empty()
        || selectedSegmentId_.empty()) {
        return false;
    }
    const auto* lane = findLane(*scenario_, selectedSegmentLaneId_);
    const auto* segment = segmentById(
        selectedSegmentLaneId_,
        selectedSegmentId_);
    if (!lane || !segment
        || (lane->kind != LaneKind::Bus && lane->kind != LaneKind::Enum)) {
        return false;
    }

    selectedLaneId_ = lane->id;
    selectedLaneIds_ = {lane->id};
    laneHeaderSelectionActive_ = false;
    const auto focusTick = cursorTick_ >= segment->start
            && cursorTick_ < segment->end
        ? cursorTick_
        : segment->start;
    cursorTick_ = focusTick;
    showBusPresetPalette(
        *lane,
        QPoint(xAtTick(focusTick), 0),
        focusTick,
        std::pair{segment->start, segment->end},
        BusEditScope::Segment);
    if (busValueEdit_) {
        if (!seed.isNull()) {
            busValueEdit_->setText(seed);
            busValueEdit_->setModified(true);
            updateBusEditActionStates(true);
        }
        busValueEdit_->setFocus(Qt::OtherFocusReason);
        busValueEdit_->selectAll();
        const auto laneId = lane->id;
        QTimer::singleShot(0, busValueEdit_, [this, laneId] {
            if (busPresetLaneId_ == laneId
                && busEditScope_ == BusEditScope::Segment
                && busValueEdit_) {
                busValueEdit_->setFocus(Qt::OtherFocusReason);
                busValueEdit_->selectAll();
            }
        });
    }
    ensureLaneVisible(lane->id);
    ensureCursorVisible(focusTick);
    emit selectionChanged(
        QString::fromStdString(lane->id),
        focusTick);
    emit statusMessage(
        tr("Edit %1 Segment · %2–%3 · current value %4 · Enter applies · Esc cancels")
            .arg(QString::fromStdString(lane->name))
            .arg(QString::fromStdString(formatTick(
                segment->start,
                project_->timeBase)))
            .arg(QString::fromStdString(formatTick(
                segment->end,
                project_->timeBase)))
            .arg(QString::fromStdString(segment->value)));
    viewport()->update();
    return true;
}

bool WaveCanvas::advanceBusValueEdit(
    const std::string& laneId,
    const std::pair<Tick, Tick>& currentRange,
    const bool forward)
{
    if (!scenario_) return false;
    const auto* lane = findLane(*scenario_, laneId);
    if (!lane
        || !isLaneDisplayed(*lane)
        || (lane->kind != LaneKind::Bus && lane->kind != LaneKind::Enum)) {
        return false;
    }
    const auto target = adjacentEditableBeatRange(*lane, currentRange, forward);
    if (!target) return false;

    showBusPresetPalette(
        *lane,
        QPoint(xAtTick(target->first), 0),
        target->first,
        *target,
        BusEditScope::Beat);
    ensureCursorVisible(forward ? target->second : target->first);
    emit selectionChanged(QString::fromStdString(laneId), cursorTick_);
    if (busValueEdit_) {
        busValueEdit_->setFocus(Qt::TabFocusReason);
        busValueEdit_->selectAll();
    }
    viewport()->update();
    return true;
}

bool WaveCanvas::advanceBusSegmentValueEdit(
    const std::string& laneId,
    const std::pair<Tick, Tick>& currentRange,
    const bool forward)
{
    if (!scenario_) return false;
    const auto* lane = findLane(*scenario_, laneId);
    if (!lane
        || !isLaneDisplayed(*lane)
        || (lane->kind != LaneKind::Bus && lane->kind != LaneKind::Enum)) {
        return false;
    }
    const auto current = std::find_if(
        lane->segments.begin(),
        lane->segments.end(),
        [currentRange](const Segment& segment) {
            return segment.start == currentRange.first
                && segment.end == currentRange.second;
        });
    if (current == lane->segments.end()) return false;
    const auto target = forward
        ? std::next(current)
        : current == lane->segments.begin()
            ? lane->segments.end()
            : std::prev(current);
    if (target == lane->segments.end()) return false;

    selectedSegmentLaneId_ = lane->id;
    selectedSegmentId_ = target->id;
    showBusPresetPalette(
        *lane,
        QPoint(xAtTick(target->start), 0),
        target->start,
        std::pair{target->start, target->end},
        BusEditScope::Segment);
    ensureCursorVisible(forward ? target->end : target->start);
    emit selectionChanged(QString::fromStdString(laneId), cursorTick_);
    if (busValueEdit_) {
        busValueEdit_->setFocus(Qt::TabFocusReason);
        busValueEdit_->selectAll();
    }
    viewport()->update();
    return true;
}

bool WaveCanvas::cycleEnumEditorSymbol(const bool forward)
{
    if (!scenario_ || !busValueEdit_ || busPresetLaneId_.empty()) return false;
    const auto* lane = findLane(*scenario_, busPresetLaneId_);
    if (!lane || lane->kind != LaneKind::Enum) return false;

    QStringList symbols;
    for (const auto& [symbol, value] : lane->enumMap) {
        Q_UNUSED(value);
        symbols.append(QString::fromStdString(symbol));
    }
    if (symbols.isEmpty()) {
        emit statusMessage(
            tr("%1 · no declared Enum symbols · draft unchanged")
                .arg(QString::fromStdString(lane->name)));
        return true;
    }

    const auto current = busValueEdit_->text().trimmed();
    auto index = symbols.indexOf(current);
    if (index < 0) {
        for (auto candidate = 0; candidate < symbols.size(); ++candidate) {
            if (symbols.at(candidate).compare(
                    current,
                    Qt::CaseInsensitive)
                == 0) {
                index = candidate;
                break;
            }
        }
    }
    if (index < 0) {
        index = forward ? 0 : symbols.size() - 1;
    } else if (forward) {
        index = (index + 1) % symbols.size();
    } else {
        index = (index + symbols.size() - 1) % symbols.size();
    }

    const auto symbol = symbols.at(index);
    busValueEdit_->setText(symbol);
    busValueEdit_->setModified(true);
    busValueEdit_->setStyleSheet({});
    busValueEdit_->setFocus(Qt::OtherFocusReason);
    busValueEdit_->selectAll();
    updateBusEditActionStates();
    emit statusMessage(
        tr("%1 · Enum symbol %2 of %3: %4 · Up/Down cycles · Enter applies")
            .arg(QString::fromStdString(lane->name))
            .arg(index + 1)
            .arg(symbols.size())
            .arg(symbol));
    return true;
}

bool WaveCanvas::stepBusEditorValue(const bool upward)
{
    if (!scenario_ || !busValueEdit_ || busPresetLaneId_.empty()) return false;
    const auto* lane = findLane(*scenario_, busPresetLaneId_);
    if (!lane || lane->kind != LaneKind::Bus) return false;

    const auto validation = validateLaneValue(
        *lane,
        busEditorValue(*lane).toStdString());
    const auto bits = validation.valid
        ? laneValueBits(*lane, validation.normalizedValue)
        : std::nullopt;
    if (!bits
        || bits->empty()
        || std::any_of(
            bits->begin(),
            bits->end(),
            [](const char bit) { return bit != '0' && bit != '1'; })) {
        busValueEdit_->setFocus(Qt::OtherFocusReason);
        busValueEdit_->selectAll();
        emit statusMessage(
            tr("%1 · numeric step needs a known 0/1 value · draft unchanged")
                .arg(QString::fromStdString(lane->name)));
        return true;
    }

    auto steppedBits = *bits;
    const auto allAfterSign = [&steppedBits](const char expected) {
        return std::all_of(
            std::next(steppedBits.begin()),
            steppedBits.end(),
            [expected](const char bit) { return bit == expected; });
    };
    const auto atMaximum = lane->isSigned
        ? steppedBits.front() == '0' && allAfterSign('1')
        : std::all_of(
              steppedBits.begin(),
              steppedBits.end(),
              [](const char bit) { return bit == '1'; });
    const auto atMinimum = lane->isSigned
        ? steppedBits.front() == '1' && allAfterSign('0')
        : std::all_of(
              steppedBits.begin(),
              steppedBits.end(),
              [](const char bit) { return bit == '0'; });
    if ((upward && atMaximum) || (!upward && atMinimum)) {
        busValueEdit_->setFocus(Qt::OtherFocusReason);
        busValueEdit_->selectAll();
        emit statusMessage(
            tr("%1 · draft already at %2 · no values changed")
                .arg(QString::fromStdString(lane->name))
                .arg(upward ? tr("maximum") : tr("minimum")));
        return true;
    }

    if (upward) {
        for (auto iterator = steppedBits.rbegin();
             iterator != steppedBits.rend();
             ++iterator) {
            if (*iterator == '0') {
                *iterator = '1';
                break;
            }
            *iterator = '0';
        }
    } else {
        for (auto iterator = steppedBits.rbegin();
             iterator != steppedBits.rend();
             ++iterator) {
            if (*iterator == '1') {
                *iterator = '0';
                break;
            }
            *iterator = '1';
        }
    }

    const auto unsignedDecimal = [](const std::string& binary) {
        std::string decimal{"0"};
        for (const auto bit : binary) {
            auto carry = bit == '1' ? 1 : 0;
            for (auto iterator = decimal.rbegin();
                 iterator != decimal.rend();
                 ++iterator) {
                const auto value = (*iterator - '0') * 2 + carry;
                *iterator = static_cast<char>('0' + value % 10);
                carry = value / 10;
            }
            if (carry > 0) {
                decimal.insert(decimal.begin(), static_cast<char>('0' + carry));
            }
        }
        return decimal;
    };
    const auto compactDigits = [](std::string digits) {
        const auto first = digits.find_first_not_of('0');
        return first == std::string::npos
            ? std::string{"0"}
            : digits.substr(first);
    };
    const auto groupedDigits = [&compactDigits](
                                   const std::string& binary,
                                   const int groupWidth,
                                   const std::string_view alphabet) {
        auto padded = binary;
        const auto remainder = padded.size() % static_cast<std::size_t>(groupWidth);
        if (remainder != 0) {
            padded.insert(
                padded.begin(),
                static_cast<std::ptrdiff_t>(
                    static_cast<std::size_t>(groupWidth) - remainder),
                '0');
        }
        std::string digits;
        digits.reserve(padded.size() / static_cast<std::size_t>(groupWidth));
        for (std::size_t index = 0; index < padded.size();
             index += static_cast<std::size_t>(groupWidth)) {
            unsigned value = 0;
            for (int offset = 0; offset < groupWidth; ++offset) {
                value = value * 2
                    + static_cast<unsigned>(
                        padded.at(index + static_cast<std::size_t>(offset)) - '0');
            }
            digits.push_back(alphabet.at(value));
        }
        return compactDigits(std::move(digits));
    };

    const auto radix = busRadixCombo_
        ? static_cast<Radix>(busRadixCombo_->currentData().toInt())
        : lane->radix;
    std::string display;
    switch (radix) {
    case Radix::Binary:
        display = "0b" + steppedBits;
        break;
    case Radix::Octal:
        display = "0o"
            + groupedDigits(steppedBits, 3, "01234567");
        break;
    case Radix::Hexadecimal:
        display = "0x"
            + groupedDigits(steppedBits, 4, "0123456789abcdef");
        break;
    case Radix::Decimal: {
        auto magnitude = steppedBits;
        const auto negative = lane->isSigned && magnitude.front() == '1';
        if (negative) {
            for (auto& bit : magnitude) bit = bit == '0' ? '1' : '0';
            for (auto iterator = magnitude.rbegin();
                 iterator != magnitude.rend();
                 ++iterator) {
                if (*iterator == '0') {
                    *iterator = '1';
                    break;
                }
                *iterator = '0';
            }
        }
        display = (negative ? "-" : "") + unsignedDecimal(magnitude);
        break;
    }
    }

    busValueEdit_->setText(QString::fromStdString(display));
    busValueEdit_->setModified(true);
    busValueEdit_->setStyleSheet({});
    busValueEdit_->setFocus(Qt::OtherFocusReason);
    busValueEdit_->selectAll();
    updateBusEditActionStates();
    emit statusMessage(
        tr("%1 · draft %2 · Up/Down adjusts · Tab applies and advances")
            .arg(QString::fromStdString(lane->name))
            .arg(QString::fromStdString(display)));
    return true;
}

bool WaveCanvas::cycleBusRecentValue(const bool forward)
{
    if (!scenario_ || !busValueEdit_ || busPresetLaneId_.empty()) return false;
    const auto* lane = findLane(*scenario_, busPresetLaneId_);
    if (!lane
        || (lane->kind != LaneKind::Bus && lane->kind != LaneKind::Enum)) {
        return false;
    }
    const auto recent = busRecentValues_.find(lane->id);
    if (recent == busRecentValues_.end() || recent->second.isEmpty()) {
        busValueEdit_->setFocus(Qt::OtherFocusReason);
        busValueEdit_->selectAll();
        emit statusMessage(
            tr("%1 · no recent values yet · enter and apply a value first")
                .arg(QString::fromStdString(lane->name)));
        return true;
    }

    const auto& values = recent->second;
    auto index = values.indexOf(busValueEdit_->text().trimmed());
    if (index < 0) {
        index = 0;
    } else if (forward) {
        index = (index + 1) % values.size();
    } else {
        index = (index + values.size() - 1) % values.size();
    }
    const auto value = values.at(index);
    busValueEdit_->setText(value);
    busValueEdit_->setModified(true);
    busValueEdit_->setStyleSheet({});
    busValueEdit_->setFocus(Qt::OtherFocusReason);
    busValueEdit_->selectAll();
    if (busRecentValuesCombo_) {
        busRecentValuesCombo_->setCurrentIndex(index + 1);
    }
    updateBusEditActionStates();
    emit statusMessage(
        tr("%1 · recent value %2 of %3: %4 · Ctrl+Up/Down cycles")
            .arg(QString::fromStdString(lane->name))
            .arg(index + 1)
            .arg(values.size())
            .arg(value));
    return true;
}

void WaveCanvas::rememberBusValue(
    const std::string& laneId,
    const QString& value)
{
    const auto normalized = value.trimmed();
    if (laneId.empty() || normalized.isEmpty()) return;
    auto& recent = busRecentValues_[laneId];
    recent.removeAll(normalized);
    recent.prepend(normalized);
    while (recent.size() > 8) recent.removeLast();
    if (busRecentValuesCombo_ && busPresetLaneId_ == laneId) {
        busRecentValuesCombo_->clear();
        busRecentValuesCombo_->addItem(tr("Recent"));
        busRecentValuesCombo_->addItems(recent);
        busRecentValuesCombo_->setCurrentIndex(0);
        busRecentValuesCombo_->setEnabled(true);
    }
}

QString WaveCanvas::busEditorValue(const Lane& lane) const
{
    if (!busValueEdit_) return {};
    return busEditorValue(lane, busValueEdit_->text());
}

QString WaveCanvas::busEditorValue(
    const Lane& lane,
    const QString& entered) const
{
    auto value = entered.trimmed();
    if (lane.kind != LaneKind::Bus || value.isEmpty()) return value;
    const auto lower = value.toLower();
    if (lower == QStringLiteral("x")) {
        return QString::fromStdString(
            busPresetValue("x", lane.width));
    }
    if (lower == QStringLiteral("z")) {
        return QString::fromStdString(
            busPresetValue("z", lane.width));
    }
    if (lower.startsWith(QStringLiteral("0b"))
        || lower.startsWith(QStringLiteral("0o"))
        || lower.startsWith(QStringLiteral("0x"))) {
        return value;
    }
    const auto radix = busRadixCombo_
        ? static_cast<Radix>(busRadixCombo_->currentData().toInt())
        : lane.radix;
    switch (radix) {
    case Radix::Binary:
        return QStringLiteral("0b") + value;
    case Radix::Octal:
        return QStringLiteral("0o") + value;
    case Radix::Hexadecimal:
        return QStringLiteral("0x") + value;
    case Radix::Decimal:
        return value;
    }
    return value;
}

WaveCanvas::BusDraftValues
WaveCanvas::parseBusDraftValues(const Lane& lane) const
{
    if (!busValueEdit_) {
        BusDraftValues draft;
        draft.error = tr("value editor is unavailable");
        return draft;
    }
    return parseBusDraftValues(
        lane,
        busValueEdit_->text(),
        true);
}

WaveCanvas::BusDraftValues
WaveCanvas::parseBusDraftValues(
    const Lane& lane,
    const QString& enteredText,
    const bool useEditorRadix) const
{
    BusDraftValues draft;
    const auto entered = enteredText.trimmed();
    if (entered.isEmpty()) {
        draft.error = lane.kind == LaneKind::Enum
            ? tr("enter an Enum symbol or numeric value")
            : tr("enter a Bus value");
        return draft;
    }
    const auto editorValue =
        [this, &lane, useEditorRadix](const QString& value) {
            return useEditorRadix
                ? busEditorValue(lane, value)
                : value.trimmed();
        };
    if (lane.kind == LaneKind::Enum) {
        const auto wholeValue =
            editorValue(entered);
        const auto wholeValidation =
            validateLaneValue(
                lane,
                wholeValue.toStdString());
        if (wholeValidation.valid) {
            const auto textStart =
                enteredText.indexOf(entered);
            draft.valid = true;
            draft.valueCount = 1;
            draft.normalizedValues.push_back(
                wholeValidation.normalizedValue);
            draft.tokens.push_back({
                static_cast<int>(
                    std::max<qsizetype>(
                        0,
                        textStart)),
                static_cast<int>(entered.size()),
                0,
                1,
                entered,
            });
            return draft;
        }
    }

    static const QRegularExpression tokenExpression(
        QStringLiteral("[^,;\\s]+"));
    struct EnteredToken {
        int textStart{0};
        int textLength{0};
        QString text;
    };
    std::vector<EnteredToken> values;
    auto matches =
        tokenExpression.globalMatch(enteredText);
    while (matches.hasNext()) {
        const auto match = matches.next();
        values.push_back({
            static_cast<int>(match.capturedStart()),
            static_cast<int>(match.capturedLength()),
            match.captured(),
        });
    }
    draft.sequence =
        values.size() > 1
        || entered.contains(QLatin1Char('*'));
    draft.valueCount =
        static_cast<std::size_t>(values.size());
    if (values.empty()) {
        draft.error = lane.kind == LaneKind::Enum
            ? tr("enter an Enum symbol or numeric value")
            : tr("enter a Bus value");
        return draft;
    }
    constexpr std::size_t MaximumSequenceValues = 1024;
    if (values.size() > MaximumSequenceValues) {
        draft.error =
            tr("sequence contains %1 values; the live editor limit is %2")
                .arg(static_cast<qulonglong>(
                    values.size()))
                .arg(static_cast<qulonglong>(
                    MaximumSequenceValues));
        return draft;
    }

    draft.normalizedValues.reserve(
        values.size());
    draft.tokens.reserve(values.size());
    for (std::size_t index = 0; index < values.size(); ++index) {
        const auto& enteredToken =
            values.at(index);
        const auto& enteredValue =
            enteredToken.text;
        auto candidate =
            editorValue(enteredValue);
        auto validation =
            validateLaneValue(lane, candidate.toStdString());
        std::size_t repeatCount = 1;
        if (!validation.valid
            && enteredValue.contains(QLatin1Char('*'))) {
            const auto marker =
                enteredValue.lastIndexOf(QLatin1Char('*'));
            const auto repeatedValue =
                enteredValue.left(marker);
            const auto countText =
                enteredValue.mid(marker + 1);
            if (marker <= 0) {
                draft.error =
                    tr("value %1 (%2): enter a value before *N")
                        .arg(index + 1)
                        .arg(enteredValue);
                return draft;
            }
            if (repeatedValue.contains(QLatin1Char('*'))) {
                draft.error =
                    tr("value %1 (%2): use one trailing *N repeat")
                        .arg(index + 1)
                        .arg(enteredValue);
                return draft;
            }
            const auto isAsciiDigit = [](const QChar character) {
                return character >= QLatin1Char('0')
                    && character <= QLatin1Char('9');
            };
            if (countText.isEmpty()
                || !std::all_of(
                    countText.begin(),
                    countText.end(),
                    isAsciiDigit)) {
                draft.error =
                    tr("value %1 (%2): repeat must use a positive decimal count such as 0x00*8")
                        .arg(index + 1)
                        .arg(enteredValue);
                return draft;
            }
            repeatCount = 0;
            for (const auto character : countText) {
                const auto digit =
                    static_cast<std::size_t>(
                        character.unicode()
                        - QLatin1Char('0').unicode());
                if (repeatCount
                    > (MaximumSequenceValues
                       - digit)
                        / 10) {
                    draft.error =
                        tr("sequence expands beyond the live editor limit of %1 values at value %2 (%3)")
                            .arg(static_cast<qulonglong>(
                                MaximumSequenceValues))
                            .arg(index + 1)
                            .arg(enteredValue);
                    return draft;
                }
                repeatCount = repeatCount * 10 + digit;
            }
            if (repeatCount == 0) {
                draft.error =
                    tr("value %1 (%2): repeat count must be greater than zero")
                        .arg(index + 1)
                        .arg(enteredValue);
                return draft;
            }
            candidate =
                editorValue(repeatedValue);
            validation =
                validateLaneValue(
                    lane,
                    candidate.toStdString());
        }
        if (!validation.valid) {
            draft.error =
                tr("value %1 (%2): %3")
                    .arg(index + 1)
                    .arg(enteredValue)
                    .arg(QString::fromStdString(validation.error));
            return draft;
        }
        if (repeatCount
            > MaximumSequenceValues
                - draft.normalizedValues.size()) {
            draft.error =
                tr("sequence expands beyond the live editor limit of %1 values at value %2 (%3)")
                    .arg(static_cast<qulonglong>(
                        MaximumSequenceValues))
                    .arg(index + 1)
                    .arg(enteredValue);
            return draft;
        }
        const auto expandedOffset =
            draft.normalizedValues.size();
        draft.normalizedValues.insert(
            draft.normalizedValues.end(),
            repeatCount,
            validation.normalizedValue);
        draft.tokens.push_back({
            enteredToken.textStart,
            enteredToken.textLength,
            expandedOffset,
            repeatCount,
            enteredValue,
        });
    }
    draft.valueCount = draft.normalizedValues.size();
    draft.sequence =
        draft.sequence
        || draft.normalizedValues.size() > 1;
    draft.valid = true;
    return draft;
}

WaveCanvas::BitPatternDraft
WaveCanvas::parseBitPatternDraft(const QString& entered) const
{
    BitPatternDraft draft;
    auto text = entered.trimmed();
    if (text.startsWith(QStringLiteral("0b"), Qt::CaseInsensitive)) {
        text.remove(0, 2);
    }
    if (text.isEmpty()) {
        draft.error = tr("enter a 0/1/X/Z pattern");
        return draft;
    }

    constexpr std::size_t MaximumPatternSymbols = 1024;
    draft.values.reserve(
        std::min<std::size_t>(
            static_cast<std::size_t>(text.size()),
            MaximumPatternSymbols));
    for (qsizetype index = 0; index < text.size(); ++index) {
        const auto character = text.at(index);
        const auto normalized = character.toUpper();
        if (normalized == QLatin1Char('0')
            || normalized == QLatin1Char('1')
            || normalized == QLatin1Char('X')
            || normalized == QLatin1Char('Z')) {
            std::size_t repeatCount = 1;
            if (index + 1 < text.size()
                && text.at(index + 1) == QLatin1Char('*')) {
                auto countIndex = index + 2;
                const auto isAsciiDigit = [](const QChar candidate) {
                    return candidate >= QLatin1Char('0')
                        && candidate <= QLatin1Char('9');
                };
                if (countIndex >= text.size()
                    || !isAsciiDigit(text.at(countIndex))) {
                    draft.values.clear();
                    draft.error =
                        tr("repeat after character %1 must use a positive decimal count such as 0*8")
                            .arg(index + 1);
                    return draft;
                }
                repeatCount = 0;
                while (countIndex < text.size()
                       && isAsciiDigit(text.at(countIndex))) {
                    const auto digit =
                        static_cast<std::size_t>(
                            text.at(countIndex).unicode()
                            - QLatin1Char('0').unicode());
                    if (repeatCount
                        > (MaximumPatternSymbols - digit) / 10) {
                        draft.values.clear();
                        draft.error =
                            tr("repeat at character %1 expands the pattern beyond the %2-symbol limit")
                                .arg(index + 1)
                                .arg(MaximumPatternSymbols);
                        return draft;
                    }
                    repeatCount = repeatCount * 10 + digit;
                    ++countIndex;
                }
                if (repeatCount == 0) {
                    draft.values.clear();
                    draft.error =
                        tr("repeat count after character %1 must be greater than zero")
                            .arg(index + 1);
                    return draft;
                }
                if (repeatCount
                    > MaximumPatternSymbols - draft.values.size()) {
                    draft.values.clear();
                    draft.error =
                        tr("repeat at character %1 expands the pattern beyond the %2-symbol limit")
                            .arg(index + 1)
                            .arg(MaximumPatternSymbols);
                    return draft;
                }
                index = countIndex - 1;
            } else if (draft.values.size()
                       >= MaximumPatternSymbols) {
                draft.values.clear();
                draft.error =
                    tr("pattern contains more than %1 symbols")
                        .arg(MaximumPatternSymbols);
                return draft;
            }
            const auto value =
                QString(normalized).toStdString();
            draft.values.insert(
                draft.values.end(),
                repeatCount,
                value);
            continue;
        }
        if (character.isSpace()
            || character == QLatin1Char('_')
            || character == QLatin1Char(',')
            || character == QLatin1Char(';')
            || character == QLatin1Char('|')
            || character == QLatin1Char('\'')) {
            continue;
        }
        draft.values.clear();
        draft.error =
            tr("character %1 (%2) is not 0, 1, X, Z, or a separator")
                .arg(index + 1)
                .arg(character);
        return draft;
    }
    if (draft.values.empty()) {
        draft.error = tr("enter at least one 0/1/X/Z symbol");
        return draft;
    }
    draft.valid = true;
    return draft;
}

WaveCanvas::BitPatternAssessment
WaveCanvas::assessBitPatternDraft(const QString& entered) const
{
    BitPatternAssessment assessment;
    assessment.summary =
        tr("Bit pattern unavailable · select one or more Bit signals over complete beats");
    if (!scenario_ || !project_ || !explicitRangeSelection_
        || !selectionRange_
        || selectionRange_->second <= selectionRange_->first
        || selectedLaneIds_.empty()) {
        return assessment;
    }
    const auto kind = explicitRangeKind();
    if (!kind || *kind != LaneKind::Bit) return assessment;

    std::vector<const Lane*> lanes;
    lanes.reserve(selectedLaneIds_.size());
    for (const auto& laneId : selectedLaneIds_) {
        const auto* lane = findLane(*scenario_, laneId);
        if (!lane || lane->kind != LaneKind::Bit) {
            assessment.summary =
                tr("Bit pattern unavailable · a selected signal no longer exists");
            return assessment;
        }
        lanes.push_back(lane);
    }
    assessment.applicable = true;

    const auto enteredPatterns =
        entered.split(
            QLatin1Char('/'),
            Qt::KeepEmptyParts);
    const auto sharedPattern =
        enteredPatterns.size() == 1;
    if (!sharedPattern
        && enteredPatterns.size()
            != static_cast<qsizetype>(lanes.size())) {
        assessment.summary =
            tr("Bit pattern unavailable · entered %1 patterns for %2 selected Bit signals; use one shared pattern or exactly one /-separated pattern per signal in top-to-bottom order")
                .arg(enteredPatterns.size())
                .arg(static_cast<qulonglong>(lanes.size()));
        return assessment;
    }

    std::vector<BitPatternDraft> laneDrafts;
    laneDrafts.reserve(lanes.size());
    if (sharedPattern) {
        const auto draft =
            parseBitPatternDraft(enteredPatterns.front());
        if (!draft.valid) {
            assessment.summary =
                tr("Bit pattern unavailable · %1")
                    .arg(draft.error);
            return assessment;
        }
        laneDrafts.assign(lanes.size(), draft);
    } else {
        for (std::size_t laneIndex = 0;
             laneIndex < lanes.size();
             ++laneIndex) {
            auto draft =
                parseBitPatternDraft(
                    enteredPatterns.at(
                        static_cast<qsizetype>(
                            laneIndex)));
            if (!draft.valid) {
                assessment.summary =
                    tr("Bit pattern unavailable · pattern for %1: %2")
                        .arg(
                            QString::fromStdString(
                                lanes.at(laneIndex)->name),
                            draft.error);
                return assessment;
            }
            laneDrafts.push_back(std::move(draft));
        }
    }

    const auto [start, end] = *selectionRange_;
    std::vector<std::pair<Tick, Tick>> beats;
    for (std::size_t laneIndex = 0;
         laneIndex < lanes.size();
         ++laneIndex) {
        const auto laneBeats =
            editableBeatRangesBetween(
                start,
                end - 1,
                *lanes.at(laneIndex));
        if (laneBeats.empty()
            || laneBeats.front().first != start
            || laneBeats.back().second != end) {
            assessment.summary =
                tr("Bit pattern unavailable · %1 does not cover complete beats over this range")
                    .arg(QString::fromStdString(
                        lanes.at(laneIndex)->name));
            return assessment;
        }
        if (laneIndex == 0) {
            beats = laneBeats;
        } else if (laneBeats != beats) {
            assessment.summary =
                tr("Bit pattern unavailable · %1 and %2 use different beat grids; select signals sharing one clock grid")
                    .arg(
                        QString::fromStdString(
                            lanes.front()->name),
                        QString::fromStdString(
                            lanes.at(laneIndex)->name));
            return assessment;
        }
    }
    constexpr std::size_t MaximumPatternBeats = 1024;
    if (beats.size() > MaximumPatternBeats) {
        assessment.summary =
            tr("Bit pattern unavailable · the selected range contains %1 beats; the live pattern limit is %2")
                .arg(static_cast<qulonglong>(beats.size()))
                .arg(MaximumPatternBeats);
        return assessment;
    }
    constexpr std::size_t MaximumPatternSignalBeats = 4096;
    if (beats.size() > 0
        && lanes.size()
            > MaximumPatternSignalBeats / beats.size()) {
        assessment.summary =
            tr("Bit pattern unavailable · %1 signals × %2 beats exceeds the live batch limit of %3 signal-beats")
                .arg(static_cast<qulonglong>(lanes.size()))
                .arg(static_cast<qulonglong>(beats.size()))
                .arg(MaximumPatternSignalBeats);
        return assessment;
    }
    for (std::size_t laneIndex = 0;
         laneIndex < laneDrafts.size();
         ++laneIndex) {
        const auto& draft = laneDrafts.at(laneIndex);
        if (draft.values.size() > beats.size()) {
            assessment.summary =
                sharedPattern
                ? tr("Bit pattern unavailable · %1 symbols do not fit in %2 selected beats")
                      .arg(static_cast<qulonglong>(
                          draft.values.size()))
                      .arg(static_cast<qulonglong>(
                          beats.size()))
                : tr("Bit pattern unavailable · pattern for %1 has %2 symbols but the range has %3 beats")
                      .arg(QString::fromStdString(
                          lanes.at(laneIndex)->name))
                      .arg(static_cast<qulonglong>(
                          draft.values.size()))
                      .arg(static_cast<qulonglong>(
                          beats.size()));
            return assessment;
        }
        if (beats.size() % draft.values.size() != 0) {
            assessment.summary =
                sharedPattern
                ? tr("Bit pattern unavailable · %1 selected beats are not an exact multiple of the %2-symbol pattern")
                      .arg(static_cast<qulonglong>(
                          beats.size()))
                      .arg(static_cast<qulonglong>(
                          draft.values.size()))
                : tr("Bit pattern unavailable · %1: %2 selected beats are not an exact multiple of its %3-symbol pattern")
                      .arg(QString::fromStdString(
                          lanes.at(laneIndex)->name))
                      .arg(static_cast<qulonglong>(
                          beats.size()))
                      .arg(static_cast<qulonglong>(
                          draft.values.size()));
            return assessment;
        }
    }

    BitPatternProjection projection;
    projection.laneIds = selectedLaneIds_;
    projection.start = start;
    projection.end = end;
    projection.patternLength =
        laneDrafts.front().values.size();
    projection.beatCount = beats.size();
    projection.sharedPattern = sharedPattern;
    projection.beatWidth =
        beats.front().second - beats.front().first;
    if (!std::all_of(
            beats.begin(),
            beats.end(),
            [&projection](const auto& beat) {
                return beat.second - beat.first
                    == projection.beatWidth;
            })) {
        projection.beatWidth = 0;
    }

    std::unordered_set<std::string> reservedSegmentIds;
    for (const auto& existingLane : scenario_->lanes) {
        for (const auto& segment : existingLane.segments) {
            reservedSegmentIds.insert(segment.id);
        }
    }
    std::size_t previewSequence = 0;
    const auto nextPreviewSegmentId =
        [&reservedSegmentIds, &previewSequence] {
            std::string candidate;
            do {
                candidate =
                    std::string{"__bit_pattern_preview_segment_"}
                    + std::to_string(++previewSequence);
            } while (reservedSegmentIds.contains(candidate));
            reservedSegmentIds.insert(candidate);
            return candidate;
        };

    projection.lanes.reserve(lanes.size());
    projection.assignments.reserve(lanes.size());
    const auto compactPattern =
        [](const BitPatternDraft& draft) {
            QString pattern;
            const auto shown =
                std::min<std::size_t>(
                    draft.values.size(),
                    16);
            for (std::size_t index = 0;
                 index < shown;
                 ++index) {
                pattern.append(
                    QString::fromStdString(
                        draft.values.at(index)));
            }
            if (shown < draft.values.size()) {
                pattern.append(QChar(0x2026));
            }
            return pattern;
        };
    for (std::size_t laneIndex = 0;
         laneIndex < lanes.size();
         ++laneIndex) {
        const auto* lane = lanes.at(laneIndex);
        const auto& draft = laneDrafts.at(laneIndex);
        auto projectedLane = *lane;
        LaneSequenceAssignment assignment;
        assignment.laneId = lane->id;
        assignment.steps.reserve(beats.size());
        for (std::size_t index = 0;
             index < beats.size();
             ++index) {
            const auto& beat = beats.at(index);
            const auto& value =
                draft.values.at(
                    index % draft.values.size());
            assignment.steps.push_back({
                beat.first,
                beat.second,
                value,
                {},
            });
            if (projectedRangeAlreadyEquals(
                    projectedLane,
                    beat.first,
                    beat.second,
                    value,
                    {})) {
                continue;
            }
            auto nextId = nextPreviewSegmentId;
            setProjectedSegmentRange(
                projectedLane,
                beat.first,
                beat.second,
                value,
                {},
                {},
                nextId);
        }
        const auto laneChanges =
            !sameProjectedWaveform(projectedLane, *lane);
        projection.changedLaneCount +=
            laneChanges ? 1 : 0;
        projection.waveformChanges =
            projection.waveformChanges || laneChanges;
        projection.lanes.push_back(
            std::move(projectedLane));
        projection.assignments.push_back(
            std::move(assignment));
        projection.patternSummaries.append(
            tr("%1=%2×%3")
                .arg(
                    QString::fromStdString(
                        lane->name),
                    compactPattern(draft))
                .arg(static_cast<qulonglong>(
                    beats.size()
                    / draft.values.size())));
    }

    projection.relationImpact =
        relationRemovalImpactForProjectedLanes(
            projection.lanes);
    projection.modelChanges =
        projection.waveformChanges
        || !projection.relationImpact.ids.empty();

    const auto pattern =
        compactPattern(laneDrafts.front());
    const auto repeats =
        beats.size()
        / laneDrafts.front().values.size();
    const auto range =
        tr("%1–%2")
            .arg(
                QString::fromStdString(
                    formatTick(start, project_->timeBase)),
                QString::fromStdString(
                    formatTick(end, project_->timeBase)));
    assessment.valid = true;
    assessment.enabled = projection.modelChanges;
    if (sharedPattern) {
        assessment.summary =
            projection.modelChanges
            ? tr("Pattern %1 · %2 symbols repeat %3× across %4 beats on %5 Bit signal(s) · changes %6 · %7 · Enter applies as one Undo")
                  .arg(pattern)
                  .arg(static_cast<qulonglong>(
                      laneDrafts.front().values.size()))
                  .arg(static_cast<qulonglong>(
                      repeats))
                  .arg(static_cast<qulonglong>(
                      beats.size()))
                  .arg(static_cast<qulonglong>(
                      lanes.size()))
                  .arg(static_cast<qulonglong>(
                      projection.changedLaneCount))
                  .arg(range)
            : tr("Pattern %1 · all %2 beats on %3 selected Bit signal(s) already match · no command will run")
                  .arg(pattern)
                  .arg(static_cast<qulonglong>(
                      beats.size()))
                  .arg(static_cast<qulonglong>(
                      lanes.size()));
    } else {
        const auto mappings =
            projection.patternSummaries.join(
                QStringLiteral("; "));
        assessment.summary =
            projection.modelChanges
            ? tr("Patterns %1 · %2 beats on %3 Bit signals · changes %4 · %5 · Enter applies as one Undo")
                  .arg(mappings)
                  .arg(static_cast<qulonglong>(
                      beats.size()))
                  .arg(static_cast<qulonglong>(
                      lanes.size()))
                  .arg(static_cast<qulonglong>(
                      projection.changedLaneCount))
                  .arg(range)
            : tr("Patterns %1 · all %2 beats on %3 selected Bit signals already match · no command will run")
                  .arg(mappings)
                  .arg(static_cast<qulonglong>(
                      beats.size()))
                  .arg(static_cast<qulonglong>(
                      lanes.size()));
    }
    if (!projection.relationImpact.ids.empty()) {
        assessment.summary +=
            tr("\nWarning: applying this pattern removes %1 relation(s); Ctrl+Z restores waveform and relations")
                .arg(static_cast<qulonglong>(
                    projection.relationImpact.ids.size()));
        for (const auto& summary
             : projection.relationImpact.summaries) {
            assessment.summary +=
                tr("\nAffected: %1").arg(summary);
        }
    } else if (projection.modelChanges) {
        assessment.summary += tr("\nNo Relation will be removed");
    }
    assessment.projection = std::move(projection);
    return assessment;
}

WaveCanvas::BusRangeSequenceAssessment
WaveCanvas::assessBusRangeSequenceDraft(
    const QString& entered) const
{
    BusRangeSequenceAssessment assessment;
    assessment.summary =
        tr("Sequence unavailable \u00b7 select one or more Bus or Enum signals over complete beats");
    if (!scenario_ || !project_ || !explicitRangeSelection_
        || !selectionRange_
        || selectionRange_->second <= selectionRange_->first
        || selectedLaneIds_.empty()) {
        return assessment;
    }
    const auto kind = explicitRangeKind();
    if (!kind
        || (*kind != LaneKind::Bus
            && *kind != LaneKind::Enum)) {
        return assessment;
    }

    std::vector<const Lane*> lanes;
    lanes.reserve(selectedLaneIds_.size());
    for (const auto& laneId : selectedLaneIds_) {
        const auto* lane = findLane(*scenario_, laneId);
        if (!lane || lane->kind != *kind) {
            assessment.summary =
                tr("Sequence unavailable \u00b7 a selected signal no longer exists");
            return assessment;
        }
        lanes.push_back(lane);
    }
    assessment.applicable = true;

    struct EnteredSequence {
        QString text;
        int textStart{0};
    };
    std::vector<EnteredSequence> enteredSequences;
    int sequenceStart = 0;
    while (sequenceStart <= entered.size()) {
        const auto separator =
            entered.indexOf(
                QLatin1Char('/'),
                sequenceStart);
        const auto sequenceEnd =
            separator < 0
            ? static_cast<int>(entered.size())
            : separator;
        enteredSequences.push_back({
            entered.mid(
                sequenceStart,
                sequenceEnd - sequenceStart),
            sequenceStart,
        });
        if (separator < 0) break;
        sequenceStart = separator + 1;
    }
    const auto sharedSequence =
        enteredSequences.size() == 1;
    assessment.sequence = !sharedSequence;
    if (!sharedSequence
        && enteredSequences.size()
            != lanes.size()) {
        assessment.summary =
            tr("Sequence unavailable \u00b7 entered %1 sequences for %2 selected %3 signals; use one shared sequence or exactly one /-separated sequence per signal in top-to-bottom order")
                .arg(static_cast<qulonglong>(
                    enteredSequences.size()))
                .arg(static_cast<qulonglong>(lanes.size()))
                .arg(*kind == LaneKind::Bus ? tr("Bus") : tr("Enum"));
        return assessment;
    }

    std::vector<BusDraftValues> laneDrafts;
    laneDrafts.reserve(lanes.size());
    if (sharedSequence) {
        const auto rawValue = entered.trimmed().toStdString();
        const auto exactScalarForAnyLane =
            std::any_of(
                lanes.begin(),
                lanes.end(),
                [&rawValue](const Lane* lane) {
                    return validateLaneValue(
                               *lane,
                               rawValue)
                        .valid;
                });
        for (const auto* lane : lanes) {
            laneDrafts.push_back(
                parseBusDraftValues(
                    *lane,
                    enteredSequences.front().text,
                    false));
        }
        assessment.sequence =
            rangeSequenceBaselineMatchesContext()
            || (!exactScalarForAnyLane
            && std::any_of(
                laneDrafts.begin(),
                laneDrafts.end(),
                [](const BusDraftValues& draft) {
                    return draft.sequence;
                }));
        if (!assessment.sequence) {
            return assessment;
        }
        for (std::size_t laneIndex = 0;
             laneIndex < laneDrafts.size();
             ++laneIndex) {
            const auto& draft = laneDrafts.at(laneIndex);
            if (draft.valid) continue;
            assessment.summary =
                tr("Sequence unavailable \u00b7 value for %1: %2")
                    .arg(
                        QString::fromStdString(
                            lanes.at(laneIndex)->name),
                        draft.error);
            return assessment;
        }
    } else {
        for (std::size_t laneIndex = 0;
             laneIndex < lanes.size();
             ++laneIndex) {
            auto draft =
                parseBusDraftValues(
                    *lanes.at(laneIndex),
                    enteredSequences.at(
                        laneIndex).text,
                    false);
            if (!draft.valid) {
                assessment.summary =
                    tr("Sequence unavailable \u00b7 sequence for %1: %2")
                        .arg(
                            QString::fromStdString(
                                lanes.at(laneIndex)->name),
                            draft.error);
                return assessment;
            }
            laneDrafts.push_back(std::move(draft));
        }
    }

    const auto [start, end] = *selectionRange_;
    std::vector<std::pair<Tick, Tick>> beats;
    for (std::size_t laneIndex = 0;
         laneIndex < lanes.size();
         ++laneIndex) {
        const auto laneBeats =
            editableBeatRangesBetween(
                start,
                end - 1,
                *lanes.at(laneIndex));
        if (laneBeats.empty()
            || laneBeats.front().first != start
            || laneBeats.back().second != end) {
            assessment.summary =
                tr("Sequence unavailable \u00b7 %1 does not cover complete beats over this range")
                    .arg(QString::fromStdString(
                        lanes.at(laneIndex)->name));
            return assessment;
        }
        if (laneIndex == 0) {
            beats = laneBeats;
        } else if (laneBeats != beats) {
            assessment.summary =
                tr("Sequence unavailable \u00b7 %1 and %2 use different beat grids; select signals sharing one clock grid")
                    .arg(
                        QString::fromStdString(
                            lanes.front()->name),
                        QString::fromStdString(
                            lanes.at(laneIndex)->name));
            return assessment;
        }
    }

    constexpr std::size_t MaximumSequenceBeats = 1024;
    if (beats.size() > MaximumSequenceBeats) {
        assessment.summary =
            tr("Sequence unavailable \u00b7 the selected range contains %1 beats; the live sequence limit is %2")
                .arg(static_cast<qulonglong>(beats.size()))
                .arg(MaximumSequenceBeats);
        return assessment;
    }
    constexpr std::size_t MaximumSequenceSignalBeats = 4096;
    if (!beats.empty()
        && lanes.size()
            > MaximumSequenceSignalBeats / beats.size()) {
        assessment.summary =
            tr("Sequence unavailable \u00b7 %1 signals x %2 beats exceeds the live batch limit of %3 signal-beats")
                .arg(static_cast<qulonglong>(lanes.size()))
                .arg(static_cast<qulonglong>(beats.size()))
                .arg(MaximumSequenceSignalBeats);
        return assessment;
    }
    for (std::size_t laneIndex = 0;
         laneIndex < laneDrafts.size();
         ++laneIndex) {
        const auto& draft = laneDrafts.at(laneIndex);
        if (draft.normalizedValues.size() > beats.size()) {
            assessment.summary =
                sharedSequence
                ? tr("Sequence unavailable \u00b7 %1 values do not fit in %2 selected beats")
                      .arg(static_cast<qulonglong>(
                          draft.normalizedValues.size()))
                      .arg(static_cast<qulonglong>(
                          beats.size()))
                : tr("Sequence unavailable \u00b7 sequence for %1 has %2 values but the range has %3 beats")
                      .arg(QString::fromStdString(
                          lanes.at(laneIndex)->name))
                      .arg(static_cast<qulonglong>(
                          draft.normalizedValues.size()))
                      .arg(static_cast<qulonglong>(
                          beats.size()));
            return assessment;
        }
        if (draft.normalizedValues.empty()
            || beats.size()
                   % draft.normalizedValues.size()
                != 0) {
            assessment.summary =
                sharedSequence
                ? tr("Sequence unavailable \u00b7 %1 selected beats are not an exact multiple of the %2-value sequence")
                      .arg(static_cast<qulonglong>(
                          beats.size()))
                      .arg(static_cast<qulonglong>(
                          draft.normalizedValues.size()))
                : tr("Sequence unavailable \u00b7 %1: %2 selected beats are not an exact multiple of its %3-value sequence")
                      .arg(QString::fromStdString(
                          lanes.at(laneIndex)->name))
                      .arg(static_cast<qulonglong>(
                          beats.size()))
                      .arg(static_cast<qulonglong>(
                          draft.normalizedValues.size()));
            return assessment;
        }
    }

    const auto appendTokenTargets =
        [&beats, &lanes](
            RangeSequenceTextSpan& textSpan,
            const std::size_t laneIndex,
            const BusDraftValues& draft,
            const BusDraftToken& token) {
            if (draft.normalizedValues.empty()
                || token.expandedCount == 0
                || token.expandedOffset
                    >= draft.normalizedValues.size()
                || token.expandedCount
                    > draft.normalizedValues.size()
                        - token.expandedOffset) {
                return;
            }
            const auto patternLength =
                draft.normalizedValues.size();
            for (std::size_t patternStart = 0;
                 patternStart < beats.size();
                 patternStart += patternLength) {
                const auto firstBeat =
                    patternStart
                    + token.expandedOffset;
                const auto finalBeat =
                    firstBeat
                    + token.expandedCount
                    - 1;
                if (finalBeat >= beats.size()) {
                    return;
                }
                textSpan.targets.push_back({
                    lanes.at(laneIndex)->id,
                    beats.at(firstBeat).first,
                    beats.at(finalBeat).second,
                    firstBeat,
                    token.expandedCount,
                    beats.size(),
                });
            }
        };
    if (sharedSequence) {
        const auto& firstDraft =
            laneDrafts.front();
        assessment.textSpans.reserve(
            firstDraft.tokens.size());
        for (std::size_t tokenIndex = 0;
             tokenIndex < firstDraft.tokens.size();
             ++tokenIndex) {
            const auto& firstToken =
                firstDraft.tokens.at(tokenIndex);
            RangeSequenceTextSpan textSpan{
                enteredSequences.front().textStart
                    + firstToken.textStart,
                firstToken.textLength,
                firstToken.token,
                {},
            };
            for (std::size_t laneIndex = 0;
                 laneIndex < laneDrafts.size();
                 ++laneIndex) {
                const auto& laneDraft =
                    laneDrafts.at(laneIndex);
                if (tokenIndex
                    >= laneDraft.tokens.size()) {
                    continue;
                }
                const auto& laneToken =
                    laneDraft.tokens.at(tokenIndex);
                if (laneToken.textStart
                        != firstToken.textStart
                    || laneToken.textLength
                        != firstToken.textLength) {
                    continue;
                }
                appendTokenTargets(
                    textSpan,
                    laneIndex,
                    laneDraft,
                    laneToken);
            }
            if (!textSpan.targets.empty()) {
                assessment.textSpans.push_back(
                    std::move(textSpan));
            }
        }
    } else {
        for (std::size_t laneIndex = 0;
             laneIndex < laneDrafts.size();
             ++laneIndex) {
            const auto& draft =
                laneDrafts.at(laneIndex);
            const auto& enteredSequence =
                enteredSequences.at(laneIndex);
            for (const auto& token : draft.tokens) {
                RangeSequenceTextSpan textSpan{
                    enteredSequence.textStart
                        + token.textStart,
                    token.textLength,
                    token.token,
                    {},
                };
                appendTokenTargets(
                    textSpan,
                    laneIndex,
                    draft,
                    token);
                if (!textSpan.targets.empty()) {
                    assessment.textSpans.push_back(
                        std::move(textSpan));
                }
            }
        }
    }

    BusRangeSequenceProjection projection;
    projection.laneIds = selectedLaneIds_;
    projection.start = start;
    projection.end = end;
    projection.patternLength =
        laneDrafts.front().normalizedValues.size();
    projection.beatCount = beats.size();
    projection.sharedSequence = sharedSequence;
    projection.beatWidth =
        beats.front().second - beats.front().first;
    if (!std::all_of(
            beats.begin(),
            beats.end(),
            [&projection](const auto& beat) {
                return beat.second - beat.first
                    == projection.beatWidth;
            })) {
        projection.beatWidth = 0;
    }

    std::unordered_set<std::string> reservedSegmentIds;
    for (const auto& existingLane : scenario_->lanes) {
        for (const auto& segment : existingLane.segments) {
            reservedSegmentIds.insert(segment.id);
        }
    }
    std::size_t previewSequence = 0;
    const auto nextPreviewSegmentId =
        [&reservedSegmentIds, &previewSequence] {
            std::string candidate;
            do {
                candidate =
                    std::string{"__bus_range_sequence_preview_segment_"}
                    + std::to_string(++previewSequence);
            } while (reservedSegmentIds.contains(candidate));
            reservedSegmentIds.insert(candidate);
            return candidate;
        };
    const auto compactSequence =
        [](const BusDraftValues& draft) {
            QStringList values;
            const auto shown =
                std::min<std::size_t>(
                    draft.normalizedValues.size(),
                    6);
            for (std::size_t index = 0;
                 index < shown;
                 ++index) {
                values.append(
                    QString::fromStdString(
                        draft.normalizedValues.at(index)));
            }
            if (shown < draft.normalizedValues.size()) {
                values.append(QString(QChar(0x2026)));
            }
            return QStringLiteral("[%1]")
                .arg(values.join(QStringLiteral(", ")));
        };

    projection.lanes.reserve(lanes.size());
    projection.assignments.reserve(lanes.size());
    for (std::size_t laneIndex = 0;
         laneIndex < lanes.size();
         ++laneIndex) {
        const auto* lane = lanes.at(laneIndex);
        const auto& draft = laneDrafts.at(laneIndex);
        auto projectedLane = *lane;
        LaneSequenceAssignment assignment;
        assignment.laneId = lane->id;
        assignment.steps.reserve(beats.size());
        for (std::size_t beatIndex = 0;
             beatIndex < beats.size();
             ++beatIndex) {
            const auto& beat = beats.at(beatIndex);
            const auto& value =
                draft.normalizedValues.at(
                    beatIndex
                    % draft.normalizedValues.size());
            const auto preserveExisting =
                rangeSequenceBaselineMatchesContext()
                && laneIndex < rangeSequenceBaseline_->values.size()
                && beatIndex < rangeSequenceBaseline_->values.at(laneIndex).size()
                && rangeSequenceBaseline_->values.at(laneIndex).at(beatIndex)
                    == value;
            assignment.steps.push_back({
                beat.first,
                beat.second,
                value,
                {},
                preserveExisting,
            });
            if (preserveExisting) continue;
            if (projectedRangeAlreadyEquals(
                    projectedLane,
                    beat.first,
                    beat.second,
                    value,
                    {})) {
                continue;
            }
            auto nextId = nextPreviewSegmentId;
            setProjectedSegmentRange(
                projectedLane,
                beat.first,
                beat.second,
                value,
                {},
                {},
                nextId);
        }
        const auto laneChanges =
            !sameProjectedWaveform(projectedLane, *lane);
        projection.changedLaneCount +=
            laneChanges ? 1 : 0;
        projection.waveformChanges =
            projection.waveformChanges || laneChanges;
        projection.lanes.push_back(
            std::move(projectedLane));
        projection.assignments.push_back(
            std::move(assignment));
        projection.sequenceSummaries.append(
            tr("%1=%2 x%3")
                .arg(
                    QString::fromStdString(
                        lane->name),
                    compactSequence(draft))
                .arg(static_cast<qulonglong>(
                    beats.size()
                    / draft.normalizedValues.size())));
    }

    projection.relationImpact =
        relationRemovalImpactForProjectedLanes(
            projection.lanes);
    projection.modelChanges =
        projection.waveformChanges
        || !projection.relationImpact.ids.empty();

    const auto sequence =
        compactSequence(laneDrafts.front());
    const auto repeats =
        beats.size()
        / laneDrafts.front().normalizedValues.size();
    const auto range =
        tr("%1\u2013%2")
            .arg(
                QString::fromStdString(
                    formatTick(start, project_->timeBase)),
                QString::fromStdString(
                    formatTick(end, project_->timeBase)));
    const auto kindName =
        *kind == LaneKind::Bus ? tr("Bus") : tr("Enum");
    assessment.valid = true;
    assessment.enabled = projection.modelChanges;
    if (sharedSequence) {
        assessment.summary =
            projection.modelChanges
            ? tr("Sequence %1 \u00b7 %2 values repeat %3x across %4 beats on %5 %6 signal(s) \u00b7 changes %7 \u00b7 %8 \u00b7 Enter applies as one Undo")
                  .arg(sequence)
                  .arg(static_cast<qulonglong>(
                      laneDrafts.front().normalizedValues.size()))
                  .arg(static_cast<qulonglong>(repeats))
                  .arg(static_cast<qulonglong>(beats.size()))
                  .arg(static_cast<qulonglong>(lanes.size()))
                  .arg(kindName)
                  .arg(static_cast<qulonglong>(
                      projection.changedLaneCount))
                  .arg(range)
            : tr("Sequence %1 \u00b7 all %2 beats on %3 selected %4 signal(s) already match \u00b7 no command will run")
                  .arg(sequence)
                  .arg(static_cast<qulonglong>(beats.size()))
                  .arg(static_cast<qulonglong>(lanes.size()))
                  .arg(kindName);
    } else {
        const auto mappings =
            projection.sequenceSummaries.join(
                QStringLiteral("; "));
        assessment.summary =
            projection.modelChanges
            ? tr("Sequences %1 \u00b7 %2 beats on %3 %4 signals \u00b7 changes %5 \u00b7 %6 \u00b7 Enter applies as one Undo")
                  .arg(mappings)
                  .arg(static_cast<qulonglong>(beats.size()))
                  .arg(static_cast<qulonglong>(lanes.size()))
                  .arg(kindName)
                  .arg(static_cast<qulonglong>(
                      projection.changedLaneCount))
                  .arg(range)
            : tr("Sequences %1 \u00b7 all %2 beats on %3 selected %4 signals already match \u00b7 no command will run")
                  .arg(mappings)
                  .arg(static_cast<qulonglong>(beats.size()))
                  .arg(static_cast<qulonglong>(lanes.size()))
                  .arg(kindName);
    }
    if (!projection.relationImpact.ids.empty()) {
        assessment.summary +=
            tr("\nWarning: applying this sequence removes %1 relation(s); Ctrl+Z restores waveform and relations")
                .arg(static_cast<qulonglong>(
                    projection.relationImpact.ids.size()));
        for (const auto& summary
             : projection.relationImpact.summaries) {
            assessment.summary +=
                tr("\nAffected: %1").arg(summary);
        }
    } else if (projection.modelChanges) {
        assessment.summary +=
            tr("\nNo Relation will be removed");
    }
    assessment.projection = std::move(projection);
    return assessment;
}

WaveCanvas::RangeSequenceSeed
WaveCanvas::currentRangeSequenceSeed() const
{
    RangeSequenceSeed seed;
    seed.summary =
        tr("Current values unavailable · select Bus or Enum signals over complete beats");
    if (!scenario_ || !project_ || !explicitRangeSelection_
        || !selectionRange_
        || selectionRange_->second <= selectionRange_->first
        || selectedLaneIds_.empty()) {
        return seed;
    }
    const auto kind = explicitRangeKind();
    if (!kind
        || (*kind != LaneKind::Bus
            && *kind != LaneKind::Enum)) {
        return seed;
    }
    seed.applicable = true;

    std::vector<const Lane*> lanes;
    lanes.reserve(selectedLaneIds_.size());
    for (const auto& laneId : selectedLaneIds_) {
        const auto* lane = findLane(*scenario_, laneId);
        if (!lane || lane->kind != *kind) {
            seed.summary =
                tr("Current values unavailable · a selected signal no longer exists");
            return seed;
        }
        lanes.push_back(lane);
    }

    const auto [start, end] = *selectionRange_;
    std::vector<std::pair<Tick, Tick>> beats;
    for (std::size_t laneIndex = 0;
         laneIndex < lanes.size();
         ++laneIndex) {
        const auto laneBeats =
            editableBeatRangesBetween(
                start,
                end - 1,
                *lanes.at(laneIndex));
        if (laneBeats.empty()
            || laneBeats.front().first != start
            || laneBeats.back().second != end) {
            seed.summary =
                tr("Current values unavailable · %1 does not cover complete beats over this range")
                    .arg(QString::fromStdString(
                        lanes.at(laneIndex)->name));
            return seed;
        }
        if (laneIndex == 0) {
            beats = laneBeats;
        } else if (laneBeats != beats) {
            seed.summary =
                tr("Current values unavailable · %1 and %2 use different beat grids")
                    .arg(
                        QString::fromStdString(
                            lanes.front()->name),
                        QString::fromStdString(
                            lanes.at(laneIndex)->name));
            return seed;
        }
    }
    constexpr std::size_t MaximumLoadedBeats = 1024;
    constexpr std::size_t MaximumLoadedSignalBeats = 4096;
    constexpr std::size_t MaximumLoadedCharacters = 8192;
    if (beats.size() > MaximumLoadedBeats
        || (!beats.empty()
            && lanes.size()
                > MaximumLoadedSignalBeats
                    / beats.size())) {
        seed.summary =
            tr("Current values unavailable · %1 signals x %2 beats exceeds the live load limit")
                .arg(static_cast<qulonglong>(
                    lanes.size()))
                .arg(static_cast<qulonglong>(
                    beats.size()));
        return seed;
    }

    // Loading existing Bus values first materializes one normalized value per
    // beat. Budget that intermediate representation before constructing a
    // potentially very wide implicit X literal (or validating an equally wide
    // stored literal). The estimate deliberately assumes binary-width values;
    // it is conservative and keeps both the draft and its baseline bounded.
    std::size_t conservativeCharacterCount = 0;
    const auto addToCharacterBudget =
        [&conservativeCharacterCount](const std::size_t amount) {
            if (amount > MaximumLoadedCharacters
                    - conservativeCharacterCount) {
                return false;
            }
            conservativeCharacterCount += amount;
            return true;
        };
    for (std::size_t laneIndex = 0;
         laneIndex < lanes.size();
         ++laneIndex) {
        const auto* lane = lanes.at(laneIndex);
        if (lane->kind != LaneKind::Bus) continue;
        if (laneIndex > 0
            && !addToCharacterBudget(3)) {
            conservativeCharacterCount = MaximumLoadedCharacters + 1;
            break;
        }
        const auto valueCharacters =
            static_cast<std::size_t>(
                std::max<std::uint32_t>(1, lane->width))
            + 2;
        for (std::size_t beatIndex = 0;
             beatIndex < beats.size();
             ++beatIndex) {
            if ((beatIndex > 0
                 && !addToCharacterBudget(1))
                || !addToCharacterBudget(valueCharacters)) {
                conservativeCharacterCount = MaximumLoadedCharacters + 1;
                break;
            }
        }
        if (conservativeCharacterCount > MaximumLoadedCharacters) break;
    }
    if (conservativeCharacterCount > MaximumLoadedCharacters) {
        seed.summary =
            tr("Current values unavailable · the conservative Bus value budget exceeds %1 characters; use a narrower signal or a shorter range")
                .arg(static_cast<qulonglong>(MaximumLoadedCharacters));
        return seed;
    }

    const auto format = [this](const Tick tick) {
        return QString::fromStdString(
            formatTick(tick, project_->timeBase));
    };
    struct SerializedValueSpan {
        int textStart{0};
        int textLength{0};
        std::size_t firstBeat{0};
        std::size_t beatCount{0};
        QString token;
    };
    struct SerializedValues {
        QString text;
        std::vector<SerializedValueSpan> spans;
    };
    const auto serializeValues =
        [](const std::vector<std::string>& values) {
            SerializedValues serialized;
            const auto appendToken =
                [&serialized](
                    const QString& token,
                    const std::size_t firstBeat,
                    const std::size_t beatCount) {
                    if (!serialized.text.isEmpty()) {
                        serialized.text.append(
                            QLatin1Char(' '));
                    }
                    const auto textStart =
                        static_cast<int>(
                            serialized.text.size());
                    serialized.text.append(token);
                    serialized.spans.push_back({
                        textStart,
                        static_cast<int>(token.size()),
                        firstBeat,
                        beatCount,
                        token,
                    });
                };
            if (values.size() <= 16) {
                serialized.spans.reserve(
                    values.size());
                for (std::size_t index = 0;
                     index < values.size();
                     ++index) {
                    appendToken(
                        QString::fromStdString(
                            values.at(index)),
                        index,
                        1);
                }
                return serialized;
            }
            for (std::size_t index = 0;
                 index < values.size();) {
                auto runEnd = index + 1;
                while (runEnd < values.size()
                       && values.at(runEnd)
                           == values.at(index)) {
                    ++runEnd;
                }
                const auto count = runEnd - index;
                const auto value =
                    QString::fromStdString(
                        values.at(index));
                if (count >= 3
                    && !value.contains(
                        QLatin1Char('*'))) {
                    appendToken(
                        QStringLiteral("%1*%2")
                            .arg(value)
                            .arg(
                                static_cast<qulonglong>(
                                    count)),
                        index,
                        count);
                } else {
                    for (std::size_t repeat = 0;
                         repeat < count;
                         ++repeat) {
                        appendToken(
                            value,
                            index + repeat,
                            1);
                    }
                }
                index = runEnd;
            }
            return serialized;
        };

    seed.spans.reserve(
        lanes.size() * beats.size());
    seed.values.reserve(lanes.size());
    for (std::size_t laneIndex = 0;
         laneIndex < lanes.size();
         ++laneIndex) {
        const auto* lane = lanes.at(laneIndex);
        std::vector<std::string> values;
        values.reserve(beats.size());
        for (const auto& beat : beats) {
            const auto* segment =
                segmentAtTick(*lane, beat.first);
            if (!segment) {
                if (lane->kind != LaneKind::Bus) {
                    seed.summary =
                        tr("Current values unavailable · %1 uses an implicit value at %2 that cannot be represented safely")
                            .arg(
                                QString::fromStdString(lane->name),
                                format(beat.first));
                    return seed;
                }
                const auto implicitValue = busPresetValue("x", lane->width);
                const auto implicitValidation =
                    validateLaneValue(*lane, implicitValue);
                if (!implicitValidation.valid) {
                    seed.summary =
                        tr("Current values unavailable · %1 uses an invalid implicit X at %2")
                            .arg(
                                QString::fromStdString(lane->name),
                                format(beat.first));
                    return seed;
                }
                values.push_back(implicitValidation.normalizedValue);
                continue;
            }
            const auto validation =
                validateLaneValue(
                    *lane,
                    segment->value);
            if (!validation.valid) {
                seed.summary =
                    tr("Current values unavailable · %1 contains an invalid stored value at %2")
                        .arg(
                            QString::fromStdString(
                                lane->name),
                            format(beat.first));
                return seed;
            }
            values.push_back(
                validation.normalizedValue);
        }

        seed.values.push_back(values);

        const auto serialized =
            serializeValues(values);
        if (serialized.text.contains(
                QLatin1Char('/'))) {
            seed.summary =
                tr("Current values unavailable · %1 contains '/' which conflicts with per-signal mapping syntax")
                    .arg(QString::fromStdString(
                        lane->name));
            return seed;
        }
        const auto roundTrip =
            parseBusDraftValues(
                *lane,
                serialized.text,
                false);
        if (!roundTrip.valid
            || roundTrip.normalizedValues
                != values) {
            seed.summary =
                tr("Current values unavailable · %1 contains values that cannot be represented safely in the inline sequence field")
                    .arg(QString::fromStdString(
                        lane->name));
            return seed;
        }
        if (!seed.text.isEmpty()) {
            seed.text.append(
                QStringLiteral(" / "));
        }
        const auto laneTextStart =
            static_cast<int>(
                seed.text.size());
        seed.text.append(serialized.text);
        for (const auto& span : serialized.spans) {
            const auto finalBeat =
                span.firstBeat
                + span.beatCount
                - 1;
            seed.spans.push_back({
                laneTextStart
                    + span.textStart,
                span.textLength,
                span.token,
                {{
                    lane->id,
                    beats.at(
                        span.firstBeat).first,
                    beats.at(finalBeat).second,
                    span.firstBeat,
                    span.beatCount,
                    beats.size(),
                }},
            });
        }
    }

    if (seed.text.size()
        > static_cast<qsizetype>(MaximumLoadedCharacters)) {
        const auto characterCount =
            seed.text.size();
        seed.text.clear();
        seed.spans.clear();
        seed.summary =
            tr("Current values unavailable · the editable text would contain %1 characters; the live field limit is %2")
                .arg(characterCount)
                .arg(static_cast<qulonglong>(MaximumLoadedCharacters));
        return seed;
    }
    seed.available = true;
    seed.laneCount = lanes.size();
    seed.beatCount = beats.size();
    seed.summary =
        lanes.size() == 1
        ? tr("Load %1 current beat value(s) from %2 over %3–%4 · no data changes until the edited draft is applied")
              .arg(static_cast<qulonglong>(
                  beats.size()))
              .arg(QString::fromStdString(
                  lanes.front()->name))
              .arg(format(start))
              .arg(format(end))
        : tr("Load %1 current beat value(s) for each of %2 selected %3 signals over %4–%5 in top-to-bottom / order · no data changes until the edited draft is applied")
              .arg(static_cast<qulonglong>(
                  beats.size()))
              .arg(static_cast<qulonglong>(
                  lanes.size()))
              .arg(
                  *kind == LaneKind::Bus
                      ? tr("Bus")
                      : tr("Enum"))
              .arg(format(start))
              .arg(format(end));
    return seed;
}

void WaveCanvas::loadCurrentRangeSequence()
{
    if (!rangeValueEdit_
        || !rangeLoadValuesAction_
        || !explicitRangeSelection_) {
        return;
    }
    const auto seed =
        currentRangeSequenceSeed();
    if (!seed.applicable
        || !seed.available
        || seed.text.isEmpty()) {
        emit statusMessage(seed.summary);
        return;
    }
    clearRangeSequenceCaretTarget();
    clearRangeSequenceAnchor();
    rangeValueEdit_->setText(seed.text);
    rangeSequenceTextSpans_ =
        seed.spans;
    rangeSequenceBaseline_ = RangeSequenceBaseline{
        scenario_,
        selectedLaneIds_,
        *selectionRange_,
        commandStack_ ? commandStack_->stateId() : 0,
        seed.text,
        seed.spans,
        seed.values,
    };
    rangeValueEdit_->setModified(false);
    rangeValueEdit_->setProperty(
        "loadedExisting",
        true);
    rangeValueEdit_->style()->unpolish(
        rangeValueEdit_);
    rangeValueEdit_->style()->polish(
        rangeValueEdit_);
    rangeValueEdit_->setStyleSheet({});
    rangeValueEdit_->setToolTip(
        seed.summary
        + tr("\nCurrent values are loaded but unchanged; edit the text, then press Enter")
        + tr("\nClick a preview beat to select its text token")
        + tr("\nTab and Shift+Tab move between mapped token targets")
        + tr("\nEsc hides the loaded values; Esc again closes the selection"));
    rangeValueEdit_->setAccessibleDescription(
        rangeValueEdit_->toolTip());
    rangeLoadValuesAction_->setVisible(false);
    rangeValueEdit_->setFocus(
        Qt::OtherFocusReason);
    rangeValueEdit_->setSelection(
        rangeValueEdit_->text().size(),
        -rangeValueEdit_->text().size());
    emit statusMessage(
        seed.summary
        + tr(" · edit in place, then press Enter"));
}

void WaveCanvas::clearRangeSequenceBaseline()
{
    rangeSequenceBaseline_.reset();
}

bool WaveCanvas::rangeSequenceBaselineMatchesContext() const noexcept
{
    return rangeSequenceBaseline_
        && scenario_
        && selectionRange_
        && rangeSequenceBaseline_->scenario == scenario_
        && rangeSequenceBaseline_->laneIds == selectedLaneIds_
        && rangeSequenceBaseline_->range == *selectionRange_
        && rangeSequenceBaseline_->historyStateId
            == (commandStack_ ? commandStack_->stateId() : 0);
}

bool WaveCanvas::restoreRangeSequenceBaseline()
{
    if (!rangeValueEdit_
        || !rangeSequenceBaselineMatchesContext()) {
        return false;
    }
    clearRangeSequenceCaretTarget();
    clearRangeSequenceAnchor();
    busRangeSequencePreview_.reset();
    rangeValueEdit_->setText(
        rangeSequenceBaseline_->text);
    rangeSequenceTextSpans_ =
        rangeSequenceBaseline_->spans;
    rangeValueEdit_->setModified(false);
    rangeValueEdit_->setProperty(
        "loadedExisting",
        true);
    rangeValueEdit_->setStyleSheet({});
    rangeValueEdit_->style()->unpolish(
        rangeValueEdit_);
    rangeValueEdit_->style()->polish(
        rangeValueEdit_);
    showRangeEditPalette();
    rangeValueEdit_->setFocus(
        Qt::OtherFocusReason);
    rangeValueEdit_->selectAll();
    viewport()->update();
    return true;
}

void WaveCanvas::clearRangeSequenceCaretTarget()
{
    if (!rangeSequenceCaretTarget_) return;
    rangeSequenceCaretTarget_.reset();
    viewport()->update();
}

void WaveCanvas::clearRangeSequenceAnchor()
{
    rangeSequenceAnchor_.reset();
}

void WaveCanvas::updateRangeSequenceCaretTarget(
    const bool announce)
{
    if (!rangeValueEdit_
        || rangeSequenceTextSpans_.empty()) {
        clearRangeSequenceCaretTarget();
        return;
    }
    const auto modified =
        rangeValueEdit_->isModified();
    const auto loadedExisting =
        rangeValueEdit_->property(
            "loadedExisting").toBool();
    if ((!modified && !loadedExisting)
        || (modified
            && !busRangeSequencePreview_)) {
        clearRangeSequenceCaretTarget();
        return;
    }
    const auto selectedText =
        rangeValueEdit_->selectedText();
    if (!rangeValueEdit_->text().isEmpty()
        && selectedText.size()
            == rangeValueEdit_->text().size()) {
        clearRangeSequenceCaretTarget();
        return;
    }
    const auto spanAt =
        [this](const int position) {
            return std::find_if(
                rangeSequenceTextSpans_.begin(),
                rangeSequenceTextSpans_.end(),
                [position](
                    const RangeSequenceTextSpan& span) {
                    return position >= span.textStart
                        && position
                            < span.textStart
                                + span.textLength;
                });
        };
    auto span =
        rangeSequenceTextSpans_.end();
    const auto selectionStart =
        rangeValueEdit_->selectionStart();
    if (!selectedText.isEmpty()
        && selectionStart >= 0) {
        const auto selectionEnd =
            selectionStart
            + static_cast<int>(
                selectedText.size());
        span = std::find_if(
            rangeSequenceTextSpans_.begin(),
            rangeSequenceTextSpans_.end(),
            [selectionStart, selectionEnd](
                const RangeSequenceTextSpan& candidate) {
                return selectionStart
                        >= candidate.textStart
                    && selectionEnd
                        <= candidate.textStart
                            + candidate.textLength;
            });
    } else {
        const auto probe =
            rangeValueEdit_->cursorPosition();
        span = spanAt(probe);
        if (span == rangeSequenceTextSpans_.end()
            && probe > 0) {
            span = spanAt(probe - 1);
        }
    }
    if (span == rangeSequenceTextSpans_.end()
        || span->targets.empty()) {
        clearRangeSequenceCaretTarget();
        return;
    }
    auto resolvedSpan = *span;
    if (rangeSequenceAnchor_) {
        const auto anchoredTarget =
            std::find_if(
                resolvedSpan.targets.begin(),
                resolvedSpan.targets.end(),
                [this](
                    const RangeSequenceTarget& target) {
                    return target.laneId
                            == rangeSequenceAnchor_->laneId
                        && rangeSequenceAnchor_->tick
                            >= target.start
                        && rangeSequenceAnchor_->tick
                            < target.end;
                });
        if (anchoredTarget
            != resolvedSpan.targets.end()) {
            std::rotate(
                resolvedSpan.targets.begin(),
                anchoredTarget,
                std::next(anchoredTarget));
        } else {
            rangeSequenceAnchor_ = RangeSequenceAnchor{
                resolvedSpan.targets.front().laneId,
                resolvedSpan.targets.front().start,
            };
        }
    } else {
        rangeSequenceAnchor_ = RangeSequenceAnchor{
            resolvedSpan.targets.front().laneId,
            resolvedSpan.targets.front().start,
        };
    }
    const auto unchanged =
        rangeSequenceCaretTarget_
        && rangeSequenceCaretTarget_->textStart
            == resolvedSpan.textStart
        && rangeSequenceCaretTarget_->textLength
            == resolvedSpan.textLength
        && rangeSequenceCaretTarget_->targets.size()
            == resolvedSpan.targets.size()
        && !rangeSequenceCaretTarget_->targets.empty()
        && rangeSequenceCaretTarget_->targets.front().laneId
            == resolvedSpan.targets.front().laneId
        && rangeSequenceCaretTarget_->targets.front().start
            == resolvedSpan.targets.front().start
        && rangeSequenceCaretTarget_->targets.front().end
            == resolvedSpan.targets.front().end;
    rangeSequenceCaretTarget_ = std::move(resolvedSpan);
    if (unchanged && !announce) return;

    const auto& firstTarget =
        rangeSequenceCaretTarget_->targets.front();
    ensureLaneVisible(firstTarget.laneId);
    ensureCursorVisible(firstTarget.start);
    viewport()->update();
    if (!announce || !scenario_ || !project_) {
        return;
    }
    const auto* firstLane =
        findLane(
            *scenario_,
            firstTarget.laneId);
    if (!firstLane) return;
    const auto range =
        tr("%1–%2")
            .arg(
                QString::fromStdString(
                    formatTick(
                        firstTarget.start,
                        project_->timeBase)),
                QString::fromStdString(
                    formatTick(
                        firstTarget.end,
                        project_->timeBase)));
    const auto beat =
        firstTarget.beatCount == 1
        ? tr("beat %1 of %2")
              .arg(static_cast<qulonglong>(
                  firstTarget.firstBeat + 1))
              .arg(static_cast<qulonglong>(
                  firstTarget.totalBeatCount))
        : tr("beats %1–%2 of %3")
              .arg(static_cast<qulonglong>(
                  firstTarget.firstBeat + 1))
              .arg(static_cast<qulonglong>(
                  firstTarget.firstBeat
                  + firstTarget.beatCount))
              .arg(static_cast<qulonglong>(
                  firstTarget.totalBeatCount));
    if (modified
        || rangeSequenceCaretTarget_->targets.size() > 1) {
        std::set<std::string> laneIds;
        std::set<std::size_t> beatIndexes;
        for (const auto& target
             : rangeSequenceCaretTarget_->targets) {
            laneIds.insert(target.laneId);
            for (std::size_t beatOffset = 0;
                 beatOffset < target.beatCount;
                 ++beatOffset) {
                beatIndexes.insert(
                    target.firstBeat
                    + beatOffset);
            }
        }
        QString beatSummary;
        if (beatIndexes.size() <= 8) {
            QStringList beatRuns;
            for (auto beat = beatIndexes.begin();
                 beat != beatIndexes.end();) {
                const auto runStart =
                    *beat;
                auto runEnd =
                    runStart;
                auto next = std::next(beat);
                while (next != beatIndexes.end()
                       && *next == runEnd + 1) {
                    runEnd = *next;
                    beat = next;
                    next = std::next(next);
                }
                beatRuns.append(
                    runStart == runEnd
                    ? QString::number(
                          static_cast<qulonglong>(
                              runStart + 1))
                    : QStringLiteral("%1\u2013%2")
                          .arg(
                              static_cast<qulonglong>(
                                  runStart + 1))
                          .arg(
                              static_cast<qulonglong>(
                                  runEnd + 1)));
                ++beat;
            }
            beatSummary =
                tr("beats %1 of %2")
                    .arg(beatRuns.join(
                        QStringLiteral(", ")))
                    .arg(static_cast<qulonglong>(
                        firstTarget
                            .totalBeatCount));
        } else {
            beatSummary =
                tr("%1 beats of %2")
                    .arg(
                        static_cast<qulonglong>(
                            beatIndexes.size()))
                    .arg(
                        static_cast<qulonglong>(
                            firstTarget
                                .totalBeatCount));
        }
        emit statusMessage(
            tr("Draft token \u00b7 primary %1 · %2 · %3 \u00b7 %4 target occurrence(s) on %5 signal(s) \u00b7 %6 \u00b7 value = %7")
                .arg(QString::fromStdString(
                    firstLane->name))
                .arg(beat)
                .arg(range)
                .arg(static_cast<qulonglong>(
                    rangeSequenceCaretTarget_
                        ->targets.size()))
                .arg(static_cast<qulonglong>(
                    laneIds.size()))
                .arg(beatSummary)
                .arg(rangeSequenceCaretTarget_->token));
        return;
    }
    emit statusMessage(
        tr("Current-value token · %1 · %2 · %3 = %4 · typing previews this target")
            .arg(
                QString::fromStdString(
                    firstLane->name),
                beat,
                range,
                rangeSequenceCaretTarget_->token));
}

bool WaveCanvas::navigateRangeSequenceTarget(
    const bool forward)
{
    if (!rangeValueEdit_
        || !hasActiveRangeSequenceTokenMapping()) {
        return false;
    }
    struct TargetEntry {
        const RangeSequenceTextSpan* span{nullptr};
        const RangeSequenceTarget* target{nullptr};
        std::size_t laneIndex{0};
    };
    std::vector<TargetEntry> entries;
    for (std::size_t laneIndex = 0;
         laneIndex < selectedLaneIds_.size();
         ++laneIndex) {
        for (const auto& span : rangeSequenceTextSpans_) {
            for (const auto& target : span.targets) {
                if (target.laneId
                    == selectedLaneIds_.at(laneIndex)) {
                    entries.push_back({
                        &span,
                        &target,
                        laneIndex,
                    });
                }
            }
        }
    }
    std::sort(
        entries.begin(),
        entries.end(),
        [](const TargetEntry& left,
           const TargetEntry& right) {
            if (left.laneIndex != right.laneIndex) {
                return left.laneIndex < right.laneIndex;
            }
            if (left.target->start
                != right.target->start) {
                return left.target->start
                    < right.target->start;
            }
            if (left.target->end
                != right.target->end) {
                return left.target->end
                    < right.target->end;
            }
            return left.span->textStart
                < right.span->textStart;
        });
    if (entries.empty()) return false;

    auto current = entries.end();
    if (rangeSequenceAnchor_) {
        current = std::find_if(
            entries.begin(),
            entries.end(),
            [this](const TargetEntry& entry) {
                return entry.target->laneId
                        == rangeSequenceAnchor_->laneId
                    && rangeSequenceAnchor_->tick
                        >= entry.target->start
                    && rangeSequenceAnchor_->tick
                        < entry.target->end;
            });
    }
    std::size_t targetIndex = forward ? 0 : entries.size() - 1;
    if (current != entries.end()) {
        const auto index = static_cast<std::size_t>(
            std::distance(entries.begin(), current));
        if ((forward && index + 1 >= entries.size())
            || (!forward && index == 0)) {
            rangeValueEdit_->setFocus(
                Qt::TabFocusReason);
            emit statusMessage(
                forward
                    ? tr("Last mapped token target reached · draft and focus kept")
                    : tr("First mapped token target reached · draft and focus kept"));
            return true;
        }
        targetIndex = forward
            ? index + 1
            : index - 1;
    }
    const auto& next = entries.at(targetIndex);
    rangeSequenceAnchor_ = RangeSequenceAnchor{
        next.target->laneId,
        next.target->start,
    };
    rangeValueEdit_->setFocus(
        Qt::TabFocusReason);
    rangeValueEdit_->setSelection(
        next.span->textStart,
        next.span->textLength);
    updateRangeSequenceCaretTarget();
    return true;
}

bool WaveCanvas::hasActiveRangeSequenceTokenMapping() const noexcept
{
    if (!rangeValueEdit_
        || !rangeEditPaletteVisible_
        || !explicitRangeSelection_
        || rangeSequenceTextSpans_.empty()) {
        return false;
    }
    return rangeValueEdit_->isModified()
        ? busRangeSequencePreview_.has_value()
        : rangeValueEdit_->property(
              "loadedExisting").toBool();
}

bool WaveCanvas::rangeSequenceTokenAt(
    const QPoint& position) const
{
    if (!scenario_
        || !hasActiveRangeSequenceTokenMapping()
        || position.x() < headerWidth_
        || !explicitRangeContains(position)
        || explicitRangeBoundaryAt(position)
            != SegmentBoundary::None) {
        return false;
    }
    const auto* lane = laneAtY(position.y());
    if (!lane
        || (lane->kind != LaneKind::Bus
            && lane->kind != LaneKind::Enum)
        || scenario_->duration <= 0) {
        return false;
    }
    const auto tick = std::clamp<Tick>(
        tickAtX(position.x()),
        0,
        scenario_->duration - 1);
    return rangeSequenceTextSpanAt(
               lane->id,
               tick)
        != nullptr;
}

const WaveCanvas::RangeSequenceTextSpan*
WaveCanvas::rangeSequenceTextSpanAt(
    const std::string& laneId,
    const Tick tick) const
{
    if (!hasActiveRangeSequenceTokenMapping()) {
        return nullptr;
    }
    const auto span = std::find_if(
        rangeSequenceTextSpans_.begin(),
        rangeSequenceTextSpans_.end(),
        [&laneId, tick](
            const RangeSequenceTextSpan& candidate) {
            return std::any_of(
                candidate.targets.begin(),
                candidate.targets.end(),
                [&laneId, tick](
                    const RangeSequenceTarget& target) {
                    return target.laneId == laneId
                        && tick >= target.start
                        && tick < target.end;
                });
        });
    return span == rangeSequenceTextSpans_.end()
        ? nullptr
        : &*span;
}

bool WaveCanvas::selectRangeSequenceTokenAt(
    const QPoint& position)
{
    if (!scenario_
        || !rangeValueEdit_
        || !hasActiveRangeSequenceTokenMapping()
        || position.x() < headerWidth_
        || !explicitRangeContains(position)
        || explicitRangeBoundaryAt(position)
            != SegmentBoundary::None) {
        return false;
    }
    const auto* lane =
        laneAtY(position.y());
    if (!lane
        || (lane->kind != LaneKind::Bus
            && lane->kind != LaneKind::Enum)
        || scenario_->duration <= 0) {
        return false;
    }
    const auto tick = std::clamp<Tick>(
        tickAtX(position.x()),
        0,
        scenario_->duration - 1);
    auto span = std::find_if(
        rangeSequenceTextSpans_.begin(),
        rangeSequenceTextSpans_.end(),
        [&lane, tick](
            const RangeSequenceTextSpan& candidate) {
            return std::any_of(
                candidate.targets.begin(),
                candidate.targets.end(),
                [&lane, tick](
                    const RangeSequenceTarget& target) {
                    return target.laneId
                            == lane->id
                        && tick >= target.start
                        && tick < target.end;
                });
        });
    if (span == rangeSequenceTextSpans_.end()) {
        return false;
    }
    const auto clickedTarget =
        std::find_if(
            span->targets.begin(),
            span->targets.end(),
            [&lane, tick](
                const RangeSequenceTarget& target) {
                return target.laneId
                        == lane->id
                    && tick >= target.start
                    && tick < target.end;
            });
    if (clickedTarget == span->targets.end()) {
        return false;
    }
    rangeSequenceAnchor_ = RangeSequenceAnchor{
        lane->id,
        tick,
    };
    QToolTip::hideText();
    hideBusPresetPalette();
    rangeValueEdit_->setFocus(
        Qt::MouseFocusReason);
    rangeValueEdit_->setSelection(
        span->textStart,
        span->textLength);
    updateRangeSequenceCaretTarget();
    return true;
}

std::optional<LaneKind> WaveCanvas::explicitRangeKind() const
{
    if (!scenario_ || !explicitRangeSelection_ || selectedLaneIds_.empty()) {
        return std::nullopt;
    }
    std::optional<LaneKind> kind;
    for (const auto& laneId : selectedLaneIds_) {
        const auto* lane = findLane(*scenario_, laneId);
        if (!lane
            || (lane->kind != LaneKind::Clock
                && lane->kind != LaneKind::Bit
                && lane->kind != LaneKind::Bus
                && lane->kind != LaneKind::Enum)) {
            return std::nullopt;
        }
        if (!kind) {
            kind = lane->kind;
        } else if (*kind != lane->kind) {
            return std::nullopt;
        }
    }
    return kind;
}

QStringList WaveCanvas::explicitRangeEnumSymbols() const
{
    if (!scenario_ || !explicitRangeSelection_ || selectedLaneIds_.empty()) {
        return {};
    }
    QStringList commonSymbols;
    bool firstLane = true;
    for (const auto& laneId : selectedLaneIds_) {
        const auto* lane = findLane(*scenario_, laneId);
        if (!lane || lane->kind != LaneKind::Enum) return {};
        QStringList laneSymbols;
        for (const auto& [symbol, value] : lane->enumMap) {
            Q_UNUSED(value);
            laneSymbols.append(QString::fromStdString(symbol));
        }
        if (firstLane) {
            commonSymbols = std::move(laneSymbols);
            firstLane = false;
            continue;
        }
        for (auto symbol = commonSymbols.begin(); symbol != commonSymbols.end();) {
            if (!laneSymbols.contains(*symbol)) {
                symbol = commonSymbols.erase(symbol);
            } else {
                ++symbol;
            }
        }
    }
    return commonSymbols;
}

WaveCanvas::RangePasteAvailability WaveCanvas::rangePasteAvailability() const
{
    if (!scenario_ || !project_ || !explicitRangeSelection_
        || !selectionRange_
        || selectionRange_->second <= selectionRange_->first
        || selectedLaneIds_.empty()) {
        return {
            false,
            tr("Paste unavailable · select a non-empty waveform range first"),
        };
    }
    return pasteAvailabilityWithImpact(
        selectedLaneIds_,
        selectionRange_->first,
        selectionRange_->second - selectionRange_->first);
}

WaveCanvas::RangeClearAvailability WaveCanvas::rangeClearAvailability() const
{
    RangeClearAvailability availability;
    if (!scenario_ || !project_ || !explicitRangeSelection_
        || !selectionRange_
        || selectionRange_->second <= selectionRange_->first
        || selectedLaneIds_.empty()) {
        availability.toolTip =
            tr("Clear unavailable · select a non-empty waveform range first");
        return availability;
    }

    const auto [start, end] = *selectionRange_;
    const auto rangeKind = explicitRangeKind();
    availability.clockRange =
        rangeKind && *rangeKind == LaneKind::Clock;

    std::unordered_set<std::string> reservedSegmentIds;
    for (const auto& lane : scenario_->lanes) {
        for (const auto& segment : lane.segments) {
            reservedSegmentIds.insert(segment.id);
        }
    }
    std::size_t previewSegmentSequence = 0;
    const auto nextPreviewSegmentId =
        [&previewSegmentSequence, &reservedSegmentIds] {
            std::string candidate;
            do {
                candidate = std::string{"__clear_preview_segment_"}
                    + std::to_string(++previewSegmentSequence);
            } while (reservedSegmentIds.contains(candidate));
            reservedSegmentIds.insert(candidate);
            return candidate;
        };
    const auto normalizePreviewSegments = [](Lane& lane) {
        std::sort(
            lane.segments.begin(),
            lane.segments.end(),
            [](const Segment& left, const Segment& right) {
                return left.start == right.start
                    ? left.end < right.end
                    : left.start < right.start;
            });
        std::vector<Segment> normalized;
        normalized.reserve(lane.segments.size());
        for (auto& candidate : lane.segments) {
            if (!normalized.empty()
                && normalized.back().end == candidate.start
                && normalized.back().value == candidate.value
                && normalized.back().extensions == candidate.extensions) {
                normalized.back().end = candidate.end;
            } else {
                normalized.push_back(std::move(candidate));
            }
        }
        lane.segments = std::move(normalized);
    };
    const auto clearPreviewRange =
        [&nextPreviewSegmentId, &normalizePreviewSegments](
            Lane& lane,
            const Tick clearStart,
            const Tick clearEnd) {
            std::vector<Segment> retained;
            retained.reserve(lane.segments.size() + 1);
            for (const auto& existing : lane.segments) {
                if (existing.end <= clearStart
                    || existing.start >= clearEnd) {
                    retained.push_back(existing);
                    continue;
                }
                if (existing.start < clearStart) {
                    auto left = existing;
                    left.end = clearStart;
                    retained.push_back(std::move(left));
                }
                if (existing.end > clearEnd) {
                    auto right = existing;
                    right.id = nextPreviewSegmentId();
                    right.start = clearEnd;
                    retained.push_back(std::move(right));
                }
            }
            lane.segments = std::move(retained);
            normalizePreviewSegments(lane);
        };

    std::vector<Lane> projectedLanes;
    projectedLanes.reserve(selectedLaneIds_.size());
    for (const auto& laneId : selectedLaneIds_) {
        const auto* lane = findLane(*scenario_, laneId);
        if (!lane || lane->kind == LaneKind::Group) continue;
        const auto overlaps = std::any_of(
            lane->segments.begin(),
            lane->segments.end(),
            [start, end](const Segment& segment) {
                return segment.start < end && segment.end > start;
            });
        if (!overlaps) continue;
        auto projected = *lane;
        clearPreviewRange(projected, start, end);
        projectedLanes.push_back(std::move(projected));
    }

    availability.affectedLaneCount = projectedLanes.size();
    if (projectedLanes.empty()) {
        availability.toolTip = availability.clockRange
            ? tr("Run unavailable · selected Clock range already runs normally; no overrides or Relations need clearing")
            : tr("Clear unavailable · selected range already uses implicit values; no waveform or Relation change is needed");
        return availability;
    }

    availability.relationImpact =
        relationRemovalImpactForProjectedLanes(projectedLanes);
    availability.enabled = true;
    const auto format = [this](const Tick tick) {
        return QString::fromStdString(
            formatTick(tick, project_->timeBase));
    };
    availability.toolTip = availability.clockRange
        ? tr("Restore %1 Clock signal(s) over %2–%3 to Run (R or Delete)")
              .arg(static_cast<qulonglong>(
                  availability.affectedLaneCount))
              .arg(format(start), format(end))
        : tr("Clear explicit values from %1 of %2 selected signal(s) over %3–%4 (Delete)")
              .arg(static_cast<qulonglong>(
                  availability.affectedLaneCount))
              .arg(static_cast<qulonglong>(
                  selectedLaneIds_.size()))
              .arg(format(start), format(end));
    if (availability.relationImpact.ids.empty()) {
        availability.toolTip += tr("\nNo Relation will be removed");
        return availability;
    }

    availability.toolTip +=
        tr("\nWarning: removes %1 relation(s) because referenced edges disappear; Ctrl+Z restores waveform and relations")
            .arg(static_cast<qulonglong>(
                availability.relationImpact.ids.size()));
    for (const auto& summary : availability.relationImpact.summaries) {
        availability.toolTip += tr("\nAffected: %1").arg(summary);
    }
    return availability;
}

WaveCanvas::RangeValueAvailability WaveCanvas::rangeValueAvailability(
    const std::string& value,
    const std::string& presetId) const
{
    RangeValueAvailability availability;
    if (!scenario_ || !project_ || !explicitRangeSelection_
        || !selectionRange_
        || selectionRange_->second <= selectionRange_->first
        || selectedLaneIds_.empty()) {
        availability.toolTip =
            tr("Set unavailable · select a non-empty waveform range first");
        return availability;
    }

    const auto kind = explicitRangeKind();
    if (!kind) {
        availability.toolTip =
            tr("Set unavailable · select only Clock, only Bit, only Bus, or only Enum signals");
        return availability;
    }

    const auto [start, end] = *selectionRange_;
    availability.displayValue = *kind == LaneKind::Clock
        ? value == "gated"
            ? tr("GATED")
            : value == "disabled"
                ? tr("DISABLED · X")
                : QString::fromStdString(value)
        : !presetId.empty()
            ? busPresetDisplayLabel(presetId)
            : QString::fromStdString(value);

    std::unordered_set<std::string> reservedSegmentIds;
    for (const auto& lane : scenario_->lanes) {
        for (const auto& segment : lane.segments) {
            reservedSegmentIds.insert(segment.id);
        }
    }
    std::size_t previewSegmentSequence = 0;
    const auto nextPreviewSegmentId =
        [&previewSegmentSequence, &reservedSegmentIds] {
            std::string candidate;
            do {
                candidate = std::string{"__range_value_preview_segment_"}
                    + std::to_string(++previewSegmentSequence);
            } while (reservedSegmentIds.contains(candidate));
            reservedSegmentIds.insert(candidate);
            return candidate;
        };
    const auto normalizePreviewSegments = [](Lane& lane) {
        std::sort(
            lane.segments.begin(),
            lane.segments.end(),
            [](const Segment& left, const Segment& right) {
                return left.start == right.start
                    ? left.end < right.end
                    : left.start < right.start;
            });
        std::vector<Segment> normalized;
        normalized.reserve(lane.segments.size());
        for (auto& candidate : lane.segments) {
            if (!normalized.empty()
                && normalized.back().end == candidate.start
                && normalized.back().value == candidate.value
                && normalized.back().extensions == candidate.extensions) {
                normalized.back().end = candidate.end;
            } else {
                normalized.push_back(std::move(candidate));
            }
        }
        lane.segments = std::move(normalized);
    };
    const auto rangeAlreadyEqualsPreview = [](
                                               const Lane& lane,
                                               const std::string_view targetValue,
                                               const JsonExtensions& extensions,
                                               const Tick rangeStart,
                                               const Tick rangeEnd) {
        auto coveredUntil = rangeStart;
        for (const auto& segment : lane.segments) {
            if (segment.end <= coveredUntil) continue;
            if (segment.start > coveredUntil
                || segment.value != targetValue
                || segment.extensions != extensions) {
                return false;
            }
            coveredUntil = std::min(rangeEnd, segment.end);
            if (coveredUntil >= rangeEnd) return true;
        }
        return false;
    };
    const auto setPreviewRange =
        [&nextPreviewSegmentId, &normalizePreviewSegments](
            Lane& lane,
            const std::string& targetValue,
            const JsonExtensions& extensions,
            const Tick rangeStart,
            const Tick rangeEnd) {
            std::vector<Segment> replaced;
            replaced.reserve(lane.segments.size() + 2);
            for (const auto& existing : lane.segments) {
                if (existing.end <= rangeStart
                    || existing.start >= rangeEnd) {
                    replaced.push_back(existing);
                    continue;
                }
                if (existing.start < rangeStart) {
                    auto left = existing;
                    left.end = rangeStart;
                    replaced.push_back(std::move(left));
                }
                if (existing.end > rangeEnd) {
                    auto right = existing;
                    right.id = nextPreviewSegmentId();
                    right.start = rangeEnd;
                    replaced.push_back(std::move(right));
                }
            }
            replaced.push_back({
                nextPreviewSegmentId(),
                rangeStart,
                rangeEnd,
                targetValue,
                extensions,
            });
            lane.segments = std::move(replaced);
            normalizePreviewSegments(lane);
        };

    std::vector<Lane> projectedLanes;
    projectedLanes.reserve(selectedLaneIds_.size());
    for (const auto& laneId : selectedLaneIds_) {
        const auto* lane = findLane(*scenario_, laneId);
        if (!lane || lane->kind == LaneKind::Group) {
            availability.toolTip =
                tr("Set unavailable · a selected signal no longer exists");
            return availability;
        }

        auto targetValue = value;
        JsonExtensions extensions;
        if (*kind == LaneKind::Bus && !presetId.empty()) {
            targetValue = busPresetValue(presetId, lane->width);
            extensions.emplace(
                std::string(kBusPresetExtension),
                "\"" + presetId + "\"");
        }
        const auto validation = validateLaneValue(*lane, targetValue);
        if (targetValue.empty() || !validation.valid) {
            availability.toolTip = targetValue.empty()
                ? *kind == LaneKind::Enum
                    ? tr("Set unavailable · enter an Enum symbol or numeric value")
                    : tr("Set unavailable · enter a Bus value")
                : tr("Set unavailable for %1 · %2")
                      .arg(
                          QString::fromStdString(lane->name),
                          QString::fromStdString(validation.error));
            return availability;
        }
        if (availability.displayValue.isEmpty()) {
            availability.displayValue =
                QString::fromStdString(validation.normalizedValue);
        }
        if (rangeAlreadyEqualsPreview(
                *lane,
                validation.normalizedValue,
                extensions,
                start,
                end)) {
            continue;
        }
        auto projected = *lane;
        setPreviewRange(
            projected,
            validation.normalizedValue,
            extensions,
            start,
            end);
        projectedLanes.push_back(std::move(projected));
    }

    availability.valid = true;
    availability.affectedLaneCount = projectedLanes.size();
    const auto format = [this](const Tick tick) {
        return QString::fromStdString(
            formatTick(tick, project_->timeBase));
    };
    if (projectedLanes.empty()) {
        availability.toolTip =
            tr("Set unavailable · %1–%2 already = %3 on all %4 selected signal(s); no waveform or Relation change is needed")
                .arg(format(start))
                .arg(format(end))
                .arg(availability.displayValue)
                .arg(static_cast<qulonglong>(selectedLaneIds_.size()));
        return availability;
    }

    availability.relationImpact =
        relationRemovalImpactForProjectedLanes(projectedLanes);
    availability.enabled = true;
    availability.toolTip =
        tr("Set %1 of %2 selected %3 signal(s) over %4–%5 to %6")
            .arg(static_cast<qulonglong>(
                availability.affectedLaneCount))
            .arg(static_cast<qulonglong>(
                selectedLaneIds_.size()))
            .arg(
                *kind == LaneKind::Clock ? tr("Clock")
                : *kind == LaneKind::Bit ? tr("Bit")
                : *kind == LaneKind::Bus ? tr("Bus")
                                         : tr("Enum"))
            .arg(format(start))
            .arg(format(end))
            .arg(availability.displayValue);
    if (availability.relationImpact.ids.empty()) {
        availability.toolTip += tr("\nNo Relation will be removed");
        return availability;
    }

    availability.toolTip +=
        tr("\nWarning: removes %1 relation(s) because referenced edges disappear; Ctrl+Z restores waveform and relations")
            .arg(static_cast<qulonglong>(
                availability.relationImpact.ids.size()));
    for (const auto& summary : availability.relationImpact.summaries) {
        availability.toolTip += tr("\nAffected: %1").arg(summary);
    }
    return availability;
}

WaveCanvas::RangeRepeatAvailability
WaveCanvas::rangeRepeatAvailability() const
{
    RangeRepeatAvailability availability;
    if (!scenario_ || !project_ || !explicitRangeSelection_
        || !selectionRange_
        || selectionRange_->second <= selectionRange_->first
        || selectedLaneIds_.empty()) {
        availability.toolTip =
            tr("Repeat unavailable · select a non-empty waveform range first");
        return availability;
    }

    const auto [sourceStart, sourceEnd] = *selectionRange_;
    availability.destination = sourceEnd;
    availability.duration = sourceEnd - sourceStart;
    if (availability.duration
        > std::numeric_limits<Tick>::max() - availability.destination) {
        availability.toolTip =
            tr("Repeat unavailable · the following range is outside the supported timeline");
        return availability;
    }

    const auto preview = buildRepeatPreview();
    if (!preview
        || preview->lanes.size() != selectedLaneIds_.size()) {
        availability.toolTip =
            tr("Repeat unavailable · one or more selected signals no longer exist");
        return availability;
    }

    availability.extendsEnd =
        availability.destination + availability.duration
        > scenario_->duration;
    availability.relationImpact.ids = preview->relationRemovalIds;
    availability.relationImpact.endpointSummaries =
        preview->relationRemovalEndpointSummaries;
    availability.relationImpact.summaries =
        preview->relationRemovalSummaries;
    const auto format = [this](const Tick tick) {
        return QString::fromStdString(
            formatTick(tick, project_->timeBase));
    };
    if (!preview->modelChanges) {
        availability.toolTip =
            tr("Repeat unavailable · following range %1–%2 already matches selected range %3–%4; no waveform, Relation, or End change is needed\nClipboard unchanged")
                .arg(format(availability.destination))
                .arg(format(
                    availability.destination + availability.duration))
                .arg(format(sourceStart))
                .arg(format(sourceEnd));
        return availability;
    }

    availability.enabled = true;
    availability.toolTip =
        tr("Repeat %1 selected signal(s) from %2–%3 into %4–%5 (Ctrl+D)\nClipboard unchanged")
            .arg(static_cast<qulonglong>(preview->lanes.size()))
            .arg(format(sourceStart))
            .arg(format(sourceEnd))
            .arg(format(availability.destination))
            .arg(format(
                availability.destination + availability.duration));
    if (availability.extendsEnd) {
        availability.toolTip +=
            tr("\nExtends End to %1")
                .arg(format(
                    availability.destination + availability.duration));
    }
    if (availability.relationImpact.ids.empty()) {
        availability.toolTip += tr("\nNo Relation will be removed");
        return availability;
    }

    availability.toolTip +=
        tr("\nWarning: removes %1 relation(s) because referenced edges disappear; Ctrl+Z restores waveform and relations")
            .arg(static_cast<qulonglong>(
                availability.relationImpact.ids.size()));
    for (const auto& summary : availability.relationImpact.summaries) {
        availability.toolTip += tr("\nAffected: %1").arg(summary);
    }
    availability.toolTip +=
        tr("\nAffected relation lines are highlighted in amber on the canvas");
    return availability;
}

WaveCanvas::RangePasteAvailability WaveCanvas::pasteAvailabilityForTargets(
    const std::vector<std::string>& targetLaneIds,
    const Tick pasteStart,
    const std::optional<Tick> selectedWidth) const
{
    const auto unavailable = [](const QString& reason) {
        return RangePasteAvailability{
            false,
            tr("Paste unavailable · %1").arg(reason),
        };
    };
    if (!scenario_ || !project_ || targetLaneIds.empty()) {
        return unavailable(tr("select one or more target signals first"));
    }

    const auto* mime = QApplication::clipboard()->mimeData();
    if (!mime) return unavailable(tr("copy a waveform range first"));
    const auto content = mime->hasFormat(kRangeMimeType)
        ? mime->data(kRangeMimeType)
        : mime->text().toUtf8();
    if (content.trimmed().isEmpty()) {
        return unavailable(tr("copy a waveform range first"));
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(content, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return unavailable(tr("clipboard does not contain a Wave Workbench range"));
    }
    const auto root = document.object();
    const auto schemaVersion =
        root.value(QStringLiteral("schemaVersion")).toInt(-1);
    if ((schemaVersion != 1 && schemaVersion != 2)
        || !root.value(QStringLiteral("durationTick")).isString()
        || !root.value(QStringLiteral("lanes")).isArray()) {
        return unavailable(tr("clipboard range schema is invalid"));
    }

    bool validDuration = false;
    const auto duration = root.value(QStringLiteral("durationTick"))
                              .toString()
                              .toLongLong(&validDuration);
    if (!validDuration || duration <= 0) {
        return unavailable(tr("clipboard range duration is invalid"));
    }
    if (pasteStart < 0
        || pasteStart > scenario_->duration
        || duration > std::numeric_limits<Tick>::max() - pasteStart) {
        return unavailable(tr("selected paste destination is invalid"));
    }

    const auto laneValues = root.value(QStringLiteral("lanes")).toArray();
    if (laneValues.isEmpty()) {
        return unavailable(tr("clipboard range contains no signals"));
    }
    if (laneValues.size() != static_cast<qsizetype>(targetLaneIds.size())) {
        return unavailable(
            tr("copied range has %1 signal(s), current selection has %2 · select the same number of targets")
                .arg(laneValues.size())
                .arg(static_cast<qulonglong>(targetLaneIds.size())));
    }

    const auto kindLabel = [](const LaneKind kind) {
        const auto label = toString(kind);
        return QString::fromLatin1(
            label.data(),
            static_cast<qsizetype>(label.size()));
    };
    QStringList mappings;
    mappings.reserve(laneValues.size());
    for (qsizetype index = 0; index < laneValues.size(); ++index) {
        if (!laneValues.at(index).isObject()) {
            return unavailable(tr("clipboard range signal data is invalid"));
        }
        const auto laneObject = laneValues.at(index).toObject();
        const auto sourceLaneId =
            laneObject.value(QStringLiteral("laneId")).toString().toStdString();
        if (sourceLaneId.empty()
            || !laneObject.value(QStringLiteral("segments")).isArray()) {
            return unavailable(tr("clipboard range signal data is invalid"));
        }

        QString sourceName;
        LaneKind sourceKind{LaneKind::Bit};
        std::size_t sourceWidth{1};
        if (schemaVersion == 2) {
            bool validWidth = false;
            const auto kind = laneKindFromString(
                laneObject.value(QStringLiteral("kind")).toString().toStdString());
            const auto width = laneObject.value(QStringLiteral("width"))
                                   .toString()
                                   .toULongLong(&validWidth);
            if (!laneObject.value(QStringLiteral("name")).isString()
                || !laneObject.value(QStringLiteral("kind")).isString()
                || !laneObject.value(QStringLiteral("width")).isString()
                || !kind
                || *kind == LaneKind::Group
                || !validWidth
                || width == 0) {
                return unavailable(tr("clipboard range signal metadata is invalid"));
            }
            sourceName = laneObject.value(QStringLiteral("name")).toString();
            sourceKind = *kind;
            sourceWidth = static_cast<std::size_t>(width);
        } else {
            const auto* sourceLane = findLane(*scenario_, sourceLaneId);
            if (!sourceLane || sourceLane->kind == LaneKind::Group) {
                return unavailable(
                    tr("legacy clipboard source %1 no longer exists · copy the range again")
                        .arg(QString::fromStdString(sourceLaneId)));
            }
            sourceName = QString::fromStdString(sourceLane->name);
            sourceKind = sourceLane->kind;
            sourceWidth = sourceLane->width;
        }

        const auto& targetId = targetLaneIds.at(
            static_cast<std::size_t>(index));
        const auto* targetLane = findLane(*scenario_, targetId);
        if (!targetLane || targetLane->kind == LaneKind::Group) {
            return unavailable(
                tr("selected target for %1 is unavailable")
                    .arg(sourceName));
        }
        const auto targetName = QString::fromStdString(targetLane->name);
        if (sourceKind != targetLane->kind) {
            return unavailable(
                tr("%1 (%2) → %3 (%4) · signal types must match")
                    .arg(
                        sourceName,
                        kindLabel(sourceKind),
                        targetName,
                        kindLabel(targetLane->kind)));
        }
        if ((sourceKind == LaneKind::Bus || sourceKind == LaneKind::Enum)
            && sourceWidth != targetLane->width) {
            return unavailable(
                tr("%1 (%2-bit) → %3 (%4-bit) · signal widths must match")
                    .arg(sourceName)
                    .arg(static_cast<qulonglong>(sourceWidth))
                    .arg(targetName)
                    .arg(static_cast<qulonglong>(targetLane->width)));
        }

        const auto segments =
            laneObject.value(QStringLiteral("segments")).toArray();
        for (const auto& segmentValue : segments) {
            if (!segmentValue.isObject()) {
                return unavailable(
                    tr("copied values for %1 are invalid").arg(sourceName));
            }
            const auto segment = segmentValue.toObject();
            bool validStart = false;
            bool validEnd = false;
            const auto start = segment.value(QStringLiteral("startTick"))
                                   .toString()
                                   .toLongLong(&validStart);
            const auto end = segment.value(QStringLiteral("endTick"))
                                 .toString()
                                 .toLongLong(&validEnd);
            if (!validStart
                || !validEnd
                || start < 0
                || end <= start
                || end > duration
                || !segment.value(QStringLiteral("value")).isString()) {
                return unavailable(
                    tr("copied values for %1 are invalid").arg(sourceName));
            }
            const auto validation = validateLaneValue(
                *targetLane,
                segment.value(QStringLiteral("value")).toString().toStdString());
            if (!validation.valid) {
                return unavailable(
                    tr("%1 → %2 · %3")
                        .arg(
                            sourceName,
                            targetName,
                            QString::fromStdString(validation.error)));
            }
        }
        mappings.append(tr("%1 → %2").arg(sourceName, targetName));
    }

    const auto format = [this](const Tick tick) {
        return QString::fromStdString(formatTick(tick, project_->timeBase));
    };
    auto toolTip =
        tr("Paste %1 copied signal(s), width %2, at %3 (Ctrl+V)\n%4")
            .arg(laneValues.size())
            .arg(format(duration))
            .arg(format(pasteStart))
            .arg(mappings.join(QStringLiteral(", ")));
    if (selectedWidth && *selectedWidth != duration) {
        toolTip += tr("\nPaste uses the copied width; current selected width is %1")
                       .arg(format(*selectedWidth));
    }
    const auto pasteEnd = pasteStart + duration;
    if (pasteEnd > scenario_->duration) {
        toolTip += tr("\nExtends End to %1").arg(format(pasteEnd));
    }
    return {true, toolTip};
}

WaveCanvas::RangePasteAvailability
WaveCanvas::pasteAvailabilityWithImpact(
    const std::vector<std::string>& targetLaneIds,
    const Tick pasteStart,
    const std::optional<Tick> selectedWidth) const
{
    auto availability = pasteAvailabilityForTargets(
        targetLaneIds,
        pasteStart,
        selectedWidth);
    if (!availability.enabled) return availability;

    const auto preview = buildPastePreview(targetLaneIds, pasteStart);
    if (!preview) {
        return availability;
    }
    if (!preview->modelChanges) {
        availability.enabled = false;
        availability.toolTip =
            tr("Paste unavailable · target already matches copied range; no waveform, Relation, or End change is needed\n%1")
                .arg(availability.toolTip);
        return availability;
    }
    if (preview->relationRemovalCount == 0) return availability;
    availability.toolTip += tr(
        "\nWarning: removes %1 relation(s) because referenced edges disappear; Ctrl+Z restores waveform and relations")
                                .arg(static_cast<qulonglong>(
                                    preview->relationRemovalCount));
    for (const auto& summary : preview->relationRemovalSummaries) {
        availability.toolTip += tr("\nAffected: %1").arg(summary);
    }
    availability.toolTip += tr(
        "\nAffected relation lines are highlighted in amber on the canvas");
    return availability;
}

std::optional<WaveCanvas::LaneHeaderPastePreview>
WaveCanvas::laneHeaderPastePreview() const
{
    if (!scenario_ || !project_ || tool_ != Tool::WaveEdit
        || !laneHeaderSelectionActive_ || selectedLaneIds_.empty()) {
        return std::nullopt;
    }
    const auto availability = pasteAvailabilityForTargets(
        selectedLaneIds_,
        cursorTick_);
    if (!availability.enabled) return std::nullopt;
    auto preview = buildPastePreview(selectedLaneIds_, cursorTick_);
    if (!preview || !preview->modelChanges) return std::nullopt;
    return preview;
}

std::optional<WaveCanvas::LaneHeaderPastePreview>
WaveCanvas::explicitRangePastePreview() const
{
    if (!scenario_ || !project_ || tool_ != Tool::WaveEdit
        || !explicitRangeSelection_ || !selectionRange_
        || selectionRange_->second <= selectionRange_->first
        || selectedLaneIds_.empty()) {
        return std::nullopt;
    }
    const auto selectedWidth =
        selectionRange_->second - selectionRange_->first;
    const auto availability = pasteAvailabilityForTargets(
        selectedLaneIds_,
        selectionRange_->first,
        selectedWidth);
    if (!availability.enabled) return std::nullopt;
    auto preview = buildPastePreview(
        selectedLaneIds_,
        selectionRange_->first);
    if (!preview) return std::nullopt;
    if (!preview->modelChanges
        && preview->duration == selectedWidth) {
        return std::nullopt;
    }
    return preview;
}

std::optional<WaveCanvas::LaneHeaderPastePreview>
WaveCanvas::pastePreview() const
{
    if (explicitRangeSelection_) {
        return explicitRangePastePreview();
    }
    return laneHeaderPastePreview();
}

std::optional<WaveCanvas::LaneHeaderPastePreview>
WaveCanvas::buildPastePreview(
    const std::vector<std::string>& targetLaneIds,
    const Tick pasteStart) const
{
    if (!scenario_ || !project_ || tool_ != Tool::WaveEdit
        || targetLaneIds.empty() || pasteStart < 0) {
        return std::nullopt;
    }
    const auto* mime = QApplication::clipboard()->mimeData();
    if (!mime) return std::nullopt;
    const auto content = mime->hasFormat(kRangeMimeType)
        ? mime->data(kRangeMimeType)
        : mime->text().toUtf8();
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(content, &parseError);
    if (parseError.error != QJsonParseError::NoError
        || !document.isObject()) {
        return std::nullopt;
    }
    const auto root = document.object();
    bool validDuration = false;
    const auto duration = root.value(QStringLiteral("durationTick"))
                              .toString()
                              .toLongLong(&validDuration);
    const auto laneValues = root.value(QStringLiteral("lanes")).toArray();
    if (!validDuration
        || duration <= 0
        || pasteStart > scenario_->duration
        || duration > std::numeric_limits<Tick>::max() - pasteStart
        || laneValues.size()
            != static_cast<qsizetype>(targetLaneIds.size())) {
        return std::nullopt;
    }

    std::unordered_set<std::string> reservedSegmentIds;
    for (const auto& lane : scenario_->lanes) {
        for (const auto& segment : lane.segments) {
            reservedSegmentIds.insert(segment.id);
        }
    }
    std::size_t previewSegmentSequence = 0;
    const auto nextPreviewSegmentId =
        [&previewSegmentSequence, &reservedSegmentIds] {
        std::string candidate;
        do {
            candidate = std::string{"__paste_preview_segment_"}
                + std::to_string(++previewSegmentSequence);
        } while (reservedSegmentIds.contains(candidate));
        reservedSegmentIds.insert(candidate);
        return candidate;
    };
    const auto normalizePreviewSegments = [](Lane& lane) {
        std::sort(
            lane.segments.begin(),
            lane.segments.end(),
            [](const Segment& left, const Segment& right) {
                return left.start == right.start
                    ? left.end < right.end
                    : left.start < right.start;
            });
        std::vector<Segment> normalized;
        normalized.reserve(lane.segments.size());
        for (auto& candidate : lane.segments) {
            if (!normalized.empty()
                && normalized.back().end == candidate.start
                && normalized.back().value == candidate.value
                && normalized.back().extensions == candidate.extensions) {
                normalized.back().end = candidate.end;
            } else {
                normalized.push_back(std::move(candidate));
            }
        }
        lane.segments = std::move(normalized);
    };
    const auto clearPreviewRange =
        [&nextPreviewSegmentId, &normalizePreviewSegments](
            Lane& lane,
            const Tick start,
            const Tick end) {
            std::vector<Segment> retained;
            retained.reserve(lane.segments.size() + 1);
            for (const auto& existing : lane.segments) {
                if (existing.end <= start || existing.start >= end) {
                    retained.push_back(existing);
                    continue;
                }
                if (existing.start < start) {
                    auto left = existing;
                    left.end = start;
                    retained.push_back(std::move(left));
                }
                if (existing.end > end) {
                    auto right = existing;
                    right.id = nextPreviewSegmentId();
                    right.start = end;
                    retained.push_back(std::move(right));
                }
            }
            lane.segments = std::move(retained);
            normalizePreviewSegments(lane);
        };
    const auto insertPreviewSegment =
        [&nextPreviewSegmentId, &normalizePreviewSegments](
            Lane& lane,
            Segment segment) {
        std::vector<Segment> replaced;
        replaced.reserve(lane.segments.size() + 2);
        for (const auto& existing : lane.segments) {
            if (existing.end <= segment.start
                || existing.start >= segment.end) {
                replaced.push_back(existing);
                continue;
            }
            if (existing.start < segment.start) {
                auto left = existing;
                left.end = segment.start;
                replaced.push_back(std::move(left));
            }
            if (existing.end > segment.end) {
                auto right = existing;
                right.id = nextPreviewSegmentId();
                right.start = segment.end;
                replaced.push_back(std::move(right));
            }
        }
        segment.id = nextPreviewSegmentId();
        replaced.push_back(std::move(segment));
        lane.segments = std::move(replaced);
        normalizePreviewSegments(lane);
    };
    const auto sameWaveform = [](const Lane& left, const Lane& right) {
        return left.segments.size() == right.segments.size()
            && std::equal(
                left.segments.begin(),
                left.segments.end(),
                right.segments.begin(),
                [](const Segment& leftSegment,
                   const Segment& rightSegment) {
                    return leftSegment.start == rightSegment.start
                        && leftSegment.end == rightSegment.end
                        && leftSegment.value == rightSegment.value
                        && leftSegment.extensions
                            == rightSegment.extensions;
                });
    };

    LaneHeaderPastePreview preview;
    preview.start = pasteStart;
    preview.duration = duration;
    preview.lanes.reserve(targetLaneIds.size());
    for (std::size_t index = 0; index < targetLaneIds.size(); ++index) {
        const auto* target = findLane(*scenario_, targetLaneIds.at(index));
        if (!target || target->kind == LaneKind::Group) return std::nullopt;
        const auto laneObject =
            laneValues.at(static_cast<qsizetype>(index)).toObject();
        const auto segmentValues =
            laneObject.value(QStringLiteral("segments")).toArray();

        auto previewLane = *target;
        clearPreviewRange(
            previewLane,
            preview.start,
            preview.start + preview.duration);
        previewLane.color =
            laneColor(*target).lighter(145).name(QColor::HexRgb).toStdString();
        for (const auto& segmentValue : segmentValues) {
            if (!segmentValue.isObject()) return std::nullopt;
            const auto segmentObject = segmentValue.toObject();
            bool validStart = false;
            bool validEnd = false;
            const auto relativeStart =
                segmentObject.value(QStringLiteral("startTick"))
                    .toString()
                    .toLongLong(&validStart);
            const auto relativeEnd =
                segmentObject.value(QStringLiteral("endTick"))
                    .toString()
                    .toLongLong(&validEnd);
            if (!validStart
                || !validEnd
                || relativeStart < 0
                || relativeEnd <= relativeStart
                || relativeEnd > duration
                || !segmentObject.value(QStringLiteral("value")).isString()) {
                return std::nullopt;
            }
            Segment segment;
            segment.start = preview.start + relativeStart;
            segment.end = preview.start + relativeEnd;
            const auto validation = validateLaneValue(
                *target,
                segmentObject.value(QStringLiteral("value"))
                    .toString()
                    .toStdString());
            if (!validation.valid) return std::nullopt;
            segment.value = validation.normalizedValue;
            const auto extensions =
                segmentObject.value(QStringLiteral("extensions"));
            if (extensions.isObject()) {
                const auto extensionObject = extensions.toObject();
                for (auto iterator = extensionObject.begin();
                     iterator != extensionObject.end();
                     ++iterator) {
                    QJsonArray wrapper;
                    wrapper.append(iterator.value());
                    auto encoded =
                        QJsonDocument(wrapper).toJson(QJsonDocument::Compact);
                    encoded.remove(0, 1);
                    encoded.chop(1);
                    segment.extensions.emplace(
                        iterator.key().toStdString(),
                        encoded.toStdString());
                }
            }
            const auto segmentStart = segment.start;
            const auto segmentEnd = segment.end;
            const auto projectedValue = segment.value;
            auto segmentExtensions = std::move(segment.extensions);
            segment.extensions.clear();
            insertPreviewSegment(previewLane, std::move(segment));
            if (!segmentExtensions.empty()) {
                const auto pasted = std::find_if(
                    previewLane.segments.begin(),
                    previewLane.segments.end(),
                    [segmentStart, segmentEnd, &projectedValue](
                        const Segment& candidate) {
                        return candidate.start <= segmentStart
                            && candidate.end >= segmentEnd
                            && candidate.value == projectedValue;
                    });
                if (pasted != previewLane.segments.end()) {
                    pasted->extensions = std::move(segmentExtensions);
                }
            }
        }
        preview.waveformChanges =
            preview.waveformChanges
            || !sameWaveform(previewLane, *target);
        preview.lanes.push_back(std::move(previewLane));
    }

    auto relationImpact =
        relationRemovalImpactForProjectedLanes(preview.lanes);
    preview.relationRemovalIds = std::move(relationImpact.ids);
    preview.relationRemovalEndpointSummaries =
        std::move(relationImpact.endpointSummaries);
    preview.relationRemovalSummaries =
        std::move(relationImpact.summaries);
    preview.relationRemovalCount = preview.relationRemovalIds.size();
    preview.modelChanges =
        preview.waveformChanges
        || preview.relationRemovalCount > 0
        || preview.start + preview.duration > scenario_->duration;
    return preview;
}

std::optional<WaveCanvas::LaneHeaderPastePreview>
WaveCanvas::buildRepeatPreview() const
{
    if (!scenario_ || !project_ || tool_ != Tool::WaveEdit
        || !explicitRangeSelection_ || !selectionRange_
        || selectionRange_->second <= selectionRange_->first
        || selectedLaneIds_.empty()) {
        return std::nullopt;
    }
    const auto [sourceStart, sourceEnd] = *selectionRange_;
    const auto duration = sourceEnd - sourceStart;
    const auto destination = sourceEnd;
    if (destination < 0
        || duration > std::numeric_limits<Tick>::max() - destination) {
        return std::nullopt;
    }
    auto copiedLanes = captureLaneRanges(
        *scenario_,
        selectedLaneIds_,
        sourceStart,
        sourceEnd,
        false);
    if (copiedLanes.empty()
        || copiedLanes.size() != selectedLaneIds_.size()) {
        return std::nullopt;
    }
    return buildCopiedRangePreview(
        copiedLanes,
        destination,
        duration,
        RangePreviewOperation::Repeat);
}

std::optional<WaveCanvas::LaneHeaderPastePreview>
WaveCanvas::buildCopiedRangePreview(
    const std::vector<CopiedLaneRange>& copiedLanes,
    const Tick destination,
    const Tick duration,
    const RangePreviewOperation operation) const
{
    if (!scenario_ || !project_ || tool_ != Tool::WaveEdit
        || copiedLanes.empty() || destination < 0 || duration <= 0
        || destination > scenario_->duration
        || duration > std::numeric_limits<Tick>::max() - destination) {
        return std::nullopt;
    }

    std::unordered_set<std::string> reservedSegmentIds;
    for (const auto& lane : scenario_->lanes) {
        for (const auto& segment : lane.segments) {
            reservedSegmentIds.insert(segment.id);
        }
    }
    std::size_t previewSegmentSequence = 0;
    const auto nextPreviewSegmentId =
        [&previewSegmentSequence, &reservedSegmentIds] {
            std::string candidate;
            do {
                candidate = std::string{"__range_preview_segment_"}
                    + std::to_string(++previewSegmentSequence);
            } while (reservedSegmentIds.contains(candidate));
            reservedSegmentIds.insert(candidate);
            return candidate;
        };
    const auto normalizePreviewSegments = [](Lane& lane) {
        std::sort(
            lane.segments.begin(),
            lane.segments.end(),
            [](const Segment& left, const Segment& right) {
                return left.start == right.start
                    ? left.end < right.end
                    : left.start < right.start;
            });
        std::vector<Segment> normalized;
        normalized.reserve(lane.segments.size());
        for (auto& candidate : lane.segments) {
            if (!normalized.empty()
                && normalized.back().end == candidate.start
                && normalized.back().value == candidate.value
                && normalized.back().extensions == candidate.extensions) {
                normalized.back().end = candidate.end;
            } else {
                normalized.push_back(std::move(candidate));
            }
        }
        lane.segments = std::move(normalized);
    };
    const auto clearPreviewRange =
        [&nextPreviewSegmentId, &normalizePreviewSegments](
            Lane& lane,
            const Tick start,
            const Tick end) {
            std::vector<Segment> retained;
            retained.reserve(lane.segments.size() + 1);
            for (const auto& existing : lane.segments) {
                if (existing.end <= start || existing.start >= end) {
                    retained.push_back(existing);
                    continue;
                }
                if (existing.start < start) {
                    auto left = existing;
                    left.end = start;
                    retained.push_back(std::move(left));
                }
                if (existing.end > end) {
                    auto right = existing;
                    right.id = nextPreviewSegmentId();
                    right.start = end;
                    retained.push_back(std::move(right));
                }
            }
            lane.segments = std::move(retained);
            normalizePreviewSegments(lane);
        };
    const auto insertPreviewSegment =
        [&nextPreviewSegmentId, &normalizePreviewSegments](
            Lane& lane,
            Segment segment) {
            std::vector<Segment> replaced;
            replaced.reserve(lane.segments.size() + 2);
            for (const auto& existing : lane.segments) {
                if (existing.end <= segment.start
                    || existing.start >= segment.end) {
                    replaced.push_back(existing);
                    continue;
                }
                if (existing.start < segment.start) {
                    auto left = existing;
                    left.end = segment.start;
                    replaced.push_back(std::move(left));
                }
                if (existing.end > segment.end) {
                    auto right = existing;
                    right.id = nextPreviewSegmentId();
                    right.start = segment.end;
                    replaced.push_back(std::move(right));
                }
            }
            segment.id = nextPreviewSegmentId();
            replaced.push_back(std::move(segment));
            lane.segments = std::move(replaced);
            normalizePreviewSegments(lane);
        };
    const auto sameWaveform = [](const Lane& left, const Lane& right) {
        return left.segments.size() == right.segments.size()
            && std::equal(
                left.segments.begin(),
                left.segments.end(),
                right.segments.begin(),
                [](const Segment& leftSegment,
                   const Segment& rightSegment) {
                    return leftSegment.start == rightSegment.start
                        && leftSegment.end == rightSegment.end
                        && leftSegment.value == rightSegment.value
                        && leftSegment.extensions
                            == rightSegment.extensions;
                });
    };

    LaneHeaderPastePreview preview;
    preview.operation = operation;
    preview.start = destination;
    preview.duration = duration;
    preview.lanes.reserve(copiedLanes.size());
    for (const auto& copiedLane : copiedLanes) {
        const auto* target = findLane(*scenario_, copiedLane.laneId);
        if (!target || target->kind == LaneKind::Group) {
            return std::nullopt;
        }

        auto previewLane = *target;
        clearPreviewRange(
            previewLane,
            preview.start,
            preview.start + preview.duration);
        previewLane.color =
            laneColor(*target).lighter(145).name(QColor::HexRgb).toStdString();
        for (const auto& relative : copiedLane.relativeSegments) {
            if (relative.start < 0
                || relative.end <= relative.start
                || relative.end > duration) {
                return std::nullopt;
            }
            const auto validation =
                validateLaneValue(*target, relative.value);
            if (!validation.valid) return std::nullopt;
            Segment segment;
            segment.start = preview.start + relative.start;
            segment.end = preview.start + relative.end;
            segment.value = validation.normalizedValue;
            const auto segmentStart = segment.start;
            const auto segmentEnd = segment.end;
            const auto projectedValue = segment.value;
            auto segmentExtensions = relative.extensions;
            insertPreviewSegment(previewLane, std::move(segment));
            if (!segmentExtensions.empty()) {
                const auto pasted = std::find_if(
                    previewLane.segments.begin(),
                    previewLane.segments.end(),
                    [segmentStart, segmentEnd, &projectedValue](
                        const Segment& candidate) {
                        return candidate.start <= segmentStart
                            && candidate.end >= segmentEnd
                            && candidate.value == projectedValue;
                    });
                if (pasted != previewLane.segments.end()) {
                    pasted->extensions = std::move(segmentExtensions);
                }
            }
        }
        preview.waveformChanges =
            preview.waveformChanges
            || !sameWaveform(previewLane, *target);
        preview.lanes.push_back(std::move(previewLane));
    }

    auto relationImpact =
        relationRemovalImpactForProjectedLanes(preview.lanes);
    preview.relationRemovalIds = std::move(relationImpact.ids);
    preview.relationRemovalEndpointSummaries =
        std::move(relationImpact.endpointSummaries);
    preview.relationRemovalSummaries =
        std::move(relationImpact.summaries);
    preview.relationRemovalCount = preview.relationRemovalIds.size();
    preview.modelChanges =
        preview.waveformChanges
        || preview.relationRemovalCount > 0
        || preview.start + preview.duration > scenario_->duration;
    return preview;
}

WaveCanvas::RelationRemovalImpact
WaveCanvas::relationRemovalImpactForProjectedLanes(
    const std::vector<Lane>& projectedLanes) const
{
    RelationRemovalImpact impact;
    if (!scenario_ || !project_) return impact;

    std::unordered_set<std::string> removedEventIds;
    for (const auto& projectedLane : projectedLanes) {
        if (projectedLane.kind != LaneKind::Bit
            && projectedLane.kind != LaneKind::Bus
            && projectedLane.kind != LaneKind::Enum) {
            continue;
        }
        std::vector<const Event*> linkedEvents;
        for (const auto& event : scenario_->events) {
            if (event.waveformLinked && event.laneId == projectedLane.id) {
                linkedEvents.push_back(&event);
            }
        }
        std::vector<bool> reused(linkedEvents.size(), false);
        const auto reusable =
            [&linkedEvents, &reused](const auto& predicate) {
                for (std::size_t eventIndex = 0;
                     eventIndex < linkedEvents.size();
                     ++eventIndex) {
                    if (!reused[eventIndex]
                        && predicate(*linkedEvents[eventIndex])) {
                        return eventIndex;
                    }
                }
                return linkedEvents.size();
            };
        for (const auto& segment : projectedLane.segments) {
            auto eventIndex = reusable([&segment](const Event& event) {
                return event.linkedSegmentId == segment.id;
            });
            if (eventIndex == linkedEvents.size()) {
                eventIndex = reusable([&segment](const Event& event) {
                    return event.tick == segment.start
                        && event.value == segment.value;
                });
            }
            if (eventIndex == linkedEvents.size()) {
                eventIndex = reusable([&segment](const Event& event) {
                    return event.tick == segment.start;
                });
            }
            if (eventIndex != linkedEvents.size()) {
                reused[eventIndex] = true;
            }
        }
        for (std::size_t eventIndex = 0;
             eventIndex < linkedEvents.size();
             ++eventIndex) {
            if (!reused[eventIndex]) {
                removedEventIds.insert(linkedEvents[eventIndex]->id);
            }
        }
    }
    const auto endpointSummary = [this](const std::string& eventId) {
        const auto* event = findEvent(*scenario_, eventId);
        if (!event) {
            return tr("missing event %1")
                .arg(QString::fromStdString(eventId));
        }
        const auto* lane = findLane(*scenario_, event->laneId);
        const auto laneName = lane
            ? QString::fromStdString(lane->name)
            : event->laneId.empty()
                ? tr("unassigned event")
                : QString::fromStdString(event->laneId);
        return tr("%1 @ %2")
            .arg(
                laneName,
                QString::fromStdString(
                    formatTick(event->tick, project_->timeBase)));
    };
    for (const auto& relation : scenario_->relations) {
        if (!removedEventIds.contains(relation.sourceEventId)
            && !removedEventIds.contains(relation.targetEventId)) {
            continue;
        }
        impact.ids.push_back(relation.id);
        const auto endpoints = tr("%1 → %2")
            .arg(
                endpointSummary(relation.sourceEventId),
                endpointSummary(relation.targetEventId));
        impact.endpointSummaries.append(endpoints);
        const auto description =
            QString::fromStdString(relation.description).trimmed();
        impact.summaries.append(
            description.isEmpty()
                ? endpoints
                : tr("%1 · %2").arg(description, endpoints));
    }
    return impact;
}

QString WaveCanvas::laneHeaderPasteHint() const
{
    if (!scenario_ || !project_ || !laneHeaderSelectionActive_
        || selectedLaneIds_.empty()) {
        return {};
    }
    const auto* mime = QApplication::clipboard()->mimeData();
    if (!mime || !mime->hasFormat(kRangeMimeType)) return {};

    const auto [enabled, toolTip] = selectedLanePasteAvailability();
    if (enabled) {
        auto details = toolTip.section(QLatin1Char('\n'), 1);
        details.replace(QLatin1Char('\n'), QStringLiteral(" · "));
        return tr("Paste preview shown · Ctrl+V pastes copied range at %1 · %2")
            .arg(
                QString::fromStdString(
                    formatTick(cursorTick_, project_->timeBase)),
                details);
    }
    auto reason = toolTip;
    const auto prefix = tr("Paste unavailable · ");
    if (reason.startsWith(prefix)) {
        reason.replace(0, prefix.size(), tr("Ctrl+V unavailable · "));
    }
    return reason;
}

WaveCanvas::SegmentBoundary WaveCanvas::explicitRangeBoundaryAt(
    const QPoint& position) const
{
    if (!scenario_ || !explicitRangeSelection_ || !selectionRange_
        || position.x() < headerWidth_) {
        return SegmentBoundary::None;
    }
    const auto selectedLane = std::any_of(
        laneLayout_.begin(),
        laneLayout_.end(),
        [this, &position](const LaneLayout& layout) {
            const auto& lane = scenario_->lanes.at(layout.laneIndex);
            if (std::find(selectedLaneIds_.begin(), selectedLaneIds_.end(), lane.id)
                == selectedLaneIds_.end()) {
                return false;
            }
            const auto top = RulerHeight + layout.top - verticalScrollBar()->value();
            return position.y() >= top && position.y() < top + layout.height;
        });
    if (!selectedLane) return SegmentBoundary::None;

    const auto startDistance = std::abs(position.x() - xAtTick(selectionRange_->first));
    const auto endDistance = std::abs(position.x() - xAtTick(selectionRange_->second));
    if (startDistance > kSoftSnapRadiusPixels
        && endDistance > kSoftSnapRadiusPixels) {
        return SegmentBoundary::None;
    }
    return startDistance <= endDistance
        ? SegmentBoundary::Start
        : SegmentBoundary::End;
}

bool WaveCanvas::explicitRangeContains(const QPoint& position) const
{
    if (!scenario_ || !explicitRangeSelection_ || !selectionRange_
        || position.x() < headerWidth_
        || position.x() < xAtTick(selectionRange_->first)
        || position.x() > xAtTick(selectionRange_->second)) {
        return false;
    }
    const auto* lane = laneAtY(position.y());
    return lane
        && std::find(
               selectedLaneIds_.begin(),
               selectedLaneIds_.end(),
               lane->id)
            != selectedLaneIds_.end();
}

bool WaveCanvas::hasBitRangeSelection() const
{
    if (!scenario_ || tool_ != Tool::WaveEdit || explicitRangeSelection_
        || !selectionRange_ || selectionRange_->second <= selectionRange_->first
        || !selectedSegmentId_.empty() || selectedLaneId_.empty()) {
        return false;
    }
    const auto* lane = findLane(*scenario_, selectedLaneId_);
    return lane
        && lane->visible
        && lane->kind == LaneKind::Bit
        && selectionRange_->first >= 0
        && selectionRange_->second <= scenario_->duration;
}

void WaveCanvas::showRangeEditPalette()
{
    hideBusPresetPalette();
    if (!rangeEditPalette_ || !scenario_ || !explicitRangeSelection_
        || !selectionRange_ || selectionRange_->second <= selectionRange_->first
        || selectedLaneIds_.empty()) {
        hideRangeEditPalette();
        return;
    }

    const auto kind = explicitRangeKind();
    const auto bitRange = kind && *kind == LaneKind::Bit;
    const auto busRange = kind && *kind == LaneKind::Bus;
    const auto enumRange = kind && *kind == LaneKind::Enum;
    const auto clockRange = kind && *kind == LaneKind::Clock;
    const auto editable = clockRange || bitRange || busRange || enumRange;
    const auto presetRange = bitRange || busRange;
    const auto bitPatternRange = bitRange;
    const auto valueRange =
        bitPatternRange || busRange || enumRange;
    const auto enumSymbols = enumRange
        ? explicitRangeEnumSymbols()
        : QStringList{};
    const auto clearAvailability = rangeClearAvailability();
    const auto repeatAvailability = rangeRepeatAvailability();
    if (rangeValueCompletionModel_) {
        rangeValueCompletionModel_->setStringList(enumSymbols);
    }
    const auto count = static_cast<qulonglong>(selectedLaneIds_.size());
    const auto format = [this](const Tick tick) {
        return project_
            ? QString::fromStdString(formatTick(tick, project_->timeBase))
            : QString::number(tick);
    };
    if (rangeEditContextLabel_) {
        rangeEditContextLabel_->setText(
            editable
                ? tr("%1 %2 · %3–%4")
                      .arg(count)
                      .arg(
                          clockRange ? tr("Clock")
                          : bitRange ? tr("Bit")
                          : busRange ? tr("Bus")
                                     : tr("Enum"))
                      .arg(format(selectionRange_->first))
                      .arg(format(selectionRange_->second))
                : tr("Mixed/unsupported selection · Copy, repeat, paste, or clear"));
        auto contextHelp = editable
            ? tr("Applies to every selected signal from %1 to %2")
                  .arg(format(selectionRange_->first))
                  .arg(format(selectionRange_->second))
            : tr("Batch assignment requires only Clock, only Bit, only Bus, or only Enum signals");
        contextHelp.prepend(
            tr("Click to type the exact active time edge (Ctrl+G)\n"));
        contextHelp.append(
            tr("\nCtrl+D repeats the complete selection immediately after it without changing the clipboard"
               "\nDrag inside the selection in time or onto compatible signals to move it; Ctrl+drag copies it; press or release Ctrl while dragging to switch Move/Copy"
               "\nShift+Up/Down adjusts signals; Shift+Left/Right adjusts time; "
               "Shift+Home/End selects to a timeline boundary; "
               "Ctrl+Shift+Left/Right selects to signal edges; "
               "Ctrl+A selects the full timeline"));
        contextHelp.append(
            tr("\nKeyboard time step: %1").arg(editTimingSummary()));
        contextHelp.append(
            tr("\nPaste starts at the selected left edge and uses the copied width. Compatible clipboard content appears as a dashed waveform before commit; affected Relations are highlighted in amber."));
        contextHelp.append(
            tr("\n%1").arg(repeatAvailability.toolTip));
        contextHelp.append(
            tr("\n%1").arg(clearAvailability.toolTip));
        if (enumRange) {
            contextHelp.append(
                enumSymbols.isEmpty()
                    ? tr("\nNo declared symbols are shared by every selected Enum signal")
                    : tr("\nShared symbols: %1")
                          .arg(enumSymbols.join(QStringLiteral(", "))));
            contextHelp.append(
                selectedLaneIds_.size() > 1
                    ? tr("\nType one symbol/value for the whole range, one shared beat sequence, or use / to map one sequence per selected signal in top-to-bottom order. Use value*N for a run. A shorter sequence repeats only when it fills the shared beat range exactly. The load icon in the value field inserts safely representable current beat values for editing.")
                    : tr("\nType one symbol/value for the whole range or a beat sequence. Use value*N for a run. A shorter sequence repeats only when it fills the selected beat range exactly. The load icon in the value field inserts safely representable current beat values for editing."));
        } else if (bitPatternRange) {
            contextHelp.append(
                selectedLaneIds_.size() > 1
                    ? tr("\nType one shared 0/1/X/Z pattern, or use / to map one pattern per selected signal in top-to-bottom order, for example 01 / 0011. Use symbol*N for a run, for example 0*8. A shorter pattern repeats only when it fills the shared beat range exactly.")
                    : tr("\nType one 0/1/X/Z pattern. Separators are optional; use symbol*N for a run, for example 0*8. A shorter pattern repeats only when it fills the selected beat range exactly."));
        } else if (busRange) {
            contextHelp.append(
                selectedLaneIds_.size() > 1
                    ? tr("\nType one value for the whole range, one shared beat sequence, or use / to map one sequence per selected signal in top-to-bottom order. Use value*N for a run, for example 0x00*4. A shorter sequence repeats only when it fills the shared beat range exactly. The load icon in the value field inserts safely representable current beat values for editing.")
                    : tr("\nType one value for the whole range or a beat sequence. Use value*N for a run, for example 0x00*4. A shorter sequence repeats only when it fills the selected beat range exactly. The load icon in the value field inserts safely representable current beat values for editing."));
        } else if (clockRange) {
            contextHelp.append(
                tr("\nRun clears explicit overrides; Gate and Disable apply to the complete selected range · R/G/X"));
        }
        rangeEditContextLabel_->setToolTip(contextHelp);
    }

    if (rangeCopyButton_) {
        rangeCopyButton_->setVisible(true);
        rangeCopyButton_->setEnabled(true);
    }
    if (rangeRepeatButton_) {
        rangeRepeatButton_->setVisible(true);
        rangeRepeatButton_->setEnabled(repeatAvailability.enabled);
        const auto relationRemovalCount =
            repeatAvailability.relationImpact.ids.size();
        const auto relationRisk = relationRemovalCount > 0;
        rangeRepeatButton_->setText(
            relationRisk
                ? tr("Repeat ⚠%1")
                      .arg(static_cast<qulonglong>(
                          relationRemovalCount))
                : tr("Repeat"));
        if (rangeRepeatButton_->property("relationRisk").toBool()
            != relationRisk) {
            rangeRepeatButton_->setProperty(
                "relationRisk",
                relationRisk);
            rangeRepeatButton_->style()->unpolish(rangeRepeatButton_);
            rangeRepeatButton_->style()->polish(rangeRepeatButton_);
            rangeRepeatButton_->update();
        }
        rangeRepeatButton_->setToolTip(repeatAvailability.toolTip);
        rangeRepeatButton_->setAccessibleName(
            relationRisk
                ? tr("Repeat selected range after; warning: removes %1 relation(s)")
                      .arg(static_cast<qulonglong>(
                          relationRemovalCount))
                : tr("Repeat selected range after"));
        rangeRepeatButton_->setAccessibleDescription(
            repeatAvailability.toolTip);
    }
    if (rangePasteButton_) {
        const auto availability = rangePasteAvailability();
        rangePasteButton_->setVisible(true);
        rangePasteButton_->setEnabled(availability.enabled);
        rangePasteButton_->setToolTip(availability.toolTip);
    }
    if (rangeClearButton_) {
        rangeClearButton_->setVisible(true);
        rangeClearButton_->setEnabled(clearAvailability.enabled);
        const auto relationRemovalCount =
            clearAvailability.relationImpact.ids.size();
        rangeClearButton_->setText(
            relationRemovalCount > 0
                ? tr("%1 ⚠%2")
                      .arg(clockRange ? tr("Run") : tr("Clear"))
                      .arg(static_cast<qulonglong>(
                          relationRemovalCount))
                : clockRange ? tr("Run") : tr("Clear"));
        const auto relationRisk = relationRemovalCount > 0;
        if (rangeClearButton_->property("relationRisk").toBool()
            != relationRisk) {
            rangeClearButton_->setProperty("relationRisk", relationRisk);
            rangeClearButton_->style()->unpolish(rangeClearButton_);
            rangeClearButton_->style()->polish(rangeClearButton_);
            rangeClearButton_->update();
        }
        rangeClearButton_->setAccessibleName(
            clockRange
                ? relationRemovalCount > 0
                    ? tr("Restore selected Clock range to Run; warning: removes %1 relation(s)")
                          .arg(static_cast<qulonglong>(
                              relationRemovalCount))
                    : tr("Restore selected Clock range to Run")
                : relationRemovalCount > 0
                    ? tr("Clear selected range; warning: removes %1 relation(s)")
                          .arg(static_cast<qulonglong>(
                              relationRemovalCount))
                    : tr("Clear selected range"));
        rangeClearButton_->setToolTip(clearAvailability.toolTip);
    }
    const auto zeroAvailability = bitRange
        ? rangeValueAvailability("0")
        : busRange ? rangeValueAvailability({}, "zero")
                   : RangeValueAvailability{};
    const auto oneAvailability = bitRange
        ? rangeValueAvailability("1")
        : RangeValueAvailability{};
    const auto xAvailability = bitRange
        ? rangeValueAvailability("X")
        : busRange ? rangeValueAvailability({}, "x")
                   : RangeValueAvailability{};
    const auto zAvailability = bitRange
        ? rangeValueAvailability("Z")
        : busRange ? rangeValueAvailability({}, "z")
                   : RangeValueAvailability{};
    const auto dontCareAvailability = busRange
        ? rangeValueAvailability({}, "dont-care")
        : RangeValueAvailability{};
    const auto reservedAvailability = busRange
        ? rangeValueAvailability({}, "reserved")
        : RangeValueAvailability{};
    const auto clockGateAvailability = clockRange
        ? rangeValueAvailability("gated")
        : RangeValueAvailability{};
    const auto clockDisableAvailability = clockRange
        ? rangeValueAvailability("disabled")
        : RangeValueAvailability{};
    const auto configureValueButton =
        [this](
            QToolButton* button,
            const bool visible,
            const QString& baseText,
            const QString& shortcut,
            const RangeValueAvailability& availability) {
            if (!button) return;
            button->setVisible(visible);
            if (!visible) {
                button->setEnabled(false);
                if (button->property("relationRisk").toBool()) {
                    button->setProperty("relationRisk", false);
                    button->style()->unpolish(button);
                    button->style()->polish(button);
                    button->update();
                }
                button->setText(baseText);
                return;
            }
            const auto relationRemovalCount =
                availability.relationImpact.ids.size();
            const auto relationRisk = relationRemovalCount > 0;
            button->setEnabled(
                availability.valid && availability.enabled);
            button->setText(
                relationRisk
                    ? tr("%1 ⚠%2")
                          .arg(baseText)
                          .arg(static_cast<qulonglong>(
                              relationRemovalCount))
                    : baseText);
            if (button->property("relationRisk").toBool()
                != relationRisk) {
                button->setProperty("relationRisk", relationRisk);
                button->style()->unpolish(button);
                button->style()->polish(button);
                button->update();
            }
            auto toolTip = availability.toolTip;
            if (!shortcut.isEmpty()) {
                toolTip += tr("\nShortcut: %1").arg(shortcut);
            }
            button->setToolTip(toolTip);
            button->setAccessibleName(
                relationRisk
                    ? tr("Set selected range to %1; warning: removes %2 relation(s)")
                          .arg(baseText)
                          .arg(static_cast<qulonglong>(
                              relationRemovalCount))
                    : tr("Set selected range to %1").arg(baseText));
            button->setAccessibleDescription(toolTip);
        };
    configureValueButton(
        rangeZeroButton_,
        presetRange,
        QStringLiteral("0"),
        QStringLiteral("0"),
        zeroAvailability);
    configureValueButton(
        rangeOneButton_,
        bitRange,
        QStringLiteral("1"),
        QStringLiteral("1"),
        oneAvailability);
    configureValueButton(
        rangeXButton_,
        presetRange,
        QStringLiteral("X"),
        QStringLiteral("X"),
        xAvailability);
    configureValueButton(
        rangeZButton_,
        presetRange,
        QStringLiteral("Z"),
        QStringLiteral("Z"),
        zAvailability);
    configureValueButton(
        rangeDontCareButton_,
        busRange,
        QStringLiteral("DC"),
        {},
        dontCareAvailability);
    configureValueButton(
        rangeReservedButton_,
        busRange,
        QStringLiteral("R"),
        {},
        reservedAvailability);
    if (busRange && rangeDontCareButton_) {
        rangeDontCareButton_->setAccessibleName(
            tr("Set selected range to Don't care"));
    }
    if (busRange && rangeReservedButton_) {
        rangeReservedButton_->setAccessibleName(
            tr("Set selected range to Reserved"));
    }
    configureValueButton(
        rangeClockGateButton_,
        clockRange,
        tr("Gate"),
        QStringLiteral("G"),
        clockGateAvailability);
    configureValueButton(
        rangeClockDisableButton_,
        clockRange,
        tr("Disable"),
        QStringLiteral("X"),
        clockDisableAvailability);
    if (rangeValueEdit_) {
        const auto restoreCanvasFocus = rangeValueEdit_->hasFocus() && !valueRange;
        rangeValueEdit_->setVisible(valueRange);
        rangeValueEdit_->setMinimumWidth(
            bitPatternRange ? 140 : 90);
        rangeValueEdit_->setMaximumWidth(
            bitPatternRange ? 180 : 120);
        const auto setDraftProperty =
            [this](const char* name, const bool value) {
                if (!rangeValueEdit_
                    || rangeValueEdit_->property(name).toBool() == value) {
                    return;
                }
                rangeValueEdit_->setProperty(name, value);
                rangeValueEdit_->style()->unpolish(rangeValueEdit_);
                rangeValueEdit_->style()->polish(rangeValueEdit_);
                rangeValueEdit_->update();
            };
        RangeSequenceSeed sequenceSeed;
        if (!valueRange) {
            bitPatternPreview_.reset();
            busRangeSequencePreview_.reset();
            rangeValueEdit_->clear();
            rangeValueEdit_->setModified(false);
            rangeValueEdit_->setStyleSheet({});
            rangeValueEdit_->setToolTip({});
            setDraftProperty("relationRisk", false);
            setDraftProperty("noEffect", false);
            setDraftProperty("invalidDraft", false);
            setDraftProperty("loadedExisting", false);
            rangeSequenceTextSpans_.clear();
            clearRangeSequenceBaseline();
            clearRangeSequenceAnchor();
            clearRangeSequenceCaretTarget();
            if (rangeLoadValuesAction_) {
                rangeLoadValuesAction_->setVisible(false);
                rangeLoadValuesAction_->setEnabled(false);
            }
        } else {
            rangeValueEdit_->setAccessibleName(
                bitPatternRange
                    ? tr("Selected Bit range pattern")
                : enumRange
                    ? tr("Selected Enum range value")
                    : tr("Selected Bus range value"));
            rangeValueEdit_->setPlaceholderText(
                bitPatternRange
                    ? selectedLaneIds_.size() > 1
                        ? tr("01 or 0*4 / 1*4 + Enter")
                        : tr("01XZ or 0*8 + Enter")
                : enumRange
                    ? tr("Symbol/list \u00b7 *N + Enter")
                    : tr("Value/list \u00b7 *N + Enter"));
            const auto preserveLoadedBaseline =
                rangeValueEdit_->property(
                    "loadedExisting").toBool()
                && rangeSequenceBaselineMatchesContext();
            if (rangeValueEdit_->property(
                    "loadedExisting").toBool()
                && !preserveLoadedBaseline) {
                rangeValueEdit_->setText({});
                rangeValueEdit_->setModified(false);
                setDraftProperty("loadedExisting", false);
                rangeSequenceTextSpans_.clear();
                busRangeSequencePreview_.reset();
                clearRangeSequenceBaseline();
                clearRangeSequenceAnchor();
                clearRangeSequenceCaretTarget();
            } else if (rangeSequenceBaseline_
                       && !rangeSequenceBaselineMatchesContext()) {
                clearRangeSequenceBaseline();
                clearRangeSequenceAnchor();
                clearRangeSequenceCaretTarget();
            }
            if (!rangeValueEdit_->hasFocus()
                && !rangeValueEdit_->isModified()
                && !preserveLoadedBaseline) {
                rangeValueEdit_->clear();
                rangeValueEdit_->setModified(false);
                setDraftProperty("loadedExisting", false);
                rangeSequenceTextSpans_.clear();
                clearRangeSequenceBaseline();
                clearRangeSequenceAnchor();
                clearRangeSequenceCaretTarget();
            }
            if ((busRange || enumRange)
                && !rangeValueEdit_->isModified()) {
                sequenceSeed =
                    currentRangeSequenceSeed();
            }
            const auto enteredDraft =
                rangeValueEdit_->text();
            const auto draft =
                enteredDraft.trimmed();
            if (rangeValueEdit_->isModified() && !draft.isEmpty()) {
                setDraftProperty("loadedExisting", false);
                if (bitPatternRange) {
                    rangeSequenceTextSpans_.clear();
                    clearRangeSequenceCaretTarget();
                    busRangeSequencePreview_.reset();
                    const auto assessment =
                        assessBitPatternDraft(draft);
                    const auto relationRisk =
                        assessment.projection
                        && !assessment.projection
                                ->relationImpact.ids.empty();
                    const auto noEffect =
                        assessment.valid && !assessment.enabled;
                    setDraftProperty("relationRisk", relationRisk);
                    setDraftProperty("noEffect", noEffect);
                    setDraftProperty("invalidDraft", !assessment.valid);
                    bitPatternPreview_ =
                        assessment.valid
                        && assessment.projection
                        ? assessment.projection
                        : std::nullopt;
                    rangeValueEdit_->setStyleSheet({});
                    rangeValueEdit_->setToolTip(
                        assessment.summary
                        + (assessment.valid
                               ? tr("\nEsc discards this draft")
                               : tr("\nCorrect the pattern before pressing Enter")));
                    rangeValueEdit_->setAccessibleDescription(
                        rangeValueEdit_->toolTip());
                } else {
                    bitPatternPreview_.reset();
                    const auto sequenceAssessment =
                        assessBusRangeSequenceDraft(
                            enteredDraft);
                    if (sequenceAssessment.sequence) {
                        const auto relationRisk =
                            sequenceAssessment.projection
                            && !sequenceAssessment.projection
                                    ->relationImpact.ids.empty();
                        const auto noEffect =
                            sequenceAssessment.valid
                            && !sequenceAssessment.enabled;
                        setDraftProperty(
                            "relationRisk",
                            relationRisk);
                        setDraftProperty(
                            "noEffect",
                            noEffect);
                        setDraftProperty(
                            "invalidDraft",
                            !sequenceAssessment.valid);
                        busRangeSequencePreview_ =
                            sequenceAssessment.valid
                            && sequenceAssessment.projection
                            ? sequenceAssessment.projection
                            : std::nullopt;
                        rangeSequenceTextSpans_ =
                            sequenceAssessment.valid
                            ? sequenceAssessment.textSpans
                            : std::vector<
                                  RangeSequenceTextSpan>{};
                        if (!sequenceAssessment.valid) {
                            clearRangeSequenceCaretTarget();
                        }
                        rangeValueEdit_->setStyleSheet({});
                        rangeValueEdit_->setToolTip(
                            sequenceAssessment.summary
                            + (sequenceAssessment.valid
                                   ? tr("\nClick a preview beat to select its text token\nTab and Shift+Tab move between mapped token targets\nEsc discards this draft")
                                   : tr("\nCorrect the sequence before pressing Enter")));
                        rangeValueEdit_->setAccessibleDescription(
                            rangeValueEdit_->toolTip());
                    } else {
                        rangeSequenceTextSpans_.clear();
                        clearRangeSequenceCaretTarget();
                        busRangeSequencePreview_.reset();
                        const auto availability =
                            rangeValueAvailability(
                                draft.toStdString());
                        const auto relationRisk =
                            !availability.relationImpact.ids.empty();
                        const auto noEffect =
                            availability.valid
                            && !availability.enabled;
                        setDraftProperty(
                            "relationRisk",
                            relationRisk);
                        setDraftProperty(
                            "noEffect",
                            noEffect);
                        setDraftProperty(
                            "invalidDraft",
                            !availability.valid);
                        rangeValueEdit_->setStyleSheet({});
                        rangeValueEdit_->setToolTip(
                            availability.toolTip
                            + (availability.valid
                                   ? tr("\nEnter applies; Esc discards this draft")
                                   : tr("\nCorrect the value before pressing Enter")));
                        rangeValueEdit_->setAccessibleDescription(
                            rangeValueEdit_->toolTip());
                    }
                }
            } else if (!rangeValueEdit_->isModified()) {
                bitPatternPreview_.reset();
                busRangeSequencePreview_.reset();
                setDraftProperty("relationRisk", false);
                setDraftProperty("noEffect", false);
                setDraftProperty("invalidDraft", false);
                rangeValueEdit_->setToolTip(
                    bitPatternRange
                        ? selectedLaneIds_.size() > 1
                            ? tr("Type one shared 0/1/X/Z pattern, or separate one pattern per selected signal with / in top-to-bottom order (for example 01 / 0011). Use symbol*N for a run (for example 0*8). Spaces, commas, underscores, and 0b are accepted inside each pattern. All targets must share one beat grid, and a shorter pattern repeats only when it fills the range exactly.")
                            : tr("Type one 0/1/X/Z pattern; use symbol*N for a run (for example 0*8). Spaces, commas, underscores, and 0b are accepted. A shorter pattern repeats only when it fills the range exactly.")
                    : enumRange
                        ? enumSymbols.isEmpty()
                            ? tr("Type one numeric value for the whole range, or a beat sequence; use value*N for repeats and / for per-signal mappings")
                            : tr("Type one shared symbol/value or a beat sequence; use value*N for repeats and / for per-signal mappings · symbols: %1")
                                  .arg(enumSymbols.join(QStringLiteral(", ")))
                        : tr("Type one value for the whole Bus range, or a beat sequence; use value*N for repeats and / for per-signal mappings"));
                rangeValueEdit_->setStyleSheet({});
                if (rangeValueEdit_->property(
                        "loadedExisting").toBool()
                    && sequenceSeed.available) {
                    rangeValueEdit_->setToolTip(
                        sequenceSeed.summary
                        + tr("\nCurrent values are loaded but unchanged; edit the text, then press Enter")
                        + tr("\nClick a preview beat to select its text token")
                        + tr("\nTab and Shift+Tab move between mapped token targets")
                        + tr("\nEsc hides the loaded values; Esc again closes the selection"));
                } else {
                    if ((busRange || enumRange)
                        && draft.isEmpty()) {
                        rangeValueEdit_->setToolTip(
                            rangeValueEdit_->toolTip()
                            + (sequenceSeed.available
                                   ? tr("\nUse the trailing load button to edit the current beat values")
                                   : tr("\n%1").arg(
                                       sequenceSeed.summary)));
                    }
                    rangeValueEdit_->setToolTip(
                        rangeValueEdit_->toolTip()
                        + tr("\nEsc discards a draft; Esc again closes the selection"));
                }
                rangeValueEdit_->setAccessibleDescription(
                    rangeValueEdit_->toolTip());
            } else {
                rangeSequenceTextSpans_.clear();
                clearRangeSequenceCaretTarget();
                bitPatternPreview_.reset();
                busRangeSequencePreview_.reset();
                setDraftProperty("relationRisk", false);
                setDraftProperty("noEffect", false);
                setDraftProperty("invalidDraft", false);
            }
            if (rangeLoadValuesAction_) {
                const auto loadedExisting =
                    rangeValueEdit_->property(
                        "loadedExisting").toBool();
                const auto showLoadAction =
                    (busRange || enumRange)
                    && !rangeValueEdit_->isModified()
                    && draft.isEmpty()
                    && !loadedExisting;
                rangeLoadValuesAction_->setVisible(
                    showLoadAction);
                rangeLoadValuesAction_->setEnabled(
                    showLoadAction
                    && sequenceSeed.available);
                rangeLoadValuesAction_->setToolTip(
                    sequenceSeed.summary);
                rangeLoadValuesAction_->setStatusTip(
                    sequenceSeed.summary);
            }
        }
        if (restoreCanvasFocus) viewport()->setFocus(Qt::OtherFocusReason);
    }
    rangeEditPalette_->adjustSize();
    rangeEditPalette_->updateGeometry();
    updateRangeSequenceCaretTarget(false);
    viewport()->update();
    if (!rangeEditPaletteVisible_) {
        rangeEditPaletteVisible_ = true;
        emit rangeEditPaletteVisibilityChanged(true);
    }
}

void WaveCanvas::hideRangeEditPalette()
{
    const auto restoreCanvasFocus = rangeValueEdit_ && rangeValueEdit_->hasFocus();
    rangeSequenceTokenPress_ = false;
    rangeSequenceTokenDragRejected_ = false;
    clearRangeSequenceBaseline();
    clearRangeSequenceAnchor();
    clearRangeSequenceCaretTarget();
    const auto repeatPreviewWasActive = rangeRepeatPreviewActive_;
    const auto bitPatternPreviewWasActive =
        bitPatternPreview_.has_value();
    const auto busRangeSequencePreviewWasActive =
        busRangeSequencePreview_.has_value();
    rangeRepeatPreviewActive_ = false;
    bitPatternPreview_.reset();
    busRangeSequencePreview_.reset();
    if (rangeEditPaletteVisible_) {
        rangeEditPaletteVisible_ = false;
        emit rangeEditPaletteVisibilityChanged(false);
    }
    if (restoreCanvasFocus) viewport()->setFocus(Qt::OtherFocusReason);
    if (repeatPreviewWasActive
        || bitPatternPreviewWasActive
        || busRangeSequencePreviewWasActive) {
        viewport()->update();
    }
}

void WaveCanvas::clearExplicitRangeSelection(const bool clearLanes)
{
    explicitRangeSelection_ = false;
    hideRangeEditPalette();
    rangeSequenceTextSpans_.clear();
    clearRangeSequenceCaretTarget();
    if (rangeValueEdit_) {
        rangeValueEdit_->setModified(false);
        rangeValueEdit_->setStyleSheet({});
    }
    bitPatternPreview_.reset();
    busRangeSequencePreview_.reset();
    selectionRange_.reset();
    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    if (clearLanes) {
        selectedLaneId_.clear();
        selectedLaneIds_.clear();
    }
    viewport()->update();
}

bool WaveCanvas::applyExplicitRangeValue(
    const std::string& value,
    const std::string& presetId)
{
    if (!scenario_ || !commandStack_ || !explicitRangeSelection_
        || !selectionRange_ || selectionRange_->second <= selectionRange_->first) {
        return false;
    }
    const auto kind = explicitRangeKind();
    if (!kind) {
        emit statusMessage(
            tr("No values changed · select only Clock, only Bit, only Bus, or only Enum signals"));
        return false;
    }

    std::vector<LaneRangeAssignment> assignments;
    assignments.reserve(selectedLaneIds_.size());
    for (const auto& laneId : selectedLaneIds_) {
        const auto* lane = findLane(*scenario_, laneId);
        if (!lane) {
            emit statusMessage(tr("No values changed · a selected signal no longer exists"));
            return false;
        }
        auto targetValue = value;
        JsonExtensions extensions;
        if (*kind == LaneKind::Bus && !presetId.empty()) {
            targetValue = busPresetValue(presetId, lane->width);
            extensions.emplace(
                std::string(kBusPresetExtension),
                "\"" + presetId + "\"");
        }
        const auto validation = validateLaneValue(*lane, targetValue);
        if (targetValue.empty() || !validation.valid) {
            auto message = targetValue.empty()
                ? *kind == LaneKind::Enum
                    ? tr("Enter an enum symbol or numeric value.")
                    : tr("Enter a bus value.")
                : QString::fromStdString(validation.error);
            if (*kind == LaneKind::Enum) {
                const auto symbols = explicitRangeEnumSymbols();
                if (!symbols.isEmpty()) {
                    message.append(
                        tr(" · shared symbols: %1")
                            .arg(symbols.join(QStringLiteral(", "))));
                }
            }
            if (rangeValueEdit_ && rangeValueEdit_->isVisible()) {
                rangeValueEdit_->setStyleSheet(QStringLiteral(
                    "color: #fff1f1; background: #4b2d35; border: 1px solid #ef7773;"
                    "border-radius: 4px; padding: 3px 6px;"));
                rangeValueEdit_->setToolTip(message);
                rangeValueEdit_->setModified(true);
            }
            emit statusMessage(tr("No values changed · %1").arg(message));
            return false;
        }
        assignments.push_back({
            laneId,
            validation.normalizedValue,
            std::move(extensions),
        });
    }

    const auto [start, end] = *selectionRange_;
    const auto valuePreflight =
        rangeValueAvailability(value, presetId);
    if (valuePreflight.valid && !valuePreflight.enabled) {
        if (rangeValueEdit_) {
            rangeValueEdit_->setModified(false);
            rangeValueEdit_->setStyleSheet({});
        }
        showRangeEditPalette();
        auto reason = valuePreflight.toolTip;
        reason.replace(QLatin1Char('\n'), QStringLiteral(" · "));
        emit statusMessage(
            tr("Set skipped · %1 · no values changed").arg(reason));
        return true;
    }

    const auto relationCountBefore = scenario_->relations.size();
    bool changed = false;
    try {
        changed = commandStack_->execute(std::make_unique<SetLaneRangesCommand>(
            *scenario_,
            start,
            end,
            std::move(assignments)));
    } catch (const std::exception& exception) {
        emit statusMessage(
            tr("No values changed · %1").arg(QString::fromUtf8(exception.what())));
        return false;
    }

    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    cursorTick_ = start;
    if (rangeValueEdit_) {
        rangeValueEdit_->setModified(false);
        rangeValueEdit_->setStyleSheet({});
        rangeValueEdit_->setToolTip({});
    }
    if (changed) {
        emit modelEdited();
        emit commandAvailabilityChanged();
    }
    if (!selectedLaneIds_.empty()) {
        emit selectionChanged(QString::fromStdString(selectedLaneIds_.front()), start);
    }
    rebuildLaneLayout();
    showRangeEditPalette();
    viewport()->update();
    auto displayValue = valuePreflight.displayValue;
    if (displayValue.isEmpty()) {
        displayValue = *kind == LaneKind::Clock
            ? value == "gated"
                ? tr("GATED")
                : value == "disabled"
                    ? tr("DISABLED · X")
                    : QString::fromStdString(value)
            : !presetId.empty()
                ? busPresetDisplayLabel(presetId)
                : QString::fromStdString(value);
    }
    if (!changed) {
        emit statusMessage(
            tr("%1 signals · %2–%3 already = %4 · no values changed")
                .arg(static_cast<qulonglong>(selectedLaneIds_.size()))
                .arg(QString::fromStdString(formatTick(start, project_->timeBase)))
                .arg(QString::fromStdString(formatTick(end, project_->timeBase)))
                .arg(displayValue));
        return true;
    }
    const auto affectedLaneCount = valuePreflight.affectedLaneCount > 0
        ? valuePreflight.affectedLaneCount
        : selectedLaneIds_.size();
    auto message = tr("%1 of %2 signals · %3–%4 = %5")
                       .arg(static_cast<qulonglong>(affectedLaneCount))
                       .arg(static_cast<qulonglong>(selectedLaneIds_.size()))
                       .arg(QString::fromStdString(formatTick(start, project_->timeBase)))
                       .arg(QString::fromStdString(formatTick(end, project_->timeBase)))
                       .arg(displayValue);
    const auto removedRelationCount = relationCountBefore
        - std::min(relationCountBefore, scenario_->relations.size());
    if (removedRelationCount > 0) {
        message += tr(" · removed %1 relation(s) because referenced edges disappeared")
                       .arg(static_cast<qulonglong>(removedRelationCount));
        message += tr(" · Ctrl+Z restores waveform and relations");
        if (valuePreflight.relationImpact.ids.size()
                == removedRelationCount
            && !valuePreflight.relationImpact.summaries.isEmpty()) {
            message += tr(" · affected: %1")
                           .arg(
                               valuePreflight.relationImpact.summaries.join(
                                   QStringLiteral("; ")));
        }
    } else {
        message += tr(" · Ctrl+Z to undo");
    }
    emit statusMessage(message);
    return true;
}

bool WaveCanvas::clearExplicitRange(const bool cutting)
{
    if (!scenario_ || !commandStack_ || !explicitRangeSelection_
        || !selectionRange_ || selectionRange_->second <= selectionRange_->first) {
        return false;
    }

    const auto clearPreflight = rangeClearAvailability();
    const auto [start, end] = *selectionRange_;
    const auto rangeKind = explicitRangeKind();
    const auto clockRange =
        rangeKind && *rangeKind == LaneKind::Clock;
    std::vector<std::string> laneIds;
    laneIds.reserve(selectedLaneIds_.size());
    for (const auto& laneId : selectedLaneIds_) {
        const auto* lane = findLane(*scenario_, laneId);
        if (!lane || lane->kind == LaneKind::Group) continue;
        const auto overlaps = std::any_of(
            lane->segments.begin(),
            lane->segments.end(),
            [start, end](const Segment& segment) {
                return segment.start < end && segment.end > start;
            });
        if (overlaps) laneIds.push_back(laneId);
    }
    if (laneIds.empty()) {
        emit statusMessage(
            cutting
                ? tr("Copied selected range · source already uses implicit values")
                : clockRange
                    ? tr("Clock range already runs normally · no values changed")
                : tr("No values cleared · selected range already uses implicit values"));
        return false;
    }

    const auto relationCountBefore = scenario_->relations.size();
    try {
        commandStack_->execute(std::make_unique<ClearLaneRangesCommand>(
            *scenario_, start, end, laneIds));
    } catch (const std::exception& exception) {
        emit statusMessage(
            (cutting ? tr("Cut failed · %1") : tr("No values cleared · %1"))
                .arg(QString::fromUtf8(exception.what())));
        return false;
    }

    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    cursorTick_ = start;
    emit modelEdited();
    emit commandAvailabilityChanged();
    if (!selectedLaneIds_.empty()) {
        emit selectionChanged(QString::fromStdString(selectedLaneIds_.front()), start);
    }
    rebuildLaneLayout();
    showRangeEditPalette();
    viewport()->update();

    auto message = (
        cutting
            ? tr("Cut %1 signal(s) over %2 · Ctrl+Z restores source")
            : clockRange
                ? tr("Restored %1 Clock signal(s) over %2 to Run · Ctrl+Z to undo")
                : tr("Cleared %1 signal(s) over %2 · Ctrl+Z to undo"))
                       .arg(static_cast<qulonglong>(laneIds.size()))
                       .arg(QString::fromStdString(
                           formatTick(end - start, project_->timeBase)));
    const auto removedRelationCount = relationCountBefore
        - std::min(relationCountBefore, scenario_->relations.size());
    if (removedRelationCount > 0) {
        message = (
            cutting
                ? tr("Cut %1 signal(s) over %2 · removed %3 relation(s) · "
                     "Ctrl+Z restores source and relations")
                : clockRange
                    ? tr("Restored %1 Clock signal(s) over %2 to Run · removed %3 relation(s) · "
                         "Ctrl+Z restores overrides and relations")
                    : tr("Cleared %1 signal(s) over %2 · removed %3 relation(s) · "
                         "Ctrl+Z restores all"))
                      .arg(static_cast<qulonglong>(laneIds.size()))
                      .arg(QString::fromStdString(
                          formatTick(end - start, project_->timeBase)))
                      .arg(static_cast<qulonglong>(removedRelationCount));
        if (clearPreflight.relationImpact.ids.size()
                == removedRelationCount
            && !clearPreflight.relationImpact.summaries.isEmpty()) {
            message += tr(" · affected: %1")
                           .arg(
                               clearPreflight.relationImpact.summaries.join(
                                   QStringLiteral("; ")));
        }
    }
    emit statusMessage(message);
    return true;
}

void WaveCanvas::applyExplicitRangePreset(const std::string& presetId)
{
    const auto kind = explicitRangeKind();
    if (!kind) {
        emit statusMessage(
            tr("No values changed · select only Clock, only Bit, only Bus, or only Enum signals"));
        return;
    }
    if (*kind == LaneKind::Clock) {
        if (presetId == "clock-gate") {
            static_cast<void>(applyExplicitRangeValue("gated"));
        } else if (presetId == "clock-disable") {
            static_cast<void>(applyExplicitRangeValue("disabled"));
        } else if (presetId == "clock-run") {
            static_cast<void>(clearExplicitRange());
        } else {
            emit statusMessage(
                tr("No values changed · use Run, Gate, or Disable for Clock ranges"));
        }
        return;
    }
    if (*kind == LaneKind::Bit) {
        const auto value = presetId == "zero" ? std::string{"0"}
            : presetId == "one" ? std::string{"1"}
            : presetId == "x" ? std::string{"X"}
            : presetId == "z" ? std::string{"Z"}
                               : std::string{};
        if (value.empty()) {
            emit statusMessage(tr("No values changed · Don't care is available for Bus ranges"));
            return;
        }
        static_cast<void>(applyExplicitRangeValue(value));
        return;
    }
    if (*kind == LaneKind::Enum) {
        emit statusMessage(
            tr("No values changed · type a declared Enum symbol in the range field"));
        return;
    }
    if (presetId == "one") {
        emit statusMessage(tr("No values changed · use a custom Bus value for 1"));
        return;
    }
    static_cast<void>(applyExplicitRangeValue({}, presetId));
}

bool WaveCanvas::submitBitPattern()
{
    if (!scenario_ || !project_ || !commandStack_
        || !rangeValueEdit_ || !explicitRangeSelection_) {
        return false;
    }
    const auto assessment =
        assessBitPatternDraft(rangeValueEdit_->text());
    const auto reject = [this](const QString& reason) {
        rangeValueEdit_->setStyleSheet(QStringLiteral(
            "color: #fff1f1; background: #4b2d35; border: 1px solid #ef7773;"
            "border-radius: 4px; padding: 3px 6px;"));
        rangeValueEdit_->setToolTip(reason);
        rangeValueEdit_->setAccessibleDescription(reason);
        rangeValueEdit_->setModified(true);
        rangeValueEdit_->setFocus(Qt::OtherFocusReason);
        rangeValueEdit_->selectAll();
        emit statusMessage(
            tr("No values changed · %1 · range kept")
                .arg(reason));
        viewport()->update();
    };
    if (!assessment.applicable
        || !assessment.valid
        || !assessment.projection) {
        reject(assessment.summary);
        return false;
    }

    const auto projection = *assessment.projection;
    if (projection.laneIds.empty()
        || projection.assignments.size()
            != projection.laneIds.size()
        || std::any_of(
            projection.laneIds.begin(),
            projection.laneIds.end(),
            [this](const std::string& laneId) {
                const auto* lane =
                    findLane(*scenario_, laneId);
                return !lane
                    || lane->kind != LaneKind::Bit;
            })) {
        reject(tr("Bit pattern unavailable · a selected signal no longer exists"));
        return false;
    }
    if (!assessment.enabled) {
        bitPatternPreview_.reset();
        rangeValueEdit_->clear();
        rangeValueEdit_->setModified(false);
        rangeValueEdit_->setStyleSheet({});
        rangeValueEdit_->clearFocus();
        showRangeEditPalette();
        viewport()->setFocus(Qt::OtherFocusReason);
        emit statusMessage(
            assessment.summary
            + tr(" · selection kept"));
        viewport()->update();
        return true;
    }

    const auto relationCountBefore =
        scenario_->relations.size();
    const auto historyStateBefore =
        commandStack_->stateId();
    const auto historySelectionBefore =
        historySelectionSnapshot();
    const auto activeLaneBefore = selectedLaneId_;
    bool changed = false;
    try {
        changed = commandStack_->execute(
            std::make_unique<SetLaneSequencesCommand>(
                *scenario_,
                projection.assignments));
    } catch (const std::exception& exception) {
        reject(QString::fromUtf8(exception.what()));
        return false;
    }
    if (!changed) {
        bitPatternPreview_.reset();
        rangeValueEdit_->clear();
        rangeValueEdit_->setModified(false);
        rangeValueEdit_->setStyleSheet({});
        showRangeEditPalette();
        viewport()->setFocus(Qt::OtherFocusReason);
        emit statusMessage(
            assessment.summary
            + tr(" · selection kept"));
        return true;
    }

    emit modelEdited();
    emit commandAvailabilityChanged();
    refreshModel();
    selectedLaneIds_ = projection.laneIds;
    selectedLaneId_ =
        std::find(
            selectedLaneIds_.begin(),
            selectedLaneIds_.end(),
            activeLaneBefore)
            != selectedLaneIds_.end()
        ? activeLaneBefore
        : selectedLaneIds_.front();
    laneHeaderSelectionActive_ = false;
    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    selectionRange_ =
        std::pair{projection.start, projection.end};
    explicitRangeSelection_ = true;
    cursorTick_ = projection.start;
    waveEditHoverLaneId_.clear();
    waveEditHoverRange_.reset();
    bitPatternPreview_.reset();
    rangeValueEdit_->clear();
    rangeValueEdit_->setModified(false);
    rangeValueEdit_->setStyleSheet({});
    rangeValueEdit_->setToolTip({});
    rangeValueEdit_->clearFocus();
    rememberHistorySelectionTransition(
        historyStateBefore,
        historySelectionBefore,
        commandStack_->stateId());
    ensureLaneVisible(selectedLaneId_);
    ensureCursorVisible(cursorTick_);
    showRangeEditPalette();
    emit selectionChanged(
        QString::fromStdString(selectedLaneId_),
        cursorTick_);

    const auto format = [this](const Tick tick) {
        return QString::fromStdString(
            formatTick(tick, project_->timeBase));
    };
    const auto repeats =
        projection.patternLength > 0
        ? projection.beatCount / projection.patternLength
        : 0;
    QString message;
    if (projection.laneIds.size() == 1) {
        const auto* lane =
            findLane(
                *scenario_,
                projection.laneIds.front());
        message =
            tr("%1 · applied %2-symbol Bit pattern %3× across %4 beats · %5–%6 · one Undo")
                .arg(
                    lane
                        ? QString::fromStdString(lane->name)
                        : tr("Bit signal"))
                .arg(static_cast<qulonglong>(
                    projection.patternLength))
                .arg(static_cast<qulonglong>(repeats))
                .arg(static_cast<qulonglong>(
                    projection.beatCount))
                .arg(format(projection.start))
                .arg(format(projection.end));
    } else if (projection.sharedPattern) {
        message =
            tr("%1 Bit signals · applied one %2-symbol pattern %3× across %4 beats each · changed %5 · %6–%7 · one Undo")
                .arg(static_cast<qulonglong>(
                    projection.laneIds.size()))
                .arg(static_cast<qulonglong>(
                    projection.patternLength))
                .arg(static_cast<qulonglong>(repeats))
                .arg(static_cast<qulonglong>(
                    projection.beatCount))
                .arg(static_cast<qulonglong>(
                    projection.changedLaneCount))
                .arg(format(projection.start))
                .arg(format(projection.end));
    } else {
        message =
            tr("%1 Bit signals · applied %2 · %3 beats each · changed %4 · %5–%6 · one Undo")
                .arg(static_cast<qulonglong>(
                    projection.laneIds.size()))
                .arg(
                    projection.patternSummaries.join(
                        QStringLiteral("; ")))
                .arg(static_cast<qulonglong>(
                    projection.beatCount))
                .arg(static_cast<qulonglong>(
                    projection.changedLaneCount))
                .arg(format(projection.start))
                .arg(format(projection.end));
    }
    const auto removedRelationCount =
        relationCountBefore
        - std::min(
            relationCountBefore,
            scenario_->relations.size());
    if (removedRelationCount > 0
        && projection.relationImpact.ids.size()
            == removedRelationCount
        && !projection.relationImpact.summaries.isEmpty()) {
        message +=
            tr(" · affected: %1")
                .arg(
                    projection.relationImpact.summaries.join(
                        QStringLiteral("; ")));
    }
    emit statusMessage(
        appendRelationAwareUndo(
            message,
            relationCountBefore,
            scenario_->relations.size()));
    viewport()->setFocus(Qt::OtherFocusReason);
    viewport()->update();
    return true;
}

bool WaveCanvas::submitBusRangeSequence()
{
    if (!scenario_ || !project_ || !commandStack_
        || !rangeValueEdit_ || !explicitRangeSelection_) {
        return false;
    }
    const auto assessment =
        assessBusRangeSequenceDraft(
            rangeValueEdit_->text());
    const auto reject = [this](const QString& reason) {
        busRangeSequencePreview_.reset();
        rangeValueEdit_->setStyleSheet(QStringLiteral(
            "color: #fff1f1; background: #4b2d35; border: 1px solid #ef7773;"
            "border-radius: 4px; padding: 3px 6px;"));
        rangeValueEdit_->setToolTip(reason);
        rangeValueEdit_->setAccessibleDescription(reason);
        rangeValueEdit_->setModified(true);
        rangeValueEdit_->setFocus(Qt::OtherFocusReason);
        rangeValueEdit_->selectAll();
        emit statusMessage(
            tr("No values changed · %1 · range kept")
                .arg(reason));
        viewport()->update();
    };
    if (!assessment.applicable
        || !assessment.sequence
        || !assessment.valid
        || !assessment.projection) {
        reject(assessment.summary);
        return false;
    }

    const auto projection = *assessment.projection;
    const auto kind = explicitRangeKind();
    if (!kind
        || (*kind != LaneKind::Bus
            && *kind != LaneKind::Enum)
        || projection.laneIds.empty()
        || projection.assignments.size()
            != projection.laneIds.size()
        || std::any_of(
            projection.laneIds.begin(),
            projection.laneIds.end(),
            [this, kind](const std::string& laneId) {
                const auto* lane =
                    findLane(*scenario_, laneId);
                return !lane
                    || lane->kind != *kind;
            })) {
        reject(
            tr("Sequence unavailable · a selected signal no longer exists"));
        return false;
    }
    if (!assessment.enabled) {
        busRangeSequencePreview_.reset();
        clearRangeSequenceBaseline();
        clearRangeSequenceAnchor();
        rangeSequenceTextSpans_.clear();
        clearRangeSequenceCaretTarget();
        rangeValueEdit_->clear();
        rangeValueEdit_->setModified(false);
        rangeValueEdit_->setStyleSheet({});
        rangeValueEdit_->clearFocus();
        showRangeEditPalette();
        viewport()->setFocus(Qt::OtherFocusReason);
        emit statusMessage(
            assessment.summary
            + tr(" · selection kept"));
        viewport()->update();
        return true;
    }

    const auto relationCountBefore =
        scenario_->relations.size();
    const auto historyStateBefore =
        commandStack_->stateId();
    const auto historySelectionBefore =
        historySelectionSnapshot();
    const auto activeLaneBefore = selectedLaneId_;
    bool changed = false;
    try {
        changed = commandStack_->execute(
            std::make_unique<SetLaneSequencesCommand>(
                *scenario_,
                projection.assignments));
    } catch (const std::exception& exception) {
        reject(QString::fromUtf8(exception.what()));
        return false;
    }
    if (!changed) {
        busRangeSequencePreview_.reset();
        clearRangeSequenceBaseline();
        clearRangeSequenceAnchor();
        rangeSequenceTextSpans_.clear();
        clearRangeSequenceCaretTarget();
        rangeValueEdit_->clear();
        rangeValueEdit_->setModified(false);
        rangeValueEdit_->setStyleSheet({});
        showRangeEditPalette();
        viewport()->setFocus(Qt::OtherFocusReason);
        emit statusMessage(
            assessment.summary
            + tr(" · selection kept"));
        return true;
    }

    emit modelEdited();
    emit commandAvailabilityChanged();
    refreshModel();
    selectedLaneIds_ = projection.laneIds;
    selectedLaneId_ =
        std::find(
            selectedLaneIds_.begin(),
            selectedLaneIds_.end(),
            activeLaneBefore)
            != selectedLaneIds_.end()
        ? activeLaneBefore
        : selectedLaneIds_.front();
    laneHeaderSelectionActive_ = false;
    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    selectionRange_ =
        std::pair{projection.start, projection.end};
    explicitRangeSelection_ = true;
    cursorTick_ = projection.start;
    waveEditHoverLaneId_.clear();
    waveEditHoverRange_.reset();
    busRangeSequencePreview_.reset();
    clearRangeSequenceBaseline();
    clearRangeSequenceAnchor();
    rangeSequenceTextSpans_.clear();
    clearRangeSequenceCaretTarget();
    rangeValueEdit_->clear();
    rangeValueEdit_->setModified(false);
    rangeValueEdit_->setStyleSheet({});
    rangeValueEdit_->setToolTip({});
    rangeValueEdit_->clearFocus();
    rememberHistorySelectionTransition(
        historyStateBefore,
        historySelectionBefore,
        commandStack_->stateId());
    ensureLaneVisible(selectedLaneId_);
    ensureCursorVisible(cursorTick_);
    showRangeEditPalette();
    emit selectionChanged(
        QString::fromStdString(selectedLaneId_),
        cursorTick_);

    const auto format = [this](const Tick tick) {
        return QString::fromStdString(
            formatTick(tick, project_->timeBase));
    };
    const auto repeats =
        projection.patternLength > 0
        ? projection.beatCount
              / projection.patternLength
        : 0;
    const auto kindName =
        *kind == LaneKind::Bus ? tr("Bus") : tr("Enum");
    QString message;
    if (projection.laneIds.size() == 1) {
        const auto* lane =
            findLane(
                *scenario_,
                projection.laneIds.front());
        message =
            tr("%1 · applied %2-value %3 sequence %4x across %5 beats · %6–%7 · one Undo")
                .arg(
                    lane
                        ? QString::fromStdString(lane->name)
                        : kindName)
                .arg(static_cast<qulonglong>(
                    projection.patternLength))
                .arg(kindName)
                .arg(static_cast<qulonglong>(repeats))
                .arg(static_cast<qulonglong>(
                    projection.beatCount))
                .arg(format(projection.start))
                .arg(format(projection.end));
    } else if (projection.sharedSequence) {
        message =
            tr("%1 %2 signals · applied one %3-value sequence %4x across %5 beats each · changed %6 · %7–%8 · one Undo")
                .arg(static_cast<qulonglong>(
                    projection.laneIds.size()))
                .arg(kindName)
                .arg(static_cast<qulonglong>(
                    projection.patternLength))
                .arg(static_cast<qulonglong>(repeats))
                .arg(static_cast<qulonglong>(
                    projection.beatCount))
                .arg(static_cast<qulonglong>(
                    projection.changedLaneCount))
                .arg(format(projection.start))
                .arg(format(projection.end));
    } else {
        message =
            tr("%1 %2 signals · applied %3 · %4 beats each · changed %5 · %6–%7 · one Undo")
                .arg(static_cast<qulonglong>(
                    projection.laneIds.size()))
                .arg(kindName)
                .arg(
                    projection.sequenceSummaries.join(
                        QStringLiteral("; ")))
                .arg(static_cast<qulonglong>(
                    projection.beatCount))
                .arg(static_cast<qulonglong>(
                    projection.changedLaneCount))
                .arg(format(projection.start))
                .arg(format(projection.end));
    }
    const auto removedRelationCount =
        relationCountBefore
        - std::min(
            relationCountBefore,
            scenario_->relations.size());
    if (removedRelationCount > 0
        && projection.relationImpact.ids.size()
            == removedRelationCount
        && !projection.relationImpact.summaries.isEmpty()) {
        message +=
            tr(" · affected: %1")
                .arg(
                    projection.relationImpact.summaries.join(
                        QStringLiteral("; ")));
    }
    emit statusMessage(
        appendRelationAwareUndo(
            message,
            relationCountBefore,
            scenario_->relations.size()));
    viewport()->setFocus(Qt::OtherFocusReason);
    viewport()->update();
    return true;
}

void WaveCanvas::submitRangeValue()
{
    if (!rangeValueEdit_ || !explicitRangeSelection_) return;
    const auto kind = explicitRangeKind();
    if (kind
        && *kind == LaneKind::Bit) {
        static_cast<void>(submitBitPattern());
        return;
    }
    if (kind
        && (*kind == LaneKind::Bus
            || *kind == LaneKind::Enum)) {
        const auto sequenceAssessment =
            assessBusRangeSequenceDraft(
                rangeValueEdit_->text());
        if (sequenceAssessment.sequence) {
            static_cast<void>(
                submitBusRangeSequence());
            return;
        }
    }
    const auto refocusRangeValue = [this] {
        rangeValueEdit_->setFocus(Qt::OtherFocusReason);
        rangeValueEdit_->selectAll();
        QTimer::singleShot(0, rangeValueEdit_, [this] {
            if (rangeValueEdit_
                && rangeValueEdit_->isVisible()
                && rangeValueEdit_->isModified()) {
                rangeValueEdit_->setFocus(Qt::OtherFocusReason);
                rangeValueEdit_->selectAll();
            }
        });
    };
    const auto value = rangeValueEdit_->text().trimmed();
    if (value.isEmpty()) {
        auto message = kind && *kind == LaneKind::Enum
            ? tr("Enter an enum symbol or numeric value.")
            : tr("Enter a bus value.");
        if (kind && *kind == LaneKind::Enum) {
            const auto symbols = explicitRangeEnumSymbols();
            if (!symbols.isEmpty()) {
                message.append(
                    tr(" · shared symbols: %1")
                        .arg(symbols.join(QStringLiteral(", "))));
            }
        }
        rangeValueEdit_->setStyleSheet(QStringLiteral(
            "color: #fff1f1; background: #4b2d35; border: 1px solid #ef7773;"
            "border-radius: 4px; padding: 3px 6px;"));
        rangeValueEdit_->setToolTip(message);
        rangeValueEdit_->setModified(true);
        refocusRangeValue();
        emit statusMessage(tr("No values changed · %1").arg(message));
        return;
    }
    if (applyExplicitRangeValue(value.toStdString())) {
        clearRangeSequenceBaseline();
        clearRangeSequenceAnchor();
        rangeSequenceTextSpans_.clear();
        clearRangeSequenceCaretTarget();
        rangeValueEdit_->setModified(false);
        rangeValueEdit_->clearFocus();
        viewport()->setFocus(Qt::OtherFocusReason);
        return;
    }
    rangeValueEdit_->setModified(true);
    refocusRangeValue();
}

void WaveCanvas::submitBusSequence(
    const BusEditCommitAction action,
    const BusDraftValues& draft,
    const BusEditActionAssessment& assessment)
{
    Q_UNUSED(action);
    if (!scenario_ || !project_ || !commandStack_
        || !busValueEdit_ || busPresetLaneId_.empty()
        || !busEditRange_) {
        return;
    }
    const auto* lane = findLane(*scenario_, busPresetLaneId_);
    if (!lane
        || (lane->kind != LaneKind::Bus
            && lane->kind != LaneKind::Enum)) {
        hideBusPresetPalette();
        return;
    }

    const auto reject = [this, lane](QString message) {
        if (message.isEmpty()) {
            message = tr("Sequence is not valid for this target");
        }
        busValueEdit_->setStyleSheet(QStringLiteral(
            "color: #fff1f1; background: #4b2d35; border: 1px solid #ef7773;"
            "border-radius: 4px; padding: 3px 6px;"));
        busValueEdit_->setToolTip(message);
        busValueEdit_->setAccessibleDescription(message);
        busValueEdit_->setModified(true);
        selectedLaneId_ = lane->id;
        selectedLaneIds_ = {lane->id};
        laneHeaderSelectionActive_ = false;
        selectionRange_ = busEditRange_;
        cursorTick_ = busEditRange_->first;
        ensureLaneVisible(lane->id);
        ensureCursorVisible(cursorTick_);
        positionBusPresetPalette();
        busValueEdit_->setFocus(Qt::OtherFocusReason);
        busValueEdit_->selectAll();
        updateBusEditActionStates(true);
        emit statusMessage(
            tr("No values changed · %1 · target kept")
                .arg(message));
        viewport()->update();
    };

    if (!draft.valid || !assessment.state.valid
        || !assessment.projection
        || assessment.projection->sequenceSteps.empty()) {
        reject(
            !assessment.state.summary.isEmpty()
                ? assessment.state.summary
                : draft.error);
        return;
    }

    const auto projection = *assessment.projection;
    const auto laneId = lane->id;
    const auto laneName = lane->name;
    const auto relationCountBefore = scenario_->relations.size();
    const auto durationBefore = scenario_->duration;
    const auto historyStateBefore = commandStack_->stateId();
    const auto historySelectionBefore = historySelectionSnapshot();
    bool changed = false;
    if (assessment.state.modelChanges) {
        try {
            changed = commandStack_->execute(
                std::make_unique<SetLaneSequenceCommand>(
                    *scenario_,
                    laneId,
                    projection.sequenceSteps));
        } catch (const std::exception& exception) {
            reject(QString::fromUtf8(exception.what()));
            return;
        }
    }

    for (const auto& value : draft.normalizedValues) {
        rememberBusValue(
            laneId,
            QString::fromStdString(value));
    }
    hideBusPresetPalette();
    if (changed) {
        emit modelEdited();
        emit commandAvailabilityChanged();
        refreshModel();
    }

    selectedLaneId_ = laneId;
    selectedLaneIds_ = {laneId};
    laneHeaderSelectionActive_ = false;
    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    selectionRange_ =
        std::pair{projection.start, projection.end};
    explicitRangeSelection_ = true;
    cursorTick_ = projection.start;
    waveEditHoverLaneId_.clear();
    waveEditHoverRange_.reset();
    ensureLaneVisible(laneId);
    ensureCursorVisible(cursorTick_);
    showRangeEditPalette();
    if (changed) {
        rememberHistorySelectionTransition(
            historyStateBefore,
            historySelectionBefore,
            commandStack_->stateId());
    }
    emit selectionChanged(
        QString::fromStdString(laneId),
        cursorTick_);

    const auto format = [this](const Tick tick) {
        return QString::fromStdString(
            formatTick(tick, project_->timeBase));
    };
    const auto step = projection.sequenceSteps.front().end
        - projection.sequenceSteps.front().start;
    auto message = changed
        ? tr("%1 · applied %2 values to %3–%4 · %5 per value · one Undo")
              .arg(QString::fromStdString(laneName))
              .arg(static_cast<qulonglong>(
                  projection.sequenceSteps.size()))
              .arg(format(projection.start))
              .arg(format(projection.end))
              .arg(format(step))
        : tr("%1 · %2 values already match %3–%4 · no values changed")
              .arg(QString::fromStdString(laneName))
              .arg(static_cast<qulonglong>(
                  projection.sequenceSteps.size()))
              .arg(format(projection.start))
              .arg(format(projection.end));
    if (changed && scenario_->duration > durationBefore) {
        message += tr(" · End %1 → %2")
                       .arg(format(durationBefore))
                       .arg(format(scenario_->duration));
    }
    if (changed) {
        const auto removedRelationCount =
            relationCountBefore
            - std::min(
                relationCountBefore,
                scenario_->relations.size());
        if (removedRelationCount > 0
            && projection.relationImpact.ids.size()
                == removedRelationCount
            && !projection.relationImpact.summaries.isEmpty()) {
            message += tr(" · affected: %1").arg(
                projection.relationImpact.summaries.join(
                    QStringLiteral("; ")));
        }
        message = appendRelationAwareUndo(
            message,
            relationCountBefore,
            scenario_->relations.size());
    }
    emit statusMessage(message);
    viewport()->setFocus(Qt::OtherFocusReason);
    viewport()->update();
}

void WaveCanvas::submitBusValue(const BusEditCommitAction action)
{
    if (hasQuickLaneSetup()) {
        submitQuickLaneSetup();
        return;
    }
    if (!scenario_ || busPresetLaneId_.empty() || !busPresetAnchorTick_
        || !busEditRange_ || !busValueEdit_) {
        return;
    }
    const auto* lane = findLane(*scenario_, busPresetLaneId_);
    if (!lane
        || (lane->kind != LaneKind::Bus && lane->kind != LaneKind::Enum)) {
        hideBusPresetPalette();
        return;
    }
    const auto navigateWithoutValue = busEditScope_ == BusEditScope::Beat
        && (action == BusEditCommitAction::PreviousBeat
            || action == BusEditCommitAction::NextBeat)
        && busValueEdit_->text().trimmed().isEmpty()
        && !busValueEdit_->isModified();
    if (navigateWithoutValue) {
        const auto laneId = busPresetLaneId_;
        const auto currentRange = *busEditRange_;
        const auto forward = action == BusEditCommitAction::NextBeat;
        if (advanceBusValueEdit(laneId, currentRange, forward)) {
            emit statusMessage(
                tr("%1 · implicit X skipped · %2")
                    .arg(QString::fromStdString(lane->name))
                    .arg(forward
                             ? tr("Tab advances · no values changed")
                             : tr("Shift+Tab goes back · no values changed")));
            return;
        }
        busValueEdit_->setFocus(Qt::TabFocusReason);
        busValueEdit_->selectAll();
        emit statusMessage(
            tr("%1 · implicit X unchanged · already at %2 · %3")
                .arg(QString::fromStdString(lane->name))
                .arg(forward ? tr("End") : tr("start"))
                .arg(forward ? tr("Shift+Tab goes back") : tr("Tab advances")));
        viewport()->update();
        return;
    }
    if (scenario_->duration <= 0
        || *busPresetAnchorTick_ < 0
        || *busPresetAnchorTick_ >= scenario_->duration
        || busEditRange_->first < 0
        || busEditRange_->second <= busEditRange_->first
        || busEditRange_->second > scenario_->duration) {
        const auto message = tr(
            "This %1 draft is beyond End. Extend the timeline or press Esc to discard it.")
                                 .arg(lane->kind == LaneKind::Enum
                                         ? tr("Enum")
                                         : tr("Bus"));
        busValueEdit_->setStyleSheet(QStringLiteral(
            "color: #fff1f1; background: #4b2d35; border: 1px solid #ef7773;"
            "border-radius: 4px; padding: 3px 6px;"));
        busValueEdit_->setToolTip(message);
        busValueEdit_->setModified(true);
        if (scenario_->duration > 0) {
            revealLocation(
                QString::fromStdString(lane->id),
                std::clamp<Tick>(*busPresetAnchorTick_, 0, scenario_->duration - 1));
        }
        positionBusPresetPalette();
        busValueEdit_->setFocus(Qt::OtherFocusReason);
        busValueEdit_->selectAll();
        updateBusEditActionStates(true);
        busValueEdit_->setToolTip(message);
        busValueEdit_->setAccessibleDescription(message);
        emit statusMessage(message);
        return;
    }
    const auto draft = parseBusDraftValues(*lane);
    if (draft.sequence) {
        const auto assessment =
            assessBusEditAction(BusEditAction::ApplyDraft);
        submitBusSequence(action, draft, assessment);
        return;
    }
    const auto entered = busValueEdit_->text().trimmed();
    const auto value = busEditorValue(*lane);
    const auto validation = validateLaneValue(*lane, value.toStdString());
    if (entered.isEmpty() || !validation.valid) {
        auto message = entered.isEmpty()
            ? lane->kind == LaneKind::Enum
                ? tr("Enter an enum symbol or numeric value.")
                : tr("Enter a bus value.")
            : QString::fromStdString(validation.error);
        if (lane->kind == LaneKind::Enum
            && !validation.valid
            && !lane->enumMap.empty()) {
            QStringList symbols;
            for (const auto& [symbol, mappedValue] : lane->enumMap) {
                Q_UNUSED(mappedValue);
                symbols.append(QString::fromStdString(symbol));
            }
            message.append(
                tr(" · symbols: %1").arg(symbols.join(QStringLiteral(", "))));
        }
        busValueEdit_->setStyleSheet(QStringLiteral(
            "color: #fff1f1; background: #4b2d35; border: 1px solid #ef7773;"
            "border-radius: 4px; padding: 3px 6px;"));
        busValueEdit_->setToolTip(message);
        busValueEdit_->setModified(true);
        selectedLaneId_ = lane->id;
        selectedLaneIds_ = {lane->id};
        laneHeaderSelectionActive_ = false;
        selectionRange_ = *busEditRange_;
        cursorTick_ = busEditRange_->first;
        ensureLaneVisible(lane->id);
        ensureCursorVisible(cursorTick_);
        positionBusPresetPalette();
        busValueEdit_->setFocus(Qt::OtherFocusReason);
        busValueEdit_->selectAll();
        updateBusEditActionStates(true);
        emit statusMessage(
            tr("No values changed · %1 · target kept")
                .arg(message));
        viewport()->update();
        return;
    }
    const auto laneId = busPresetLaneId_;
    const auto laneName = lane->name;
    const auto [start, end] = *busEditRange_;
    const auto editorScope = busEditScope_;
    const auto valuePreflight =
        assessBusEditAction(BusEditAction::ApplyDraft);
    const auto navigateBeatAfterCommit = editorScope == BusEditScope::Beat
        && (action == BusEditCommitAction::PreviousBeat
            || action == BusEditCommitAction::NextBeat);
    const auto navigateSegmentAfterCommit =
        editorScope == BusEditScope::Segment
        && (action == BusEditCommitAction::PreviousSegment
            || action == BusEditCommitAction::NextSegment);
    const auto stayAfterCommit = action == BusEditCommitAction::Stay;
    const auto historySizeBefore = commandStack_ ? commandStack_->size() : 0;
    bool applied = false;
    std::string committedSegmentId;
    if (busEditScope_ == BusEditScope::Segment) {
        const auto probe = start + (end - start) / 2;
        const auto* segment = segmentAtTick(*lane, probe);
        if (!segment || segment->start != start || segment->end != end) {
            emit statusMessage(tr("Segment changed before value submission · select it again"));
            return;
        }
        const auto segmentId = segment->id;
        committedSegmentId = segmentId;
        const auto relationCountBefore = scenario_->relations.size();
        auto replacementExtensions = segment->extensions;
        if (segment->value != validation.normalizedValue) {
            replacementExtensions.erase(std::string(kBusPresetExtension));
        }
        const auto alreadyMatches = segment->value == validation.normalizedValue
            && replacementExtensions == segment->extensions;
        if (!alreadyMatches) {
            try {
                commandStack_->execute(std::make_unique<EditSegmentCommand>(
                    *scenario_,
                    laneId,
                    segmentId,
                    start,
                    end,
                    validation.normalizedValue,
                    replacementExtensions));
            } catch (const std::exception& exception) {
                emit statusMessage(QString::fromUtf8(exception.what()));
                return;
            }
            emit modelEdited();
            emit commandAvailabilityChanged();
            refreshModel();
        }
        selectedLaneId_ = laneId;
        selectedLaneIds_ = {laneId};
        selectedSegmentLaneId_ = laneId;
        selectedSegmentId_ = segmentId;
        selectionRange_ = std::pair{start, end};
        cursorTick_ = start;
        emit selectionChanged(QString::fromStdString(laneId), start);
        const auto message = alreadyMatches
            ? tr("%1 Segment already = %2 · no values changed")
                  .arg(QString::fromStdString(laneName))
                  .arg(QString::fromStdString(validation.normalizedValue))
            : tr("%1 Segment · %2–%3 = %4")
                  .arg(QString::fromStdString(laneName))
                  .arg(QString::fromStdString(formatTick(start, project_->timeBase)))
                  .arg(QString::fromStdString(formatTick(end, project_->timeBase)))
                  .arg(QString::fromStdString(validation.normalizedValue));
        if (!alreadyMatches) {
            auto result = appendRelationAwareUndo(
                message,
                relationCountBefore,
                scenario_->relations.size());
            const auto removedRelationCount = relationCountBefore
                - std::min(
                    relationCountBefore,
                    scenario_->relations.size());
            if (valuePreflight.state.relationRemovalCount
                    == removedRelationCount
                && removedRelationCount > 0
                && valuePreflight.projection
                && !valuePreflight.projection->relationImpact
                        .summaries.isEmpty()) {
                result += tr(" · affected: %1").arg(
                    valuePreflight.projection->relationImpact
                        .summaries.join(QStringLiteral("; ")));
            }
            emit statusMessage(result);
        } else {
            emit statusMessage(message);
        }
        applied = true;
    } else {
        applied = setLaneRangeValue(
            laneId,
            start,
            end,
            validation.normalizedValue,
            {},
            valuePreflight.projection
                ? &valuePreflight.projection->relationImpact
                : nullptr);
    }
    if (applied) {
        rememberBusValue(laneId, QString::fromStdString(validation.normalizedValue));
        busValueEdit_->setText(QString::fromStdString(validation.normalizedValue));
        busValueEdit_->setModified(false);
        busValueEdit_->setStyleSheet({});
        if (stayAfterCommit) {
            if (const auto* refreshedLane = findLane(*scenario_, laneId)) {
                showBusPresetPalette(
                    *refreshedLane,
                    QPoint(xAtTick(start), 0),
                    start,
                    std::pair{start, end},
                    editorScope);
            }
            if (busValueEdit_) {
                busValueEdit_->setFocus(Qt::OtherFocusReason);
                busValueEdit_->selectAll();
            }
            const auto changed = commandStack_
                && commandStack_->size() != historySizeBefore;
            emit statusMessage(
                tr("%1 · %2 %3 · target kept · Enter finishes")
                    .arg(QString::fromStdString(laneName))
                    .arg(QString::fromStdString(validation.normalizedValue))
                    .arg(changed
                             ? tr("applied · Ctrl+Z")
                             : tr("confirmed · no values changed")));
            viewport()->update();
            return;
        }
        if (navigateBeatAfterCommit) {
            const auto forward = action == BusEditCommitAction::NextBeat;
            if (advanceBusValueEdit(laneId, {start, end}, forward)) return;

            if (const auto* refreshedLane = findLane(*scenario_, laneId)) {
                showBusPresetPalette(
                    *refreshedLane,
                    QPoint(xAtTick(start), 0),
                    start,
                    std::pair{start, end},
                    BusEditScope::Beat);
            }
            if (busValueEdit_) {
                busValueEdit_->setFocus(Qt::TabFocusReason);
                busValueEdit_->selectAll();
            }
            const auto changed = commandStack_
                && commandStack_->size() != historySizeBefore;
            emit statusMessage(
                tr("%1 · %2 %3 · already at %4 · %5")
                    .arg(QString::fromStdString(laneName))
                    .arg(QString::fromStdString(validation.normalizedValue))
                    .arg(changed ? tr("applied · Ctrl+Z") : tr("confirmed · no values changed"))
                    .arg(forward ? tr("End") : tr("start"))
                    .arg(forward ? tr("Shift+Tab goes back") : tr("Tab advances")));
            viewport()->update();
            return;
        }
        if (navigateSegmentAfterCommit) {
            const auto forward =
                action == BusEditCommitAction::NextSegment;
            const auto changed = commandStack_
                && commandStack_->size() != historySizeBefore;
            if (advanceBusSegmentValueEdit(
                    laneId,
                    {start, end},
                    forward)) {
                emit statusMessage(
                    (forward
                         ? tr("%1 · %2 %3 · next Segment opened · Shift+Tab goes back")
                         : tr("%1 · %2 %3 · previous Segment opened · Tab goes forward"))
                        .arg(QString::fromStdString(laneName))
                        .arg(QString::fromStdString(
                            validation.normalizedValue))
                        .arg(changed
                                 ? tr("applied · Ctrl+Z")
                                 : tr("confirmed · no values changed")));
                return;
            }
            if (const auto* refreshedLane = findLane(*scenario_, laneId)) {
                selectedSegmentLaneId_ = laneId;
                selectedSegmentId_ = committedSegmentId;
                showBusPresetPalette(
                    *refreshedLane,
                    QPoint(xAtTick(start), 0),
                    start,
                    std::pair{start, end},
                    BusEditScope::Segment);
            }
            if (busValueEdit_) {
                busValueEdit_->setFocus(Qt::TabFocusReason);
                busValueEdit_->selectAll();
            }
            emit statusMessage(
                (forward
                     ? tr("%1 · %2 %3 · no next Segment · Shift+Tab goes back · target kept")
                     : tr("%1 · %2 %3 · no previous Segment · Tab goes forward · target kept"))
                    .arg(QString::fromStdString(laneName))
                    .arg(QString::fromStdString(
                        validation.normalizedValue))
                    .arg(changed
                             ? tr("applied · Ctrl+Z")
                             : tr("confirmed · no values changed")));
            viewport()->update();
            return;
        }
        hideBusPresetPalette();
        viewport()->setFocus(Qt::OtherFocusReason);
    }
}
void WaveCanvas::applyBusPreset(
    const std::string& laneId,
    const std::string& presetId,
    const Tick tick,
    const bool useEditorRange)
{
    if (hasQuickLaneSetup()) {
        submitQuickLaneSetup();
        return;
    }
    if (!scenario_ || !commandStack_) return;
    auto* lane = findLane(*scenario_, laneId);
    if (!lane || lane->kind != LaneKind::Bus) return;
    const auto value = busPresetValue(presetId, lane->width);
    const auto validation = validateLaneValue(*lane, value);
    const auto range = useEditorRange && busPresetLaneId_ == laneId && busEditRange_
        ? *busEditRange_
        : editableBeatRangeAt(tick, *lane);
    const auto [start, end] = range;
    if (value.empty() || !validation.valid || end <= start) {
        emit statusMessage(tr("Cannot apply the selected bus preset"));
        return;
    }
    auto previewAction = std::optional<BusEditAction>{};
    if (presetId == "zero") {
        previewAction = BusEditAction::PresetZero;
    } else if (presetId == "reserved") {
        previewAction = BusEditAction::PresetReserved;
    } else if (presetId == "x") {
        previewAction = BusEditAction::PresetX;
    } else if (presetId == "z") {
        previewAction = BusEditAction::PresetZ;
    } else if (presetId == "dont-care") {
        previewAction = BusEditAction::PresetDontCare;
    }
    const auto preflight =
        useEditorRange && previewAction
        ? assessBusEditAction(*previewAction)
        : BusEditActionAssessment{};
    if (useEditorRange
        && previewAction
        && !preflight.state.valid) {
        updateBusEditActionStates();
        emit statusMessage(preflight.state.summary);
        return;
    }
    if (useEditorRange
        && previewAction
        && !preflight.state.modelChanges) {
        updateBusEditActionStates();
        emit statusMessage(preflight.state.summary);
        return;
    }

    JsonExtensions extensions;
    extensions.emplace(
        std::string(kBusPresetExtension),
        "\"" + presetId + "\"");
    const auto laneName = lane->name;
    const auto relationCountBefore = scenario_->relations.size();
    bool changed = false;
    try {
        const auto probe = start + (end - start) / 2;
        const auto* targetSegment = useEditorRange
                && busEditScope_ == BusEditScope::Segment
            ? segmentAtTick(*lane, probe)
            : nullptr;
        if (targetSegment
            && targetSegment->start == start
            && targetSegment->end == end) {
            auto replacementExtensions = targetSegment->extensions;
            replacementExtensions[std::string(kBusPresetExtension)] =
                "\"" + presetId + "\"";
            changed = commandStack_->execute(std::make_unique<EditSegmentCommand>(
                *scenario_,
                laneId,
                targetSegment->id,
                start,
                end,
                value,
                std::move(replacementExtensions)));
        } else {
            changed = commandStack_->execute(std::make_unique<SetLaneRangeCommand>(
                *scenario_,
                laneId,
                start,
                end,
                value,
                extensions));
        }
    } catch (const std::exception& exception) {
        emit statusMessage(QString::fromUtf8(exception.what()));
        return;
    }

    selectedLaneId_ = laneId;
    selectedLaneIds_ = {laneId};
    selectionRange_ = std::pair{start, end};
    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    const auto completeSegmentTarget =
        useEditorRange && busEditScope_ == BusEditScope::Segment;
    if (completeSegmentTarget) {
        selectedSegmentLaneId_ = laneId;
        if (const auto* refreshed = findLane(*scenario_, laneId)) {
            const auto segment = std::find_if(
                refreshed->segments.begin(),
                refreshed->segments.end(),
                [start, end, &presetId](const Segment& candidate) {
                    return candidate.start <= start
                        && candidate.end >= end
                        && busPresetId(candidate) == presetId;
                });
            selectedSegmentId_ = segment == refreshed->segments.end()
                ? std::string{}
                : segment->id;
        }
    } else {
        waveEditHoverLaneId_ = laneId;
        waveEditHoverRange_ = selectionRange_;
    }
    cursorTick_ = start;
    if (changed) {
        emit modelEdited();
        emit commandAvailabilityChanged();
    }
    emit selectionChanged(QString::fromStdString(laneId), start);
    rebuildLaneLayout();
    if (busValueEdit_ && busPresetLaneId_ == laneId) {
        busValueEdit_->setText(QString::fromStdString(validation.normalizedValue));
        busValueEdit_->setModified(false);
        busValueEdit_->setStyleSheet({});
        if (busScopeButton_) {
            busScopeButton_->setEnabled(true);
            if (busEditScope_ == BusEditScope::Beat) {
                busScopeButton_->setToolTip(
                    tr("Switch to the complete explicit Segment containing this beat"));
            }
        }
        positionBusPresetPalette();
        updateBusEditActionStates();
        positionBusPresetPalette();
    }
    viewport()->setFocus(Qt::OtherFocusReason);
    viewport()->update();
    auto message = changed
        ? tr("%1 · %2–%3 = %4")
              .arg(QString::fromStdString(laneName))
              .arg(QString::fromStdString(formatTick(start, project_->timeBase)))
              .arg(QString::fromStdString(formatTick(end, project_->timeBase)))
              .arg(busPresetDisplayLabel(presetId))
        : tr("%1 · %2–%3 already = %4 · no values changed")
              .arg(QString::fromStdString(laneName))
              .arg(QString::fromStdString(formatTick(start, project_->timeBase)))
              .arg(QString::fromStdString(formatTick(end, project_->timeBase)))
              .arg(busPresetDisplayLabel(presetId));
    if (changed) {
        message = appendRelationAwareUndo(
            message,
            relationCountBefore,
            scenario_->relations.size());
        const auto removedRelationCount = relationCountBefore
            - std::min(
                relationCountBefore,
                scenario_->relations.size());
        if (preflight.state.relationRemovalCount
                == removedRelationCount
            && removedRelationCount > 0
            && preflight.projection
            && !preflight.projection->relationImpact
                    .summaries.isEmpty()) {
            message += tr(" · affected: %1").arg(
                preflight.projection->relationImpact
                    .summaries.join(QStringLiteral("; ")));
        }
    }
    emit statusMessage(message);
}
void WaveCanvas::promptBusValueAt(const std::string& laneId, const Tick tick)
{
    if (!scenario_) return;
    const auto* lane = findLane(*scenario_, laneId);
    if (!lane
        || (lane->kind != LaneKind::Bus && lane->kind != LaneKind::Enum)) {
        return;
    }
    showBusPresetPalette(*lane, QPoint(xAtTick(tick), 0), tick);
    if (busValueEdit_) {
        busValueEdit_->setFocus(Qt::OtherFocusReason);
        busValueEdit_->selectAll();
    }
}

void WaveCanvas::clearClockBeat(
    const std::string& laneId,
    const Tick start,
    const Tick end)
{
    if (!scenario_ || !commandStack_ || start < 0 || end <= start
        || end > scenario_->duration) {
        return;
    }
    const auto* lane = findLane(*scenario_, laneId);
    if (!lane || lane->kind != LaneKind::Clock) return;
    const auto laneName = lane->name;
    const auto relationCountBefore = scenario_->relations.size();
    bool changed = false;
    try {
        changed = commandStack_->execute(
            std::make_unique<ClearLaneRangeCommand>(
                *scenario_,
                laneId,
                start,
                end));
    } catch (const std::exception& exception) {
        emit statusMessage(
            tr("Clock not changed · %1")
                .arg(QString::fromUtf8(exception.what())));
        return;
    }
    selectedLaneId_ = laneId;
    selectedLaneIds_ = {laneId};
    laneHeaderSelectionActive_ = false;
    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    selectionRange_ = std::pair{start, end};
    waveEditHoverLaneId_ = laneId;
    waveEditHoverRange_ = selectionRange_;
    cursorTick_ = start;
    if (changed) {
        emit modelEdited();
        emit commandAvailabilityChanged();
    }
    emit selectionChanged(QString::fromStdString(laneId), start);
    refreshModel();
    auto message = changed
        ? tr("%1 · %2–%3 restored normal clock waveform")
              .arg(QString::fromStdString(laneName))
              .arg(QString::fromStdString(formatTick(
                  start,
                  project_->timeBase)))
              .arg(QString::fromStdString(formatTick(
                  end,
                  project_->timeBase)))
        : tr("%1 · %2–%3 already uses normal clock waveform · no values changed")
              .arg(QString::fromStdString(laneName))
              .arg(QString::fromStdString(formatTick(
                  start,
                  project_->timeBase)))
              .arg(QString::fromStdString(formatTick(
                  end,
                  project_->timeBase)));
    emit statusMessage(
        changed
            ? appendRelationAwareUndo(
                  message,
                  relationCountBefore,
                  scenario_->relations.size())
            : message);
}

bool WaveCanvas::setLaneRangeValue(
    const std::string& laneId,
    const Tick start,
    const Tick end,
    std::string value,
    JsonExtensions extensions,
    const RelationRemovalImpact* predictedRelationImpact)
{
    if (!scenario_ || !commandStack_ || start < 0 || end <= start
        || end > scenario_->duration) {
        return false;
    }
    const auto* lane = findLane(*scenario_, laneId);
    if (!lane || lane->kind == LaneKind::Group) return false;
    const auto validation = validateLaneValue(*lane, value);
    if (!validation.valid) {
        QToolTip::showText(
            viewport()->mapToGlobal(QPoint(xAtTick(start), RulerHeight + 4)),
            QString::fromStdString(validation.error),
            viewport());
        emit statusMessage(
            tr("No values changed · %1")
                .arg(QString::fromStdString(validation.error)));
        return false;
    }
    const auto laneName = lane->name;
    const auto relationCountBefore = scenario_->relations.size();
    bool changed = false;
    try {
        changed = commandStack_->execute(std::make_unique<SetLaneRangeCommand>(
            *scenario_,
            laneId,
            start,
            end,
            validation.normalizedValue,
            std::move(extensions)));
    } catch (const std::exception& exception) {
        emit statusMessage(QString::fromUtf8(exception.what()));
        return false;
    }

    selectedLaneId_ = laneId;
    selectedLaneIds_ = {laneId};
    laneHeaderSelectionActive_ = false;
    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    selectionRange_ = std::pair{start, end};
    cursorTick_ = start;
    waveEditHoverLaneId_ = laneId;
    waveEditHoverRange_ = selectionRange_;
    if (changed) {
        emit modelEdited();
        emit commandAvailabilityChanged();
    }
    emit selectionChanged(QString::fromStdString(laneId), cursorTick_);
    refreshModel();
    auto message = changed
        ? tr("%1 · %2–%3 = %4")
              .arg(QString::fromStdString(laneName))
              .arg(QString::fromStdString(formatTick(start, project_->timeBase)))
              .arg(QString::fromStdString(formatTick(end, project_->timeBase)))
              .arg(QString::fromStdString(validation.normalizedValue))
        : tr("%1 · %2–%3 already = %4 · no values changed")
              .arg(QString::fromStdString(laneName))
              .arg(QString::fromStdString(formatTick(start, project_->timeBase)))
              .arg(QString::fromStdString(formatTick(end, project_->timeBase)))
              .arg(QString::fromStdString(validation.normalizedValue));
    if (changed) {
        message = appendRelationAwareUndo(
            message,
            relationCountBefore,
            scenario_->relations.size());
        const auto removedRelationCount = relationCountBefore
            - std::min(
                relationCountBefore,
                scenario_->relations.size());
        if (predictedRelationImpact
            && predictedRelationImpact->ids.size()
                == removedRelationCount
            && removedRelationCount > 0
            && !predictedRelationImpact->summaries.isEmpty()) {
            message += tr(" · affected: %1").arg(
                predictedRelationImpact->summaries.join(
                    QStringLiteral("; ")));
        }
    }
    emit statusMessage(message);
    return true;
}

bool WaveCanvas::clearSelectedBeatRange()
{
    if (!scenario_ || !commandStack_ || explicitRangeSelection_
        || !selectionRange_ || selectionRange_->second <= selectionRange_->first
        || !selectedSegmentId_.empty()) {
        return false;
    }
    const auto* lane = findLane(*scenario_, selectedLaneId_);
    if (!lane || lane->kind == LaneKind::Group) return false;
    const auto [start, end] = *selectionRange_;
    if (start < 0 || end > scenario_->duration) return false;
    const auto intersectsExplicit = std::any_of(
        lane->segments.begin(),
        lane->segments.end(),
        [start, end](const Segment& segment) {
            return segment.start < end && segment.end > start;
        });
    if (!intersectsExplicit) {
        const auto result = lane->kind == LaneKind::Clock
            ? tr("normal clock waveform")
            : lane->kind == LaneKind::Bit
                ? tr("implicit 0")
                : tr("implicit X");
        emit statusMessage(
            tr("%1 · %2–%3 already uses %4 · no values changed")
                .arg(QString::fromStdString(lane->name))
                .arg(QString::fromStdString(formatTick(start, project_->timeBase)))
                .arg(QString::fromStdString(formatTick(end, project_->timeBase)))
                .arg(result));
        return false;
    }

    const auto laneId = lane->id;
    const auto laneName = lane->name;
    const auto laneKind = lane->kind;
    const auto relationCountBefore = scenario_->relations.size();
    const auto historyStateBefore = commandStack_->stateId();
    const auto historySelectionBefore = historySelectionSnapshot();
    bool changed = false;
    try {
        changed = commandStack_->execute(std::make_unique<ClearLaneRangeCommand>(
            *scenario_,
            laneId,
            start,
            end));
    } catch (const std::exception& exception) {
        emit statusMessage(QString::fromUtf8(exception.what()));
        return false;
    }
    selectedLaneId_ = laneId;
    selectedLaneIds_ = {laneId};
    laneHeaderSelectionActive_ = false;
    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    selectionRange_ = std::pair{start, end};
    waveEditHoverLaneId_ = laneId;
    waveEditHoverRange_ = selectionRange_;
    cursorTick_ = start;
    if (changed) {
        rememberHistorySelectionTransition(
            historyStateBefore,
            historySelectionBefore,
            commandStack_->stateId());
        emit modelEdited();
        emit commandAvailabilityChanged();
    }
    emit selectionChanged(QString::fromStdString(laneId), cursorTick_);
    refreshModel();
    if (busEditPaletteVisible_
        && busPresetLaneId_ == laneId
        && busEditScope_ == BusEditScope::Beat) {
        if (const auto* refreshed = findLane(*scenario_, laneId)) {
            showBusPresetPalette(
                *refreshed,
                QPoint(xAtTick(start), 0),
                start,
                std::pair{start, end},
                BusEditScope::Beat);
        }
    }
    const auto result = laneKind == LaneKind::Clock
        ? tr("normal clock waveform")
        : laneKind == LaneKind::Bit
            ? tr("implicit 0")
            : tr("implicit X");
    emit statusMessage(appendRelationAwareUndo(
        tr("%1 · %2–%3 cleared to %4")
            .arg(QString::fromStdString(laneName))
            .arg(QString::fromStdString(formatTick(start, project_->timeBase)))
            .arg(QString::fromStdString(formatTick(end, project_->timeBase)))
            .arg(result),
        relationCountBefore,
        scenario_->relations.size()));
    return true;
}

void WaveCanvas::clearSelectedSegment()
{
    if (!scenario_ || !commandStack_ || selectedSegmentLaneId_.empty()
        || selectedSegmentId_.empty()) {
        return;
    }
    const auto* lane = findLane(*scenario_, selectedSegmentLaneId_);
    const auto* segment = segmentById(selectedSegmentLaneId_, selectedSegmentId_);
    if (!lane || !segment) {
        clearWaveEditState();
        viewport()->update();
        return;
    }
    const auto laneId = selectedSegmentLaneId_;
    const auto laneName = lane->name;
    const auto laneKind = lane->kind;
    const auto start = segment->start;
    const auto end = segment->end;
    const auto relationCountBefore = scenario_->relations.size();
    try {
        commandStack_->execute(std::make_unique<ClearLaneRangeCommand>(
            *scenario_,
            laneId,
            start,
            end));
    } catch (const std::exception& exception) {
        emit statusMessage(QString::fromUtf8(exception.what()));
        return;
    }
    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    selectionRange_.reset();
    cursorTick_ = start;
    emit modelEdited();
    emit commandAvailabilityChanged();
    refreshModel();
    const auto formatTime = [this](const Tick tick) {
        return project_
            ? QString::fromStdString(formatTick(tick, project_->timeBase))
            : QString::number(tick);
    };
    const auto result = laneKind == LaneKind::Clock
        ? tr("normal clock waveform")
        : laneKind == LaneKind::Bit
            ? tr("implicit 0")
            : tr("implicit X");
    emit statusMessage(appendRelationAwareUndo(
        tr("%1 · %2–%3 cleared to %4")
            .arg(QString::fromStdString(laneName))
            .arg(formatTime(start))
            .arg(formatTime(end))
            .arg(result),
        relationCountBefore,
        scenario_->relations.size()));
}

bool WaveCanvas::duplicateSelectedSegmentAfter()
{
    return duplicateSelectedSegment(true);
}

bool WaveCanvas::duplicateSelectedSegmentBefore()
{
    return duplicateSelectedSegment(false);
}

void WaveCanvas::selectPreviousSegment()
{
    navigateSelectedSegment(false);
}

void WaveCanvas::selectNextSegment()
{
    navigateSelectedSegment(true);
}

bool WaveCanvas::moveSelectedSegmentEarlier()
{
    return nudgeSelectedSegment(false);
}

bool WaveCanvas::moveSelectedSegmentLater()
{
    return nudgeSelectedSegment(true);
}

bool WaveCanvas::expandSelectedSegmentStart()
{
    return resizeSelectedSegmentBoundary(SegmentBoundary::Start, true);
}

bool WaveCanvas::trimSelectedSegmentStart()
{
    return resizeSelectedSegmentBoundary(SegmentBoundary::Start, false);
}

bool WaveCanvas::expandSelectedSegmentEnd()
{
    return resizeSelectedSegmentBoundary(SegmentBoundary::End, true);
}

bool WaveCanvas::trimSelectedSegmentEnd()
{
    return resizeSelectedSegmentBoundary(SegmentBoundary::End, false);
}

bool WaveCanvas::duplicateSelectedSegment(const bool after)
{
    const auto assessment = assessSelectedSegmentAction(
        after
            ? SegmentAction::DuplicateAfter
            : SegmentAction::DuplicateBefore);
    segmentActionPreview_.reset();
    if (!assessment.state.valid || !assessment.state.modelChanges) {
        emit statusMessage(assessment.state.summary);
        viewport()->update();
        return false;
    }
    const auto preflight = assessment.projection;
    if (!scenario_ || !commandStack_ || selectedSegmentLaneId_.empty()
        || selectedSegmentId_.empty()) {
        return false;
    }
    const auto* lane = findLane(*scenario_, selectedSegmentLaneId_);
    const auto* segment = segmentById(selectedSegmentLaneId_, selectedSegmentId_);
    if (!lane || !segment
        || (lane->kind != LaneKind::Bus && lane->kind != LaneKind::Enum)) {
        return false;
    }

    const auto laneId = lane->id;
    const auto laneName = lane->name;
    const auto sourceSegmentId = segment->id;
    const auto width = segment->end - segment->start;
    const auto targetStart = after
        ? segment->end
        : segment->start - std::max<Tick>(0, width);
    const auto formatTime = [this](const Tick tick) {
        return project_
            ? QString::fromStdString(formatTick(tick, project_->timeBase))
            : QString::number(tick);
    };
    if (width <= 0
        || targetStart < 0
        || targetStart > scenario_->duration
        || width > scenario_->duration - targetStart) {
        const auto targetEnd = targetStart + std::max<Tick>(0, width);
        emit statusMessage(
            after
                ? tr("%1 segment not duplicated · next interval %2–%3 exceeds End %4")
                      .arg(QString::fromStdString(laneName))
                      .arg(formatTime(targetStart))
                      .arg(formatTime(targetEnd))
                      .arg(formatTime(scenario_->duration))
                : tr("%1 segment not duplicated · previous interval %2–%3 precedes start 0")
                      .arg(QString::fromStdString(laneName))
                      .arg(formatTime(targetStart))
                      .arg(formatTime(targetEnd)));
        return false;
    }

    const auto targetEnd = targetStart + width;
    const auto relationCountBefore = scenario_->relations.size();
    const auto historyStateBefore = commandStack_->stateId();
    const auto historySelectionBefore = historySelectionSnapshot();
    bool changed = false;
    try {
        changed = commandStack_->execute(std::make_unique<CopySegmentCommand>(
            *scenario_,
            laneId,
            sourceSegmentId,
            targetStart,
            targetEnd));
    } catch (const std::exception& exception) {
        emit statusMessage(QString::fromUtf8(exception.what()));
        return false;
    }
    if (!changed) {
        emit statusMessage(
            tr("%1 · %2–%3 already matches the segment · no values changed")
                .arg(QString::fromStdString(laneName))
                .arg(formatTime(targetStart))
                .arg(formatTime(targetEnd)));
        return false;
    }

    emit modelEdited();
    emit commandAvailabilityChanged();
    refreshModel();
    selectedLaneId_ = laneId;
    selectedLaneIds_ = {laneId};
    laneHeaderSelectionActive_ = false;
    cursorTick_ = targetStart;
    if (const auto* refreshedLane = findLane(*scenario_, laneId)) {
        const auto midpoint = targetStart + width / 2;
        if (const auto* target = segmentAtTick(*refreshedLane, midpoint)) {
            selectedSegmentLaneId_ = refreshedLane->id;
            selectedSegmentId_ = target->id;
            selectionRange_ = std::pair{targetStart, targetEnd};
        }
    }
    rememberHistorySelectionTransition(
        historyStateBefore,
        historySelectionBefore,
        commandStack_->stateId());
    emit selectionChanged(QString::fromStdString(laneId), cursorTick_);
    auto message =
        (after
             ? tr("%1 segment duplicated after to %2–%3 · source kept")
             : tr("%1 segment duplicated before to %2–%3 · source kept"))
            .arg(QString::fromStdString(laneName))
            .arg(formatTime(targetStart))
            .arg(formatTime(targetEnd));
    const auto removedRelationCount =
        relationCountBefore
        - std::min(relationCountBefore, scenario_->relations.size());
    if (preflight
        && removedRelationCount > 0
        && preflight->relationImpact.ids.size() == removedRelationCount
        && !preflight->relationImpact.summaries.isEmpty()) {
        message += tr(" · removed: %1").arg(
            preflight->relationImpact.summaries.join(
                QStringLiteral("; ")));
    }
    emit statusMessage(appendRelationAwareUndo(
        message,
        relationCountBefore,
        scenario_->relations.size()));
    viewport()->update();
    return true;
}

bool WaveCanvas::nudgeSelectedSegment(const bool forward)
{
    const auto assessment = assessSelectedSegmentAction(
        forward
            ? SegmentAction::MoveLater
            : SegmentAction::MoveEarlier);
    segmentActionPreview_.reset();
    if (!assessment.state.valid || !assessment.state.modelChanges) {
        emit statusMessage(assessment.state.summary);
        viewport()->update();
        return false;
    }
    const auto preflight = assessment.projection;
    if (!scenario_ || !commandStack_ || selectedSegmentLaneId_.empty()
        || selectedSegmentId_.empty()) {
        return false;
    }
    auto* lane = findLane(*scenario_, selectedSegmentLaneId_);
    const auto* segment = segmentById(
        selectedSegmentLaneId_,
        selectedSegmentId_);
    if (!lane || !segment || lane->kind == LaneKind::Group) return false;

    const auto current = std::find_if(
        lane->segments.begin(),
        lane->segments.end(),
        [this](const Segment& candidate) {
            return candidate.id == selectedSegmentId_;
        });
    if (current == lane->segments.end()) return false;
    const auto index = static_cast<std::size_t>(
        std::distance(lane->segments.begin(), current));
    const auto unit = minimumWaveEditUnit(*lane);
    const auto width = segment->end - segment->start;
    auto lower = Tick{0};
    auto upper = scenario_->duration - width;
    if (index > 0) {
        const auto& previous = lane->segments.at(index - 1);
        lower = previous.end == segment->start
            ? previous.start + unit
            : previous.end;
    }
    if (index + 1 < lane->segments.size()) {
        const auto& next = lane->segments.at(index + 1);
        upper = next.start == segment->end
            ? next.end - width - unit
            : next.start - width;
    }
    const auto requestedStart = forward
        ? segment->start + unit
        : segment->start - unit;
    const auto formatTime = [this](const Tick tick) {
        return project_
            ? QString::fromStdString(formatTick(tick, project_->timeBase))
            : QString::number(tick);
    };
    if (upper < lower
        || requestedStart < lower
        || requestedStart > upper) {
        emit statusMessage(
            forward
                ? tr("%1 Segment cannot move later by %2 · next content or End blocks it")
                      .arg(QString::fromStdString(lane->name))
                      .arg(formatTime(unit))
                : tr("%1 Segment cannot move earlier by %2 · previous content or start blocks it")
                      .arg(QString::fromStdString(lane->name))
                      .arg(formatTime(unit)));
        return false;
    }

    const auto targetStart = requestedStart;
    const auto targetEnd = targetStart + width;
    const auto laneId = lane->id;
    const auto laneName = lane->name;
    const auto segmentId = segment->id;
    const auto segmentValue = segment->value;
    const auto relationCountBefore = scenario_->relations.size();
    const auto historyStateBefore = commandStack_->stateId();
    const auto historySelectionBefore = historySelectionSnapshot();
    bool changed = false;
    try {
        changed = commandStack_->execute(std::make_unique<EditSegmentCommand>(
            *scenario_,
            laneId,
            segmentId,
            targetStart,
            targetEnd,
            segmentValue));
    } catch (const std::exception& exception) {
        emit statusMessage(QString::fromUtf8(exception.what()));
        return false;
    }
    if (!changed) {
        emit statusMessage(
            tr("%1 Segment position is unchanged").arg(
                QString::fromStdString(laneName)));
        return false;
    }

    emit modelEdited();
    emit commandAvailabilityChanged();
    refreshModel();
    selectedLaneId_ = laneId;
    selectedLaneIds_ = {laneId};
    laneHeaderSelectionActive_ = false;
    cursorTick_ = targetStart;
    if (const auto* refreshedLane = findLane(*scenario_, laneId)) {
        const auto midpoint = targetStart + width / 2;
        if (const auto* target = segmentAtTick(*refreshedLane, midpoint)) {
            selectedSegmentLaneId_ = laneId;
            selectedSegmentId_ = target->id;
            selectionRange_ = std::pair{targetStart, targetEnd};
        }
    }
    rememberHistorySelectionTransition(
        historyStateBefore,
        historySelectionBefore,
        commandStack_->stateId());
    ensureCursorVisible(cursorTick_);
    emit selectionChanged(QString::fromStdString(laneId), cursorTick_);
    auto message =
        tr("%1 Segment nudged %2 to %3–%4 · step %5")
            .arg(QString::fromStdString(laneName))
            .arg(forward ? tr("later") : tr("earlier"))
            .arg(formatTime(targetStart))
            .arg(formatTime(targetEnd))
            .arg(formatTime(unit));
    const auto removedRelationCount =
        relationCountBefore
        - std::min(relationCountBefore, scenario_->relations.size());
    if (preflight
        && removedRelationCount > 0
        && preflight->relationImpact.ids.size() == removedRelationCount
        && !preflight->relationImpact.summaries.isEmpty()) {
        message += tr(" · removed: %1").arg(
            preflight->relationImpact.summaries.join(
                QStringLiteral("; ")));
    }
    emit statusMessage(appendRelationAwareUndo(
        message,
        relationCountBefore,
        scenario_->relations.size()));
    viewport()->update();
    return true;
}

bool WaveCanvas::resizeSelectedSegmentBoundary(
    const SegmentBoundary boundary,
    const bool expand)
{
    if (boundary == SegmentBoundary::None) return false;
    const auto action =
        boundary == SegmentBoundary::Start
        ? expand
            ? SegmentAction::ExpandStart
            : SegmentAction::TrimStart
        : expand
            ? SegmentAction::ExpandEnd
            : SegmentAction::TrimEnd;
    const auto assessment = assessSelectedSegmentAction(action);
    segmentActionPreview_.reset();
    if (!assessment.state.valid || !assessment.state.modelChanges) {
        emit statusMessage(assessment.state.summary);
        viewport()->update();
        return false;
    }
    const auto preflight = assessment.projection;
    if (!scenario_ || !commandStack_
        || selectedSegmentLaneId_.empty()
        || selectedSegmentId_.empty()) {
        return false;
    }
    auto* lane = findLane(*scenario_, selectedSegmentLaneId_);
    const auto* segment = segmentById(
        selectedSegmentLaneId_,
        selectedSegmentId_);
    if (!lane || !segment || lane->kind == LaneKind::Group) return false;

    const auto current = std::find_if(
        lane->segments.begin(),
        lane->segments.end(),
        [this](const Segment& candidate) {
            return candidate.id == selectedSegmentId_;
        });
    if (current == lane->segments.end()) return false;
    const auto index = static_cast<std::size_t>(
        std::distance(lane->segments.begin(), current));
    const auto unit = minimumWaveEditUnit(*lane);
    auto targetStart = segment->start;
    auto targetEnd = segment->end;
    auto lower = Tick{0};
    auto upper = scenario_->duration;
    if (boundary == SegmentBoundary::Start) {
        if (index > 0) {
            const auto& previous = lane->segments.at(index - 1);
            lower = previous.end == segment->start
                ? previous.start + unit
                : previous.end;
        }
        upper = segment->end - unit;
        targetStart = expand
            ? segment->start - unit
            : segment->start + unit;
    } else {
        lower = segment->start + unit;
        if (index + 1 < lane->segments.size()) {
            const auto& next = lane->segments.at(index + 1);
            upper = next.start == segment->end
                ? next.end - unit
                : next.start;
        }
        targetEnd = expand
            ? segment->end + unit
            : segment->end - unit;
    }

    const auto requested = boundary == SegmentBoundary::Start
        ? targetStart
        : targetEnd;
    const auto formatTime = [this](const Tick tick) {
        return project_
            ? QString::fromStdString(formatTick(tick, project_->timeBase))
            : QString::number(tick);
    };
    if (upper < lower || requested < lower || requested > upper) {
        emit statusMessage(
            tr("%1 Segment %2 boundary cannot move %3 by %4 · adjacent content or minimum width blocks it")
                .arg(QString::fromStdString(lane->name))
                .arg(boundary == SegmentBoundary::Start
                         ? tr("left")
                         : tr("right"))
                .arg(
                    boundary == SegmentBoundary::Start
                        ? expand ? tr("earlier") : tr("later")
                        : expand ? tr("later") : tr("earlier"))
                .arg(formatTime(unit)));
        return false;
    }

    const auto laneId = lane->id;
    const auto laneName = lane->name;
    const auto segmentId = segment->id;
    const auto segmentValue = segment->value;
    const auto relationCountBefore = scenario_->relations.size();
    const auto historyStateBefore = commandStack_->stateId();
    const auto historySelectionBefore = historySelectionSnapshot();
    bool changed = false;
    try {
        changed = commandStack_->execute(std::make_unique<EditSegmentCommand>(
            *scenario_,
            laneId,
            segmentId,
            targetStart,
            targetEnd,
            segmentValue));
    } catch (const std::exception& exception) {
        emit statusMessage(QString::fromUtf8(exception.what()));
        return false;
    }
    if (!changed) {
        emit statusMessage(
            tr("%1 Segment boundary is unchanged")
                .arg(QString::fromStdString(laneName)));
        return false;
    }

    emit modelEdited();
    emit commandAvailabilityChanged();
    refreshModel();
    selectedLaneId_ = laneId;
    selectedLaneIds_ = {laneId};
    laneHeaderSelectionActive_ = false;
    cursorTick_ = boundary == SegmentBoundary::Start
        ? targetStart
        : targetEnd;
    if (const auto* refreshedLane = findLane(*scenario_, laneId)) {
        const auto midpoint =
            targetStart + (targetEnd - targetStart) / 2;
        if (const auto* target = segmentAtTick(*refreshedLane, midpoint)) {
            selectedSegmentLaneId_ = laneId;
            selectedSegmentId_ = target->id;
            selectionRange_ = std::pair{targetStart, targetEnd};
        }
    }
    rememberHistorySelectionTransition(
        historyStateBefore,
        historySelectionBefore,
        commandStack_->stateId());
    ensureCursorVisible(cursorTick_);
    emit selectionChanged(QString::fromStdString(laneId), cursorTick_);
    auto message =
        tr("%1 Segment %2 boundary moved %3 to %4 · width %5 · step %6")
            .arg(QString::fromStdString(laneName))
            .arg(boundary == SegmentBoundary::Start
                     ? tr("left")
                     : tr("right"))
            .arg(
                boundary == SegmentBoundary::Start
                    ? expand ? tr("earlier") : tr("later")
                    : expand ? tr("later") : tr("earlier"))
            .arg(formatTime(requested))
            .arg(formatTime(targetEnd - targetStart))
            .arg(formatTime(unit));
    const auto removedRelationCount =
        relationCountBefore
        - std::min(relationCountBefore, scenario_->relations.size());
    if (preflight
        && removedRelationCount > 0
        && preflight->relationImpact.ids.size() == removedRelationCount
        && !preflight->relationImpact.summaries.isEmpty()) {
        message += tr(" · removed: %1").arg(
            preflight->relationImpact.summaries.join(
                QStringLiteral("; ")));
    }
    emit statusMessage(appendRelationAwareUndo(
        message,
        relationCountBefore,
        scenario_->relations.size()));
    viewport()->update();
    return true;
}

void WaveCanvas::updateLaneDragAutoScroll(const int pointerY)
{
    laneDragAutoScrollPointerY_ = pointerY;
    auto direction = 0;
    if (laneHeaderDragging_ && verticalScrollBar()->maximum() > 0) {
        if (pointerY <= RulerHeight + LaneDragAutoScrollMargin
            && verticalScrollBar()->value() > verticalScrollBar()->minimum()) {
            direction = -1;
        } else if (pointerY >= viewport()->height() - LaneDragAutoScrollMargin
                   && verticalScrollBar()->value() < verticalScrollBar()->maximum()) {
            direction = 1;
        }
    }
    laneDragAutoScrollDirection_ = direction;
    if (direction == 0) {
        if (laneDragAutoScrollTimer_) laneDragAutoScrollTimer_->stop();
    } else if (laneDragAutoScrollTimer_ && !laneDragAutoScrollTimer_->isActive()) {
        laneDragAutoScrollTimer_->start();
    }
}

void WaveCanvas::advanceLaneDragAutoScroll()
{
    if (!laneHeaderDragging_ || laneDragAutoScrollDirection_ == 0) {
        stopLaneDragAutoScroll();
        return;
    }
    const auto previous = verticalScrollBar()->value();
    verticalScrollBar()->setValue(
        previous + laneDragAutoScrollDirection_ * LaneDragAutoScrollStep);
    if (verticalScrollBar()->value() == previous) {
        stopLaneDragAutoScroll();
        return;
    }
    updateLaneDropTarget(laneDragAutoScrollPointerY_);
    viewport()->update();
}

void WaveCanvas::stopLaneDragAutoScroll()
{
    laneDragAutoScrollDirection_ = 0;
    if (laneDragAutoScrollTimer_) laneDragAutoScrollTimer_->stop();
}

void WaveCanvas::updateWaveEditDragAutoScroll(
    const QPoint& pointerPosition,
    const Qt::KeyboardModifiers modifiers)
{
    waveEditDragAutoScrollPointer_ = pointerPosition;
    waveEditDragAutoScrollModifiers_ = modifiers;
    auto direction = 0;
    auto verticalDirection = 0;
    const auto dragStarted =
        (pointerPosition - waveEditPressPosition_).manhattanLength()
        >= QApplication::startDragDistance();
    if (drawing_
        && tool_ == Tool::WaveEdit
        && dragStarted
        && horizontalScrollBar()->maximum() > 0) {
        if (pointerPosition.x() <= headerWidth_ + WaveEditDragAutoScrollMargin
            && horizontalScrollBar()->value() > horizontalScrollBar()->minimum()) {
            direction = -1;
        } else if (pointerPosition.x()
                       >= viewport()->width() - WaveEditDragAutoScrollMargin
                   && horizontalScrollBar()->value()
                       < horizontalScrollBar()->maximum()) {
            direction = 1;
        }
    }
    if (drawing_
        && tool_ == Tool::WaveEdit
        && waveEditInteraction_ == WaveEditInteraction::MoveRange
        && dragStarted
        && verticalScrollBar()->maximum() > 0) {
        if (pointerPosition.y() <= RulerHeight + WaveEditDragAutoScrollMargin
            && verticalScrollBar()->value()
                > verticalScrollBar()->minimum()) {
            verticalDirection = -1;
        } else if (pointerPosition.y()
                       >= viewport()->height()
                           - WaveEditDragAutoScrollMargin
                   && verticalScrollBar()->value()
                       < verticalScrollBar()->maximum()) {
            verticalDirection = 1;
        }
    }
    waveEditDragAutoScrollDirection_ = direction;
    waveEditDragVerticalAutoScrollDirection_ = verticalDirection;
    if (direction == 0 && verticalDirection == 0) {
        if (waveEditDragAutoScrollTimer_) waveEditDragAutoScrollTimer_->stop();
    } else if (waveEditDragAutoScrollTimer_
               && !waveEditDragAutoScrollTimer_->isActive()) {
        waveEditDragAutoScrollTimer_->start();
    }
}

void WaveCanvas::advanceWaveEditDragAutoScroll()
{
    if (!drawing_
        || tool_ != Tool::WaveEdit
        || (waveEditDragAutoScrollDirection_ == 0
            && waveEditDragVerticalAutoScrollDirection_ == 0)) {
        stopWaveEditDragAutoScroll();
        return;
    }
    const auto previousHorizontal = horizontalScrollBar()->value();
    const auto previousVertical = verticalScrollBar()->value();
    if (waveEditDragAutoScrollDirection_ != 0) {
        horizontalScrollBar()->setValue(
            previousHorizontal
            + waveEditDragAutoScrollDirection_
                * WaveEditDragAutoScrollStep);
        if (horizontalScrollBar()->value() == previousHorizontal) {
            waveEditDragAutoScrollDirection_ = 0;
        }
    }
    if (waveEditDragVerticalAutoScrollDirection_ != 0) {
        verticalScrollBar()->setValue(
            previousVertical
            + waveEditDragVerticalAutoScrollDirection_
                * WaveEditDragAutoScrollStep);
        if (verticalScrollBar()->value() == previousVertical) {
            waveEditDragVerticalAutoScrollDirection_ = 0;
        }
    }
    if (horizontalScrollBar()->value() == previousHorizontal
        && verticalScrollBar()->value() == previousVertical) {
        stopWaveEditDragAutoScroll();
        return;
    }
    waveEditDragAutoScrolled_ = true;
    QMouseEvent syntheticMove(
        QEvent::MouseMove,
        QPointF(waveEditDragAutoScrollPointer_),
        QPointF(viewport()->mapToGlobal(waveEditDragAutoScrollPointer_)),
        Qt::NoButton,
        Qt::LeftButton,
        waveEditDragAutoScrollModifiers_);
    mouseMoveEvent(&syntheticMove);
}

void WaveCanvas::stopWaveEditDragAutoScroll()
{
    waveEditDragAutoScrollDirection_ = 0;
    waveEditDragVerticalAutoScrollDirection_ = 0;
    waveEditDragAutoScrollModifiers_ = Qt::NoModifier;
    if (waveEditDragAutoScrollTimer_) waveEditDragAutoScrollTimer_->stop();
}

void WaveCanvas::cancelExplicitRangeDrag(const QString& message)
{
    const auto originalRange = waveEditOriginalRange_;
    const auto interaction = waveEditInteraction_;
    stopWaveEditDragAutoScroll();
    if (waveEditDragAutoScrolled_) {
        horizontalScrollBar()->setValue(
            waveEditDragOriginalHorizontalScroll_);
        verticalScrollBar()->setValue(
            waveEditDragOriginalVerticalScroll_);
    }
    waveEditDragAutoScrolled_ = false;
    drawing_ = false;
    waveEditInteraction_ = WaveEditInteraction::None;
    waveEditCopyDrag_ = false;
    waveEditPreviewRange_.reset();
    waveEditOriginalRange_.reset();
    waveEditRangeTargetLaneIds_.clear();
    waveEditRangeGrabLaneOffset_ = 0;
    waveEditRangeTargetValid_ = true;
    waveEditRangeTargetError_.clear();
    waveEditHoverLaneId_.clear();
    waveEditHoverRange_.reset();
    snapGuideTick_.reset();

    if (originalRange
        && originalRange->second > originalRange->first
        && !selectedLaneIds_.empty()) {
        selectionRange_ = originalRange;
        explicitRangeSelection_ = true;
        cursorTick_ = interaction == WaveEditInteraction::ResizeRangeEnd
            ? originalRange->second
            : originalRange->first;
        showRangeEditPalette();
        emit selectionChanged(
            QString::fromStdString(selectedLaneId_),
            cursorTick_);
    }
    viewport()->setCursor(Qt::SizeAllCursor);
    viewport()->update();
    emit statusMessage(message);
}

void WaveCanvas::updateRangeTransferTargetLanes(const Lane* pointerLane)
{
    waveEditRangeTargetLaneIds_.clear();
    waveEditRangeTargetValid_ = false;
    waveEditRangeTargetError_.clear();
    if (!scenario_
        || !pointerLane
        || pointerLane->kind == LaneKind::Group
        || selectedLaneIds_.empty()) {
        waveEditRangeTargetError_ =
            tr("Drop on a visible signal to choose the target");
        return;
    }

    std::vector<const Lane*> visibleSignals;
    visibleSignals.reserve(laneLayout_.size());
    for (const auto& layout : laneLayout_) {
        const auto& lane = scenario_->lanes.at(layout.laneIndex);
        if (lane.visible && lane.kind != LaneKind::Group) {
            visibleSignals.push_back(&lane);
        }
    }
    const auto pointer = std::find_if(
        visibleSignals.begin(),
        visibleSignals.end(),
        [pointerLane](const Lane* lane) {
            return lane->id == pointerLane->id;
        });
    if (pointer == visibleSignals.end()) {
        waveEditRangeTargetError_ =
            tr("Drop on a visible signal to choose the target");
        return;
    }

    const auto pointerIndex = static_cast<std::size_t>(
        std::distance(visibleSignals.begin(), pointer));
    if (pointerIndex < waveEditRangeGrabLaneOffset_) {
        waveEditRangeTargetError_ =
            tr("Not enough target signals above this position");
        return;
    }
    const auto firstTarget =
        pointerIndex - waveEditRangeGrabLaneOffset_;
    if (selectedLaneIds_.size() > visibleSignals.size() - firstTarget) {
        waveEditRangeTargetError_ =
            tr("Not enough target signals below this position");
        return;
    }

    waveEditRangeTargetLaneIds_.reserve(selectedLaneIds_.size());
    for (std::size_t index = 0; index < selectedLaneIds_.size(); ++index) {
        waveEditRangeTargetLaneIds_.push_back(
            visibleSignals.at(firstTarget + index)->id);
    }

    const auto sourceRange = waveEditOriginalRange_
        ? waveEditOriginalRange_
        : selectionRange_;
    for (std::size_t index = 0; index < selectedLaneIds_.size(); ++index) {
        const auto* source =
            findLane(*scenario_, selectedLaneIds_.at(index));
        const auto* target =
            findLane(*scenario_, waveEditRangeTargetLaneIds_.at(index));
        if (!source || !target) {
            waveEditRangeTargetError_ =
                tr("A source or target signal is no longer available");
            return;
        }
        const auto sourceName = QString::fromStdString(source->name);
        const auto targetName = QString::fromStdString(target->name);
        if (source->kind != target->kind) {
            waveEditRangeTargetError_ =
                tr("%1 cannot receive %2 · signal types differ")
                    .arg(targetName, sourceName);
            return;
        }
        if ((source->kind == LaneKind::Bus
             || source->kind == LaneKind::Enum)
            && source->width != target->width) {
            waveEditRangeTargetError_ =
                tr("%1 cannot receive %2 · signal widths differ")
                    .arg(targetName, sourceName);
            return;
        }
        if (!sourceRange) continue;
        auto segment = std::lower_bound(
            source->segments.begin(),
            source->segments.end(),
            sourceRange->first,
            [](const Segment& candidate, const Tick tick) {
                return candidate.end <= tick;
            });
        for (; segment != source->segments.end()
               && segment->start < sourceRange->second;
             ++segment) {
            const auto validation = validateLaneValue(*target, segment->value);
            if (!validation.valid) {
                waveEditRangeTargetError_ =
                    tr("%1 cannot receive value %2 from %3")
                        .arg(
                            targetName,
                            QString::fromStdString(segment->value),
                            sourceName);
                return;
            }
        }
    }
    waveEditRangeTargetValid_ = true;
}

bool WaveCanvas::rangeTransferTargetOverlapsSource() const
{
    if (!waveEditOriginalRange_
        || !waveEditPreviewRange_
        || waveEditRangeTargetLaneIds_.empty()
        || waveEditPreviewRange_->first >= waveEditOriginalRange_->second
        || waveEditPreviewRange_->second <= waveEditOriginalRange_->first) {
        return false;
    }
    return std::any_of(
        waveEditRangeTargetLaneIds_.begin(),
        waveEditRangeTargetLaneIds_.end(),
        [this](const std::string& targetLaneId) {
            return std::find(
                       selectedLaneIds_.begin(),
                       selectedLaneIds_.end(),
                       targetLaneId)
                != selectedLaneIds_.end();
        });
}

QString WaveCanvas::rangeTransferTargetSummary() const
{
    if (!scenario_ || waveEditRangeTargetLaneIds_.empty()) return {};
    const auto blockLabel = [this](
                                const std::vector<std::string>& laneIds) {
        if (laneIds.empty()) return QString{};
        const auto* first = findLane(*scenario_, laneIds.front());
        const auto* last = findLane(*scenario_, laneIds.back());
        if (!first || !last) return QString{};
        if (laneIds.size() == 1) {
            return QString::fromStdString(first->name);
        }
        return tr("%1…%2 (%3)")
            .arg(
                QString::fromStdString(first->name),
                QString::fromStdString(last->name))
            .arg(static_cast<qulonglong>(laneIds.size()));
    };
    const auto target = blockLabel(waveEditRangeTargetLaneIds_);
    if (waveEditRangeTargetLaneIds_ == selectedLaneIds_) return target;
    const auto source = blockLabel(selectedLaneIds_);
    return source.isEmpty() || target.isEmpty()
        ? QString{}
        : tr("%1 → %2").arg(source, target);
}

std::optional<WaveCanvas::RangeTransferProjection>
WaveCanvas::buildRangeTransferProjection() const
{
    if (!scenario_ || !project_
        || !waveEditOriginalRange_
        || !waveEditPreviewRange_
        || selectedLaneIds_.empty()
        || !waveEditRangeTargetValid_
        || (waveEditCopyDrag_ && rangeTransferTargetOverlapsSource())) {
        return std::nullopt;
    }

    const auto [sourceStart, sourceEnd] = *waveEditOriginalRange_;
    const auto [targetStart, targetEnd] = *waveEditPreviewRange_;
    const auto duration = sourceEnd - sourceStart;
    const auto targetLaneIds = waveEditRangeTargetLaneIds_.empty()
        ? selectedLaneIds_
        : waveEditRangeTargetLaneIds_;
    if (sourceStart < 0
        || sourceEnd <= sourceStart
        || sourceEnd > scenario_->duration
        || targetStart < 0
        || targetEnd <= targetStart
        || targetEnd - targetStart != duration
        || targetLaneIds.size() != selectedLaneIds_.size()) {
        return std::nullopt;
    }

    RangeTransferProjection projection;
    projection.copy = waveEditCopyDrag_;
    projection.sourceStart = sourceStart;
    projection.targetStart = targetStart;
    projection.duration = duration;
    projection.targetLaneIds = targetLaneIds;
    projection.copiedLanes = captureLaneRanges(
        *scenario_,
        selectedLaneIds_,
        sourceStart,
        sourceEnd,
        !projection.copy);
    if (projection.copiedLanes.size() != selectedLaneIds_.size()) {
        return std::nullopt;
    }
    for (std::size_t index = 0;
         index < projection.copiedLanes.size();
         ++index) {
        projection.copiedLanes[index].sourceLaneId =
            selectedLaneIds_[index];
        projection.copiedLanes[index].laneId =
            targetLaneIds[index];
    }

    std::vector<std::string> affectedLaneIds;
    affectedLaneIds.reserve(projection.copiedLanes.size() * 2);
    std::unordered_map<std::string, Lane> projectedByLaneId;
    projectedByLaneId.reserve(projection.copiedLanes.size() * 2);
    const auto rememberLane =
        [this, &affectedLaneIds, &projectedByLaneId](
            const std::string& laneId) {
            if (projectedByLaneId.contains(laneId)) return true;
            const auto* lane = findLane(*scenario_, laneId);
            if (!lane || lane->kind == LaneKind::Group) return false;
            affectedLaneIds.push_back(laneId);
            projectedByLaneId.emplace(laneId, *lane);
            return true;
        };
    for (const auto& copiedLane : projection.copiedLanes) {
        if (!rememberLane(copiedLane.sourceLaneId)
            || !rememberLane(copiedLane.laneId)) {
            return std::nullopt;
        }
    }

    std::unordered_set<std::string> reservedSegmentIds;
    for (const auto& lane : scenario_->lanes) {
        for (const auto& segment : lane.segments) {
            reservedSegmentIds.insert(segment.id);
        }
    }
    std::size_t previewSegmentSequence = 0;
    const auto nextSegmentId =
        [&previewSegmentSequence, &reservedSegmentIds] {
            std::string candidate;
            do {
                candidate =
                    std::string{"__range_transfer_preview_segment_"}
                    + std::to_string(++previewSegmentSequence);
            } while (reservedSegmentIds.contains(candidate));
            reservedSegmentIds.insert(candidate);
            return candidate;
        };

    if (!projection.copy) {
        for (const auto& copiedLane : projection.copiedLanes) {
            clearProjectedSegmentRange(
                projectedByLaneId.at(copiedLane.sourceLaneId),
                sourceStart,
                sourceEnd,
                nextSegmentId);
        }
    }
    for (const auto& copiedLane : projection.copiedLanes) {
        clearProjectedSegmentRange(
            projectedByLaneId.at(copiedLane.laneId),
            targetStart,
            targetEnd,
            nextSegmentId);
    }
    for (const auto& copiedLane : projection.copiedLanes) {
        auto& targetLane = projectedByLaneId.at(copiedLane.laneId);
        for (const auto& relative : copiedLane.relativeSegments) {
            const auto start = targetStart + relative.start;
            const auto end = targetStart + relative.end;
            if (end <= start) continue;
            const auto validation =
                validateLaneValue(targetLane, relative.value);
            if (!validation.valid) return std::nullopt;

            std::string segmentId;
            if (!projection.copy
                && copiedLane.sourceLaneId == copiedLane.laneId
                && !relative.id.empty()) {
                const auto idStillUsed = std::any_of(
                    targetLane.segments.begin(),
                    targetLane.segments.end(),
                    [&relative](const Segment& segment) {
                        return segment.id == relative.id;
                    });
                if (!idStillUsed) segmentId = relative.id;
            }
            setProjectedSegmentRange(
                targetLane,
                start,
                end,
                validation.normalizedValue,
                std::move(segmentId),
                relative.extensions,
                nextSegmentId);
        }
    }

    projection.lanes.reserve(affectedLaneIds.size());
    for (const auto& laneId : affectedLaneIds) {
        auto projected = std::move(projectedByLaneId.at(laneId));
        const auto* original = findLane(*scenario_, laneId);
        if (!original) return std::nullopt;
        projection.waveformChanges =
            projection.waveformChanges
            || !sameProjectedWaveform(projected, *original);
        projection.lanes.push_back(std::move(projected));
    }
    projection.relationImpact =
        relationRemovalImpactForProjectedLanes(projection.lanes);
    projection.extendsEnd = targetEnd > scenario_->duration;
    projection.modelChanges =
        projection.waveformChanges
        || !projection.relationImpact.ids.empty()
        || projection.extendsEnd;
    return projection;
}

QString WaveCanvas::rangeTransferPreviewStatus(
    const RangeTransferProjection& projection) const
{
    const auto operation = projection.copy ? tr("Copy") : tr("Move");
    auto message = projection.copy
        ? tr("Copy range preview · source remains")
        : tr("Move range preview · source clears on release");
    if (!projection.modelChanges) {
        message += tr(" · no %1 needed · waveform, Relation, and End already match · source selection stays")
                       .arg(operation);
    } else if (!projection.relationImpact.ids.empty()) {
        message += tr(" · removes %1 relation(s)")
                       .arg(static_cast<qulonglong>(
                           projection.relationImpact.ids.size()));
        if (!projection.relationImpact.summaries.isEmpty()) {
            message += tr(" · affected: %1")
                           .arg(
                               projection.relationImpact.summaries.join(
                                   QStringLiteral("; ")));
        }
    } else {
        message += tr(" · no Relation will be removed");
    }
    if (projection.extendsEnd) {
        message += tr(" · End → %1")
                       .arg(QString::fromStdString(formatTick(
                           projection.targetStart + projection.duration,
                           project_->timeBase)));
    }
    const auto targetSummary = rangeTransferTargetSummary();
    if (!targetSummary.isEmpty()) {
        message += tr(" · %1").arg(targetSummary);
    }
    return message;
}

std::optional<WaveCanvas::SegmentEditProjection>
WaveCanvas::buildSegmentEditProjection() const
{
    const auto interaction = waveEditInteraction_;
    if (!scenario_ || !project_ || !drawing_
        || !waveEditOriginalRange_
        || !waveEditPreviewRange_
        || selectedSegmentLaneId_.empty()
        || selectedSegmentId_.empty()
        || (interaction != WaveEditInteraction::MoveSegment
            && interaction != WaveEditInteraction::ResizeStart
            && interaction != WaveEditInteraction::ResizeEnd)) {
        return std::nullopt;
    }

    const auto [originalStart, originalEnd] = *waveEditOriginalRange_;
    const auto [start, end] = *waveEditPreviewRange_;
    const auto operation =
        interaction == WaveEditInteraction::ResizeStart
        ? SegmentEditOperation::ResizeStart
        : interaction == WaveEditInteraction::ResizeEnd
            ? SegmentEditOperation::ResizeEnd
            : waveEditCopyDrag_
                ? SegmentEditOperation::Copy
                : SegmentEditOperation::Move;
    return buildSegmentEditProjection(
        operation,
        selectedSegmentLaneId_,
        selectedSegmentId_,
        originalStart,
        originalEnd,
        start,
        end);
}

std::optional<WaveCanvas::SegmentEditProjection>
WaveCanvas::buildSegmentEditProjection(
    const SegmentEditOperation operation,
    const std::string& laneId,
    const std::string& segmentId,
    const Tick originalStart,
    const Tick originalEnd,
    const Tick start,
    const Tick end) const
{
    if (!scenario_ || !project_ || laneId.empty() || segmentId.empty()) {
        return std::nullopt;
    }
    const auto* sourceLane = findLane(*scenario_, laneId);
    const auto* sourceSegment = sourceLane
        ? segmentById(laneId, segmentId)
        : nullptr;
    if (!sourceLane || !sourceSegment
        || sourceLane->kind == LaneKind::Group
        || originalStart < 0
        || originalEnd <= originalStart
        || start < 0
        || end <= start
        || end > scenario_->duration
        || sourceSegment->start != originalStart
        || sourceSegment->end != originalEnd) {
        return std::nullopt;
    }

    SegmentEditProjection projection;
    projection.operation = operation;
    projection.laneId = sourceLane->id;
    projection.sourceSegmentId = sourceSegment->id;
    projection.originalStart = originalStart;
    projection.originalEnd = originalEnd;
    projection.start = start;
    projection.end = end;
    projection.lane = *sourceLane;

    if (projection.operation == SegmentEditOperation::Copy) {
        if (sourceLane->kind == LaneKind::Clock) return std::nullopt;
        if (!projectedRangeAlreadyEquals(
                projection.lane,
                start,
                end,
                sourceSegment->value,
                sourceSegment->extensions)) {
            std::unordered_set<std::string> reservedSegmentIds;
            for (const auto& lane : scenario_->lanes) {
                for (const auto& segment : lane.segments) {
                    reservedSegmentIds.insert(segment.id);
                }
            }
            std::size_t previewSegmentSequence = 0;
            const auto nextSegmentId =
                [&previewSegmentSequence, &reservedSegmentIds] {
                    std::string candidate;
                    do {
                        candidate =
                            std::string{"__segment_edit_preview_segment_"}
                            + std::to_string(++previewSegmentSequence);
                    } while (reservedSegmentIds.contains(candidate));
                    reservedSegmentIds.insert(candidate);
                    return candidate;
                };
            setProjectedSegmentRange(
                projection.lane,
                start,
                end,
                sourceSegment->value,
                {},
                sourceSegment->extensions,
                nextSegmentId);
        }
    } else {
        auto projectedSegment = std::find_if(
            projection.lane.segments.begin(),
            projection.lane.segments.end(),
            [sourceSegment](const Segment& segment) {
                return segment.id == sourceSegment->id;
            });
        if (projectedSegment == projection.lane.segments.end()) {
            return std::nullopt;
        }
        const auto index = static_cast<std::size_t>(
            std::distance(
                projection.lane.segments.begin(),
                projectedSegment));
        auto* previous = index > 0
            ? &projection.lane.segments.at(index - 1)
            : nullptr;
        auto* next = index + 1 < projection.lane.segments.size()
            ? &projection.lane.segments.at(index + 1)
            : nullptr;
        const auto previousTouches =
            previous && previous->end == originalStart;
        const auto nextTouches =
            next && next->start == originalEnd;
        if ((previousTouches && start <= previous->start)
            || (!previousTouches && previous && start < previous->end)
            || (nextTouches && end >= next->end)
            || (!nextTouches && next && end > next->start)) {
            return std::nullopt;
        }
        if (previousTouches) previous->end = start;
        if (nextTouches) next->start = end;
        projectedSegment->start = start;
        projectedSegment->end = end;
        normalizeProjectedSegments(projection.lane);
    }

    projection.waveformChanges =
        !sameProjectedWaveform(projection.lane, *sourceLane);
    projection.relationImpact =
        relationRemovalImpactForProjectedLanes({projection.lane});
    projection.modelChanges =
        projection.waveformChanges
        || !projection.relationImpact.ids.empty();
    return projection;
}

std::optional<WaveCanvas::SegmentEditProjection>
WaveCanvas::activeSegmentEditProjection() const
{
    if (drawing_) return buildSegmentEditProjection();
    if (!segmentActionPreview_
        || selectedSegmentLaneId_
            != segmentActionPreview_->laneId
        || selectedSegmentId_
            != segmentActionPreview_->sourceSegmentId) {
        return std::nullopt;
    }
    const auto* segment = segmentById(
        segmentActionPreview_->laneId,
        segmentActionPreview_->sourceSegmentId);
    if (!segment
        || segment->start != segmentActionPreview_->originalStart
        || segment->end != segmentActionPreview_->originalEnd) {
        return std::nullopt;
    }
    return segmentActionPreview_;
}

WaveCanvas::SegmentActionAssessment
WaveCanvas::assessSelectedSegmentAction(const SegmentAction action) const
{
    SegmentActionAssessment assessment;
    auto& state = assessment.state;
    const auto actionName = [this, action] {
        switch (action) {
        case SegmentAction::DuplicateBefore:
            return tr("Duplicate Segment before");
        case SegmentAction::DuplicateAfter:
            return tr("Duplicate Segment after");
        case SegmentAction::MoveEarlier:
            return tr("Move Segment earlier");
        case SegmentAction::MoveLater:
            return tr("Move Segment later");
        case SegmentAction::ExpandStart:
            return tr("Expand Segment left boundary");
        case SegmentAction::TrimStart:
            return tr("Trim Segment left boundary");
        case SegmentAction::ExpandEnd:
            return tr("Expand Segment right boundary");
        case SegmentAction::TrimEnd:
            return tr("Trim Segment right boundary");
        }
        return tr("Segment action");
    }();
    state.summary =
        tr("%1 unavailable · select an explicit Segment in Wave Edit")
            .arg(actionName);
    if (!scenario_ || !project_ || tool_ != Tool::WaveEdit
        || selectedSegmentLaneId_.empty()
        || selectedSegmentId_.empty()) {
        return assessment;
    }

    const auto* lane = findLane(*scenario_, selectedSegmentLaneId_);
    const auto* segment = lane
        ? segmentById(selectedSegmentLaneId_, selectedSegmentId_)
        : nullptr;
    if (!lane || !segment || lane->kind == LaneKind::Group) {
        state.summary =
            tr("%1 unavailable · the selected Segment no longer exists")
                .arg(actionName);
        return assessment;
    }

    const auto duplicate =
        action == SegmentAction::DuplicateBefore
        || action == SegmentAction::DuplicateAfter;
    if (duplicate
        && lane->kind != LaneKind::Bus
        && lane->kind != LaneKind::Enum) {
        state.summary =
            tr("%1 unavailable · only Bus and Enum Segments can be copied")
                .arg(actionName);
        return assessment;
    }
    state.applicable = true;

    const auto current = std::find_if(
        lane->segments.begin(),
        lane->segments.end(),
        [this](const Segment& candidate) {
            return candidate.id == selectedSegmentId_;
        });
    if (current == lane->segments.end()) {
        state.summary =
            tr("%1 unavailable · the selected Segment no longer exists")
                .arg(actionName);
        return assessment;
    }
    const auto index = static_cast<std::size_t>(
        std::distance(lane->segments.begin(), current));
    const auto width = segment->end - segment->start;
    const auto unit = minimumWaveEditUnit(*lane);
    auto targetStart = segment->start;
    auto targetEnd = segment->end;
    auto operation = SegmentEditOperation::Move;
    auto blockedReason = QString{};

    switch (action) {
    case SegmentAction::DuplicateBefore:
        operation = SegmentEditOperation::Copy;
        if (width <= 0 || segment->start < width) {
            blockedReason =
                tr("the previous interval would precede timeline start");
        } else {
            targetStart = segment->start - width;
            targetEnd = segment->start;
        }
        break;
    case SegmentAction::DuplicateAfter:
        operation = SegmentEditOperation::Copy;
        if (width <= 0
            || segment->end > scenario_->duration
            || width > scenario_->duration - segment->end) {
            blockedReason =
                tr("the next interval exceeds End");
        } else {
            targetStart = segment->end;
            targetEnd = segment->end + width;
        }
        break;
    case SegmentAction::MoveEarlier:
    case SegmentAction::MoveLater: {
        operation = SegmentEditOperation::Move;
        auto lower = Tick{0};
        auto upper = scenario_->duration - width;
        if (index > 0) {
            const auto& previous = lane->segments.at(index - 1);
            lower = previous.end == segment->start
                ? previous.start + unit
                : previous.end;
        }
        if (index + 1 < lane->segments.size()) {
            const auto& next = lane->segments.at(index + 1);
            upper = next.start == segment->end
                ? next.end - width - unit
                : next.start - width;
        }
        const auto forward = action == SegmentAction::MoveLater;
        if ((!forward && segment->start < unit)
            || (forward
                && segment->start
                    > scenario_->duration - std::min(
                          unit,
                          scenario_->duration))) {
            blockedReason = forward
                ? tr("timeline End blocks the next editing step")
                : tr("timeline start blocks the previous editing step");
            break;
        }
        targetStart = forward
            ? segment->start + unit
            : segment->start - unit;
        targetEnd = targetStart + width;
        if (upper < lower
            || targetStart < lower
            || targetStart > upper) {
            blockedReason = forward
                ? tr("next content or timeline End blocks the next editing step")
                : tr("previous content or timeline start blocks the previous editing step");
        }
        break;
    }
    case SegmentAction::ExpandStart:
    case SegmentAction::TrimStart: {
        operation = SegmentEditOperation::ResizeStart;
        auto lower = Tick{0};
        if (index > 0) {
            const auto& previous = lane->segments.at(index - 1);
            lower = previous.end == segment->start
                ? previous.start + unit
                : previous.end;
        }
        const auto upper = segment->end - unit;
        const auto expand = action == SegmentAction::ExpandStart;
        if (expand && segment->start < unit) {
            blockedReason =
                tr("timeline start blocks the previous editing step");
            break;
        }
        targetStart = expand
            ? segment->start - unit
            : segment->start + unit;
        if (upper < lower
            || targetStart < lower
            || targetStart > upper) {
            blockedReason =
                tr("adjacent content or minimum Segment width blocks this boundary");
        }
        break;
    }
    case SegmentAction::ExpandEnd:
    case SegmentAction::TrimEnd: {
        operation = SegmentEditOperation::ResizeEnd;
        const auto lower = segment->start + unit;
        auto upper = scenario_->duration;
        if (index + 1 < lane->segments.size()) {
            const auto& next = lane->segments.at(index + 1);
            upper = next.start == segment->end
                ? next.end - unit
                : next.start;
        }
        const auto expand = action == SegmentAction::ExpandEnd;
        if (expand
            && segment->end
                > scenario_->duration - std::min(
                      unit,
                      scenario_->duration)) {
            blockedReason =
                tr("timeline End blocks the next editing step");
            break;
        }
        targetEnd = expand
            ? segment->end + unit
            : segment->end - unit;
        if (upper < lower
            || targetEnd < lower
            || targetEnd > upper) {
            blockedReason =
                tr("adjacent content or minimum Segment width blocks this boundary");
        }
        break;
    }
    }

    const auto formatTime = [this](const Tick tick) {
        return QString::fromStdString(formatTick(
            tick,
            project_->timeBase));
    };
    if (!blockedReason.isEmpty()) {
        state.summary =
            tr("%1 unavailable · %2 · current %3–%4 · step %5")
                .arg(actionName)
                .arg(blockedReason)
                .arg(formatTime(segment->start))
                .arg(formatTime(segment->end))
                .arg(formatTime(duplicate ? width : unit));
        return assessment;
    }

    assessment.projection = buildSegmentEditProjection(
        operation,
        lane->id,
        segment->id,
        segment->start,
        segment->end,
        targetStart,
        targetEnd);
    if (!assessment.projection) {
        state.summary =
            tr("%1 unavailable · the target no longer satisfies Segment constraints")
                .arg(actionName);
        return assessment;
    }

    state.valid = true;
    state.modelChanges = assessment.projection->modelChanges;
    state.start = targetStart;
    state.end = targetEnd;
    state.relationRemovalCount =
        assessment.projection->relationImpact.ids.size();
    state.summary =
        tr("%1 preview · %2 · %3–%4 → %5–%6 · width %7")
            .arg(actionName)
            .arg(QString::fromStdString(lane->name))
            .arg(formatTime(segment->start))
            .arg(formatTime(segment->end))
            .arg(formatTime(targetStart))
            .arg(formatTime(targetEnd))
            .arg(formatTime(targetEnd - targetStart));
    if (!duplicate) {
        state.summary += tr(" · step %1").arg(formatTime(unit));
    }
    if (!state.modelChanges) {
        state.summary +=
            tr(" · no change needed · waveform and Relation already match · no command will run");
    } else if (state.relationRemovalCount > 0) {
        state.summary += tr(" · removes %1 relation(s)")
                             .arg(static_cast<qulonglong>(
                                 state.relationRemovalCount));
        if (!assessment.projection->relationImpact.summaries.isEmpty()) {
            state.summary += tr(" · affected: %1")
                                 .arg(
                                     assessment.projection->relationImpact
                                         .summaries.join(
                                             QStringLiteral("; ")));
        }
        state.summary +=
            tr(" · hover previews final waveform · click to apply");
    } else {
        state.summary +=
            tr(" · no Relation will be removed · hover previews final waveform · click to apply");
    }
    return assessment;
}

QString WaveCanvas::segmentEditPreviewStatus(
    const SegmentEditProjection& projection) const
{
    const auto operation =
        projection.operation == SegmentEditOperation::Copy
        ? tr("Copy")
        : projection.operation == SegmentEditOperation::Move
            ? tr("Move")
            : tr("Resize");
    auto message =
        projection.operation == SegmentEditOperation::Copy
        ? tr("Copy Segment preview · source remains")
        : projection.operation == SegmentEditOperation::Move
            ? tr("Move Segment preview · source moves on release")
            : projection.operation == SegmentEditOperation::ResizeStart
                ? tr("Resize Segment start preview")
                : tr("Resize Segment end preview");
    if (!projection.modelChanges) {
        message += tr(" · no %1 needed · waveform and Relation already match · source selection stays")
                       .arg(operation);
    } else if (!projection.relationImpact.ids.empty()) {
        message += tr(" · removes %1 relation(s)")
                       .arg(static_cast<qulonglong>(
                           projection.relationImpact.ids.size()));
        if (!projection.relationImpact.summaries.isEmpty()) {
            message += tr(" · affected: %1")
                           .arg(
                               projection.relationImpact.summaries.join(
                                   QStringLiteral("; ")));
        }
    } else {
        message += tr(" · no Relation will be removed");
    }
    return message;
}

void WaveCanvas::updateLaneDropTarget(const int y)
{
    laneDropDestinationIndex_.reset();
    laneDropInsertionSlot_.reset();
    laneDropIndicatorY_.reset();
    laneDropGroupId_.clear();
    if (!scenario_ || scenario_->lanes.empty() || laneDragId_.empty()) return;

    const auto dragIds = laneDragIds_.empty()
        ? std::vector<std::string>{laneDragId_}
        : laneDragIds_;
    const auto selected = [&dragIds](const std::string& laneId) {
        return std::find(dragIds.begin(), dragIds.end(), laneId)
            != dragIds.end();
    };
    const auto batchDrag = dragIds.size() > 1;
    const auto source = std::find_if(
        scenario_->lanes.begin(),
        scenario_->lanes.end(),
        [this](const Lane& lane) { return lane.id == laneDragId_; });
    if (source == scenario_->lanes.end() || laneLayout_.empty()) return;
    if (batchDrag
        && std::any_of(
            dragIds.begin(),
            dragIds.end(),
            [this](const std::string& laneId) {
                const auto* lane = findLane(*scenario_, laneId);
                return !lane || lane->kind == LaneKind::Group;
            })) {
        return;
    }
    const auto sourceIndex = static_cast<std::size_t>(
        std::distance(scenario_->lanes.begin(), source));

    if (source->kind != LaneKind::Group) {
        const auto* targetLayout = layoutAtY(y);
        const auto* target = targetLayout
            ? &scenario_->lanes.at(targetLayout->laneIndex)
            : nullptr;
        if (target
            && target->kind == LaneKind::Group
            && target->id != source->id) {
            laneDropGroupId_ = target->id;
            return;
        }
    }

    auto insertionSlot = scenario_->lanes.size();
    auto indicatorY = RulerHeight;
    auto found = false;
    for (const auto& layout : laneLayout_) {
        const auto screenTop = RulerHeight + layout.top - verticalScrollBar()->value();
        const auto center = screenTop + layout.height / 2;
        if (y < center) {
            insertionSlot = layout.laneIndex;
            indicatorY = screenTop;
            found = true;
            break;
        }
    }
    if (!found) {
        const auto& last = laneLayout_.back();
        insertionSlot = std::min(scenario_->lanes.size(), last.laneIndex + 1);
        indicatorY = RulerHeight + last.top + last.height
            - verticalScrollBar()->value();
    }

    insertionSlot = std::min(insertionSlot, scenario_->lanes.size());
    laneDropInsertionSlot_ = insertionSlot;
    if (batchDrag) {
        const auto selectedBeforeSlot = static_cast<std::size_t>(
            std::count_if(
                scenario_->lanes.begin(),
                scenario_->lanes.begin()
                    + static_cast<std::ptrdiff_t>(insertionSlot),
                [&selected](const Lane& lane) {
                    return selected(lane.id);
                }));
        const auto remainingLaneCount = scenario_->lanes.size() - dragIds.size();
        laneDropDestinationIndex_ = std::min(
            insertionSlot - selectedBeforeSlot,
            remainingLaneCount);
        laneDropIndicatorY_ = std::clamp(
            indicatorY,
            RulerHeight,
            std::max(RulerHeight, viewport()->height() - 1));
        return;
    }
    auto destination = insertionSlot;
    if (destination > sourceIndex) --destination;
    destination = std::min(destination, scenario_->lanes.size() - 1);
    laneDropDestinationIndex_ = destination;
    laneDropIndicatorY_ = std::clamp(
        indicatorY,
        RulerHeight,
        std::max(RulerHeight, viewport()->height() - 1));
}

void WaveCanvas::commitLaneReorder()
{
    if (!scenario_
        || !commandStack_
        || laneDragId_.empty()) {
        return;
    }
    const auto source = std::find_if(
        scenario_->lanes.begin(),
        scenario_->lanes.end(),
        [this](const Lane& lane) { return lane.id == laneDragId_; });
    if (source == scenario_->lanes.end()) return;
    const auto sourceIndex = static_cast<std::size_t>(
        std::distance(scenario_->lanes.begin(), source));
    const auto laneName = QString::fromStdString(source->name);
    const auto dragIds = laneDragIds_.empty()
        ? std::vector<std::string>{laneDragId_}
        : laneDragIds_;
    const auto selected = [&dragIds](const std::string& laneId) {
        return std::find(dragIds.begin(), dragIds.end(), laneId)
            != dragIds.end();
    };
    const auto batchDrag = dragIds.size() > 1;
    if (!laneDropGroupId_.empty()) {
        const auto target = std::find_if(
            scenario_->lanes.begin(),
            scenario_->lanes.end(),
            [this](const Lane& lane) {
                return lane.id == laneDropGroupId_
                    && lane.kind == LaneKind::Group;
            });
        if (target == scenario_->lanes.end()) {
            emit statusMessage(tr("Move cancelled 路 the target group no longer exists"));
            return;
        }
        const auto targetName = QString::fromStdString(target->name);
        const auto targetId = target->id;
        const auto movedCount = static_cast<std::size_t>(std::count_if(
            scenario_->lanes.begin(),
            scenario_->lanes.end(),
            [&selected, &targetId](const Lane& lane) {
                return selected(lane.id)
                    && lane.groupId != targetId;
            }));
        const auto alreadyThere = dragIds.size() - movedCount;
        const auto existingMembers = static_cast<std::size_t>(std::count_if(
            scenario_->lanes.begin(),
            scenario_->lanes.end(),
            [&selected, &targetId](const Lane& lane) {
                return !selected(lane.id)
                    && lane.groupId == targetId;
            }));
        try {
            const auto changed = batchDrag
                ? commandStack_->execute(
                    std::make_unique<SetLanesGroupCommand>(
                        *scenario_,
                        dragIds,
                        targetId))
                : commandStack_->execute(
                    std::make_unique<SetLaneGroupCommand>(
                        *scenario_,
                        laneDragId_,
                        targetId));
            if (!changed) {
                emit statusMessage(
                    batchDrag
                        ? tr("All %1 selected signals are already in group %2 · no order changed")
                              .arg(
                                  static_cast<qulonglong>(dragIds.size()))
                              .arg(targetName)
                        : tr("%1 is already in group %2 · no order changed")
                              .arg(laneName, targetName));
                return;
            }
        } catch (const std::exception& exception) {
            QToolTip::showText(
                viewport()->mapToGlobal(laneHeaderPressPosition_),
                QString::fromUtf8(exception.what()),
                viewport());
            return;
        }
        rebuildLaneLayout();
        if (batchDrag) {
            QStringList ids;
            ids.reserve(static_cast<qsizetype>(dragIds.size()));
            for (const auto& laneId : dragIds) {
                ids.push_back(QString::fromStdString(laneId));
            }
            selectLaneHeaders(
                ids,
                QString::fromStdString(laneDragId_));
        } else {
            revealLane(QString::fromStdString(laneDragId_));
        }
        emit modelEdited();
        emit commandAvailabilityChanged();
        if (batchDrag) {
            emit statusMessage(
                tr("Moved %1 selected signals to group %2 · %3 moved · %4 already there · placed after %5 unselected member signal(s) · Ctrl+Z to undo")
                    .arg(static_cast<qulonglong>(dragIds.size()))
                    .arg(targetName)
                    .arg(static_cast<qulonglong>(movedCount))
                    .arg(static_cast<qulonglong>(alreadyThere))
                    .arg(static_cast<qulonglong>(existingMembers)));
        } else {
            emit statusMessage(
                tr("Moved %1 to group %2 · placed after %3 existing member signal(s) · Ctrl+Z to undo")
                    .arg(laneName, targetName)
                    .arg(static_cast<qulonglong>(existingMembers)));
        }
        return;
    }
    if (batchDrag) {
        if (!laneDropInsertionSlot_ || !laneDropDestinationIndex_) return;
        const auto destinationIndex = *laneDropDestinationIndex_;
        try {
            if (!commandStack_->execute(std::make_unique<MoveLanesCommand>(
                    *scenario_,
                    dragIds,
                    *laneDropInsertionSlot_))) {
                emit statusMessage(
                    tr("%1 selected signals remain in place · no order changed")
                        .arg(static_cast<qulonglong>(dragIds.size())));
                return;
            }
        } catch (const std::exception& exception) {
            QToolTip::showText(
                viewport()->mapToGlobal(laneHeaderPressPosition_),
                QString::fromUtf8(exception.what()),
                viewport());
            return;
        }
        rebuildLaneLayout();
        rebuildSnapIndex();
        updateScrollBars();
        QStringList ids;
        ids.reserve(static_cast<qsizetype>(dragIds.size()));
        for (const auto& laneId : dragIds) {
            ids.push_back(QString::fromStdString(laneId));
        }
        selectLaneHeaders(
            ids,
            QString::fromStdString(laneDragId_));
        emit modelEdited();
        emit commandAvailabilityChanged();
        emit statusMessage(
            tr("Moved %1 selected signals to positions %2–%3 · relative order and Group memberships kept · Ctrl+Z to undo")
                .arg(static_cast<qulonglong>(dragIds.size()))
                .arg(destinationIndex + 1)
                .arg(destinationIndex + dragIds.size()));
        return;
    }
    if (!laneDropDestinationIndex_) return;
    if (sourceIndex == *laneDropDestinationIndex_) {
        emit statusMessage(
            tr("%1 remains at position %2. No order changed.")
                .arg(laneName)
                .arg(sourceIndex + 1));
        return;
    }
    const auto destinationIndex = *laneDropDestinationIndex_;

    try {
        commandStack_->execute(std::make_unique<MoveLaneCommand>(
            *scenario_,
            laneDragId_,
            destinationIndex));
    } catch (const std::exception& exception) {
        QToolTip::showText(
            viewport()->mapToGlobal(laneHeaderPressPosition_),
            QString::fromUtf8(exception.what()),
            viewport());
        return;
    }
    rebuildLaneLayout();
    rebuildSnapIndex();
    updateScrollBars();
    emit modelEdited();
    emit commandAvailabilityChanged();
    emit selectionChanged(QString::fromStdString(laneDragId_), cursorTick_);
    emit statusMessage(
        tr("Moved %1: position %2 -> %3. Ctrl+Z to undo.")
            .arg(laneName)
            .arg(sourceIndex + 1)
            .arg(destinationIndex + 1));
}

void WaveCanvas::drawLaneReorderOverlay(QPainter& painter)
{
    if (!laneHeaderDragging_) return;

    const QColor accent(111, 168, 255);
    if (!laneDropGroupId_.empty() && scenario_) {
        const auto layout = std::find_if(
            laneLayout_.begin(),
            laneLayout_.end(),
            [this](const LaneLayout& candidate) {
                return scenario_->lanes.at(candidate.laneIndex).id
                    == laneDropGroupId_;
            });
        const auto* source = findLane(*scenario_, laneDragId_);
        const auto* target = findLane(*scenario_, laneDropGroupId_);
        if (layout == laneLayout_.end() || !source || !target) return;
        const QRect row(
            3,
            RulerHeight + layout->top - verticalScrollBar()->value() + 2,
            std::max(1, viewport()->width() - 6),
            std::max(1, layout->height - 4));
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(accent, 2.0));
        painter.setBrush(QColor(accent.red(), accent.green(), accent.blue(), 52));
        painter.drawRoundedRect(row, 5, 5);
        auto font = painter.font();
        font.setBold(true);
        painter.setFont(font);
        painter.setPen(kTextPrimary);
        const auto dragCount = laneDragIds_.empty()
            ? std::size_t{1}
            : laneDragIds_.size();
        const auto text = dragCount > 1
            ? tr("Move %1 selected signals into %2")
                  .arg(static_cast<qulonglong>(dragCount))
                  .arg(QString::fromStdString(target->name))
            : tr("Move %1 into %2")
                  .arg(
                      QString::fromStdString(source->name),
                      QString::fromStdString(target->name));
        painter.drawText(
            row.adjusted(14, 2, -14, -2),
            Qt::AlignCenter,
            painter.fontMetrics().elidedText(
                text,
                Qt::ElideRight,
                std::max(1, row.width() - 28)));
        painter.restore();
        return;
    }
    if (!laneDropIndicatorY_) return;

    const auto y = *laneDropIndicatorY_;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(accent, 2.5));
    painter.drawLine(8, y, viewport()->width() - 8, y);
    painter.setPen(Qt::NoPen);
    painter.setBrush(accent);
    painter.drawPolygon(QPolygon{
        QPoint(headerWidth_ - 2, y),
        QPoint(headerWidth_ - 11, y - 6),
        QPoint(headerWidth_ - 11, y + 6),
    });
    const auto dragCount = laneDragIds_.empty()
        ? std::size_t{1}
        : laneDragIds_.size();
    if (dragCount > 1) {
        const auto text = tr("Move %1 selected signals")
            .arg(static_cast<qulonglong>(dragCount));
        auto font = painter.font();
        font.setBold(true);
        painter.setFont(font);
        const auto textWidth = painter.fontMetrics().horizontalAdvance(text);
        const auto labelWidth = textWidth + 20;
        const auto labelHeight = painter.fontMetrics().height() + 10;
        const auto labelX = std::clamp(
            headerWidth_ + 12,
            4,
            std::max(4, viewport()->width() - labelWidth - 4));
        const auto labelY = std::clamp(
            y - labelHeight - 5,
            RulerHeight + 3,
            std::max(RulerHeight + 3, viewport()->height() - labelHeight - 3));
        const QRect labelRect(labelX, labelY, labelWidth, labelHeight);
        painter.setPen(QPen(accent, 1.5));
        painter.setBrush(QColor(20, 28, 40, 232));
        painter.drawRoundedRect(labelRect, 5, 5);
        painter.setPen(kTextPrimary);
        painter.drawText(labelRect, Qt::AlignCenter, text);
    }
    painter.restore();
}

void WaveCanvas::setScale(const double scale, const int anchorX)
{
    if (!scenario_) return;
    pendingVisibleTimeSpanRestore_.reset();
    const auto clampedAnchor = std::clamp(anchorX, headerWidth_, viewport()->width());
    const auto anchorTick = tickAtX(clampedAnchor);
    pixelsPerTick_ = std::clamp(scale, 1.0e-9, 100.0);
    updateScrollBars();
    const auto newScroll = static_cast<double>(anchorTick) * pixelsPerTick_
        - static_cast<double>(clampedAnchor - headerWidth_);
    horizontalScrollBar()->setValue(static_cast<int>(std::clamp(
        std::llround(newScroll),
        0LL,
        static_cast<long long>(horizontalScrollBar()->maximum()))));
    positionBusPresetPalette();
    viewport()->update();
}

void WaveCanvas::schedulePendingVisibleTimeSpanRestore()
{
    if (pendingVisibleTimeSpanRestoreScheduled_
        || !pendingVisibleTimeSpanRestore_) {
        return;
    }
    pendingVisibleTimeSpanRestoreScheduled_ = true;
    QTimer::singleShot(0, this, [this] {
        pendingVisibleTimeSpanRestoreScheduled_ = false;
        if (!pendingVisibleTimeSpanRestore_ || !isVisible()) return;
        const auto [span, anchorTick] = *pendingVisibleTimeSpanRestore_;
        pendingVisibleTimeSpanRestore_.reset();
        applyVisibleTimeSpan(span, anchorTick);
    });
}

int WaveCanvas::zoomAnchorX() const
{
    const auto center = headerWidth_ + waveViewportWidth() / 2;
    if (!scenario_) return center;
    const auto cursorX = xAtTick(cursorTick_);
    return cursorX >= headerWidth_ && cursorX <= viewport()->width()
        ? cursorX
        : center;
}

double WaveCanvas::contentWidth() const
{
    return scenario_
        ? std::max(0.0, static_cast<double>(scenario_->duration) * pixelsPerTick_)
        : 0.0;
}

int WaveCanvas::waveViewportWidth() const
{
    return std::max(1, viewport()->width() - headerWidth_);
}

Tick WaveCanvas::tickAtX(const int x) const
{
    if (!scenario_) return 0;
    const TimelineViewport timeline(
        0,
        scenario_->duration,
        pixelsPerTick_,
        headerWidth_,
        horizontalScrollBar()->value());
    return timeline.tickAtPixel(x);
}

int WaveCanvas::xAtTick(const Tick tick) const
{
    const TimelineViewport timeline(
        0,
        scenario_ ? scenario_->duration : 0,
        pixelsPerTick_,
        headerWidth_,
        horizontalScrollBar()->value());
    const auto x = timeline.pixelForTick(tick);
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
Segment* WaveCanvas::segmentById(
    const std::string& laneId,
    const std::string& segmentId)
{
    if (!scenario_) return nullptr;
    auto* lane = findLane(*scenario_, laneId);
    if (!lane) return nullptr;
    const auto segment = std::find_if(
        lane->segments.begin(),
        lane->segments.end(),
        [&segmentId](const Segment& candidate) {
            return candidate.id == segmentId;
        });
    return segment == lane->segments.end() ? nullptr : &*segment;
}

const Segment* WaveCanvas::segmentById(
    const std::string& laneId,
    const std::string& segmentId) const
{
    return const_cast<WaveCanvas*>(this)->segmentById(laneId, segmentId);
}

const Segment* WaveCanvas::segmentAtTick(const Lane& lane, const Tick tick) const
{
    const auto iterator = std::upper_bound(
        lane.segments.begin(),
        lane.segments.end(),
        tick,
        [](const Tick value, const Segment& candidate) {
            return value < candidate.start;
        });
    if (iterator == lane.segments.begin()) return nullptr;
    const auto& candidate = *std::prev(iterator);
    return candidate.start <= tick && tick < candidate.end ? &candidate : nullptr;
}

WaveCanvas::SegmentHit WaveCanvas::segmentHitAtPosition(
    const Lane& lane,
    const QPoint& position) const
{
    if (position.x() < headerWidth_ || lane.kind == LaneKind::Group) return {};
    constexpr int HandleRadius = 7;
    const auto tick = tickAtX(position.x());
    SegmentHit result{segmentAtTick(lane, tick), SegmentBoundary::None};
    auto bestDistance = HandleRadius + 1;

    const auto consider = [&](const Segment& segment) {
        const auto startDistance = std::abs(position.x() - xAtTick(segment.start));
        const auto endDistance = std::abs(position.x() - xAtTick(segment.end));
        if (startDistance < bestDistance) {
            result = {&segment, SegmentBoundary::Start};
            bestDistance = startDistance;
        }
        if (endDistance < bestDistance) {
            result = {&segment, SegmentBoundary::End};
            bestDistance = endDistance;
        }
    };

    if (lane.id == selectedSegmentLaneId_) {
        if (const auto* selected = segmentById(lane.id, selectedSegmentId_)) {
            consider(*selected);
            if (bestDistance <= HandleRadius) return result;
        }
    }

    const auto next = std::lower_bound(
        lane.segments.begin(),
        lane.segments.end(),
        tick,
        [](const Segment& candidate, const Tick value) {
            return candidate.start < value;
        });
    if (next != lane.segments.end()) consider(*next);
    if (next != lane.segments.begin()) consider(*std::prev(next));
    if (bestDistance > HandleRadius) result.boundary = SegmentBoundary::None;
    return result;
}

std::pair<Tick, Tick> WaveCanvas::beatGrid(const Lane& lane) const
{
    auto step = cursorKeyboardStep();
    Tick anchor = 0;
    if (project_ && !lane.clockDomainId.empty()) {
        if (const auto* clock = findClock(*project_, lane.clockDomainId);
            clock && clock->isValid()) {
            step = clock->period;
            anchor = tickAtCycle(*clock, 0, clock->activeEdge).value_or(clock->phase);
        }
    }
    return {std::max<Tick>(1, step), anchor};
}

Tick WaveCanvas::synchronousBoundaryTick(
    const Tick tick,
    const Lane& lane) const
{
    if (!scenario_) return std::max<Tick>(0, tick);
    const auto bounded = std::clamp<Tick>(tick, 0, scenario_->duration);
    const auto [step, anchor] = beatGrid(lane);
    const auto lower = anchor + floorToStep(bounded - anchor, step);
    auto best = bounded;
    auto bestDistance = std::numeric_limits<Tick>::max();
    const auto consider = [&](const Tick candidate) {
        if (candidate < 0 || candidate > scenario_->duration) return;
        const auto distance = candidate > bounded
            ? candidate - bounded
            : bounded - candidate;
        if (distance < bestDistance
            || (distance == bestDistance && candidate < best)) {
            best = candidate;
            bestDistance = distance;
        }
    };
    consider(0);
    consider(scenario_->duration);
    consider(lower);
    if (lower <= std::numeric_limits<Tick>::max() - step) consider(lower + step);
    return best;
}

Tick WaveCanvas::editTick(const Tick tick, const Lane& lane) const
{
    return asynchronousEditing_
        ? snappedTick(tick, &lane)
        : synchronousBoundaryTick(tick, lane);
}

Tick WaveCanvas::minimumWaveEditUnit(const Lane& lane) const
{
    return asynchronousEditing_ ? Tick{1} : beatGrid(lane).first;
}

std::pair<Tick, Tick> WaveCanvas::beatRangeAt(
    const Tick tick,
    const Lane& lane) const
{
    if (!scenario_ || scenario_->duration <= 0) return {0, 0};
    const auto bounded = std::clamp<Tick>(tick, 0, scenario_->duration - 1);
    const auto [step, anchor] = beatGrid(lane);
    const auto relative = bounded - anchor;
    const auto rawStart = anchor + floorToStep(relative, step);
    const auto rawEnd = rawStart > std::numeric_limits<Tick>::max() - step
        ? std::numeric_limits<Tick>::max()
        : rawStart + step;
    auto start = std::clamp<Tick>(rawStart, 0, scenario_->duration);
    auto end = std::clamp<Tick>(rawEnd, 0, scenario_->duration);
    if (end <= start) {
        start = floorToStep(bounded, step);
        end = std::min(scenario_->duration, start + step);
    }
    return {start, end};
}

std::pair<Tick, Tick> WaveCanvas::editableBeatRangeAt(
    const Tick tick,
    const Lane& lane) const
{
    if (!asynchronousEditing_) return beatRangeAt(tick, lane);
    if (!scenario_ || scenario_->duration <= 0) return {0, 0};
    const auto step = beatGrid(lane).first;
    auto start = snappedTick(
        std::clamp<Tick>(tick, 0, scenario_->duration - 1),
        &lane);
    start = std::min(start, scenario_->duration - 1);
    auto end = start > std::numeric_limits<Tick>::max() - step
        ? scenario_->duration
        : std::min(scenario_->duration, start + step);
    if (end <= start) {
        start = std::max<Tick>(0, scenario_->duration - std::min(step, scenario_->duration));
        end = scenario_->duration;
    }
    return {start, end};
}

std::vector<std::pair<Tick, Tick>> WaveCanvas::beatRangesBetween(
    const Tick first,
    const Tick second,
    const Lane& lane) const
{
    const auto firstBeat = beatRangeAt(first, lane);
    const auto secondBeat = beatRangeAt(second, lane);
    const auto start = std::min(firstBeat.first, secondBeat.first);
    const auto end = std::max(firstBeat.second, secondBeat.second);
    std::vector<std::pair<Tick, Tick>> beats;
    auto cursor = start;
    constexpr std::size_t MaximumBeatCount = 100'000;
    while (cursor < end && beats.size() < MaximumBeatCount) {
        auto beat = beatRangeAt(cursor, lane);
        beat.first = std::max(beat.first, start);
        beat.second = std::min(beat.second, end);
        if (beat.second <= beat.first || beat.second <= cursor) break;
        beats.push_back(beat);
        cursor = beat.second;
    }
    if (cursor < end) return {};
    return beats;
}

std::vector<std::pair<Tick, Tick>> WaveCanvas::editableBeatRangesBetween(
    const Tick first,
    const Tick second,
    const Lane& lane) const
{
    if (!asynchronousEditing_) return beatRangesBetween(first, second, lane);
    if (!scenario_ || scenario_->duration <= 0) return {};
    const auto unit = editableBeatRangeAt(first, lane);
    if (unit.second <= unit.first) return {};
    const auto step = beatGrid(lane).first;
    const auto probe = snappedTick(
        std::clamp<Tick>(second, 0, scenario_->duration - 1),
        &lane);
    auto start = unit.first;
    auto end = unit.second;
    if (probe >= unit.first) {
        const auto distance = probe - unit.first;
        const auto count = distance / step + 1;
        const auto available = scenario_->duration - unit.first;
        const auto span = count > available / step ? available : count * step;
        end = unit.first + span;
    } else {
        const auto distance = unit.first - probe;
        const auto count = distance / step + (distance % step == 0 ? 0 : 1);
        const auto offset = count > unit.first / step ? unit.first : count * step;
        start = unit.first - offset;
    }
    std::vector<std::pair<Tick, Tick>> beats;
    constexpr std::size_t MaximumBeatCount = 100'000;
    for (auto cursor = start;
         cursor < end && beats.size() < MaximumBeatCount;) {
        const auto next = cursor + std::min(step, end - cursor);
        if (next <= cursor) break;
        beats.emplace_back(cursor, next);
        cursor = next;
    }
    if (beats.empty() || beats.back().second < end) return {};
    return beats;
}

std::optional<std::pair<Tick, Tick>> WaveCanvas::adjacentEditableBeatRange(
    const Lane& lane,
    const std::pair<Tick, Tick>& currentRange,
    const bool forward) const
{
    if (!scenario_
        || scenario_->duration <= 0
        || currentRange.first < 0
        || currentRange.second <= currentRange.first
        || currentRange.second > scenario_->duration) {
        return std::nullopt;
    }

    std::pair<Tick, Tick> target;
    if (forward) {
        if (currentRange.second >= scenario_->duration) return std::nullopt;
        if (asynchronousEditing_) {
            target.first = currentRange.second;
            target.second = target.first
                + std::min(beatGrid(lane).first, scenario_->duration - target.first);
        } else {
            target = beatRangeAt(currentRange.second, lane);
        }
    } else {
        if (currentRange.first <= 0) return std::nullopt;
        if (asynchronousEditing_) {
            target.second = currentRange.first;
            target.first = target.second - std::min(beatGrid(lane).first, target.second);
        } else {
            target = beatRangeAt(currentRange.first - 1, lane);
        }
    }
    return target.second > target.first
        ? std::optional<std::pair<Tick, Tick>>{target}
        : std::nullopt;
}

bool WaveCanvas::navigateSelectedBeat(const bool forward)
{
    if (!scenario_ || scenario_->duration <= 0) {
        return false;
    }
    if (drawing_ || laneHeaderPressed_ || laneHeaderDragging_) {
        emit statusMessage(
            tr("Finish or cancel the current drag before navigating beats"));
        return true;
    }
    if (explicitRangeSelection_) {
        emit statusMessage(
            tr("Esc clears the selected range before navigating beats"));
        return true;
    }
    const auto* lane = findLane(*scenario_, selectedLaneId_);
    if (!lane || lane->kind == LaneKind::Group) {
        return false;
    }
    if (hasPendingBusValueEdit()) {
        emit statusMessage(
            tr("Finish or cancel the Bus/Enum draft before navigating beats"));
        if (busValueEdit_) {
            busValueEdit_->setFocus(Qt::OtherFocusReason);
            busValueEdit_->selectAll();
        }
        return true;
    }

    const auto current = editableBeatRangeAt(
        std::clamp<Tick>(cursorTick_, 0, scenario_->duration - 1),
        *lane);
    const auto target = adjacentEditableBeatRange(*lane, current, forward);
    if (!target) {
        emit statusMessage(
            tr("%1 · already at timeline %2 · %3")
                .arg(QString::fromStdString(lane->name))
                .arg(forward ? tr("End") : tr("start"))
                .arg(forward ? tr("Shift+Tab goes back") : tr("Tab advances")));
        return true;
    }

    hideBusPresetPalette();
    selectedLaneIds_ = {lane->id};
    laneHeaderSelectionActive_ = false;
    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    explicitRangeSelection_ = false;
    selectionRange_ = *target;
    cursorTick_ = target->first;
    waveEditHoverLaneId_ = lane->id;
    waveEditHoverRange_ = *target;
    snapGuideTick_.reset();
    ensureLaneVisible(lane->id);
    ensureCursorVisible(cursorTick_);
    emit selectionChanged(QString::fromStdString(lane->id), cursorTick_);
    emit statusMessage(
        tr("%1 · %2–%3 · value %4 · %5")
            .arg(QString::fromStdString(lane->name))
            .arg(QString::fromStdString(formatTick(
                target->first,
                project_->timeBase)))
            .arg(QString::fromStdString(formatTick(
                target->second,
                project_->timeBase)))
            .arg(laneValueAt(*lane, cursorTick_))
            .arg(forward
                     ? tr("Tab advances · Shift+Tab goes back")
                     : tr("Shift+Tab goes back · Tab advances")));
    viewport()->update();
    return true;
}

void WaveCanvas::navigateSelectedSegment(const bool forward)
{
    if (!scenario_ || tool_ != Tool::WaveEdit) return;
    if (drawing_ || laneHeaderPressed_ || laneHeaderDragging_) {
        emit statusMessage(
            tr("Finish or cancel the current drag before navigating Segments"));
        return;
    }
    if (explicitRangeSelection_) {
        emit statusMessage(
            tr("Esc clears the selected range before navigating Segments"));
        return;
    }
    auto* lane = findLane(*scenario_, selectedSegmentLaneId_);
    if (!lane || selectedSegmentId_.empty()) {
        lane = findLane(*scenario_, selectedLaneId_);
        if (!lane || lane->kind == LaneKind::Group
            || lane->kind == LaneKind::Bit) {
            emit statusMessage(
                tr("Select a Bus, Enum or Clock signal before navigating Segments"));
            return;
        }
        auto target = lane->segments.end();
        if (forward) {
            target = std::find_if(
                lane->segments.begin(),
                lane->segments.end(),
                [this](const Segment& segment) {
                    return segment.end > cursorTick_;
                });
        } else {
            const auto reverseTarget = std::find_if(
                lane->segments.rbegin(),
                lane->segments.rend(),
                [this](const Segment& segment) {
                    return segment.start < cursorTick_;
                });
            if (reverseTarget != lane->segments.rend()) {
                target = std::prev(reverseTarget.base());
            }
        }
        if (target == lane->segments.end()) {
            emit statusMessage(
                (forward
                     ? tr("No Segment at or after edit cursor %1 on %2")
                     : tr("No Segment before edit cursor %1 on %2"))
                    .arg(QString::fromStdString(formatTick(
                        cursorTick_,
                        project_->timeBase)))
                    .arg(QString::fromStdString(lane->name)));
            return;
        }
        selectedLaneId_ = lane->id;
        selectedLaneIds_ = {lane->id};
        laneHeaderSelectionActive_ = false;
        selectedSegmentLaneId_ = lane->id;
        selectedSegmentId_ = target->id;
        selectionRange_ = std::pair{target->start, target->end};
        waveEditOriginalRange_.reset();
        waveEditPreviewRange_.reset();
        waveEditHoverLaneId_.clear();
        waveEditHoverRange_.reset();
        cursorTick_ = target->start;
        hideBusPresetPalette();
        ensureLaneVisible(lane->id);
        ensureCursorVisible(cursorTick_);
        emit selectionChanged(
            QString::fromStdString(lane->id),
            cursorTick_);
        emit statusMessage(
            (forward
                 ? tr("Next Segment from edit cursor · %1 · %2 · %3–%4")
                 : tr("Previous Segment from edit cursor · %1 · %2 · %3–%4"))
                .arg(QString::fromStdString(lane->name))
                .arg(QString::fromStdString(target->value))
                .arg(QString::fromStdString(formatTick(
                    target->start,
                    project_->timeBase)))
                .arg(QString::fromStdString(formatTick(
                    target->end,
                    project_->timeBase))));
        viewport()->update();
        return;
    }
    const auto current = std::find_if(
        lane->segments.begin(),
        lane->segments.end(),
        [this](const Segment& segment) {
            return segment.id == selectedSegmentId_;
        });
    if (current == lane->segments.end()) {
        clearWaveEditState();
        emit statusMessage(tr("The selected Segment no longer exists"));
        viewport()->update();
        return;
    }
    const auto target = forward
        ? std::next(current)
        : current == lane->segments.begin()
            ? lane->segments.end()
            : std::prev(current);
    if (target == lane->segments.end()) {
        emit statusMessage(
            forward
                ? tr("No next Segment on %1")
                      .arg(QString::fromStdString(lane->name))
                : tr("No previous Segment on %1")
                      .arg(QString::fromStdString(lane->name)));
        return;
    }

    selectedLaneId_ = lane->id;
    selectedLaneIds_ = {lane->id};
    laneHeaderSelectionActive_ = false;
    selectedSegmentLaneId_ = lane->id;
    selectedSegmentId_ = target->id;
    selectionRange_ = std::pair{target->start, target->end};
    waveEditOriginalRange_.reset();
    waveEditPreviewRange_.reset();
    waveEditHoverLaneId_.clear();
    waveEditHoverRange_.reset();
    cursorTick_ = target->start;
    hideBusPresetPalette();
    ensureLaneVisible(lane->id);
    ensureCursorVisible(cursorTick_);
    emit selectionChanged(QString::fromStdString(lane->id), cursorTick_);
    emit statusMessage(
        (forward
             ? tr("Next Segment · %1 · %2 · %3–%4")
             : tr("Previous Segment · %1 · %2 · %3–%4"))
            .arg(QString::fromStdString(lane->name))
            .arg(QString::fromStdString(target->value))
            .arg(QString::fromStdString(formatTick(
                target->start,
                project_->timeBase)))
            .arg(QString::fromStdString(formatTick(
                target->end,
                project_->timeBase))));
    viewport()->update();
}

void WaveCanvas::selectSegmentAtCursor()
{
    if (!scenario_ || tool_ != Tool::WaveEdit) return;
    if (drawing_ || laneHeaderPressed_ || laneHeaderDragging_) {
        emit statusMessage(
            tr("Finish or cancel the current drag before selecting a Segment"));
        return;
    }
    if (explicitRangeSelection_) {
        emit statusMessage(
            tr("Esc clears the selected range before selecting a Segment"));
        return;
    }
    const auto* lane = findLane(*scenario_, selectedLaneId_);
    if (!lane || lane->kind == LaneKind::Group) {
        emit statusMessage(
            tr("Select a signal before using Select Segment at cursor"));
        return;
    }
    if (lane->kind == LaneKind::Bit) {
        emit statusMessage(
            tr("%1 uses beat editing · click or use the arrow keys to target one beat")
                .arg(QString::fromStdString(lane->name)));
        return;
    }
    const auto* segment = cursorTick_ < scenario_->duration
        ? segmentAtTick(*lane, cursorTick_)
        : nullptr;
    if (!segment) {
        const auto previous = std::find_if(
            lane->segments.rbegin(),
            lane->segments.rend(),
            [this](const Segment& candidate) {
                return candidate.end == cursorTick_;
            });
        if (previous != lane->segments.rend()) segment = &*previous;
    }
    const auto formatTime = [this](const Tick tick) {
        return project_
            ? QString::fromStdString(formatTick(tick, project_->timeBase))
            : QString::number(tick);
    };
    if (!segment) {
        emit statusMessage(
            tr("No explicit Segment on %1 at %2 · signal and edit cursor kept")
                .arg(QString::fromStdString(lane->name))
                .arg(formatTime(cursorTick_)));
        return;
    }

    hideBusPresetPalette();
    selectedLaneIds_ = {lane->id};
    laneHeaderSelectionActive_ = false;
    selectedSegmentLaneId_ = lane->id;
    selectedSegmentId_ = segment->id;
    selectionRange_ = std::pair{segment->start, segment->end};
    waveEditOriginalRange_.reset();
    waveEditPreviewRange_.reset();
    waveEditHoverLaneId_.clear();
    waveEditHoverRange_.reset();
    snapGuideTick_.reset();
    ensureLaneVisible(lane->id);
    ensureCursorVisible(cursorTick_);
    emit selectionChanged(QString::fromStdString(lane->id), cursorTick_);
    emit statusMessage(
        tr("Selected %1 Segment at %2 · value %3 · %4–%5")
            .arg(QString::fromStdString(lane->name))
            .arg(formatTime(cursorTick_))
            .arg(QString::fromStdString(segment->value))
            .arg(formatTime(segment->start))
            .arg(formatTime(segment->end)));
    viewport()->update();
}

void WaveCanvas::navigateTimelinePage(const bool forward)
{
    if (!scenario_ || scenario_->duration <= 0) {
        emit statusMessage(tr("Timeline has no navigable duration"));
        return;
    }
    const auto visibleStart = std::clamp<Tick>(
        tickAtX(headerWidth_),
        0,
        scenario_->duration);
    const auto visibleEnd = std::clamp<Tick>(
        tickAtX(viewport()->width()),
        visibleStart,
        scenario_->duration);
    const auto page = std::max<Tick>(
        cursorKeyboardStep(),
        std::max<Tick>(1, visibleEnd - visibleStart));
    const auto previous = cursorTick_;
    cursorTick_ = forward
        ? cursorTick_ + std::min(scenario_->duration - cursorTick_, page)
        : cursorTick_ - std::min(cursorTick_, page);
    if (cursorTick_ == previous) {
        emit statusMessage(
            forward
                ? tr("Timeline End reached · PageUp goes back")
                : tr("Timeline start reached · PageDown goes forward"));
        return;
    }

    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    waveEditHoverLaneId_.clear();
    waveEditHoverRange_.reset();
    if (const auto* lane = findLane(*scenario_, selectedLaneId_);
        lane && lane->kind != LaneKind::Group && cursorTick_ < scenario_->duration) {
        const auto target = editableBeatRangeAt(cursorTick_, *lane);
        selectionRange_ = target;
        waveEditHoverLaneId_ = lane->id;
        waveEditHoverRange_ = target;
    } else {
        selectionRange_.reset();
    }
    hideBusPresetPalette();
    snapGuideTick_.reset();
    ensureCursorVisible(cursorTick_);
    emit selectionChanged(
        QString::fromStdString(selectedLaneId_),
        cursorTick_);
    emit statusMessage(
        tr("%1 one visible page · edit cursor %2 · %3")
            .arg(forward ? tr("Forward") : tr("Back"))
            .arg(QString::fromStdString(formatTick(
                cursorTick_,
                project_->timeBase)))
            .arg(forward ? tr("PageUp goes back") : tr("PageDown goes forward")));
    viewport()->update();
}

void WaveCanvas::clearWaveEditState()
{
    stopWaveEditDragAutoScroll();
    waveEditDragAutoScrolled_ = false;
    waveEditCopyDrag_ = false;
    segmentActionPreview_.reset();
    explicitRangeSelection_ = false;
    hideRangeEditPalette();
    if (rangeValueEdit_) rangeValueEdit_->setModified(false);
    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    waveEditInteraction_ = WaveEditInteraction::None;
    waveEditOriginalRange_.reset();
    waveEditPreviewRange_.reset();
    waveEditRangeTargetLaneIds_.clear();
    waveEditRangeGrabLaneOffset_ = 0;
    waveEditRangeTargetValid_ = true;
    waveEditRangeTargetError_.clear();
    waveEditHoverLaneId_.clear();
    waveEditHoverRange_.reset();
    selectionRange_.reset();
    drawing_ = false;
    activeEventId_.clear();
}

Tick WaveCanvas::constrainedTransitionTick(
    const Event& event,
    const Tick requested) const
{
    if (!scenario_ || !event.waveformLinked) return event.tick;
    const auto* lane = findLane(*scenario_, event.laneId);
    if (!lane) return event.tick;
    const auto segment = std::find_if(
        lane->segments.begin(),
        lane->segments.end(),
        [&event](const Segment& candidate) {
            return candidate.id == event.linkedSegmentId;
        });
    if (segment == lane->segments.end()) return event.tick;
    const auto index = static_cast<std::size_t>(
        std::distance(lane->segments.begin(), segment));
    const auto unit = minimumWaveEditUnit(*lane);
    const auto lower = index > 0
        ? lane->segments.at(index - 1).start + unit
        : Tick{0};
    const auto upper = segment->end - unit;
    return upper >= lower ? std::clamp(requested, lower, upper) : event.tick;
}

const Marker* WaveCanvas::markerAtPosition(const QPoint& position) const
{
    if (!scenario_
        || position.x() < headerWidth_
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

QString WaveCanvas::markerLocationText(const Marker& marker) const
{
    const auto format = [this](const Tick tick) {
        return project_
            ? QString::fromStdString(formatTick(tick, project_->timeBase))
            : tr("%1 ticks").arg(tick);
    };
    if (marker.start == marker.end) return format(marker.start);
    return tr("%1–%2 (%3)")
        .arg(format(marker.start))
        .arg(format(marker.end))
        .arg(cursorDeltaText(marker.start, marker.end));
}

std::string WaveCanvas::nextLockedMarkerName(const bool interval) const
{
    const auto prefix = interval
        ? std::string{"Locked range "}
        : std::string{"Locked cursor "};
    if (!scenario_) return prefix + "1";
    for (auto suffix = scenario_->markers.size() + 1;; ++suffix) {
        const auto candidate = prefix + std::to_string(suffix);
        const auto exists = std::any_of(
            scenario_->markers.begin(),
            scenario_->markers.end(),
            [&candidate](const Marker& marker) {
                return QString::compare(
                           QString::fromStdString(marker.name),
                           QString::fromStdString(candidate),
                           Qt::CaseInsensitive)
                    == 0;
            });
        if (!exists) return candidate;
    }
}

Tick WaveCanvas::cursorKeyboardStep() const
{
    if (!project_) return 1;
    return std::max<Tick>(
        1,
        toTicks(10, TimeUnit::Nanosecond, project_->timeBase).value_or(1));
}

std::optional<std::pair<Tick, Tick>>
WaveCanvas::explicitRangeAnchorAndActive() const noexcept
{
    if (!explicitRangeSelection_ || !selectionRange_
        || selectionRange_->second <= selectionRange_->first) {
        return std::nullopt;
    }
    const auto [start, end] = *selectionRange_;
    if (cursorTick_ == end) return std::pair{start, end};
    if (cursorTick_ == start) return std::pair{end, start};

    const auto startDistance = cursorTick_ >= start
        ? cursorTick_ - start
        : start - cursorTick_;
    const auto endDistance = cursorTick_ >= end
        ? cursorTick_ - end
        : end - cursorTick_;
    return endDistance < startDistance
        ? std::pair{start, end}
        : std::pair{end, start};
}

std::optional<Tick> WaveCanvas::adjacentKeyboardTick(
    const Tick from,
    const bool forward,
    const Lane* lane) const
{
    if (!scenario_ || scenario_->duration <= 0) return std::nullopt;
    const auto bounded = std::clamp<Tick>(from, 0, scenario_->duration);
    if (forward && bounded >= scenario_->duration) return std::nullopt;
    if (!forward && bounded <= 0) return std::nullopt;

    if (asynchronousEditing_) {
        return forward ? bounded + 1 : bounded - 1;
    }

    if (!lane || lane->kind == LaneKind::Group) {
        const auto step = cursorKeyboardStep();
        return forward
            ? bounded + std::min(scenario_->duration - bounded, step)
            : bounded - std::min(bounded, step);
    }

    const auto [step, anchor] = beatGrid(*lane);
    const auto lower = anchor + floorToStep(bounded - anchor, step);
    if (forward) {
        auto candidate = lower;
        if (candidate <= bounded) {
            candidate = candidate > std::numeric_limits<Tick>::max() - step
                ? scenario_->duration
                : candidate + step;
        }
        candidate = std::clamp<Tick>(candidate, 0, scenario_->duration);
        if (candidate <= bounded) candidate = scenario_->duration;
        return candidate;
    }

    auto candidate = lower < bounded
        ? lower
        : lower < std::numeric_limits<Tick>::min() + step
            ? Tick{0}
            : lower - step;
    candidate = std::clamp<Tick>(candidate, 0, scenario_->duration);
    if (candidate >= bounded) candidate = 0;
    return candidate;
}

void WaveCanvas::adjustTimeRangeByKeyboard(
    const bool forward,
    const KeyboardRangeTarget target)
{
    if (!scenario_ || scenario_->duration <= 0) {
        emit statusMessage(tr("Timeline has no editable time range"));
        return;
    }
    const auto* lane = findLane(*scenario_, selectedLaneId_);
    if (!lane || !isLaneDisplayed(*lane) || lane->kind == LaneKind::Group) {
        emit statusMessage(
            tr("Select a signal before using Shift with Left, Right, Home, or End"));
        return;
    }
    if (rangeValueEdit_
        && rangeValueEdit_->isVisible()
        && rangeValueEdit_->isModified()
        && rangeValueEdit_->hasFocus()) {
        rangeValueEdit_->setFocus(Qt::OtherFocusReason);
        emit statusMessage(
            tr("Finish the selected range value or press Esc before adjusting its time"));
        return;
    }

    Tick anchor = cursorTick_;
    Tick active = cursorTick_;
    if (const auto endpoints = explicitRangeAnchorAndActive()) {
        anchor = endpoints->first;
        active = endpoints->second;
    } else if (selectedLaneIds_.empty()
               || std::find(
                      selectedLaneIds_.begin(),
                      selectedLaneIds_.end(),
                      selectedLaneId_)
                   == selectedLaneIds_.end()) {
        selectedLaneIds_ = {selectedLaneId_};
    }

    Tick next = active;
    if (target == KeyboardRangeTarget::TimelineBoundary) {
        next = forward ? scenario_->duration : Tick{0};
    } else if (target == KeyboardRangeTarget::SignalEdge) {
        const auto edge = adjacentEdgeTick(*lane, active, forward);
        if (!edge) {
            const auto activeText = project_
                ? QString::fromStdString(formatTick(active, project_->timeBase))
                : QString::number(active);
            emit statusMessage(
                forward
                    ? tr("No later edge on %1 from %2 · range unchanged · Shift+End selects to timeline end")
                          .arg(QString::fromStdString(lane->name))
                          .arg(activeText)
                    : tr("No earlier edge on %1 from %2 · range unchanged · Shift+Home selects to timeline start")
                          .arg(QString::fromStdString(lane->name))
                          .arg(activeText));
            return;
        }
        next = *edge;
    } else {
        next = adjacentKeyboardTick(active, forward, lane).value_or(active);
    }
    if (next == active) {
        const auto message = target == KeyboardRangeTarget::TimelineBoundary
            ? (forward
                   ? tr("Timeline end reached · range unchanged · Shift+Home moves the active edge back")
                   : tr("Timeline start reached · range unchanged · Shift+End moves the active edge forward"))
            : (forward
                   ? tr("Timeline end reached · range unchanged · Shift+Left moves back")
                   : tr("Timeline start reached · range unchanged · Shift+Right moves forward"));
        emit statusMessage(message);
        return;
    }

    cursorTick_ = next;
    ensureCursorVisible(cursorTick_);
    snapGuideTick_.reset();
    laneHeaderSelectionActive_ = false;
    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    waveEditHoverLaneId_.clear();
    waveEditHoverRange_.reset();
    hideBusPresetPalette();

    const auto format = [this](const Tick tick) {
        return project_
            ? QString::fromStdString(formatTick(tick, project_->timeBase))
            : QString::number(tick);
    };
    if (next == anchor) {
        clearExplicitRangeSelection(false);
        emit selectionChanged(
            QString::fromStdString(selectedLaneId_),
            cursorTick_);
        emit statusMessage(
            tr("Range collapsed at %1 · %2 signal(s) remain selected · "
               "Shift+Left/Right or Shift+Home/End starts a new range · Ctrl+Shift+Left/Right selects to edges")
                .arg(format(cursorTick_))
                .arg(static_cast<qulonglong>(selectedLaneIds_.size())));
        viewport()->update();
        return;
    }

    selectionRange_ = std::pair{
        std::min(anchor, next),
        std::max(anchor, next),
    };
    explicitRangeSelection_ = true;
    showRangeEditPalette();
    emit selectionChanged(
        QString::fromStdString(selectedLaneId_),
        cursorTick_);
    if (target == KeyboardRangeTarget::SignalEdge) {
        emit statusMessage(
            tr("Keyboard edge range on %1 · %2 to %3 · %4 · %5 signal(s) · "
               "Ctrl+Shift+Left/Right adjusts to signal edges · Shift+Home/End selects to boundary · Esc clears")
                .arg(QString::fromStdString(lane->name))
                .arg(format(selectionRange_->first))
                .arg(format(selectionRange_->second))
                .arg(format(selectionRange_->second - selectionRange_->first))
                .arg(static_cast<qulonglong>(selectedLaneIds_.size())));
    } else {
        emit statusMessage(
            tr("Keyboard range %1 to %2 · %3 · %4 signal(s) · %5 · "
               "Shift+Left/Right adjusts the active edge · Ctrl+Shift+Left/Right adjusts to signal edges · Shift+Home/End selects to boundary · Shift+Up/Down adjusts signals · Esc clears")
                .arg(format(selectionRange_->first))
                .arg(format(selectionRange_->second))
                .arg(format(selectionRange_->second - selectionRange_->first))
                .arg(static_cast<qulonglong>(selectedLaneIds_.size()))
                .arg(editTimingSummary()));
    }
    viewport()->update();
}

void WaveCanvas::adjustRangeSignalsByKeyboard(const bool downward)
{
    if (!scenario_ || !explicitRangeSelection_ || !selectionRange_
        || selectionRange_->second <= selectionRange_->first) {
        emit statusMessage(
            tr("Use Shift+Left or Shift+Right to select time before adding signals"));
        return;
    }
    if (hasPendingRangeValueEdit()) {
        if (rangeValueEdit_) rangeValueEdit_->setFocus(Qt::OtherFocusReason);
        emit statusMessage(
            tr("Finish the selected range value or press Esc before changing signals"));
        return;
    }

    std::vector<const Lane*> selectableLanes;
    selectableLanes.reserve(scenario_->lanes.size());
    for (const auto& lane : scenario_->lanes) {
        if (isLaneDisplayed(lane) && lane.kind != LaneKind::Group) {
            selectableLanes.push_back(&lane);
        }
    }
    const auto signalIndex = [&selectableLanes](const std::string& laneId)
        -> std::optional<std::size_t> {
        const auto found = std::find_if(
            selectableLanes.begin(),
            selectableLanes.end(),
            [&laneId](const Lane* lane) { return lane->id == laneId; });
        if (found == selectableLanes.end()) return std::nullopt;
        return static_cast<std::size_t>(found - selectableLanes.begin());
    };
    const auto activeIndex = signalIndex(selectedLaneId_);
    if (!activeIndex || selectedLaneIds_.empty()) {
        emit statusMessage(
            tr("The selected time range has no active signal · press Esc and select it again"));
        return;
    }

    std::vector<std::size_t> selectedIndices;
    selectedIndices.reserve(selectedLaneIds_.size());
    for (const auto& laneId : selectedLaneIds_) {
        const auto index = signalIndex(laneId);
        if (!index) {
            emit statusMessage(
                tr("Keyboard signal adjustment requires visible signal targets"));
            return;
        }
        selectedIndices.push_back(*index);
    }
    std::sort(selectedIndices.begin(), selectedIndices.end());
    selectedIndices.erase(
        std::unique(selectedIndices.begin(), selectedIndices.end()),
        selectedIndices.end());
    if (selectedIndices.size() != selectedLaneIds_.size()
        || selectedIndices.back() - selectedIndices.front() + 1
            != selectedIndices.size()) {
        emit statusMessage(
            tr("Keyboard signal adjustment requires one contiguous signal block"));
        return;
    }

    auto anchorIndex = *activeIndex;
    if (selectedIndices.size() > 1) {
        if (*activeIndex == selectedIndices.front()) {
            anchorIndex = selectedIndices.back();
        } else if (*activeIndex == selectedIndices.back()) {
            anchorIndex = selectedIndices.front();
        } else {
            emit statusMessage(
                tr("The active signal is inside the selected block · press Esc and select it again"));
            return;
        }
    }

    if ((!downward && *activeIndex == 0)
        || (downward && *activeIndex + 1 >= selectableLanes.size())) {
        const auto* active = selectableLanes.at(*activeIndex);
        emit statusMessage(
            downward
                ? tr("No signal below %1 · range unchanged · Shift+Up moves back")
                      .arg(QString::fromStdString(active->name))
                : tr("No signal above %1 · range unchanged · Shift+Down moves forward")
                      .arg(QString::fromStdString(active->name)));
        return;
    }

    const auto targetIndex = downward ? *activeIndex + 1 : *activeIndex - 1;
    const auto firstIndex = std::min(anchorIndex, targetIndex);
    const auto lastIndex = std::max(anchorIndex, targetIndex);
    selectedLaneIds_.clear();
    selectedLaneIds_.reserve(lastIndex - firstIndex + 1);
    for (auto index = firstIndex; index <= lastIndex; ++index) {
        selectedLaneIds_.push_back(selectableLanes.at(index)->id);
    }
    selectedLaneId_ = selectableLanes.at(targetIndex)->id;
    laneHeaderSelectionActive_ = false;
    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    waveEditHoverLaneId_.clear();
    waveEditHoverRange_.reset();
    hideBusPresetPalette();
    snapGuideTick_.reset();
    ensureLaneVisible(selectedLaneId_);
    showRangeEditPalette();
    emit selectionChanged(
        QString::fromStdString(selectedLaneId_),
        cursorTick_);

    const auto kind = explicitRangeKind();
    const auto type = kind
        ? *kind == LaneKind::Clock
            ? tr("Clock")
            : *kind == LaneKind::Bit
            ? tr("Bit")
            : *kind == LaneKind::Bus
                ? tr("Bus")
                : tr("Enum")
        : tr("mixed");
    emit statusMessage(
        tr("Keyboard signal range · %1 %2 signal(s) · active %3 · "
           "Shift+Up/Down adjusts signals · Shift+Left/Right adjusts time · Ctrl+Shift+Left/Right adjusts to signal edges · Shift+Home/End selects to boundary · Esc clears")
            .arg(static_cast<qulonglong>(selectedLaneIds_.size()))
            .arg(type)
            .arg(QString::fromStdString(selectableLanes.at(targetIndex)->name)));
    viewport()->update();
}

std::optional<Tick> WaveCanvas::adjacentEdgeTick(
    const Lane& lane,
    const Tick from,
    const bool forward) const
{
    if (!scenario_ || lane.kind == LaneKind::Group) return std::nullopt;
    std::optional<Tick> result;
    const auto consider = [&](const Tick candidate) {
        if (candidate < 0 || candidate > scenario_->duration) return;
        if (forward ? candidate <= from : candidate >= from) return;
        if (!result
            || (forward ? candidate < *result : candidate > *result)) {
            result = candidate;
        }
    };
    for (const auto& segment : lane.segments) {
        consider(segment.start);
        consider(segment.end);
    }

    if (lane.kind == LaneKind::Clock && project_) {
        const auto* clock = findClock(*project_, lane.clockDomainId);
        if (clock && clock->isValid()) {
            const auto rawCycle = std::floor(
                (static_cast<long double>(from)
                 - static_cast<long double>(clock->phase))
                / static_cast<long double>(clock->period));
            const auto minimumCycle = static_cast<long double>(
                std::numeric_limits<std::int64_t>::min() + 3);
            const auto maximumCycle = static_cast<long double>(
                std::numeric_limits<std::int64_t>::max() - 3);
            const auto baseCycle = static_cast<std::int64_t>(
                std::clamp(rawCycle, minimumCycle, maximumCycle));
            for (auto offset = std::int64_t{-2}; offset <= 2; ++offset) {
                const auto cycle = baseCycle + offset;
                for (const auto edge : {ClockEdge::Rising, ClockEdge::Falling}) {
                    if (const auto candidate = tickAtCycle(*clock, cycle, edge)) {
                        consider(*candidate);
                    }
                }
            }
        }
    }
    return result;
}

void WaveCanvas::selectAdjacentLane(const bool downward)
{
    if (!scenario_) return;
    const auto selectable = [this](const Lane& lane) {
        return isLaneDisplayed(lane) && lane.kind != LaneKind::Group;
    };
    const auto current = std::find_if(
        scenario_->lanes.begin(),
        scenario_->lanes.end(),
        [this](const Lane& lane) { return lane.id == selectedLaneId_; });
    auto target = scenario_->lanes.end();
    if (downward) {
        const auto first = current == scenario_->lanes.end()
            ? scenario_->lanes.begin()
            : std::next(current);
        target = std::find_if(first, scenario_->lanes.end(), selectable);
    } else {
        const auto first = current == scenario_->lanes.end()
            ? scenario_->lanes.rbegin()
            : std::make_reverse_iterator(current);
        const auto reverseTarget = std::find_if(
            first,
            scenario_->lanes.rend(),
            selectable);
        if (reverseTarget != scenario_->lanes.rend()) {
            target = std::prev(reverseTarget.base());
        }
    }

    if (target == scenario_->lanes.end()) {
        const auto* currentLane = current == scenario_->lanes.end()
            ? nullptr
            : &*current;
        const auto visibleSignalCount = std::count_if(
            scenario_->lanes.begin(),
            scenario_->lanes.end(),
            selectable);
        if (visibleSignalCount == 0) {
            emit statusMessage(tr("No visible signals to select"));
        } else if (!currentLane) {
            emit statusMessage(
                downward
                    ? tr("No signal selected · Down starts at the first visible signal")
                    : tr("No signal selected · Up starts at the last visible signal"));
        } else {
            emit statusMessage(
                downward
                    ? tr("No signal below %1 · Up selects the previous signal")
                          .arg(QString::fromStdString(currentLane->name))
                    : tr("No signal above %1 · Down selects the next signal")
                          .arg(QString::fromStdString(currentLane->name)));
        }
        return;
    }

    clearWaveEditState();
    hideBusPresetPalette();
    selectedLaneId_ = target->id;
    selectedLaneIds_ = {target->id};
    laneHeaderSelectionActive_ = false;
    snapGuideTick_.reset();
    ensureLaneVisible(target->id);
    const auto ordinal = std::count_if(
        scenario_->lanes.begin(),
        std::next(target),
        selectable);
    const auto total = std::count_if(
        scenario_->lanes.begin(),
        scenario_->lanes.end(),
        selectable);
    emit selectionChanged(QString::fromStdString(target->id), cursorTick_);
    auto message = tr(
        "Selected signal %1 · %2 of %3 · value %4 · Up/Down selects signals · Ctrl+Left/Right jumps edges")
                       .arg(QString::fromStdString(target->name))
                       .arg(ordinal)
                       .arg(total)
                       .arg(laneValueAt(*target, cursorTick_));
    message.append(tr(" · Shift+Left/Right selects time · Shift+Home/End selects to boundary · Ctrl+Shift+Left/Right selects to edges · Ctrl+A selects full timeline"));
    if (target->kind == LaneKind::Bus || target->kind == LaneKind::Enum) {
        message.append(tr(" · Enter edits value"));
    } else if (target->kind == LaneKind::Clock) {
        message.append(tr(" · G gates · X drives unknown · R runs"));
    }
    emit statusMessage(message);
    viewport()->setCursor(Qt::PointingHandCursor);
    viewport()->update();
}

void WaveCanvas::ensureLaneVisible(const std::string& laneId)
{
    if (!scenario_) return;
    const auto layout = std::find_if(
        laneLayout_.begin(),
        laneLayout_.end(),
        [this, &laneId](const LaneLayout& candidate) {
            return scenario_->lanes.at(candidate.laneIndex).id == laneId;
        });
    if (layout == laneLayout_.end()) return;
    const auto visibleHeight = std::max(0, viewport()->height() - RulerHeight);
    if (visibleHeight <= 0) return;
    const auto currentTop = verticalScrollBar()->value();
    const auto currentBottom = currentTop + visibleHeight;
    if (layout->top < currentTop) {
        verticalScrollBar()->setValue(layout->top);
    } else if (layout->top + layout->height > currentBottom) {
        verticalScrollBar()->setValue(
            layout->top + layout->height - visibleHeight);
    }
}

QString WaveCanvas::laneValueAt(const Lane& lane, const Tick tick) const
{
    if (!project_ || lane.kind == LaneKind::Group) return {};
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
    const auto undefinedValue = lane.kind == LaneKind::Bus
        || lane.kind == LaneKind::Enum
        ? QStringLiteral("X")
        : lane.kind == LaneKind::Bit
            ? QStringLiteral("0")
            : QStringLiteral("?");
    if (segment == lane.segments.begin()) return undefinedValue;
    const auto& candidate = *std::prev(segment);
    return candidate.start <= tick && tick < candidate.end
        ? QString::fromStdString(candidate.value)
        : undefinedValue;
}

QString WaveCanvas::cursorValue(const Lane& lane) const
{
    return movableCursorTick_
        ? laneValueAt(lane, *movableCursorTick_)
        : QString{};
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

QString WaveCanvas::cursorMeasurementText() const
{
    if (!movableCursorTick_) return tr("No active cursor");
    const auto format = [this](const Tick tick) {
        return project_
            ? QString::fromStdString(formatTick(tick, project_->timeBase))
            : tr("%1 ticks").arg(tick);
    };
    if (!temporaryCursorTick_) {
        return tr("Cursor %1 · values shown beside signals")
            .arg(format(*movableCursorTick_));
    }
    return tr("Reference %1 · Cursor %2 · %3")
        .arg(format(*temporaryCursorTick_))
        .arg(format(*movableCursorTick_))
        .arg(cursorDeltaText(*temporaryCursorTick_, *movableCursorTick_));
}

void WaveCanvas::ensureCursorVisible(const Tick tick)
{
    const auto x = xAtTick(tick);
    if (x >= headerWidth_ + 24 && x <= viewport()->width() - 24) return;
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
    const auto* marker = markerById(selectedMarkerId_);
    if (!marker) {
        selectedMarkerId_.clear();
        emit statusMessage(tr("The selected locked cursor no longer exists."));
        viewport()->update();
        return;
    }
    const auto markerName = QString::fromStdString(marker->name);
    const auto location = markerLocationText(*marker);
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
    emit statusMessage(
        tr("Deleted %1 · %2 · Ctrl+Z to undo")
            .arg(markerName)
            .arg(location));
}

void WaveCanvas::moveSelectedMarkerBy(const Tick delta)
{
    if (!scenario_ || !commandStack_ || selectedMarkerId_.empty()) return;
    const auto* marker = markerById(selectedMarkerId_);
    if (!marker) {
        selectedMarkerId_.clear();
        emit statusMessage(tr("The selected locked cursor no longer exists."));
        viewport()->update();
        return;
    }
    const auto markerName = QString::fromStdString(marker->name);
    const auto previousLocation = markerLocationText(*marker);
    const auto offset = std::clamp(
        delta,
        -marker->start,
        scenario_->duration - marker->end);
    if (offset == 0) {
        emit statusMessage(
            tr("%1 remains at %2 · timeline boundary reached · no position changed")
                .arg(markerName)
                .arg(previousLocation));
        return;
    }
    auto replacement = *marker;
    replacement.start += offset;
    replacement.end += offset;
    const auto replacementLocation = markerLocationText(replacement);
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
    emit statusMessage(
        tr("Moved %1 · %2 → %3 · Ctrl+Z to undo")
            .arg(markerName)
            .arg(previousLocation)
            .arg(replacementLocation));
}

Tick WaveCanvas::snappedTick(const Tick input, const Lane* lane) const
{
    if (!scenario_) return std::max<Tick>(0, input);
    const auto bounded = std::clamp(input, Tick{0}, scenario_->duration);
    if (bypassSnap_) return bounded;

    auto best = bounded;
    auto bestDistance = kSoftSnapRadiusPixels + 1;
    const auto rawX = xAtTick(bounded);
    const auto consider = [&](const Tick candidate, const bool preferOnTie) {
        if (candidate < 0 || candidate > scenario_->duration) return;
        const auto distance = static_cast<int>(std::min<long long>(
            std::abs(static_cast<long long>(xAtTick(candidate)) - rawX),
            std::numeric_limits<int>::max()));
        if (distance > kSoftSnapRadiusPixels
            || (distance > bestDistance)
            || (distance == bestDistance && !preferOnTie)) {
            return;
        }
        best = candidate;
        bestDistance = distance;
    };

    const auto minorTick = std::max<Tick>(1, majorTickStep() / 5);
    const auto lowerTick = floorToStep(bounded, minorTick);
    consider(lowerTick, false);
    if (lowerTick <= std::numeric_limits<Tick>::max() - minorTick) {
        consider(lowerTick + minorTick, false);
    }

    const auto edge = std::lower_bound(
        signalEdgeIndex_.begin(),
        signalEdgeIndex_.end(),
        bounded);
    if (edge != signalEdgeIndex_.end()) consider(*edge, true);
    if (edge != signalEdgeIndex_.begin()) consider(*std::prev(edge), true);

    if (project_) {
        const auto considerClock = [&](const ClockDomain& clock) {
            if (!clock.isValid()) return;
            const SnapContext context{1, 1, &clock, {}, {}};
            consider(snapTick(bounded, SnapMode::ClockRising, context), true);
            consider(snapTick(bounded, SnapMode::ClockFalling, context), true);
        };
        for (const auto& clock : project_->clockDomains) considerClock(clock);
        if (lane && !lane->clockDomainId.empty()) {
            if (const auto* clock = findClock(*project_, lane->clockDomainId)) {
                considerClock(*clock);
            }
        }
    }
    return best;
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
    const auto start = tickAtX(headerWidth_);
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


void WaveCanvas::commitWaveEdit(const QPoint& releasePosition)
{
    const auto rangeTransferPreflight =
        waveEditInteraction_ == WaveEditInteraction::MoveRange
        ? buildRangeTransferProjection()
        : std::nullopt;
    const auto segmentEditPreflight =
        waveEditInteraction_ == WaveEditInteraction::MoveSegment
            || waveEditInteraction_ == WaveEditInteraction::ResizeStart
            || waveEditInteraction_ == WaveEditInteraction::ResizeEnd
        ? buildSegmentEditProjection()
        : std::nullopt;
    drawing_ = false;
    if (!scenario_ || !commandStack_) {
        waveEditInteraction_ = WaveEditInteraction::None;
        return;
    }
    auto* lane = findLane(*scenario_, drawLaneId_);
    if (!lane) {
        clearWaveEditState();
        return;
    }

    const auto interaction = waveEditInteraction_;
    const auto copyDrag = waveEditCopyDrag_
        && (interaction == WaveEditInteraction::MoveSegment
            || interaction == WaveEditInteraction::MoveRange);
    waveEditInteraction_ = WaveEditInteraction::None;
    waveEditCopyDrag_ = false;
    const auto rawTick = scenario_->duration > 0
        ? std::clamp<Tick>(tickAtX(releasePosition.x()), 0, scenario_->duration - 1)
        : Tick{0};
    cursorTick_ = editTick(rawTick, *lane);

    if (interaction == WaveEditInteraction::MoveTransition) {
        if (const auto* event = findEvent(*scenario_, activeEventId_)) {
            drawCurrent_ = constrainedTransitionTick(*event, cursorTick_);
            cursorTick_ = drawCurrent_;
        }
        commitTransition(releasePosition);
        viewport()->setCursor(Qt::PointingHandCursor);
        return;
    }

    if (interaction == WaveEditInteraction::MoveRange) {
        if (!waveEditOriginalRange_
            || !waveEditPreviewRange_
            || selectedLaneIds_.empty()) {
            waveEditOriginalRange_.reset();
            waveEditPreviewRange_.reset();
            viewport()->update();
            return;
        }
        const auto source = *waveEditOriginalRange_;
        const auto target = *waveEditPreviewRange_;
        const auto targetLaneIds = waveEditRangeTargetLaneIds_.empty()
            ? selectedLaneIds_
            : waveEditRangeTargetLaneIds_;
        const auto targetSummary = rangeTransferTargetSummary();
        const auto targetValid = waveEditRangeTargetValid_
            && targetLaneIds.size() == selectedLaneIds_.size();
        const auto restoreDragView = [this] {
            if (waveEditDragAutoScrolled_) {
                horizontalScrollBar()->setValue(
                    waveEditDragOriginalHorizontalScroll_);
                verticalScrollBar()->setValue(
                    waveEditDragOriginalVerticalScroll_);
                waveEditDragAutoScrolled_ = false;
            }
        };
        const auto meaningfulDrag =
            waveEditDragAutoScrolled_
            || (releasePosition - waveEditPressPosition_).manhattanLength()
                >= QApplication::startDragDistance();
        if (meaningfulDrag && !targetValid) {
            restoreDragView();
            selectionRange_ = source;
            explicitRangeSelection_ = true;
            cursorTick_ = source.first;
            waveEditOriginalRange_.reset();
            waveEditPreviewRange_.reset();
            waveEditRangeTargetLaneIds_.clear();
            showRangeEditPalette();
            emit selectionChanged(
                QString::fromStdString(selectedLaneId_),
                cursorTick_);
            emit statusMessage(
                tr("Range drag cancelled · %1")
                    .arg(waveEditRangeTargetError_.isEmpty()
                             ? tr("target signals are unavailable")
                             : waveEditRangeTargetError_));
            viewport()->setCursor(Qt::SizeAllCursor);
            viewport()->update();
            return;
        }
        if (!meaningfulDrag
            || (target == source && targetLaneIds == selectedLaneIds_)) {
            restoreDragView();
            selectionRange_ = source;
            explicitRangeSelection_ = true;
            cursorTick_ = source.first;
            waveEditOriginalRange_.reset();
            waveEditPreviewRange_.reset();
            waveEditRangeTargetLaneIds_.clear();
            showRangeEditPalette();
            emit selectionChanged(
                QString::fromStdString(selectedLaneId_),
                cursorTick_);
            emit statusMessage(
                meaningfulDrag
                    ? tr("Selected range stayed at %1–%2 · no values changed")
                          .arg(QString::fromStdString(formatTick(
                              source.first,
                              project_->timeBase)))
                          .arg(QString::fromStdString(formatTick(
                              source.second,
                              project_->timeBase)))
                    : tr("Selected range kept · drag its body in time or onto compatible signals to move · "
                         "Ctrl+drag copies · press or release Ctrl while dragging to switch Move/Copy"));
            viewport()->setCursor(Qt::SizeAllCursor);
            viewport()->update();
            return;
        }
        const auto overlapsSource = rangeTransferTargetOverlapsSource();
        if (copyDrag && overlapsSource) {
            restoreDragView();
            selectionRange_ = source;
            explicitRangeSelection_ = true;
            cursorTick_ = source.first;
            waveEditOriginalRange_.reset();
            waveEditPreviewRange_.reset();
            waveEditRangeTargetLaneIds_.clear();
            showRangeEditPalette();
            emit selectionChanged(
                QString::fromStdString(selectedLaneId_),
                cursorTick_);
            emit statusMessage(
                tr("Copy cancelled · target overlaps source signal(s) · "
                   "move the preview outside %1–%2 or to other signals")
                    .arg(QString::fromStdString(formatTick(
                        source.first,
                        project_->timeBase)))
                    .arg(QString::fromStdString(formatTick(
                        source.second,
                        project_->timeBase))));
            viewport()->setCursor(Qt::SizeAllCursor);
            viewport()->update();
            return;
        }

        if (!rangeTransferPreflight
            || rangeTransferPreflight->copy != copyDrag
            || rangeTransferPreflight->targetLaneIds != targetLaneIds) {
            restoreDragView();
            selectionRange_ = source;
            explicitRangeSelection_ = true;
            cursorTick_ = source.first;
            waveEditOriginalRange_.reset();
            waveEditPreviewRange_.reset();
            waveEditRangeTargetLaneIds_.clear();
            showRangeEditPalette();
            emit statusMessage(
                tr("Range drag cancelled · selected signals are unavailable"));
            viewport()->update();
            return;
        }
        if (!rangeTransferPreflight->modelChanges) {
            restoreDragView();
            selectionRange_ = source;
            explicitRangeSelection_ = true;
            cursorTick_ = source.first;
            waveEditOriginalRange_.reset();
            waveEditPreviewRange_.reset();
            waveEditRangeTargetLaneIds_.clear();
            waveEditHoverLaneId_.clear();
            waveEditHoverRange_.reset();
            snapGuideTick_.reset();
            showRangeEditPalette();
            emit selectionChanged(
                QString::fromStdString(selectedLaneId_),
                cursorTick_);
            emit statusMessage(
                copyDrag
                    ? tr("Copy skipped · target already matches · source selection kept · no waveform, Relation, End, history, or clipboard change")
                    : tr("Move skipped · transfer would make no waveform, Relation, or End change · source selection kept · history and clipboard unchanged"));
            viewport()->setCursor(
                copyDrag ? Qt::DragCopyCursor : Qt::SizeAllCursor);
            viewport()->update();
            return;
        }
        auto copiedLanes = rangeTransferPreflight->copiedLanes;

        const auto durationBeforeTransfer = scenario_->duration;
        const auto relationCountBefore = scenario_->relations.size();
        const auto historyStateBefore = commandStack_->stateId();
        auto historySelectionBefore = historySelectionSnapshot();
        historySelectionBefore.selectionRange = source;
        historySelectionBefore.cursorTick = source.first;
        historySelectionBefore.explicitRangeSelection = true;
        const auto activeSource = std::find(
            selectedLaneIds_.begin(),
            selectedLaneIds_.end(),
            selectedLaneId_);
        const auto activeTargetIndex =
            activeSource == selectedLaneIds_.end()
            ? std::size_t{0}
            : static_cast<std::size_t>(
                  std::distance(selectedLaneIds_.begin(), activeSource));
        bool changed = false;
        try {
            changed = commandStack_->execute(
                std::make_unique<TransferRangeCommand>(
                    *scenario_,
                    copiedLanes,
                    source.first,
                    target.first,
                    source.second - source.first,
                    copyDrag
                        ? RangeTransferMode::Copy
                        : RangeTransferMode::Move));
        } catch (const std::exception& exception) {
            restoreDragView();
            selectionRange_ = source;
            explicitRangeSelection_ = true;
            cursorTick_ = source.first;
            waveEditOriginalRange_.reset();
            waveEditPreviewRange_.reset();
            waveEditRangeTargetLaneIds_.clear();
            showRangeEditPalette();
            emit statusMessage(
                tr("Range drag failed · %1")
                    .arg(QString::fromUtf8(exception.what())));
            viewport()->update();
            return;
        }
        if (!changed) {
            restoreDragView();
            selectionRange_ = source;
            explicitRangeSelection_ = true;
            cursorTick_ = source.first;
            waveEditOriginalRange_.reset();
            waveEditPreviewRange_.reset();
            waveEditRangeTargetLaneIds_.clear();
            waveEditHoverLaneId_.clear();
            waveEditHoverRange_.reset();
            snapGuideTick_.reset();
            showRangeEditPalette();
            emit selectionChanged(
                QString::fromStdString(selectedLaneId_),
                cursorTick_);
            emit statusMessage(
                tr("%1 skipped · projected change was no longer needed · source selection kept · history and clipboard unchanged")
                    .arg(copyDrag ? tr("Copy") : tr("Move")));
            viewport()->setCursor(
                copyDrag ? Qt::DragCopyCursor : Qt::SizeAllCursor);
            viewport()->update();
            return;
        }

        selectionRange_ = target;
        explicitRangeSelection_ = true;
        cursorTick_ = target.first;
        selectedLaneIds_ = targetLaneIds;
        selectedLaneId_ = selectedLaneIds_.at(std::min(
            activeTargetIndex,
            selectedLaneIds_.size() - 1));
        selectedSegmentLaneId_.clear();
        selectedSegmentId_.clear();
        waveEditOriginalRange_.reset();
        waveEditPreviewRange_.reset();
        waveEditRangeTargetLaneIds_.clear();
        waveEditHoverLaneId_.clear();
        waveEditHoverRange_.reset();
        snapGuideTick_.reset();
        hideBusPresetPalette();
        rememberHistorySelectionTransition(
            historyStateBefore,
            historySelectionBefore,
            commandStack_->stateId());
        emit modelEdited();
        emit commandAvailabilityChanged();
        refreshModel();
        emit selectionChanged(
            QString::fromStdString(selectedLaneId_),
            cursorTick_);

        if (selectionRange_) {
            const auto desiredScroll = static_cast<int>(std::clamp(
                std::ceil(
                    static_cast<double>(selectionRange_->second) * pixelsPerTick_
                    - static_cast<double>(std::max(
                        1,
                        waveViewportWidth() - 20))),
                0.0,
                static_cast<double>(horizontalScrollBar()->maximum())));
            if (desiredScroll > horizontalScrollBar()->value()) {
                horizontalScrollBar()->setValue(desiredScroll);
            }
        }

        const auto format = [this](const Tick tick) {
            return project_
                ? QString::fromStdString(formatTick(tick, project_->timeBase))
                : QString::number(tick);
        };
        auto message = copyDrag
            ? tr("Copied %1 signal(s) range to %2–%3 · source kept")
                  .arg(static_cast<qulonglong>(copiedLanes.size()))
                  .arg(format(target.first))
                  .arg(format(target.second))
            : tr("Moved %1 signal(s) range to %2–%3 · "
                 "source cleared where explicit")
                  .arg(static_cast<qulonglong>(copiedLanes.size()))
                  .arg(format(target.first))
                  .arg(format(target.second));
        if (!targetSummary.isEmpty()) {
            message += tr(" · %1").arg(targetSummary);
        }
        if (scenario_->duration > durationBeforeTransfer) {
            message += tr(" · End extended to %1")
                           .arg(format(scenario_->duration));
        }
        const auto removedRelationCount =
            relationCountBefore
            - std::min(
                relationCountBefore,
                scenario_->relations.size());
        if (removedRelationCount > 0
            && rangeTransferPreflight->relationImpact.ids.size()
                == removedRelationCount
            && !rangeTransferPreflight->relationImpact.summaries
                    .isEmpty()) {
            message += tr(" · removed: %1")
                           .arg(
                               rangeTransferPreflight->relationImpact
                                   .summaries.join(
                                       QStringLiteral("; ")));
        }
        emit statusMessage(appendRelationAwareUndo(
            message,
            relationCountBefore,
            scenario_->relations.size()));
        viewport()->setCursor(
            copyDrag ? Qt::DragCopyCursor : Qt::SizeAllCursor);
        viewport()->update();
        return;
    }

    if (interaction == WaveEditInteraction::ResizeRangeStart
        || interaction == WaveEditInteraction::ResizeRangeEnd) {
        if (waveEditPreviewRange_
            && waveEditPreviewRange_->second > waveEditPreviewRange_->first) {
            selectionRange_ = waveEditPreviewRange_;
            explicitRangeSelection_ = true;
            cursorTick_ = interaction == WaveEditInteraction::ResizeRangeStart
                ? selectionRange_->first
                : selectionRange_->second;
            showRangeEditPalette();
            emit selectionChanged(
                QString::fromStdString(selectedLaneId_),
                cursorTick_);
            emit statusMessage(
                tr("Adjusted range to %1–%2 · width %3 · %4 signals")
                    .arg(QString::fromStdString(
                        formatTick(selectionRange_->first, project_->timeBase)))
                    .arg(QString::fromStdString(
                        formatTick(selectionRange_->second, project_->timeBase)))
                    .arg(QString::fromStdString(formatTick(
                        selectionRange_->second - selectionRange_->first,
                        project_->timeBase)))
                    .arg(static_cast<qulonglong>(selectedLaneIds_.size())));
        }
        waveEditOriginalRange_.reset();
        waveEditPreviewRange_.reset();
        viewport()->setCursor(Qt::SplitHCursor);
        viewport()->update();
        return;
    }

    if (interaction == WaveEditInteraction::SelectRange) {
        drawCurrent_ = editTick(rawTick, *lane);
        selectionRange_ = std::pair{
            std::min(drawStart_, drawCurrent_),
            std::max(drawStart_, drawCurrent_),
        };
        if (const auto* releaseLane = laneAtY(releasePosition.y())) {
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
                    const auto& candidate =
                        scenario_->lanes.at(laneLayout_[index].laneIndex);
                    if (candidate.visible && candidate.kind != LaneKind::Group) {
                        selectedLaneIds_.push_back(candidate.id);
                    }
                }
                if (!selectedLaneIds_.empty()) {
                    selectedLaneId_ = first <= last
                        ? selectedLaneIds_.back()
                        : selectedLaneIds_.front();
                }
            }
        }
        selectedSegmentLaneId_.clear();
        selectedSegmentId_.clear();
        waveEditHoverLaneId_.clear();
        waveEditHoverRange_.reset();
        explicitRangeSelection_ = selectionRange_
            && selectionRange_->second > selectionRange_->first
            && !selectedLaneIds_.empty();
        hideBusPresetPalette();
        viewport()->setCursor(Qt::PointingHandCursor);
        if (!explicitRangeSelection_) {
            selectionRange_.reset();
            hideRangeEditPalette();
            emit statusMessage(tr("Drag across time to create a non-empty range"));
            viewport()->update();
            return;
        }

        cursorTick_ = selectionRange_->first;
        showRangeEditPalette();
        const auto duration = selectionRange_->second - selectionRange_->first;
        if (const auto rangeKind = explicitRangeKind()) {
            emit statusMessage(
                *rangeKind == LaneKind::Clock
                    ? tr("Selected %1–%2 · width %3 · %4 Clock signals · Ctrl+D repeats · use Run/Gate/Disable or R/G/X · Shift+Up/Down adjusts signals · Esc clears")
                          .arg(
                              QString::fromStdString(formatTick(
                                  selectionRange_->first,
                                  project_->timeBase)),
                              QString::fromStdString(formatTick(
                                  selectionRange_->second,
                                  project_->timeBase)),
                              QString::fromStdString(formatTick(
                                  duration,
                                  project_->timeBase)))
                          .arg(static_cast<qulonglong>(selectedLaneIds_.size()))
                : *rangeKind == LaneKind::Enum
                    ? tr("Selected %1–%2 · width %3 · %4 Enum signals · Ctrl+D repeats · type a shared symbol in the range toolbar · Shift+Up/Down adjusts signals · Esc clears")
                          .arg(
                              QString::fromStdString(formatTick(
                                  selectionRange_->first,
                                  project_->timeBase)),
                              QString::fromStdString(formatTick(
                                  selectionRange_->second,
                                  project_->timeBase)),
                              QString::fromStdString(formatTick(
                                  duration,
                                  project_->timeBase)))
                          .arg(static_cast<qulonglong>(selectedLaneIds_.size()))
                : *rangeKind == LaneKind::Bit
                    ? (selectedLaneIds_.size() > 1
                           ? tr("Selected %1–%2 · width %3 · %4 Bit signals · type one shared pattern or use / for one per signal · symbol*N creates a run · value buttons still apply one value to all · Ctrl+D repeats · Esc clears")
                           : tr("Selected %1–%2 · width %3 · %4 Bit signal · type a 0/1/X/Z pattern (0*8 creates a run) or use the value buttons · Ctrl+D repeats · Esc clears"))
                          .arg(
                              QString::fromStdString(formatTick(
                                  selectionRange_->first,
                                  project_->timeBase)),
                              QString::fromStdString(formatTick(
                                  selectionRange_->second,
                                  project_->timeBase)),
                              QString::fromStdString(formatTick(
                                  duration,
                                  project_->timeBase)))
                          .arg(static_cast<qulonglong>(
                              selectedLaneIds_.size()))
                    : tr("Selected %1–%2 · width %3 · %4 signals · Ctrl+D repeats · use 0/1/X/Z or the range toolbar · Shift+Up/Down adjusts signals · Esc clears")
                          .arg(
                              QString::fromStdString(formatTick(
                                  selectionRange_->first,
                                  project_->timeBase)),
                              QString::fromStdString(formatTick(
                                  selectionRange_->second,
                                  project_->timeBase)),
                              QString::fromStdString(formatTick(
                                  duration,
                                  project_->timeBase)))
                          .arg(static_cast<qulonglong>(selectedLaneIds_.size())));
        } else {
            emit statusMessage(
                tr("Selected %1–%2 · width %3 · mixed/unsupported signal types · Ctrl+D repeats · Copy, Cut, or Delete to clear · Shift+Up/Down adjusts signals")
                    .arg(
                        QString::fromStdString(formatTick(
                            selectionRange_->first,
                            project_->timeBase)),
                        QString::fromStdString(formatTick(
                            selectionRange_->second,
                            project_->timeBase)),
                        QString::fromStdString(formatTick(
                            duration,
                            project_->timeBase))));
        }
        emit selectionChanged(QString::fromStdString(selectedLaneId_), cursorTick_);
        viewport()->update();
        return;
    }

    if (interaction == WaveEditInteraction::ToggleBitRange) {
        const auto beats = editableBeatRangesBetween(drawStart_, rawTick, *lane);
        waveEditOriginalRange_.reset();
        waveEditPreviewRange_.reset();
        if (beats.empty()) {
            QToolTip::showText(
                viewport()->mapToGlobal(releasePosition),
                tr("The selection contains too many beats to toggle at once"),
                viewport());
            viewport()->update();
            return;
        }
        commitBitToggle(beats, releasePosition);
        return;
    }

    if ((interaction != WaveEditInteraction::ResizeStart
         && interaction != WaveEditInteraction::ResizeEnd
         && interaction != WaveEditInteraction::MoveSegment)
        || !waveEditPreviewRange_) {
        waveEditOriginalRange_.reset();
        waveEditPreviewRange_.reset();
        viewport()->update();
        return;
    }

    const auto* segment = segmentById(selectedSegmentLaneId_, selectedSegmentId_);
    if (!segment) {
        clearWaveEditState();
        viewport()->update();
        return;
    }
    const auto [start, end] = *waveEditPreviewRange_;
    const auto segmentValue = segment->value;
    const auto sourceSegmentId = segment->id;
    const auto editedLaneId = lane->id;
    const auto editedLaneName = lane->name;
    const auto historyStateBefore = commandStack_->stateId();
    auto historySelectionBefore = historySelectionSnapshot();
    if (waveEditOriginalRange_) {
        historySelectionBefore.selectionRange = waveEditOriginalRange_;
        historySelectionBefore.cursorTick = waveEditOriginalRange_->first;
    }
    if (copyDrag
        && segmentEditPreflight
        && !segmentEditPreflight->modelChanges) {
        selectionRange_ = std::pair{
            segmentEditPreflight->originalStart,
            segmentEditPreflight->originalEnd,
        };
        cursorTick_ = segmentEditPreflight->originalStart;
        emit selectionChanged(
            QString::fromStdString(editedLaneId),
            cursorTick_);
        emit statusMessage(
            tr("%1 copy skipped · target already matches · source selection kept · history, Redo, and clipboard unchanged")
                .arg(QString::fromStdString(editedLaneName)));
        waveEditOriginalRange_.reset();
        waveEditPreviewRange_.reset();
        viewport()->setCursor(Qt::SizeAllCursor);
        viewport()->update();
        return;
    }
    if (copyDrag && (start != segment->start || end != segment->end)) {
        const auto relationCountBefore = scenario_->relations.size();
        bool changed = false;
        try {
            changed = commandStack_->execute(std::make_unique<CopySegmentCommand>(
                *scenario_,
                editedLaneId,
                sourceSegmentId,
                start,
                end));
        } catch (const std::exception& exception) {
            QToolTip::showText(
                viewport()->mapToGlobal(releasePosition),
                QString::fromUtf8(exception.what()),
                viewport());
            waveEditOriginalRange_.reset();
            waveEditPreviewRange_.reset();
            viewport()->update();
            return;
        }
        if (changed) {
            emit modelEdited();
            emit commandAvailabilityChanged();
            refreshModel();
            if (const auto* refreshedLane = findLane(*scenario_, editedLaneId)) {
                const auto midpoint = start + (end - start) / 2;
                if (const auto* refreshed =
                        segmentAtTick(*refreshedLane, midpoint)) {
                    selectedSegmentLaneId_ = refreshedLane->id;
                    selectedSegmentId_ = refreshed->id;
                    selectionRange_ =
                        std::pair{refreshed->start, refreshed->end};
                }
            }
            rememberHistorySelectionTransition(
                historyStateBefore,
                historySelectionBefore,
                commandStack_->stateId());
        } else if (waveEditOriginalRange_) {
            selectionRange_ = waveEditOriginalRange_;
            cursorTick_ = waveEditOriginalRange_->first;
        }
        auto message = changed
            ? tr("%1 segment copied to %2–%3 · source kept")
                  .arg(QString::fromStdString(editedLaneName))
                  .arg(QString::fromStdString(formatTick(start, project_->timeBase)))
                  .arg(QString::fromStdString(formatTick(end, project_->timeBase)))
            : tr("%1 copy target already matches · no values changed")
                  .arg(QString::fromStdString(editedLaneName));
        const auto removedRelationCount =
            relationCountBefore
            - std::min(
                relationCountBefore,
                scenario_->relations.size());
        if (changed
            && segmentEditPreflight
            && removedRelationCount > 0
            && segmentEditPreflight->relationImpact.ids.size()
                == removedRelationCount
            && !segmentEditPreflight->relationImpact.summaries
                    .isEmpty()) {
            message += tr(" · removed: %1").arg(
                segmentEditPreflight->relationImpact.summaries.join(
                    QStringLiteral("; ")));
        }
        emit statusMessage(
            changed
                ? appendRelationAwareUndo(
                      message,
                      relationCountBefore,
                      scenario_->relations.size())
                : message);
        waveEditOriginalRange_.reset();
        waveEditPreviewRange_.reset();
        viewport()->setCursor(Qt::SizeAllCursor);
        viewport()->update();
        return;
    }
    if (!copyDrag
        && (start != segment->start || end != segment->end)
        && segmentEditPreflight
        && !segmentEditPreflight->modelChanges) {
        selectionRange_ = std::pair{
            segmentEditPreflight->originalStart,
            segmentEditPreflight->originalEnd,
        };
        cursorTick_ = segmentEditPreflight->originalStart;
        emit selectionChanged(
            QString::fromStdString(editedLaneId),
            cursorTick_);
        emit statusMessage(
            tr("%1 edit skipped · waveform and Relation already match · source selection kept · history and Redo unchanged")
                .arg(QString::fromStdString(editedLaneName)));
        waveEditOriginalRange_.reset();
        waveEditPreviewRange_.reset();
        viewport()->setCursor(Qt::SizeAllCursor);
        viewport()->update();
        return;
    }
    if (start != segment->start || end != segment->end) {
        const auto relationCountBefore = scenario_->relations.size();
        bool changed = false;
        try {
            changed = commandStack_->execute(std::make_unique<EditSegmentCommand>(
                *scenario_,
                editedLaneId,
                segment->id,
                start,
                end,
                segmentValue));
        } catch (const std::exception& exception) {
            QToolTip::showText(
                viewport()->mapToGlobal(releasePosition),
                QString::fromUtf8(exception.what()),
                viewport());
            waveEditOriginalRange_.reset();
            waveEditPreviewRange_.reset();
            viewport()->update();
            return;
        }
        if (!changed) {
            selectionRange_ = waveEditOriginalRange_;
            if (waveEditOriginalRange_) {
                cursorTick_ = waveEditOriginalRange_->first;
            }
            emit statusMessage(
                tr("%1 edit skipped · no model change · source selection kept · history and Redo unchanged")
                    .arg(QString::fromStdString(editedLaneName)));
            waveEditOriginalRange_.reset();
            waveEditPreviewRange_.reset();
            viewport()->setCursor(Qt::SizeAllCursor);
            viewport()->update();
            return;
        }
        emit modelEdited();
        emit commandAvailabilityChanged();
        refreshModel();
        if (const auto* refreshedLane = findLane(*scenario_, editedLaneId)) {
            const auto midpoint = start + (end - start) / 2;
            if (const auto* refreshed = segmentAtTick(*refreshedLane, midpoint)) {
                selectedSegmentLaneId_ = refreshedLane->id;
                selectedSegmentId_ = refreshed->id;
                selectionRange_ = std::pair{refreshed->start, refreshed->end};
            }
        }
        rememberHistorySelectionTransition(
            historyStateBefore,
            historySelectionBefore,
            commandStack_->stateId());
        auto message = tr("%1 segment %2 to %3–%4")
                .arg(QString::fromStdString(editedLaneName))
                .arg(interaction == WaveEditInteraction::MoveSegment ? tr("moved") : tr("resized"))
                .arg(QString::fromStdString(formatTick(start, project_->timeBase)))
                .arg(QString::fromStdString(formatTick(end, project_->timeBase)));
        const auto removedRelationCount =
            relationCountBefore
            - std::min(
                relationCountBefore,
                scenario_->relations.size());
        if (segmentEditPreflight
            && removedRelationCount > 0
            && segmentEditPreflight->relationImpact.ids.size()
                == removedRelationCount
            && !segmentEditPreflight->relationImpact.summaries
                    .isEmpty()) {
            message += tr(" · removed: %1").arg(
                segmentEditPreflight->relationImpact.summaries.join(
                    QStringLiteral("; ")));
        }
        emit statusMessage(appendRelationAwareUndo(
            message,
            relationCountBefore,
            scenario_->relations.size()));
    } else {
        const auto selectedBeatInsteadOfSegment =
            (interaction == WaveEditInteraction::MoveSegment
             || interaction == WaveEditInteraction::ResizeStart
             || interaction == WaveEditInteraction::ResizeEnd)
            && (lane->kind == LaneKind::Bus || lane->kind == LaneKind::Enum)
            && busEditPaletteVisible_
            && busPresetLaneId_ == editedLaneId
            && busEditScope_ == BusEditScope::Beat
            && busEditRange_;
        if (selectedBeatInsteadOfSegment) {
            selectedSegmentLaneId_.clear();
            selectedSegmentId_.clear();
            selectionRange_ = busEditRange_;
            cursorTick_ = busEditRange_->first;
            waveEditHoverLaneId_ = editedLaneId;
            waveEditHoverRange_ = busEditRange_;
            emit selectionChanged(
                QString::fromStdString(editedLaneId),
                cursorTick_);
            const auto probe =
                busEditRange_->first
                + (busEditRange_->second - busEditRange_->first) / 2;
            emit statusMessage(
                tr("Selected %1 Beat · value %2 · %3–%4 · width %5 · Delete clears this beat · drag moves the Segment")
                    .arg(QString::fromStdString(editedLaneName))
                    .arg(laneValueAt(*lane, probe))
                    .arg(QString::fromStdString(formatTick(
                        busEditRange_->first,
                        project_->timeBase)))
                    .arg(QString::fromStdString(formatTick(
                        busEditRange_->second,
                        project_->timeBase)))
                    .arg(QString::fromStdString(formatTick(
                        busEditRange_->second - busEditRange_->first,
                        project_->timeBase))));
            waveEditOriginalRange_.reset();
            waveEditPreviewRange_.reset();
            viewport()->setCursor(Qt::PointingHandCursor);
            viewport()->update();
            return;
        }
        const auto laneName = QString::fromStdString(editedLaneName);
        const auto value = QString::fromStdString(segmentValue);
        const auto startText = QString::fromStdString(
            formatTick(start, project_->timeBase));
        const auto endText = QString::fromStdString(
            formatTick(end, project_->timeBase));
        const auto widthText = QString::fromStdString(
            formatTick(end - start, project_->timeBase));
        emit statusMessage(
            tr("Selected %1 Segment · value %2 · %3–%4 · width %5")
                .arg(laneName, value, startText, endText, widthText));
    }
    waveEditOriginalRange_.reset();
    waveEditPreviewRange_.reset();
    viewport()->setCursor(
        interaction == WaveEditInteraction::MoveSegment
            ? Qt::SizeAllCursor
            : Qt::PointingHandCursor);
    viewport()->update();
}

void WaveCanvas::commitBitToggle(
    const std::vector<std::pair<Tick, Tick>>& beats,
    const QPoint& position)
{
    if (!scenario_ || !commandStack_ || beats.empty() || drawLaneId_.empty()) return;
    const auto* originalLane = findLane(*scenario_, drawLaneId_);
    if (!originalLane) return;
    const auto laneName = originalLane->name;
    const auto relationCountBefore = scenario_->relations.size();
    QString beforeValue = QStringLiteral("0");
    QString afterValue = QStringLiteral("1");
    if (beats.size() == 1) {
        const auto probe = beats.front().first
            + (beats.front().second - beats.front().first) / 2;
        if (const auto* segment = segmentAtTick(*originalLane, probe)) {
            beforeValue = QString::fromStdString(segment->value).toUpper();
        }
        afterValue = beforeValue == QStringLiteral("1")
            ? QStringLiteral("0")
            : QStringLiteral("1");
    }
    try {
        commandStack_->execute(std::make_unique<ToggleBitRangeCommand>(
            *scenario_,
            drawLaneId_,
            beats));
    } catch (const std::exception& exception) {
        QToolTip::showText(
            viewport()->mapToGlobal(position),
            QString::fromUtf8(exception.what()),
            viewport());
        viewport()->update();
        return;
    }
    emit modelEdited();
    emit commandAvailabilityChanged();
    refreshModel();
    selectionRange_ = std::pair{beats.front().first, beats.back().second};
    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    if (const auto* lane = findLane(*scenario_, drawLaneId_)) {
        const auto hoverTick = scenario_->duration > 0
            ? std::clamp<Tick>(tickAtX(position.x()), 0, scenario_->duration - 1)
            : Tick{0};
        waveEditHoverLaneId_ = lane->id;
        waveEditHoverRange_ = editableBeatRangeAt(hoverTick, *lane);
    }
    viewport()->update();
    if (beats.size() == 1) {
        emit statusMessage(appendRelationAwareUndo(
            tr("%1 · %2–%3: %4 → %5")
                .arg(QString::fromStdString(laneName))
                .arg(QString::fromStdString(formatTick(beats.front().first, project_->timeBase)))
                .arg(QString::fromStdString(formatTick(beats.front().second, project_->timeBase)))
                .arg(beforeValue, afterValue),
            relationCountBefore,
            scenario_->relations.size()));
    } else {
        emit statusMessage(appendRelationAwareUndo(
            tr("%1 · toggled %2 beats from %3 to %4")
                .arg(QString::fromStdString(laneName))
                .arg(beats.size())
                .arg(QString::fromStdString(formatTick(beats.front().first, project_->timeBase)))
                .arg(QString::fromStdString(formatTick(beats.back().second, project_->timeBase))),
            relationCountBefore,
            scenario_->relations.size()));
    }
}

void WaveCanvas::editSegmentAt(const QPoint& position)
{
    if (!scenario_ || !commandStack_ || position.x() < headerWidth_) return;
    auto* lane = laneAtY(position.y());
    if (!lane || lane->kind == LaneKind::Group) return;
    const auto* segment = segmentAtTick(*lane, tickAtX(position.x()));
    if (!segment) return;

    if (lane->kind == LaneKind::Bit) {
        const auto hoverTick = scenario_->duration > 0
            ? std::clamp<Tick>(tickAtX(position.x()), 0, scenario_->duration - 1)
            : Tick{0};
        selectedSegmentLaneId_.clear();
        selectedSegmentId_.clear();
        selectionRange_ = editableBeatRangeAt(hoverTick, *lane);
        waveEditHoverLaneId_ = lane->id;
        waveEditHoverRange_ = selectionRange_;
        QToolTip::showText(
            viewport()->mapToGlobal(position),
            tr("Click or drag to toggle beats; press 0, 1, X or Z to set an explicit value"),
            viewport());
        viewport()->update();
        return;
    }

    selectedSegmentLaneId_ = lane->id;
    selectedSegmentId_ = segment->id;
    selectionRange_ = std::pair{segment->start, segment->end};
    if (lane->kind == LaneKind::Bus || lane->kind == LaneKind::Enum) {
        const auto focusTick = std::clamp<Tick>(
            tickAtX(position.x()),
            segment->start,
            std::max(segment->start, segment->end - 1));
        cursorTick_ = focusTick;
        showBusPresetPalette(
            *lane,
            position,
            focusTick,
            std::pair{segment->start, segment->end});
        if (busValueEdit_) {
            busValueEdit_->setFocus(Qt::OtherFocusReason);
            busValueEdit_->selectAll();
            const auto laneId = lane->id;
            QTimer::singleShot(0, busValueEdit_, [this, laneId] {
                if (busPresetLaneId_ == laneId
                    && busEditScope_ == BusEditScope::Segment
                    && busValueEdit_) {
                    busValueEdit_->setFocus(Qt::OtherFocusReason);
                    busValueEdit_->selectAll();
                }
            });
        }
        emit selectionChanged(QString::fromStdString(lane->id), cursorTick_);
        emit statusMessage(
            tr("Edit %1 Segment · %2–%3 · Enter applies · Esc cancels")
                .arg(QString::fromStdString(lane->name))
                .arg(QString::fromStdString(formatTick(segment->start, project_->timeBase)))
                .arg(QString::fromStdString(formatTick(segment->end, project_->timeBase))));
        viewport()->update();
        return;
    }

    bool accepted = false;
    std::string replacementValue;
    bool clearClockOverride = false;
    if (lane->kind == LaneKind::Clock) {
        const auto choice = QInputDialog::getItem(
            this,
            tr("Edit clock segment"),
            tr("Mode for %1:").arg(QString::fromStdString(lane->name)),
            {
                tr("Gated ? hold low"),
                tr("Disabled ? drive X"),
                tr("Run ? clear override"),
            },
            segment->value == "disabled" ? 1 : 0,
            false,
            &accepted);
        if (accepted) {
            clearClockOverride = choice.startsWith(tr("Run"));
            replacementValue = choice.startsWith(tr("Disabled"))
                ? "disabled"
                : "gated";
        }
    } else {
        const auto entered = QInputDialog::getText(
            this,
            tr("Edit segment value"),
            tr("Value for %1:").arg(QString::fromStdString(lane->name)),
            QLineEdit::Normal,
            QString::fromStdString(segment->value),
            &accepted);
        replacementValue = entered.toStdString();
    }
    if (!accepted) return;

    const auto laneId = lane->id;
    const auto laneName = lane->name;
    const auto segmentId = segment->id;
    const auto start = segment->start;
    const auto end = segment->end;
    const auto formatTime = [this](const Tick tick) {
        return project_
            ? QString::fromStdString(formatTick(tick, project_->timeBase))
            : QString::number(tick);
    };
    if (!clearClockOverride) {
        const auto validation = validateLaneValue(*lane, replacementValue);
        if (!validation.valid) {
            const auto error = QString::fromStdString(validation.error);
            QToolTip::showText(
                viewport()->mapToGlobal(position),
                error,
                viewport());
            emit statusMessage(tr("No values changed · %1").arg(error));
            viewport()->update();
            return;
        }
        replacementValue = validation.normalizedValue;
        if (replacementValue == segment->value) {
            emit statusMessage(
                tr("%1 · %2–%3 already = %4 · no values changed")
                    .arg(QString::fromStdString(laneName))
                    .arg(formatTime(start))
                    .arg(formatTime(end))
                    .arg(QString::fromStdString(replacementValue)));
            return;
        }
    }

    const auto relationCountBefore = scenario_->relations.size();
    try {
        if (clearClockOverride) {
            commandStack_->execute(std::make_unique<ClearLaneRangeCommand>(
                *scenario_, laneId, start, end));
        } else {
            commandStack_->execute(std::make_unique<EditSegmentCommand>(
                *scenario_, laneId, segmentId, start, end, replacementValue));
        }
    } catch (const std::exception& exception) {
        const auto error = QString::fromUtf8(exception.what());
        QToolTip::showText(
            viewport()->mapToGlobal(position),
            error,
            viewport());
        emit statusMessage(tr("No values changed · %1").arg(error));
        viewport()->update();
        return;
    }
    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    selectionRange_.reset();
    emit modelEdited();
    emit commandAvailabilityChanged();
    refreshModel();
    auto displayValue = QString::fromStdString(replacementValue);
    if (const auto* refreshedLane = findLane(*scenario_, laneId)) {
        const auto probe = start + (end - start) / 2;
        if (const auto* refreshed = segmentAtTick(*refreshedLane, probe)) {
            selectedSegmentLaneId_ = refreshedLane->id;
            selectedSegmentId_ = refreshed->id;
            selectionRange_ = std::pair{refreshed->start, refreshed->end};
            displayValue = QString::fromStdString(refreshed->value);
        }
    }
    emit selectionChanged(QString::fromStdString(laneId), start);
    viewport()->update();
    const auto message = clearClockOverride
        ? tr("%1 · %2–%3 restored normal clock waveform")
              .arg(QString::fromStdString(laneName))
              .arg(formatTime(start))
              .arg(formatTime(end))
        : tr("%1 · %2–%3 = %4")
              .arg(QString::fromStdString(laneName))
              .arg(formatTime(start))
              .arg(formatTime(end))
              .arg(displayValue);
    emit statusMessage(appendRelationAwareUndo(
        message,
        relationCountBefore,
        scenario_->relations.size()));
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
        emit statusMessage(cursorMeasurementText());
        viewport()->update();
        return;
    }

    if (interaction == CursorInteraction::MoveLocked) {
        const auto* marker = markerById(selectedMarkerId_);
        if (!marker || !lockedMarkerOriginalRange_) {
            selectedMarkerId_.clear();
            lockedMarkerOriginalRange_.reset();
            emit statusMessage(tr("The selected locked cursor no longer exists."));
            viewport()->update();
            return;
        }
        const auto [originalStart, originalEnd] = *lockedMarkerOriginalRange_;
        auto original = *marker;
        original.start = originalStart;
        original.end = originalEnd;
        const auto markerName = QString::fromStdString(marker->name);
        const auto previousLocation = markerLocationText(original);
        const auto requested = drawCurrent_ - drawStart_;
        const auto offset = std::clamp(
            requested,
            -originalStart,
            scenario_->duration - originalEnd);
        if (offset != 0) {
            auto replacement = *marker;
            replacement.start = originalStart + offset;
            replacement.end = originalEnd + offset;
            const auto replacementLocation = markerLocationText(replacement);
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
                emit statusMessage(
                    tr("Locked cursor not moved · %1")
                        .arg(QString::fromUtf8(exception.what())));
                lockedMarkerOriginalRange_.reset();
                viewport()->update();
                return;
            }
            cursorTick_ = replacement.start;
            emit modelEdited();
            emit commandAvailabilityChanged();
            refreshModel();
            emit statusMessage(
                tr("Moved %1 · %2 → %3 · Ctrl+Z to undo")
                    .arg(markerName)
                    .arg(previousLocation)
                    .arg(replacementLocation));
        } else if (requested == 0) {
            emit statusMessage(
                tr("Selected %1 · %2 · drag or arrow keys to move · Delete to remove")
                    .arg(markerName)
                    .arg(previousLocation));
        } else {
            emit statusMessage(
                tr("%1 remains at %2 · timeline boundary reached · no position changed")
                    .arg(markerName)
                    .arg(previousLocation));
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
    marker.name = nextLockedMarkerName(start != end);
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
        emit statusMessage(
            tr("Locked cursor not created · %1")
                .arg(QString::fromUtf8(exception.what())));
        viewport()->update();
        return;
    }
    selectedMarkerId_ = marker.id;
    cursorTick_ = drawCurrent_;
    emit modelEdited();
    emit commandAvailabilityChanged();
    refreshModel();
    emit statusMessage(
        tr("Created %1 · %2 · Ctrl+Z to undo")
            .arg(QString::fromStdString(marker.name))
            .arg(markerLocationText(marker)));
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
    if (!scenario_ || !commandStack_ || activeEventId_.empty()) {
        activeEventId_.clear();
        return;
    }
    const auto* existing = findEvent(*scenario_, activeEventId_);
    if (!existing) {
        activeEventId_.clear();
        return;
    }
    const auto previousTick = existing->tick;
    const auto transitionLaneId = existing->laneId;
    auto replacement = *existing;
    replacement.tick = constrainedTransitionTick(*existing, drawCurrent_);
    cursorTick_ = replacement.tick;
    if (replacement.tick == existing->tick) {
        activeEventId_.clear();
        viewport()->setCursor(Qt::PointingHandCursor);
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
        activeEventId_.clear();
        viewport()->setCursor(Qt::PointingHandCursor);
        viewport()->update();
        return;
    }
    activeEventId_.clear();
    emit modelEdited();
    emit commandAvailabilityChanged();
    refreshModel();
    viewport()->setCursor(Qt::PointingHandCursor);
    const auto* transitionLane = findLane(*scenario_, transitionLaneId);
    emit statusMessage(
        tr("%1 edge moved from %2 to %3 · Ctrl+Z to undo")
            .arg(transitionLane ? QString::fromStdString(transitionLane->name) : tr("Signal"))
            .arg(QString::fromStdString(formatTick(previousTick, project_->timeBase)))
            .arg(QString::fromStdString(formatTick(cursorTick_, project_->timeBase))));
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
    painter.fillRect(QRect(headerWidth_, 0, waveViewportWidth(), RulerHeight), kRulerBackground);
    painter.setClipRect(QRect(headerWidth_, 0, waveViewportWidth(), viewport()->height()));
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
    painter.drawLine(headerWidth_, RulerHeight - 1, viewport()->width(), RulerHeight - 1);
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
    const QRect waveformRect(headerWidth_, y, waveViewportWidth(), layout.height);

    const auto selected = std::find(
        selectedLaneIds_.begin(),
        selectedLaneIds_.end(),
        lane.id) != selectedLaneIds_.end();
    const auto groupHeader = lane.kind == LaneKind::Group;
    const auto* parentGroup = visibleParentGroup(lane);
    const auto groupedMember = parentGroup != nullptr;
    painter.fillRect(
        QRect(0, y, headerWidth_, layout.height),
        selected
            ? QColor(63, 80, 104)
            : groupHeader
                ? QColor(38, 50, 67)
                : kHeaderBackground);
    if (groupHeader) {
        painter.fillRect(waveformRect, QColor(86, 111, 142, 24));
    }
    if (selected) painter.fillRect(waveformRect, QColor(105, 151, 205, 34));
    if (groupedMember) {
        painter.setPen(QPen(QColor(101, 127, 157, 145), 1.0));
        painter.drawLine(20, y, 20, y + layout.height);
        painter.drawLine(20, y + layout.height / 2, 28, y + layout.height / 2);
    }
    if (groupHeader) {
        const auto disclosure = groupDisclosureRect(lane);
        if (!disclosure.isEmpty()) {
            const auto center = disclosure.center();
            painter.setPen(Qt::NoPen);
            painter.setBrush(
                selected ? QColor(219, 232, 247) : QColor(176, 196, 218));
            if (collapsedGroupIds_.contains(lane.id)) {
                painter.drawPolygon(QPolygon{
                    QPoint(center.x() - 3, center.y() - 5),
                    QPoint(center.x() + 4, center.y()),
                    QPoint(center.x() - 3, center.y() + 5),
                });
            } else {
                painter.drawPolygon(QPolygon{
                    QPoint(center.x() - 5, center.y() - 3),
                    QPoint(center.x() + 5, center.y() - 3),
                    QPoint(center.x(), center.y() + 4),
                });
            }
        }
    }
    painter.setPen(kTextPrimary);
    QFont nameFont = painter.font();
    nameFont.setBold(selected || groupHeader);
    painter.setFont(nameFont);
    const auto sampledValue = tool_ == Tool::Marker
        ? cursorValue(lane)
        : tool_ == Tool::WaveEdit && selected && !drawing_
            ? laneValueAt(lane, cursorTick_)
            : QString{};
    const auto valueWidth = sampledValue.isEmpty() ? 0 : 78;
    const auto textLeft = groupHeader || groupedMember ? 34 : 14;
    const QRect nameRect(
        textLeft,
        y + 4,
        headerWidth_ - textLeft - 14 - valueWidth,
        layout.height / 2);
    painter.drawText(
        nameRect,
        Qt::AlignLeft | Qt::AlignVCenter,
        painter.fontMetrics().elidedText(
            QString::fromStdString(lane.name),
            Qt::ElideMiddle,
            nameRect.width()));
    if (!sampledValue.isEmpty()) {
        painter.setPen(kMovableCursor);
        nameFont.setBold(true);
        painter.setFont(nameFont);
        const QRect valueRect(
            headerWidth_ - 88,
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
    if (groupHeader) {
        const auto memberCount = visibleGroupMemberCount(lane.id);
        detail = tr("Group · %1 signal(s) · %2")
                     .arg(static_cast<qulonglong>(memberCount))
                     .arg(
                         collapsedGroupIds_.contains(lane.id)
                             ? tr("Collapsed")
                             : tr("Expanded"));
    } else if (lane.kind == LaneKind::Bus || lane.kind == LaneKind::Enum) {
        detail += tr(" · %1-bit").arg(lane.width);
    }
    painter.drawText(
        QRect(
            textLeft,
            y + layout.height / 2 - 2,
            headerWidth_ - textLeft - 14,
            layout.height / 2),
        Qt::AlignLeft | Qt::AlignVCenter,
        painter.fontMetrics().elidedText(
            detail,
            Qt::ElideRight,
            headerWidth_ - textLeft - 14));

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

    const auto empty = scenario_ && std::none_of(
        scenario_->lanes.begin(),
        scenario_->lanes.end(),
        [](const Lane& lane) { return lane.visible && lane.kind != LaneKind::Group; });
    const auto hiddenCount = hiddenLaneCount();
    if (empty) {
        const auto buttonsTop = addLaneButtons_.front()
            ? addLaneButtons_.front()->geometry().top()
            : RulerHeight + 150;
        QFont title = painter.font();
        title.setBold(true);
        title.setPointSizeF(title.pointSizeF() + 2.0);
        painter.setFont(title);
        painter.setPen(kTextPrimary);
        painter.drawText(
            QRect(headerWidth_ + 24, buttonsTop - 70, waveViewportWidth() - 48, 28),
            Qt::AlignCenter,
            hiddenCount == 0
                ? tr("Start with a clock or signal")
                : hiddenCount == 1
                    ? tr("1 item is hidden")
                    : tr("%1 items are hidden").arg(
                        static_cast<qulonglong>(hiddenCount)));
        QFont detail = painter.font();
        detail.setBold(false);
        detail.setPointSizeF(std::max(8.0, detail.pointSizeF() - 1.0));
        painter.setFont(detail);
        painter.setPen(kTextSecondary);
        painter.drawText(
            QRect(headerWidth_ + 24, buttonsTop - 40, waveViewportWidth() - 48, 24),
            Qt::AlignCenter,
            hiddenCount == 0
                ? tr("Add a lane, then click or drag its waveform to edit.")
                : tr("Restore hidden items below or add another lane."));
        return;
    }

    painter.fillRect(row, QColor(35, 43, 55));
    painter.fillRect(
        QRect(0, row.top(), headerWidth_, row.height()),
        kHeaderBackground);
    painter.setPen(QPen(kGridMinor, 1.0, Qt::DashLine));
    painter.drawLine(
        headerWidth_ + 14,
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
    const Tick visibleEnd,
    const bool preview)
{
    const auto* clock = project_ ? findClock(*project_, lane.clockDomainId) : nullptr;
    if (!clock || !clock->isValid()) {
        painter.setPen(kUndefined);
        painter.drawText(rect.adjusted(12, 0, -8, 0), Qt::AlignVCenter, tr("Unresolved clock domain"));
        return;
    }

    const auto highY = rect.top() + 12;
    const auto lowY = rect.bottom() - 12;
    QPen pen(
        laneColor(lane),
        2.0,
        preview ? Qt::DashLine : Qt::SolidLine);
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
                preview || !gated ? Qt::DashLine : Qt::SolidLine));
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
        painter.setPen(QPen(
            laneColor(lane),
            1.0,
            preview ? Qt::DashLine : Qt::SolidLine));
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
    const Tick visibleEnd,
    const bool preview)
{
    const auto highY = rect.top() + 12;
    const auto lowY = rect.bottom() - 12;
    auto previousY = lowY;
    auto havePrevious = false;
    const auto drawInterval = [&](const Tick start, const Tick end, const char rawValue) {
        if (end <= start) return;
        const auto left = xAtTick(start);
        const auto right = xAtTick(end);
        if (right <= left) return;
        const auto value = rawValue == 'x' ? 'X' : rawValue == 'z' ? 'Z' : rawValue;
        if (value == 'X' || value == 'Z') {
            painter.save();
            QColor fill = value == 'Z' ? QColor(171, 110, 191) : kUndefined;
            fill.setAlpha(35);
            painter.fillRect(
                QRect(left, highY, std::max(1, right - left), lowY - highY),
                fill);
            painter.setPen(QPen(
                value == 'Z' ? QColor(171, 110, 191) : kUndefined,
                1.25,
                Qt::DashLine));
            painter.drawLine(left, (highY + lowY) / 2, right, (highY + lowY) / 2);
            painter.drawText(
                QRect(left + 4, highY, std::max(0, right - left - 8), lowY - highY),
                Qt::AlignCenter,
                QString(QChar::fromLatin1(value)));
            painter.restore();
            havePrevious = false;
            return;
        }
        const auto y = value == '1' ? highY : lowY;
        painter.setPen(QPen(
            laneColor(lane),
            2.0,
            preview ? Qt::DashLine : Qt::SolidLine));
        if (havePrevious && previousY != y) painter.drawLine(left, previousY, left, y);
        painter.drawLine(left, y, right, y);
        previousY = y;
        havePrevious = true;
    };

    auto iterator = std::lower_bound(
        lane.segments.begin(),
        lane.segments.end(),
        visibleStart,
        [](const Segment& segment, const Tick tick) { return segment.end <= tick; });
    auto cursor = visibleStart;
    for (; iterator != lane.segments.end() && iterator->start < visibleEnd; ++iterator) {
        const auto segmentStart = std::max(iterator->start, visibleStart);
        const auto segmentEnd = std::min(iterator->end, visibleEnd);
        if (segmentStart > cursor) drawInterval(cursor, segmentStart, '0');
        if (segmentEnd > segmentStart) {
            drawInterval(
                segmentStart,
                segmentEnd,
                iterator->value.empty() ? 'X' : iterator->value.front());
            cursor = std::max(cursor, segmentEnd);
        }
    }
    if (cursor < visibleEnd) drawInterval(cursor, visibleEnd, '0');
}

void WaveCanvas::drawBusSegments(
    QPainter& painter,
    const Lane& lane,
    const QRect& rect,
    const Tick visibleStart,
    const Tick visibleEnd,
    const bool preview)
{
    const auto top = rect.top() + 10;
    const auto bottom = rect.bottom() - 10;
    const auto middle = (top + bottom) / 2;
    const auto drawUndefined = [this, &painter, top, bottom, middle](
                                   const Tick start,
                                   const Tick end,
                                   const QString& label,
                                   const bool highImpedance = false) {
        if (end <= start) return;
        const auto left = xAtTick(start);
        const auto right = xAtTick(end);
        if (right <= left) return;
        const auto color = highImpedance ? QColor(171, 110, 191) : kUndefined;
        auto fill = color;
        fill.setAlpha(24);
        painter.fillRect(QRect(left, top, std::max(1, right - left), bottom - top), fill);
        painter.setPen(QPen(color, 1.5, Qt::DashLine));
        painter.drawLine(left, middle, right, middle);
        painter.setPen(QPen(color, 1.0, Qt::DashLine));
        painter.drawLine(left, top, left, bottom);
        painter.drawLine(right, top, right, bottom);
        if (right - left > 24) {
            painter.setPen(color.lighter(135));
            painter.drawText(
                QRect(left + 4, top, right - left - 8, bottom - top),
                Qt::AlignCenter,
                label);
        }
    };
    const auto drawDontCare = [this, &painter, top, bottom](
                                  const Tick start,
                                  const Tick end) {
        if (end <= start) return;
        const auto left = xAtTick(start);
        const auto right = xAtTick(end);
        if (right <= left) return;
        const QColor color(148, 158, 171);
        auto fill = color;
        fill.setAlpha(42);
        painter.fillRect(
            QRect(left, top, std::max(1, right - left), bottom - top),
            QBrush(fill, Qt::BDiagPattern));
        painter.setPen(QPen(color, 1.25, Qt::DotLine));
        painter.drawRect(QRect(left, top, std::max(1, right - left), bottom - top));
        if (right - left > 44) {
            painter.setPen(color.lighter(145));
            painter.drawText(
                QRect(left + 4, top, right - left - 8, bottom - top),
                Qt::AlignCenter,
                tr("DON'T CARE"));
        }
    };

    const auto implicitUndefined = lane.kind == LaneKind::Bus;
    auto iterator = std::lower_bound(
        lane.segments.begin(),
        lane.segments.end(),
        visibleStart,
        [](const Segment& segment, const Tick tick) { return segment.end <= tick; });
    auto cursor = visibleStart;
    for (; iterator != lane.segments.end() && iterator->start < visibleEnd; ++iterator) {
        const auto segmentStart = std::max(iterator->start, visibleStart);
        const auto segmentEnd = std::min(iterator->end, visibleEnd);
        if (implicitUndefined && segmentStart > cursor) {
            drawUndefined(cursor, segmentStart, QStringLiteral("X"));
        }
        if (segmentEnd <= segmentStart) continue;

        const auto bits = implicitUndefined
            ? laneValueBits(lane, iterator->value)
            : std::optional<std::string>{};
        const auto allX = bits && !bits->empty()
            && std::all_of(bits->begin(), bits->end(), [](const char bit) { return bit == 'X'; });
        const auto allZ = bits && !bits->empty()
            && std::all_of(bits->begin(), bits->end(), [](const char bit) { return bit == 'Z'; });
        const auto preset = busPresetId(*iterator);
        const auto presetLabel = preset.empty()
            ? QString{}
            : busPresetDisplayLabel(preset);
        if (preset == "dont-care") {
            drawDontCare(segmentStart, segmentEnd);
            cursor = std::max(cursor, segmentEnd);
            continue;
        }
        if (allX || allZ) {
            drawUndefined(
                segmentStart,
                segmentEnd,
                presetLabel.isEmpty()
                    ? (allZ ? QStringLiteral("Z") : QStringLiteral("X"))
                    : presetLabel,
                allZ);
            cursor = std::max(cursor, segmentEnd);
            continue;
        }

        const auto left = xAtTick(segmentStart);
        const auto right = xAtTick(segmentEnd);
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
        painter.setPen(QPen(
            laneColor(lane),
            1.5,
            preview ? Qt::DashLine : Qt::SolidLine));
        painter.drawPath(path);
        if (right - left > 28) {
            painter.setPen(kTextPrimary);
            painter.drawText(
                QRect(left + bevel + 4, top, right - left - 2 * bevel - 8, bottom - top),
                Qt::AlignCenter,
                presetLabel.isEmpty()
                    ? QString::fromStdString(iterator->value)
                    : presetLabel);
        }
        cursor = std::max(cursor, segmentEnd);
    }
    if (implicitUndefined && cursor < visibleEnd) {
        drawUndefined(cursor, visibleEnd, QStringLiteral("X"));
    }
}

void WaveCanvas::drawLaneHeaderPastePreview(
    QPainter& painter,
    const LaneHeaderPastePreview* preview)
{
    if (!preview || preview->duration <= 0 || preview->lanes.empty()) return;

    const auto start = preview->start;
    const auto end = start + preview->duration;
    const auto left = xAtTick(start);
    const auto right = std::max(left + 1, xAtTick(end));
    const auto removesRelations = preview->relationRemovalCount > 0;
    const auto noChange = !preview->modelChanges;
    const auto repeat =
        preview->operation == RangePreviewOperation::Repeat;
    const QColor accent = removesRelations
        ? QColor(255, 183, 77)
        : noChange
            ? QColor(162, 176, 190)
            : QColor(105, 187, 255);
    const LaneLayout* firstVisibleLayout = nullptr;
    for (const auto& lane : preview->lanes) {
        const auto layout = std::find_if(
            laneLayout_.begin(),
            laneLayout_.end(),
            [this, &lane](const LaneLayout& candidate) {
                return scenario_->lanes.at(candidate.laneIndex).id == lane.id;
            });
        if (layout == laneLayout_.end()) continue;
        const auto y =
            RulerHeight + layout->top - verticalScrollBar()->value();
        if (y + layout->height < RulerHeight
            || y > viewport()->height()) {
            continue;
        }
        if (!firstVisibleLayout) firstVisibleLayout = &*layout;

        const QRect waveformRect(
            headerWidth_,
            y,
            waveViewportWidth(),
            layout->height);
        const QRect previewRect(
            QPoint(left, y + 3),
            QPoint(right, y + layout->height - 4));
        painter.save();
        painter.setClipRect(
            waveformRect.adjusted(0, 1, 0, -1),
            Qt::IntersectClip);
        auto shade = removesRelations
            ? QColor(68, 42, 16)
            : noChange
                ? QColor(29, 35, 43)
                : QColor(12, 24, 38);
        shade.setAlpha(112);
        painter.fillRect(previewRect, shade);
        painter.setClipRect(previewRect, Qt::IntersectClip);
        painter.setOpacity(0.96);
        if (lane.kind == LaneKind::Clock) {
            drawClock(painter, lane, waveformRect, start, end, true);
        } else if (lane.kind == LaneKind::Bit) {
            drawBitSegments(painter, lane, waveformRect, start, end, true);
        } else {
            drawBusSegments(painter, lane, waveformRect, start, end, true);
        }
        painter.restore();

        painter.save();
        painter.setClipRect(
            waveformRect.adjusted(0, 1, 0, -1),
            Qt::IntersectClip);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(accent, 1.5, Qt::DashLine));
        painter.drawRect(previewRect.adjusted(0, 0, -1, -1));
        painter.restore();

        const auto* currentLane = findLane(*scenario_, lane.id);
        if (currentLane) {
            const auto currentValue = laneValueAt(*currentLane, start);
            const auto previewValue = laneValueAt(lane, start);
            const auto valueLabel = currentValue == previewValue
                ? tr("=%1").arg(previewValue)
                : tr("%1→%2").arg(currentValue, previewValue);
            const QRect valueRect(
                headerWidth_ - 88,
                y + 4,
                74,
                layout->height / 2);
            painter.save();
            painter.fillRect(valueRect, QColor(63, 80, 104));
            auto font = painter.font();
            font.setBold(true);
            painter.setFont(font);
            painter.setPen(
                removesRelations
                    ? QColor(255, 210, 128)
                    : noChange
                        ? QColor(202, 211, 220)
                        : QColor(151, 214, 255));
            painter.drawText(
                valueRect,
                Qt::AlignRight | Qt::AlignVCenter,
                painter.fontMetrics().elidedText(
                    valueLabel,
                    Qt::ElideLeft,
                    valueRect.width()));
            painter.restore();
        }
    }

    if (!firstVisibleLayout) return;
    auto label = noChange
        ? repeat
            ? tr("Following range already matches")
            : tr("Already matches copied range")
        : removesRelations
            ? repeat
                ? tr("Repeat preview · removes %1 relation(s)")
                      .arg(static_cast<qulonglong>(
                          preview->relationRemovalCount))
                : tr("Paste preview · removes %1 relation(s)")
                      .arg(static_cast<qulonglong>(
                          preview->relationRemovalCount))
            : repeat ? tr("Repeat preview") : tr("Paste preview");
    if (removesRelations
        && !preview->relationRemovalEndpointSummaries.isEmpty()) {
        label += tr(" · %1")
                     .arg(
                         preview->relationRemovalEndpointSummaries.front());
        if (preview->relationRemovalEndpointSummaries.size() > 1) {
            label += tr(" · +%1 highlighted")
                         .arg(
                             preview->relationRemovalEndpointSummaries.size()
                             - 1);
        }
    }
    const auto startLabel = QString::fromStdString(
        formatTick(start, project_->timeBase));
    const auto endLabel = QString::fromStdString(
        formatTick(end, project_->timeBase));
    label += noChange
        ? repeat
            ? tr(" · %1 signal(s) · target %2–%3 · no Repeat needed")
                  .arg(static_cast<qulonglong>(preview->lanes.size()))
                  .arg(startLabel, endLabel)
            : tr(" · %1 signal(s) · copied width %2–%3 · no Paste needed")
                  .arg(static_cast<qulonglong>(preview->lanes.size()))
                  .arg(startLabel, endLabel)
        : repeat
            ? tr(" · %1 signal(s) · %2–%3 · Ctrl+D")
                  .arg(static_cast<qulonglong>(preview->lanes.size()))
                  .arg(startLabel, endLabel)
            : tr(" · %1 signal(s) · %2–%3 · Ctrl+V")
                  .arg(static_cast<qulonglong>(preview->lanes.size()))
                  .arg(startLabel, endLabel);
    const auto labelWidth = std::min(
        painter.fontMetrics().horizontalAdvance(label) + 16,
        std::max(0, viewport()->width() - headerWidth_ - 8));
    if (labelWidth <= 0) return;
    const auto minimumLeft = headerWidth_ + 4;
    const auto maximumLeft =
        std::max(minimumLeft, viewport()->width() - labelWidth - 4);
    const auto rightCandidate = right + 8;
    const auto leftCandidate = left - labelWidth - 8;
    const auto labelLeft =
        rightCandidate <= maximumLeft
        ? rightCandidate
        : leftCandidate >= minimumLeft
            ? leftCandidate
            : std::clamp(left + 8, minimumLeft, maximumLeft);
    const auto firstY = RulerHeight + firstVisibleLayout->top
        - verticalScrollBar()->value();
    const QRect labelRect(
        labelLeft,
        std::max(RulerHeight + 3, firstY + 3),
        labelWidth,
        22);
    painter.save();
    painter.setPen(QPen(accent, 1.0, Qt::DashLine));
    painter.setBrush(QColor(14, 27, 43, 235));
    painter.drawRoundedRect(labelRect, 4, 4);
    painter.setPen(QColor(225, 241, 255));
    painter.drawText(
        labelRect.adjusted(8, 0, -8, 0),
        Qt::AlignLeft | Qt::AlignVCenter,
        label);
    painter.restore();
}

void WaveCanvas::drawRangeTransferWaveformPreview(
    QPainter& painter,
    const RangeTransferProjection* transferProjection)
{
    if (!scenario_
        || !drawing_
        || waveEditInteraction_ != WaveEditInteraction::MoveRange
        || !waveEditOriginalRange_
        || !waveEditPreviewRange_
        || !waveEditRangeTargetValid_
        || (waveEditCopyDrag_ && rangeTransferTargetOverlapsSource())
        || selectedLaneIds_.empty()) {
        return;
    }

    const auto targetLaneIds = waveEditRangeTargetLaneIds_.empty()
        ? selectedLaneIds_
        : waveEditRangeTargetLaneIds_;
    if (targetLaneIds.size() != selectedLaneIds_.size()
        || (*waveEditPreviewRange_ == *waveEditOriginalRange_
            && targetLaneIds == selectedLaneIds_)) {
        return;
    }

    std::unordered_map<std::string, const Lane*> laneById;
    laneById.reserve(scenario_->lanes.size());
    for (const auto& lane : scenario_->lanes) {
        laneById.emplace(lane.id, &lane);
    }
    std::unordered_map<std::string, const LaneLayout*> layoutByLaneId;
    layoutByLaneId.reserve(laneLayout_.size());
    for (const auto& layout : laneLayout_) {
        layoutByLaneId.emplace(
            scenario_->lanes.at(layout.laneIndex).id,
            &layout);
    }

    const auto targetStart = waveEditPreviewRange_->first;
    const auto targetEnd = waveEditPreviewRange_->second;
    const auto left = xAtTick(targetStart);
    const auto right = std::max(left + 1, xAtTick(targetEnd));
    for (std::size_t index = 0; index < targetLaneIds.size(); ++index) {
        const auto sourceLane = laneById.find(selectedLaneIds_[index]);
        const auto targetLane = laneById.find(targetLaneIds[index]);
        const auto targetLayout = layoutByLaneId.find(targetLaneIds[index]);
        if (sourceLane == laneById.end()
            || targetLane == laneById.end()
            || targetLayout == layoutByLaneId.end()) {
            continue;
        }
        const auto* source = sourceLane->second;
        const auto* target = targetLane->second;
        const auto* layout = targetLayout->second;
        const auto y =
            RulerHeight + layout->top - verticalScrollBar()->value();
        if (y + layout->height < RulerHeight || y > viewport()->height()) {
            continue;
        }

        Lane previewLane;
        previewLane.id = target->id;
        previewLane.name = target->name;
        previewLane.kind = target->kind;
        previewLane.width = target->width;
        previewLane.isSigned = target->isSigned;
        previewLane.radix = target->radix;
        previewLane.enumMap = target->enumMap;
        previewLane.clockDomainId = target->clockDomainId;
        previewLane.color = target->color;
        previewLane.height = target->height;
        auto sourceSegment = std::lower_bound(
            source->segments.begin(),
            source->segments.end(),
            waveEditOriginalRange_->first,
            [](const Segment& segment, const Tick tick) {
                return segment.end <= tick;
            });
        for (; sourceSegment != source->segments.end()
               && sourceSegment->start < waveEditOriginalRange_->second;
             ++sourceSegment) {
            const auto clippedStart = std::max(
                waveEditOriginalRange_->first,
                sourceSegment->start);
            const auto clippedEnd = std::min(
                waveEditOriginalRange_->second,
                sourceSegment->end);
            if (clippedEnd <= clippedStart) continue;
            auto translated = *sourceSegment;
            translated.id.clear();
            translated.start =
                targetStart + clippedStart - waveEditOriginalRange_->first;
            translated.end =
                targetStart + clippedEnd - waveEditOriginalRange_->first;
            previewLane.segments.push_back(std::move(translated));
        }
        const auto previewColor =
            transferProjection
                && !transferProjection->relationImpact.ids.empty()
            ? QColor(255, 183, 77)
            : transferProjection
                  && !transferProjection->modelChanges
                ? QColor(164, 174, 188)
                : laneColor(*target).lighter(135);
        previewLane.color = previewColor.name(QColor::HexRgb).toStdString();

        const QRect waveformRect(
            headerWidth_,
            y,
            waveViewportWidth(),
            layout->height);
        const QRect previewClip(
            left + 1,
            y + 3,
            std::max(1, right - left - 2),
            std::max(1, layout->height - 6));
        painter.save();
        painter.setClipRect(
            waveformRect.adjusted(0, 1, 0, -1),
            Qt::IntersectClip);
        painter.setClipRect(previewClip, Qt::IntersectClip);
        painter.setOpacity(0.92);
        if (previewLane.kind == LaneKind::Clock) {
            const auto highY = waveformRect.top() + 12;
            const auto lowY = waveformRect.bottom() - 12;
            for (const auto& segment : previewLane.segments) {
                const auto segmentLeft = xAtTick(segment.start);
                const auto segmentRight = xAtTick(segment.end);
                if (segmentRight <= segmentLeft) continue;
                const auto mode = clockOverrideModeFromString(segment.value);
                if (!mode) continue;
                const auto gated = *mode == ClockOverrideMode::Gated;
                auto fill = gated ? QColor(255, 183, 77) : kUndefined;
                fill.setAlpha(42);
                painter.fillRect(
                    QRect(
                        segmentLeft,
                        waveformRect.top() + 2,
                        segmentRight - segmentLeft,
                        waveformRect.height() - 4),
                    fill);
                painter.setPen(QPen(
                    gated ? QColor(255, 202, 40) : QColor(255, 138, 128),
                    2.0,
                    Qt::DashLine));
                painter.drawLine(
                    segmentLeft,
                    gated ? lowY : (highY + lowY) / 2,
                    segmentRight,
                    gated ? lowY : (highY + lowY) / 2);
            }
        } else if (previewLane.kind == LaneKind::Bit) {
            drawBitSegments(
                painter,
                previewLane,
                waveformRect,
                targetStart,
                targetEnd,
                true);
        } else {
            drawBusSegments(
                painter,
                previewLane,
                waveformRect,
                targetStart,
                targetEnd,
                true);
        }
        painter.restore();
    }
}

void WaveCanvas::drawProjectedLaneWaveformPreview(
    QPainter& painter,
    const Lane& projectedLane,
    const Tick affectedStart,
    const Tick affectedEnd,
    const RelationRemovalImpact& relationImpact,
    const bool modelChanges)
{
    if (!scenario_ || affectedEnd <= affectedStart) return;
    const auto layout = std::find_if(
        laneLayout_.begin(),
        laneLayout_.end(),
        [this, &projectedLane](const LaneLayout& candidate) {
            return scenario_->lanes.at(candidate.laneIndex).id
                == projectedLane.id;
        });
    if (layout == laneLayout_.end()) return;

    const auto y =
        RulerHeight + layout->top - verticalScrollBar()->value();
    if (y + layout->height < RulerHeight || y > viewport()->height()) {
        return;
    }
    auto previewLane = projectedLane;
    const auto previewColor =
        !relationImpact.ids.empty()
        ? QColor(255, 183, 77)
        : !modelChanges
            ? QColor(164, 174, 188)
            : laneColor(previewLane).lighter(135);
    previewLane.color =
        previewColor.name(QColor::HexRgb).toStdString();

    const QRect waveformRect(
        headerWidth_,
        y,
        waveViewportWidth(),
        layout->height);
    const auto left = xAtTick(affectedStart);
    const auto right = std::max(left + 1, xAtTick(affectedEnd));
    const QRect previewClip(
        left + 1,
        y + 3,
        std::max(1, right - left - 2),
        std::max(1, layout->height - 6));
    painter.save();
    painter.setClipRect(
        waveformRect.adjusted(0, 1, 0, -1),
        Qt::IntersectClip);
    painter.setClipRect(previewClip, Qt::IntersectClip);
    auto wash = previewColor;
    wash.setAlpha(20);
    painter.fillRect(previewClip, wash);
    painter.setOpacity(0.94);
    if (previewLane.kind == LaneKind::Clock) {
        drawClock(
            painter,
            previewLane,
            waveformRect,
            affectedStart,
            affectedEnd,
            true);
    } else if (previewLane.kind == LaneKind::Bit) {
        drawBitSegments(
            painter,
            previewLane,
            waveformRect,
            affectedStart,
            affectedEnd,
            true);
    } else {
        drawBusSegments(
            painter,
            previewLane,
            waveformRect,
            affectedStart,
            affectedEnd,
            true);
    }
    painter.restore();
}

void WaveCanvas::drawSegmentEditWaveformPreview(
    QPainter& painter,
    const SegmentEditProjection* segmentProjection)
{
    if (!segmentProjection) return;
    const auto affectedStart =
        segmentProjection->operation == SegmentEditOperation::Copy
        ? segmentProjection->start
        : std::min(
              segmentProjection->originalStart,
              segmentProjection->start);
    const auto affectedEnd =
        segmentProjection->operation == SegmentEditOperation::Copy
        ? segmentProjection->end
        : std::max(
              segmentProjection->originalEnd,
              segmentProjection->end);
    drawProjectedLaneWaveformPreview(
        painter,
        segmentProjection->lane,
        affectedStart,
        affectedEnd,
        segmentProjection->relationImpact,
        segmentProjection->modelChanges);
}

void WaveCanvas::drawWaveEditOverlay(
    QPainter& painter,
    const RangeTransferProjection* transferProjection,
    const SegmentEditProjection* segmentProjection,
    const BusEditProjection* busEditProjection)
{
    if (tool_ != Tool::WaveEdit || !scenario_) return;

    const QColor accent(83, 164, 255);
    const auto layoutForLane = [this](const std::string& laneId) {
        return std::find_if(
            laneLayout_.begin(),
            laneLayout_.end(),
            [this, &laneId](const LaneLayout& layout) {
                return scenario_->lanes.at(layout.laneIndex).id == laneId;
            });
    };
    const auto drawRange = [&](const std::string& laneId,
                               const std::pair<Tick, Tick>& range,
                               const bool handles,
                               const bool preview,
                               const QColor& rangeColor) {
        const auto layout = layoutForLane(laneId);
        if (layout == laneLayout_.end() || range.second <= range.first) return;
        const auto y = RulerHeight + layout->top - verticalScrollBar()->value();
        if (y + layout->height < RulerHeight || y > viewport()->height()) return;
        const auto left = xAtTick(range.first);
        const auto right = xAtTick(range.second);
        const QRect segmentRect(
            QPoint(left, y + 2),
            QPoint(std::max(left + 1, right), y + layout->height - 3));
        auto fill = rangeColor;
        fill.setAlpha(preview ? 58 : 34);
        painter.fillRect(segmentRect, fill);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(
            rangeColor,
            preview ? 1.5 : 2.0,
            preview ? Qt::DashLine : Qt::SolidLine));
        painter.drawRect(segmentRect.adjusted(0, 0, -1, -1));
        if (!handles) return;
        const auto handleY = y + layout->height / 2 - 10;
        painter.setPen(QPen(QColor(222, 237, 255), 1.0));
        painter.setBrush(rangeColor);
        painter.drawRoundedRect(QRect(left - 4, handleY, 8, 20), 2, 2);
        painter.drawRoundedRect(QRect(right - 4, handleY, 8, 20), 2, 2);
    };

    painter.save();
    painter.setClipRect(QRect(
        headerWidth_,
        RulerHeight,
        waveViewportWidth(),
        viewport()->height() - RulerHeight));
    drawWaveEditTransitionPreview(painter);
    if (explicitRangeSelection_ && selectionRange_) {
        const auto resizing = drawing_
            && (waveEditInteraction_ == WaveEditInteraction::ResizeRangeStart
                || waveEditInteraction_ == WaveEditInteraction::ResizeRangeEnd);
        const auto transferring = drawing_
            && waveEditInteraction_ == WaveEditInteraction::MoveRange
            && waveEditOriginalRange_
            && waveEditPreviewRange_;
        const auto displayedSelection = transferring
            ? *waveEditOriginalRange_
            : *selectionRange_;
        for (std::size_t index = 0; index < selectedLaneIds_.size(); ++index) {
            const auto handles = index == 0 || index + 1 == selectedLaneIds_.size();
            drawRange(
                selectedLaneIds_[index],
                displayedSelection,
                handles,
                resizing,
                accent);
        }
        const auto previewLaneIds = waveEditRangeTargetLaneIds_.empty()
            ? selectedLaneIds_
            : waveEditRangeTargetLaneIds_;
        const auto copyOverlap = transferring
            && waveEditCopyDrag_
            && rangeTransferTargetOverlapsSource();
        const auto transferPreviewValid =
            waveEditRangeTargetValid_ && !copyOverlap;
        const auto transferRemovesRelations =
            transferPreviewValid
            && transferProjection
            && !transferProjection->relationImpact.ids.empty();
        const auto transferHasNoEffect =
            transferPreviewValid
            && transferProjection
            && !transferProjection->modelChanges;
        const auto transferPreviewColor =
            !transferPreviewValid
            ? QColor(239, 83, 80)
            : transferRemovesRelations
                ? QColor(255, 183, 77)
                : transferHasNoEffect
                    ? QColor(164, 174, 188)
                    : accent;
        if (transferring
            && (*waveEditPreviewRange_ != displayedSelection
                || previewLaneIds != selectedLaneIds_)) {
            for (const auto& laneId : previewLaneIds) {
                drawRange(
                    laneId,
                    *waveEditPreviewRange_,
                    false,
                    true,
                    transferPreviewColor);
            }
            if (transferPreviewValid) {
                drawRangeTransferWaveformPreview(
                    painter,
                    transferProjection);
            }
        }
        if ((resizing || transferring) && !selectedLaneIds_.empty()) {
            const auto& labelLaneIds =
                transferring && !previewLaneIds.empty()
                ? previewLaneIds
                : selectedLaneIds_;
            const auto layout = layoutForLane(labelLaneIds.front());
            if (layout != laneLayout_.end()) {
                const auto timingRange = transferring
                    ? *waveEditPreviewRange_
                    : *selectionRange_;
                auto timingLabel = tr("%1–%2 · width %3")
                    .arg(
                        QString::fromStdString(formatTick(
                            timingRange.first,
                            project_->timeBase)),
                        QString::fromStdString(formatTick(
                            timingRange.second,
                            project_->timeBase)),
                        QString::fromStdString(formatTick(
                            timingRange.second - timingRange.first,
                            project_->timeBase)));
                if (transferring) {
                    auto prefix = !transferPreviewValid
                        ? tr("Cannot drop")
                        : transferHasNoEffect
                            ? waveEditCopyDrag_
                                ? tr("Copy · no Copy needed")
                                : tr("Move · no Move needed")
                            : transferRemovesRelations
                                ? tr("%1 ⚠%2")
                                      .arg(
                                          waveEditCopyDrag_
                                              ? tr("Copy")
                                              : tr("Move"))
                                      .arg(static_cast<qulonglong>(
                                          transferProjection
                                              ->relationImpact.ids.size()))
                                : waveEditCopyDrag_
                                    ? tr("Copy")
                                    : tr("Move");
                    const auto mapping = rangeTransferTargetSummary();
                    if (!mapping.isEmpty()
                        && waveEditRangeTargetLaneIds_
                            != selectedLaneIds_) {
                        prefix += tr(" · %1").arg(mapping);
                    }
                    if (transferProjection
                        && transferProjection->extendsEnd) {
                        prefix += tr(" · End → %1")
                                      .arg(QString::fromStdString(
                                          formatTick(
                                              transferProjection->targetStart
                                                  + transferProjection
                                                        ->duration,
                                              project_->timeBase)));
                    }
                    if (transferRemovesRelations
                        && !transferProjection->relationImpact
                                .endpointSummaries.isEmpty()) {
                        prefix += tr(" · %1")
                                      .arg(
                                          transferProjection->relationImpact
                                              .endpointSummaries.front());
                    }
                    timingLabel.prepend(prefix + tr(" · "));
                }
                const auto labelWidth = std::min(
                    painter.fontMetrics().horizontalAdvance(timingLabel) + 16,
                    std::max(0, viewport()->width() - headerWidth_ - 8));
                const auto minimumLeft = headerWidth_ + 4;
                const auto maximumLeft = std::max(
                    minimumLeft,
                    viewport()->width() - labelWidth - 4);
                const auto rangeLeft = xAtTick(timingRange.first);
                const auto rangeRight = xAtTick(timingRange.second);
                const auto rightCandidate = rangeRight + 8;
                const auto leftCandidate = rangeLeft - labelWidth - 8;
                const auto labelLeft =
                    rightCandidate <= maximumLeft
                    ? rightCandidate
                    : leftCandidate >= minimumLeft
                        ? leftCandidate
                        : std::clamp(
                              rangeLeft + 8,
                              minimumLeft,
                              maximumLeft);
                const auto y = RulerHeight + layout->top
                    - verticalScrollBar()->value();
                const QRect labelRect(
                    labelLeft,
                    std::max(RulerHeight + 3, y + 3),
                    labelWidth,
                    22);
                painter.fillRect(
                    labelRect,
                    !transferPreviewValid
                        ? QColor(74, 24, 29, 232)
                        : transferRemovesRelations
                            ? QColor(66, 47, 18, 232)
                            : transferHasNoEffect
                                ? QColor(45, 51, 61, 232)
                                : QColor(16, 25, 39, 224));
                painter.setPen(QColor(225, 239, 255));
                painter.drawText(
                    labelRect.adjusted(8, 0, -8, 0),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    timingLabel);
            }
        }
    }
    if (rangeSequenceCaretTarget_
        && rangeValueEdit_
        && rangeEditPaletteVisible_) {
        for (std::size_t targetIndex = 0;
             targetIndex
                 < rangeSequenceCaretTarget_
                       ->targets.size();
             ++targetIndex) {
            const auto& target =
                rangeSequenceCaretTarget_
                    ->targets.at(targetIndex);
            drawRange(
                target.laneId,
                {
                    target.start,
                    target.end,
                },
                false,
                targetIndex != 0,
                targetIndex == 0
                    ? QColor(166, 246, 255)
                    : QColor(92, 176, 188));
        }
    }
    const auto persistentBitSelection = !drawing_ && hasBitRangeSelection();
    if (persistentBitSelection) {
        drawRange(selectedLaneId_, *selectionRange_, false, false, accent);
    }
    if (!drawing_
        && waveEditHoverRange_
        && !waveEditHoverLaneId_.empty()) {
        const auto duplicatesPersistentSelection = persistentBitSelection
            && waveEditHoverLaneId_ == selectedLaneId_
            && *waveEditHoverRange_ == *selectionRange_;
        const auto* selectedSegment = segmentById(
            selectedSegmentLaneId_,
            selectedSegmentId_);
        const auto duplicatesSelectedSegment = selectedSegment
            && waveEditHoverLaneId_ == selectedSegmentLaneId_
            && *waveEditHoverRange_
                == std::pair{selectedSegment->start, selectedSegment->end};
        const auto duplicatesActiveBusBeat = busEditPaletteVisible_
            && busEditScope_ == BusEditScope::Beat
            && busEditRange_
            && waveEditHoverLaneId_ == busPresetLaneId_
            && *waveEditHoverRange_ == *busEditRange_;
        if (!duplicatesPersistentSelection
            && !duplicatesSelectedSegment
            && !duplicatesActiveBusBeat) {
            drawRange(
                waveEditHoverLaneId_,
                *waveEditHoverRange_,
                false,
                false,
                accent);
        }
        const auto* hoverLane = findLane(*scenario_, waveEditHoverLaneId_);
        const auto layout = layoutForLane(waveEditHoverLaneId_);
        if (hoverLane && hoverLane->kind == LaneKind::Bit
            && layout != laneLayout_.end()) {
            const auto probe = waveEditHoverRange_->first
                + (waveEditHoverRange_->second - waveEditHoverRange_->first) / 2;
            QString current = QStringLiteral("0");
            if (const auto* segment = segmentAtTick(*hoverLane, probe)) {
                current = QString::fromStdString(segment->value).toUpper();
            }
            const auto target = current == QStringLiteral("0")
                ? QStringLiteral("→ 1")
                : current == QStringLiteral("1")
                    ? QStringLiteral("→ 0")
                    : tr("press 0/1");
            const auto left = xAtTick(waveEditHoverRange_->first);
            const auto right = xAtTick(waveEditHoverRange_->second);
            if (right - left > 24) {
                const auto y = RulerHeight + layout->top - verticalScrollBar()->value();
                painter.setPen(QColor(225, 239, 255));
                painter.drawText(
                    QRect(left + 3, y + 2, right - left - 6, layout->height - 4),
                    Qt::AlignCenter,
                    target);
            }
        }
    }
    const auto busPreviewing =
        busEditProjection
        && busEditProjection->laneId == busPresetLaneId_
        && busEditRange_
        && busEditProjection->start == busEditRange_->first
        && (busEditProjection->sequenceSteps.size() > 1
            || busEditProjection->end == busEditRange_->second);
    if (busPreviewing) {
        const auto removesRelations =
            !busEditProjection->relationImpact.ids.empty();
        const auto noEffect =
            !busEditProjection->modelChanges;
        const auto previewColor = removesRelations
            ? QColor(255, 183, 77)
            : noEffect
                ? QColor(164, 174, 188)
                : accent;
        const auto range = std::pair{
            busEditProjection->start,
            busEditProjection->end};
        drawRange(
            busEditProjection->laneId,
            range,
            busEditScope_ == BusEditScope::Segment,
            true,
            previewColor);
        const auto layout =
            layoutForLane(busEditProjection->laneId);
        if (layout != laneLayout_.end()) {
            const auto operation =
                busEditProjection->action
                    == BusEditAction::Clear
                ? tr("Clear")
                : busEditProjection->sequenceSteps.size() > 1
                    ? tr("Set %1 values")
                          .arg(static_cast<qulonglong>(
                              busEditProjection
                                  ->sequenceSteps.size()))
                    : tr("Set");
            auto prefix = noEffect
                ? tr("%1 · no change").arg(operation)
                : removesRelations
                    ? tr("%1 ⚠%2")
                          .arg(operation)
                          .arg(static_cast<qulonglong>(
                              busEditProjection
                                  ->relationImpact.ids.size()))
                    : operation;
            if (removesRelations
                && !busEditProjection->relationImpact
                        .endpointSummaries.isEmpty()) {
                prefix += tr(" · %1").arg(
                    busEditProjection->relationImpact
                        .endpointSummaries.front());
            }
            if (busEditProjection->extendsEnd) {
                prefix += tr(" · End → %1")
                              .arg(QString::fromStdString(
                                  formatTick(
                                      busEditProjection->end,
                                      project_->timeBase)));
            }
            const auto rangeStart = QString::fromStdString(
                formatTick(
                    range.first,
                    project_->timeBase));
            const auto rangeEnd = QString::fromStdString(
                formatTick(
                    range.second,
                    project_->timeBase));
            const auto label =
                busEditProjection->sequenceSteps.size() > 1
                ? tr("%1 · %2–%3")
                      .arg(prefix, rangeStart, rangeEnd)
                : tr("%1 · %2 · %3–%4")
                      .arg(
                          prefix,
                          busEditProjection->displayValue,
                          rangeStart,
                          rangeEnd);
            const auto y = RulerHeight + layout->top
                - verticalScrollBar()->value();
            const auto labelLeft = xAtTick(range.first) + 8;
            const auto labelWidth = std::max(
                180,
                painter.fontMetrics().horizontalAdvance(label) + 8);
            const auto visibleLabelWidth = std::max(
                0,
                std::min(
                    labelWidth,
                    viewport()->width() - labelLeft - 4));
            const QRect labelRect(
                labelLeft,
                y + 3,
                visibleLabelWidth,
                20);
            painter.fillRect(
                labelRect,
                removesRelations
                    ? QColor(66, 47, 18, 232)
                    : noEffect
                        ? QColor(45, 51, 61, 232)
                        : QColor(16, 25, 39, 224));
            painter.setPen(QColor(225, 239, 255));
            painter.drawText(
                labelRect.adjusted(4, 0, -4, 0),
                Qt::AlignLeft | Qt::AlignVCenter,
                label);
        }
    }
    const auto activeBusBeat = !busPreviewing
        && busEditPaletteVisible_
        && busPresetPalette_
        && busPresetPalette_->isVisible()
        && busEditScope_ == BusEditScope::Beat
        && busEditRange_
        && !busPresetLaneId_.empty();
    if (activeBusBeat) {
        const auto layout = layoutForLane(busPresetLaneId_);
        if (layout != laneLayout_.end()
            && busEditRange_->second > busEditRange_->first) {
            const auto y = RulerHeight + layout->top
                - verticalScrollBar()->value();
            const auto left = xAtTick(busEditRange_->first);
            const auto right = xAtTick(busEditRange_->second);
            const QRect beatRect(
                QPoint(left, y + 2),
                QPoint(std::max(left + 1, right), y + layout->height - 3));
            auto fill = QColor(255, 183, 77);
            fill.setAlpha(38);
            painter.fillRect(beatRect, fill);
            painter.setPen(QPen(QColor(255, 183, 77), 2.0, Qt::SolidLine));
            painter.drawRect(beatRect.adjusted(0, 0, -1, -1));
        }
    }
    if (waveEditInteraction_ == WaveEditInteraction::ToggleBitRange
        && waveEditPreviewRange_
        && !drawLaneId_.empty()) {
        drawRange(
            drawLaneId_,
            *waveEditPreviewRange_,
            false,
            true,
            accent);
    } else if (!activeBusBeat
               && !busPreviewing
               && !selectedSegmentId_.empty()) {
        const auto* segment = segmentById(
            selectedSegmentLaneId_,
            selectedSegmentId_);
        if (segment) {
            const auto previewing =
                segmentProjection
                && segmentProjection->laneId == selectedSegmentLaneId_
                && segmentProjection->sourceSegmentId
                    == selectedSegmentId_;
            const auto range = previewing
                ? std::pair{
                      segmentProjection->start,
                      segmentProjection->end}
                : std::pair{segment->start, segment->end};
            const auto segmentRemovesRelations =
                previewing
                && segmentProjection
                && !segmentProjection->relationImpact.ids.empty();
            const auto segmentHasNoEffect =
                previewing
                && segmentProjection
                && !segmentProjection->modelChanges;
            const auto segmentPreviewColor =
                segmentRemovesRelations
                ? QColor(255, 183, 77)
                : segmentHasNoEffect
                    ? QColor(164, 174, 188)
                    : accent;
            if (previewing) {
                drawSegmentEditWaveformPreview(
                    painter,
                    segmentProjection);
            }
            drawRange(
                selectedSegmentLaneId_,
                range,
                true,
                previewing,
                segmentPreviewColor);

            const auto layout = layoutForLane(selectedSegmentLaneId_);
            if (layout != laneLayout_.end()) {
                const auto y = RulerHeight + layout->top
                    - verticalScrollBar()->value();
                painter.setPen(QColor(222, 237, 255));
                const auto valueLabel = QString::fromStdString(segment->value);
                const auto timingLabel = tr("%1–%2 · width %3")
                    .arg(
                        QString::fromStdString(formatTick(
                            range.first,
                            project_->timeBase)),
                        QString::fromStdString(formatTick(
                            range.second,
                            project_->timeBase)),
                        QString::fromStdString(formatTick(
                            range.second - range.first,
                            project_->timeBase)));
                auto previewPrefix = QString{};
                if (previewing && segmentProjection) {
                    const auto operation =
                        segmentProjection->operation
                                == SegmentEditOperation::Copy
                        ? tr("Copy")
                        : segmentProjection->operation
                                == SegmentEditOperation::Move
                            ? tr("Move")
                            : segmentProjection->operation
                                    == SegmentEditOperation::ResizeStart
                                ? tr("Resize start")
                                : tr("Resize end");
                    previewPrefix =
                        segmentHasNoEffect
                        ? tr("%1 · no %1 needed").arg(operation)
                        : segmentRemovesRelations
                            ? tr("%1 ⚠%2")
                                  .arg(operation)
                                  .arg(static_cast<qulonglong>(
                                      segmentProjection
                                          ->relationImpact.ids.size()))
                            : operation;
                    if (segmentRemovesRelations
                        && !segmentProjection->relationImpact
                                .endpointSummaries.isEmpty()) {
                        previewPrefix += tr(" · %1").arg(
                            segmentProjection->relationImpact
                                .endpointSummaries.front());
                    }
                }
                const auto previewLabel = previewing
                    ? tr("%1 · %2 · %3")
                          .arg(previewPrefix, valueLabel, timingLabel)
                    : valueLabel;
                const auto labelLeft = xAtTick(range.first) + 8;
                const auto labelWidth = std::max(
                    180,
                    painter.fontMetrics().horizontalAdvance(previewLabel) + 8);
                painter.drawText(
                    QRect(
                        labelLeft,
                        y + 3,
                        std::max(0, std::min(
                            labelWidth,
                            viewport()->width() - labelLeft - 4)),
                        18),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    previewLabel);
            }
        }
    }
    painter.restore();
}

void WaveCanvas::drawEditGuide(QPainter& painter)
{
    if (tool_ != Tool::WaveEdit || !scenario_ || cursorTick_ < 0
        || cursorTick_ > scenario_->duration) {
        return;
    }
    const auto x = xAtTick(cursorTick_);
    if (x < headerWidth_ || x > viewport()->width()) return;
    const auto snapped = snapGuideTick_.has_value();
    const auto color = snapped ? QColor(255, 183, 77) : QColor(79, 195, 247, 145);
    painter.save();
    painter.setClipRect(QRect(
        headerWidth_,
        0,
        waveViewportWidth(),
        viewport()->height()));
    painter.setPen(QPen(color, snapped ? 1.75 : 1.0, Qt::DashLine));
    painter.drawLine(x, RulerHeight, x, viewport()->height());
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawPolygon(QPolygon{
        QPoint(x - 5, RulerHeight),
        QPoint(x + 5, RulerHeight),
        QPoint(x, RulerHeight + 7),
    });
    const auto label = (snapped ? tr("Snap  ") : QString{})
        + QString::fromStdString(formatTick(cursorTick_, project_->timeBase));
    const auto width = painter.fontMetrics().horizontalAdvance(label) + 12;
    const auto labelX = std::clamp(
        x + 6,
        headerWidth_ + 3,
        std::max(headerWidth_ + 3, viewport()->width() - width - 3));
    auto fill = color;
    fill.setAlpha(210);
    painter.fillRect(QRect(labelX, RulerHeight + 3, width, 19), fill);
    painter.setPen(QColor(20, 26, 34));
    painter.drawText(
        QRect(labelX + 6, RulerHeight + 3, width - 8, 19),
        Qt::AlignLeft | Qt::AlignVCenter,
        label);
    painter.restore();
}
void WaveCanvas::drawWaveEditTransitionPreview(QPainter& painter)
{
    if (!scenario_
        || !drawing_
        || waveEditInteraction_ != WaveEditInteraction::MoveTransition
        || activeEventId_.empty()) {
        return;
    }
    const auto* event = findEvent(*scenario_, activeEventId_);
    const auto* sourceLane = event ? findLane(*scenario_, event->laneId) : nullptr;
    if (!event || !sourceLane || sourceLane->kind != LaneKind::Bit) return;
    const auto previewRange = waveEditTransitionPreviewRange();
    if (!previewRange) return;

    auto previewLane = *sourceLane;
    const auto segment = std::find_if(
        previewLane.segments.begin(),
        previewLane.segments.end(),
        [event](const Segment& candidate) {
            return candidate.id == event->linkedSegmentId;
        });
    if (segment == previewLane.segments.end()) return;
    const auto segmentIndex = static_cast<std::size_t>(
        std::distance(previewLane.segments.begin(), segment));
    if (segmentIndex > 0
        && previewLane.segments.at(segmentIndex - 1).end == segment->start) {
        previewLane.segments.at(segmentIndex - 1).end = drawCurrent_;
    }
    segment->start = drawCurrent_;

    const auto layout = std::find_if(
        laneLayout_.begin(),
        laneLayout_.end(),
        [this, event](const LaneLayout& candidate) {
            return scenario_->lanes.at(candidate.laneIndex).id == event->laneId;
        });
    if (layout == laneLayout_.end()) return;
    const auto y = RulerHeight + layout->top - verticalScrollBar()->value();
    const QRect rect(headerWidth_, y, waveViewportWidth(), layout->height);
    if (rect.bottom() < RulerHeight || rect.top() > viewport()->height()) return;

    const auto highY = rect.top() + 12;
    const auto lowY = rect.bottom() - 12;
    const QColor accent(111, 168, 255);
    painter.save();
    const auto previewLeft = xAtTick(previewRange->first);
    const auto previewRight = xAtTick(previewRange->second);
    painter.setClipRect(
        rect.intersected(QRect(
            QPoint(previewLeft - 7, rect.top()),
            QPoint(std::max(previewLeft, previewRight) + 1, rect.bottom()))),
        Qt::IntersectClip);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(accent, 2.25, Qt::DashLine));
    int previousY = lowY;
    auto havePrevious = false;
    for (const auto& preview : previewLane.segments) {
        const auto left = xAtTick(preview.start);
        const auto right = xAtTick(preview.end);
        if (right < headerWidth_ || left > viewport()->width()) continue;
        const auto value = preview.value.empty() ? 'X' : preview.value.front();
        if (value == 'X' || value == 'Z') {
            painter.drawLine(left, (highY + lowY) / 2, right, (highY + lowY) / 2);
            havePrevious = false;
            continue;
        }
        const auto levelY = value == '1' ? highY : lowY;
        if (havePrevious && previousY != levelY) {
            painter.drawLine(left, previousY, left, levelY);
        }
        painter.drawLine(left, levelY, right, levelY);
        previousY = levelY;
        havePrevious = true;
    }
    const auto x = xAtTick(drawCurrent_);
    painter.setBrush(kBackground);
    painter.drawPolygon(QPolygon{
        QPoint(x, y + 6),
        QPoint(x + 6, y + 12),
        QPoint(x, y + 18),
        QPoint(x - 6, y + 12),
    });
    painter.drawLine(x, y + 19, x, y + layout->height - 4);
    painter.restore();
}

void WaveCanvas::drawScenarioOverlays(
    QPainter& painter,
    const Tick visibleStart,
    const Tick visibleEnd,
    const std::vector<std::string>* relationRemovalIds)
{
    if (!scenario_) return;
    eventHitRegions_.clear();
    painter.save();
    painter.setClipRect(QRect(headerWidth_, RulerHeight, waveViewportWidth(), viewport()->height() - RulerHeight));

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
        const auto willBeRemoved =
            relationRemovalIds
            && std::find(
                   relationRemovalIds->begin(),
                   relationRemovalIds->end(),
                   relation.id)
                != relationRemovalIds->end();
        if (!relationsVisible_ && !willBeRemoved) continue;
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
        const QColor color = willBeRemoved
            ? QColor(255, 183, 77)
            : relation.severity == Severity::Error
                ? QColor(239, 108, 115)
                : relation.severity == Severity::Warning
                    ? QColor(255, 183, 77)
                    : QColor(100, 181, 246);
        if (willBeRemoved) {
            painter.setPen(QPen(
                QColor(255, 183, 77, 72),
                6.0,
                Qt::SolidLine,
                Qt::RoundCap));
            painter.drawLine(sourcePoint, targetPoint);
        }
        painter.setPen(QPen(
            color,
            willBeRemoved ? 2.5 : 1.5,
            willBeRemoved ? Qt::DashLine : Qt::SolidLine,
            Qt::RoundCap));
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
        if (willBeRemoved) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(color, 2.0));
            painter.drawEllipse(sourcePoint, 9, 9);
            painter.drawEllipse(targetPoint, 9, 9);

            const QPoint midpoint(
                (sourcePoint.x() + targetPoint.x()) / 2,
                (sourcePoint.y() + targetPoint.y()) / 2);
            painter.setBrush(color);
            painter.setPen(QPen(kBackground, 1.5));
            painter.drawEllipse(midpoint, 8, 8);
            painter.setPen(QPen(QColor(61, 39, 10), 1.8));
            painter.drawLine(
                midpoint + QPoint(-3, -3),
                midpoint + QPoint(3, 3));
            painter.drawLine(
                midpoint + QPoint(-3, 3),
                midpoint + QPoint(3, -3));
        }
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
            headerWidth_ + 3,
            std::max(headerWidth_ + 3, viewport()->width() - width - 3));
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
