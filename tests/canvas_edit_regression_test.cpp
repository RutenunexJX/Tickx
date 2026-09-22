#include "wave_canvas.h"
#include "waveform_theme.h"
#include "ui_controls.h"
#include "wave/generation.h"
#include "wave/export.h"
#include "wave/project_io.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QLineEdit>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTest>
#include <QToolButton>

#include <algorithm>
#include <cmath>

namespace {

void applyTheme(const wave::WaveformColorScheme scheme)
{
    const auto theme = wave::waveformTheme(scheme);
    auto palette = QApplication::palette();
    palette.setColor(QPalette::Window, theme.application);
    palette.setColor(QPalette::WindowText, theme.text);
    palette.setColor(QPalette::Base, theme.raised);
    palette.setColor(QPalette::Text, theme.text);
    palette.setColor(QPalette::Button, theme.panel);
    palette.setColor(QPalette::ButtonText, theme.text);
    QApplication::setPalette(palette);
    wave::ui::applyTheme(scheme);
}

wave::Project makeProject()
{
    wave::Project project;
    project.id = "canvas-regression";
    project.name = "Canvas regression";
    project.timeBase.picosecondsPerTick = 1'000;
    wave::ClockDomain clock;
    clock.id = "clk";
    clock.name = "clk";
    clock.period = 10;
    project.clockDomains.push_back(clock);
    wave::Scenario scenario;
    scenario.id = "scenario";
    scenario.name = "Scenario";
    scenario.duration = 200;
    for (const auto kind : {wave::LaneKind::Clock, wave::LaneKind::Bit, wave::LaneKind::Bus}) {
        wave::Lane lane;
        lane.id = lane.name = std::string(wave::toString(kind));
        lane.kind = kind;
        lane.clockDomainId = "clk";
        lane.height = 56;
        lane.width = kind == wave::LaneKind::Bus ? 8 : 1;
        scenario.lanes.push_back(lane);
    }
    project.scenarios.push_back(scenario);
    return project;
}

struct Fixture {
    wave::Project project{makeProject()};
    wave::CommandStack commands;
    wave::WaveCanvas canvas;
    double scale{1.0};

    void show()
    {
        canvas.resize(1'400, 700);
        canvas.setDocument(&project, &project.scenarios.front(), &commands);
        canvas.setTool(wave::WaveCanvas::Tool::WaveEdit);
        canvas.show();
        QCoreApplication::processEvents();
        canvas.fitScenario();
        scale = double(canvas.viewport()->width() - canvas.signalHeaderWidth())
            / double(project.scenarios.front().duration);
        QCoreApplication::processEvents();
    }

    QPoint point(const double tick, const int lane = 1) const
    {
        return {canvas.signalHeaderWidth() + int(std::llround(tick * scale))
                    - canvas.horizontalScrollBar()->value(),
            40 + lane * 56 + 28};
    }

    wave::Lane& lane(const int index = 1) { return project.scenarios.front().lanes.at(index); }
};

void mouse(wave::WaveCanvas& canvas, const QEvent::Type type, const QPoint point)
{
    const auto release = type == QEvent::MouseButtonRelease;
    QMouseEvent event(type, QPointF(point), QPointF(canvas.viewport()->mapToGlobal(point)),
        type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
        release ? Qt::NoButton : Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(canvas.viewport(), &event);
    QCoreApplication::processEvents();
}

void click(wave::WaveCanvas& canvas, const QPoint point)
{
    mouse(canvas, QEvent::MouseButtonPress, point);
    mouse(canvas, QEvent::MouseButtonRelease, point);
}

void editBus(wave::WaveCanvas& canvas, const QPoint point)
{
    click(canvas, point);
    mouse(canvas, QEvent::MouseButtonDblClick, point);
    mouse(canvas, QEvent::MouseButtonRelease, point);
}

QColor pixel(const QImage& image, const QPoint point)
{
    return image.pixelColor(int(point.x() * image.devicePixelRatio()),
        int(point.y() * image.devicePixelRatio()));
}

void savePreview(wave::WaveCanvas& canvas, const QString& name)
{
    const auto directory = qEnvironmentVariable("WAVE_CANVAS_PREVIEW_DIR");
    if (directory.isEmpty()) return;
    QVERIFY(QDir().mkpath(directory));
    QVERIFY(canvas.viewport()->grab().save(directory + QLatin1Char('/') + name + ".png"));
}

} // namespace

class CanvasEditRegressionTest final : public QObject {
    Q_OBJECT
private slots:
    void init()
    {
        applyTheme(wave::WaveformColorScheme::Light);
        QApplication::setFont(QFont("Segoe UI", 9));
    }

    void singleBeatClick_data()
    {
        QTest::addColumn<int>("tick");
        QTest::addColumn<int>("jitter");
        QTest::addColumn<int>("phase");
        QTest::addColumn<int>("end");
        QTest::newRow("first-half") << 12 << 0 << 0 << 200;
        QTest::newRow("second-half") << 17 << 0 << 0 << 200;
        QTest::newRow("last-tick") << 19 << 0 << 0 << 200;
        QTest::newRow("boundary-jitter") << 19 << 7 << 0 << 200;
        QTest::newRow("reverse-jitter") << 20 << -3 << 0 << 200;
        QTest::newRow("phase") << 21 << 0 << 3 << 200;
        QTest::newRow("partial-final-beat") << 202 << 0 << 0 << 203;
    }

    void singleBeatClick()
    {
        QFETCH(int, tick);
        QFETCH(int, jitter);
        QFETCH(int, phase);
        QFETCH(int, end);
        Fixture f;
        f.project.clockDomains.front().phase = phase;
        f.project.scenarios.front().duration = end;
        f.show();
        const auto start = phase + ((tick - phase) / 10) * 10;
        const auto finish = std::min(start + 10, end);
        const auto press = f.point(tick);
        mouse(f.canvas, QEvent::MouseButtonPress, press);
        mouse(f.canvas, QEvent::MouseMove, press + QPoint(jitter, 0));
        QCOMPARE(f.canvas.selectedTimeRange(), (std::optional{std::pair<wave::Tick, wave::Tick>{start, finish}}));
        mouse(f.canvas, QEvent::MouseButtonRelease, press + QPoint(jitter, 0));
        QCOMPARE(f.commands.size(), std::size_t{1});
        QCOMPARE(f.lane().segments.size(), std::size_t{1});
        QCOMPARE(f.lane().segments.front().start, start);
        QCOMPARE(f.lane().segments.front().end, finish);
        QCOMPARE(f.lane().segments.front().value, std::string("1"));
        QCOMPARE(f.canvas.selectedTimeRange(), (std::optional{std::pair<wave::Tick, wave::Tick>{start, finish}}));
        QCOMPARE(f.canvas.hoveredBitBeatRange(), f.canvas.selectedTimeRange());
        QVERIFY(f.canvas.cursorTick() >= start && f.canvas.cursorTick() < finish);
        QVERIFY(f.commands.undo());
        QVERIFY(f.lane().segments.empty());
        QVERIFY(f.commands.redo());
        QCOMPARE(f.lane().segments.front().start, start);
        QCOMPARE(f.lane().segments.front().end, finish);
    }

    void genuineDrag()
    {
        for (const bool reverse : {false, true}) {
            Fixture f;
            f.show();
            const auto from = f.point(reverse ? 37 : 17);
            const auto to = f.point(reverse ? 17 : 37);
            mouse(f.canvas, QEvent::MouseButtonPress, from);
            mouse(f.canvas, QEvent::MouseMove, to);
            mouse(f.canvas, QEvent::MouseButtonRelease, to);
            QCOMPARE(f.commands.size(), std::size_t{1});
            QCOMPARE(f.lane().segments.size(), std::size_t{1});
            QCOMPARE(f.lane().segments.front().start, wave::Tick{10});
            QCOMPARE(f.lane().segments.front().end, wave::Tick{40});
        }
    }

    void asynchronousClickKeepsPreview()
    {
        Fixture f;
        f.show();
        f.canvas.setAsynchronousEditing(true);
        const auto press = f.point(17);
        mouse(f.canvas, QEvent::MouseButtonPress, press);
        const auto preview = f.canvas.selectedTimeRange();
        QVERIFY(preview);
        mouse(f.canvas, QEvent::MouseMove, press + QPoint(7, 0));
        mouse(f.canvas, QEvent::MouseButtonRelease, press + QPoint(7, 0));
        QCOMPARE(f.lane().segments.size(), std::size_t{1});
        QCOMPARE(f.lane().segments.front().start, preview->first);
        QCOMPARE(f.lane().segments.front().end, preview->second);
        QCOMPARE(f.canvas.hoveredBitBeatRange(), preview);
        QCOMPARE(f.canvas.cursorTick(), preview->first);
    }

    void clockGrid_data()
    {
        QTest::addColumn<int>("phase");
        QTest::addColumn<bool>("falling");
        QTest::addColumn<bool>("zoomed");
        QTest::newRow("rising") << 0 << false << false;
        QTest::newRow("phase") << 3 << false << false;
        QTest::newRow("falling") << 3 << true << false;
        QTest::newRow("zoom-scroll") << 3 << true << true;
    }

    void clockGrid()
    {
        QFETCH(int, phase);
        QFETCH(bool, falling);
        QFETCH(bool, zoomed);
        Fixture f;
        auto& clock = f.project.clockDomains.front();
        clock.period = 12;
        clock.phase = phase;
        clock.activeEdge = falling ? wave::ClockEdge::Falling : wave::ClockEdge::Rising;
        f.show();
        f.canvas.selectLaneHeaders({"bit"}, "bit");
        if (zoomed) {
            f.canvas.zoomIn();
            f.scale *= 1.25;
            f.canvas.horizontalScrollBar()->setValue(41);
        }
        const auto image = f.canvas.viewport()->grab().toImage();
        const auto background = wave::waveformTheme(wave::WaveformColorScheme::Light).canvas;
        const auto anchor = phase + (falling ? 6 : 0);
        const auto y = f.canvas.viewport()->height() - 30;
        for (const int cycle : {3, 5, 8}) {
            const auto edge = f.point(anchor + 12 * cycle).x();
            QVERIFY(pixel(image, {edge, y}) != background || pixel(image, {edge - 1, y}) != background);
            QCOMPARE(pixel(image, {f.point(anchor + 12 * cycle + 4).x(), y}), background);
        }
    }

    void themeSurfaces()
    {
        for (const auto scheme : {wave::WaveformColorScheme::Light, wave::WaveformColorScheme::Dark}) {
            const auto theme = wave::waveformTheme(scheme);
            applyTheme(scheme);
            Fixture f;
            f.lane(0).segments = {{"gated", 20, 40, "gated", {}}, {"disabled", 60, 80, "disabled", {}}};
            f.show();
            f.canvas.selectLaneHeaders({"bit"}, "bit");
            const auto image = f.canvas.viewport()->grab().toImage();
            QCOMPARE(pixel(image, {3, 40 + 56 + 5}), theme.selection);
            QCOMPARE(pixel(image, {f.point(33).x(), 40 + 3 * 56 + 5}), theme.panel);
            QCOMPARE(pixel(image, {f.point(25).x(), 44}), theme.warningSurface);
            QCOMPARE(pixel(image, {f.point(65).x(), 44}), theme.errorSurface);
            savePreview(f.canvas, scheme == wave::WaveformColorScheme::Light ? "light" : "dark");
        }
    }

    void textBusRoundTrip()
    {
        Fixture f;
        f.show();
        editBus(f.canvas, f.point(17, 2));
        auto* edit = f.canvas.findChild<QLineEdit*>("BusPresetValueEdit");
        auto* mode = f.canvas.findChild<QComboBox*>("BusEditRadixCombo");
        QVERIFY(edit && edit->isVisible());
        QVERIFY(mode);
        const auto textMode = mode->findText("Text");
        QVERIFY(textMode >= 0);
        mode->setCurrentIndex(textMode);
        const auto label = QString::fromUtf8("IDLE, DATA *4; 总线 \"ready\"");
        edit->setText(label);
        edit->setModified(true);
        QVERIFY(f.canvas.busEditActionState(wave::WaveCanvas::BusEditAction::ApplyDraft).valid);
        QTest::keyClick(edit, Qt::Key_Return, Qt::ControlModifier);
        QCOMPARE(f.commands.size(), std::size_t{1});
        QCOMPARE(f.lane(2).segments.size(), std::size_t{1});
        QCOMPARE(f.lane(2).segments.front().value, "text:" + label.toStdString());
        QCOMPARE(f.lane(2).segments.front().start, wave::Tick{10});
        QCOMPARE(f.lane(2).segments.front().end, wave::Tick{20});
        QCOMPARE(edit->text(), label);
        QVERIFY(!wave::laneValueBits(f.lane(2), f.lane(2).segments.front().value));
        const auto serialized = wave::serializeProject(f.project);
        const auto loaded = wave::deserializeProject(serialized);
        QVERIFY2(loaded.ok(), qPrintable(loaded.error));
        QCOMPARE(loaded.project->scenarios.front().lanes.at(2).segments, f.lane(2).segments);
        const auto diagram = QJsonDocument::fromJson(wave::generateWaveDromJson(
            f.project, f.project.scenarios.front(), {}));
        const auto data = diagram.object().value("signal").toArray().at(2).toObject().value("data").toArray();
        QVERIFY(data.contains(label));
        const auto svg = wave::renderWaveformSvg(f.project, f.project.scenarios.front(), {});
        QVERIFY(!svg.isEmpty());
        QVERIFY(!svg.contains("text:IDLE"));
        QVERIFY(f.commands.undo());
        QVERIFY(f.lane(2).segments.empty());
        QVERIFY(f.commands.redo());
        f.canvas.refreshModel();
        QCOMPARE(f.lane(2).segments.front().value, "text:" + label.toStdString());
        // Plain numeric errors remain errors; text is never a made-up simulation value.
        QVERIFY(!wave::validateLaneValue(f.lane(2), "0x1ff").valid);
        QVERIFY(!wave::validateLaneValue(f.lane(2), "IDLE").valid);
        QVERIFY(!wave::validateLaneValue(f.lane(2), "text:").valid);
        const auto plan = wave::buildGenerationPlan(f.project, f.project.scenarios.front());
        QVERIFY(plan.ok());
        QVERIFY(!wave::generateSystemVerilog(*plan.plan).ok());
        QVERIFY(!wave::generateCocotb(*plan.plan).ok());
        // A literal numeric-looking label is still text, including after reopening.
        f.canvas.dismissInlineValueEditor();
        editBus(f.canvas, f.point(17, 2));
        QCOMPARE(mode->currentIndex(), textMode);
        QCOMPARE(edit->text(), label);
        auto* scope = f.canvas.findChild<QToolButton*>("BusEditScopeButton");
        QVERIFY(scope);
        QCOMPARE(scope->text(), QString("Segment"));
        scope->click();
        QCOMPARE(scope->text(), QString("Beat"));
        edit->setText("0x1ff");
        edit->setModified(true);
        QTest::keyClick(edit, Qt::Key_Tab);
        QCOMPARE(f.lane(2).segments.front().value, std::string("text:0x1ff"));
        QCOMPARE(mode->currentIndex(), textMode);
        edit->setText("DATA");
        edit->setModified(true);
        QTest::keyClick(edit, Qt::Key_Return);
        QCOMPARE(f.lane(2).segments.size(), std::size_t{2});
        QCOMPARE(f.lane(2).segments.back().value, std::string("text:DATA"));
        savePreview(f.canvas, "bus-text");
    }
};

QTEST_MAIN(CanvasEditRegressionTest)
#include "canvas_edit_regression_test.moc"
