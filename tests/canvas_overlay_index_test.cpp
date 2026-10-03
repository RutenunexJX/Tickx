#include "wave_canvas.h"
#include "wave/project_io.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QScrollBar>
#include <QTest>

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <tuple>

namespace {
QJsonArray pointJson(const QPointF& point) { return {point.x(), point.y()}; }
QJsonArray rectJson(const QRect& rect)
{
    return {rect.x(), rect.y(), rect.width(), rect.height()};
}
QJsonObject regionJson(const std::string& id, const QPoint& source, const QPoint& target)
{
    return {{"id", QString::fromStdString(id)}, {"source", pointJson(source)},
        {"target", pointJson(target)},
        {"sourceHandle", rectJson(QRect(source - QPoint(7, 7), QSize(15, 15)))},
        {"targetHandle", rectJson(QRect(target - QPoint(7, 7), QSize(15, 15)))}};
}
} // namespace

namespace wave {
// Observe existing geometry and hit testing, without reading the optimized index
// or adding exported testing methods/data members to WaveCanvas.
class CanvasOverlayTestAccess {
public:
    static QPoint point(const WaveCanvas& canvas, const Event& event)
    {
        return canvas.eventPoint(event);
    }
    static QJsonArray regions(const WaveCanvas& canvas)
    {
        QJsonArray result;
        for (const auto& region : canvas.relationHitRegions_) {
            result.append(QJsonObject{{"id", QString::fromStdString(region.relationId)},
                {"source", pointJson(region.line.p1())},
                {"target", pointJson(region.line.p2())},
                {"sourceHandle", rectJson(region.sourceHandle)},
                {"targetHandle", rectJson(region.targetHandle)}});
        }
        return result;
    }
    static QString hitId(const WaveCanvas& canvas, const QPoint& point)
    {
        const auto hit = canvas.relationHitAtPosition(point);
        return hit.relation ? QString::fromStdString(hit.relation->id) : QString{};
    }
    static QJsonArray probes(const WaveCanvas& canvas)
    {
        QJsonArray result;
        const auto append = [&](const QPoint& position) {
            const auto hit = canvas.relationHitAtPosition(position);
            result.append(QJsonObject{{"point", pointJson(position)},
                {"id", hit.relation ? QString::fromStdString(hit.relation->id) : QString{}},
                {"endpoint", static_cast<int>(hit.endpoint)}});
        };
        for (const auto& region : canvas.relationHitRegions_) {
            append(region.line.p1().toPoint());
            append(region.line.pointAt(0.5).toPoint());
            append(region.line.p2().toPoint());
        }
        append({0, 0});
        return result;
    }
    static QImage overlays(WaveCanvas& canvas, const Tick start, const Tick end,
        const std::vector<std::string>* removals)
    {
        QImage image(canvas.viewport()->size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setFont(canvas.font());
        canvas.drawScenarioOverlays(painter, start, end, removals);
        return image;
    }
    static const std::vector<Tick>& snapTicks(const WaveCanvas& canvas)
    {
        return canvas.signalEdgeIndex_;
    }
};
} // namespace wave

namespace {
using Access = wave::CanvasOverlayTestAccess;
constexpr wave::Tick Duration = 240'000;

wave::Project makeProject()
{
    wave::Project project;
    project.id = project.name = "overlay-index-regression";
    wave::Scenario scenario;
    scenario.id = scenario.name = "scenario";
    scenario.duration = Duration;
    constexpr std::array<wave::Tick, 10> ticks{
        20'000, 60'000, 110'000, 150'000, 180'000,
        100'000, 30'000, 210'000, 80'000, 120'000};
    for (std::size_t index = 0; index < ticks.size(); ++index) {
        wave::Lane lane;
        lane.id = lane.name = "lane-" + std::to_string(index);
        lane.height = 48;
        lane.visible = index != 6;
        lane.segments = {{"a-" + lane.id, 0, 60'000, "0", {}},
            {"b-" + lane.id, 60'000, Duration, "1", {}}};
        scenario.lanes.push_back(lane);
        wave::Event event;
        event.id = "event-" + std::to_string(index);
        event.laneId = lane.id;
        event.tick = ticks[index];
        event.value = "1";
        scenario.events.push_back(event);
    }
    const auto add = [&](const char* id, const char* source, const char* target) {
        wave::Relation relation;
        relation.id = id;
        relation.sourceEventId = source;
        relation.targetEventId = target;
        relation.maximumDelay = Duration;
        scenario.relations.push_back(relation);
    };
    // Equal spans retain model order; the last-painted line wins hit testing.
    add("back", "event-0", "event-1");
    add("front", "event-0", "event-1");
    add("reverse", "event-3", "event-2");
    add("crossing", "event-0", "event-9");
    add("hidden", "event-6", "event-7");
    add("below", "event-8", "event-9");
    add("missing-source", "absent", "event-1");
    add("missing-target", "event-0", "absent");
    wave::Marker marker;
    marker.id = marker.name = "marker";
    marker.start = 45'000;
    marker.end = 75'000;
    marker.kind = wave::MarkerKind::Interval;
    scenario.markers.push_back(marker);
    project.scenarios.push_back(scenario);
    return project;
}

struct Fixture {
    wave::Project project{makeProject()};
    std::size_t activeScenario{0};
    wave::CommandStack commands;
    wave::WaveCanvas canvas;
    wave::Scenario& scenario() { return project.scenarios.at(activeScenario); }
    Fixture()
    {
        canvas.resize(1'280, 390);
        canvas.setDocument(&project, &scenario(), &commands);
        canvas.setTool(wave::WaveCanvas::Tool::Relation);
        canvas.setRelationsVisible(true);
        canvas.show();
        QCoreApplication::processEvents();
        canvas.fitScenario();
        QCoreApplication::processEvents();
    }
};

// A deliberately linear model reference. No cached indices or cached endpoints
// are consulted. Last duplicate IDs used to order spans; first IDs were drawn.
QJsonArray referenceRegions(const Fixture& fixture, const wave::Tick start,
    const wave::Tick end, const std::vector<std::string>* removals)
{
    const auto& scenario = fixture.project.scenarios.at(fixture.activeScenario);
    struct Entry {
        std::size_t order;
        wave::Tick start;
        wave::Tick end;
        const wave::Event* source;
        const wave::Event* target;
    };
    std::vector<Entry> entries;
    for (std::size_t index = 0; index < scenario.relations.size(); ++index) {
        const auto& relation = scenario.relations[index];
        const auto* source = wave::findEvent(scenario, relation.sourceEventId);
        const auto* target = wave::findEvent(scenario, relation.targetEventId);
        if (!source || !target) continue;
        const auto lastSource = std::find_if(scenario.events.rbegin(), scenario.events.rend(),
            [&](const auto& event) { return event.id == relation.sourceEventId; });
        const auto lastTarget = std::find_if(scenario.events.rbegin(), scenario.events.rend(),
            [&](const auto& event) { return event.id == relation.targetEventId; });
        entries.push_back({index, std::min(lastSource->tick, lastTarget->tick),
            std::max(lastSource->tick, lastTarget->tick), source, target});
    }
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        return std::tie(a.start, a.end, a.order) < std::tie(b.start, b.end, b.order);
    });
    QJsonArray result;
    auto prefixEnd = std::numeric_limits<wave::Tick>::lowest();
    for (const auto& entry : entries) {
        if (entry.start > end) break;
        prefixEnd = std::max(prefixEnd, entry.end);
        if (prefixEnd < start) continue;
        const auto& relation = scenario.relations[entry.order];
        const bool removing = removals
            && std::find(removals->begin(), removals->end(), relation.id) != removals->end();
        if (!fixture.canvas.relationsVisible() && !removing) continue;
        if (std::max(entry.source->tick, entry.target->tick) < start
            || std::min(entry.source->tick, entry.target->tick) > end) continue;
        const auto source = Access::point(fixture.canvas, *entry.source);
        const auto target = Access::point(fixture.canvas, *entry.target);
        if (source.isNull() || target.isNull()) continue;
        if (std::max(source.y(), target.y()) < 40
            || std::min(source.y(), target.y()) > fixture.canvas.viewport()->height()) continue;
        result.append(regionJson(relation.id, source, target));
    }
    return result;
}

struct Capture {
    QByteArray model;
    QImage canvas;
    QImage overlays;
    QJsonArray regions;
    QJsonArray probes;
    QJsonArray snapTicks;
    QJsonObject metrics;
};

Capture capture(Fixture& fixture, const wave::Tick start = 0,
    const wave::Tick end = Duration, const std::vector<std::string>* removals = nullptr)
{
    QCoreApplication::processEvents();
    Capture result;
    result.canvas = fixture.canvas.viewport()->grab().toImage();
    result.overlays = Access::overlays(fixture.canvas, start, end, removals);
    result.model = wave::serializeProject(fixture.project);
    result.regions = Access::regions(fixture.canvas);
    result.probes = Access::probes(fixture.canvas);
    for (const auto tick : Access::snapTicks(fixture.canvas)) result.snapTicks.append(tick);
    for (const auto* name : {"overlayIndexBuildCount", "overlayIndexEventCount",
             "overlayIndexEndpointLookupCount", "relationCandidateCount",
             "relationEndpointAccessCount", "renderedRelationCount", "snapIndexBuildCount",
             "snapIndexReserveCalls", "snapIndexCapacityGrowths", "snapIndexEndpointCount",
             "snapIndexUniqueCount"}) {
        const auto key = QByteArray("wavewidgets.") + name;
        result.metrics.insert(QLatin1String(name),
            QJsonValue::fromVariant(fixture.canvas.property(key.constData())));
    }
    return result;
}

// Baseline comparison skips ONLY counters that were added by this change.
// Model, images, geometry and interactive selection assertions always run.
bool checkNewCounters() { return !qEnvironmentVariableIsSet("WAVE_OVERLAY_BASELINE"); }
bool checkSnapCounters()
{
    return checkNewCounters() && !qEnvironmentVariableIsSet("WAVE_SNAP_BASELINE");
}

void verifyAndSave(Fixture& fixture, const Capture& result, const QString& name,
    const wave::Tick start = 0, const wave::Tick end = Duration,
    const std::vector<std::string>* removals = nullptr)
{
    const auto expected = referenceRegions(fixture, start, end, removals);
    QCOMPARE(result.regions,
        fixture.canvas.tool() == wave::WaveCanvas::Tool::Relation ? expected : QJsonArray{});
    QCOMPARE(result.metrics.value("renderedRelationCount").toInteger(), expected.size());
    if (checkNewCounters()) {
        QVERIFY(result.metrics.value("overlayIndexBuildCount").toInteger() > 0);
        QCOMPARE(result.metrics.value("overlayIndexEventCount").toInteger(),
            static_cast<qint64>(fixture.scenario().events.size()));
        QCOMPARE(result.metrics.value("overlayIndexEndpointLookupCount").toInteger(),
            static_cast<qint64>(fixture.scenario().relations.size() * 2));
        QVERIFY(result.metrics.value("relationEndpointAccessCount").toInteger()
            <= result.metrics.value("relationCandidateCount").toInteger() * 2);
    }
    const auto directory = qEnvironmentVariable("WAVE_OVERLAY_EVIDENCE_DIR");
    if (directory.isEmpty()) return;
    QVERIFY(QDir().mkpath(directory));
    const auto base = directory + QLatin1Char('/') + name;
    const auto write = [](const QString& path, const QByteArray& bytes) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
    };
    QVERIFY(write(base + ".model.json", result.model));
    QVERIFY(write(base + ".geometry.json", QJsonDocument(QJsonObject{
        {"regions", result.regions}, {"probes", result.probes},
        {"snapTicks", result.snapTicks}}).toJson()));
    QVERIFY(write(base + ".metrics.json", QJsonDocument(result.metrics).toJson()));
    QVERIFY(result.canvas.save(base + ".canvas.png"));
    QVERIFY(result.overlays.save(base + ".overlays.png"));
}
} // namespace

class CanvasOverlayIndexTest final : public QObject {
    Q_OBJECT
private slots:
    void orderedGeometryClippingAndStableSelection()
    {
        Fixture f;
        const auto initial = capture(f);
        verifyAndSave(f, initial, "initial");
        QCOMPARE(initial.regions.size(), qsizetype{4});
        const auto source = Access::point(f.canvas, f.scenario().events[0]);
        const auto target = Access::point(f.canvas, f.scenario().events[1]);
        const auto midpoint = QLineF(source, target).pointAt(0.5).toPoint();
        QCOMPARE(Access::hitId(f.canvas, midpoint), QString("front"));
        QTest::mouseClick(f.canvas.viewport(), Qt::LeftButton, Qt::NoModifier, midpoint);
        QCOMPARE(f.canvas.selectedRelationIds(), QStringList{QStringLiteral("front")});
        const auto selected = capture(f);
        verifyAndSave(f, selected, "selected");
        QCOMPARE(selected.model, initial.model);
        f.canvas.verticalScrollBar()->setValue(f.canvas.verticalScrollBar()->maximum());
        f.canvas.restoreVisibleTimeSpan(80'000, 130'000);
        verifyAndSave(f, capture(f, 90'000, 170'000), "clipped", 90'000, 170'000);
    }

    void relocationAndEqualCountChanges()
    {
        Fixture f;
        const auto before = capture(f);
        f.scenario().events.reserve(f.scenario().events.capacity() + 100);
        const auto relocated = capture(f);
        verifyAndSave(f, relocated, "relocated");
        QCOMPARE(relocated.model, before.model);
        QCOMPARE(relocated.overlays, before.overlays);
        QCOMPARE(relocated.regions, before.regions);
        if (checkNewCounters()) QCOMPARE(relocated.metrics.value("overlayIndexBuildCount"),
            before.metrics.value("overlayIndexBuildCount"));
        // Equal-size model replacement follows the existing refreshModel contract.
        std::reverse(f.scenario().events.begin(), f.scenario().events.end());
        f.canvas.refreshModel();
        const auto reordered = capture(f);
        verifyAndSave(f, reordered, "reordered");
        QCOMPARE(reordered.overlays, before.overlays);
        QCOMPARE(reordered.regions, before.regions);
        std::reverse(f.scenario().relations.begin(), f.scenario().relations.end());
        f.canvas.refreshModel();
        verifyAndSave(f, capture(f), "relations-reordered");
        const auto midpoint = QLineF(
            Access::point(f.canvas, *wave::findEvent(f.scenario(), "event-0")),
            Access::point(f.canvas, *wave::findEvent(f.scenario(), "event-1")))
                                  .pointAt(0.5).toPoint();
        QCOMPARE(Access::hitId(f.canvas, midpoint), QString("back"));
        auto* renamed = wave::findEvent(f.scenario(), "event-1");
        QVERIFY(renamed);
        renamed->id = "renamed";
        renamed->tick = 85'000;
        renamed->laneId = "lane-3";
        for (auto& relation : f.scenario().relations) {
            if (relation.targetEventId == "event-1") relation.targetEventId = "renamed";
        }
        wave::findRelation(f.scenario(), "front")->sourceEventId = "event-2";
        f.canvas.refreshModel();
        const auto changed = capture(f);
        verifyAndSave(f, changed, "renamed-retargeted");
        QVERIFY(changed.regions != before.regions);
        if (checkNewCounters()) QCOMPARE(changed.metrics.value("overlayIndexBuildCount").toInteger(),
            before.metrics.value("overlayIndexBuildCount").toInteger() + 3);
        // Count changes retain the existing paint-time fallback, without refresh.
        std::erase_if(f.scenario().events, [](const auto& event) { return event.id == "renamed"; });
        verifyAndSave(f, capture(f), "removed-endpoint");
        auto replacement = makeProject().scenarios.front();
        replacement.events[0].tick = 90'000;
        // Copy assignment preserves the live ScenarioRef, as snapshot undo does.
        // Moving in a new Scenario transfers identity and requires setDocument.
        f.scenario() = replacement;
        f.canvas.refreshModel();
        verifyAndSave(f, capture(f), "scenario-replaced");
        auto second = f.scenario();
        second.id = "second";
        second.events[0].laneId = "lane-4";
        f.project.scenarios.push_back(std::move(second));
        f.activeScenario = 1;
        f.canvas.setDocument(&f.project, &f.project.scenarios.back(), &f.commands);
        f.canvas.fitScenario();
        const auto secondImage = capture(f);
        verifyAndSave(f, secondImage, "scenario-second");
        f.activeScenario = 0;
        f.canvas.setDocument(&f.project, &f.project.scenarios.front(), &f.commands);
        f.canvas.fitScenario();
        const auto firstAgain = capture(f);
        verifyAndSave(f, firstAgain, "scenario-returned");
        QVERIFY(secondImage.regions != firstAgain.regions);
    }

    void commandsUndoRedoPreserveCompleteResults()
    {
        Fixture f;
        const auto before = capture(f);
        auto replacement = *wave::findRelation(f.scenario(), "front");
        replacement.targetEventId = "event-4";
        QVERIFY(f.commands.execute(std::make_unique<wave::ChangeRelationCommand>(
            f.scenario(), "front", replacement)));
        f.canvas.refreshModel();
        const auto after = capture(f);
        verifyAndSave(f, after, "command-after");
        QVERIFY(after.regions != before.regions);
        QVERIFY(f.commands.undo());
        f.canvas.refreshModel();
        const auto undone = capture(f);
        verifyAndSave(f, undone, "command-undone");
        QCOMPARE(undone.model, before.model);
        QCOMPARE(undone.overlays, before.overlays);
        QCOMPARE(undone.regions, before.regions);
        QCOMPARE(undone.probes, before.probes);
        QVERIFY(f.commands.redo());
        f.canvas.refreshModel();
        const auto redone = capture(f);
        verifyAndSave(f, redone, "command-redone");
        QCOMPARE(redone.model, after.model);
        QCOMPARE(redone.overlays, after.overlays);
        QCOMPARE(redone.regions, after.regions);
        QCOMPARE(redone.probes, after.probes);
    }

    void removalPreviewAndDuplicateIds()
    {
        Fixture f;
        f.canvas.setRelationsVisible(false);
        verifyAndSave(f, capture(f), "relations-hidden");
        const std::vector<std::string> removals{"front", "missing-target"};
        const auto preview = capture(f, 0, Duration, &removals);
        verifyAndSave(f, preview, "removal-preview", 0, Duration, &removals);
        QCOMPARE(preview.regions.size(), qsizetype{1});
        f.canvas.setTool(wave::WaveCanvas::Tool::WaveEdit);
        const auto editing = capture(f, 0, Duration, &removals);
        verifyAndSave(f, editing, "removal-edit-preview", 0, Duration, &removals);
        QVERIFY(editing.regions.isEmpty());
        QCOMPARE(editing.model, preview.model);
        f.canvas.setTool(wave::WaveCanvas::Tool::Relation);
        f.canvas.setRelationsVisible(true);
        // Invalid duplicate IDs must retain the previous deterministic behavior.
        auto duplicate = f.scenario().events.front();
        duplicate.tick = 200'000;
        duplicate.laneId = "lane-4";
        f.scenario().events.push_back(duplicate);
        f.canvas.refreshModel();
        verifyAndSave(f, capture(f), "duplicate-full");
        verifyAndSave(f, capture(f, 0, 40'000), "duplicate-clipped", 0, 40'000);
    }

    void snapCapacityPreservesAllUniqueEdges()
    {
        Fixture f;
        f.scenario().lanes.front().kind = wave::LaneKind::Group;
        f.scenario().lanes.front().segments = {{"group-only", 13, 17, "0", {}}};
        f.scenario().lanes[6].segments = {{"hidden-only", 19, 23, "0", {}}};
        f.scenario().lanes[1].groupId = "lane-0";
        f.scenario().lanes[1].segments.push_back({"overlap", 60'000, 95'000, "0", {}});
        f.canvas.refreshModel();
        const std::vector<wave::Tick> expected{0, 60'000, 95'000, Duration};
        QCOMPARE(Access::snapTicks(f.canvas), expected);
        const auto result = capture(f);
        verifyAndSave(f, result, "snap-edges");
        if (checkSnapCounters()) {
            QCOMPARE(result.metrics.value("snapIndexReserveCalls").toInteger(), qint64{1});
            QVERIFY(result.metrics.value("snapIndexCapacityGrowths").toInteger() <= 1);
            QCOMPARE(result.metrics.value("snapIndexEndpointCount").toInteger(), qint64{34});
            QCOMPARE(result.metrics.value("snapIndexUniqueCount").toInteger(), qint64{4});
        }
        QVERIFY(f.canvas.setGroupCollapsed(QStringLiteral("lane-0"), true));
        const std::vector<wave::Tick> collapsedEdges{0, 60'000, Duration};
        QCOMPARE(Access::snapTicks(f.canvas), collapsedEdges);
        verifyAndSave(f, capture(f), "snap-collapsed");
        QVERIFY(f.canvas.setGroupCollapsed(QStringLiteral("lane-0"), false));
        QCOMPARE(Access::snapTicks(f.canvas), expected);
        for (auto& lane : f.scenario().lanes) lane.segments.clear();
        f.canvas.refreshModel();
        QVERIFY(Access::snapTicks(f.canvas).empty());
        if (checkSnapCounters()) QCOMPARE(
            f.canvas.property("wavewidgets.snapIndexReserveCalls").toULongLong(), qulonglong{0});
        verifyAndSave(f, capture(f), "snap-empty");
    }
};

QTEST_MAIN(CanvasOverlayIndexTest)
#include "canvas_overlay_index_test.moc"
