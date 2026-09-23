#include "quick_waveform.h"
#include "ui_controls.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDrag>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

#include <limits>
#include <algorithm>
#include <numeric>

namespace wave {
bool validQuickBitPattern(const QuickBitPattern& pattern)
{
    return pattern.timeBase.isValid() && !pattern.bits.isEmpty() && pattern.bits.size() <= maxQuickPatternBits && pattern.step > 0
        && pattern.step <= std::numeric_limits<Tick>::max()/pattern.bits.size()
        && std::all_of(pattern.bits.begin(),pattern.bits.end(),[](QChar bit) { return bit == '0' || bit == '1'; });
}

std::optional<Tick> quickBitPatternStep(const QuickBitPattern& pattern, const TimeBase& target)
{
    if (!validQuickBitPattern(pattern) || !target.isValid()) return std::nullopt;
    const auto divisor = std::gcd(pattern.timeBase.picosecondsPerTick,target.picosecondsPerTick);
    const auto numerator = pattern.timeBase.picosecondsPerTick/divisor;
    const auto denominator = target.picosecondsPerTick/divisor;
    if (pattern.step%denominator != 0 || pattern.step/denominator > std::numeric_limits<Tick>::max()/numerator)
        return std::nullopt;
    return pattern.step/denominator*numerator;
}

QByteArray encodeQuickBitPattern(const QuickBitPattern& pattern)
{
    if (!validQuickBitPattern(pattern)) return {};
    return QJsonDocument(QJsonObject{{"version",1},{"bits",pattern.bits},
        {"stepTicks",QString::number(pattern.step)},{"useTargetClock",pattern.useTargetClock},
        {"picosecondsPerTick",QString::number(pattern.timeBase.picosecondsPerTick)}}).toJson(QJsonDocument::Compact);
}

std::optional<QuickBitPattern> decodeQuickBitPattern(const QByteArray& bytes)
{
    if (bytes.size() > 2*maxQuickPatternBits) return std::nullopt;
    const auto object = QJsonDocument::fromJson(bytes).object();
    if (object.value("version").toInt() != 1 || !object.value("useTargetClock").isBool()) return std::nullopt;
    bool ok = false;
    bool baseOk = false;
    QuickBitPattern pattern{object.value("bits").toString(),object.value("stepTicks").toString().toLongLong(&ok),
        object.value("useTargetClock").toBool(),{object.value("picosecondsPerTick").toString().toLongLong(&baseOk)}};
    return ok && baseOk && validQuickBitPattern(pattern) ? std::optional{pattern} : std::nullopt;
}

namespace {
class PatternPreview final : public QWidget {
public:
    explicit PatternPreview(QWidget* parent) : QWidget(parent) {
        setObjectName("QuickWaveformPreview");
        setMinimumSize(300,120);
        setAccessibleName(tr("Bit waveform preview"));
    }
    QuickBitPattern pattern;
    bool ready{false};
protected:
    void paintEvent(QPaintEvent*) override {
        const auto theme = waveformTheme(waveformColorScheme(palette()));
        QPainter painter(this);
        painter.fillRect(rect(),theme.canvas);
        painter.setRenderHint(QPainter::Antialiasing);
        if (!validQuickBitPattern(pattern)) return;
        const auto cell = double(width()-24)/pattern.bits.size();
        QPainterPath path;
        auto level = [&](QChar bit) { return bit == '1' ? 30 : 70; };
        path.moveTo(12,level(pattern.bits[0]));
        // Subpixel transitions collapse to an envelope, not thousands of paths.
        if (cell < 1) {
            painter.fillRect(QRect(12,30,width()-24,40),theme.selection);
        } else {
            for (qsizetype i=0;i<pattern.bits.size();++i) {
                path.lineTo(12+i*cell,level(pattern.bits[i]));
                path.lineTo(12+(i+1)*cell,level(pattern.bits[i]));
            }
            painter.strokePath(path,QPen(theme.bit,2));
        }
        painter.setPen(theme.text);
        painter.drawText(QRect(12,84,width()-24,28),Qt::AlignCenter,
            ready ? tr("Drag onto Bit · replaces target range") : tr("%1 bits · preview only").arg(pattern.bits.size()));
    }
    void mousePressEvent(QMouseEvent* event) override { press_ = event->position().toPoint(); }
    void mouseMoveEvent(QMouseEvent* event) override {
        if (!ready || !(event->buttons() & Qt::LeftButton)
            || (event->position().toPoint()-press_).manhattanLength() < QApplication::startDragDistance()) return;
        QDrag drag(this);
        auto* mime = new QMimeData;
        mime->setData(bitPatternMime,encodeQuickBitPattern(pattern));
        drag.setMimeData(mime);
        drag.setPixmap(grab());
        drag.setHotSpot(press_);
        drag.exec(Qt::CopyAction);
    }
private:
    QPoint press_;
};
}

QWidget* createQuickWaveformComposer(const TimeBase& timeBase, Tick initialStep, bool boundClock, QWidget* parent)
{
    auto* dialog = new ui::Dialog(parent);
    dialog->setObjectName("QuickWaveformComposer");
    dialog->setWindowTitle(QObject::tr("Quick waveform · Bit"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setModal(false);
    auto* layout = new QVBoxLayout(dialog);
    auto* form = new ui::FormLayout;
    auto* sequence = ui::lineEdit("01010101111010",dialog);
    sequence->setObjectName("QuickWaveformSequence");
    sequence->setMaxLength(maxQuickPatternBits);
    sequence->setAccessibleName(QObject::tr("Binary waveform sequence"));
    form->addRow(QObject::tr("Bit sequence"),sequence);
    auto* step = ui::lineEdit(QString::fromStdString(formatTick(initialStep,timeBase)),dialog);
    step->setObjectName("QuickWaveformStep");
    step->setAccessibleName(QObject::tr("Duration per bit"));
    form->addRow(QObject::tr("Per bit"),step);
    auto* clock = ui::checkBox(QObject::tr("Use target clock period when bound"),dialog);
    clock->setObjectName("QuickWaveformTargetClock");
    clock->setChecked(true);
    step->setToolTip(boundClock ? QObject::tr("Selected clock period; editing this field switches to a manual duration")
        : QObject::tr("Async fallback duration; a bound drop target uses its clock period unless unchecked"));
    form->addRow(clock);
    layout->addLayout(form);
    auto* preview = new PatternPreview(dialog);
    layout->addWidget(preview);
    auto* error = ui::text(dialog);
    error->setObjectName("QuickWaveformError");
    error->setProperty("waveState","error");
    error->setWordWrap(true);
    layout->addWidget(error);
    auto* buttons = ui::buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Close,dialog);
    auto* confirm = buttons->button(QDialogButtonBox::Ok);
    confirm->setText(QObject::tr("Confirm fragment"));
    confirm->setObjectName("QuickWaveformConfirm");
    layout->addWidget(buttons);
    const auto update = [=] {
        static const QRegularExpression duration(QStringLiteral("^([0-9]+(?:\\.[0-9]+)?)\\s*(ps|ns|us|ms|tick)$"));
        const auto match = duration.match(step->text().trimmed());
        std::optional<Tick> parsed;
        if (match.hasMatch()) {
            const auto unit = match.captured(2);
            if (unit == "tick") {
                bool ok = false; const auto ticks = match.captured(1).toLongLong(&ok);
                if (ok) parsed = ticks;
            } else {
                parsed = toTicks(match.captured(1).toStdString(), unit == "ps" ? TimeUnit::Picosecond
                    : unit == "us" ? TimeUnit::Microsecond : unit == "ms" ? TimeUnit::Millisecond : TimeUnit::Nanosecond,timeBase);
            }
        }
        preview->pattern = {sequence->text().trimmed(),parsed.value_or(0),clock->isChecked(),timeBase};
        preview->ready = false;
        preview->setProperty("fragmentReady",false);
        preview->setCursor(Qt::ArrowCursor);
        const bool valid = validQuickBitPattern(preview->pattern);
        error->setText(valid ? QString{} : QObject::tr("Use only 0/1 (up to %1 bits), and a positive exact-tick duration.").arg(maxQuickPatternBits));
        error->setVisible(!valid);
        confirm->setEnabled(valid);
        preview->update();
    };
    QObject::connect(sequence,&QLineEdit::textChanged,dialog,update);
    QObject::connect(step,&QLineEdit::textChanged,dialog,update);
    QObject::connect(step,&QLineEdit::textEdited,clock,[clock] { clock->setChecked(false); });
    QObject::connect(clock,&QCheckBox::toggled,dialog,update);
    QObject::connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::close);
    QObject::connect(buttons,&QDialogButtonBox::accepted,dialog,[=] {
        if (!validQuickBitPattern(preview->pattern)) return;
        preview->ready = true;
        preview->setProperty("fragmentReady",true);
        preview->setCursor(Qt::OpenHandCursor);
        preview->update();
    });
    update();
    dialog->resize(450,300);
    dialog->show();
    return dialog;
}
} // namespace wave
