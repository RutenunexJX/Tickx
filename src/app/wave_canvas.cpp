#include "wave_canvas.h"

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
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QScrollBar>
#include <QStringListModel>
#include <QToolButton>
#include <QToolTip>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <tuple>

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
    if (presetId == "zero" || presetId == "reserved") return QStringLiteral("0");
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
    auto* quickHint = new QLabel(tr("Enter to apply · Esc to cancel"), quickLaneSetupPanel_);
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
    durationEdit_->setToolTip(tr("Edit the timeline end, for example 500 ns, then press Enter"));
    durationEdit_->setAlignment(Qt::AlignCenter);
    durationEdit_->setStyleSheet(QStringLiteral(
        "QLineEdit { color: #edf2f8; background: #2d3949;"
        " border: 1px solid #65758b; border-radius: 4px; padding: 2px 5px; }"
        "QLineEdit:focus { border-color: #8fc3ff; }"));
    durationEdit_->installEventFilter(this);
    connect(durationEdit_, &QLineEdit::editingFinished, this, [this] {
        const auto consumeCanvasInput = durationEditMouseFocusOut_;
        durationEditMouseFocusOut_ = false;
        if (!durationEdit_->isModified() || durationEditSubmitting_) return;
        submitDurationEdit(consumeCanvasInput);
        if (consumeCanvasInput) durationEditBlurConsumesCanvasInput_ = true;
    });
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget*, QWidget* newFocus) {
        if (!durationEditBlurConsumesCanvasInput_) return;
        if (newFocus != this && newFocus != viewport()) {
            durationEditBlurConsumesCanvasInput_ = false;
        }
    });

    busPresetPalette_ = new QFrame(this);
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
        "QFrame#BusPresetPalette QToolButton {"
        " color: #f3f6fa; background: #46566c;"
        " border: 1px solid #6f8099; border-radius: 4px; padding: 3px 7px;"
        "}"
        "QFrame#BusPresetPalette QToolButton:hover {"
        " background: #56708f; border-color: #8dc0ff;"
        "}"));
    auto* presetLayout = new QHBoxLayout(busPresetPalette_);
    presetLayout->setContentsMargins(7, 5, 7, 5);
    presetLayout->setSpacing(5);
    busPresetContextLabel_ = new QLabel(tr("Bus · Beat"), busPresetPalette_);
    busPresetContextLabel_->setObjectName(QStringLiteral("BusPresetContextLabel"));
    busPresetContextLabel_->setMaximumWidth(270);
    presetLayout->addWidget(busPresetContextLabel_);
    busRadixCombo_ = new QComboBox(busPresetPalette_);
    busRadixCombo_->setObjectName(QStringLiteral("BusEditRadixCombo"));
    busRadixCombo_->setAccessibleName(tr("Bus input radix"));
    busRadixCombo_->setToolTip(tr("Interpret values without a prefix using this radix"));
    busRadixCombo_->addItem(QStringLiteral("HEX"), static_cast<int>(Radix::Hexadecimal));
    busRadixCombo_->addItem(QStringLiteral("BIN"), static_cast<int>(Radix::Binary));
    busRadixCombo_->addItem(QStringLiteral("DEC"), static_cast<int>(Radix::Decimal));
    busRadixCombo_->addItem(QStringLiteral("OCT"), static_cast<int>(Radix::Octal));
    busRadixCombo_->setMaximumWidth(72);
    presetLayout->addWidget(busRadixCombo_);
    busValueEdit_ = new QLineEdit(busPresetPalette_);
    busValueEdit_->setObjectName(QStringLiteral("BusPresetValueEdit"));
    busValueEdit_->setPlaceholderText(tr("Value + Enter"));
    busValueEdit_->setAccessibleName(tr("Bus value"));
    busValueEdit_->setMinimumWidth(105);
    busValueEdit_->setMaximumWidth(150);
    laneValueCompletionModel_ = new QStringListModel(this);
    laneValueCompleter_ = new QCompleter(laneValueCompletionModel_, this);
    laneValueCompleter_->setCaseSensitivity(Qt::CaseInsensitive);
    laneValueCompleter_->setCompletionMode(QCompleter::PopupCompletion);
    busValueEdit_->setCompleter(laneValueCompleter_);
    busValueEdit_->installEventFilter(this);
    presetLayout->addWidget(busValueEdit_);
    busRecentValuesCombo_ = new QComboBox(busPresetPalette_);
    busRecentValuesCombo_->setObjectName(QStringLiteral("BusEditRecentValuesCombo"));
    busRecentValuesCombo_->setAccessibleName(tr("Recent Bus values"));
    busRecentValuesCombo_->setToolTip(tr("Reuse a recent value for this signal"));
    busRecentValuesCombo_->addItem(tr("Recent"));
    busRecentValuesCombo_->setEnabled(false);
    busRecentValuesCombo_->setMinimumWidth(82);
    busRecentValuesCombo_->setMaximumWidth(120);
    presetLayout->addWidget(busRecentValuesCombo_);
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
        });
    const std::array<std::tuple<QString, QString, QString>, 4> presets{{
        {QStringLiteral("zero"), QStringLiteral("0"), QStringLiteral("BusPresetZeroButton")},
        {QStringLiteral("x"), QStringLiteral("X"), QStringLiteral("BusPresetXButton")},
        {QStringLiteral("z"), QStringLiteral("Z"), QStringLiteral("BusPresetZButton")},
        {QStringLiteral("dont-care"), tr("Don't care"), QStringLiteral("BusPresetDontCareButton")},
    }};
    for (const auto& [presetId, label, objectName] : presets) {
        auto* button = new QToolButton(busPresetPalette_);
        button->setText(label);
        button->setObjectName(objectName);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->setCursor(Qt::PointingHandCursor);
        button->setToolTip(
            presetId == QStringLiteral("dont-care")
                ? tr("Ignore this target range during Expected/Actual comparison")
                : presetId == QStringLiteral("x")
                    ? tr("Drive an unknown value; X remains significant unless compare rules ignore it")
                    : tr("Apply %1 to the current Bus target").arg(label));
        presetLayout->addWidget(button);
        connect(button, &QToolButton::clicked, this, [this, presetId] {
            if (!busPresetLaneId_.empty() && busPresetAnchorTick_) {
                applyBusPreset(busPresetLaneId_, presetId.toStdString(), *busPresetAnchorTick_);
            }
        });
    }
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
        "QFrame#RangeEditPalette QLineEdit {"
        " color: #f3f6fa; background: #263241;"
        " border: 1px solid #7588a2; border-radius: 4px; padding: 3px 6px;"
        "}"
        "QFrame#RangeEditPalette QLineEdit:focus { border-color: #8dc0ff; }"
        "QFrame#RangeEditPalette QToolButton {"
        " color: #f3f6fa; background: #46566c;"
        " border: 1px solid #6f8099; border-radius: 4px; padding: 3px 8px;"
        "}"
        "QFrame#RangeEditPalette QToolButton:hover {"
        " background: #56708f; border-color: #8dc0ff;"
        "}"
        "QFrame#RangeEditPalette QToolButton:disabled {"
        " color: #8894a5; background: #374352; border-color: #526074;"
        "}"));
    auto* rangeLayout = new QHBoxLayout(rangeEditPalette_);
    rangeLayout->setContentsMargins(7, 5, 7, 5);
    rangeLayout->setSpacing(5);
    rangeEditContextLabel_ = new QLabel(tr("Selected range"), rangeEditPalette_);
    rangeEditContextLabel_->setObjectName(QStringLiteral("RangeEditContextLabel"));
    rangeEditContextLabel_->setMaximumWidth(310);
    rangeLayout->addWidget(rangeEditContextLabel_);
    rangeCopyButton_ = new QToolButton(rangeEditPalette_);
    rangeCopyButton_->setText(tr("Copy"));
    rangeCopyButton_->setObjectName(QStringLiteral("RangeEditCopyButton"));
    rangeCopyButton_->setAutoRaise(true);
    rangeCopyButton_->setFocusPolicy(Qt::NoFocus);
    rangeCopyButton_->setCursor(Qt::PointingHandCursor);
    rangeCopyButton_->setToolTip(tr("Copy the selected signals and time range (Ctrl+C)"));
    rangeCopyButton_->setAccessibleName(tr("Copy selected range"));
    rangeLayout->addWidget(rangeCopyButton_);
    connect(rangeCopyButton_, &QToolButton::clicked, this, &WaveCanvas::copySelection);
    rangeCutButton_ = new QToolButton(rangeEditPalette_);
    rangeCutButton_->setText(tr("Cut"));
    rangeCutButton_->setObjectName(QStringLiteral("RangeEditCutButton"));
    rangeCutButton_->setAutoRaise(true);
    rangeCutButton_->setFocusPolicy(Qt::NoFocus);
    rangeCutButton_->setCursor(Qt::PointingHandCursor);
    rangeCutButton_->setToolTip(tr("Copy and clear the selected range (Ctrl+X)"));
    rangeCutButton_->setAccessibleName(tr("Cut selected range"));
    rangeLayout->addWidget(rangeCutButton_);
    connect(rangeCutButton_, &QToolButton::clicked, this, &WaveCanvas::cutSelection);
    rangePasteButton_ = new QToolButton(rangeEditPalette_);
    rangePasteButton_->setText(tr("Paste"));
    rangePasteButton_->setObjectName(QStringLiteral("RangeEditPasteButton"));
    rangePasteButton_->setAutoRaise(true);
    rangePasteButton_->setFocusPolicy(Qt::NoFocus);
    rangePasteButton_->setCursor(Qt::PointingHandCursor);
    rangePasteButton_->setToolTip(tr("Paste a copied range at the selected start (Ctrl+V)"));
    rangePasteButton_->setAccessibleName(tr("Paste copied range into selected signals"));
    rangeLayout->addWidget(rangePasteButton_);
    connect(rangePasteButton_, &QToolButton::clicked, this, &WaveCanvas::pasteAtCursor);
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, [this] {
        if (rangeEditPaletteVisible_) showRangeEditPalette();
    });
    rangeClearButton_ = new QToolButton(rangeEditPalette_);
    rangeClearButton_->setText(tr("Clear"));
    rangeClearButton_->setObjectName(QStringLiteral("RangeEditClearButton"));
    rangeClearButton_->setAutoRaise(true);
    rangeClearButton_->setFocusPolicy(Qt::NoFocus);
    rangeClearButton_->setCursor(Qt::PointingHandCursor);
    rangeClearButton_->setToolTip(tr("Clear values in the selected range (Delete)"));
    rangeClearButton_->setAccessibleName(tr("Clear selected range"));
    rangeLayout->addWidget(rangeClearButton_);
    connect(rangeClearButton_, &QToolButton::clicked, this, [this] {
        static_cast<void>(clearExplicitRange());
    });
    rangeValueEdit_ = new QLineEdit(rangeEditPalette_);
    rangeValueEdit_->setObjectName(QStringLiteral("RangeEditValueEdit"));
    rangeValueEdit_->setPlaceholderText(tr("Value + Enter"));
    rangeValueEdit_->setAccessibleName(tr("Selected bus range value"));
    rangeValueEdit_->setMinimumWidth(105);
    rangeValueEdit_->setMaximumWidth(140);
    rangeValueCompletionModel_ = new QStringListModel(this);
    rangeValueCompleter_ = new QCompleter(rangeValueCompletionModel_, this);
    rangeValueCompleter_->setCaseSensitivity(Qt::CaseInsensitive);
    rangeValueCompleter_->setCompletionMode(QCompleter::PopupCompletion);
    rangeValueEdit_->setCompleter(rangeValueCompleter_);
    rangeValueEdit_->installEventFilter(this);
    rangeLayout->addWidget(rangeValueEdit_);
    const auto makeRangeButton = [this, rangeLayout](
                                     const QString& text,
                                     const QString& objectName,
                                     const std::string& presetId) {
        auto* button = new QToolButton(rangeEditPalette_);
        button->setText(text);
        button->setObjectName(objectName);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
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
        kind == LaneKind::Clock ? tr("Period, e.g. 10 ns") : tr("Width, e.g. 8"));
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
    emit statusMessage(message);
}

bool WaveCanvas::event(QEvent* event)
{
    if (event->type() == QEvent::KeyPress && tool_ == Tool::WaveEdit) {
        const auto* keyEvent = static_cast<QKeyEvent*>(event);
        const auto segmentForward = keyEvent->key() == Qt::Key_Tab
            && keyEvent->modifiers() == Qt::ControlModifier;
        const auto segmentBackward =
            (keyEvent->key() == Qt::Key_Backtab
             && keyEvent->modifiers()
                 == (Qt::ControlModifier | Qt::ShiftModifier))
            || (keyEvent->key() == Qt::Key_Tab
                && keyEvent->modifiers()
                    == (Qt::ControlModifier | Qt::ShiftModifier));
        if (segmentForward || segmentBackward) {
            navigateSelectedSegment(segmentForward);
            event->accept();
            return true;
        }
        const auto forward = keyEvent->key() == Qt::Key_Tab
            && keyEvent->modifiers() == Qt::NoModifier;
        const auto backward = (keyEvent->key() == Qt::Key_Backtab
            && keyEvent->modifiers() == Qt::ShiftModifier)
            || (keyEvent->key() == Qt::Key_Tab
                && keyEvent->modifiers() == Qt::ShiftModifier);
        if (forward || backward) {
            navigateSelectedBeat(forward);
            event->accept();
            return true;
        }
    }
    return QAbstractScrollArea::event(event);
}

bool WaveCanvas::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == durationEdit_ && event->type() == QEvent::FocusOut) {
        const auto* focusEvent = static_cast<QFocusEvent*>(event);
        durationEditMouseFocusOut_ = focusEvent->reason() == Qt::MouseFocusReason;
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
            hideBusPresetPalette();
            viewport()->setFocus(Qt::OtherFocusReason);
            return true;
        }
        if (watched == rangeValueEdit_ && acceptKey) {
            submitRangeValue();
            return true;
        }
        if (watched == rangeValueEdit_ && keyEvent->key() == Qt::Key_Escape) {
            clearExplicitRangeSelection();
            viewport()->setFocus(Qt::OtherFocusReason);
            emit statusMessage(tr("Range selection cleared"));
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
                ? tr("Rename cancelled · Selected group %1 · Delete removes group · F2 renames")
                      .arg(laneName)
                : tr("Rename cancelled · Selected signal %1 · Delete removes signal · F2 renames · Ctrl+D duplicates")
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
        && (*kind == LaneKind::Bus || *kind == LaneKind::Enum);
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
        tr("Edit the timeline end, for example 500 ns, then press Enter"));
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
    selectedLaneId_.clear();
    selectedLaneIds_.clear();
    selectionRange_.reset();
    movableCursorTick_.reset();
    clearWaveEditState();
    temporaryCursorTick_.reset();
    selectedMarkerId_.clear();
    cursorInteraction_ = CursorInteraction::None;
    laneHeaderPressed_ = false;
    laneHeaderDragging_ = false;
    laneHeaderSelectionActive_ = false;
    headerResizing_ = false;
    laneDragId_.clear();
    laneDropDestinationIndex_.reset();
    laneDropIndicatorY_.reset();
    lockedMarkerOriginalRange_.reset();
    hideBusPresetPalette();
    rebuildLaneLayout();
    syncDurationEditor();
    positionDurationEditor();
    fitPending_ = true;
    if (viewport()->width() > headerWidth_ + 40) {
        fitPending_ = false;
        fitScenario();
    }
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
    }
    if (headerResizing_) setSignalHeaderWidth(headerResizeOriginalWidth_);
    const auto previousTool = tool_;
    tool_ = tool;
    drawing_ = false;
    panning_ = false;
    spaceHeld_ = false;
    bypassSnap_ = false;
    snapGuideTick_.reset();
    cursorInteraction_ = CursorInteraction::None;
    lockedMarkerOriginalRange_.reset();
    laneHeaderPressed_ = false;
    laneHeaderDragging_ = false;
    headerResizing_ = false;
    laneDragId_.clear();
    laneDropDestinationIndex_.reset();
    laneDropIndicatorY_.reset();
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
    if (!lane || !lane->visible || lane->kind == LaneKind::Group) {
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
                    && selected->visible
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
           "Ctrl+C copies · Delete clears · Esc cancels")
            .arg(format(0))
            .arg(format(scenario_->duration))
            .arg(static_cast<qulonglong>(selectedLaneIds_.size())));
    viewport()->update();
}

void WaveCanvas::refreshModel()
{
    rebuildLaneLayout();
    if (scenario_) {
        std::erase_if(
            selectedLaneIds_,
            [this](const std::string& laneId) {
                const auto* lane = findLane(*scenario_, laneId);
                return !lane || !lane->visible;
            });
        if (!selectedLaneId_.empty()) {
            const auto* lane = findLane(*scenario_, selectedLaneId_);
            if (!lane || !lane->visible) {
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
        if (!lane || !lane->visible) hideBusPresetPalette();
    }
    if (!busPresetLaneId_.empty()) {
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
                    return lane && lane->visible;
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

void WaveCanvas::revealLane(const QString& laneId)
{
    if (!scenario_) return;
    const auto* lane = findLane(*scenario_, laneId.toStdString());
    if (!lane || !lane->visible) return;
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
    laneHeaderSelectionActive_ = true;
    snapGuideTick_.reset();
    ensureLaneVisible(lane->id);
    emit selectionChanged(laneId, cursorTick_);
    viewport()->update();
}

void WaveCanvas::goToTick(const qint64 tick)
{
    if (!scenario_) return;
    cursorTick_ = std::clamp<Tick>(tick, 0, scenario_->duration);
    ensureCursorVisible(cursorTick_);
    snapGuideTick_.reset();
    positionBusPresetPalette();
    viewport()->update();
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

    const auto durationBeforePaste = scenario_->duration;
    const auto relationCountBefore = scenario_->relations.size();
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
    if (changed) {
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
        if (help->pos().x() < headerWidth_) {
            if (const auto* lane = laneAtY(help->pos().y())) {
                QToolTip::showText(
                    help->globalPos(),
                    QString::fromStdString(lane->name),
                    viewport());
                return true;
            }
        }
        if (tool_ == Tool::WaveEdit
            && scenario_
            && project_
            && help->pos().x() >= headerWidth_) {
            if (const auto* lane = laneAtY(help->pos().y());
                lane
                && lane->kind != LaneKind::Group
                && lane->kind != LaneKind::Bit) {
                const auto tick = scenario_->duration > 0
                    ? std::clamp<Tick>(
                          tickAtX(help->pos().x()),
                          0,
                          scenario_->duration - 1)
                    : Tick{0};
                if (const auto* segment = segmentAtTick(*lane, tick)) {
                    const auto details =
                        tr("%1 · value %2\n%3–%4 · width %5\nClick selects · drag moves · double-click edits")
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
                            : details + tr("\nCtrl+drag copies"),
                        viewport());
                    return true;
                }
            }
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
    if (durationEditBlurConsumesCanvasInput_) {
        durationEditBlurConsumesCanvasInput_ = false;
        event->accept();
        return;
    }
    const auto hadPendingInlineEdit = hasQuickLaneSetup()
        || hasLaneRename()
        || hasPendingValueEdit();
    if (!commitPendingInlineEdits() || hadPendingInlineEdit) {
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
        selectedLaneId_ = lane->id;
        selectedLaneIds_ = {lane->id};
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
        auto selectionMessage = lane->kind == LaneKind::Group
            ? tr("Selected group %1 · Delete removes group · F2 renames")
                  .arg(QString::fromStdString(lane->name))
            : tr("Selected signal %1 · Delete removes signal · F2 renames · Ctrl+D duplicates")
                  .arg(QString::fromStdString(lane->name));
        if (tool_ == Tool::WaveEdit && lane->kind != LaneKind::Group) {
            selectionMessage.append(
                tr(" · value %1 · Up/Down selects signals · Ctrl+Left/Right jumps edges")
                    .arg(laneValueAt(*lane, cursorTick_)));
            selectionMessage.append(tr(" · Shift+Left/Right selects time · Shift+Home/End selects to boundary · Ctrl+Shift+Left/Right selects to edges · Ctrl+A selects full timeline"));
            if (lane->kind == LaneKind::Bus || lane->kind == LaneKind::Enum) {
                selectionMessage.append(tr(" · Enter edits value"));
            } else if (lane->kind == LaneKind::Clock) {
                selectionMessage.append(tr(" · G gates · X drives unknown · R runs"));
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
    QAction* pasteRange = nullptr;
    const auto* clipboardMime = QApplication::clipboard()->mimeData();
    if (clipboardMime && clipboardMime->hasFormat(kRangeMimeType)) {
        pasteRange = menu.addAction(tr("Paste copied range here"));
        pasteRange->setObjectName(QStringLiteral("PasteRangeHereAction"));
        pasteRange->setToolTip(
            tr("Paste the copied signals at this time as one undo command"));
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
        duplicateBefore->setToolTip(
            tr("Copy this complete segment into the immediately preceding interval"));
        duplicateAfter = menu.addAction(tr("Duplicate segment after"));
        duplicateAfter->setObjectName(QStringLiteral("DuplicateSegmentAfterAction"));
        duplicateAfter->setToolTip(
            tr("Copy this complete segment into the immediately following interval"));
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
            static_cast<void>(clearSelectedBitRange());
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
    if (event->key() == Qt::Key_Space
        && event->modifiers() == Qt::NoModifier
        && !event->isAutoRepeat()) {
        spaceHeld_ = true;
        viewport()->setCursor(Qt::OpenHandCursor);
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
        auto cancellation = tr("Move cancelled");
        if (scenario_) {
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
        laneDragId_.clear();
        laneDropDestinationIndex_.reset();
        laneDropIndicatorY_.reset();
        viewport()->setCursor(defaultCursorShape());
        viewport()->update();
        emit statusMessage(cancellation);
        event->accept();
        return;
    }

    if (scenario_
        && event->key() == Qt::Key_D
        && event->modifiers()
            == (Qt::ControlModifier | Qt::ShiftModifier)
        && tool_ == Tool::WaveEdit
        && !selectedSegmentId_.empty()) {
        static_cast<void>(duplicateSelectedSegmentBefore());
        event->accept();
        return;
    }

    if (scenario_
        && event->key() == Qt::Key_D
        && event->modifiers() == Qt::ControlModifier) {
        if (tool_ == Tool::WaveEdit && !selectedSegmentId_.empty()) {
            static_cast<void>(duplicateSelectedSegmentAfter());
            event->accept();
            return;
        }
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
        emit removeLaneRequested(QString::fromStdString(selectedLaneId_));
        event->accept();
        return;
    }

    if (tool_ == Tool::WaveEdit && scenario_) {
        if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
            && explicitRangeSelection_) {
            static_cast<void>(clearExplicitRange());
            event->accept();
            return;
        }
        if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
            && selectionRange_) {
            const auto* lane = findLane(*scenario_, selectedLaneId_);
            if (lane && lane->kind == LaneKind::Bit) {
                static_cast<void>(clearSelectedBitRange());
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
        if (event->key() == Qt::Key_Escape) {
            const auto cancelledWaveEditDrag = drawing_;
            const auto restoredViewport = cancelledWaveEditDrag
                && waveEditDragAutoScrolled_;
            stopWaveEditDragAutoScroll();
            if (restoredViewport) {
                horizontalScrollBar()->setValue(
                    waveEditDragOriginalHorizontalScroll_);
            }
            panning_ = false;
            clearWaveEditState();
            selectedLaneId_.clear();
            selectedLaneIds_.clear();
            laneHeaderSelectionActive_ = false;
            hideBusPresetPalette();
            snapGuideTick_.reset();
            viewport()->setCursor(Qt::PointingHandCursor);
            viewport()->update();
            if (cancelledWaveEditDrag) {
                emit statusMessage(
                    restoredViewport
                        ? tr("Waveform drag cancelled · view restored")
                        : tr("Waveform drag cancelled"));
            }
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Space
            && event->modifiers() == Qt::ControlModifier) {
            selectSegmentAtCursor();
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
                if (rangeValueEdit_ && rangeValueEdit_->isVisible()) {
                    rangeValueEdit_->setFocus(Qt::OtherFocusReason);
                    rangeValueEdit_->selectAll();
                    const auto rangeKind = explicitRangeKind();
                    emit statusMessage(
                        tr("Edit selected %1 range · type a value and press Enter · Esc clears the range")
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
        if ((event->key() == Qt::Key_Left
             || event->key() == Qt::Key_Right)
            && event->modifiers() == Qt::AltModifier
            && !selectedSegmentId_.empty()) {
            static_cast<void>(
                nudgeSelectedSegment(event->key() == Qt::Key_Right));
            event->accept();
            return;
        }
        if ((event->key() == Qt::Key_BracketLeft
             || event->key() == Qt::Key_BraceLeft)
            && (event->modifiers() == Qt::NoModifier
                || event->modifiers() == Qt::ShiftModifier)
            && !selectedSegmentId_.empty()) {
            static_cast<void>(resizeSelectedSegmentBoundary(
                SegmentBoundary::Start,
                event->modifiers() == Qt::NoModifier));
            event->accept();
            return;
        }
        if ((event->key() == Qt::Key_BracketRight
             || event->key() == Qt::Key_BraceRight)
            && (event->modifiers() == Qt::NoModifier
                || event->modifiers() == Qt::ShiftModifier)
            && !selectedSegmentId_.empty()) {
            static_cast<void>(resizeSelectedSegmentBoundary(
                SegmentBoundary::End,
                event->modifiers() == Qt::NoModifier));
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) {
            const auto step = cursorKeyboardStep();
            const auto direction = event->key() == Qt::Key_Left ? Tick{-1} : Tick{1};
            cursorTick_ = direction < 0
                ? cursorTick_ - std::min(cursorTick_, step)
                : cursorTick_ + std::min(scenario_->duration - cursorTick_, step);
            ensureCursorVisible(cursorTick_);
            snapGuideTick_.reset();
            emit statusMessage(
                tr("Edit cursor %1").arg(QString::fromStdString(
                    formatTick(cursorTick_, project_->timeBase))));
            viewport()->update();
            event->accept();
            return;
        }
        if (event->modifiers() == Qt::NoModifier
            && (event->key() == Qt::Key_G
                || event->key() == Qt::Key_X
                || event->key() == Qt::Key_R)) {
            const auto* lane = findLane(*scenario_, selectedLaneId_);
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
                        tr("No values changed · select only Bit, only Bus, or only Enum signals"));
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
    drawScenarioOverlays(painter, visibleStart, visibleEnd);
    drawWaveEditOverlay(painter);
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
    if (fitPending_ && viewport()->width() > headerWidth_ + 40) {
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

void WaveCanvas::mousePressEvent(QMouseEvent* event)
{
    if (!scenario_) {
        QAbstractScrollArea::mousePressEvent(event);
        return;
    }
    if (event->button() == Qt::LeftButton && durationEditBlurConsumesCanvasInput_) {
        durationEditBlurConsumesCanvasInput_ = false;
        event->accept();
        return;
    }
    const auto position = event->position().toPoint();
    auto rangeClearedForRetarget = false;
    if (event->button() == Qt::LeftButton && hasLaneRename()) {
        submitLaneRename();
        if (hasLaneRename()) {
            event->accept();
            return;
        }
    }
    if (event->button() == Qt::LeftButton && hasQuickLaneSetup()) {
        submitQuickLaneSetup();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && hasPendingValueEdit()) {
        static_cast<void>(commitPendingInlineEdits());
        event->accept();
        return;
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
            waveEditDragAutoScrolled_ = false;
            drawing_ = true;
            cursorTick_ = drawStart_;
            snapGuideTick_.reset();
            viewport()->setCursor(Qt::SplitHCursor);
            viewport()->update();
            event->accept();
            return;
        }
        clearExplicitRangeSelection();
        snapGuideTick_.reset();
        viewport()->setFocus(Qt::MouseFocusReason);
        const auto retargetsWithoutEditing = position.y() < RulerHeight
            || position.x() < headerWidth_;
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
        cursorTick_ = snappedTick(tickAtX(position.x()), nullptr);
        selectedSegmentLaneId_.clear();
        selectedSegmentId_.clear();
        selectionRange_.reset();
        snapGuideTick_.reset();
        if (rangeClearedForRetarget) {
            emit statusMessage(
                tr("Edit cursor %1 · range cleared")
                    .arg(QString::fromStdString(
                        formatTick(cursorTick_, project_->timeBase))));
        }
        viewport()->update();
        event->accept();
        return;
    }
    if (addLaneRowRect().contains(position)) return;
    auto* lane = laneAtY(position.y());
    bypassSnap_ = event->modifiers().testFlag(Qt::AltModifier);
    const auto startingExplicitRange = tool_ == Tool::WaveEdit
        && event->modifiers().testFlag(Qt::ShiftModifier);
    if (tool_ == Tool::WaveEdit
        && lane
        && (lane->kind == LaneKind::Bus || lane->kind == LaneKind::Enum)
        && !startingExplicitRange) {
        showBusPresetPalette(*lane, position);
    } else {
        hideBusPresetPalette();
    }

    if (position.x() < headerWidth_ && lane) {
        setFocus(Qt::MouseFocusReason);
        if (tool_ == Tool::WaveEdit) clearWaveEditState();
        selectedLaneId_ = lane->id;
        selectedLaneIds_ = {lane->id};
        const auto lockedMarkerDeselected =
            tool_ == Tool::Marker && !selectedMarkerId_.empty();
        if (tool_ == Tool::Marker) {
            selectedMarkerId_.clear();
            cursorInteraction_ = CursorInteraction::None;
            lockedMarkerOriginalRange_.reset();
        }
        laneHeaderSelectionActive_ = true;
        laneHeaderPressed_ = true;
        laneHeaderDragging_ = false;
        laneHeaderPressPosition_ = position;
        laneDragOriginalVerticalScroll_ = verticalScrollBar()->value();
        laneDragId_ = lane->id;
        laneDropDestinationIndex_.reset();
        laneDropIndicatorY_.reset();
        emit selectionChanged(QString::fromStdString(lane->id), cursorTick_);
        auto selectionMessage = lane->kind == LaneKind::Group
            ? tr("Selected group %1 · Delete removes group · F2 renames")
                  .arg(QString::fromStdString(lane->name))
            : tr("Selected signal %1 · Delete removes signal · F2 renames · Ctrl+D duplicates")
                  .arg(QString::fromStdString(lane->name));
        if (tool_ == Tool::WaveEdit && lane->kind != LaneKind::Group) {
            selectionMessage.append(
                tr(" · value %1 · Up/Down selects signals · Ctrl+Left/Right jumps edges")
                    .arg(laneValueAt(*lane, cursorTick_)));
            selectionMessage.append(tr(" · Shift+Left/Right selects time · Shift+Home/End selects to boundary · Ctrl+Shift+Left/Right selects to edges · Ctrl+A selects full timeline"));
            if (lane->kind == LaneKind::Bus || lane->kind == LaneKind::Enum) {
                selectionMessage.append(tr(" · Enter edits value"));
            } else if (lane->kind == LaneKind::Clock) {
                selectionMessage.append(tr(" · G gates · X drives unknown · R runs"));
            }
        }
        if (lockedMarkerDeselected) {
            selectionMessage.append(tr(" · locked cursor/range deselected"));
        }
        if (rangeClearedForRetarget) {
            selectionMessage.append(tr(" · range cleared"));
        }
        emit statusMessage(selectionMessage);
        viewport()->setCursor(Qt::OpenHandCursor);
        viewport()->update();
        return;
    }

    laneHeaderSelectionActive_ = false;
    laneHeaderPressed_ = false;
    laneHeaderDragging_ = false;
    laneDragId_.clear();
    laneDropDestinationIndex_.reset();
    laneDropIndicatorY_.reset();

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
            selectionRange_.reset();
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
    if (headerResizing_) {
        setSignalHeaderWidth(
            headerResizeOriginalWidth_ + position.x() - headerResizePressX_);
        viewport()->setCursor(Qt::SplitHCursor);
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
            laneDragId_.clear();
            laneDropDestinationIndex_.reset();
            laneDropIndicatorY_.reset();
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
            viewport()->setCursor(Qt::ClosedHandCursor);
            viewport()->update();
        }
        return;
    }
    if (tool_ == Tool::WaveEdit
        && drawing_
        && !event->buttons().testFlag(Qt::LeftButton)) {
        const auto restoredViewport = waveEditDragAutoScrolled_;
        stopWaveEditDragAutoScroll();
        if (restoredViewport) {
            horizontalScrollBar()->setValue(
                waveEditDragOriginalHorizontalScroll_);
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
    bypassSnap_ = event->modifiers().testFlag(Qt::AltModifier);
    interactionCurrent_ = position;
    const auto* lane = laneAtY(position.y());
    const auto rawTick = tickAtX(position.x());
    const auto boundedRaw = std::clamp<Tick>(rawTick, 0, scenario_->duration);
    cursorTick_ = tool_ == Tool::WaveEdit && lane && lane->kind != LaneKind::Group
        ? editTick(rawTick, *lane)
        : snappedTick(rawTick, lane);
    snapGuideTick_ = cursorTick_ == boundedRaw
        ? std::optional<Tick>{}
        : std::optional<Tick>{cursorTick_};

    if (tool_ == Tool::WaveEdit) {
        if (drawing_) {
            updateWaveEditDragAutoScroll(position, event->modifiers());
            const auto* editLane = findLane(*scenario_, drawLaneId_);
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
            viewport()->setCursor(Qt::SplitHCursor);
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
            const auto hadHover = waveEditHoverRange_.has_value()
                || !waveEditHoverLaneId_.empty();
            waveEditHoverLaneId_.clear();
            waveEditHoverRange_.reset();
            const auto hit = segmentHitAtPosition(*lane, position);
            viewport()->setCursor(
                hit.boundary == SegmentBoundary::None
                    ? (hit.segment ? Qt::SizeAllCursor : Qt::PointingHandCursor)
                    : Qt::SplitHCursor);
            if (hadHover) viewport()->update();
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
                   && waveEditInteraction_ == WaveEditInteraction::MoveSegment
                   && waveEditCopyDrag_) {
            message += tr("  |  Copy preview · source remains");
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
        emit statusMessage(message);
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
        if (laneHeaderDragging_) commitLaneReorder();
        laneHeaderPressed_ = false;
        laneHeaderDragging_ = false;
        laneDragId_.clear();
        laneDropDestinationIndex_.reset();
        laneDropIndicatorY_.reset();
        viewport()->setCursor(defaultCursorShape());
        viewport()->update();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && drawing_) {
        bypassSnap_ = event->modifiers().testFlag(Qt::AltModifier);
        switch (tool_) {
        case Tool::Draw:
            commitDraw(event->position().toPoint());
            break;
        case Tool::WaveEdit:
            stopWaveEditDragAutoScroll();
            commitWaveEdit(event->position().toPoint());
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
    const auto hadPendingInlineEdit = hasQuickLaneSetup()
        || hasLaneRename()
        || hasPendingValueEdit();
    if (hadPendingInlineEdit) {
        static_cast<void>(commitPendingInlineEdits());
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
            selectedLaneId_ = lane->id;
            selectedLaneIds_ = {lane->id};
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

int WaveCanvas::fittedSignalHeaderWidth() const
{
    QFont nameFont = viewport()->font();
    nameFont.setBold(true);
    const QFontMetrics nameMetrics(nameFont);
    const QFontMetrics detailMetrics(viewport()->font());
    auto fitted = DefaultHeaderWidth;
    if (scenario_) {
        for (const auto& lane : scenario_->lanes) {
            if (!lane.visible) continue;
            fitted = std::max(
                fitted,
                nameMetrics.horizontalAdvance(QString::fromStdString(lane.name)) + 32);
            auto detail = laneKindLabel(lane.kind);
            if (lane.kind == LaneKind::Bus || lane.kind == LaneKind::Enum) {
                detail += tr(" · %1-bit").arg(lane.width);
            }
            fitted = std::max(fitted, detailMetrics.horizontalAdvance(detail) + 28);
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

void WaveCanvas::positionBusPresetPalette()
{
    if (!busPresetPalette_ || !scenario_ || busPresetLaneId_.empty()
        || !busPresetAnchorTick_ || !busEditRange_) {
        hideBusPresetPalette();
        return;
    }
    const auto* lane = findLane(*scenario_, busPresetLaneId_);
    if (!lane
        || !lane->visible
        || (lane->kind != LaneKind::Bus && lane->kind != LaneKind::Enum)) {
        hideBusPresetPalette();
        return;
    }
    busPresetPalette_->adjustSize();
    busPresetPalette_->updateGeometry();
    if (!busEditPaletteVisible_) {
        busEditPaletteVisible_ = true;
        emit busEditPaletteVisibilityChanged(true);
    }
    busPresetPalette_->show();
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
    cursorTick_ = start;
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
                tr("\nTab applies and advances · Shift+Tab applies and goes back"));
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
    if (busValueEdit_) {
        const auto probe = start + (end - start) / 2;
        const auto* existing = segmentAtTick(lane, probe);
        busValueEdit_->setText(existing ? QString::fromStdString(existing->value) : QString{});
        busValueEdit_->setModified(false);
        busValueEdit_->setPlaceholderText(
            busEditScope_ == BusEditScope::Beat
                ? existing
                    ? enumLane
                        ? tr("Symbol · Tab next")
                        : tr("Value · Tab next")
                    : tr("X (implicit) · Tab skips")
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
        if (!enumLane) {
            busValueEdit_->setToolTip(
                busValueEdit_->toolTip()
                + tr("\nUp/Down adjusts a known numeric draft by one"));
        }
        busValueEdit_->setToolTip(
            busValueEdit_->toolTip()
            + tr("\nCtrl+Up/Down cycles this signal's recent values")
            + tr("\nCtrl+Enter applies and keeps the current target open"));
        busValueEdit_->setStyleSheet({});
    }
    positionBusPresetPalette();
    viewport()->update();
}

void WaveCanvas::hideBusPresetPalette()
{
    const auto restoreCanvasFocus = busValueEdit_ && busValueEdit_->hasFocus();
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
    showBusPresetPalette(
        *lane,
        QPoint(xAtTick(segment->start), 0),
        segment->start,
        std::pair{segment->start, segment->end},
        BusEditScope::Segment);
    if (busValueEdit_) {
        if (!seed.isNull()) {
            busValueEdit_->setText(seed);
            busValueEdit_->setModified(true);
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
    ensureCursorVisible(segment->start);
    emit selectionChanged(
        QString::fromStdString(lane->id),
        segment->start);
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
        || !lane->visible
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
        || !lane->visible
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
    auto value = busValueEdit_->text().trimmed();
    if (lane.kind != LaneKind::Bus || value.isEmpty()) return value;
    const auto lower = value.toLower();
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
std::optional<LaneKind> WaveCanvas::explicitRangeKind() const
{
    if (!scenario_ || !explicitRangeSelection_ || selectedLaneIds_.empty()) {
        return std::nullopt;
    }
    std::optional<LaneKind> kind;
    for (const auto& laneId : selectedLaneIds_) {
        const auto* lane = findLane(*scenario_, laneId);
        if (!lane
            || (lane->kind != LaneKind::Bit
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
    const auto editable = bitRange || busRange || enumRange;
    const auto presetRange = bitRange || busRange;
    const auto valueRange = busRange || enumRange;
    const auto enumSymbols = enumRange
        ? explicitRangeEnumSymbols()
        : QStringList{};
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
                      .arg(bitRange ? tr("Bit") : busRange ? tr("Bus") : tr("Enum"))
                      .arg(format(selectionRange_->first))
                      .arg(format(selectionRange_->second))
                : tr("Mixed/unsupported selection · Copy, cut, or clear"));
        auto contextHelp = editable
            ? tr("Applies to every selected signal from %1 to %2")
                  .arg(format(selectionRange_->first))
                  .arg(format(selectionRange_->second))
            : tr("Batch assignment requires only Bit, only Bus, or only Enum signals");
        contextHelp.append(
            tr("\nShift+Up/Down adjusts signals; Shift+Left/Right adjusts time; Shift+Home/End selects to a timeline boundary; Ctrl+Shift+Left/Right selects to signal edges; Ctrl+A selects the full timeline"));
        if (enumRange) {
            contextHelp.append(
                enumSymbols.isEmpty()
                    ? tr("\nNo declared symbols are shared by every selected Enum signal")
                    : tr("\nShared symbols: %1")
                          .arg(enumSymbols.join(QStringLiteral(", "))));
        }
        rangeEditContextLabel_->setToolTip(contextHelp);
    }

    if (rangeCopyButton_) {
        rangeCopyButton_->setVisible(true);
        rangeCopyButton_->setEnabled(true);
    }
    if (rangeCutButton_) {
        rangeCutButton_->setVisible(true);
        rangeCutButton_->setEnabled(true);
    }
    if (rangePasteButton_) {
        const auto* mime = QApplication::clipboard()->mimeData();
        const auto content = !mime
            ? QByteArray{}
            : mime->hasFormat(kRangeMimeType)
                ? mime->data(kRangeMimeType)
                : mime->text().toUtf8();
        const auto document = QJsonDocument::fromJson(content);
        const auto root = document.isObject() ? document.object() : QJsonObject{};
        const auto schemaVersion =
            root.value(QStringLiteral("schemaVersion")).toInt(-1);
        const auto copiedCount = (schemaVersion == 1 || schemaVersion == 2)
                && root.value(QStringLiteral("lanes")).isArray()
            ? root.value(QStringLiteral("lanes")).toArray().size()
            : qsizetype{0};
        rangePasteButton_->setVisible(true);
        rangePasteButton_->setEnabled(copiedCount > 0);
        rangePasteButton_->setToolTip(
            copiedCount <= 0
                ? tr("Copy a waveform range before pasting")
                : copiedCount == static_cast<qsizetype>(selectedLaneIds_.size())
                    ? tr("Paste %1 copied signal(s) into the selected targets at %2 (Ctrl+V)")
                          .arg(copiedCount)
                          .arg(format(selectionRange_->first))
                    : tr("Copied range has %1 signal(s); current selection has %2")
                          .arg(copiedCount)
                          .arg(static_cast<qulonglong>(selectedLaneIds_.size())));
    }
    if (rangeClearButton_) {
        rangeClearButton_->setVisible(true);
        rangeClearButton_->setEnabled(true);
    }
    for (auto* button : {
             rangeZeroButton_,
             rangeOneButton_,
             rangeXButton_,
             rangeZButton_,
             rangeDontCareButton_}) {
        if (button) button->setEnabled(presetRange);
    }
    if (rangeZeroButton_) rangeZeroButton_->setVisible(presetRange);
    if (rangeOneButton_) rangeOneButton_->setVisible(bitRange);
    if (rangeXButton_) rangeXButton_->setVisible(presetRange);
    if (rangeZButton_) rangeZButton_->setVisible(presetRange);
    if (rangeDontCareButton_) rangeDontCareButton_->setVisible(busRange);
    if (rangeValueEdit_) {
        const auto restoreCanvasFocus = rangeValueEdit_->hasFocus() && !valueRange;
        rangeValueEdit_->setVisible(valueRange);
        if (!valueRange) {
            rangeValueEdit_->clear();
            rangeValueEdit_->setModified(false);
            rangeValueEdit_->setStyleSheet({});
            rangeValueEdit_->setToolTip({});
        } else {
            rangeValueEdit_->setAccessibleName(
                enumRange
                    ? tr("Selected Enum range value")
                    : tr("Selected Bus range value"));
            rangeValueEdit_->setPlaceholderText(
                enumRange ? tr("Symbol + Enter") : tr("Value + Enter"));
            if (!rangeValueEdit_->hasFocus() && !rangeValueEdit_->isModified()) {
                rangeValueEdit_->clear();
            }
            if (!rangeValueEdit_->isModified()) {
                rangeValueEdit_->setToolTip(
                    enumRange
                        ? enumSymbols.isEmpty()
                            ? tr("Type one numeric value for every selected Enum signal")
                            : tr("Type one shared symbol for the selected Enum range · symbols: %1")
                                  .arg(enumSymbols.join(QStringLiteral(", ")))
                        : tr("Type one value for the whole selected Bus range"));
                rangeValueEdit_->setStyleSheet({});
            }
        }
        if (restoreCanvasFocus) viewport()->setFocus(Qt::OtherFocusReason);
    }
    rangeEditPalette_->adjustSize();
    rangeEditPalette_->updateGeometry();
    if (!rangeEditPaletteVisible_) {
        rangeEditPaletteVisible_ = true;
        emit rangeEditPaletteVisibilityChanged(true);
    }
}

void WaveCanvas::hideRangeEditPalette()
{
    const auto restoreCanvasFocus = rangeValueEdit_ && rangeValueEdit_->hasFocus();
    if (rangeEditPaletteVisible_) {
        rangeEditPaletteVisible_ = false;
        emit rangeEditPaletteVisibilityChanged(false);
    }
    if (restoreCanvasFocus) viewport()->setFocus(Qt::OtherFocusReason);
}

void WaveCanvas::clearExplicitRangeSelection(const bool clearLanes)
{
    explicitRangeSelection_ = false;
    hideRangeEditPalette();
    if (rangeValueEdit_) {
        rangeValueEdit_->setModified(false);
        rangeValueEdit_->setStyleSheet({});
    }
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
            tr("No values changed · select only Bit, only Bus, or only Enum signals"));
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
    const auto displayValue = !presetId.empty()
        ? busPresetDisplayLabel(presetId)
        : QString::fromStdString(value);
    if (!changed) {
        emit statusMessage(
            tr("%1 signals · %2–%3 already = %4 · no values changed")
                .arg(static_cast<qulonglong>(selectedLaneIds_.size()))
                .arg(QString::fromStdString(formatTick(start, project_->timeBase)))
                .arg(QString::fromStdString(formatTick(end, project_->timeBase)))
                .arg(displayValue));
        return true;
    }
    auto message = tr("%1 signals · %2–%3 = %4")
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

    const auto [start, end] = *selectionRange_;
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

    auto message = (cutting
                        ? tr("Cut %1 signal(s) over %2 · Ctrl+Z restores source")
                        : tr("Cleared %1 signal(s) over %2 · Ctrl+Z to undo"))
                       .arg(static_cast<qulonglong>(laneIds.size()))
                       .arg(QString::fromStdString(
                           formatTick(end - start, project_->timeBase)));
    const auto removedRelationCount = relationCountBefore
        - std::min(relationCountBefore, scenario_->relations.size());
    if (removedRelationCount > 0) {
        message = (cutting
                       ? tr("Cut %1 signal(s) over %2 · removed %3 relation(s) · "
                            "Ctrl+Z restores source and relations")
                       : tr("Cleared %1 signal(s) over %2 · removed %3 relation(s) · "
                            "Ctrl+Z restores all"))
                      .arg(static_cast<qulonglong>(laneIds.size()))
                      .arg(QString::fromStdString(
                          formatTick(end - start, project_->timeBase)))
                      .arg(static_cast<qulonglong>(removedRelationCount));
    }
    emit statusMessage(message);
    return true;
}

void WaveCanvas::applyExplicitRangePreset(const std::string& presetId)
{
    const auto kind = explicitRangeKind();
    if (!kind) {
        emit statusMessage(
            tr("No values changed · select only Bit, only Bus, or only Enum signals"));
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

void WaveCanvas::submitRangeValue()
{
    if (!rangeValueEdit_ || !explicitRangeSelection_) return;
    const auto value = rangeValueEdit_->text().trimmed();
    if (value.isEmpty()) {
        const auto kind = explicitRangeKind();
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
        rangeValueEdit_->setFocus(Qt::OtherFocusReason);
        emit statusMessage(tr("No values changed · %1").arg(message));
        return;
    }
    if (applyExplicitRangeValue(value.toStdString())) {
        rangeValueEdit_->setModified(false);
        rangeValueEdit_->clearFocus();
        viewport()->setFocus(Qt::OtherFocusReason);
        return;
    }
    rangeValueEdit_->setModified(true);
    rangeValueEdit_->setFocus(Qt::OtherFocusReason);
    rangeValueEdit_->selectAll();
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
        emit statusMessage(message);
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
        emit statusMessage(tr("No values changed · %1").arg(message));
        viewport()->update();
        return;
    }
    const auto laneId = busPresetLaneId_;
    const auto laneName = lane->name;
    const auto [start, end] = *busEditRange_;
    const auto editorScope = busEditScope_;
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
        emit statusMessage(
            alreadyMatches
                ? message
                : appendRelationAwareUndo(
                      message,
                      relationCountBefore,
                      scenario_->relations.size()));
        applied = true;
    } else {
        applied = setLaneRangeValue(
            laneId,
            start,
            end,
            validation.normalizedValue);
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
        positionBusPresetPalette();
    }
    viewport()->setFocus(Qt::OtherFocusReason);
    viewport()->update();
    const auto message = changed
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
    emit statusMessage(
        changed
            ? appendRelationAwareUndo(
                  message,
                  relationCountBefore,
                  scenario_->relations.size())
            : message);
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
    const auto message = changed
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
    JsonExtensions extensions)
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
    const auto laneKind = lane->kind;
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
    if (laneKind == LaneKind::Bit) {
        waveEditHoverLaneId_ = laneId;
        waveEditHoverRange_ = selectionRange_;
    } else if (const auto* refreshedLane = findLane(*scenario_, laneId)) {
        const auto probe = start + (end - start) / 2;
        if (const auto* segment = segmentAtTick(*refreshedLane, probe)) {
            selectedSegmentLaneId_ = laneId;
            selectedSegmentId_ = segment->id;
            selectionRange_ = std::pair{segment->start, segment->end};
        }
    }
    if (changed) {
        emit modelEdited();
        emit commandAvailabilityChanged();
    }
    emit selectionChanged(QString::fromStdString(laneId), cursorTick_);
    refreshModel();
    const auto message = changed
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
    emit statusMessage(
        changed
            ? appendRelationAwareUndo(
                  message,
                  relationCountBefore,
                  scenario_->relations.size())
            : message);
    return true;
}

bool WaveCanvas::clearSelectedBitRange()
{
    if (!scenario_ || !commandStack_ || explicitRangeSelection_
        || !selectionRange_ || selectionRange_->second <= selectionRange_->first) {
        return false;
    }
    const auto* lane = findLane(*scenario_, selectedLaneId_);
    if (!lane || lane->kind != LaneKind::Bit) return false;
    const auto [start, end] = *selectionRange_;
    if (start < 0 || end > scenario_->duration) return false;
    const auto intersectsExplicit = std::any_of(
        lane->segments.begin(),
        lane->segments.end(),
        [start, end](const Segment& segment) {
            return segment.start < end && segment.end > start;
        });
    if (!intersectsExplicit) {
        emit statusMessage(
            tr("%1 · %2–%3 already uses implicit 0")
                .arg(QString::fromStdString(lane->name))
                .arg(QString::fromStdString(formatTick(start, project_->timeBase)))
                .arg(QString::fromStdString(formatTick(end, project_->timeBase))));
        return false;
    }

    const auto laneId = lane->id;
    const auto laneName = lane->name;
    const auto relationCountBefore = scenario_->relations.size();
    try {
        commandStack_->execute(std::make_unique<ClearLaneRangeCommand>(
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
    emit modelEdited();
    emit commandAvailabilityChanged();
    emit selectionChanged(QString::fromStdString(laneId), cursorTick_);
    refreshModel();
    emit statusMessage(appendRelationAwareUndo(
        tr("%1 · %2–%3 cleared to implicit 0")
            .arg(QString::fromStdString(laneName))
            .arg(QString::fromStdString(formatTick(start, project_->timeBase)))
            .arg(QString::fromStdString(formatTick(end, project_->timeBase))),
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

bool WaveCanvas::duplicateSelectedSegment(const bool after)
{
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
    emit selectionChanged(QString::fromStdString(laneId), cursorTick_);
    emit statusMessage(appendRelationAwareUndo(
        (after
             ? tr("%1 segment duplicated after to %2–%3 · source kept")
             : tr("%1 segment duplicated before to %2–%3 · source kept"))
            .arg(QString::fromStdString(laneName))
            .arg(formatTime(targetStart))
            .arg(formatTime(targetEnd)),
        relationCountBefore,
        scenario_->relations.size()));
    viewport()->update();
    return true;
}

bool WaveCanvas::nudgeSelectedSegment(const bool forward)
{
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
    ensureCursorVisible(cursorTick_);
    emit selectionChanged(QString::fromStdString(laneId), cursorTick_);
    emit statusMessage(appendRelationAwareUndo(
        tr("%1 Segment nudged %2 to %3–%4 · step %5")
            .arg(QString::fromStdString(laneName))
            .arg(forward ? tr("later") : tr("earlier"))
            .arg(formatTime(targetStart))
            .arg(formatTime(targetEnd))
            .arg(formatTime(unit)),
        relationCountBefore,
        scenario_->relations.size()));
    viewport()->update();
    return true;
}

bool WaveCanvas::resizeSelectedSegmentBoundary(
    const SegmentBoundary boundary,
    const bool expand)
{
    if (!scenario_ || !commandStack_
        || boundary == SegmentBoundary::None
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
    ensureCursorVisible(cursorTick_);
    emit selectionChanged(QString::fromStdString(laneId), cursorTick_);
    emit statusMessage(appendRelationAwareUndo(
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
            .arg(formatTime(unit)),
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
    waveEditDragAutoScrollDirection_ = direction;
    if (direction == 0) {
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
        || waveEditDragAutoScrollDirection_ == 0) {
        stopWaveEditDragAutoScroll();
        return;
    }
    const auto previous = horizontalScrollBar()->value();
    horizontalScrollBar()->setValue(
        previous
        + waveEditDragAutoScrollDirection_ * WaveEditDragAutoScrollStep);
    if (horizontalScrollBar()->value() == previous) {
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
    waveEditDragAutoScrollModifiers_ = Qt::NoModifier;
    if (waveEditDragAutoScrollTimer_) waveEditDragAutoScrollTimer_->stop();
}

void WaveCanvas::updateLaneDropTarget(const int y)
{
    laneDropDestinationIndex_.reset();
    laneDropIndicatorY_.reset();
    if (!scenario_ || scenario_->lanes.empty() || laneDragId_.empty()) return;

    const auto source = std::find_if(
        scenario_->lanes.begin(),
        scenario_->lanes.end(),
        [this](const Lane& lane) { return lane.id == laneDragId_; });
    if (source == scenario_->lanes.end() || laneLayout_.empty()) return;
    const auto sourceIndex = static_cast<std::size_t>(
        std::distance(scenario_->lanes.begin(), source));

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
        || laneDragId_.empty()
        || !laneDropDestinationIndex_) {
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
    if (!laneHeaderDragging_ || !laneDropIndicatorY_) return;

    const auto y = *laneDropIndicatorY_;
    const QColor accent(111, 168, 255);
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
    painter.restore();
}

void WaveCanvas::setScale(const double scale, const int anchorX)
{
    if (!scenario_) return;
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
    const auto contentX = static_cast<double>(horizontalScrollBar()->value())
        + static_cast<double>(x - headerWidth_);
    const auto tick = static_cast<long double>(contentX) / pixelsPerTick_;
    return std::clamp<Tick>(
        static_cast<Tick>(std::llround(tick)),
        0,
        scenario_->duration);
}

int WaveCanvas::xAtTick(const Tick tick) const
{
    const auto x = static_cast<double>(headerWidth_)
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

void WaveCanvas::navigateSelectedBeat(const bool forward)
{
    if (!scenario_ || scenario_->duration <= 0) {
        emit statusMessage(tr("Timeline has no editable beat"));
        return;
    }
    if (drawing_ || laneHeaderPressed_ || laneHeaderDragging_) {
        emit statusMessage(
            tr("Finish or cancel the current drag before navigating beats"));
        return;
    }
    if (explicitRangeSelection_) {
        emit statusMessage(
            tr("Esc clears the selected range before navigating beats"));
        return;
    }
    const auto* lane = findLane(*scenario_, selectedLaneId_);
    if (!lane || lane->kind == LaneKind::Group) {
        emit statusMessage(
            tr("Select a signal before using Tab or Shift+Tab"));
        return;
    }
    if (hasPendingBusValueEdit()) {
        emit statusMessage(
            tr("Finish or cancel the Bus/Enum draft before navigating beats"));
        if (busValueEdit_) {
            busValueEdit_->setFocus(Qt::OtherFocusReason);
            busValueEdit_->selectAll();
        }
        return;
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
        return;
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
                 ? tr("Next Segment from edit cursor on %1 · %2 · %3–%4 · Ctrl+Shift+Tab goes back")
                 : tr("Previous Segment from edit cursor on %1 · %2 · %3–%4 · Ctrl+Tab goes forward"))
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
                ? tr("No next Segment on %1 · Ctrl+Shift+Tab goes back")
                      .arg(QString::fromStdString(lane->name))
                : tr("No previous Segment on %1 · Ctrl+Tab goes forward")
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
             ? tr("Next Segment on %1 · %2 · %3–%4 · Ctrl+Shift+Tab goes back")
             : tr("Previous Segment on %1 · %2 · %3–%4 · Ctrl+Tab goes forward"))
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
            tr("Select a signal before pressing Ctrl+Space"));
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
    auto message =
        tr("%1 Segment selected at edit cursor %2 · value %3 · %4–%5")
            .arg(QString::fromStdString(lane->name))
            .arg(formatTime(cursorTick_))
            .arg(QString::fromStdString(segment->value))
            .arg(formatTime(segment->start))
            .arg(formatTime(segment->end));
    if (lane->kind == LaneKind::Bus || lane->kind == LaneKind::Enum) {
        message.append(tr(" · Enter edits"));
    }
    message.append(
        tr(" · Ctrl+Tab next · Ctrl+Shift+Tab previous · Delete removes"));
    emit statusMessage(message);
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
    explicitRangeSelection_ = false;
    hideRangeEditPalette();
    if (rangeValueEdit_) rangeValueEdit_->setModified(false);
    selectedSegmentLaneId_.clear();
    selectedSegmentId_.clear();
    waveEditInteraction_ = WaveEditInteraction::None;
    waveEditOriginalRange_.reset();
    waveEditPreviewRange_.reset();
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

void WaveCanvas::adjustTimeRangeByKeyboard(
    const bool forward,
    const KeyboardRangeTarget target)
{
    if (!scenario_ || scenario_->duration <= 0) {
        emit statusMessage(tr("Timeline has no editable time range"));
        return;
    }
    const auto* lane = findLane(*scenario_, selectedLaneId_);
    if (!lane || !lane->visible || lane->kind == LaneKind::Group) {
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
    if (explicitRangeSelection_
        && selectionRange_
        && selectionRange_->second > selectionRange_->first) {
        const auto [start, end] = *selectionRange_;
        if (cursorTick_ == end) {
            anchor = start;
            active = end;
        } else if (cursorTick_ == start) {
            anchor = end;
            active = start;
        } else {
            const auto startDistance = cursorTick_ >= start
                ? cursorTick_ - start
                : start - cursorTick_;
            const auto endDistance = cursorTick_ >= end
                ? cursorTick_ - end
                : end - cursorTick_;
            if (endDistance < startDistance) {
                anchor = start;
                active = end;
            } else {
                anchor = end;
                active = start;
            }
        }
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
        const auto step = cursorKeyboardStep();
        next = forward
            ? active + std::min(scenario_->duration - active, step)
            : active - std::min(active, step);
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
            tr("Keyboard range %1 to %2 · %3 · %4 signal(s) · "
               "Shift+Left/Right adjusts the active edge · Ctrl+Shift+Left/Right adjusts to signal edges · Shift+Home/End selects to boundary · Shift+Up/Down adjusts signals · Esc clears")
                .arg(format(selectionRange_->first))
                .arg(format(selectionRange_->second))
                .arg(format(selectionRange_->second - selectionRange_->first))
                .arg(static_cast<qulonglong>(selectedLaneIds_.size())));
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
        if (lane.visible && lane.kind != LaneKind::Group) {
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
        ? *kind == LaneKind::Bit
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
    const auto selectable = [](const Lane& lane) {
        return lane.visible && lane.kind != LaneKind::Group;
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
    const auto copyDrag = interaction == WaveEditInteraction::MoveSegment
        && waveEditCopyDrag_;
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
                *rangeKind == LaneKind::Enum
                    ? tr("Selected %1–%2 · width %3 · %4 Enum signals · type a shared symbol in the range toolbar · Shift+Up/Down adjusts signals · Esc clears")
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
                    : tr("Selected %1–%2 · width %3 · %4 signals · use 0/1/X/Z or the range toolbar · Shift+Up/Down adjusts signals · Esc clears")
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
                tr("Selected %1–%2 · width %3 · mixed/unsupported signal types · Copy, Cut, or Delete to clear · Shift+Up/Down adjusts signals")
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
        }
        refreshModel();
        if (const auto* refreshedLane = findLane(*scenario_, editedLaneId)) {
            const auto midpoint = start + (end - start) / 2;
            if (const auto* refreshed = segmentAtTick(*refreshedLane, midpoint)) {
                selectedSegmentLaneId_ = refreshedLane->id;
                selectedSegmentId_ = refreshed->id;
                selectionRange_ = std::pair{refreshed->start, refreshed->end};
            }
        }
        const auto message = changed
            ? tr("%1 segment copied to %2–%3 · source kept")
                  .arg(QString::fromStdString(editedLaneName))
                  .arg(QString::fromStdString(formatTick(start, project_->timeBase)))
                  .arg(QString::fromStdString(formatTick(end, project_->timeBase)))
            : tr("%1 copy target already matches · no values changed")
                  .arg(QString::fromStdString(editedLaneName));
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
    if (start != segment->start || end != segment->end) {
        const auto relationCountBefore = scenario_->relations.size();
        try {
            commandStack_->execute(std::make_unique<EditSegmentCommand>(
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
        emit statusMessage(appendRelationAwareUndo(
            tr("%1 segment %2 to %3–%4")
                .arg(QString::fromStdString(editedLaneName))
                .arg(interaction == WaveEditInteraction::MoveSegment ? tr("moved") : tr("resized"))
                .arg(QString::fromStdString(formatTick(start, project_->timeBase)))
                .arg(QString::fromStdString(formatTick(end, project_->timeBase))),
            relationCountBefore,
            scenario_->relations.size()));
    } else {
        const auto laneName = QString::fromStdString(editedLaneName);
        const auto value = QString::fromStdString(segmentValue);
        const auto startText = QString::fromStdString(
            formatTick(start, project_->timeBase));
        const auto endText = QString::fromStdString(
            formatTick(end, project_->timeBase));
        const auto widthText = QString::fromStdString(
            formatTick(end - start, project_->timeBase));
        emit statusMessage(
            lane->kind == LaneKind::Clock
                ? tr("%1 Segment selected · value %2 · %3–%4 · width %5 · Alt+Left/Right moves · double-click edits · Delete removes")
                      .arg(laneName, value, startText, endText, widthText)
                : tr("%1 Segment selected · value %2 · %3–%4 · width %5 · Alt+Left/Right moves · Ctrl+D copies after · Ctrl+Shift+D copies before · double-click edits · Delete removes")
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
        cursorTick_ = segment->start;
        showBusPresetPalette(
            *lane,
            position,
            segment->start,
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
    painter.fillRect(
        QRect(0, y, headerWidth_, layout.height),
        selected ? QColor(63, 80, 104) : kHeaderBackground);
    if (selected) painter.fillRect(waveformRect, QColor(105, 151, 205, 34));
    painter.setPen(kTextPrimary);
    QFont nameFont = painter.font();
    nameFont.setBold(selected);
    painter.setFont(nameFont);
    const auto sampledValue = tool_ == Tool::Marker
        ? cursorValue(lane)
        : tool_ == Tool::WaveEdit && selected && !drawing_
            ? laneValueAt(lane, cursorTick_)
            : QString{};
    const auto valueWidth = sampledValue.isEmpty() ? 0 : 78;
    const QRect nameRect(
        14,
        y + 4,
        headerWidth_ - 28 - valueWidth,
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
    if (lane.kind == LaneKind::Bus || lane.kind == LaneKind::Enum) {
        detail += tr(" · %1-bit").arg(lane.width);
    }
    painter.drawText(
        QRect(14, y + layout.height / 2 - 2, headerWidth_ - 28, layout.height / 2),
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
        painter.setPen(QPen(laneColor(lane), 2.0));
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
    const Tick visibleEnd)
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
        painter.setPen(QPen(laneColor(lane), 1.5));
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

void WaveCanvas::drawWaveEditOverlay(QPainter& painter)
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
                               const bool preview) {
        const auto layout = layoutForLane(laneId);
        if (layout == laneLayout_.end() || range.second <= range.first) return;
        const auto y = RulerHeight + layout->top - verticalScrollBar()->value();
        if (y + layout->height < RulerHeight || y > viewport()->height()) return;
        const auto left = xAtTick(range.first);
        const auto right = xAtTick(range.second);
        const QRect segmentRect(
            QPoint(left, y + 2),
            QPoint(std::max(left + 1, right), y + layout->height - 3));
        auto fill = accent;
        fill.setAlpha(preview ? 58 : 34);
        painter.fillRect(segmentRect, fill);
        painter.setPen(QPen(
            accent,
            preview ? 1.5 : 2.0,
            preview ? Qt::DashLine : Qt::SolidLine));
        painter.drawRect(segmentRect.adjusted(0, 0, -1, -1));
        if (!handles) return;
        const auto handleY = y + layout->height / 2 - 10;
        painter.setPen(QPen(QColor(222, 237, 255), 1.0));
        painter.setBrush(accent);
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
        for (std::size_t index = 0; index < selectedLaneIds_.size(); ++index) {
            const auto handles = index == 0 || index + 1 == selectedLaneIds_.size();
            drawRange(selectedLaneIds_[index], *selectionRange_, handles, resizing);
        }
        if (resizing && !selectedLaneIds_.empty()) {
            const auto layout = layoutForLane(selectedLaneIds_.front());
            if (layout != laneLayout_.end()) {
                const auto timingLabel = tr("%1–%2 · width %3")
                    .arg(
                        QString::fromStdString(formatTick(
                            selectionRange_->first,
                            project_->timeBase)),
                        QString::fromStdString(formatTick(
                            selectionRange_->second,
                            project_->timeBase)),
                        QString::fromStdString(formatTick(
                            selectionRange_->second - selectionRange_->first,
                            project_->timeBase)));
                const auto labelWidth = std::min(
                    painter.fontMetrics().horizontalAdvance(timingLabel) + 16,
                    std::max(0, viewport()->width() - headerWidth_ - 8));
                const auto maximumLeft = std::max(
                    headerWidth_ + 4,
                    viewport()->width() - labelWidth - 4);
                const auto labelLeft = std::clamp(
                    xAtTick(selectionRange_->first) + 8,
                    headerWidth_ + 4,
                    maximumLeft);
                const auto y = RulerHeight + layout->top
                    - verticalScrollBar()->value();
                const QRect labelRect(
                    labelLeft,
                    std::max(RulerHeight + 3, y + 3),
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
    const auto persistentBitSelection = !drawing_ && hasBitRangeSelection();
    if (persistentBitSelection) {
        drawRange(selectedLaneId_, *selectionRange_, false, false);
    }
    if (!drawing_
        && waveEditHoverRange_
        && !waveEditHoverLaneId_.empty()) {
        const auto duplicatesPersistentSelection = persistentBitSelection
            && waveEditHoverLaneId_ == selectedLaneId_
            && *waveEditHoverRange_ == *selectionRange_;
        if (!duplicatesPersistentSelection) {
            drawRange(waveEditHoverLaneId_, *waveEditHoverRange_, false, false);
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
    const auto activeBusBeat = busEditPaletteVisible_
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
        drawRange(drawLaneId_, *waveEditPreviewRange_, false, true);
    } else if (!activeBusBeat && !selectedSegmentId_.empty()) {
        const auto* segment = segmentById(
            selectedSegmentLaneId_,
            selectedSegmentId_);
        if (segment) {
            const auto previewing = waveEditInteraction_ == WaveEditInteraction::ResizeStart
                || waveEditInteraction_ == WaveEditInteraction::ResizeEnd
                || waveEditInteraction_ == WaveEditInteraction::MoveSegment;
            const auto range = previewing && waveEditPreviewRange_
                ? *waveEditPreviewRange_
                : std::pair{segment->start, segment->end};
            drawRange(selectedSegmentLaneId_, range, true, previewing);

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
                const auto previewLabel = previewing
                    ? waveEditCopyDrag_
                        ? tr("Copy · %1 · %2").arg(valueLabel, timingLabel)
                        : tr("%1 · %2").arg(valueLabel, timingLabel)
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
    const Tick visibleEnd)
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
