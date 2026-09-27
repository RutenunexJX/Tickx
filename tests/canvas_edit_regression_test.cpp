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
#include <QSignalSpy>
#include <QTest>
#include <QToolButton>

#include <algorithm>
#include <cmath>
#include <stdexcept>

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

void hover(wave::WaveCanvas& canvas, const QPoint point)
{
    QMouseEvent event(QEvent::MouseMove, QPointF(point),
        QPointF(canvas.viewport()->mapToGlobal(point)), Qt::NoButton, Qt::NoButton,
        Qt::NoModifier);
    QCoreApplication::sendEvent(canvas.viewport(), &event);
    QCoreApplication::processEvents();
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
    void randomFillKeepsClocksBoundsAndUndo()
    {
        Fixture f;
        f.project.clockDomains.front().phase = 3;
        f.lane(1).segments = {{"unknown-bit", 0, 200, "X", {}}};
        f.lane(2).width = 64;
        f.lane(2).isSigned = true;
        f.lane(2).segments = {{"unknown-bus", 0, 200, "0bx", {}}};
        wave::Lane mode;
        mode.id = mode.name = "mode";
        mode.kind = wave::LaneKind::Enum;
        mode.width = 3;
        mode.clockDomainId = "clk";
        mode.enumMap = {{"IDLE", "0"}, {"ACTIVE", "3"}, {"DONE", "5"}};
        mode.segments = {{"unknown-enum", 0, 200, "0bx", {}}};
        f.project.scenarios.front().lanes.push_back(mode);
        f.canvas.setProperty("wavewidgets.fixedSignals", true);
        f.show();
        f.canvas.setTool(wave::WaveCanvas::Tool::Selection);
        mouse(f.canvas, QEvent::MouseButtonPress, f.point(13, 0));
        mouse(f.canvas, QEvent::MouseMove, f.point(83, 3));
        mouse(f.canvas, QEvent::MouseButtonRelease, f.point(83, 3));
        const auto range = f.canvas.selectedTimeRange();
        QVERIFY(range);
        const auto before = f.project.scenarios.front().lanes;
        QSignalSpy status(&f.canvas, &wave::WaveCanvas::statusMessage);
        const auto randomized = f.canvas.randomizeSelectedRange();
        QVERIFY2(randomized, status.isEmpty() ? "No random-fill diagnostic" : qPrintable(status.last().first().toString()));
        QCOMPARE(f.commands.size(), std::size_t{1});
        QCOMPARE(f.lane(0), before[0]);
        for (int index = 1; index < 4; ++index) {
            const auto& lane = f.lane(index);
            std::size_t filled = 0;
            for (const auto& segment : lane.segments) {
                if (segment.end <= range->first || segment.start >= range->second) {
                    QCOMPARE(segment.value, lane.kind == wave::LaneKind::Bit ? std::string("X") : std::string("0bx"));
                    continue;
                }
                QVERIFY(segment.start >= range->first && segment.end <= range->second);
                QCOMPARE((segment.start - 3) % 10, wave::Tick{0});
                if (lane.kind == wave::LaneKind::Enum) QVERIFY(lane.enumMap.contains(segment.value));
                else if (lane.kind == wave::LaneKind::Bit) QVERIFY(segment.value == "0" || segment.value == "1");
                else {
                    const auto bits = wave::laneValueBits(lane, segment.value);
                    QVERIFY(bits);
                    QCOMPARE(bits->size(), std::size_t{64});
                    QVERIFY(bits->find_first_not_of("01") == std::string::npos);
                }
                ++filled;
            }
            QVERIFY(filled > 0);
        }
        const auto after = f.project.scenarios.front().lanes;
        QVERIFY(f.commands.undo());
        QCOMPARE(f.project.scenarios.front().lanes, before);
        QVERIFY(f.commands.redo());
        QCOMPARE(f.project.scenarios.front().lanes, after);
        f.canvas.refreshModel();
        mouse(f.canvas, QEvent::MouseButtonPress, f.point(3, 0));
        mouse(f.canvas, QEvent::MouseButtonRelease, f.point(43, 0));
        QVERIFY(!f.canvas.randomizeSelectedRange());
        QCOMPARE(f.project.scenarios.front().lanes, after);
        QCOMPARE(f.commands.size(), std::size_t{1});
        f.project.scenarios.front().duration = 100000;
        f.canvas.refreshModel();
        f.canvas.selectLaneHeaders({"bus"}, "bus");
        f.canvas.selectEntireTimeline();
        QVERIFY(!f.canvas.randomizeSelectedRange()); // no partial edit on an oversized fill
        QCOMPARE(f.project.scenarios.front().lanes, after);
        QCOMPARE(f.commands.size(), std::size_t{1});
    }

    void readonlyClockRemainsVisible()
    {
        Fixture f;
        f.project.scenarios.front().duration = 1'000'000;
        f.canvas.setProperty("wavewidgets.fixedSignals", true);
        f.show();
        f.canvas.showClockCycles();
        QCoreApplication::processEvents();
        QVERIFY(f.canvas.visibleTimeSpan() <= 121);
        const auto clock = f.lane(0);
        const auto image = f.canvas.viewport()->grab().toImage();
        const auto scale = double(f.canvas.viewport()->width() - f.canvas.signalHeaderWidth())
            / double(f.canvas.visibleTimeSpan());
        const auto x = f.canvas.signalHeaderWidth() + int(2 * scale);
        // Actual high and low plateaus differ from their row background.
        QVERIFY(pixel(image, QPoint(x, 52)) != pixel(image, QPoint(x, 68)));
        f.canvas.fitScenario();
        QCoreApplication::processEvents();
        savePreview(f.canvas, "readonly-clock-overview");
        mouse(f.canvas, QEvent::MouseButtonDblClick,
            QPoint(f.canvas.signalHeaderWidth() + 50, 68));
        QCoreApplication::processEvents();
        QVERIFY(f.canvas.visibleTimeSpan() <= 121);
        QCOMPARE(f.lane(0), clock);
        QVERIFY(!f.commands.canUndo());
        savePreview(f.canvas, "readonly-clock-cycles");
    }

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

    void toggleBackRemovesVanishedEdgeAndRelation()
    {
        Fixture f;
        auto& scenario = f.project.scenarios.front();
        wave::setSegmentRange(f.lane(), 140, 150, "1", "other-pulse");
        wave::setSegmentRange(f.lane(2), 180, 190, "0x2a", "bus-target");
        wave::synchronizeLaneEventsFromSegments(scenario, "bit");
        wave::synchronizeLaneEventsFromSegments(scenario, "bus");
        const auto eventId = [&scenario](const std::string& lane, wave::Tick tick) {
            const auto it = std::find_if(scenario.events.begin(), scenario.events.end(),
                [&](const wave::Event& event) { return event.laneId == lane && event.tick == tick; });
            return it == scenario.events.end() ? std::string{} : it->id;
        };
        const auto otherId = eventId("bit", 140);
        const auto targetId = eventId("bus", 180);
        QVERIFY(!otherId.empty()); QVERIFY(!targetId.empty());
        f.show();
        click(f.canvas, f.point(15));
        const auto sourceId = eventId("bit", 10);
        QVERIFY(!sourceId.empty());
        wave::Relation relation;
        relation.id = "removed-edge-relation";
        relation.sourceEventId = sourceId;
        relation.targetEventId = targetId;
        relation.maximumDelay = 200;
        scenario.relations.push_back(relation);
        relation.id = "other-edge-relation";
        relation.sourceEventId = otherId;
        scenario.relations.push_back(relation);
        f.canvas.refreshModel();
        const auto before = scenario;

        click(f.canvas, f.point(15));
        QVERIFY2(!wave::findEvent(scenario, sourceId), "Flat implicit-0 to explicit-0 retained an edge event");
        QVERIFY(!wave::findRelation(scenario, "removed-edge-relation"));
        QVERIFY(wave::findRelation(scenario, "other-edge-relation"));
        QCOMPARE(eventId("bit", 140), otherId);
        QCOMPARE(eventId("bus", 180), targetId);
        QVERIFY(eventId("bit", 10).empty());
        f.canvas.viewport()->grab();
        QCOMPARE(f.canvas.property("wavewidgets.renderedEventCount").toULongLong(), qulonglong(2));
        const auto after = scenario;
        QVERIFY(f.commands.undo()); QCOMPARE(scenario, before);
        QVERIFY(f.commands.redo()); QCOMPARE(scenario, after);
        const auto loaded = wave::deserializeProject(wave::serializeProject(f.project));
        QVERIFY(loaded.ok());
        QCOMPARE(loaded.project->scenarios.front(), after);
        savePreview(f.canvas, QStringLiteral("bit-toggle-no-phantom-edge"));
    }

    void bitEventSynchronizationPreservesRealEdgesAndExplicitActions()
    {
        Fixture f;
        auto& scenario = f.project.scenarios.front();
        wave::setSegmentRange(f.lane(), 0, 10, "0", "initial");
        wave::setSegmentRange(f.lane(), 20, 30, "0", "flat");
        wave::setSegmentRange(f.lane(), 40, 50, "1", "high");
        wave::setSegmentRange(f.lane(), 50, 60, "0", "falling");
        wave::setSegmentRange(f.lane(), 70, 80, "X", "unknown");
        wave::setSegmentRange(f.lane(), 80, 90, "0", "known");
        wave::Event note;
        note.id = "independent-note"; note.laneId = "bit"; note.tick = 20;
        note.action = wave::EventAction::Note;
        scenario.events.push_back(note);
        const auto segments = f.lane().segments;
        wave::synchronizeLaneEventsFromSegments(scenario, "bit");
        QCOMPARE(f.lane().segments, segments);
        QCOMPARE(scenario.events.size(), std::size_t(6));
        QVERIFY(wave::findEvent(scenario, note.id));
        QVERIFY(std::none_of(scenario.events.begin(), scenario.events.end(),
            [](const wave::Event& event) { return event.waveformLinked && event.tick == 20; }));
        for (const auto tick : {0, 40, 50, 70, 80}) {
            QVERIFY(std::any_of(scenario.events.begin(), scenario.events.end(),
                [tick](const wave::Event& event) { return event.waveformLinked && event.tick == tick; }));
        }
        const auto stable = scenario;
        wave::synchronizeLaneEventsFromSegments(scenario, "bit");
        QCOMPARE(scenario, stable);

        wave::Event expectation;
        expectation.id = "explicit-expect-zero"; expectation.laneId = "bit";
        expectation.tick = 20; expectation.value = "0";
        expectation.action = wave::EventAction::Expect;
        expectation.waveformLinked = true; expectation.linkedSegmentId = "flat";
        scenario.events.push_back(expectation);
        wave::synchronizeLaneEventsFromSegments(scenario, "bit");
        const auto* kept = wave::findEvent(scenario, expectation.id);
        QVERIFY(kept); QCOMPARE(*kept, expectation);
        QCOMPARE(f.lane().segments, segments);
    }

    void noEdgeEventCreationIsAtomic()
    {
        Fixture f;
        auto& scenario = f.project.scenarios.front();
        const auto before = scenario;
        wave::Event event;
        event.id = "no-edge-drive"; event.laneId = "bit";
        event.tick = 10; event.value = "0";
        QVERIFY_EXCEPTION_THROWN(f.commands.execute(
            std::make_unique<wave::AddEventCommand>(scenario, event)), std::invalid_argument);
        QCOMPARE(scenario, before);
        QCOMPARE(f.commands.size(), std::size_t(0));
        event.id = "explicit-expect"; event.action = wave::EventAction::Expect;
        QVERIFY(f.commands.execute(std::make_unique<wave::AddEventCommand>(scenario, event)));
        const auto* added = wave::findEvent(scenario, event.id);
        QVERIFY(added); QVERIFY(added->action == wave::EventAction::Expect);
        QVERIFY(f.commands.undo()); QCOMPARE(scenario, before);
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

    void signalHeaderDoesNotPreviewWaveform_data()
    {
        QTest::addColumn<int>("laneIndex");
        QTest::addColumn<bool>("scrolled");
        QTest::newRow("clock") << 0 << false;
        QTest::newRow("bit") << 1 << false;
        QTest::newRow("bus") << 2 << false;
        QTest::newRow("clock-scrolled") << 0 << true;
        QTest::newRow("bit-scrolled") << 1 << true;
        QTest::newRow("bus-scrolled") << 2 << true;
    }

    void signalHeaderDoesNotPreviewWaveform()
    {
        QFETCH(int, laneIndex);
        QFETCH(bool, scrolled);
        Fixture f;
        f.show();
        click(f.canvas, f.point(15));
        if (scrolled) {
            f.canvas.zoomIn();
            f.scale *= 1.25;
            f.canvas.horizontalScrollBar()->setValue(41);
        }
        const auto before = f.project.scenarios.front();
        const auto selection = f.canvas.selectedTimeRange();
        const auto selectedLane = f.canvas.selectedLaneId();
        const auto cursor = f.canvas.cursorTick();
        const auto historySize = f.commands.size();
        QVERIFY(selection);
        hover(f.canvas, f.point(35, laneIndex));
        QVERIFY(f.canvas.hoveredBitBeatRange());
        QCOMPARE(f.canvas.hoveredBitBeatLaneId(), QString::fromStdString(f.lane(laneIndex).id));
        for (const auto x : {40, f.canvas.signalHeaderWidth() - 15}) {
            hover(f.canvas, {x, f.point(35, laneIndex).y()});
            QVERIFY(!f.canvas.hoveredBitBeatRange());
            QVERIFY(f.canvas.hoveredBitBeatLaneId().isEmpty());
            QCOMPARE(f.canvas.selectedTimeRange(), selection);
            QCOMPARE(f.canvas.selectedLaneId(), selectedLane);
            QCOMPARE(f.canvas.cursorTick(), cursor);
            QCOMPARE(f.commands.size(), historySize);
            QCOMPARE(f.project.scenarios.front(), before);
        }
        hover(f.canvas, f.point(35, laneIndex));
        QVERIFY(f.canvas.hoveredBitBeatRange());
    }

    void groupedWaveformsKeepTheirFill_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::newRow("light") << false;
        QTest::newRow("dark") << true;
    }

    void groupedWaveformsKeepTheirFill()
    {
        QFETCH(bool, dark);
        applyTheme(dark ? wave::WaveformColorScheme::Dark : wave::WaveformColorScheme::Light);
        Fixture f;
        auto& scenario = f.project.scenarios.front();
        wave::setSegmentRange(f.lane(2), 20, 40, "0x2a", "numeric");
        wave::setSegmentRange(f.lane(2), 60, 80, "text:IDLE", "text");
        auto secondClock = f.lane(0);
        secondClock.id = secondClock.name = "second-clock";
        auto secondBus = f.lane(2);
        secondBus.id = secondBus.name = "second-bus";
        for (auto& segment : secondBus.segments) segment.id += "-second";
        scenario.lanes.push_back(secondClock);
        scenario.lanes.push_back(secondBus);
        wave::Lane group;
        group.id = "signals"; group.name = "Signals";
        group.kind = wave::LaneKind::Group; group.height = 56;
        scenario.lanes.push_back(group);
        f.show();
        const auto before = scenario;
        const std::vector<std::string> members{"clock", "bit", "bus", "second-clock", "second-bus"};
        const auto samples = [&] {
            const auto image = f.canvas.viewport()->grab().toImage();
            std::vector<QColor> colors;
            auto top = 40;
            for (const auto& lane : scenario.lanes) {
                if (!f.canvas.isLaneDisplayed(QString::fromStdString(lane.id))) continue;
                if (lane.kind == wave::LaneKind::Clock || lane.kind == wave::LaneKind::Bus) {
                    for (const auto tick : {22, 27, 32, 37, 62, 67, 72, 77}) {
                        colors.push_back(pixel(image, {f.point(tick).x(), top + 17}));
                    }
                }
                top += lane.height;
            }
            return colors;
        };
        const auto originalFill = samples();
        QCOMPARE(originalFill.size(), std::size_t(32));
        QVERIFY(f.commands.execute(std::make_unique<wave::SetLanesGroupCommand>(
            scenario, members, group.id)));
        f.canvas.refreshModel();
        QCOMPARE(samples(), originalFill);
        QVERIFY(f.canvas.setGroupCollapsed("signals", true));
        QVERIFY(f.canvas.setGroupCollapsed("signals", false));
        QCOMPARE(samples(), originalFill);
        QVERIFY(f.commands.undo());
        f.canvas.refreshModel();
        QCOMPARE(scenario, before);
        QCOMPARE(samples(), originalFill);
        QVERIFY(f.commands.redo());
        f.canvas.refreshModel();
        QCOMPARE(samples(), originalFill);
        savePreview(f.canvas, dark ? "grouped-dark" : "grouped-light");
        QVERIFY(f.commands.execute(std::make_unique<wave::SetLanesGroupCommand>(
            scenario, members, std::string{})));
        f.canvas.refreshModel();
        QCOMPARE(samples(), originalFill);
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

    void signalHeaderLayout_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::addColumn<int>("width");
        QTest::addColumn<int>("height");
        for (const auto dark : {false, true}) {
            for (const auto width : {140, 190, 320}) {
                for (const auto height : {30, 56}) {
                    const auto name = QByteArray(dark ? "dark-" : "light-")
                        + QByteArray::number(width) + '-' + QByteArray::number(height);
                    QTest::newRow(name.constData()) << dark << width << height;
                }
            }
        }
    }

    void signalHeaderRangeLabel_data()
    {
        QTest::addColumn<int>("width");
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("label");
        QTest::newRow("bus-8") << 8 << QString("bus") << QString("bus[7:0]");
        QTest::newRow("bus-16") << 16 << QString("data") << QString("data[15:0]");
        QTest::newRow("bus-32") << 32 << QString("payload") << QString("payload[31:0]");
        QTest::newRow("bus-64") << 64 << QString("data") << QString("data[63:0]");
        QTest::newRow("one-bit") << 1 << QString("ready") << QString("ready");
        QTest::newRow("already-indexed") << 32 << QString("data[31:0]") << QString("data[31:0]");
        QTest::newRow("declared-range") << 8 << QString("data[15:8]") << QString("data[15:8]");
        QTest::newRow("array-member") << 8 << QString("port[2].data") << QString("port[2].data[7:0]");
    }

    void signalHeaderRangeLabel()
    {
        QFETCH(int, width);
        QFETCH(QString, name);
        QFETCH(QString, label);
        Fixture f;
        f.lane(2).name = name.toStdString();
        f.lane(2).width = width;
        const auto original = wave::serializeProject(f.project);
        f.show();
        f.canvas.setSignalHeaderWidth(320);
        const auto header = [&] {
            return f.canvas.viewport()->grab(QRect(0, 40 + 2 * 56, 319, 56)).toImage();
        };
        const auto rendered = header();
        QCOMPARE(wave::serializeProject(f.project), original);
        QCOMPARE(f.commands.size(), std::size_t(0));
        f.lane(2).name = label.toStdString();
        f.canvas.refreshModel();
        QCOMPARE(header(), rendered);
        f.canvas.beginLaneRename("bus", name);
        const auto* rename = f.canvas.findChild<QLineEdit*>("LaneRenameEdit");
        QVERIFY(rename);
        QCOMPARE(rename->text(), name);
    }

    void signalHeadersHaveNoSubtitles()
    {
        Fixture f;
        f.show();
        const auto original = wave::serializeProject(f.project);
        const auto background = wave::waveformTheme(wave::WaveformColorScheme::Light).raised;
        const auto image = f.canvas.viewport()->grab().toImage();
        for (const auto lane : {0, 1, 2}) {
            for (int y = 40; y < 51; ++y) {
                for (int x = 38; x < f.canvas.signalHeaderWidth() - 12; ++x) {
                    QCOMPARE(pixel(image, {x, 40 + lane * 56 + y}), background);
                }
            }
        }
        const auto clockHeader = f.canvas.viewport()->grab(QRect(0, 40, 189, 56)).toImage();
        f.project.clockDomains.front().period = 25;
        f.project.clockDomains.front().name = "reference_clock";
        f.canvas.refreshModel();
        QCOMPARE(f.canvas.viewport()->grab(QRect(0, 40, 189, 56)).toImage(), clockHeader);
        f.project.clockDomains.front().period = 10;
        f.project.clockDomains.front().name = "clk";
        QCOMPARE(wave::serializeProject(f.project), original);
        savePreview(f.canvas, "single-line-signal-headers");
    }

    void signalHeaderLayout()
    {
        QFETCH(bool, dark);
        QFETCH(int, width);
        QFETCH(int, height);
        applyTheme(dark ? wave::WaveformColorScheme::Dark : wave::WaveformColorScheme::Light);
        Fixture f;
        auto& scenario = f.project.scenarios.front();
        scenario.lanes.clear();
        std::vector<std::string> ids;
        for (const auto index : {0, 1, 2}) {
            wave::Lane lane;
            lane.id = "data-" + std::to_string(index);
            lane.name = "request_payload[31:0]";
            lane.kind = wave::LaneKind::Bus; lane.width = 32;
            lane.height = height; lane.clockDomainId = "clk";
            ids.push_back(lane.id);
            scenario.lanes.push_back(lane);
        }
        wave::Lane group;
        group.id = "request"; group.name = "Request interface";
        group.kind = wave::LaneKind::Group; group.height = height;
        scenario.lanes.push_back(group);
        f.show();
        f.canvas.setSignalHeaderWidth(width);
        const auto checkRows = [&](int firstTop) {
            const auto image = f.canvas.viewport()->grab().toImage();
            const auto ratio = image.devicePixelRatio();
            const auto crop = [&](int top) {
                return image.copy(QRect(qRound(32 * ratio), qRound((top + 5) * ratio),
                    qRound((width - 42) * ratio), qRound((height - 10) * ratio)));
            };
            // Identical signals must not inherit smaller fonts from preceding rows.
            // Fractional row strides have different subpixel rasterization phases.
            if (qFuzzyCompare(height * ratio, qreal(qRound(height * ratio)))) {
                QCOMPARE(crop(firstTop), crop(firstTop + height));
                QCOMPARE(crop(firstTop), crop(firstTop + 2 * height));
            }
        };
        checkRows(40);
        QVERIFY(f.commands.execute(std::make_unique<wave::SetLanesGroupCommand>(scenario, ids, group.id)));
        f.canvas.refreshModel();
        checkRows(40 + height);
        f.canvas.selectLaneHeaders({"data-1"}, "data-1");
        const auto grouped = scenario;
        const auto image = f.canvas.viewport()->grab().toImage();
        QCOMPARE(pixel(image, {width - 10, 40 + 2 * height + height / 2}),
            wave::waveformTheme(dark ? wave::WaveformColorScheme::Dark : wave::WaveformColorScheme::Light).selection);
        if (width == 190 && height == 56) {
            savePreview(f.canvas, dark ? "signal-header-dark" : "signal-header-light");
        }
        f.canvas.beginLaneRename("data-1", "request_payload[31:0]");
        auto* rename = f.canvas.findChild<QLineEdit*>("LaneRenameEdit");
        QVERIFY(rename && rename->isVisible());
        QVERIFY(rename->font().pointSizeF() >= 11.5);
        QVERIFY(rename->geometry().left() >= 32);
        QVERIFY(rename->geometry().right() < width);
        QVERIFY(rename->geometry().top() >= 40 + 2 * height);
        QVERIFY(rename->geometry().bottom() < 40 + 3 * height);
        QTest::keyClick(rename, Qt::Key_Escape);
        QVERIFY(!f.canvas.hasLaneRename());
        click(f.canvas, {16, 40 + height / 2});
        QVERIFY(f.canvas.isGroupCollapsed("request"));
        QVERIFY(!f.canvas.isLaneDisplayed("data-1"));
        click(f.canvas, {16, 40 + height / 2});
        QVERIFY(!f.canvas.isGroupCollapsed("request"));
        QVERIFY(f.canvas.isLaneDisplayed("data-1"));
        QCOMPARE(scenario, grouped);
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
            QCOMPARE(pixel(image, {f.canvas.signalHeaderWidth() - 10, 40 + 56 + 10}), theme.selection);
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
