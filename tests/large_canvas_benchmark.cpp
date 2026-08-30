#include "wave_canvas.h"

#include "wave/commands.h"
#include "wave/model.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QScrollBar>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

namespace {

constexpr std::size_t LaneCount = 1'000;
constexpr std::size_t SegmentsPerLane = 256;
constexpr std::size_t EventsPerLane = 20;
constexpr std::size_t MarkerCount = 20'000;
constexpr wave::Tick SegmentWidth = 1'000;
constexpr wave::Tick Duration =
    static_cast<wave::Tick>(SegmentsPerLane) * SegmentWidth;

[[nodiscard]] std::uint64_t workingSetBytes()
{
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
            sizeof(counters))) {
        return static_cast<std::uint64_t>(counters.WorkingSetSize);
    }
#endif
    return 0;
}

[[nodiscard]] wave::Project makeLargeProject()
{
    wave::Project project;
    project.id = "large-canvas-project";
    project.name = "Large canvas benchmark";

    wave::Scenario scenario;
    scenario.id = "large-canvas-scenario";
    scenario.name = "1000 lanes";
    scenario.duration = Duration;
    scenario.lanes.reserve(LaneCount);
    scenario.events.reserve(LaneCount * EventsPerLane);
    scenario.markers.reserve(MarkerCount);

    for (std::size_t laneIndex = 0; laneIndex < LaneCount; ++laneIndex) {
        wave::Lane lane;
        lane.id = "lane-" + std::to_string(laneIndex);
        lane.name = "Signal " + std::to_string(laneIndex);
        lane.kind = wave::LaneKind::Bit;
        lane.height = 40;
        lane.segments.reserve(SegmentsPerLane);
        for (std::size_t segmentIndex = 0;
             segmentIndex < SegmentsPerLane;
             ++segmentIndex) {
            const auto start = static_cast<wave::Tick>(segmentIndex)
                * SegmentWidth;
            lane.segments.push_back({
                "segment-" + std::to_string(laneIndex) + "-"
                    + std::to_string(segmentIndex),
                start,
                start + SegmentWidth,
                segmentIndex % 2 == 0 ? "0" : "1",
                {},
            });
        }
        scenario.lanes.push_back(std::move(lane));

        for (std::size_t eventIndex = 0;
             eventIndex < EventsPerLane;
             ++eventIndex) {
            wave::Event event;
            event.id = "event-" + std::to_string(laneIndex) + "-"
                + std::to_string(eventIndex);
            event.laneId = "lane-" + std::to_string(laneIndex);
            event.tick = static_cast<wave::Tick>(
                (eventIndex * SegmentsPerLane / EventsPerLane)
                * SegmentWidth
                + (laneIndex % 500));
            event.action = eventIndex % 2 == 0
                ? wave::EventAction::Drive
                : wave::EventAction::Expect;
            event.value = eventIndex % 2 == 0 ? "0" : "1";
            scenario.events.push_back(std::move(event));
        }
    }

    scenario.relations.reserve(scenario.events.size() / 2);
    for (std::size_t eventIndex = 0;
         eventIndex + 1 < scenario.events.size();
         eventIndex += 2) {
        const auto& source = scenario.events[eventIndex];
        const auto& target = scenario.events[eventIndex + 1];
        wave::Relation relation;
        relation.id = "relation-" + std::to_string(eventIndex / 2);
        relation.sourceEventId = source.id;
        relation.targetEventId = target.id;
        relation.maximumDelay = std::max<wave::Tick>(
            1,
            std::abs(target.tick - source.tick));
        relation.description = "Benchmark relation";
        scenario.relations.push_back(std::move(relation));
    }

    for (std::size_t markerIndex = 0;
         markerIndex < MarkerCount;
         ++markerIndex) {
        const auto tick = static_cast<wave::Tick>(
            (markerIndex * static_cast<std::size_t>(Duration))
            / MarkerCount);
        wave::Marker marker;
        marker.id = "marker-" + std::to_string(markerIndex);
        marker.name = "M" + std::to_string(markerIndex);
        marker.start = tick;
        marker.end = markerIndex % 5 == 0
            ? std::min<wave::Tick>(Duration, tick + 500)
            : tick;
        marker.kind = marker.start == marker.end
            ? wave::MarkerKind::Point
            : wave::MarkerKind::Interval;
        scenario.markers.push_back(std::move(marker));
    }

    project.scenarios.push_back(std::move(scenario));
    return project;
}

[[nodiscard]] bool check(const bool condition, const char* message)
{
    if (condition) return true;
    std::cerr << message << '\n';
    return false;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    application.setQuitOnLastWindowClosed(false);

    auto project = makeLargeProject();
    wave::CommandStack commandStack;
    wave::WaveCanvas canvas;
    canvas.resize(1'280, 720);
    canvas.setRelationsVisible(true);

    const auto memoryBefore = workingSetBytes();
    QElapsedTimer coldTimer;
    coldTimer.start();
    canvas.setDocument(
        &project,
        &project.scenarios.front(),
        &commandStack);
    canvas.show();
    application.processEvents();
    canvas.restoreVisibleTimeSpan(10'000, 5'000);
    application.processEvents();
    static_cast<void>(canvas.viewport()->grab());
    application.processEvents();
    const auto coldOpenMs = coldTimer.elapsed();

    QElapsedTimer navigationTimer;
    navigationTimer.start();
    for (int step = 0; step < 32; ++step) {
        const auto verticalMaximum = canvas.verticalScrollBar()->maximum();
        const auto horizontalMaximum = canvas.horizontalScrollBar()->maximum();
        canvas.verticalScrollBar()->setValue(
            verticalMaximum == 0
                ? 0
                : (step * 997) % (verticalMaximum + 1));
        canvas.horizontalScrollBar()->setValue(
            horizontalMaximum == 0
                ? 0
                : (step * 389) % (horizontalMaximum + 1));
        if (step % 4 == 0) canvas.zoomIn();
        if (step % 4 == 2) canvas.zoomOut();
        application.processEvents();
        static_cast<void>(canvas.viewport()->grab());
    }
    const auto navigationMs = navigationTimer.elapsed();
    const auto memoryAfter = workingSetBytes();
    const auto memoryDelta = memoryAfter > memoryBefore
        ? memoryAfter - memoryBefore
        : std::uint64_t{0};
    const auto memoryDeltaMiB = static_cast<double>(memoryDelta)
        / (1024.0 * 1024.0);

    const auto renderedLanes = canvas.property(
        "wavewidgets.renderedLaneCount").toULongLong();
    const auto renderedMarkers = canvas.property(
        "wavewidgets.renderedMarkerCount").toULongLong();
    const auto renderedRelations = canvas.property(
        "wavewidgets.renderedRelationCount").toULongLong();
    const auto renderedEvents = canvas.property(
        "wavewidgets.renderedEventCount").toULongLong();
    const auto modelGeneration = canvas.property(
        "wavewidgets.modelGeneration").toULongLong();
    const auto viewGeneration = canvas.property(
        "wavewidgets.viewGeneration").toULongLong();
    const auto paintGeneration = canvas.property(
        "wavewidgets.lastPaintViewGeneration").toULongLong();

    std::cout
        << "[METRIC] lanes=" << LaneCount
        << " segments=" << LaneCount * SegmentsPerLane
        << " events=" << project.scenarios.front().events.size()
        << " relations=" << project.scenarios.front().relations.size()
        << " markers=" << MarkerCount << '\n'
        << "[METRIC] cold_open_ms=" << coldOpenMs
        << " navigation_32_frames_ms=" << navigationMs
        << " working_set_delta_mib=" << memoryDeltaMiB << '\n'
        << "[METRIC] rendered_lanes=" << renderedLanes
        << " rendered_markers=" << renderedMarkers
        << " rendered_relations=" << renderedRelations
        << " rendered_events=" << renderedEvents << '\n';

    auto passed = true;
    passed &= check(coldOpenMs < 8'000, "cold open exceeded 8 seconds");
    passed &= check(
        navigationMs < 5'000,
        "32 navigation/zoom frames exceeded 5 seconds");
    passed &= check(
        memoryDelta == 0 || memoryDelta < 768ULL * 1024ULL * 1024ULL,
        "working-set growth exceeded 768 MiB");
    passed &= check(renderedLanes > 0 && renderedLanes < 64,
        "paint traversed more than the visible lane window");
    passed &= check(renderedMarkers < 4'000,
        "paint traversed too many marker overlays");
    passed &= check(renderedRelations < 4'000,
        "paint traversed too many relation overlays");
    passed &= check(renderedEvents < 4'000,
        "paint traversed too many event overlays");
    passed &= check(modelGeneration > 0,
        "model generation did not advance");
    passed &= check(viewGeneration == paintGeneration,
        "paint did not consume the latest viewport generation");
    return passed ? 0 : 1;
}
