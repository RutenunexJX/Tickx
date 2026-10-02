#include "signal_style_preview.h"
#include "signal_style.h"
#include "ui_controls.h"
#include "wave_canvas.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

namespace wave {
namespace {
Project previewProject()
{
    Project project;
    project.id = "style-preview";
    project.name = "Style preview";
    project.timeBase = {1000};
    ClockDomain clock;
    clock.id = clock.name = "clk";
    clock.period = 40;
    project.clockDomains.push_back(clock);
    Scenario scenario;
    scenario.id = "preview";
    scenario.name = "Preview";
    scenario.duration = 240;
    const auto addLane = [&](const std::string& id, const std::string& name, LaneKind kind,
                             std::vector<Segment> segments) -> Lane& {
        Lane lane;
        lane.id = id; lane.name = name; lane.kind = kind;
        lane.height = 44;
        lane.width = kind == LaneKind::Bus ? 8 : 1;
        lane.clockDomainId = "clk";
        lane.color = kind == LaneKind::Clock ? "#bb8926" : kind == LaneKind::Bit ? "#3782bc" : "#4e9870";
        lane.segments = std::move(segments);
        scenario.lanes.push_back(std::move(lane));
        return scenario.lanes.back();
    };
    addLane("clock", "Clock", LaneKind::Clock,
        {{"gate",120,180,"gated",{}},{"disable",180,240,"disabled",{}}});
    addLane("bit", "Bit", LaneKind::Bit,
        {{"low",0,48,"0",{}},{"high",48,120,"1",{}},
         {"low2",120,192,"0",{}},{"high2",192,240,"1",{}}});
    addLane("bus", "Bus [7:0]", LaneKind::Bus,
        {{"a",0,80,"0x35",{}},{"b",80,160,"0xA7",{}},{"c",160,240,"0x35",{}}});
    auto& dontCare = addLane("dont-care", "Don't care", LaneKind::Bus, {{"dc",0,240,"0bXXXXXXXX",{}}});
    dontCare.segments.front().extensions["waveWorkbench.busPreset"] = "\"dont-care\"";
    auto& reserved = addLane("reserved", "Reserved", LaneKind::Bus, {{"reserved",0,240,"0x00",{}}});
    reserved.segments.front().extensions["waveWorkbench.busPreset"] = "\"reserved\"";
    addLane("unknown", "Bus X", LaneKind::Bus, {{"x",0,240,"0bXXXXXXXX",{}}});
    addLane("high-z", "Bus Z", LaneKind::Bus, {{"z",0,240,"0bZZZZZZZZ",{}}});
    addLane("bit-x-z", "Bit X / Z", LaneKind::Bit, {{"x",0,120,"X",{}},{"z",120,240,"Z",{}}});
    project.scenarios.push_back(std::move(scenario));
    return project;
}

class StylePreviewPage final : public QWidget {
public:
    StylePreviewPage(SignalStyleEditor* editor, QWidget* parent)
        : QWidget(parent), editor_(editor), project_(previewProject())
    {
        setObjectName("SignalStylePreviewPage");
        auto* layout = new QVBoxLayout(this);
        auto* controls = new QHBoxLayout;
        controls->addWidget(ui::text(tr("Style"), this));
        preset_ = ui::comboBox(this);
        preset_->setObjectName("SignalStylePreviewPreset");
        auto* sourcePreset = editor->findChild<QComboBox*>("SignalStylePreset");
        for (int i = 0; i < sourcePreset->count(); ++i)
            preset_->addItem(sourcePreset->itemText(i), sourcePreset->itemData(i));
        controls->addWidget(preset_, 1);
        controls->addSpacing(12);
        controls->addWidget(ui::text(tr("Preview theme"), this));
        auto* scheme = ui::comboBox(this);
        scheme->setObjectName("SignalStylePreviewTheme");
        scheme->addItem(tr("Light")); scheme->addItem(tr("Dark"));
        scheme->setCurrentIndex(waveformColorScheme(palette()) == WaveformColorScheme::Dark ? 1 : 0);
        controls->addWidget(scheme);
        layout->addLayout(controls);
        status_ = ui::text({}, this);
        status_->setObjectName("SignalStylePreviewStatus");
        status_->setWordWrap(true);
        layout->addWidget(status_);
        canvas_ = new WaveCanvas(this);
        canvas_->setObjectName("SignalStylePreviewCanvas");
        canvas_->setProperty("wavewidgets.fixedSignals", true);
        canvas_->setFocusPolicy(Qt::NoFocus);
        canvas_->setSignalHeaderWidth(200);
        canvas_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        canvas_->setDocument(&project_, &project_.scenarios.front(), nullptr);
        // Keep the real renderer and scrollbars, without any waveform-editing input.
        canvas_->viewport()->setEnabled(false);
        canvas_->viewport()->installEventFilter(this);
        canvas_->rangeEditPaletteWidget()->hide();
        canvas_->setMinimumSize(480, 96);
        layout->addWidget(canvas_, 1);
        auto* note = ui::text(tr("Normal clocks, bits and buses share one edge style. Don't care, reserved, gated, disabled, X and Z keep their own shapes."), this);
        note->setObjectName("SignalStylePreviewNote");
        note->setWordWrap(true);
        layout->addWidget(note);
        connect(preset_, &QComboBox::currentIndexChanged, this, [this, sourcePreset] {
            sourcePreset->setCurrentIndex(sourcePreset->findData(preset_->currentData()));
        });
        const auto update = [this] { refresh(); };
        for (auto* combo : editor->findChildren<QComboBox*>())
            connect(combo, &QComboBox::currentIndexChanged, this, update);
        connect(editor->findChild<QDoubleSpinBox*>("SignalStyleStroke"), &QDoubleSpinBox::valueChanged, this, update);
        connect(editor->findChild<QLineEdit*>("SignalStyleFill"), &QLineEdit::textChanged, this, update);
        const auto setScheme = [this, scheme] {
            auto palette = canvas_->palette();
            palette.setColor(QPalette::Window, scheme->currentIndex() == 1 ? QColor("#18202a") : QColor("#ffffff"));
            canvas_->setPalette(palette);
            canvas_->viewport()->update();
        };
        connect(scheme, &QComboBox::currentIndexChanged, this, setScheme);
        setScheme();
        refresh();
    }

    ~StylePreviewPage() override
    {
        // The canvas must release model references before the preview project dies.
        canvas_->viewport()->removeEventFilter(this);
        delete canvas_;
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == canvas_->viewport() && (event->type() == QEvent::Resize || event->type() == QEvent::Show))
            QTimer::singleShot(0, this, [this] { canvas_->fitScenario(); });
        return QWidget::eventFilter(watched, event);
    }

private:
    void refresh()
    {
        if (!editor_) return;
        const QSignalBlocker block(preset_);
        preset_->setCurrentIndex(preset_->findData(editor_->settings().preset));
        const auto valid = editor_->valid();
        status_->setText(valid
            ? tr("Sample waveforms. Choose OK to apply the style to your project.")
            : tr("Correct the color or line width in Settings to update the preview."));
        status_->setProperty("waveState", valid ? "" : "error");
        if (!valid) return;
        storeSignalStyleSettings(project_.extensions, editor_->settings());
        canvas_->refreshModel();
        canvas_->fitScenario();
    }

    QPointer<SignalStyleEditor> editor_;
    Project project_;
    WaveCanvas* canvas_;
    QComboBox* preset_;
    QLabel* status_;
};
}

QWidget* createSignalStylePreviewPage(SignalStyleEditor* editor, QWidget* parent)
{
    return new StylePreviewPage(editor, parent);
}
}
