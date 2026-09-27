#include "wave/widgets.h"
#include "wave/project_io.h"
#include "wave/commands.h"
#include "wave_canvas.h"
#include "ui_controls.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>
#include <QScrollArea>
#include <QComboBox>
#include <QToolButton>
#include <QSignalBlocker>
#include <QTimer>
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace {
void errorText(const QString& text, char* out, std::size_t size)
{
    if (!out || !size) return;
    const auto bytes = text.toUtf8();
    const auto count = std::min(size - 1, static_cast<std::size_t>(bytes.size()));
    std::memcpy(out, bytes.constData(), count); out[count] = 0;
}
class StimulusEditor final : public QWidget {
public:
    StimulusEditor(wave::Project project, QWidget* parent)
        : QWidget(parent), project_(std::move(project))
    {
        wave::ui::initialize();
        setObjectName(QStringLiteral("WaveStimulusEditor"));
        setProperty("wavewidgets.contract", QString::fromLatin1(wave::kStimulusEditorContract));
        setProperty("wavewidgets.dirty", false);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        auto* bar = new QHBoxLayout;
        auto* undo = wave::ui::button(tr("Undo"), this);
        auto* redo = wave::ui::button(tr("Redo"), this);
        auto* zoomIn = wave::ui::button(tr("Zoom in"), this);
        auto* zoomOut = wave::ui::button(tr("Zoom out"), this);
        auto* fit = wave::ui::button(tr("Fit"), this);
        auto* random = wave::ui::toolButton(this);
        random->setObjectName(QStringLiteral("stimulusRandomFill"));
        random->setIcon(wave::ui::icon(wave::ui::Icon::Random, random));
        random->setIconSize(QSize(20, 20));
        random->setFixedSize(32, 32);
        random->setCheckable(true);
        random->setAccessibleName(tr("Random fill"));
        random->setToolTip(tr("Random fill: turn on, then drag across inputs. One value per clock beat; enums use declared members. Ctrl+Z undoes the fill."));
        undo->setObjectName(QStringLiteral("stimulusUndo"));
        redo->setObjectName(QStringLiteral("stimulusRedo"));
        fit->setObjectName(QStringLiteral("stimulusFit"));
        for (auto* button : {undo, redo, zoomIn, zoomOut, fit}) bar->addWidget(button);
        bar->addWidget(random);
        bar->addStretch();
        layout->addLayout(bar);
        canvas_ = new wave::WaveCanvas(this);
        canvas_->setObjectName(QStringLiteral("stimulusCanvas"));
        canvas_->setProperty("wavewidgets.fixedSignals", true);
        // These editors are normally hosted in the full application's toolbars.
        // Give the small embedded surface its own host instead of letting them
        // cover the time ruler inside the canvas viewport.
        const auto paletteHost = [this, layout](QWidget* palette) {
            auto* scroll = new QScrollArea(this);
            scroll->setFrameShape(QFrame::NoFrame);
            scroll->setWidget(palette);
            scroll->setWidgetResizable(true);
            scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            scroll->hide();
            layout->addWidget(scroll);
            return scroll;
        };
        auto* rangeHost = paletteHost(canvas_->rangeEditPaletteWidget());
        const auto showPalette = [](QScrollArea* host, bool visible) {
            host->setFixedHeight(host->widget()->sizeHint().height() + 18);
            host->setVisible(visible);
        };
        connect(canvas_, &wave::WaveCanvas::rangeEditPaletteVisibilityChanged, this,
            [rangeHost, showPalette, random](bool visible) { showPalette(rangeHost, visible && !random->isChecked()); });
        auto* radix = canvas_->busEditPaletteWidget()->findChild<QComboBox*>(QStringLiteral("BusEditRadixCombo"));
        if (radix) radix->removeItem(radix->count() - 1); // display-only text has no HDL value
        canvas_->setDocument(&project_, &project_.scenarios.front(), &history_);
        canvas_->setTool(wave::WaveCanvas::Tool::WaveEdit);
        layout->addWidget(canvas_, 1);
        const auto editHint = tr("Click a bit to toggle it. Double-click a bus or enum to choose a value; double-click the clock to see its cycles.");
        auto* status = wave::ui::text(editHint, this);
        status->setWordWrap(true);
        status->setObjectName(QStringLiteral("stimulusStatus"));
        layout->addWidget(status);
        const auto update = [this, undo, redo] {
            undo->setEnabled(history_.canUndo()); redo->setEnabled(history_.canRedo());
            setProperty("wavewidgets.dirty", history_.stateId() != 0);
        };
        connect(canvas_, &wave::WaveCanvas::modelEdited, this, update);
        connect(canvas_, &wave::WaveCanvas::commandAvailabilityChanged, this, update);
        connect(canvas_, &wave::WaveCanvas::statusMessage, status, &QLabel::setText);
        connect(random, &QToolButton::toggled, this, [this, random, status, rangeHost, editHint](bool enabled) {
            if (!canvas_->commitPendingInlineEdits()) {
                const QSignalBlocker blocker(random);
                random->setChecked(!enabled);
                return;
            }
            status->setText(enabled
                ? tr("Random fill ON · drag a range across inputs; clocks stay read-only. Click the dice again to return to drawing.")
                : editHint);
            if (enabled && canvas_->selectedTimeRange()) canvas_->randomizeSelectedRange();
            canvas_->setTool(enabled ? wave::WaveCanvas::Tool::Selection : wave::WaveCanvas::Tool::WaveEdit);
            rangeHost->hide();
        });
        connect(canvas_, &wave::WaveCanvas::rangeSelectionFinished, this, [this, random] {
            if (random->isChecked()) canvas_->randomizeSelectedRange();
        });
        const auto undoAction = [this, update] { if (canvas_->commitPendingInlineEdits() && history_.undo()) canvas_->refreshModel(); update(); };
        const auto redoAction = [this, update] { if (canvas_->commitPendingInlineEdits() && history_.redo()) canvas_->refreshModel(); update(); };
        connect(undo, &QPushButton::clicked, this, undoAction);
        connect(redo, &QPushButton::clicked, this, redoAction);
        connect(zoomIn, &QPushButton::clicked, canvas_, &wave::WaveCanvas::zoomIn);
        connect(zoomOut, &QPushButton::clicked, canvas_, &wave::WaveCanvas::zoomOut);
        connect(fit, &QPushButton::clicked, canvas_, &wave::WaveCanvas::fitScenario);
        auto* undoKey = new QShortcut(QKeySequence::Undo, this); undoKey->setContext(Qt::WidgetWithChildrenShortcut);
        auto* redoKey = new QShortcut(QKeySequence::Redo, this); redoKey->setContext(Qt::WidgetWithChildrenShortcut);
        connect(undoKey, &QShortcut::activated, this, undoAction);
        connect(redoKey, &QShortcut::activated, this, redoAction);
        update();
        QTimer::singleShot(0, canvas_, &wave::WaveCanvas::showClockCycles);
    }
    ~StimulusEditor() override { canvas_->setDocument(nullptr, nullptr, nullptr); }
    QByteArray snapshot()
    {
        if (!canvas_->commitPendingInlineEdits()) throw std::runtime_error("Finish the current value edit before saving.");
        return wave::serializeProject(project_);
    }
private:
    wave::Project project_;
    wave::CommandStack history_;
    wave::WaveCanvas* canvas_{};
};
}

int wavewidgets_create_stimulus_editor_v1(const char* data, std::size_t size,
    const char* hostQtVersion, QWidget* parent, QWidget** editor, char* error,
    std::size_t errorCapacity) noexcept
{
    if (editor) *editor = nullptr;
    if (!editor || !data || !size || size > 8U * 1024U * 1024U || !hostQtVersion
        || std::strcmp(hostQtVersion, QT_VERSION_STR) != 0) {
        errorText(QStringLiteral("A valid project and a matching Qt runtime are required."), error, errorCapacity); return 1;
    }
    try {
        auto loaded = wave::deserializeProject(QByteArray(data, static_cast<qsizetype>(size)));
        if (!loaded.ok() || loaded.project->scenarios.size() != 1 || loaded.project->scenarios.front().duration <= 0) {
            errorText(loaded.error.isEmpty() ? QStringLiteral("Exactly one nonempty stimulus scenario is required.") : loaded.error, error, errorCapacity); return 2;
        }
        *editor = new StimulusEditor(std::move(*loaded.project), parent);
        errorText({}, error, errorCapacity); return 0;
    } catch (const std::exception& e) { errorText(QString::fromUtf8(e.what()), error, errorCapacity); return 3; }
    catch (...) { errorText(QStringLiteral("Could not create the stimulus editor."), error, errorCapacity); return 4; }
}

int wavewidgets_stimulus_project_v1(QWidget* editor, char* destination,
    std::size_t capacity, std::size_t* required, char* error, std::size_t errorCapacity) noexcept
{
    if (required) *required = 0;
    auto* typed = dynamic_cast<StimulusEditor*>(editor);
    if (!typed || !required) { errorText(QStringLiteral("Invalid stimulus editor."), error, errorCapacity); return 1; }
    try {
        const auto bytes = typed->snapshot();
        *required = static_cast<std::size_t>(bytes.size()) + 1;
        if (!destination && !capacity) { errorText({}, error, errorCapacity); return 0; }
        if (!destination || capacity < *required) { errorText(QStringLiteral("The output buffer is too small."), error, errorCapacity); return 2; }
        std::memcpy(destination, bytes.constData(), bytes.size()); destination[bytes.size()] = 0;
        errorText({}, error, errorCapacity); return 0;
    } catch (const std::exception& e) { errorText(QString::fromUtf8(e.what()), error, errorCapacity); return 3; }
    catch (...) { errorText(QStringLiteral("Could not read the stimulus."), error, errorCapacity); return 4; }
}
