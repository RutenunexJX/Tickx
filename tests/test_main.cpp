#include "wave/commands.h"
#include "wave/compare.h"
#include "wave/export.h"
#include "wave/generation.h"
#include "wave/integration.h"
#include "wave/model.h"
#include "wave/project_io.h"
#include "wave/time.h"
#include "wave/trace.h"
#include "wave/validation.h"

#include <QDir>
#include <QColor>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QImage>
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QUrlQuery>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

class TestFailure final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

void expect(const bool condition, const std::string& message)
{
    if (!condition) throw TestFailure(message);
}

template<typename Left, typename Right>
void expectEqual(const Left& left, const Right& right, const std::string& message)
{
    if (!(left == right)) throw TestFailure(message);
}

void testTimeConversions()
{
    const wave::TimeBase timeBase{1};
    expectEqual(
        wave::toTicks(125, wave::TimeUnit::Nanosecond, timeBase),
        std::optional<wave::Tick>{125'000},
        "125 ns must convert to 125000 ps ticks");
    expectEqual(
        wave::fromTicks(125'000, wave::TimeUnit::Nanosecond, timeBase),
        std::optional<std::int64_t>{125},
        "tick conversion must round-trip");

    const wave::TimeBase coarse{10};
    expect(
        !wave::toTicks(1, wave::TimeUnit::Picosecond, coarse).has_value(),
        "inexact conversion must be rejected");
    expect(
        !wave::toTicks(
             std::numeric_limits<std::int64_t>::max(),
             wave::TimeUnit::Millisecond,
             timeBase)
             .has_value(),
        "overflowing conversion must be rejected");

    const auto large = wave::toTicks(
        8'000'000'000,
        wave::TimeUnit::Nanosecond,
        timeBase);
    expect(large.has_value(), "large exact time must remain representable");
    expectEqual(
        wave::fromTicks(*large, wave::TimeUnit::Nanosecond, timeBase),
        std::optional<std::int64_t>{8'000'000'000},
        "large time must not lose precision");
}

void testClocksAndSnapping()
{
    const wave::ClockDomain clock{
        "clock-a",
        "clk_a",
        10'000,
        2'000,
        {1, 2},
        wave::ClockEdge::Rising,
        {},
        {},
    };
    const wave::ClockDomain clockB{
        "clock-b",
        "clk_b",
        7'500,
        500,
        {2, 5},
        wave::ClockEdge::Falling,
        {},
        {},
    };
    expect(clock.isValid() && clockB.isValid(), "multiple clock domains must validate");
    expectEqual(
        wave::tickAtCycle(clock, 3, wave::ClockEdge::Rising),
        std::optional<wave::Tick>{32'000},
        "rising cycle tick is incorrect");
    expectEqual(
        wave::tickAtCycle(clock, 3, wave::ClockEdge::Falling),
        std::optional<wave::Tick>{37'000},
        "falling cycle tick is incorrect");

    const wave::SnapContext gridContext{1'000, 10'000, nullptr, {}, {}};
    expectEqual(
        wave::snapTick(1'499, wave::SnapMode::FixedGrid, gridContext),
        wave::Tick{1'000},
        "fixed-grid snap is incorrect");
    expectEqual(
        wave::snapTick(1'500, wave::SnapMode::FixedGrid, gridContext),
        wave::Tick{1'000},
        "snap ties must resolve to the lower tick");

    const wave::SnapContext clockContext{1, 1, &clock, {}, {}};
    expectEqual(
        wave::snapTick(31'100, wave::SnapMode::ClockRising, clockContext),
        wave::Tick{32'000},
        "clock-edge snap is incorrect");

    const std::vector<wave::Tick> edges{100, 250, 900};
    const wave::SnapContext edgeContext{1, 1, nullptr, edges, {}};
    expectEqual(
        wave::snapTick(240, wave::SnapMode::SignalEdge, edgeContext),
        wave::Tick{250},
        "signal-edge snap is incorrect");
}

void testClockOverridesAndRetiming()
{
    auto project = wave::makeDemonstrationProject();
    project.scenarios.reserve(2);
    auto& scenario = project.scenarios.front();
    auto* clockLane = wave::findLane(scenario, "lane-clk");
    expect(clockLane != nullptr, "clock lane is missing");
    wave::setSegmentRange(
        *clockLane,
        160'000,
        180'000,
        "gate",
        "clock-gated");
    wave::setSegmentRange(
        *clockLane,
        190'000,
        200'000,
        "disable",
        "clock-disabled");
    expectEqual(
        clockLane->segments.front().value,
        std::string{"gated"},
        "clock gate alias was not normalized");
    expectEqual(
        wave::clockValueAt(project.clockDomains.front(), *clockLane, 165'000),
        '0',
        "gated clock did not hold low");
    expectEqual(
        wave::clockValueAt(project.clockDomains.front(), *clockLane, 195'000),
        'X',
        "disabled clock did not become unknown");
    expectEqual(
        wave::clockValueAt(project.clockDomains.front(), *clockLane, 152'000),
        '1',
        "parameterized clock value outside overrides is incorrect");

    wave::CommandStack clearStack;
    clearStack.execute(std::make_unique<wave::ClearLaneRangeCommand>(
        scenario,
        clockLane->id,
        160'000,
        180'000));
    expect(
        !wave::clockOverrideAt(*clockLane, 165'000),
        "clear clock override command did not restore running mode");
    expect(clearStack.undo(), "clock override clear undo failed");
    expectEqual(
        wave::clockOverrideAt(*clockLane, 165'000),
        std::optional<wave::ClockOverrideMode>{wave::ClockOverrideMode::Gated},
        "clock override clear undo did not restore the gate");

    const auto requestEvent = std::find_if(
        scenario.events.begin(),
        scenario.events.end(),
        [](const wave::Event& event) {
            return event.linkedSegmentId == "segment-req-high";
        });
    expect(requestEvent != scenario.events.end(), "request event is missing");
    requestEvent->cycle = 8;
    requestEvent->clockDomainId = "clock-main";
    const auto requestEventId = requestEvent->id;
    const auto requestLowEvent = std::find_if(
        scenario.events.begin(),
        scenario.events.end(),
        [](const wave::Event& event) {
            return event.linkedSegmentId == "segment-req-low-b";
        });
    expect(requestLowEvent != scenario.events.end(), "request-low event is missing");
    requestLowEvent->cycle = 13;
    requestLowEvent->clockDomainId = "clock-main";
    const auto requestLowEventId = requestLowEvent->id;
    scenario.duration = 300'000;
    for (auto& lane : scenario.lanes) {
        for (auto& segment : lane.segments) {
            if (segment.end == 220'000) segment.end = scenario.duration;
        }
    }
    wave::Scenario secondaryScenario;
    secondaryScenario.id = "scenario-secondary";
    secondaryScenario.name = "Secondary clock consumer";
    secondaryScenario.duration = 300'000;
    wave::Lane secondaryLane;
    secondaryLane.id = "lane-secondary";
    secondaryLane.name = "secondary";
    secondaryLane.kind = wave::LaneKind::Bit;
    secondaryLane.clockDomainId = "clock-main";
    secondaryScenario.lanes.push_back(std::move(secondaryLane));
    wave::Event secondaryEvent;
    secondaryEvent.id = "event-secondary-cycle";
    secondaryEvent.laneId = "lane-secondary";
    secondaryEvent.tick = 50'000;
    secondaryEvent.action = wave::EventAction::Drive;
    secondaryEvent.value = "0";
    secondaryEvent.clockDomainId = "clock-main";
    secondaryEvent.cycle = 5;
    secondaryScenario.events.push_back(std::move(secondaryEvent));
    project.scenarios.push_back(std::move(secondaryScenario));

    wave::CommandStack clockStack;
    const auto beforeUnchangedClock = project;
    const auto unchangedClock = project.clockDomains.front();
    expect(
        !clockStack.execute(std::make_unique<wave::ChangeClockCommand>(
            project,
            scenario,
            unchangedClock.id,
            unchangedClock)),
        "unchanged clock properties reported an effect");
    expectEqual(project, beforeUnchangedClock, "unchanged clock properties changed the project");
    expectEqual(clockStack.size(), std::size_t{0}, "unchanged clock properties polluted history");

    auto replacement = project.clockDomains.front();
    replacement.period = 18'000;
    expect(
        clockStack.execute(std::make_unique<wave::ChangeClockCommand>(
            project,
            scenario,
            replacement.id,
            replacement)),
        "real clock property change reported no effect");
    expectEqual(
        project.clockDomains.front().period,
        wave::Tick{18'000},
        "clock command did not update the period");
    expectEqual(
        wave::findEvent(scenario, requestEventId)->tick,
        wave::Tick{144'000},
        "cycle-based event did not retain its logical cycle");
    expectEqual(
        wave::findEvent(scenario, requestLowEventId)->tick,
        wave::Tick{234'000},
        "second cycle-based event did not retain its logical cycle");
    expectEqual(
        wave::findEvent(project.scenarios.at(1), "event-secondary-cycle")->tick,
        wave::Tick{90'000},
        "clock change did not retime a cycle event in another scenario");
    const auto* requestLane = wave::findLane(scenario, "lane-request");
    const auto movedSegment = std::find_if(
        requestLane->segments.begin(),
        requestLane->segments.end(),
        [](const wave::Segment& segment) {
            return segment.id == "segment-req-high";
        });
    expect(
        movedSegment != requestLane->segments.end()
            && movedSegment->start == 144'000,
        "retimed cycle event did not move its linked waveform boundary");
    expect(clockStack.undo(), "clock change undo failed");
    expectEqual(
        wave::findEvent(scenario, requestEventId)->tick,
        wave::Tick{80'000},
        "clock change undo did not restore the event tick");
    expectEqual(
        wave::findEvent(project.scenarios.at(1), "event-secondary-cycle")->tick,
        wave::Tick{50'000},
        "clock change undo did not restore another scenario");
    const auto afterClockUndo = project;
    const auto unchangedClockAfterUndo = project.clockDomains.front();
    expect(
        !clockStack.execute(std::make_unique<wave::ChangeClockCommand>(
            project,
            scenario,
            unchangedClockAfterUndo.id,
            unchangedClockAfterUndo)),
        "unchanged clock properties after Undo reported an effect");
    expectEqual(project, afterClockUndo, "unchanged clock properties after Undo changed the project");
    expect(clockStack.canRedo(), "unchanged clock properties discarded the clock Redo branch");
    expect(clockStack.redo(), "clock change redo failed");

    scenario.relations.front().minimumDelay = 18'000;
    scenario.relations.front().maximumDelay = 72'000;
    const auto serialized = wave::serializeProject(project);
    const auto loaded = wave::deserializeProject(serialized);
    expect(loaded.ok(), "project with clock overrides failed to reload");
    expectEqual(
        *loaded.project,
        project,
        "clock overrides changed during serialization round-trip");

    const auto planResult = wave::buildGenerationPlan(project, scenario);
    expect(planResult.ok(), "clock override generation plan contains errors");
    const auto systemVerilog = wave::generateSystemVerilog(*planResult.plan);
    expect(systemVerilog.ok(), "clock override SystemVerilog contains errors");
    expect(
        systemVerilog.text.find("assign clk = __ww_clock_clock_main_disabled")
            != std::string::npos,
        "SystemVerilog clock override mux is missing");
    expect(
        systemVerilog.text.find("clock_overrides_clock_main")
            != std::string::npos,
        "SystemVerilog clock override schedule is missing");
    expect(
        systemVerilog.text.find("#234000;") != std::string::npos,
        "cycle event after a clock override was not scheduled at its model tick");
    expect(
        std::any_of(
            systemVerilog.diagnostics.begin(),
            systemVerilog.diagnostics.end(),
            [](const wave::GenerationDiagnostic& diagnostic) {
                return diagnostic.code
                    == wave::GenerationDiagnosticCode::SvaClockOverrideOverlap;
            }),
        "clock override overlapping an SVA window did not produce a diagnostic");
    const auto assertions = wave::generateSystemVerilogAssertions(*planResult.plan);
    expect(assertions.ok(), "standalone SystemVerilog assertions contain errors");
    expect(
        assertions.text.find("module wave_workbench_assertions_scenario_handshake")
            != std::string::npos,
        "standalone SystemVerilog assertions are not wrapped in a module");

    const auto cocotb = wave::generateCocotb(*planResult.plan);
    expect(cocotb.ok(), "clock override cocotb contains errors");
    expect(
        cocotb.text.find("clock_overrides_clock_main")
            != std::string::npos,
        "cocotb clock override coroutine is missing");
    expect(
        cocotb.text.find("dut.clk.value = \"X\"")
            != std::string::npos,
        "cocotb disabled clock value is missing");
    expect(
        cocotb.text.find("await Timer(234000, unit=\"ps\")")
            != std::string::npos,
        "cocotb cycle event after a clock override was not scheduled at its model tick");

    wave::ExportOptions options;
    options.start = 150'000;
    options.end = 205'000;
    options.width = 900;
    QString error;
    const auto waveDrom = wave::generateWaveDromJson(
        project,
        scenario,
        options,
        &error);
    expect(!waveDrom.isEmpty(), error.toStdString());
    const auto signalValues = QJsonDocument::fromJson(waveDrom)
                                  .object()
                                  .value(QStringLiteral("signal"))
                                  .toArray();
    const auto clockSignal = std::find_if(
        signalValues.begin(),
        signalValues.end(),
        [](const QJsonValue& value) {
            return value.toObject().value(QStringLiteral("name")).toString()
                == QStringLiteral("clk");
        });
    expect(clockSignal != signalValues.end(), "WaveDrom clock signal is missing");
    expect(
        clockSignal->toObject()
            .value(QStringLiteral("wave"))
            .toString()
            .contains(QLatin1Char('x')),
        "WaveDrom did not preserve the disabled clock interval");
}

void testLaneAndGroupPropertyEditing()
{
    auto project = wave::makeDemonstrationProject();
    auto& scenario = project.scenarios.front();
    wave::Lane group;
    group.id = "group-property-test";
    group.name = "Handshake";
    group.kind = wave::LaneKind::Group;
    group.visible = false;

    wave::CommandStack stack;
    stack.execute(std::make_unique<wave::AddLaneCommand>(scenario, group));
    expect(
        wave::findLane(scenario, group.id) != nullptr,
        "group lane was not added");

    const auto* request = wave::findLane(scenario, "lane-request");
    expect(request != nullptr, "request lane is missing");
    const auto originalRequestGroup = request->groupId;
    auto requestReplacement = *request;
    requestReplacement.name = "request_valid";
    requestReplacement.groupId = group.id;
    requestReplacement.color = "#42A5F5";
    requestReplacement.height = 72;
    requestReplacement.visible = false;
    stack.execute(std::make_unique<wave::ChangeLaneCommand>(
        project,
        scenario,
        request->id,
        requestReplacement));
    expectEqual(
        wave::findLane(scenario, "lane-request")->groupId,
        group.id,
        "lane group membership was not updated");
    expectEqual(
        wave::findLane(scenario, "lane-request")->height,
        72,
        "lane display height was not updated");
    expect(stack.undo(), "lane property undo failed");
    expectEqual(
        wave::findLane(scenario, "lane-request")->groupId,
        originalRequestGroup,
        "lane property undo did not restore group membership");
    const auto unchangedRequest = *wave::findLane(scenario, "lane-request");
    expect(
        !stack.execute(std::make_unique<wave::ChangeLaneCommand>(
            project,
            scenario,
            unchangedRequest.id,
            unchangedRequest)),
        "unchanged lane properties after Undo reported an effect");
    expect(stack.canRedo(), "unchanged lane properties discarded the lane Redo branch");
    expectEqual(
        *wave::findLane(scenario, "lane-request"),
        unchangedRequest,
        "unchanged lane properties changed the lane");
    expect(stack.redo(), "lane property redo failed");

    const auto* groupLane = wave::findLane(scenario, group.id);
    expect(groupLane != nullptr, "group lane disappeared");
    auto groupReplacement = *groupLane;
    groupReplacement.name = "Handshake signals";
    stack.execute(std::make_unique<wave::ChangeLaneCommand>(
        project,
        scenario,
        group.id,
        groupReplacement));
    expectEqual(
        wave::findLane(scenario, group.id)->name,
        std::string{"Handshake signals"},
        "group rename failed");
    expectEqual(
        wave::findLane(scenario, "lane-request")->groupId,
        group.id,
        "group rename changed stable membership references");

    const auto beforeShowStateId = stack.stateId();
    expect(
        stack.execute(std::make_unique<wave::ShowHiddenLanesCommand>(scenario)),
        "show hidden lanes command reported no effect");
    expect(
        wave::findLane(scenario, group.id)->visible
            && wave::findLane(scenario, "lane-request")->visible,
        "show hidden lanes command did not restore every hidden lane");
    expect(
        stack.stateId() != beforeShowStateId,
        "show hidden lanes command did not create one history state");
    expect(stack.undo(), "show hidden lanes undo failed");
    expect(
        !wave::findLane(scenario, group.id)->visible
            && !wave::findLane(scenario, "lane-request")->visible,
        "show hidden lanes undo did not atomically restore hidden state");
    expect(stack.redo(), "show hidden lanes redo failed");
    const auto shownStateId = stack.stateId();
    expect(
        !stack.execute(std::make_unique<wave::ShowHiddenLanesCommand>(scenario)),
        "show hidden lanes polluted history when every lane was already visible");
    expectEqual(
        stack.stateId(),
        shownStateId,
        "no-effect show hidden lanes command changed history state");

    const auto beforeHideStateId = stack.stateId();
    expect(
        stack.execute(std::make_unique<wave::HideLaneCommand>(
            scenario,
            "lane-request")),
        "hide lane command reported no effect");
    expect(
        !wave::findLane(scenario, "lane-request")->visible,
        "hide lane command left the signal visible");
    expectEqual(
        stack.undoDescription(),
        std::string{"Hide lane"},
        "hide lane command exposed the wrong Undo description");
    const auto hiddenSignalStateId = stack.stateId();
    expect(
        !stack.execute(std::make_unique<wave::HideLaneCommand>(
            scenario,
            "lane-request")),
        "hiding an already hidden signal reported an effect");
    expectEqual(
        stack.stateId(),
        hiddenSignalStateId,
        "no-effect hide lane command changed history state");
    expect(stack.undo(), "hide lane undo failed");
    expect(
        wave::findLane(scenario, "lane-request")->visible,
        "hide lane undo did not restore the signal");
    expect(stack.redo(), "hide lane redo failed");
    expect(
        !wave::findLane(scenario, "lane-request")->visible,
        "hide lane redo did not hide the signal again");
    expect(stack.undo(), "second hide lane undo failed");
    expectEqual(
        stack.stateId(),
        beforeHideStateId,
        "hide lane Undo did not restore the original history state");

    expect(
        stack.execute(std::make_unique<wave::HideLaneCommand>(
            scenario,
            group.id)),
        "hide group command reported no effect");
    expect(
        !wave::findLane(scenario, group.id)->visible,
        "hide group command left the group visible");
    expectEqual(
        stack.undoDescription(),
        std::string{"Hide group"},
        "hide group command exposed the wrong Undo description");
    expect(stack.undo(), "hide group undo failed");
    expect(
        wave::findLane(scenario, group.id)->visible,
        "hide group undo did not restore the group");

    const auto* data = wave::findLane(scenario, "lane-data");
    expect(data != nullptr, "data lane is missing");
    auto octalData = *data;
    octalData.radix = wave::Radix::Octal;
    stack.execute(std::make_unique<wave::ChangeLaneCommand>(
        project,
        scenario,
        data->id,
        octalData));
    expectEqual(
        wave::findLane(scenario, "lane-data")->radix,
        wave::Radix::Octal,
        "lane octal radix was not updated");

    auto narrowData = *wave::findLane(scenario, "lane-data");
    narrowData.width = 4;
    bool rejectedInvalidWidth = false;
    try {
        [[maybe_unused]] wave::ChangeLaneCommand invalid(
            project,
            scenario,
            data->id,
            narrowData);
    } catch (const std::invalid_argument&) {
        rejectedInvalidWidth = true;
    }
    expect(
        rejectedInvalidWidth,
        "lane width change that invalidates values was not rejected");

    auto nonGroup = *wave::findLane(scenario, group.id);
    nonGroup.kind = wave::LaneKind::Bit;
    bool rejectedReferencedGroupConversion = false;
    try {
        [[maybe_unused]] wave::ChangeLaneCommand invalid(
            project,
            scenario,
            group.id,
            nonGroup);
    } catch (const std::invalid_argument&) {
        rejectedReferencedGroupConversion = true;
    }
    expect(
        rejectedReferencedGroupConversion,
        "referenced group conversion was not rejected");

    wave::Project quickProject;
    quickProject.id = "project-quick-clock";
    quickProject.name = "Quick clock";
    quickProject.timeBase = {1};
    quickProject.scenarios.push_back({
        "scenario-quick-clock",
        "Quick clock",
        100'000,
        {},
        {},
        {},
        {},
        {},
    });
    auto& quickScenario = quickProject.scenarios.front();
    wave::Lane quickClockLane;
    quickClockLane.id = "lane-quick-clock";
    quickClockLane.name = "clk";
    quickClockLane.kind = wave::LaneKind::Clock;
    quickClockLane.clockDomainId = "clock-quick";
    wave::ClockDomain quickClock{
        "clock-quick",
        "clk",
        10'000,
        0,
        {1, 2},
        wave::ClockEdge::Rising,
        {},
        {},
    };
    wave::CommandStack quickStack;
    quickStack.execute(std::make_unique<wave::AddLaneCommand>(
        quickProject,
        quickScenario,
        quickClockLane,
        quickClock));
    expect(wave::findLane(quickScenario, quickClockLane.id), "quick clock lane was not added");
    expect(wave::findClock(quickProject, quickClock.id), "quick clock domain was not added");
    expect(quickStack.undo(), "quick clock add undo failed");
    expect(!wave::findLane(quickScenario, quickClockLane.id), "quick clock lane survived undo");
    expect(!wave::findClock(quickProject, quickClock.id), "quick clock domain survived undo");
    expect(quickStack.redo(), "quick clock add redo failed");

    wave::Lane tailLane;
    tailLane.id = "lane-after-duplicate";
    tailLane.name = "after_duplicate";
    tailLane.kind = wave::LaneKind::Bit;
    quickScenario.lanes.push_back(tailLane);

    auto duplicatedClockLane = *wave::findLane(quickScenario, quickClockLane.id);
    duplicatedClockLane.id = "lane-quick-clock-copy";
    duplicatedClockLane.name = "clk_copy";
    duplicatedClockLane.clockDomainId = "clock-quick-copy";
    auto duplicatedClock = *wave::findClock(quickProject, quickClock.id);
    duplicatedClock.id = duplicatedClockLane.clockDomainId;
    duplicatedClock.name = duplicatedClockLane.name;
    expect(
        quickStack.execute(std::make_unique<wave::DuplicateLaneCommand>(
            quickProject,
            quickScenario,
            duplicatedClockLane,
            duplicatedClock,
            1)),
        "clock duplicate command reported no effect");
    expectEqual(
        quickScenario.lanes.at(1).id,
        duplicatedClockLane.id,
        "clock duplicate was not inserted at the requested adjacent position");
    expect(
        wave::findClock(quickProject, duplicatedClock.id) != nullptr,
        "clock duplicate did not create its independent domain");
    expectEqual(
        quickStack.undoDescription(),
        std::string{"Duplicate lane"},
        "clock duplicate did not expose a specific Undo description");
    expect(quickStack.undo(), "clock duplicate undo failed");
    expect(
        wave::findLane(quickScenario, duplicatedClockLane.id) == nullptr
            && wave::findClock(quickProject, duplicatedClock.id) == nullptr
            && wave::findLane(quickScenario, quickClockLane.id) != nullptr
            && wave::findLane(quickScenario, tailLane.id) != nullptr,
        "clock duplicate undo did not remove only the clone and its domain");
    expect(quickStack.redo(), "clock duplicate redo failed");
    expect(
        quickScenario.lanes.at(1).id == duplicatedClockLane.id
            && wave::findClock(quickProject, duplicatedClock.id) != nullptr,
        "clock duplicate redo did not restore stable IDs and position");

    auto duplicatedBitLane = tailLane;
    duplicatedBitLane.id = "lane-after-duplicate-copy";
    duplicatedBitLane.name = "after_duplicate_copy";
    expect(
        quickStack.execute(std::make_unique<wave::DuplicateLaneCommand>(
            quickScenario,
            duplicatedBitLane,
            quickScenario.lanes.size())),
        "non-clock duplicate command reported no effect");
    expect(
        wave::findLane(quickScenario, duplicatedBitLane.id) != nullptr,
        "non-clock duplicate was not added");
    expect(quickStack.undo(), "non-clock duplicate undo failed");
    expect(
        wave::findLane(quickScenario, duplicatedBitLane.id) == nullptr,
        "non-clock duplicate survived undo");
    expect(quickStack.redo(), "non-clock duplicate redo failed");
    expect(
        wave::findLane(quickScenario, duplicatedBitLane.id) != nullptr,
        "non-clock duplicate redo failed");

    const auto roundTrip = wave::deserializeProject(wave::serializeProject(project));
    expect(roundTrip.ok(), "lane/group property project failed to reload");
    expectEqual(*roundTrip.project, project, "lane/group properties changed on reload");
}

void testLaneAndGroupRemoval()
{
    auto project = wave::makeDemonstrationProject();
    auto& scenario = project.scenarios.front();
    project.importedTraces.push_back({
        "trace-removal-test",
        "trace.vcd",
        "vcd",
        0,
        {
            {"lane-request", "tb.req"},
            {"lane-data", "tb.data"},
        },
        {},
    });
    const auto original = project;

    wave::CommandStack stack;
    stack.execute(std::make_unique<wave::RemoveLaneCommand>(
        project,
        scenario,
        "lane-request"));
    expect(
        wave::findLane(scenario, "lane-request") == nullptr,
        "removed lane remains in the scenario");
    expect(
        std::none_of(
            scenario.events.begin(),
            scenario.events.end(),
            [](const wave::Event& event) {
                return event.laneId == "lane-request";
            }),
        "events owned by a removed lane were retained");
    expect(
        std::none_of(
            scenario.relations.begin(),
            scenario.relations.end(),
            [](const wave::Relation& relation) {
                return relation.id == "relation-req-ack";
            }),
        "relations referencing removed lane events were retained");
    expect(
        !project.importedTraces.front().signalMapping.contains("lane-request"),
        "trace mapping for a removed lane was retained");
    expect(
        project.importedTraces.front().signalMapping.contains("lane-data"),
        "unrelated trace mapping was removed");

    project.importedTraces.push_back({
        "trace-added-after-removal",
        "new-trace.vcd",
        "vcd",
        0,
        {{"lane-data", "tb.new_data"}},
        {},
    });
    expect(stack.undo(), "lane removal undo failed");
    expectEqual(
        project.importedTraces.size(),
        std::size_t{2},
        "lane removal undo discarded a subsequently imported trace");
    project.importedTraces.pop_back();
    expectEqual(project, original, "lane removal undo did not restore the project");
    expect(stack.redo(), "lane removal redo failed");
    expect(
        wave::findLane(scenario, "lane-request") == nullptr,
        "lane removal redo failed");
    expect(stack.undo(), "second lane removal undo failed");

    const auto eventsBeforeGroupRemoval = scenario.events;
    const auto relationsBeforeGroupRemoval = scenario.relations;
    stack.execute(std::make_unique<wave::RemoveLaneCommand>(
        project,
        scenario,
        "group-handshake"));
    expect(
        wave::findLane(scenario, "group-handshake") == nullptr,
        "removed group remains in the scenario");
    expect(
        std::all_of(
            scenario.lanes.begin(),
            scenario.lanes.end(),
            [](const wave::Lane& lane) {
                return lane.groupId != "group-handshake";
            }),
        "group members were not ungrouped");
    expectEqual(
        scenario.events,
        eventsBeforeGroupRemoval,
        "group removal changed member events");
    expectEqual(
        scenario.relations,
        relationsBeforeGroupRemoval,
        "group removal changed member relations");
    expect(stack.undo(), "group removal undo failed");
    expectEqual(project, original, "group removal undo did not restore memberships");
}

void testLaneReordering()
{
    auto project = wave::makeDemonstrationProject();
    auto& scenario = project.scenarios.front();
    const auto original = scenario;
    const auto originalPlan = wave::buildGenerationPlan(project, scenario);
    expect(originalPlan.ok(), "original generation plan is invalid");

    wave::CommandStack stack;
    stack.execute(std::make_unique<wave::MoveLaneCommand>(
        scenario,
        "lane-request",
        0));
    expectEqual(
        scenario.lanes.front().id,
        std::string{"lane-request"},
        "lane did not move to the requested display index");
    expectEqual(
        scenario.events,
        original.events,
        "lane reordering changed scenario events");
    expectEqual(
        scenario.relations,
        original.relations,
        "lane reordering changed scenario relations");

    const auto reorderedPlan = wave::buildGenerationPlan(project, scenario);
    expect(reorderedPlan.ok(), "reordered generation plan is invalid");
    const auto originalSystemVerilog = wave::generateSystemVerilog(*originalPlan.plan);
    const auto reorderedSystemVerilog = wave::generateSystemVerilog(*reorderedPlan.plan);
    const auto originalCocotb = wave::generateCocotb(*originalPlan.plan);
    const auto reorderedCocotb = wave::generateCocotb(*reorderedPlan.plan);
    expectEqual(
        reorderedSystemVerilog.text,
        originalSystemVerilog.text,
        "lane display order changed SystemVerilog behavior");
    expectEqual(
        reorderedCocotb.text,
        originalCocotb.text,
        "lane display order changed cocotb behavior");
    const auto roundTrip = wave::deserializeProject(wave::serializeProject(project));
    expect(roundTrip.ok(), "reordered project failed to reload");
    expectEqual(
        roundTrip.project->scenarios.front().lanes.front().id,
        std::string{"lane-request"},
        "lane display order did not survive serialization");

    expect(stack.undo(), "lane reorder undo failed");
    expectEqual(scenario, original, "lane reorder undo did not restore exact order");
    expect(stack.redo(), "lane reorder redo failed");
    expectEqual(
        scenario.lanes.front().id,
        std::string{"lane-request"},
        "lane reorder redo failed");

    bool rejectedSameIndex = false;
    try {
        [[maybe_unused]] wave::MoveLaneCommand invalid(
            scenario,
            "lane-request",
            0);
    } catch (const std::invalid_argument&) {
        rejectedSameIndex = true;
    }
    expect(rejectedSameIndex, "no-op lane reorder was accepted");

    bool rejectedOutside = false;
    try {
        [[maybe_unused]] wave::MoveLaneCommand invalid(
            scenario,
            "lane-request",
            scenario.lanes.size());
    } catch (const std::invalid_argument&) {
        rejectedOutside = true;
    }
    expect(rejectedOutside, "out-of-range lane reorder was accepted");
}

void testLaneValuesAndSegments()
{
    wave::Lane bit;
    bit.id = "bit";
    bit.name = "valid";
    bit.kind = wave::LaneKind::Bit;
    for (const auto* value : {"0", "1", "X", "Z", "x", "z"}) {
        expect(wave::validateLaneValue(bit, value).valid, "valid 4-state bit was rejected");
    }
    expect(!wave::validateLaneValue(bit, "2").valid, "invalid bit value was accepted");

    wave::setSegmentRange(bit, 0, 10, "0", "s0");
    wave::setSegmentRange(bit, 10, 20, "0", "s1");
    expectEqual(bit.segments.size(), std::size_t{1}, "adjacent equal segments must merge");
    expectEqual(bit.segments.front().end, wave::Tick{20}, "merged interval is incorrect");
    wave::setSegmentRange(bit, 5, 15, "1", "s2");
    expectEqual(bit.segments.size(), std::size_t{3}, "overlay must split the original interval");
    expectEqual(bit.segments.at(0).end, wave::Tick{5}, "left split is incorrect");
    expectEqual(bit.segments.at(1).value, std::string{"1"}, "overlay value is incorrect");
    expectEqual(bit.segments.at(2).start, wave::Tick{15}, "right split is incorrect");
    wave::clearSegmentRange(bit, 7, 17);
    expectEqual(bit.segments.size(), std::size_t{3}, "clear must retain all outside fragments");
    expectEqual(bit.segments.at(1).end, wave::Tick{7}, "clear left boundary is incorrect");
    expectEqual(bit.segments.at(2).start, wave::Tick{17}, "clear right boundary is incorrect");

    wave::Lane bus;
    bus.id = "bus";
    bus.name = "data";
    bus.kind = wave::LaneKind::Bus;
    bus.width = 8;
    expect(wave::validateLaneValue(bus, "0xff").valid, "in-range bus value was rejected");
    expect(!wave::validateLaneValue(bus, "0x1ff").valid, "overflowing bus value was accepted");
    expect(wave::validateLaneValue(bus, "0o377").valid, "in-range octal bus value was rejected");
    expect(!wave::validateLaneValue(bus, "0o400").valid, "overflowing octal bus value was accepted");
    expect(wave::validateLaneValue(bus, "0b10xz").valid, "X/Z bus pattern was rejected");
    expect(!wave::validateLaneValue(bus, "-1").valid, "negative unsigned value was accepted");
    bus.width = 10;
    expect(wave::validateLaneValue(bus, "0x3ff").valid, "10-bit hexadecimal maximum was rejected");
    expect(!wave::validateLaneValue(bus, "0x400").valid, "10-bit hexadecimal overflow was accepted");
    expect(wave::validateLaneValue(bus, "0xXff").valid, "partial high X digit was rejected");
    bus.width = 8;
    bus.isSigned = true;
    expect(wave::validateLaneValue(bus, "-128").valid, "minimum signed value was rejected");
    expect(!wave::validateLaneValue(bus, "-129").valid, "signed underflow was accepted");
    expectEqual(
        wave::laneValueBits(bus, "+1"),
        std::optional<std::string>{"00000001"},
        "explicit positive sign was not normalized");

    wave::Lane enumeration = bus;
    enumeration.kind = wave::LaneKind::Enum;
    enumeration.width = 2;
    enumeration.enumMap = {{"IDLE", "0"}, {"BUSY", "1"}, {"WAIT_ACK", "1"}};
    expect(wave::validateLaneValue(enumeration, "BUSY").valid, "enum symbol was rejected");
    expectEqual(
        wave::laneValueBits(enumeration, "WAIT_ACK"),
        std::optional<std::string>{"01"},
        "enum symbol underscore was treated as a numeric separator");
    expect(!wave::validateLaneValue(enumeration, "MISSING").valid, "unknown enum was accepted");
}

void testUndoRedo()
{
    auto project = wave::makeDemonstrationProject();
    auto& scenario = project.scenarios.front();
    auto* lane = wave::findLane(scenario, "lane-request");
    expect(lane != nullptr, "demonstration request lane is missing");
    const auto before = lane->segments;

    wave::CommandStack stack;
    const auto initialStateId = stack.stateId();
    stack.execute(std::make_unique<wave::SetLaneRangeCommand>(
        scenario,
        lane->id,
        10'000,
        20'000,
        "1"));
    const auto editedStateId = stack.stateId();
    expect(editedStateId != initialStateId, "executed command did not create a new history state");
    expect(stack.canUndo(), "executed command must be undoable");
    expect(lane->segments != before, "command did not change the model");
    const auto after = lane->segments;
    expect(stack.undo(), "undo failed");
    expectEqual(stack.stateId(), initialStateId, "undo did not return to the initial history state");
    expectEqual(lane->segments, before, "undo did not restore the exact model");
    expect(stack.redo(), "redo failed");
    expectEqual(stack.stateId(), editedStateId, "redo did not restore the edited history state");
    expectEqual(lane->segments, after, "redo did not restore the exact edit");
    expectEqual(stack.size(), std::size_t{1}, "one drag-style command must make one history entry");

    expect(stack.undo(), "undo before branch edit failed");
    expect(stack.execute(std::make_unique<wave::SetLaneRangeCommand>(
        scenario,
        lane->id,
        30'000,
        40'000,
        "1")), "branch edit had no effect");
    const auto branchStateId = stack.stateId();
    expect(
        branchStateId != editedStateId,
        "a replacement history branch reused the discarded redo state");
    stack.clear();
    expect(
        stack.stateId() != branchStateId,
        "clearing history did not create a distinct document root state");

    auto noEffectScenario = wave::makeDemonstrationProject().scenarios.front();
    const auto noEffectBefore = noEffectScenario;
    wave::CommandStack noEffectStack;
    const auto noEffectStateId = noEffectStack.stateId();
    const auto singleRangeChanged = noEffectStack.execute(
        std::make_unique<wave::SetLaneRangeCommand>(
            noEffectScenario,
            "lane-request",
            80'000,
            130'000,
            "1"));
    expect(!singleRangeChanged, "identical single-lane write reported an effect");
    expectEqual(
        noEffectScenario,
        noEffectBefore,
        "identical single-lane write replaced stable Segment or Event IDs");
    expectEqual(noEffectStack.size(), std::size_t{0}, "identical write polluted history");
    expectEqual(
        noEffectStack.stateId(),
        noEffectStateId,
        "an identical write changed the history state");
    expect(!noEffectStack.canUndo(), "identical write became undoable");

    const auto batchRangeChanged = noEffectStack.execute(
        std::make_unique<wave::SetLaneRangesCommand>(
            noEffectScenario,
            80'000,
            130'000,
            std::vector<wave::LaneRangeAssignment>{{"lane-request", "1", {}}}));
    expect(!batchRangeChanged, "identical multi-lane write reported an effect");
    expectEqual(
        noEffectScenario,
        noEffectBefore,
        "identical multi-lane write changed the Scenario");
    expectEqual(noEffectStack.size(), std::size_t{0}, "identical batch write polluted history");

    expect(
        !noEffectStack.execute(std::make_unique<wave::ClearLaneRangeCommand>(
            noEffectScenario,
            "lane-clk",
            60'000,
            70'000)),
        "clearing a normal Clock period reported an effect");
    expectEqual(
        noEffectScenario,
        noEffectBefore,
        "clearing a normal Clock period changed the Scenario");
    expectEqual(noEffectStack.size(), std::size_t{0}, "empty Clock clear polluted history");
    expect(
        !noEffectStack.execute(std::make_unique<wave::ClearLaneRangesCommand>(
            noEffectScenario,
            60'000,
            70'000,
            std::vector<std::string>{"lane-clk"})),
        "empty multi-lane clear reported an effect");
    expectEqual(noEffectStack.size(), std::size_t{0}, "empty batch clear polluted history");

    wave::CommandStack clearRedoStack;
    auto clearRedoScenario = wave::makeDemonstrationProject().scenarios.front();
    const auto clearRedoBefore = clearRedoScenario;
    expect(
        clearRedoStack.execute(std::make_unique<wave::ClearLaneRangeCommand>(
            clearRedoScenario,
            "lane-clk",
            170'000,
            180'000)),
        "Clock override clear did not report a real effect");
    const auto clearRedoAfter = clearRedoScenario;
    expect(clearRedoStack.undo(), "Clock override clear undo failed");
    expectEqual(clearRedoScenario, clearRedoBefore, "Clock clear undo was not exact");
    expect(
        !clearRedoStack.execute(std::make_unique<wave::ClearLaneRangeCommand>(
            clearRedoScenario,
            "lane-clk",
            60'000,
            70'000)),
        "empty Clock clear after Undo reported an effect");
    expect(clearRedoStack.canRedo(), "empty Clock clear discarded the redo branch");
    expect(clearRedoStack.redo(), "Clock clear redo was unavailable after empty clear");
    expectEqual(clearRedoScenario, clearRedoAfter, "Clock clear redo changed after empty clear");

    wave::CommandStack redoPreservationStack;
    auto redoPreservationScenario = wave::makeDemonstrationProject().scenarios.front();
    const auto redoPreservationBefore = redoPreservationScenario;
    expect(
        redoPreservationStack.execute(std::make_unique<wave::SetLaneRangeCommand>(
            redoPreservationScenario,
            "lane-request",
            10'000,
            20'000,
            "1")),
        "redo preservation fixture did not change the Scenario");
    const auto redoPreservationAfter = redoPreservationScenario;
    expect(redoPreservationStack.undo(), "redo preservation fixture undo failed");
    expectEqual(
        redoPreservationScenario,
        redoPreservationBefore,
        "redo preservation fixture did not return to its baseline");
    expect(
        !redoPreservationStack.execute(std::make_unique<wave::SetLaneRangeCommand>(
            redoPreservationScenario,
            "lane-request",
            80'000,
            130'000,
            "1")),
        "identical write after Undo reported an effect");
    expect(redoPreservationStack.canRedo(), "identical write discarded the redo branch");
    expectEqual(
        redoPreservationStack.size(),
        std::size_t{1},
        "identical write changed history size after Undo");
    expect(redoPreservationStack.redo(), "redo branch was not usable after identical write");
    expectEqual(
        redoPreservationScenario,
        redoPreservationAfter,
        "redo branch changed after identical write");

    auto relationScenario = wave::makeDemonstrationProject().scenarios.front();
    const auto relationBefore = relationScenario;
    wave::CommandStack relationStack;
    relationStack.execute(std::make_unique<wave::SetLaneRangeCommand>(
        relationScenario,
        "lane-request",
        80'000,
        130'000,
        "0"));
    expect(
        !wave::findRelation(relationScenario, "relation-req-ack"),
        "single-lane range edit left a Relation whose source edge disappeared");
    const auto relationAfter = relationScenario;
    expect(relationStack.undo(), "single-lane relation cleanup undo failed");
    expectEqual(
        relationScenario,
        relationBefore,
        "single-lane range undo did not restore waveform, events, and relations");
    expect(relationStack.redo(), "single-lane relation cleanup redo failed");
    expectEqual(
        relationScenario,
        relationAfter,
        "single-lane range redo did not reproduce relation cleanup exactly");

    auto toggleRelationScenario = wave::makeDemonstrationProject().scenarios.front();
    const auto toggleRelationBefore = toggleRelationScenario;
    wave::CommandStack toggleRelationStack;
    toggleRelationStack.execute(std::make_unique<wave::ToggleBitRangeCommand>(
        toggleRelationScenario,
        "lane-request",
        std::vector<std::pair<wave::Tick, wave::Tick>>{{80'000, 90'000}}));
    expect(
        !wave::findRelation(toggleRelationScenario, "relation-req-ack"),
        "one-beat toggle left a Relation whose source edge disappeared");
    const auto toggleRelationAfter = toggleRelationScenario;
    expect(toggleRelationStack.undo(), "one-beat relation cleanup undo failed");
    expectEqual(
        toggleRelationScenario,
        toggleRelationBefore,
        "one-beat toggle undo did not restore waveform, events, and relations");
    expect(toggleRelationStack.redo(), "one-beat relation cleanup redo failed");
    expectEqual(
        toggleRelationScenario,
        toggleRelationAfter,
        "one-beat toggle redo did not reproduce relation cleanup exactly");

    auto* bus = wave::findLane(scenario, "lane-data");
    expect(bus != nullptr, "demonstration bus lane is missing");
    const auto busBefore = bus->segments;
    wave::JsonExtensions presetExtensions{{
        "waveWorkbench.busPreset",
        "\"dont-care\"",
    }};
    wave::CommandStack presetStack;
    presetStack.execute(std::make_unique<wave::SetLaneRangeCommand>(
        scenario,
        bus->id,
        60'000,
        70'000,
        "0BXXXXXXXX",
        presetExtensions));
    const auto presetSegment = std::find_if(
        bus->segments.begin(),
        bus->segments.end(),
        [](const wave::Segment& segment) {
            return segment.start == 60'000
                && segment.end == 70'000
                && segment.value == "0bxxxxxxxx";
        });
    expect(presetSegment != bus->segments.end(), "preset range did not create an exact segment");
    expectEqual(
        presetSegment->extensions,
        presetExtensions,
        "preset range did not persist its semantic label");
    const auto busAfter = bus->segments;
    expect(presetStack.undo(), "preset range undo failed");
    expectEqual(bus->segments, busBefore, "preset range undo did not restore the bus");
    expect(presetStack.redo(), "preset range redo failed");
    expectEqual(bus->segments, busAfter, "preset range redo lost the semantic label");

    wave::Scenario adjacentScenario;
    adjacentScenario.id = "scenario-adjacent-preset";
    adjacentScenario.name = "Adjacent preset";
    adjacentScenario.duration = 30;
    wave::Lane adjacentBus;
    adjacentBus.id = "bus-adjacent";
    adjacentBus.name = "bus_adjacent";
    adjacentBus.kind = wave::LaneKind::Bus;
    adjacentBus.width = 8;
    adjacentBus.segments = {
        {"left", 0, 10, "0b00000000", {}},
        {"right", 20, 30, "0b00000000", {}},
    };
    adjacentScenario.lanes.push_back(adjacentBus);
    const wave::JsonExtensions reservedExtensions{{
        "waveWorkbench.busPreset",
        "\"reserved\"",
    }};
    wave::CommandStack adjacentStack;
    adjacentStack.execute(std::make_unique<wave::SetLaneRangeCommand>(
        adjacentScenario,
        adjacentBus.id,
        10,
        20,
        "0b00000000",
        reservedExtensions));
    const auto* editedBus = wave::findLane(adjacentScenario, adjacentBus.id);
    expect(editedBus != nullptr, "adjacent preset bus was removed");
    expectEqual(
        editedBus->segments.size(),
        std::size_t{3},
        "preset semantics merged into adjacent ordinary bus values");
    expectEqual(
        editedBus->segments.at(1).extensions,
        reservedExtensions,
        "one-beat preset semantics were not limited to the inserted range");
    expect(
        editedBus->segments.front().extensions.empty()
            && editedBus->segments.back().extensions.empty(),
        "preset semantics leaked into adjacent bus values");
}

void testMultiLaneRangeAssignmentCommand()
{
    auto project = wave::makeDemonstrationProject();
    auto& scenario = project.scenarios.front();
    const auto before = scenario;
    expectEqual(
        before.relations.size(),
        std::size_t{1},
        "demonstration scenario relation fixture is missing");
    const auto expectEventRelationIntegrity = [](const wave::Scenario& candidate) {
        std::vector<std::string> eventIds;
        eventIds.reserve(candidate.events.size());
        for (const auto& event : candidate.events) {
            expect(
                std::find(eventIds.begin(), eventIds.end(), event.id) == eventIds.end(),
                "scenario contains duplicate Event IDs");
            eventIds.push_back(event.id);
            if (!event.waveformLinked) continue;
            const auto* lane = wave::findLane(candidate, event.laneId);
            expect(lane != nullptr, "waveform Event references a missing lane");
            const auto segment = std::find_if(
                lane->segments.begin(),
                lane->segments.end(),
                [&event](const wave::Segment& candidateSegment) {
                    return candidateSegment.id == event.linkedSegmentId;
                });
            expect(
                segment != lane->segments.end(),
                "waveform Event references a missing Segment");
            expect(
                event.tick == segment->start && event.value == segment->value,
                "waveform Event is out of sync with its Segment");
        }
        for (const auto& relation : candidate.relations) {
            expect(
                wave::findEvent(candidate, relation.sourceEventId)
                    && (relation.targetEventId.empty()
                        || wave::findEvent(candidate, relation.targetEventId)),
                "Relation references a missing Event");
        }
    };
    expectEventRelationIntegrity(before);

    wave::CommandStack stack;
    stack.execute(std::make_unique<wave::SetLaneRangesCommand>(
        scenario,
        20'000,
        40'000,
        std::vector<wave::LaneRangeAssignment>{
            {"lane-request", "1", {}},
            {"lane-ack", "1", {}},
        }));
    expectEqual(
        stack.size(),
        std::size_t{1},
        "multi-lane range assignment must create one history entry");
    const auto valueAt = [](const wave::Lane& lane, const wave::Tick tick) {
        const auto iterator = std::find_if(
            lane.segments.begin(),
            lane.segments.end(),
            [tick](const wave::Segment& segment) {
                return segment.start <= tick && tick < segment.end;
            });
        return iterator == lane.segments.end() ? std::string{} : iterator->value;
    };
    const auto* request = wave::findLane(scenario, "lane-request");
    const auto* acknowledge = wave::findLane(scenario, "lane-ack");
    expect(request != nullptr && acknowledge != nullptr, "batch target lanes are missing");
    expectEqual(valueAt(*request, 25'000), std::string{"1"}, "request range was not assigned");
    expectEqual(valueAt(*acknowledge, 35'000), std::string{"1"}, "ack range was not assigned");
    const auto after = scenario;
    expect(stack.undo(), "multi-lane range assignment undo failed");
    expectEqual(scenario, before, "one undo did not restore every assigned lane");
    expect(stack.redo(), "multi-lane range assignment redo failed");
    expectEqual(scenario, after, "one redo did not restore every assigned lane");

    auto clearScenario = before;
    const auto clearedRelationId = clearScenario.relations.front().id;
    wave::CommandStack clearRangesStack;
    clearRangesStack.execute(std::make_unique<wave::ClearLaneRangesCommand>(
        clearScenario,
        80'000,
        150'000,
        std::vector<std::string>{"lane-request", "lane-ack"}));
    expectEqual(
        clearRangesStack.size(),
        std::size_t{1},
        "multi-lane range clear must create one history entry");
    const auto* clearedRequest = wave::findLane(clearScenario, "lane-request");
    const auto* clearedAcknowledge = wave::findLane(clearScenario, "lane-ack");
    expect(
        clearedRequest != nullptr && clearedAcknowledge != nullptr,
        "cleared target lanes disappeared");
    expectEqual(
        valueAt(*clearedRequest, 90'000),
        std::string{},
        "request range was not cleared to its implicit state");
    expectEqual(
        valueAt(*clearedAcknowledge, 120'000),
        std::string{},
        "ack range was not cleared to its implicit state");
    expect(
        !wave::findRelation(clearScenario, clearedRelationId),
        "relation survived after the cleared range removed its referenced edge");
    expectEventRelationIntegrity(clearScenario);
    const auto afterClear = clearScenario;
    expect(clearRangesStack.undo(), "multi-lane range clear undo failed");
    expectEqual(clearScenario, before, "one undo did not restore cleared lanes and relations");
    expect(clearRangesStack.redo(), "multi-lane range clear redo failed");
    expectEqual(clearScenario, afterClear, "one redo did not restore the complete clear result");

    auto singleClearScenario = before;
    wave::CommandStack singleClearStack;
    singleClearStack.execute(std::make_unique<wave::ClearLaneRangeCommand>(
        singleClearScenario,
        "lane-ack",
        100'000,
        120'000));
    expect(
        !wave::findRelation(singleClearScenario, clearedRelationId),
        "single-lane clear did not remove a relation whose edge disappeared");
    expect(singleClearStack.undo(), "single-lane clear undo failed");
    expectEqual(
        singleClearScenario,
        before,
        "single-lane clear undo did not restore the removed relation");

    auto preservedRelationScenario = before;
    const auto relationBefore = preservedRelationScenario.relations.front();
    const auto* targetEventBefore = wave::findEvent(
        preservedRelationScenario,
        relationBefore.targetEventId);
    expect(targetEventBefore != nullptr, "relation target fixture is missing");
    const auto targetSegmentBefore = targetEventBefore->linkedSegmentId;
    wave::CommandStack preservedRelationStack;
    preservedRelationStack.execute(std::make_unique<wave::SetLaneRangesCommand>(
        preservedRelationScenario,
        110'000,
        120'000,
        std::vector<wave::LaneRangeAssignment>{
            {"lane-request", "X", {}},
            {"lane-ack", "X", {}},
        }));
    const auto* preservedRelation = wave::findRelation(
        preservedRelationScenario,
        relationBefore.id);
    const auto* preservedSource = preservedRelation
        ? wave::findEvent(preservedRelationScenario, preservedRelation->sourceEventId)
        : nullptr;
    const auto* preservedTarget = preservedRelation
        ? wave::findEvent(preservedRelationScenario, preservedRelation->targetEventId)
        : nullptr;
    expect(
        preservedRelation && preservedSource && preservedTarget,
        "range assignment did not preserve a relation whose edge ticks still exist");
    expectEqual(
        *preservedRelation,
        relationBefore,
        "range assignment changed an unaffected Relation object");
    expectEventRelationIntegrity(preservedRelationScenario);
    expect(
        preservedTarget->linkedSegmentId != targetSegmentBefore
            && preservedTarget->tick == 110'000,
        "relation target event was not remapped to the replacement segment");
    const auto preservedRelationAfter = preservedRelationScenario;
    expect(
        preservedRelationStack.undo(),
        "relation-preserving range assignment undo failed");
    expectEqual(
        preservedRelationScenario,
        before,
        "relation-preserving range assignment undo was not exact");
    expect(
        preservedRelationStack.redo(),
        "relation-preserving range assignment redo failed");
    expectEqual(
        preservedRelationScenario,
        preservedRelationAfter,
        "relation-preserving range assignment redo was not exact");

    auto removedEdgeScenario = before;
    const auto eventForSegment = [](const wave::Scenario& candidate, const std::string_view id) {
        const auto iterator = std::find_if(
            candidate.events.begin(),
            candidate.events.end(),
            [id](const wave::Event& event) { return event.linkedSegmentId == id; });
        return iterator == candidate.events.end() ? nullptr : &*iterator;
    };
    const auto* resetEvent = eventForSegment(removedEdgeScenario, "segment-reset-high");
    const auto* dataEvent = eventForSegment(removedEdgeScenario, "segment-data-payload");
    expect(resetEvent && dataEvent, "unaffected relation Event fixtures are missing");
    auto sourceRemovedRelation = relationBefore;
    sourceRemovedRelation.id = "relation-source-removed";
    sourceRemovedRelation.sourceEventId = relationBefore.targetEventId;
    sourceRemovedRelation.targetEventId = resetEvent->id;
    auto unaffectedRelation = relationBefore;
    unaffectedRelation.id = "relation-unaffected";
    unaffectedRelation.sourceEventId = resetEvent->id;
    unaffectedRelation.targetEventId = dataEvent->id;
    removedEdgeScenario.relations.push_back(sourceRemovedRelation);
    removedEdgeScenario.relations.push_back(unaffectedRelation);
    const auto removedEdgeBefore = removedEdgeScenario;

    wave::CommandStack removedEdgeStack;
    removedEdgeStack.execute(std::make_unique<wave::SetLaneRangesCommand>(
        removedEdgeScenario,
        80'000,
        150'000,
        std::vector<wave::LaneRangeAssignment>{
            {"lane-request", "X", {}},
            {"lane-ack", "X", {}},
        }));
    expect(
        !wave::findRelation(removedEdgeScenario, relationBefore.id),
        "relation survived after its target waveform edge was removed");
    expect(
        !wave::findRelation(removedEdgeScenario, sourceRemovedRelation.id),
        "relation survived after its source waveform edge was removed");
    const auto* unaffectedAfter = wave::findRelation(
        removedEdgeScenario,
        unaffectedRelation.id);
    expect(unaffectedAfter != nullptr, "unrelated Relation was removed");
    expectEqual(
        *unaffectedAfter,
        unaffectedRelation,
        "unrelated Relation was modified");
    expectEqual(
        removedEdgeScenario.relations.size(),
        std::size_t{1},
        "range assignment removed the wrong number of Relations");
    expectEventRelationIntegrity(removedEdgeScenario);
    const auto removedEdgeAfter = removedEdgeScenario;
    expect(removedEdgeStack.undo(), "removed-edge relation cleanup undo failed");
    expectEqual(
        removedEdgeScenario,
        removedEdgeBefore,
        "removed-edge relation cleanup undo was not exact");
    expect(removedEdgeStack.redo(), "removed-edge relation cleanup redo failed");
    expectEqual(
        removedEdgeScenario,
        removedEdgeAfter,
        "removed-edge relation cleanup redo was not exact");

    auto busScenario = before;
    auto* wideBus = wave::findLane(busScenario, "lane-data");
    expect(wideBus != nullptr, "wide Bus target is missing");
    auto narrowBus = *wideBus;
    narrowBus.id = "lane-data-small";
    narrowBus.name = "data_small";
    narrowBus.width = 4;
    narrowBus.segments.clear();
    busScenario.lanes.push_back(std::move(narrowBus));
    const auto busBefore = busScenario;
    const wave::JsonExtensions dontCare{{
        "waveWorkbench.busPreset",
        "\"dont-care\"",
    }};
    wave::CommandStack busStack;
    busStack.execute(std::make_unique<wave::SetLaneRangesCommand>(
        busScenario,
        20'000,
        40'000,
        std::vector<wave::LaneRangeAssignment>{
            {"lane-data", "0bxxxxxxxx", dontCare},
            {"lane-data-small", "0bxxxx", dontCare},
        }));
    const auto* editedWide = wave::findLane(busScenario, "lane-data");
    const auto* editedNarrow = wave::findLane(busScenario, "lane-data-small");
    expect(editedWide != nullptr && editedNarrow != nullptr, "Bus batch targets disappeared");
    expectEqual(valueAt(*editedWide, 25'000), std::string{"0bxxxxxxxx"},
                "wide Bus preset did not use its own width");
    expectEqual(valueAt(*editedNarrow, 25'000), std::string{"0bxxxx"},
                "narrow Bus preset did not use its own width");
    const auto widePreset = std::find_if(
        editedWide->segments.begin(),
        editedWide->segments.end(),
        [](const wave::Segment& segment) {
            return segment.start <= 25'000 && 25'000 < segment.end;
        });
    const auto narrowPreset = std::find_if(
        editedNarrow->segments.begin(),
        editedNarrow->segments.end(),
        [](const wave::Segment& segment) {
            return segment.start <= 25'000 && 25'000 < segment.end;
        });
    expect(widePreset != editedWide->segments.end()
               && narrowPreset != editedNarrow->segments.end(),
           "Bus preset segments are missing");
    expectEqual(widePreset->extensions, dontCare,
                "wide Bus preset metadata was not preserved");
    expectEqual(narrowPreset->extensions, dontCare,
                "narrow Bus preset metadata was not preserved");
    expectEqual(busStack.size(), std::size_t{1},
                "different-width Bus assignment was not atomic");
    expect(busStack.undo(), "different-width Bus assignment undo failed");
    expectEqual(busScenario, busBefore,
                "different-width Bus assignment undo did not restore all lanes");

    wave::CommandStack invalidBusStack;
    bool invalidBusRejected = false;
    try {
        invalidBusStack.execute(std::make_unique<wave::SetLaneRangesCommand>(
            busScenario,
            50'000,
            60'000,
            std::vector<wave::LaneRangeAssignment>{
                {"lane-data", "0xa5", {}},
                {"lane-data-small", "0xa5", {}},
            }));
    } catch (const std::invalid_argument&) {
        invalidBusRejected = true;
    }
    expect(invalidBusRejected, "value incompatible with one Bus width was accepted");
    expectEqual(busScenario, busBefore,
                "incompatible multi-Bus value caused a partial edit");
    expectEqual(invalidBusStack.size(), std::size_t{0},
                "rejected multi-Bus value polluted command history");

    const auto invalidBefore = scenario;
    wave::CommandStack invalidStack;
    bool rejected = false;
    try {
        invalidStack.execute(std::make_unique<wave::SetLaneRangesCommand>(
            scenario,
            50'000,
            60'000,
            std::vector<wave::LaneRangeAssignment>{
                {"lane-request", "0", {}},
                {"lane-missing", "1", {}},
            }));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    expect(rejected, "missing batch target was accepted");
    expectEqual(scenario, invalidBefore, "failed batch assignment partially modified the scenario");
    expectEqual(
        invalidStack.size(),
        std::size_t{0},
        "failed batch assignment polluted command history");
}

void testCommandStackReplacementAndDuration()
{
    wave::Scenario scenario;
    scenario.id = "scenario-command-replacement";
    scenario.name = "Command replacement";
    scenario.duration = 100;

    wave::Lane provisional;
    provisional.id = "lane-new";
    provisional.name = "bit";
    provisional.kind = wave::LaneKind::Bit;
    wave::CommandStack stack;
    stack.execute(std::make_unique<wave::AddLaneCommand>(scenario, provisional));
    auto completed = provisional;
    completed.name = "valid";
    stack.replaceLast(std::make_unique<wave::AddLaneCommand>(scenario, completed));
    expectEqual(stack.size(), std::size_t{1}, "inline completion created two history entries");
    expect(
        wave::findLane(scenario, provisional.id)
            && wave::findLane(scenario, provisional.id)->name == "valid",
        "replacement command did not apply completed lane details");
    expect(stack.undo(), "completed quick lane undo failed");
    expect(!wave::findLane(scenario, provisional.id), "one undo did not remove the quick lane");
    expect(stack.redo(), "completed quick lane redo failed");
    expect(
        wave::findLane(scenario, provisional.id)
            && wave::findLane(scenario, provisional.id)->name == "valid",
        "quick lane redo lost completed details");
    const auto quickLaneBaseline = stack.size();
    auto interleaved = provisional;
    interleaved.id = "lane-interleaved";
    interleaved.name = "interleaved";
    auto interleavedSecond = provisional;
    interleavedSecond.id = "lane-interleaved-second";
    interleavedSecond.name = "interleaved_second";
    stack.execute(std::make_unique<wave::AddLaneCommand>(scenario, interleaved));
    stack.execute(std::make_unique<wave::AddLaneCommand>(scenario, interleavedSecond));
    expect(
        wave::findLane(scenario, interleaved.id)
            && wave::findLane(scenario, interleavedSecond.id),
        "interleaved commands were not applied");
    expect(
        stack.undoLastAfter(quickLaneBaseline),
        "pending transaction did not undo its latest command");
    expect(
        wave::findLane(scenario, interleaved.id)
            && !wave::findLane(scenario, interleavedSecond.id),
        "transaction recovery crossed more than one later command");
    expectEqual(
        stack.size(),
        quickLaneBaseline + 1,
        "first transaction recovery lost the remaining later command");
    expect(!stack.canRedo(), "transaction recovery left the latest command redoable");
    expect(
        stack.undoLastAfter(quickLaneBaseline),
        "pending transaction did not undo its remaining later command");
    expect(
        !wave::findLane(scenario, interleaved.id),
        "remaining later command survived transaction recovery");
    expectEqual(
        stack.size(),
        quickLaneBaseline,
        "transaction recovery retained redo history");
    expect(
        !stack.undoLastAfter(quickLaneBaseline),
        "transaction recovery crossed its pending-command baseline");
    expect(stack.discardLast(), "discarding the pending quick lane failed");
    expect(!wave::findLane(scenario, provisional.id), "discard did not undo the pending lane");
    expectEqual(stack.size(), std::size_t{0}, "discard retained a history entry");

    wave::Project project;
    project.id = "project-command-replacement";
    project.name = "Clock replacement";
    project.timeBase = {1};
    project.scenarios.push_back({
        "scenario-clock-replacement",
        "Clock replacement",
        100'000,
        {},
        {},
        {},
        {},
        {},
    });
    auto& clockScenario = project.scenarios.front();
    wave::Lane clockLane;
    clockLane.id = "lane-clock-new";
    clockLane.name = "clk";
    clockLane.kind = wave::LaneKind::Clock;
    clockLane.clockDomainId = "clock-new";
    wave::ClockDomain clock{
        "clock-new",
        "clk",
        10'000,
        0,
        {1, 2},
        wave::ClockEdge::Rising,
        {},
        {},
    };
    wave::CommandStack clockStack;
    clockStack.execute(std::make_unique<wave::AddLaneCommand>(
        project,
        clockScenario,
        clockLane,
        clock));
    auto completedClockLane = clockLane;
    completedClockLane.name = "sys_clk";
    auto completedClock = clock;
    completedClock.name = "sys_clk";
    completedClock.period = 20'000;
    clockStack.replaceLast(std::make_unique<wave::AddLaneCommand>(
        project,
        clockScenario,
        completedClockLane,
        completedClock));
    expectEqual(clockStack.size(), std::size_t{1}, "clock completion created two history entries");
    expect(
        wave::findClock(project, clock.id)
            && wave::findClock(project, clock.id)->period == 20'000,
        "clock replacement did not apply the inline period");
    expect(clockStack.undo(), "completed clock undo failed");
    expect(
        !wave::findLane(clockScenario, clockLane.id)
            && !wave::findClock(project, clock.id),
        "completed clock was not removed atomically");
    expect(clockStack.redo(), "completed clock redo failed");
    expect(
        wave::findLane(clockScenario, clockLane.id)
            && wave::findClock(project, clock.id),
        "completed clock was not restored atomically");

    wave::CommandStack durationStack;
    durationStack.execute(std::make_unique<wave::ChangeScenarioDurationCommand>(
        clockScenario,
        500'000));
    expectEqual(clockScenario.duration, wave::Tick{500'000}, "duration command did not apply");
    expect(durationStack.undo(), "duration undo failed");
    expectEqual(clockScenario.duration, wave::Tick{100'000}, "duration undo lost the original end");
    expect(durationStack.redo(), "duration redo failed");
    expectEqual(clockScenario.duration, wave::Tick{500'000}, "duration redo lost the new end");

    bool rejectedInvalidDuration = false;
    try {
        [[maybe_unused]] wave::ChangeScenarioDurationCommand invalid(clockScenario, 0);
    } catch (const std::invalid_argument&) {
        rejectedInvalidDuration = true;
    }
    expect(rejectedInvalidDuration, "non-positive scenario duration was accepted");
}
void testDirectSegmentEditingCommands()
{
    wave::Scenario scenario;
    scenario.id = "scenario-direct-edit";
    scenario.name = "Direct edit";
    scenario.duration = 100;

    wave::Lane bit;
    bit.id = "bit";
    bit.name = "bit";
    bit.kind = wave::LaneKind::Bit;
    bit.segments = {
        {"s0", 0, 20, "0", {}},
        {"s1", 20, 40, "1", {}},
        {"s2", 40, 60, "0", {}},
    };
    wave::Lane bus;
    bus.id = "bus";
    bus.name = "bus";
    bus.kind = wave::LaneKind::Bus;
    bus.width = 8;
    bus.segments = {{"b0", 0, 60, "0x01", {}}};
    scenario.lanes = {bit, bus};
    const auto original = scenario;

    wave::CommandStack stack;
    stack.execute(std::make_unique<wave::EditSegmentCommand>(
        scenario,
        "bit",
        "s1",
        15,
        45,
        "1"));
    auto* editedBit = wave::findLane(scenario, "bit");
    expect(editedBit != nullptr, "edited bit lane is missing");
    expectEqual(editedBit->segments.at(0).end, wave::Tick{15},
                "left boundary edit did not resize the previous segment");
    expectEqual(editedBit->segments.at(1).start, wave::Tick{15},
                "left boundary edit did not move the selected segment");
    expectEqual(editedBit->segments.at(1).end, wave::Tick{45},
                "right boundary edit did not move the selected segment");
    expectEqual(editedBit->segments.at(2).start, wave::Tick{45},
                "right boundary edit did not resize the next segment");
    expectEqual(stack.size(), std::size_t{1},
                "one boundary drag must create one history entry");
    expect(stack.undo(), "segment boundary undo failed");
    expectEqual(scenario, original, "segment boundary undo did not restore the scenario");
    expect(stack.redo(), "segment boundary redo failed");

    stack.execute(std::make_unique<wave::EditSegmentCommand>(
        scenario,
        "bus",
        "b0",
        0,
        60,
        "0x2a"));
    const auto* editedBus = wave::findLane(scenario, "bus");
    expect(editedBus != nullptr && editedBus->segments.size() == 1,
           "bus value edit changed the segment structure");
    expectEqual(editedBus->segments.front().id, std::string{"b0"},
                "bus value edit did not preserve the segment ID");
    expectEqual(editedBus->segments.front().value, std::string{"0x2a"},
                "bus value edit did not normalize the replacement value");
    expect(stack.undo(), "segment value undo failed");

    stack.clear();
    scenario = original;
    stack.execute(std::make_unique<wave::ToggleBitRangeCommand>(
        scenario,
        "bit",
        std::vector<std::pair<wave::Tick, wave::Tick>>{{0, 10}, {20, 30}}));
    editedBit = wave::findLane(scenario, "bit");
    expect(editedBit != nullptr, "toggled bit lane is missing");
    const auto valueAt = [editedBit](const wave::Tick tick) {
        const auto iterator = std::find_if(
            editedBit->segments.begin(),
            editedBit->segments.end(),
            [tick](const wave::Segment& segment) {
                return segment.start <= tick && tick < segment.end;
            });
        return iterator == editedBit->segments.end() ? std::string{} : iterator->value;
    };
    expectEqual(valueAt(5), std::string{"1"}, "first selected beat was not toggled");
    expectEqual(valueAt(15), std::string{"0"}, "unselected beat was modified");
    expectEqual(valueAt(25), std::string{"0"}, "second selected beat was not toggled");
    expectEqual(stack.size(), std::size_t{1},
                "multi-beat toggle must create one history entry");
    expect(stack.undo(), "multi-beat toggle undo failed");
    expectEqual(scenario, original, "multi-beat toggle undo did not restore the scenario");
    expect(stack.redo(), "multi-beat toggle redo failed");
}

void testMultiLanePasteCommand()
{
    auto project = wave::makeDemonstrationProject();
    auto& scenario = project.scenarios.front();

    const auto before = scenario;
    wave::Segment requestSegment;
    requestSegment.start = 0;
    requestSegment.end = 50'000;
    requestSegment.value = "1";
    wave::Segment dataSegment;
    dataSegment.start = 0;
    dataSegment.end = 50'000;
    dataSegment.value = "0x35";
    dataSegment.extensions["clipboardMetadata"] = R"("preserved")";
    std::vector<wave::CopiedLaneRange> copied{
        {"lane-request", {requestSegment}},
        {"lane-data", {dataSegment}},
    };
    wave::CommandStack stack;
    stack.execute(std::make_unique<wave::PasteRangeCommand>(
        scenario,
        copied,
        150'000,
        50'000));
    expectEqual(stack.size(), std::size_t{1}, "multi-lane paste must create one undo command");
    const auto* request = wave::findLane(scenario, "lane-request");
    const auto* data = wave::findLane(scenario, "lane-data");
    expect(request != nullptr && data != nullptr, "paste target lane is missing");
    const auto requestValue = std::find_if(
        request->segments.begin(),
        request->segments.end(),
        [](const wave::Segment& segment) {
            return segment.start <= 160'000 && 160'000 < segment.end;
        });
    const auto dataValue = std::find_if(
        data->segments.begin(),
        data->segments.end(),
        [](const wave::Segment& segment) {
            return segment.start <= 160'000 && 160'000 < segment.end;
        });
    expect(
        requestValue != request->segments.end() && requestValue->value == "1",
        "pasted bit value is incorrect");
    expect(
        dataValue != data->segments.end() && dataValue->value == "0x35",
        "pasted bus value is incorrect");
    expect(
        dataValue->extensions.contains("clipboardMetadata"),
        "pasted segment extensions were not preserved");
    const auto after = scenario;
    expect(stack.undo(), "paste undo failed");
    expectEqual(scenario, before, "paste undo did not restore the complete scenario");
    expect(stack.redo(), "paste redo failed");
    expectEqual(scenario, after, "paste redo did not restore the complete pasted range");

    auto targetScenario = wave::makeDemonstrationProject().scenarios.front();
    wave::Lane emptyTarget;
    emptyTarget.id = "lane-empty-target";
    emptyTarget.name = "empty_target";
    emptyTarget.kind = wave::LaneKind::Bit;
    emptyTarget.color = "#90caf9";
    targetScenario.lanes.push_back(emptyTarget);
    const auto targetBefore = targetScenario;
    std::vector<wave::CopiedLaneRange> emptyCopied{
        {emptyTarget.id, {}},
    };
    wave::CommandStack targetStack;
    expect(
        !targetStack.execute(std::make_unique<wave::PasteRangeCommand>(
            targetScenario,
            emptyCopied,
            20'000,
            10'000)),
        "empty Paste into an implicit range reported an effect");
    expectEqual(targetScenario, targetBefore, "empty Paste changed the target Scenario");
    expectEqual(targetStack.size(), std::size_t{0}, "empty Paste polluted history");

    wave::Segment targetSegment;
    targetSegment.start = 0;
    targetSegment.end = 10'000;
    targetSegment.value = "1";
    std::vector<wave::CopiedLaneRange> targetCopied{
        {emptyTarget.id, {targetSegment}},
    };
    expect(
        targetStack.execute(std::make_unique<wave::PasteRangeCommand>(
            targetScenario,
            targetCopied,
            40'000,
            10'000)),
        "real Paste into another lane reported no effect");
    const auto targetAfter = targetScenario;
    const auto* pastedTargetLane = wave::findLane(targetScenario, emptyTarget.id);
    const auto pastedTargetSegment = pastedTargetLane
        ? std::find_if(
              pastedTargetLane->segments.begin(),
              pastedTargetLane->segments.end(),
              [](const wave::Segment& segment) {
                  return segment.start <= 45'000 && 45'000 < segment.end;
              })
        : std::vector<wave::Segment>::const_iterator{};
    expect(
        pastedTargetLane
            && pastedTargetSegment != pastedTargetLane->segments.end()
            && pastedTargetSegment->value == "1",
        "target-aware Paste command wrote the wrong lane value");
    expect(targetStack.undo(), "target-aware Paste undo failed");
    expectEqual(targetScenario, targetBefore, "target-aware Paste undo was not exact");
    expect(
        !targetStack.execute(std::make_unique<wave::PasteRangeCommand>(
            targetScenario,
            emptyCopied,
            20'000,
            10'000)),
        "empty Paste after Undo reported an effect");
    expect(targetStack.canRedo(), "empty Paste discarded the real Paste Redo branch");
    expect(targetStack.redo(), "real Paste Redo was unavailable after empty Paste");
    expectEqual(targetScenario, targetAfter, "real Paste Redo changed after empty Paste");

    auto endScenario = wave::makeDemonstrationProject().scenarios.front();
    const auto endBefore = endScenario;
    const auto originalEnd = endScenario.duration;
    wave::Segment endSegment;
    endSegment.start = 0;
    endSegment.end = 10'000;
    endSegment.value = "1";
    std::vector<wave::CopiedLaneRange> endCopied{
        {"lane-request", {endSegment}},
    };
    wave::CommandStack endStack;
    expect(
        endStack.execute(std::make_unique<wave::PasteRangeCommand>(
            endScenario,
            endCopied,
            originalEnd,
            10'000)),
        "Paste at End did not extend the Scenario");
    expectEqual(
        endScenario.duration,
        originalEnd + 10'000,
        "Paste at End did not preserve the complete copied duration");
    const auto* extendedLane = wave::findLane(endScenario, "lane-request");
    const auto extendedValue = extendedLane
        ? std::find_if(
              extendedLane->segments.begin(),
              extendedLane->segments.end(),
              [originalEnd](const wave::Segment& segment) {
                  return segment.start <= originalEnd + 5'000
                      && originalEnd + 5'000 < segment.end;
              })
        : std::vector<wave::Segment>::const_iterator{};
    expect(
        extendedLane
            && extendedValue != extendedLane->segments.end()
            && extendedValue->value == "1",
        "Paste at End truncated the copied waveform");
    const auto endAfter = endScenario;
    expect(endStack.undo(), "extended Paste undo failed");
    expectEqual(endScenario, endBefore, "extended Paste undo did not restore End and waveform");
    expect(endStack.redo(), "extended Paste redo failed");
    expectEqual(endScenario, endAfter, "extended Paste redo did not restore End and waveform");
}

void testEventSegmentSynchronization()
{
    auto project = wave::makeDemonstrationProject();
    auto& scenario = project.scenarios.front();
    auto* requestLane = wave::findLane(scenario, "lane-request");
    expect(requestLane != nullptr, "request lane is missing");
    auto eventIterator = std::find_if(
        scenario.events.begin(),
        scenario.events.end(),
        [](const wave::Event& event) {
            return event.linkedSegmentId == "segment-req-high";
        });
    expect(eventIterator != scenario.events.end(), "waveform segment event was not synchronized");
    const auto eventId = eventIterator->id;

    auto replacement = *eventIterator;
    replacement.tick = 90'000;
    replacement.value = "X";
    wave::CommandStack stack;
    stack.execute(std::make_unique<wave::ChangeEventCommand>(
        scenario,
        eventId,
        replacement));
    const auto* changedEvent = wave::findEvent(scenario, eventId);
    expect(changedEvent != nullptr, "changed linked event lost its stable ID");
    expectEqual(changedEvent->tick, wave::Tick{90'000}, "event time was not updated");
    expectEqual(changedEvent->value, std::string{"X"}, "event value was not updated");
    const auto* changedSegment = std::find_if(
        requestLane->segments.begin(),
        requestLane->segments.end(),
        [&changedEvent](const wave::Segment& segment) {
            return segment.id == changedEvent->linkedSegmentId;
        }) != requestLane->segments.end()
        ? &*std::find_if(
            requestLane->segments.begin(),
            requestLane->segments.end(),
            [&changedEvent](const wave::Segment& segment) {
                return segment.id == changedEvent->linkedSegmentId;
            })
        : nullptr;
    expect(changedSegment != nullptr, "linked segment is missing after event edit");
    expectEqual(changedSegment->start, wave::Tick{90'000}, "event time did not move the waveform edge");
    expectEqual(changedSegment->value, std::string{"X"}, "event value did not update the segment");
    expect(stack.undo(), "event edit undo failed");
    expectEqual(
        wave::findEvent(scenario, eventId)->tick,
        wave::Tick{80'000},
        "event undo did not restore the original tick");

    const auto eventsBeforeCanvasEdit = scenario.events;
    requestLane = wave::findLane(scenario, "lane-request");
    stack.execute(std::make_unique<wave::SetLaneRangeCommand>(
        scenario,
        requestLane->id,
        20'000,
        30'000,
        "1"));
    const auto synchronized = std::find_if(
        scenario.events.begin(),
        scenario.events.end(),
        [](const wave::Event& event) {
            return event.waveformLinked && event.tick == 20'000 && event.value == "1";
        });
    expect(
        synchronized != scenario.events.end(),
        "canvas-style range edit did not update the shared event model");
    expect(stack.undo(), "canvas range undo failed");
    expectEqual(
        scenario.events,
        eventsBeforeCanvasEdit,
        "canvas range undo did not restore the event table model");

    const auto linkedRemovalBefore = scenario;
    expect(
        wave::findRelation(scenario, "relation-req-ack") != nullptr,
        "linked Event relation fixture is missing");
    stack.clear();
    stack.execute(std::make_unique<wave::RemoveEventCommand>(scenario, eventId));
    expect(!wave::findEvent(scenario, eventId), "linked Event was not removed");
    expect(
        !wave::findRelation(scenario, "relation-req-ack"),
        "linked Event removal left its Relation dangling");
    const auto linkedRemovalAfter = scenario;
    expect(stack.undo(), "linked Event removal undo failed");
    expectEqual(
        scenario,
        linkedRemovalBefore,
        "linked Event removal undo did not restore waveform and Relation");
    expect(stack.redo(), "linked Event removal redo failed");
    expectEqual(
        scenario,
        linkedRemovalAfter,
        "linked Event removal redo did not restore dependency cleanup");

    auto ordinaryScenario = linkedRemovalBefore;
    wave::Event noteEvent;
    noteEvent.id = "event-note";
    noteEvent.tick = 10'000;
    noteEvent.action = wave::EventAction::Note;
    noteEvent.description = "Manual note";
    ordinaryScenario.events.push_back(noteEvent);
    auto noteRelation = ordinaryScenario.relations.front();
    noteRelation.id = "relation-note";
    noteRelation.sourceEventId = noteEvent.id;
    ordinaryScenario.relations.push_back(noteRelation);
    const auto ordinaryRemovalBefore = ordinaryScenario;
    wave::CommandStack ordinaryStack;
    ordinaryStack.execute(std::make_unique<wave::RemoveEventCommand>(
        ordinaryScenario,
        noteEvent.id));
    expect(!wave::findEvent(ordinaryScenario, noteEvent.id), "ordinary Event was not removed");
    expect(
        !wave::findRelation(ordinaryScenario, noteRelation.id),
        "ordinary Event removal left its Relation dangling");
    expect(
        wave::findRelation(ordinaryScenario, "relation-req-ack") != nullptr,
        "ordinary Event removal deleted an unrelated Relation");
    const auto ordinaryRemovalAfter = ordinaryScenario;
    expect(ordinaryStack.undo(), "ordinary Event removal undo failed");
    expectEqual(
        ordinaryScenario,
        ordinaryRemovalBefore,
        "ordinary Event removal undo was not exact");
    expect(ordinaryStack.redo(), "ordinary Event removal redo failed");
    expectEqual(
        ordinaryScenario,
        ordinaryRemovalAfter,
        "ordinary Event removal redo was not exact");
}

void testMarkersRelationsAndValidation()
{
    auto project = wave::makeDemonstrationProject();
    auto& scenario = project.scenarios.front();
    const auto initialMarkerCount = scenario.markers.size();
    wave::CommandStack stack;
    stack.execute(std::make_unique<wave::AddMarkerCommand>(
        scenario,
        wave::Marker{
            "marker-test",
            "Transfer window",
            80'000,
            150'000,
            wave::MarkerKind::Phase,
            "test",
            {},
        }));
    expectEqual(
        scenario.markers.size(),
        initialMarkerCount + 1,
        "marker command did not add a marker");
    expect(stack.undo(), "marker undo failed");
    expectEqual(scenario.markers.size(), initialMarkerCount, "marker undo did not restore the model");
    expect(stack.redo(), "marker redo failed");

    const auto findTestMarker = [&scenario] {
        return std::find_if(
            scenario.markers.begin(),
            scenario.markers.end(),
            [](const wave::Marker& marker) {
                return marker.id == "marker-test";
            });
    };
    auto marker = findTestMarker();
    expect(marker != scenario.markers.end(), "added marker is missing");
    auto movedMarker = *marker;
    movedMarker.start = 90'000;
    movedMarker.end = 160'000;
    stack.execute(std::make_unique<wave::ChangeMarkerCommand>(
        scenario,
        movedMarker.id,
        movedMarker));
    marker = findTestMarker();
    expect(
        marker != scenario.markers.end()
            && marker->start == 90'000
            && marker->end == 160'000,
        "marker change command did not move the interval");
    expect(stack.undo(), "marker change undo failed");
    marker = findTestMarker();
    expect(
        marker != scenario.markers.end()
            && marker->start == 80'000
            && marker->end == 150'000,
        "marker change undo did not restore the interval");
    expect(stack.redo(), "marker change redo failed");
    marker = findTestMarker();
    expect(
        marker != scenario.markers.end()
            && marker->start == 90'000
            && marker->end == 160'000,
        "marker change redo did not restore the moved interval");

    stack.execute(std::make_unique<wave::RemoveMarkerCommand>(
        scenario,
        "marker-test"));
    expectEqual(
        scenario.markers.size(),
        initialMarkerCount,
        "marker removal command did not remove the marker");
    expect(stack.undo(), "marker removal undo failed");
    marker = findTestMarker();
    expect(
        marker != scenario.markers.end()
            && marker->start == 90'000
            && marker->end == 160'000,
        "marker removal undo did not restore the marker");
    expect(stack.redo(), "marker removal redo failed");
    expectEqual(
        scenario.markers.size(),
        initialMarkerCount,
        "marker removal redo did not remove the marker");
    expect(stack.undo(), "marker removal final restore failed");

    auto issues = wave::validateScenario(project, scenario);
    expect(
        std::any_of(issues.begin(), issues.end(), [](const wave::ValidationIssue& issue) {
            return issue.code == wave::ValidationCode::RelationSatisfied;
        }),
        "satisfied relation was not reported");

    auto violated = scenario;
    violated.relations.front().maximumDelay = 20'000;
    issues = wave::validateScenario(project, violated);
    expect(
        std::any_of(issues.begin(), issues.end(), [](const wave::ValidationIssue& issue) {
            return issue.code == wave::ValidationCode::RelationViolated;
        }),
        "relation max-delay violation was not reported");

    auto missing = scenario;
    missing.relations.front().targetEventId = "missing-event";
    issues = wave::validateScenario(project, missing);
    expect(
        std::any_of(issues.begin(), issues.end(), [](const wave::ValidationIssue& issue) {
            return issue.code == wave::ValidationCode::MissingTargetEvent;
        }),
        "missing relation target was not reported");

    auto undefined = scenario;
    auto* request = wave::findLane(undefined, "lane-request");
    wave::clearSegmentRange(*request, 10'000, 20'000);
    issues = wave::validateScenario(project, undefined);
    expect(
        std::any_of(issues.begin(), issues.end(), [](const wave::ValidationIssue& issue) {
            return issue.code == wave::ValidationCode::UndefinedRegion
                && issue.laneId == "lane-request";
        }),
        "undefined waveform interval was not reported");
}

void testCodeGeneration()
{
    auto project = wave::makeDemonstrationProject();
    auto& scenario = project.scenarios.front();
    const auto requestEvent = std::find_if(
        scenario.events.begin(),
        scenario.events.end(),
        [](const wave::Event& event) {
            return event.linkedSegmentId == "segment-req-high";
        });
    expect(requestEvent != scenario.events.end(), "request event is missing");
    requestEvent->cycle = 8;
    requestEvent->clockDomainId = "clock-main";
    const auto acknowledgeEvent = std::find_if(
        scenario.events.begin(),
        scenario.events.end(),
        [](const wave::Event& event) {
            return event.linkedSegmentId == "segment-ack-high";
        });
    expect(acknowledgeEvent != scenario.events.end(), "acknowledge event is missing");
    acknowledgeEvent->action = wave::EventAction::Expect;

    const auto planResult = wave::buildGenerationPlan(project, scenario);
    expect(planResult.ok(), "generation plan contains errors");
    const auto& plan = *planResult.plan;
    const auto systemVerilog = wave::generateSystemVerilog(plan);
    expect(systemVerilog.ok(), "SystemVerilog generation contains errors");
    expect(
        systemVerilog.text.find("timeunit 1ps") != std::string::npos,
        "SystemVerilog time unit is missing");
    expect(
        systemVerilog.text.find("repeat (9) @(posedge clk)") != std::string::npos,
        "cycle-based event did not use its clock domain");
    expect(
        systemVerilog.text.find("if (ack !== 1'b1)") != std::string::npos,
        "expected-value check is missing");
    expect(
        systemVerilog.text.find("$rose(req) |-> ##[1:4] $rose(ack)") != std::string::npos,
        "lossless relation SVA is missing");
    expect(
        systemVerilog.text.find("|| $isunknown(clk)") != std::string::npos,
        "disabled clock intervals were not included in SVA disable semantics");
    expect(
        systemVerilog.text.find("timeout_guard") != std::string::npos,
        "SystemVerilog timeout protection is missing");

    const auto cocotb = wave::generateCocotb(plan);
    expect(cocotb.ok(), "cocotb generation contains errors");
    expect(
        cocotb.text.find("await ClockCycles(dut.clk, 9, rising=True)") != std::string::npos,
        "cocotb cycle event did not use ClockCycles");
    expect(
        cocotb.text.find("with_timeout(Combine(*tasks)") != std::string::npos,
        "cocotb timeout is missing");
    expect(
        cocotb.text.find("assert int(dut.ack.value) == 1") != std::string::npos,
        "cocotb expected check is missing");
    expect(
        cocotb.text.find("dut.data.value = 0x35") != std::string::npos,
        "hexadecimal cocotb drive was emitted as an X/Z string or wrong HDL identifier");

    auto reordered = scenario;
    std::reverse(reordered.lanes.begin(), reordered.lanes.end());
    std::reverse(reordered.events.begin(), reordered.events.end());
    const auto reorderedPlan = wave::buildGenerationPlan(project, reordered);
    expect(reorderedPlan.ok(), "reordered generation plan contains errors");
    expectEqual(
        wave::generateSystemVerilog(*reorderedPlan.plan).text,
        systemVerilog.text,
        "generated behavior depends on UI lane/event order");

    auto conditionedPlan = plan;
    conditionedPlan.relations.front().condition = "enable";
    const auto conditionedAssertions = wave::generateSystemVerilogAssertions(conditionedPlan);
    expect(
        conditionedAssertions.text.find("property p_") == std::string::npos,
        "runtime relation condition produced approximate SVA");
    expect(
        std::any_of(
            conditionedAssertions.diagnostics.begin(),
            conditionedAssertions.diagnostics.end(),
            [](const wave::GenerationDiagnostic& diagnostic) {
                return diagnostic.code
                    == wave::GenerationDiagnosticCode::SvaUnsupportedCondition;
            }),
        "unsupported SVA condition did not produce a diagnostic");
}

void testWaveformExports()
{
    const auto project = wave::makeDemonstrationProject();
    const auto& scenario = project.scenarios.front();
    wave::ExportOptions options;
    options.start = 80'000;
    options.end = 150'000;
    options.width = 1000;
    options.pngDpi = 144;
    options.pdfPageSpanTicks = 30'000;

    QString error;
    const auto svg = wave::renderWaveformSvg(project, scenario, options, &error);
    expect(!svg.isEmpty(), error.toStdString());
    expect(svg.contains("<svg"), "SVG export is not an SVG document");
    expect(svg.contains("data[7:0]"), "SVG export cropped or omitted a signal name");

    const auto png = wave::renderWaveformPng(project, scenario, options, &error);
    expect(!png.isEmpty(), error.toStdString());
    expect(png.startsWith("\x89PNG\r\n\x1a\n"), "PNG signature is invalid");
    const auto image = QImage::fromData(png, "PNG");
    expect(!image.isNull(), "PNG cannot be decoded");
    expect(image.width() > options.width, "high-DPI PNG did not scale pixel dimensions");
    expect(
        image.pixelColor(image.width() / 2, image.height() / 2) != QColor(198, 40, 40),
        "relation brush leaked into the document background");

    const auto pdf = wave::renderWaveformPdf(project, scenario, options, &error);
    expect(!pdf.isEmpty(), error.toStdString());
    expect(pdf.startsWith("%PDF-"), "PDF signature is invalid");

    const auto waveDrom = wave::generateWaveDromJson(project, scenario, options, &error);
    expect(!waveDrom.isEmpty(), error.toStdString());
    const auto waveDromDocument = QJsonDocument::fromJson(waveDrom);
    expect(waveDromDocument.isObject(), "WaveDrom JSON is invalid");
    expectEqual(
        waveDromDocument.object()
            .value(QStringLiteral("waveWorkbench"))
            .toObject()
            .value(QStringLiteral("startTick"))
            .toString(),
        QStringLiteral("80000"),
        "WaveDrom export range is incorrect");

    const auto bundle = wave::generateArtifactBundle(project, scenario, options);
    expect(bundle.ok(), bundle.error.toStdString());
    QTemporaryDir directory;
    expect(directory.isValid(), "artifact output directory creation failed");
    expect(
        wave::writeArtifactBundleAtomic(
            *bundle.artifacts,
            directory.path(),
            QStringLiteral("handshake"),
            &error),
        error.toStdString());
    for (const auto& file : {
             QStringLiteral("handshake_tb.sv"),
             QStringLiteral("handshake_assertions.sv"),
             QStringLiteral("test_handshake.py"),
             QStringLiteral("handshake.svg"),
             QStringLiteral("handshake.png"),
             QStringLiteral("handshake.pdf"),
             QStringLiteral("handshake.wavedrom.json"),
         }) {
        expect(
            QFileInfo::exists(QDir(directory.path()).filePath(file)),
            ("artifact file is missing: " + file).toStdString());
    }
}

void testSerializationRoundTrip()
{
    auto project = wave::makeDemonstrationProject();
    project.extensions.emplace("futureRoot", R"({"enabled":true,"level":3})");
    project.scenarios.front().lanes.front().extensions.emplace("futureLane", R"(["a",2])");
    project.clockDomains.front().extensions.emplace("futureClock", R"("preserved")");
    project.exportSettings.emplace("dpi", "300");

    const auto encoded = wave::serializeProject(project);
    const auto result = wave::deserializeProject(encoded);
    expect(result.ok(), "serialized project failed to load");
    expectEqual(*result.project, project, "serialization round-trip changed the model");

    const auto encodedAgain = wave::serializeProject(*result.project);
    const auto json = QJsonDocument::fromJson(encodedAgain).object();
    expect(json.contains(QStringLiteral("futureRoot")), "unknown root field was discarded");
    const auto lane = json.value(QStringLiteral("scenarios"))
                          .toArray()
                          .first()
                          .toObject()
                          .value(QStringLiteral("lanes"))
                          .toArray()
                          .first()
                          .toObject();
    expect(lane.contains(QStringLiteral("futureLane")), "unknown lane field was discarded");
}

void testSchemaMigration()
{
    const auto legacy = QByteArrayLiteral(R"JSON(
{
  "schemaVersion": 0,
  "id": "legacy-project",
  "name": "Legacy",
  "timebase": { "baseUnitPs": "1" },
  "scenarios": [{
    "name": "Legacy scenario",
    "duration": "100",
    "lanes": [{
      "displayName": "ready",
      "kind": "bit",
      "segments": [{"start": "0", "end": "100", "value": "1"}]
    }]
  }]
}
)JSON");
    const auto result = wave::deserializeProject(legacy);
    expect(result.ok(), "schema 0 migration failed");
    expect(result.migrated, "migration flag was not reported");
    expectEqual(result.project->schemaVersion, 1, "schema version was not upgraded");
    expect(!result.project->scenarios.front().id.empty(), "scenario ID was not generated");
    expect(!result.project->scenarios.front().lanes.front().id.empty(), "lane ID was not generated");
    expect(
        !result.project->scenarios.front().lanes.front().segments.front().id.empty(),
        "segment ID was not generated");
}

void testProjectDirectoryMove()
{
    QTemporaryDir temporary;
    expect(temporary.isValid(), "temporary directory creation failed");
    const auto source = QDir(temporary.path()).filePath(QStringLiteral("source"));
    const auto destination = QDir(temporary.path()).filePath(QStringLiteral("moved"));
    auto project = wave::makeDemonstrationProject();
    project.linkedResources.push_back({
        "frame-sample",
        "traces/reference.vcd",
        "sample-1",
        "sha256:abc",
        "relative resource",
        {},
    });
    QString error;
    expect(wave::saveProjectDirectory(project, source, &error), error.toStdString());
    expect(QDir(temporary.path()).rename(QStringLiteral("source"), QStringLiteral("moved")), "directory move failed");
    const auto result = wave::loadProjectDirectory(destination);
    expect(result.ok(), result.error.toStdString());
    expectEqual(
        result.project->linkedResources.front().path,
        std::string{"traces/reference.vcd"},
        "relative resource path changed after moving the project");
}

void testVcdImportAndIndex()
{
    std::istringstream input(R"VCD(
$date 2026-07-24 $end
$version Wave Workbench import test $end
$timescale 1 ns $end
$scope module top $end
$var wire 1 ! clk $end
$var wire 1 " req $end
$var wire 8 # data [7:0] $end
$upscope $end
$enddefinitions $end
$dumpvars
0!
0"
b00000000 #
$end
#5
1!
1"
#10
b00110101 #
#20
0"
)VCD");
    wave::TraceParseOptions options;
    options.projectTimeBase = {1};
    options.identity = {"project", "trace", 7};
    options.offset = 100;
    const auto result = wave::parseVcd(input, options);
    expect(result.ok(), result.errorSummary());
    expectEqual(result.index->identity, options.identity, "trace identity changed during parsing");
    expectEqual(result.index->traceSignals.size(), std::size_t{3}, "VCD signal count is incorrect");
    expectEqual(result.index->transitionCount, std::uint64_t{7}, "VCD transition count is incorrect");
    const auto* request = result.index->findSignal("top.req");
    expect(request != nullptr, "hierarchical VCD signal ID is missing");
    expectEqual(request->transitions.front().tick, wave::Tick{100}, "VCD offset was not applied");
    expectEqual(request->transitions.at(1).tick, wave::Tick{5'100}, "VCD time conversion is incorrect");
    expectEqual(request->valueAt(6'000)->value, std::string{"1"}, "indexed value lookup is incorrect");
    const auto visible = request->visibleTransitions(6'000, 21'000);
    expectEqual(visible.size(), std::size_t{2}, "visible range must include the preceding held value");
    expectEqual(visible.front().tick, wave::Tick{5'100}, "visible range preceding transition is incorrect");

    auto project = wave::makeDemonstrationProject();
    const auto mapping = wave::suggestSignalMapping(project.scenarios.front(), *result.index);
    expectEqual(mapping.at("lane-request"), std::string{"top.req"}, "exact signal mapping failed");

    std::istringstream inexact(R"VCD(
$timescale 1 fs $end
$scope module top $end
$var wire 1 ! clk $end
$upscope $end
$enddefinitions $end
#1
1!
)VCD");
    options.offset = 0;
    const auto inexactResult = wave::parseVcd(inexact, options);
    expect(!inexactResult.ok(), "inexact femtosecond timestamp must be rejected");
    expect(
        inexactResult.errorSummary().find("exactly") != std::string::npos,
        "inexact timestamp diagnostic is missing");
}

void testCsvImportAndCancellation()
{
    std::istringstream input(
        "time[ns],req,\"data[7:0]\"\n"
        "0,0,0x00\n"
        "5,1,0x35\n"
        "10,1,0x35\n"
        "20,0,0x00\n");
    wave::TraceParseOptions options;
    options.projectTimeBase = {1};
    options.identity = {"project", "csv-trace", 9};
    const auto result = wave::parseCsv(input, options);
    expect(result.ok(), result.errorSummary());
    expectEqual(result.index->traceSignals.size(), std::size_t{2}, "CSV signal count is incorrect");
    expectEqual(result.index->transitionCount, std::uint64_t{6}, "redundant CSV values were not compacted");
    const auto* data = result.index->findSignal("data[7:0]");
    expect(data != nullptr, "CSV bus signal is missing");
    expectEqual(data->width, std::uint32_t{8}, "CSV hexadecimal bus width inference is incorrect");
    expectEqual(data->transitions.at(1).tick, wave::Tick{5'000}, "CSV unit conversion is incorrect");

    std::ostringstream large;
    large << "time,value\n";
    for (int index = 0; index < 5'000; ++index) {
        large << index << ',' << (index & 1) << '\n';
    }
    std::istringstream cancellable(large.str());
    options.csvTimeUnit = wave::CsvTimeUnit::ProjectTick;
    options.isCancelled = [] { return true; };
    const auto cancelled = wave::parseCsv(cancellable, options);
    expect(cancelled.cancelled, "CSV parser did not honor cancellation");
    expect(!cancelled.index.has_value(), "cancelled parse returned a partial index");
}

void testTraceExampleAndLargeIndex()
{
    wave::TraceParseOptions options;
    options.projectTimeBase = {1};
    options.identity = {"project-wave-workbench-demo", "trace-handshake-actual", 1};
    const auto examplePath = std::filesystem::path(WAVE_SOURCE_DIR)
        / "examples"
        / "handshake"
        / "traces"
        / "handshake_actual.vcd";
    auto parsed = wave::parseVcdFile(examplePath, options);
    expect(parsed.ok(), parsed.errorSummary());
    expectEqual(parsed.index->traceSignals.size(), std::size_t{6}, "example VCD signal count is incorrect");
    expectEqual(parsed.index->transitionCount, std::uint64_t{54}, "example VCD transition count is incorrect");
    expect(parsed.index->shift(250), "trace alignment shift failed");
    expectEqual(parsed.index->startTick, wave::Tick{250}, "trace alignment did not shift the start");
    expectEqual(parsed.index->endTick, wave::Tick{220'250}, "trace alignment did not shift the end");

    wave::TraceIndex large;
    large.projectTimeBase = {1};
    large.traceSignals.reserve(1'000);
    for (int lane = 0; lane < 1'000; ++lane) {
        wave::TraceSignal signal;
        signal.id = "signal-" + std::to_string(lane);
        signal.fullName = signal.id;
        signal.transitions.reserve(1'000);
        for (int transition = 0; transition < 1'000; ++transition) {
            signal.transitions.push_back({
                static_cast<wave::Tick>(transition) * 1'000,
                (transition & 1) == 0 ? "0" : "1",
            });
        }
        large.traceSignals.push_back(std::move(signal));
    }
    large.transitionCount = 1'000'000;
    large.startTick = 0;
    large.endTick = 999'000;
    const auto started = std::chrono::steady_clock::now();
    std::uint64_t visibleCount = 0;
    for (int pass = 0; pass < 100; ++pass) {
        const auto start = static_cast<wave::Tick>(pass) * 5'000;
        for (const auto& signal : large.traceSignals) {
            const auto range = signal.visibleRange(start, start + 20'000);
            visibleCount += range.second - range.first;
        }
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started);
    expect(visibleCount > 2'000'000, "large trace visible-range query returned too few transitions");
    expect(
        elapsed < std::chrono::seconds(3),
        "1000-lane / one-million-transition indexed queries exceeded 3 seconds");
    std::cout << "[METRIC] 1000 lanes / 1000000 transitions / 100000 queries: "
              << elapsed.count() << " ms\n";
}

wave::TraceIndex loadHandshakeTrace()
{
    wave::TraceParseOptions options;
    options.projectTimeBase = {1};
    options.identity = {"project-wave-workbench-demo", "trace-handshake-actual", 1};
    const auto path = std::filesystem::path(WAVE_SOURCE_DIR)
        / "examples"
        / "handshake"
        / "traces"
        / "handshake_actual.vcd";
    auto parsed = wave::parseVcdFile(path, options);
    if (!parsed.ok()) throw TestFailure(parsed.errorSummary());
    return std::move(*parsed.index);
}

wave::ImportedTrace handshakeReference()
{
    wave::ImportedTrace reference;
    reference.id = "trace-handshake-actual";
    reference.format = "vcd";
    reference.signalMapping = {
        {"lane-clk", "tb.dut.clk"},
        {"lane-reset", "tb.dut.reset_n"},
        {"lane-request", "tb.dut.req"},
        {"lane-ack", "tb.dut.ack"},
        {"lane-data", "tb.dut.data[7:0]"},
        {"lane-state", "tb.dut.state[1:0]"},
    };
    return reference;
}

void testRelationConditions()
{
    auto project = wave::makeDemonstrationProject();
    auto& scenario = project.scenarios.front();
    const std::map<std::string, std::string> sampledValues{
        {"lane-reset", "1"},
        {"lane-request", "1"},
        {"lane-data", "0x35"},
        {"lane-state", "WAIT_ACK"},
    };
    const wave::RelationConditionPredicate predicate =
        [&](const wave::Lane& lane, const std::string_view literal, std::string& error)
        -> std::optional<bool> {
        const auto sample = sampledValues.find(lane.id);
        if (sample == sampledValues.end()) {
            error = "test sample is unavailable";
            return std::nullopt;
        }
        return sample->second == literal;
    };

    const auto precedence = wave::evaluateRelationCondition(
        scenario,
        "false || reset_n && data[7:0] == 0x35",
        predicate);
    expect(precedence.ok() && *precedence.value, "condition precedence is incorrect");
    const auto strictOperators = wave::evaluateRelationCondition(
        scenario,
        "!(lane-reset !== 1) && (state === WAIT_ACK)",
        predicate);
    expect(
        strictOperators.ok() && *strictOperators.value,
        "strict condition operators or stable-ID lookup failed");

    auto quotedScenario = scenario;
    wave::findLane(quotedScenario, "lane-data")->name = "payload data";
    const auto quotedLane = wave::evaluateRelationCondition(
        quotedScenario,
        "`payload data` == '0x35'",
        predicate);
    expect(quotedLane.ok() && *quotedLane.value, "quoted lane or value parsing failed");

    const auto bareBus = wave::evaluateRelationCondition(
        scenario,
        "data[7:0]",
        predicate);
    expect(!bareBus.ok(), "bare multi-bit lane condition was accepted");
    const auto malformed = wave::evaluateRelationCondition(
        scenario,
        "(reset_n && true",
        predicate);
    expect(!malformed.ok(), "unclosed condition parenthesis was accepted");
    const auto noShortCircuit = wave::evaluateRelationCondition(
        scenario,
        "false && missing_lane == 1",
        predicate);
    expect(
        !noShortCircuit.ok() && noShortCircuit.errorOffset == 9,
        "condition error was hidden by boolean short-circuiting");

    auto ambiguousScenario = scenario;
    auto duplicate = *wave::findLane(ambiguousScenario, "lane-reset");
    duplicate.id = "lane-reset-duplicate";
    ambiguousScenario.lanes.push_back(std::move(duplicate));
    const auto ambiguous = wave::evaluateRelationCondition(
        ambiguousScenario,
        "reset_n == 1",
        predicate);
    expect(!ambiguous.ok(), "ambiguous display-name condition was accepted");

    scenario.relations.front().condition =
        "reset_n && data[7:0] == 0x35 && state == WAIT_ACK";
    auto issues = wave::validateScenario(project, scenario);
    expect(
        std::any_of(issues.begin(), issues.end(), [](const wave::ValidationIssue& issue) {
            return issue.code == wave::ValidationCode::RelationSatisfied;
        }),
        "true relation condition did not reach delay validation");

    auto guardedProject = project;
    auto& guardedRelation = guardedProject.scenarios.front().relations.front();
    guardedRelation.condition = "reset_n == 0";
    guardedRelation.targetEventId = "missing-target";
    issues = wave::validateScenario(
        guardedProject,
        guardedProject.scenarios.front());
    expect(
        std::any_of(issues.begin(), issues.end(), [](const wave::ValidationIssue& issue) {
            return issue.code == wave::ValidationCode::RelationNotApplicable;
        }),
        "false relation condition was not reported as not applicable");
    expect(
        std::none_of(issues.begin(), issues.end(), [](const wave::ValidationIssue& issue) {
            return issue.code == wave::ValidationCode::MissingTargetEvent;
        }),
        "false relation condition did not guard target validation");

    auto invalidProject = project;
    invalidProject.scenarios.front().relations.front().condition =
        "data[7:0] == 0x1ff";
    issues = wave::validateScenario(
        invalidProject,
        invalidProject.scenarios.front());
    expect(
        std::any_of(issues.begin(), issues.end(), [](const wave::ValidationIssue& issue) {
            return issue.code == wave::ValidationCode::InvalidRelationCondition
                && issue.laneId == "lane-data";
        }),
        "invalid relation condition literal was not localized");

    auto trace = loadHandshakeTrace();
    auto reference = handshakeReference();
    wave::CompareOptions relationOnly;
    relationOnly.relationOnly = true;

    const auto actualPass = wave::compareScenario(
        project,
        scenario,
        trace,
        reference,
        relationOnly);
    expect(
        actualPass.matches() && actualPass.diagnostics.empty(),
        "true actual relation condition rejected a valid delay");

    auto falseProject = project;
    auto& falseRelation = falseProject.scenarios.front().relations.front();
    falseRelation.condition = "reset_n == 0";
    falseRelation.targetEventId = "missing-target";
    const auto actualGuarded = wave::compareScenario(
        falseProject,
        falseProject.scenarios.front(),
        trace,
        reference,
        relationOnly);
    expect(
        actualGuarded.matches() && actualGuarded.diagnostics.size() == 1,
        "false actual relation condition did not guard the target");
    const auto guardedJson = QJsonDocument::fromJson(
        QByteArray::fromStdString(wave::compareResultJson(actualGuarded)));
    expectEqual(
        guardedJson.object().value(QStringLiteral("diagnostics")).toArray().size(),
        1,
        "compare JSON omitted relation diagnostics");
    expect(
        wave::compareResultCsv(actualGuarded).find(",diagnostic,") != std::string::npos,
        "compare CSV omitted relation diagnostics");
    const auto guardedHtml = wave::compareResultHtml(
        actualGuarded,
        falseProject,
        falseProject.scenarios.front());
    expect(
        guardedHtml.find("<h3>Diagnostics</h3>") != std::string::npos,
        "compare HTML omitted relation diagnostics");

    auto conditionErrorProject = project;
    conditionErrorProject.scenarios.front().relations.front().condition =
        "missing_lane == 1";
    const auto actualConditionError = wave::compareScenario(
        conditionErrorProject,
        conditionErrorProject.scenarios.front(),
        trace,
        reference,
        relationOnly);
    expectEqual(
        actualConditionError.differences.size(),
        std::size_t{1},
        "condition evaluation error did not fail relation-only compare");
    expectEqual(
        actualConditionError.differences.front().kind,
        wave::CompareDifferenceKind::ConditionEvaluationError,
        "condition evaluation difference kind is incorrect");
    expect(
        wave::compareResultJson(actualConditionError).find(
            "\"kind\":\"condition-evaluation-error\"") != std::string::npos,
        "condition evaluation error was omitted from JSON");

    auto missingMapping = reference;
    missingMapping.signalMapping.erase("lane-data");
    const auto mappingError = wave::compareScenario(
        project,
        scenario,
        trace,
        missingMapping,
        relationOnly);
    expect(
        !mappingError.matches()
            && mappingError.differences.front().kind
                == wave::CompareDifferenceKind::ConditionEvaluationError
            && mappingError.differences.front().laneId == "lane-data",
        "missing condition signal mapping was not localized");
}

void testExpectedActualCompareRules()
{
    auto project = wave::makeDemonstrationProject();
    auto trace = loadHandshakeTrace();
    const auto reference = handshakeReference();
    const auto& scenario = project.scenarios.front();
    const auto exact = wave::compareScenario(project, scenario, trace, reference);
    expectEqual(exact.firstMismatch, std::optional<wave::Tick>{110'000}, "first mismatch is incorrect");
    expectEqual(exact.differences.size(), std::size_t{4}, "exact compare difference count is incorrect");

    wave::CompareOptions tolerance;
    tolerance.defaultRule.edgeTolerance = 5'000;
    const auto tolerant = wave::compareScenario(project, scenario, trace, reference, tolerance);
    expect(tolerant.matches(), "edge tolerance did not accept bounded edge skew");
    expectEqual(tolerant.toleratedEdgeCount, std::uint64_t{4}, "tolerated edge count is incorrect");

    auto maskedProject = project;
    auto* data = wave::findLane(maskedProject.scenarios.front(), "lane-data");
    expect(data != nullptr, "data lane is missing");
    data->segments.at(1).value = "0x34";
    wave::CompareOptions dataWindow;
    dataWindow.start = 80'000;
    dataWindow.end = 100'000;
    const auto unmasked = wave::compareScenario(
        maskedProject,
        maskedProject.scenarios.front(),
        trace,
        reference,
        dataWindow);
    expect(!unmasked.matches(), "unmasked bus mismatch was not detected");
    dataWindow.defaultRule.busMask = "0xfe";
    const auto masked = wave::compareScenario(
        maskedProject,
        maskedProject.scenarios.front(),
        trace,
        reference,
        dataWindow);
    expect(masked.matches(), "bus mask did not suppress the masked bit");

    auto wildcardProject = project;
    auto* request = wave::findLane(wildcardProject.scenarios.front(), "lane-request");
    expect(request != nullptr, "request lane is missing");
    request->segments.at(1).value = "X";
    wave::CompareOptions wildcardWindow;
    wildcardWindow.start = 80'000;
    wildcardWindow.end = 100'000;
    const auto exactX = wave::compareScenario(
        wildcardProject,
        wildcardProject.scenarios.front(),
        trace,
        reference,
        wildcardWindow);
    expect(!exactX.matches(), "exact X compare incorrectly treated X as a wildcard");
    wildcardWindow.defaultRule.xHandling = wave::XHandling::ExpectedXWildcard;
    const auto wildcard = wave::compareScenario(
        wildcardProject,
        wildcardProject.scenarios.front(),
        trace,
        reference,
        wildcardWindow);
    expect(wildcard.matches(), "expected X wildcard did not match actual 1");
    wildcardWindow.defaultRule.xHandling = wave::XHandling::IgnoreAnyX;
    const auto ignoreX = wave::compareScenario(
        wildcardProject,
        wildcardProject.scenarios.front(),
        trace,
        reference,
        wildcardWindow);
    expect(ignoreX.matches(), "ignore-X rule did not ignore expected X");

    auto enumProject = project;
    auto* state = wave::findLane(enumProject.scenarios.front(), "lane-state");
    expect(state != nullptr, "state lane is missing");
    state->segments.at(1).value = "WAIT";
    wave::CompareOptions enumWindow;
    enumWindow.start = 80'000;
    enumWindow.end = 100'000;
    enumWindow.defaultRule.enumEquivalence["WAIT"] = "01";
    const auto equivalent = wave::compareScenario(
        enumProject,
        enumProject.scenarios.front(),
        trace,
        reference,
        enumWindow);
    expect(equivalent.matches(), "enum equivalence did not match the configured actual value");
}

void testCompareDiagnosticsRelationsAndReports()
{
    auto project = wave::makeDemonstrationProject();
    auto trace = loadHandshakeTrace();
    const auto reference = handshakeReference();
    wave::CompareOptions relationOnly;
    relationOnly.relationOnly = true;
    const auto relationPass = wave::compareScenario(
        project,
        project.scenarios.front(),
        trace,
        reference,
        relationOnly);
    expect(relationPass.matches(), "relation-only compare rejected a valid actual delay");

    project.scenarios.front().relations.front().maximumDelay = 30'000;
    const auto relationFail = wave::compareScenario(
        project,
        project.scenarios.front(),
        trace,
        reference,
        relationOnly);
    expectEqual(relationFail.differences.size(), std::size_t{1}, "relation violation was not reported");
    expectEqual(
        relationFail.differences.front().kind,
        wave::CompareDifferenceKind::RelationViolation,
        "relation difference kind is incorrect");

    auto missingReference = reference;
    missingReference.signalMapping.erase("lane-request");
    auto* actualData = trace.findSignal("tb.dut.data[7:0]");
    expect(actualData != nullptr, "actual data signal is missing");
    actualData->width = 7;
    const auto diagnostics = wave::compareScenario(
        project,
        project.scenarios.front(),
        trace,
        missingReference);
    expect(
        std::any_of(
            diagnostics.differences.begin(),
            diagnostics.differences.end(),
            [](const wave::CompareDifference& difference) {
                return difference.kind == wave::CompareDifferenceKind::MissingSignal;
            }),
        "missing signal was not reported");
    expect(
        std::any_of(
            diagnostics.differences.begin(),
            diagnostics.differences.end(),
            [](const wave::CompareDifference& difference) {
                return difference.kind == wave::CompareDifferenceKind::WidthMismatch;
            }),
        "width mismatch was not reported");

    const auto json = wave::compareResultJson(diagnostics);
    expect(
        QJsonDocument::fromJson(QByteArray::fromStdString(json)).isObject(),
        "compare JSON report is invalid");
    const auto csv = wave::compareResultCsv(diagnostics);
    expect(csv.starts_with("id,kind"), "compare CSV report header is invalid");
    const auto html = wave::compareResultHtml(
        diagnostics,
        project,
        project.scenarios.front());
    expect(html.find("<!doctype html>") != std::string::npos, "compare HTML report is invalid");
}

void testCrossApplicationContracts()
{
    const auto exampleRoot = std::filesystem::path(WAVE_SOURCE_DIR)
        / "examples"
        / "handshake";
    QFile signalFile(QString::fromStdWString(
        (exampleRoot / "integration" / "zeroslack_signals.json").wstring()));
    expect(signalFile.open(QIODevice::ReadOnly), "cannot open ZeroSlack signal-list example");
    const auto signalList = wave::parseZeroSlackSignalList(signalFile.readAll());
    expect(signalList.ok(), signalList.error.toStdString());
    auto project = wave::makeDemonstrationProject();
    const auto imported = wave::applyZeroSlackSignalList(
        project,
        project.scenarios.front(),
        *signalList.signalList);
    expectEqual(imported.added, std::size_t{1}, "ZeroSlack added count is incorrect");
    expectEqual(imported.existing, std::size_t{1}, "ZeroSlack existing count is incorrect");
    expect(
        wave::findLane(project.scenarios.front(), "zeroslack-ready") != nullptr,
        "ZeroSlack signal definition was not added");
    expect(
        std::any_of(
            project.linkedResources.begin(),
            project.linkedResources.end(),
            [](const wave::LinkedResource& resource) {
                return resource.kind == "zeroslack-signal-list";
            }),
        "ZeroSlack source reference was not retained");

    QFile frameFile(QString::fromStdWString(
        (exampleRoot / "integration" / "frame_sample_reference.json").wstring()));
    expect(frameFile.open(QIODevice::ReadOnly), "cannot open frame-reference example");
    const auto frameDocument = frameFile.readAll();
    const auto frameReference = wave::parseFrameSampleReference(frameDocument);
    expect(frameReference.ok(), frameReference.error.toStdString());
    const auto loadedExample = wave::loadProjectFile(
        QString::fromStdWString((exampleRoot / "project.wave.json").wstring()));
    expect(loadedExample.ok(), loadedExample.error.toStdString());
    auto frameProject = *loadedExample.project;
    const auto linked = wave::linkFrameSample(frameProject, *frameReference.reference);
    expect(linked.updated, "existing frame sample reference was not updated by stable ID");
    expect(linked.diagnostics.isEmpty(), "resolved transaction target was reported unresolved");
    const auto* transactionLane = wave::findLane(
        frameProject.scenarios.front(),
        "lane-transaction");
    expect(transactionLane != nullptr, "transaction lane is missing");
    expect(
        transactionLane->segments.front().extensions.contains("linkedResourceStableId"),
        "transaction segment does not retain the frame sample stable ID");
    auto invalidFrame = QJsonDocument::fromJson(frameDocument).object();
    invalidFrame.insert(QStringLiteral("encodedBytes"), QStringLiteral("123"));
    expect(
        !wave::parseFrameSampleReference(QJsonDocument(invalidFrame).toJson()).ok(),
        "odd-length encoded bytes were accepted");

    const auto manifest = QJsonDocument::fromJson(
        wave::makeWorkspaceManifest(frameProject, QStringLiteral("project.wave.json")));
    expect(manifest.isObject(), "workspace manifest JSON is invalid");
    expectEqual(
        manifest.object().value(QStringLiteral("schemaVersion")).toInt(),
        1,
        "workspace manifest schema is incorrect");

    QTemporaryDir artifacts;
    expect(artifacts.isValid(), "temporary artifact directory creation failed");
    QFile artifact(QDir(artifacts.path()).filePath(QStringLiteral("diagram.svg")));
    expect(artifact.open(QIODevice::WriteOnly), "temporary artifact creation failed");
    artifact.write("<svg/>");
    artifact.close();
    const auto entry = wave::makePinloomEntry(
        frameProject,
        frameProject.scenarios.front(),
        QStringLiteral("project.wave.json"),
        artifacts.path(),
        QDir(artifacts.path()).filePath(QStringLiteral("entry.json")));
    expect(QJsonDocument::fromJson(entry.document).isObject(), "Pinloom entry JSON is invalid");
    expectEqual(entry.archiveUri.scheme(), QStringLiteral("pinloom"), "Pinloom URI scheme is incorrect");

    QUrl uri(QStringLiteral("waveworkbench://compare"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("project"), QStringLiteral("C:/Project Folder/project.wave.json"));
    query.addQueryItem(QStringLiteral("lane"), QStringLiteral("lane-request"));
    query.addQueryItem(QStringLiteral("tick"), QStringLiteral("80000"));
    uri.setQuery(query);
    const auto launch = wave::parseWaveWorkbenchUri(uri);
    expect(launch.ok(), launch.error.toStdString());
    expect(launch.request->compareMode, "compare URI did not select Compare mode");
    expectEqual(launch.request->tick, std::optional<wave::Tick>{80'000}, "URI tick is incorrect");
    expectEqual(launch.request->laneId, QStringLiteral("lane-request"), "URI lane ID is incorrect");
}

} // namespace

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    const std::vector<std::pair<std::string, std::function<void()>>> tests{
        {"time conversions and precision", testTimeConversions},
        {"clock domains and snapping", testClocksAndSnapping},
        {"clock overrides and cycle retiming", testClockOverridesAndRetiming},
        {"lane and group property editing", testLaneAndGroupPropertyEditing},
        {"lane and group removal", testLaneAndGroupRemoval},
        {"lane display reordering", testLaneReordering},
        {"lane values and segment merge/split", testLaneValuesAndSegments},
        {"undo and redo", testUndoRedo},
        {"multi-lane range assignment command", testMultiLaneRangeAssignmentCommand},
        {"command replacement, cancel, and duration", testCommandStackReplacementAndDuration},
        {"direct segment editing commands", testDirectSegmentEditingCommands},
        {"multi-lane copy/paste command", testMultiLanePasteCommand},
        {"event and segment synchronization", testEventSegmentSynchronization},
        {"markers, relations, and validation", testMarkersRelationsAndValidation},
        {"SystemVerilog, SVA, and cocotb generation", testCodeGeneration},
        {"SVG, PNG, PDF, and WaveDrom exports", testWaveformExports},
        {"serialization round-trip", testSerializationRoundTrip},
        {"schema migration", testSchemaMigration},
        {"project directory move", testProjectDirectoryMove},
        {"VCD import and indexed lookup", testVcdImportAndIndex},
        {"CSV import and cancellation", testCsvImportAndCancellation},
        {"trace example and million-transition index", testTraceExampleAndLargeIndex},
        {"relation condition parsing and evaluation", testRelationConditions},
        {"Expected/Actual compare rules", testExpectedActualCompareRules},
        {"compare diagnostics, relations, and reports", testCompareDiagnosticsRelationsAndReports},
        {"cross-application file and URI contracts", testCrossApplicationContracts},
    };

    int failures = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& exception) {
            ++failures;
            std::cerr << "[FAIL] " << name << ": " << exception.what() << '\n';
        }
    }
    std::cout << tests.size() - static_cast<std::size_t>(failures)
              << "/" << tests.size() << " tests passed\n";
    return failures == 0 ? 0 : 1;
}
