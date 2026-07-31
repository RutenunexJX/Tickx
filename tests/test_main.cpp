#include "wave/commands.h"
#include "wave/compare.h"
#include "wave/automation.h"
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
    expectEqual(
        wave::toTicks("2.5", wave::TimeUnit::Nanosecond, timeBase),
        std::optional<wave::Tick>{2'500},
        "decimal nanoseconds must convert exactly without floating point");
    expectEqual(
        wave::toTicks(".125", wave::TimeUnit::Microsecond, timeBase),
        std::optional<wave::Tick>{125'000},
        "leading-decimal microseconds must convert exactly");
    expectEqual(
        wave::toTicks("-2.5", wave::TimeUnit::Nanosecond, timeBase),
        std::optional<wave::Tick>{-2'500},
        "negative decimal physical time must preserve its sign");
    expectEqual(
        wave::toTicks("1.000", wave::TimeUnit::Nanosecond, timeBase),
        std::optional<wave::Tick>{1'000},
        "trailing decimal zeroes must remain exact");
    expect(
        !wave::toTicks("0.0005", wave::TimeUnit::Nanosecond, timeBase)
             .has_value(),
        "sub-picosecond decimal time must be rejected");
    expect(
        !wave::toTicks("2.5", wave::TimeUnit::Nanosecond, wave::TimeBase{3})
             .has_value(),
        "decimal time not divisible by the project timebase must be rejected");
    expect(
        !wave::toTicks("1.2.3", wave::TimeUnit::Nanosecond, timeBase)
             .has_value(),
        "invalid decimal syntax must be rejected");
    expect(
        !wave::toTicks(
             "9223372036854775807",
             wave::TimeUnit::Millisecond,
             timeBase)
             .has_value(),
        "overflowing decimal time must be rejected");
    expectEqual(
        wave::toTicks(
            "-9223372036854775808",
            wave::TimeUnit::Picosecond,
            timeBase),
        std::optional<wave::Tick>{
            std::numeric_limits<std::int64_t>::min()},
        "minimum signed decimal time must remain representable");

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
    expectEqual(
        wave::formatTick(2'500, timeBase),
        std::string{"2.5 ns"},
        "decimal nanoseconds must format compactly");
    expectEqual(
        wave::formatTick(25'001, timeBase),
        std::string{"25.001 ns"},
        "picosecond precision must remain visible in decimal nanoseconds");
    expectEqual(
        wave::formatTick(1'250'000, timeBase),
        std::string{"1.25 us"},
        "decimal microseconds must format compactly");
    expectEqual(
        wave::formatTick(-2'500, timeBase),
        std::string{"-2.5 ns"},
        "negative decimal display must preserve its sign");
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
    expect(
        stack.execute(std::make_unique<wave::ShowLaneCommand>(
            scenario,
            "lane-request")),
        "show lane command reported no effect");
    expect(
        wave::findLane(scenario, "lane-request")->visible,
        "show lane command left the signal hidden");
    expectEqual(
        stack.undoDescription(),
        std::string{"Show lane"},
        "show lane command exposed the wrong Undo description");
    const auto shownSignalStateId = stack.stateId();
    expect(
        !stack.execute(std::make_unique<wave::ShowLaneCommand>(
            scenario,
            "lane-request")),
        "showing an already visible signal reported an effect");
    expectEqual(
        stack.stateId(),
        shownSignalStateId,
        "no-effect show lane command changed history state");
    expect(stack.undo(), "show lane undo failed");
    expect(
        !wave::findLane(scenario, "lane-request")->visible,
        "show lane undo did not hide the signal again");
    expect(stack.redo(), "show lane redo failed");
    expect(
        wave::findLane(scenario, "lane-request")->visible,
        "show lane redo did not restore the signal");
    expect(stack.undo(), "second show lane undo failed");
    expect(
        !wave::findLane(scenario, "lane-request")->visible,
        "second show lane undo did not restore hidden state");
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
    expect(
        stack.execute(std::make_unique<wave::ShowLaneCommand>(
            scenario,
            group.id)),
        "show group command reported no effect");
    expect(
        wave::findLane(scenario, group.id)->visible,
        "show group command left the group hidden");
    expectEqual(
        stack.undoDescription(),
        std::string{"Show group"},
        "show group command exposed the wrong Undo description");
    expect(stack.undo(), "show group undo failed");
    expect(
        !wave::findLane(scenario, group.id)->visible,
        "show group undo did not hide the group again");
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

void testBatchLaneCleanup()
{
    {
        auto project = wave::makeDemonstrationProject();
        auto& scenario = project.scenarios.front();
        const auto original = scenario;
        wave::CommandStack stack;
        expect(
            stack.execute(std::make_unique<wave::HideLanesCommand>(
                scenario,
                std::vector<std::string>{
                    "lane-reset",
                    "lane-request",
                    "lane-ack",
                })),
            "batch lane hiding reported no effect");
        expect(
            !wave::findLane(scenario, "lane-reset")->visible
                && !wave::findLane(scenario, "lane-request")->visible
                && !wave::findLane(scenario, "lane-ack")->visible,
            "batch lane hiding left a selected signal visible");
        expectEqual(
            stack.undoDescription(),
            std::string{"Hide selected signals"},
            "batch lane hiding exposed the wrong Undo description");
        expect(stack.undo(), "batch lane hiding undo failed");
        expectEqual(
            scenario,
            original,
            "batch lane hiding undo did not restore the exact scenario");
        expect(stack.redo(), "batch lane hiding redo failed");
        expect(
            !wave::findLane(scenario, "lane-reset")->visible
                && !wave::findLane(scenario, "lane-request")->visible
                && !wave::findLane(scenario, "lane-ack")->visible,
            "batch lane hiding redo failed");
        expect(stack.undo(), "second batch lane hiding undo failed");

        bool rejectedDuplicate = false;
        try {
            [[maybe_unused]] wave::HideLanesCommand invalid(
                scenario,
                {"lane-reset", "lane-reset"});
        } catch (const std::invalid_argument&) {
            rejectedDuplicate = true;
        }
        expect(
            rejectedDuplicate,
            "batch lane hiding accepted a duplicate identity");

        bool rejectedGroup = false;
        try {
            [[maybe_unused]] wave::HideLanesCommand invalid(
                scenario,
                {"lane-reset", "group-handshake"});
        } catch (const std::invalid_argument&) {
            rejectedGroup = true;
        }
        expect(
            rejectedGroup,
            "batch lane hiding accepted a Group");
    }

    auto project = wave::makeDemonstrationProject();
    auto& scenario = project.scenarios.front();
    project.importedTraces.push_back({
        "trace-batch-removal-test",
        "trace.vcd",
        "vcd",
        0,
        {
            {"lane-request", "tb.req"},
            {"lane-ack", "tb.ack"},
            {"lane-data", "tb.data"},
        },
        {},
    });
    const auto original = project;
    const std::vector<std::string> removedLaneIds{
        "lane-request",
        "lane-ack",
    };
    const auto selected = [&removedLaneIds](const std::string& laneId) {
        return std::find(
                   removedLaneIds.begin(),
                   removedLaneIds.end(),
                   laneId)
            != removedLaneIds.end();
    };
    std::vector<std::string> removedEventIds;
    for (const auto& event : scenario.events) {
        if (selected(event.laneId)) removedEventIds.push_back(event.id);
    }
    const auto removedRelationCount = static_cast<std::size_t>(std::count_if(
        scenario.relations.begin(),
        scenario.relations.end(),
        [&removedEventIds](const wave::Relation& relation) {
            const auto removed = [&removedEventIds](const std::string& eventId) {
                return std::find(
                           removedEventIds.begin(),
                           removedEventIds.end(),
                           eventId)
                    != removedEventIds.end();
            };
            return removed(relation.sourceEventId)
                || removed(relation.targetEventId);
        }));

    wave::CommandStack stack;
    const auto beforeStateId = stack.stateId();
    expect(
        stack.execute(std::make_unique<wave::RemoveLanesCommand>(
            project,
            scenario,
            removedLaneIds)),
        "batch lane removal reported no effect");
    expect(
        stack.stateId() != beforeStateId && stack.size() == 1,
        "batch lane removal did not create exactly one history entry");
    expectEqual(
        stack.undoDescription(),
        std::string{"Remove selected signals"},
        "batch lane removal exposed the wrong Undo description");
    expect(
        wave::findLane(scenario, "lane-request") == nullptr
            && wave::findLane(scenario, "lane-ack") == nullptr,
        "batch lane removal retained a selected signal");
    expectEqual(
        scenario.events.size(),
        original.scenarios.front().events.size()
            - removedEventIds.size(),
        "batch lane removal did not remove the selected signals' events");
    expectEqual(
        scenario.relations.size(),
        original.scenarios.front().relations.size()
            - removedRelationCount,
        "batch lane removal did not remove dependent relations exactly once");
    expect(
        !project.importedTraces.front().signalMapping.contains("lane-request")
            && !project.importedTraces.front().signalMapping.contains("lane-ack")
            && project.importedTraces.front().signalMapping.contains("lane-data"),
        "batch lane removal did not clean only the selected trace mappings");

    const auto removed = project;
    project.importedTraces.push_back({
        "trace-added-after-batch-removal",
        "new-trace.vcd",
        "vcd",
        0,
        {{"lane-data", "tb.new_data"}},
        {},
    });
    expect(stack.undo(), "batch lane removal undo failed");
    expectEqual(
        project.importedTraces.size(),
        std::size_t{2},
        "batch lane removal undo discarded a subsequently imported trace");
    project.importedTraces.pop_back();
    expectEqual(
        project,
        original,
        "batch lane removal undo did not restore the exact project");
    expect(stack.redo(), "batch lane removal redo failed");
    expectEqual(
        project,
        removed,
        "batch lane removal redo did not restore the exact result");
    expect(stack.undo(), "second batch lane removal undo failed");

    bool rejectedDuplicate = false;
    try {
        [[maybe_unused]] wave::RemoveLanesCommand invalid(
            project,
            scenario,
            {"lane-request", "lane-request"});
    } catch (const std::invalid_argument&) {
        rejectedDuplicate = true;
    }
    expect(
        rejectedDuplicate,
        "batch lane removal accepted a duplicate identity");

    bool rejectedGroup = false;
    try {
        [[maybe_unused]] wave::RemoveLanesCommand invalid(
            project,
            scenario,
            {"lane-request", "group-handshake"});
    } catch (const std::invalid_argument&) {
        rejectedGroup = true;
    }
    expect(
        rejectedGroup,
        "batch lane removal accepted a Group");

    auto sharedProject = original;
    auto sharedScenario = sharedProject.scenarios.front();
    sharedScenario.id = "scenario-shared-lane-id";
    sharedScenario.name = "Shared lane identity";
    sharedProject.scenarios.push_back(std::move(sharedScenario));
    auto& firstScenario = sharedProject.scenarios.front();
    wave::RemoveLanesCommand sharedRemoval(
        sharedProject,
        firstScenario,
        {"lane-request", "lane-ack"});
    sharedRemoval.redo();
    expect(
        sharedProject.importedTraces.front().signalMapping.contains(
            "lane-request")
            && sharedProject.importedTraces.front().signalMapping.contains(
                "lane-ack"),
        "batch lane removal removed mappings still used by another Scenario");
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

void testBatchLaneReordering()
{
    auto project = wave::makeDemonstrationProject();
    auto& scenario = project.scenarios.front();
    const auto original = scenario;
    const auto laneOrder = [](const wave::Scenario& value) {
        std::vector<std::string> result;
        result.reserve(value.lanes.size());
        std::transform(
            value.lanes.begin(),
            value.lanes.end(),
            std::back_inserter(result),
            [](const wave::Lane& lane) {
                return lane.id;
            });
        return result;
    };

    wave::CommandStack stack;
    const auto beforeState = stack.stateId();
    expect(
        stack.execute(std::make_unique<wave::MoveLanesCommand>(
            scenario,
            std::vector<std::string>{
                "lane-ack",
                "lane-reset",
                "lane-request",
            },
            scenario.lanes.size())),
        "batch lane reorder reported no effect");
    expect(
        stack.stateId() != beforeState,
        "batch lane reorder did not create one history state");
    expectEqual(
        stack.undoDescription(),
        std::string{"Move selected signals"},
        "batch lane reorder exposed the wrong Undo description");
    expectEqual(
        laneOrder(scenario),
        std::vector<std::string>({
            "lane-clk",
            "group-handshake",
            "lane-data",
            "lane-state",
            "lane-reset",
            "lane-request",
            "lane-ack",
        }),
        "batch lane reorder did not preserve Scenario-relative signal order");
    expectEqual(
        scenario.events,
        original.events,
        "batch lane reorder changed scenario events");
    expectEqual(
        scenario.relations,
        original.relations,
        "batch lane reorder changed scenario relations");
    expectEqual(
        wave::findLane(scenario, "lane-reset")->groupId,
        std::string{},
        "batch lane reorder changed an ungrouped signal membership");
    expectEqual(
        wave::findLane(scenario, "lane-request")->groupId,
        std::string{"group-handshake"},
        "batch lane reorder changed a grouped signal membership");
    expectEqual(
        wave::findLane(scenario, "lane-ack")->groupId,
        std::string{"group-handshake"},
        "batch lane reorder changed a second grouped signal membership");

    const auto moved = scenario;
    expect(stack.undo(), "batch lane reorder undo failed");
    expectEqual(
        scenario,
        original,
        "batch lane reorder undo did not restore the exact Scenario");
    expect(stack.redo(), "batch lane reorder redo failed");
    expectEqual(
        scenario,
        moved,
        "batch lane reorder redo did not restore the exact result");
    expect(stack.undo(), "second batch lane reorder undo failed");

    wave::CommandStack frontStack;
    expect(
        frontStack.execute(std::make_unique<wave::MoveLanesCommand>(
            scenario,
            std::vector<std::string>{
                "lane-ack",
                "lane-request",
            },
            0)),
        "batch lane reorder to the front reported no effect");
    expectEqual(
        laneOrder(scenario),
        std::vector<std::string>({
            "lane-request",
            "lane-ack",
            "lane-clk",
            "lane-reset",
            "group-handshake",
            "lane-data",
            "lane-state",
        }),
        "batch lane reorder used selection order instead of Scenario order");
    expect(frontStack.undo(), "front batch lane reorder undo failed");
    expectEqual(
        scenario,
        original,
        "front batch lane reorder undo did not restore the exact Scenario");

    wave::CommandStack noOpStack;
    const auto noOpState = noOpStack.stateId();
    expect(
        !noOpStack.execute(std::make_unique<wave::MoveLanesCommand>(
            scenario,
            std::vector<std::string>{
                "lane-ack",
                "lane-request",
            },
            5)),
        "no-op batch lane reorder created history");
    expectEqual(
        noOpStack.stateId(),
        noOpState,
        "no-op batch lane reorder changed the history state");
    expectEqual(
        scenario,
        original,
        "no-op batch lane reorder changed the Scenario");

    bool rejectedDuplicate = false;
    try {
        [[maybe_unused]] wave::MoveLanesCommand invalid(
            scenario,
            {"lane-request", "lane-request"},
            0);
    } catch (const std::invalid_argument&) {
        rejectedDuplicate = true;
    }
    expect(
        rejectedDuplicate,
        "batch lane reorder accepted a duplicate identity");

    bool rejectedGroup = false;
    try {
        [[maybe_unused]] wave::MoveLanesCommand invalid(
            scenario,
            {"lane-request", "group-handshake"},
            0);
    } catch (const std::invalid_argument&) {
        rejectedGroup = true;
    }
    expect(
        rejectedGroup,
        "batch lane reorder accepted a Group");

    bool rejectedOutside = false;
    try {
        [[maybe_unused]] wave::MoveLanesCommand invalid(
            scenario,
            {"lane-request", "lane-ack"},
            scenario.lanes.size() + 1);
    } catch (const std::invalid_argument&) {
        rejectedOutside = true;
    }
    expect(
        rejectedOutside,
        "batch lane reorder accepted an outside insertion slot");
}

void testBatchLaneDuplication()
{
    auto project = wave::makeDemonstrationProject();
    auto& scenario = project.scenarios.front();
    project.importedTraces.push_back({
        "trace-batch-duplicate",
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

    const auto* sourceClockLane =
        wave::findLane(scenario, "lane-clk");
    const auto* sourceBitLane =
        wave::findLane(scenario, "lane-request");
    const auto* sourceBusLane =
        wave::findLane(scenario, "lane-data");
    expect(
        sourceClockLane && sourceBitLane && sourceBusLane,
        "batch duplicate fixture signals are unavailable");
    const auto* sourceClock = wave::findClock(
        project,
        sourceClockLane->clockDomainId);
    expect(
        sourceClock != nullptr,
        "batch duplicate fixture clock domain is unavailable");
    const auto sourceBitGroupId = sourceBitLane->groupId;

    auto clockCopy = *sourceClockLane;
    clockCopy.id = "lane-clk-batch-copy";
    clockCopy.name = "clk_batch_copy";
    clockCopy.clockDomainId = "clock-batch-copy";
    for (auto index = std::size_t{0};
         index < clockCopy.segments.size();
         ++index) {
        clockCopy.segments[index].id =
            "segment-clk-batch-copy-" + std::to_string(index);
    }

    auto bitCopy = *sourceBitLane;
    bitCopy.id = "lane-request-batch-copy";
    bitCopy.name = "req_batch_copy";
    for (auto index = std::size_t{0};
         index < bitCopy.segments.size();
         ++index) {
        bitCopy.segments[index].id =
            "segment-request-batch-copy-" + std::to_string(index);
    }

    auto busCopy = *sourceBusLane;
    busCopy.id = "lane-data-batch-copy";
    busCopy.name = "data_batch_copy";
    for (auto index = std::size_t{0};
         index < busCopy.segments.size();
         ++index) {
        busCopy.segments[index].id =
            "segment-data-batch-copy-" + std::to_string(index);
    }

    auto clockDomainCopy = *sourceClock;
    clockDomainCopy.id = clockCopy.clockDomainId;
    clockDomainCopy.name = clockCopy.name;

    const auto sourceBusPosition = std::find_if(
        scenario.lanes.begin(),
        scenario.lanes.end(),
        [](const wave::Lane& lane) {
            return lane.id == "lane-data";
        });
    expect(
        sourceBusPosition != scenario.lanes.end(),
        "batch duplicate insertion source is unavailable");
    const auto insertionIndex = static_cast<std::size_t>(
        std::distance(scenario.lanes.begin(), sourceBusPosition)) + 1;
    auto expectedOrder = std::vector<std::string>{};
    expectedOrder.reserve(scenario.lanes.size() + 3);
    std::transform(
        scenario.lanes.begin(),
        scenario.lanes.end(),
        std::back_inserter(expectedOrder),
        [](const wave::Lane& lane) {
            return lane.id;
        });
    expectedOrder.insert(
        expectedOrder.begin()
            + static_cast<std::ptrdiff_t>(insertionIndex),
        {
            clockCopy.id,
            bitCopy.id,
            busCopy.id,
        });

    wave::CommandStack stack;
    expect(
        stack.execute(
            std::make_unique<wave::DuplicateLanesCommand>(
                project,
                scenario,
                std::vector<wave::Lane>{
                    clockCopy,
                    bitCopy,
                    busCopy,
                },
                std::vector<wave::ClockDomain>{
                    clockDomainCopy,
                },
                insertionIndex)),
        "batch lane duplicate reported no effect");
    expectEqual(
        stack.size(),
        std::size_t{1},
        "batch lane duplicate created more than one history entry");
    expectEqual(
        stack.undoDescription(),
        std::string{"Duplicate selected signals"},
        "batch lane duplicate exposed the wrong Undo description");

    std::vector<std::string> actualOrder;
    actualOrder.reserve(scenario.lanes.size());
    std::transform(
        scenario.lanes.begin(),
        scenario.lanes.end(),
        std::back_inserter(actualOrder),
        [](const wave::Lane& lane) {
            return lane.id;
        });
    expectEqual(
        actualOrder,
        expectedOrder,
        "batch lane duplicate did not insert one ordered block");
    expectEqual(
        wave::findLane(scenario, bitCopy.id)->groupId,
        sourceBitGroupId,
        "batch lane duplicate changed Group membership");
    expectEqual(
        wave::findLane(scenario, busCopy.id)->segments,
        busCopy.segments,
        "batch lane duplicate changed copied waveform data");
    expect(
        wave::findClock(project, clockDomainCopy.id) != nullptr,
        "batch lane duplicate did not add the independent clock domain");
    expectEqual(
        scenario.events,
        original.scenarios.front().events,
        "batch lane duplicate inferred new Events");
    expectEqual(
        scenario.relations,
        original.scenarios.front().relations,
        "batch lane duplicate inferred new Relations");
    expectEqual(
        project.importedTraces,
        original.importedTraces,
        "batch lane duplicate inferred new Trace mappings");

    const auto duplicated = project;
    expect(stack.undo(), "batch lane duplicate undo failed");
    expectEqual(
        project,
        original,
        "batch lane duplicate undo did not restore the exact Project");
    expect(stack.redo(), "batch lane duplicate redo failed");
    expectEqual(
        project,
        duplicated,
        "batch lane duplicate redo did not restore stable identities and order");
    expect(stack.undo(), "second batch lane duplicate undo failed");

    bool rejectedExistingIdentity = false;
    try {
        auto invalid = *wave::findLane(scenario, "lane-request");
        [[maybe_unused]] wave::DuplicateLanesCommand command(
            project,
            scenario,
            {invalid},
            {},
            scenario.lanes.size());
    } catch (const std::invalid_argument&) {
        rejectedExistingIdentity = true;
    }
    expect(
        rejectedExistingIdentity,
        "batch lane duplicate accepted an existing Lane identity");

    bool rejectedMissingClock = false;
    try {
        [[maybe_unused]] wave::DuplicateLanesCommand command(
            project,
            scenario,
            {clockCopy},
            {},
            scenario.lanes.size());
    } catch (const std::invalid_argument&) {
        rejectedMissingClock = true;
    }
    expect(
        rejectedMissingClock,
        "batch lane duplicate accepted a Clock without independent settings");

    bool rejectedOutside = false;
    try {
        [[maybe_unused]] wave::DuplicateLanesCommand command(
            project,
            scenario,
            {bitCopy},
            {},
            scenario.lanes.size() + 1);
    } catch (const std::invalid_argument&) {
        rejectedOutside = true;
    }
    expect(
        rejectedOutside,
        "batch lane duplicate accepted an outside insertion index");
}

void testLaneGroupAssignment()
{
    auto project = wave::makeDemonstrationProject();
    auto& scenario = project.scenarios.front();
    wave::Lane unrelated;
    unrelated.id = "lane-unrelated";
    unrelated.name = "unrelated";
    unrelated.kind = wave::LaneKind::Bit;
    const auto dataPosition = std::find_if(
        scenario.lanes.begin(),
        scenario.lanes.end(),
        [](const wave::Lane& lane) {
            return lane.id == "lane-data";
        });
    scenario.lanes.insert(dataPosition, std::move(unrelated));
    const auto original = scenario;
    const auto originalEvents = scenario.events;
    const auto originalRelations = scenario.relations;

    wave::CommandStack stack;
    const auto beforeState = stack.stateId();
    expect(
        stack.execute(std::make_unique<wave::SetLaneGroupCommand>(
            scenario,
            "lane-reset",
            "group-handshake")),
        "lane group assignment reported no effect");
    expect(
        stack.stateId() != beforeState,
        "lane group assignment did not create one history state");
    expectEqual(
        stack.undoDescription(),
        std::string{"Move signal to group"},
        "lane group assignment exposed the wrong Undo description");

    const auto* reset = wave::findLane(scenario, "lane-reset");
    const auto resetIterator = std::find_if(
        scenario.lanes.begin(),
        scenario.lanes.end(),
        [](const wave::Lane& lane) {
            return lane.id == "lane-reset";
        });
    const auto stateIterator = std::find_if(
        scenario.lanes.begin(),
        scenario.lanes.end(),
        [](const wave::Lane& lane) {
            return lane.id == "lane-state";
        });
    expect(
        reset
            && reset->groupId == "group-handshake"
            && resetIterator != scenario.lanes.end()
            && stateIterator != scenario.lanes.end()
            && resetIterator == std::next(stateIterator),
        "lane group assignment did not place the signal after the existing member block");
    expectEqual(
        scenario.events,
        originalEvents,
        "lane group assignment changed scenario events");
    expectEqual(
        scenario.relations,
        originalRelations,
        "lane group assignment changed scenario relations");

    const auto groupedState = scenario;
    const auto groupedStateId = stack.stateId();
    expect(
        !stack.execute(std::make_unique<wave::SetLaneGroupCommand>(
            scenario,
            "lane-reset",
            "group-handshake")),
        "assigning a lane to its current group reported an effect");
    expectEqual(
        stack.stateId(),
        groupedStateId,
        "no-op lane group assignment polluted command history");
    expectEqual(
        scenario,
        groupedState,
        "no-op lane group assignment changed the scenario");

    expect(stack.undo(), "lane group assignment undo failed");
    expectEqual(
        scenario,
        original,
        "lane group assignment undo did not restore membership and order");
    expect(stack.redo(), "lane group assignment redo failed");
    expectEqual(
        scenario,
        groupedState,
        "lane group assignment redo did not restore membership and order");

    const auto groupedOrder = scenario.lanes;
    expect(
        stack.execute(std::make_unique<wave::SetLaneGroupCommand>(
            scenario,
            "lane-reset",
            "")),
        "removing a lane from its group reported no effect");
    expect(
        wave::findLane(scenario, "lane-reset")->groupId.empty(),
        "removing a lane from its group kept the group reference");
    expectEqual(
        scenario.lanes.size(),
        groupedOrder.size(),
        "removing a lane from its group changed the lane count");
    expectEqual(
        stack.undoDescription(),
        std::string{"Remove signal from group"},
        "lane ungrouping exposed the wrong Undo description");
    expect(stack.undo(), "lane ungrouping undo failed");
    expectEqual(
        scenario,
        groupedState,
        "lane ungrouping undo did not restore the exact grouped state");

    bool rejectedGroupSource = false;
    try {
        [[maybe_unused]] wave::SetLaneGroupCommand invalid(
            scenario,
            "group-handshake",
            "group-handshake");
    } catch (const std::invalid_argument&) {
        rejectedGroupSource = true;
    }
    expect(rejectedGroupSource, "group nesting was accepted");

    bool rejectedSignalTarget = false;
    try {
        [[maybe_unused]] wave::SetLaneGroupCommand invalid(
            scenario,
            "lane-reset",
            "lane-request");
    } catch (const std::invalid_argument&) {
        rejectedSignalTarget = true;
    }
    expect(rejectedSignalTarget, "a non-group lane was accepted as a group target");
}

void testCreateGroupWithLane()
{
    auto project = wave::makeDemonstrationProject();
    auto& scenario = project.scenarios.front();
    const auto original = scenario;
    const auto originalEvents = scenario.events;
    const auto originalRelations = scenario.relations;

    wave::Lane group;
    group.id = "group-request-only";
    group.name = "Request controls";
    group.kind = wave::LaneKind::Group;
    group.color = "#7986cb";
    group.height = 40;
    group.visible = true;

    wave::CommandStack stack;
    const auto beforeState = stack.stateId();
    expect(
        stack.execute(std::make_unique<wave::CreateGroupWithLaneCommand>(
            scenario,
            group,
            "lane-request")),
        "create group with signal reported no effect");
    expect(
        stack.stateId() != beforeState,
        "create group with signal did not create one history state");
    expectEqual(
        stack.undoDescription(),
        std::string{"Create group with signal"},
        "create group with signal exposed the wrong Undo description");

    const auto groupIterator = std::find_if(
        scenario.lanes.begin(),
        scenario.lanes.end(),
        [&group](const wave::Lane& lane) {
            return lane.id == group.id;
        });
    const auto requestIterator = std::find_if(
        scenario.lanes.begin(),
        scenario.lanes.end(),
        [](const wave::Lane& lane) {
            return lane.id == "lane-request";
        });
    expect(
        groupIterator != scenario.lanes.end()
            && requestIterator != scenario.lanes.end()
            && std::next(groupIterator) == requestIterator
            && requestIterator->groupId == group.id,
        "new group was not inserted immediately above its first member");
    expect(
        wave::findLane(scenario, "lane-ack")->groupId
            == "group-handshake",
        "creating a group changed another member of the previous group");
    expectEqual(
        scenario.events,
        originalEvents,
        "creating a group with a signal changed scenario events");
    expectEqual(
        scenario.relations,
        originalRelations,
        "creating a group with a signal changed scenario relations");

    const auto created = scenario;
    expect(stack.undo(), "create group with signal undo failed");
    expectEqual(
        scenario,
        original,
        "create group with signal undo did not restore membership and order");
    expect(stack.redo(), "create group with signal redo failed");
    expectEqual(
        scenario,
        created,
        "create group with signal redo did not restore the exact result");

    bool rejectedDuplicateGroup = false;
    try {
        [[maybe_unused]] wave::CreateGroupWithLaneCommand invalid(
            scenario,
            group,
            "lane-reset");
    } catch (const std::invalid_argument&) {
        rejectedDuplicateGroup = true;
    }
    expect(rejectedDuplicateGroup, "duplicate group identity was accepted");

    auto nested = group;
    nested.id = "group-nested";
    nested.name = "Nested";
    bool rejectedGroupSource = false;
    try {
        [[maybe_unused]] wave::CreateGroupWithLaneCommand invalid(
            scenario,
            nested,
            "group-handshake");
    } catch (const std::invalid_argument&) {
        rejectedGroupSource = true;
    }
    expect(rejectedGroupSource, "a Group was accepted as another Group's first member");

    auto blank = group;
    blank.id = "group-blank";
    blank.name = "   ";
    bool rejectedBlankName = false;
    try {
        [[maybe_unused]] wave::CreateGroupWithLaneCommand invalid(
            scenario,
            blank,
            "lane-reset");
    } catch (const std::invalid_argument&) {
        rejectedBlankName = true;
    }
    expect(rejectedBlankName, "a blank Group name was accepted");
}

void testBatchLaneGrouping()
{
    auto project = wave::makeDemonstrationProject();
    auto& scenario = project.scenarios.front();
    const auto original = scenario;
    const auto originalEvents = scenario.events;
    const auto originalRelations = scenario.relations;

    wave::CommandStack stack;
    expect(
        stack.execute(std::make_unique<wave::SetLanesGroupCommand>(
            scenario,
            std::vector<std::string>{
                "lane-reset",
                "lane-request",
                "lane-ack",
            },
            "group-handshake")),
        "batch lane group assignment reported no effect");
    expectEqual(
        stack.undoDescription(),
        std::string{"Move selected signals to group"},
        "batch lane group assignment exposed the wrong Undo description");
    expect(
        wave::findLane(scenario, "lane-reset")->groupId
                == "group-handshake"
            && wave::findLane(scenario, "lane-request")->groupId
                == "group-handshake"
            && wave::findLane(scenario, "lane-ack")->groupId
                == "group-handshake",
        "batch lane group assignment did not assign every selected signal");
    expectEqual(
        scenario.events,
        originalEvents,
        "batch lane group assignment changed scenario events");
    expectEqual(
        scenario.relations,
        originalRelations,
        "batch lane group assignment changed scenario relations");
    const auto grouped = scenario;
    expect(stack.undo(), "batch lane group assignment undo failed");
    expectEqual(
        scenario,
        original,
        "batch lane group assignment undo did not restore exact order and membership");
    expect(stack.redo(), "batch lane group assignment redo failed");
    expectEqual(
        scenario,
        grouped,
        "batch lane group assignment redo did not restore the exact result");
    expect(stack.undo(), "second batch lane group assignment undo failed");

    const auto originalOrder = [&scenario] {
        std::vector<std::string> result;
        result.reserve(scenario.lanes.size());
        for (const auto& lane : scenario.lanes) result.push_back(lane.id);
        return result;
    }();
    expect(
        stack.execute(std::make_unique<wave::SetLanesGroupCommand>(
            scenario,
            std::vector<std::string>{"lane-request", "lane-ack"},
            "")),
        "batch lane ungrouping reported no effect");
    expect(
        wave::findLane(scenario, "lane-request")->groupId.empty()
            && wave::findLane(scenario, "lane-ack")->groupId.empty(),
        "batch lane ungrouping retained a selected membership");
    std::vector<std::string> ungroupedOrder;
    ungroupedOrder.reserve(scenario.lanes.size());
    for (const auto& lane : scenario.lanes) ungroupedOrder.push_back(lane.id);
    expectEqual(
        ungroupedOrder,
        originalOrder,
        "batch lane ungrouping changed signal order");
    expectEqual(
        stack.undoDescription(),
        std::string{"Remove selected signals from groups"},
        "batch lane ungrouping exposed the wrong Undo description");
    expect(stack.undo(), "batch lane ungrouping undo failed");
    expectEqual(
        scenario,
        original,
        "batch lane ungrouping undo did not restore the exact baseline");

    wave::Lane group;
    group.id = "group-control-bundle";
    group.name = "Control bundle";
    group.kind = wave::LaneKind::Group;
    group.color = "#5c6bc0";
    group.height = 40;
    group.visible = true;
    expect(
        stack.execute(std::make_unique<wave::CreateGroupWithLanesCommand>(
            scenario,
            group,
            std::vector<std::string>{"lane-ack", "lane-reset"})),
        "create group with selected signals reported no effect");
    expectEqual(
        stack.undoDescription(),
        std::string{"Create group with selected signals"},
        "batch group creation exposed the wrong Undo description");
    const auto groupIterator = std::find_if(
        scenario.lanes.begin(),
        scenario.lanes.end(),
        [&group](const wave::Lane& lane) {
            return lane.id == group.id;
        });
    const auto resetIterator = std::find_if(
        scenario.lanes.begin(),
        scenario.lanes.end(),
        [](const wave::Lane& lane) {
            return lane.id == "lane-reset";
        });
    const auto ackIterator = std::find_if(
        scenario.lanes.begin(),
        scenario.lanes.end(),
        [](const wave::Lane& lane) {
            return lane.id == "lane-ack";
        });
    expect(
        groupIterator != scenario.lanes.end()
            && resetIterator == std::next(groupIterator)
            && ackIterator == std::next(resetIterator)
            && resetIterator->groupId == group.id
            && ackIterator->groupId == group.id,
        "batch group creation did not preserve display order below the new Group");
    expect(
        wave::findLane(scenario, "lane-request")->groupId
            == "group-handshake",
        "batch group creation changed an unselected signal membership");
    expectEqual(
        scenario.events,
        originalEvents,
        "batch group creation changed scenario events");
    expectEqual(
        scenario.relations,
        originalRelations,
        "batch group creation changed scenario relations");
    const auto created = scenario;
    expect(stack.undo(), "batch group creation undo failed");
    expectEqual(
        scenario,
        original,
        "batch group creation undo did not restore the exact baseline");
    expect(stack.redo(), "batch group creation redo failed");
    expectEqual(
        scenario,
        created,
        "batch group creation redo did not restore the exact result");

    auto duplicateIdGroup = group;
    duplicateIdGroup.id = "group-duplicate-member-test";
    duplicateIdGroup.name = "Duplicate member test";
    bool rejectedDuplicateMember = false;
    try {
        [[maybe_unused]] wave::CreateGroupWithLanesCommand invalid(
            scenario,
            duplicateIdGroup,
            std::vector<std::string>{"lane-reset", "lane-reset"});
    } catch (const std::invalid_argument&) {
        rejectedDuplicateMember = true;
    }
    expect(
        rejectedDuplicateMember,
        "batch group creation accepted a duplicate member identity");
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

void testLaneSequenceCommand()
{
    wave::Scenario scenario;
    scenario.id = "scenario-sequence";
    scenario.name = "Sequence";
    scenario.duration = 30;
    wave::Lane bus;
    bus.id = "lane-sequence";
    bus.name = "sequence_bus";
    bus.kind = wave::LaneKind::Bus;
    bus.width = 8;
    bus.segments = {
        {"sequence-0", 0, 10, "0x00", {}},
        {"sequence-1", 10, 20, "0x01", {}},
        {"sequence-2", 20, 30, "0x02", {}},
    };
    scenario.lanes.push_back(bus);
    const auto before = scenario;
    const std::vector<wave::LaneSequenceStep> steps{
        {10, 20, "0x0a", {}},
        {20, 30, "0x0b", {}},
        {30, 40, "0x0c", {}},
    };
    const auto valueAt = [](const wave::Lane& lane, const wave::Tick tick) {
        const auto segment = std::find_if(
            lane.segments.begin(),
            lane.segments.end(),
            [tick](const wave::Segment& candidate) {
                return candidate.start <= tick && candidate.end > tick;
            });
        return segment == lane.segments.end()
            ? std::string{}
            : segment->value;
    };

    wave::CommandStack stack;
    expect(
        stack.execute(std::make_unique<wave::SetLaneSequenceCommand>(
            scenario,
            bus.id,
            steps)),
        "lane sequence command reported no effect");
    const auto* edited = wave::findLane(scenario, bus.id);
    expect(edited != nullptr, "lane sequence removed its target");
    expectEqual(
        scenario.duration,
        wave::Tick{40},
        "lane sequence did not extend the scenario exactly once");
    expectEqual(
        valueAt(*edited, 15),
        std::string{"0x0a"},
        "lane sequence first value is incorrect");
    expectEqual(
        valueAt(*edited, 25),
        std::string{"0x0b"},
        "lane sequence second value is incorrect");
    expectEqual(
        valueAt(*edited, 35),
        std::string{"0x0c"},
        "lane sequence final value is incorrect");
    expectEqual(
        stack.size(),
        std::size_t{1},
        "lane sequence created more than one history entry");
    const auto after = scenario;
    expect(stack.undo(), "lane sequence undo failed");
    expectEqual(
        scenario,
        before,
        "lane sequence undo did not restore waveform and End");
    expect(stack.redo(), "lane sequence redo failed");
    expectEqual(
        scenario,
        after,
        "lane sequence redo did not restore its exact result");

    wave::CommandStack noEffectStack;
    const auto noEffectBefore = scenario;
    expect(
        !noEffectStack.execute(
            std::make_unique<wave::SetLaneSequenceCommand>(
                scenario,
                bus.id,
                steps)),
        "identical lane sequence reported an effect");
    expectEqual(
        scenario,
        noEffectBefore,
        "identical lane sequence changed stable model state");
    expectEqual(
        noEffectStack.size(),
        std::size_t{0},
        "identical lane sequence polluted history");

    auto rejectedGap = false;
    try {
        [[maybe_unused]] const wave::SetLaneSequenceCommand invalid(
            scenario,
            bus.id,
            {
                {0, 10, "0x01", {}},
                {11, 20, "0x02", {}},
            });
    } catch (const std::invalid_argument&) {
        rejectedGap = true;
    }
    expect(
        rejectedGap,
        "lane sequence accepted non-contiguous value ranges");
}

void testMultiLaneSequenceCommand()
{
    wave::Scenario scenario;
    scenario.id = "scenario-sequence-batch";
    scenario.name = "Sequence batch";
    scenario.duration = 40;
    for (const auto& [id, name] : std::array{
             std::pair{"lane-sequence-a", "sequence_a"},
             std::pair{"lane-sequence-b", "sequence_b"}}) {
        wave::Lane lane;
        lane.id = id;
        lane.name = name;
        lane.kind = wave::LaneKind::Bit;
        lane.width = 1;
        lane.segments = {
            {std::string{id} + "-initial", 0, 40, "0", {}},
        };
        scenario.lanes.push_back(std::move(lane));
    }
    const auto before = scenario;
    const std::vector<wave::LaneSequenceStep> firstPattern{
        {0, 10, "0", {}},
        {10, 20, "1", {}},
        {20, 30, "0", {}},
        {30, 40, "1", {}},
    };
    const std::vector<wave::LaneSequenceStep> secondPattern{
        {0, 10, "0", {}},
        {10, 20, "0", {}},
        {20, 30, "1", {}},
        {30, 40, "1", {}},
    };
    const auto assignments =
        std::vector<wave::LaneSequenceAssignment>{
            {"lane-sequence-a", firstPattern},
            {"lane-sequence-b", secondPattern},
        };
    const auto valueAt = [](const wave::Lane& lane, const wave::Tick tick) {
        const auto segment = std::find_if(
            lane.segments.begin(),
            lane.segments.end(),
            [tick](const wave::Segment& candidate) {
                return candidate.start <= tick
                    && candidate.end > tick;
            });
        return segment == lane.segments.end()
            ? std::string{}
            : segment->value;
    };

    wave::CommandStack stack;
    expect(
        stack.execute(
            std::make_unique<wave::SetLaneSequencesCommand>(
                scenario,
                assignments)),
        "multi-lane sequence command reported no effect");
    const auto* first =
        wave::findLane(scenario, "lane-sequence-a");
    const auto* second =
        wave::findLane(scenario, "lane-sequence-b");
    expect(
        first && second,
        "multi-lane sequence removed a target");
    const auto expectPattern =
        [&valueAt](
            const wave::Lane& lane,
            const std::array<std::string, 4>& expected) {
        expectEqual(
            valueAt(lane, 5),
            expected.at(0),
            "multi-lane sequence first beat is incorrect");
        expectEqual(
            valueAt(lane, 15),
            expected.at(1),
            "multi-lane sequence second beat is incorrect");
        expectEqual(
            valueAt(lane, 25),
            expected.at(2),
            "multi-lane sequence third beat is incorrect");
        expectEqual(
            valueAt(lane, 35),
            expected.at(3),
            "multi-lane sequence final beat is incorrect");
    };
    expectPattern(
        *first,
        {"0", "1", "0", "1"});
    expectPattern(
        *second,
        {"0", "0", "1", "1"});
    expectEqual(
        stack.size(),
        std::size_t{1},
        "multi-lane sequence created more than one history entry");
    const auto after = scenario;
    expect(
        stack.undo(),
        "multi-lane sequence undo failed");
    expectEqual(
        scenario,
        before,
        "multi-lane sequence undo did not restore every signal");
    expect(
        stack.redo(),
        "multi-lane sequence redo failed");
    expectEqual(
        scenario,
        after,
        "multi-lane sequence redo did not restore its exact result");

    wave::CommandStack noEffectStack;
    expect(
        !noEffectStack.execute(
            std::make_unique<wave::SetLaneSequencesCommand>(
                scenario,
                assignments)),
        "identical multi-lane sequence reported an effect");
    expectEqual(
        noEffectStack.size(),
        std::size_t{0},
        "identical multi-lane sequence polluted history");

    const auto beforeRejected = scenario;
    auto rejectedDuplicate = false;
    try {
        [[maybe_unused]] const wave::SetLaneSequencesCommand invalid(
                scenario,
                {
                {"lane-sequence-a", firstPattern},
                {"lane-sequence-a", firstPattern},
                });
    } catch (const std::invalid_argument&) {
        rejectedDuplicate = true;
    }
    expect(
        rejectedDuplicate,
        "multi-lane sequence accepted a duplicate target");
    expectEqual(
        scenario,
        beforeRejected,
        "rejected multi-lane sequence changed the scenario");
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

    auto clockRangeScenario = before;
    const auto* primaryClock =
        wave::findLane(clockRangeScenario, "lane-clk");
    expect(primaryClock != nullptr, "primary Clock range fixture is missing");
    auto secondaryClock = *primaryClock;
    secondaryClock.id = "lane-clk-secondary";
    secondaryClock.name = "clk_secondary";
    secondaryClock.segments.clear();
    clockRangeScenario.lanes.push_back(std::move(secondaryClock));
    const auto beforeClockRange = clockRangeScenario;
    wave::CommandStack clockRangeStack;
    expect(
        clockRangeStack.execute(
            std::make_unique<wave::SetLaneRangesCommand>(
                clockRangeScenario,
                50'000,
                70'000,
                std::vector<wave::LaneRangeAssignment>{
                    {"lane-clk", "gated", {}},
                    {"lane-clk-secondary", "gated", {}},
                })),
        "multi-Clock Gate range reported no effect");
    const auto* gatedPrimary =
        wave::findLane(clockRangeScenario, "lane-clk");
    const auto* gatedSecondary =
        wave::findLane(clockRangeScenario, "lane-clk-secondary");
    expect(
        gatedPrimary && gatedSecondary,
        "multi-Clock Gate removed a target lane");
    expectEqual(
        valueAt(*gatedPrimary, 60'000),
        std::string{"gated"},
        "primary Clock range was not gated");
    expectEqual(
        valueAt(*gatedSecondary, 60'000),
        std::string{"gated"},
        "secondary Clock range was not gated");
    const auto afterClockGate = clockRangeScenario;
    expect(
        !clockRangeStack.execute(
            std::make_unique<wave::SetLaneRangesCommand>(
                clockRangeScenario,
                50'000,
                70'000,
                std::vector<wave::LaneRangeAssignment>{
                    {"lane-clk", "gated", {}},
                    {"lane-clk-secondary", "gated", {}},
                })),
        "identical multi-Clock Gate polluted history");
    expectEqual(
        clockRangeStack.size(),
        std::size_t{1},
        "identical multi-Clock Gate added an Undo entry");
    expect(clockRangeStack.undo(), "multi-Clock Gate undo failed");
    expectEqual(
        clockRangeScenario,
        beforeClockRange,
        "multi-Clock Gate undo was not exact");
    expect(clockRangeStack.redo(), "multi-Clock Gate redo failed");
    expectEqual(
        clockRangeScenario,
        afterClockGate,
        "multi-Clock Gate redo was not exact");
    expect(
        clockRangeStack.execute(
            std::make_unique<wave::ClearLaneRangesCommand>(
                clockRangeScenario,
                50'000,
                70'000,
                std::vector<std::string>{
                    "lane-clk",
                    "lane-clk-secondary",
                })),
        "multi-Clock Run range reported no effect");
    expect(
        valueAt(
            *wave::findLane(clockRangeScenario, "lane-clk"),
            60'000)
                .empty()
            && valueAt(
                   *wave::findLane(
                       clockRangeScenario,
                       "lane-clk-secondary"),
                   60'000)
                .empty(),
        "multi-Clock Run did not clear both overrides");
    expect(clockRangeStack.undo(), "multi-Clock Run undo failed");
    expectEqual(
        clockRangeScenario,
        afterClockGate,
        "multi-Clock Run undo did not restore both overrides");

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

void testScenarioDurationTruncation()
{
    wave::Scenario scenario;
    scenario.id = "scenario-truncate";
    scenario.name = "Truncate";
    scenario.duration = 100;

    wave::Lane lane;
    lane.id = "lane-truncate";
    lane.name = "signal";
    lane.kind = wave::LaneKind::Bit;
    lane.segments = {
        {"segment-a", 0, 20, "0", {}},
        {"segment-b", 20, 70, "1", {}},
        {"segment-c", 70, 100, "0", {}},
    };
    scenario.lanes.push_back(std::move(lane));

    wave::Event source;
    source.id = "event-source";
    source.laneId = "lane-truncate";
    source.tick = 10;
    wave::Event target = source;
    target.id = "event-target";
    target.tick = 30;
    wave::Event removed = source;
    removed.id = "event-removed";
    removed.tick = 50;
    scenario.events = {source, target, removed};

    wave::Relation retainedRelation;
    retainedRelation.id = "relation-retained";
    retainedRelation.sourceEventId = source.id;
    retainedRelation.targetEventId = target.id;
    retainedRelation.minimumDelay = 0;
    retainedRelation.maximumDelay = 30;
    wave::Relation removedRelation = retainedRelation;
    removedRelation.id = "relation-removed";
    removedRelation.targetEventId = removed.id;
    scenario.relations = {retainedRelation, removedRelation};

    wave::Marker point;
    point.id = "marker-point";
    point.name = "Point";
    point.start = 20;
    point.end = 20;
    wave::Marker boundary = point;
    boundary.id = "marker-boundary";
    boundary.start = 50;
    boundary.end = 50;
    wave::Marker late = point;
    late.id = "marker-late";
    late.start = 70;
    late.end = 70;
    wave::Marker clipped = point;
    clipped.id = "marker-clipped";
    clipped.start = 40;
    clipped.end = 80;
    clipped.kind = wave::MarkerKind::Interval;
    wave::Marker removedInterval = clipped;
    removedInterval.id = "marker-removed";
    removedInterval.start = 50;
    removedInterval.end = 90;
    scenario.markers = {
        point,
        boundary,
        late,
        clipped,
        removedInterval,
    };

    const auto original = scenario;
    wave::CommandStack stack;
    auto command =
        std::make_unique<wave::TruncateScenarioDurationCommand>(
            scenario, 50);
    const auto summary = command->summary();
    expect(
        summary.changesContent()
            && summary.clippedSegmentCount == 1
            && summary.removedSegmentCount == 1
            && summary.removedEventCount == 1
            && summary.removedRelationCount == 1
            && summary.clippedMarkerCount == 1
            && summary.removedMarkerCount == 2,
        "scenario truncation summary is incorrect");
    expect(stack.execute(std::move(command)), "scenario truncation did not execute");
    expectEqual(scenario.duration, wave::Tick{50}, "scenario truncation kept the old end");
    expect(
        scenario.lanes.front().segments.size() == 2
            && scenario.lanes.front().segments.back().id == "segment-b"
            && scenario.lanes.front().segments.back().end == 50,
        "scenario truncation did not clip and remove Segment content");
    expect(
        scenario.events.size() == 2
            && !wave::findEvent(scenario, "event-removed")
            && scenario.relations.size() == 1
            && scenario.relations.front().id == "relation-retained",
        "scenario truncation left an out-of-range Event or dependent Relation");
    const auto clippedMarker = std::find_if(
        scenario.markers.begin(),
        scenario.markers.end(),
        [](const wave::Marker& marker) {
            return marker.id == "marker-clipped";
        });
    expect(
        scenario.markers.size() == 3
            && clippedMarker != scenario.markers.end()
            && clippedMarker->end == 50,
        "scenario truncation did not preserve boundary markers or clip intervals");
    expect(stack.undo() && scenario == original, "scenario truncation undo lost content");
    expect(
        stack.redo() && scenario.duration == 50
            && scenario.lanes.front().segments.size() == 2,
        "scenario truncation redo did not reproduce the edit");

    for (const auto invalidDuration : {wave::Tick{0}, wave::Tick{50}}) {
        auto rejected = false;
        try {
            [[maybe_unused]] wave::TruncateScenarioDurationCommand invalid(
                scenario, invalidDuration);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        expect(rejected, "invalid scenario truncation duration was accepted");
    }
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
    bus.segments = {{"b0", 0, 60, "0x01", {{"copyMeta", "\"kept\""}}}};
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
    expectEqual(editedBus->segments.front().extensions,
                wave::JsonExtensions{{"copyMeta", "\"kept\""}},
                "bus value edit discarded unrelated segment metadata");
    expect(stack.undo(), "segment value undo failed");

    stack.clear();
    scenario = original;
    stack.execute(std::make_unique<wave::CopySegmentCommand>(
        scenario,
        "bus",
        "b0",
        80,
        100));
    editedBus = wave::findLane(scenario, "bus");
    expect(editedBus != nullptr, "copied bus lane is missing");
    const auto copiedSegment = std::find_if(
        editedBus->segments.begin(),
        editedBus->segments.end(),
        [](const wave::Segment& segment) {
            return segment.start == 80 && segment.end == 100;
        });
    expect(copiedSegment != editedBus->segments.end(),
           "segment copy did not create the target interval");
    expectEqual(copiedSegment->value, std::string{"0x01"},
                "segment copy changed the source value");
    expectEqual(copiedSegment->extensions,
                wave::JsonExtensions{{"copyMeta", "\"kept\""}},
                "segment copy lost semantic metadata");
    expect(copiedSegment->id != "b0", "segment copy reused the source identity");
    const auto sourceAfterCopy = std::find_if(
        editedBus->segments.begin(),
        editedBus->segments.end(),
        [](const wave::Segment& segment) { return segment.id == "b0"; });
    expect(sourceAfterCopy != editedBus->segments.end()
               && sourceAfterCopy->start == 0
               && sourceAfterCopy->end == 60,
           "segment copy moved or removed the source interval");
    const auto copiedScenario = scenario;
    expectEqual(stack.undoDescription(), std::string{"Copy segment"},
                "segment copy did not expose a clear Undo label");
    expect(stack.undo(), "segment copy undo failed");
    expectEqual(scenario, original, "segment copy undo did not restore the scenario");
    expect(stack.redo(), "segment copy redo failed");
    expectEqual(scenario, copiedScenario, "segment copy redo was not exact");

    stack.clear();
    scenario = original;
    const auto copiedOntoSelf = stack.execute(
        std::make_unique<wave::CopySegmentCommand>(
            scenario,
            "bus",
            "b0",
            0,
            60));
    expect(!copiedOntoSelf,
           "copying a segment onto the identical range created a false edit");
    expectEqual(scenario, original,
                "no-effect segment copy changed the scenario");
    expectEqual(stack.size(), std::size_t{0},
                "no-effect segment copy entered Undo history");

    wave::synchronizeLaneEventsFromSegments(scenario, "bit");
    const auto noEffectEditScenario = scenario;
    const auto noEffectEditState = stack.stateId();
    const auto editedOntoSelf = stack.execute(
        std::make_unique<wave::EditSegmentCommand>(
            scenario,
            "bit",
            "s1",
            20,
            40,
            "1"));
    expect(!editedOntoSelf,
           "editing a segment to its existing range/value created a false edit");
    expectEqual(scenario, noEffectEditScenario,
                "no-effect segment edit changed the scenario");
    expectEqual(stack.size(), std::size_t{0},
                "no-effect segment edit entered Undo history");
    expectEqual(stack.stateId(), noEffectEditState,
                "no-effect segment edit changed the history state");

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
    expectEqual(
        stack.undoDescription(),
        std::string{"Paste range"},
        "default Paste command description changed");
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
            10'000,
            "Duplicate range")),
        "Paste at End did not extend the Scenario");
    expectEqual(
        endStack.undoDescription(),
        std::string{"Duplicate range"},
        "custom range-write description was not retained");
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

    auto moveScenario = wave::makeDemonstrationProject().scenarios.front();
    const auto moveBefore = moveScenario;
    const auto* moveSourceLane = wave::findLane(moveScenario, "lane-request");
    expect(moveSourceLane != nullptr, "range move source lane is missing");
    const auto moveSourceSegment = std::find_if(
        moveSourceLane->segments.begin(),
        moveSourceLane->segments.end(),
        [](const wave::Segment& segment) {
            return segment.id == "segment-req-high";
        });
    expect(
        moveSourceSegment != moveSourceLane->segments.end(),
        "range move source segment is missing");
    const auto movedSegmentId = moveSourceSegment->id;
    const auto movedEvent = std::find_if(
        moveScenario.events.begin(),
        moveScenario.events.end(),
        [&movedSegmentId](const wave::Event& event) {
            return event.linkedSegmentId == movedSegmentId;
        });
    expect(
        movedEvent != moveScenario.events.end(),
        "range move source Event is missing");
    const auto movedEventId = movedEvent->id;
    const auto relationCountBeforeMove = moveScenario.relations.size();
    auto movedRelative = *moveSourceSegment;
    const auto moveSourceStart = moveSourceSegment->start;
    const auto moveDuration = moveSourceSegment->end - moveSourceSegment->start;
    movedRelative.start = 0;
    movedRelative.end = moveDuration;
    std::vector<wave::CopiedLaneRange> movedRanges{
        {"lane-request", {movedRelative}},
    };
    wave::CommandStack moveStack;
    expect(
        moveStack.execute(std::make_unique<wave::TransferRangeCommand>(
            moveScenario,
            movedRanges,
            moveSourceStart,
            130'000,
            moveDuration,
            wave::RangeTransferMode::Move)),
        "range move reported no effect");
    expectEqual(
        moveStack.undoDescription(),
        std::string{"Move selected range"},
        "range move history description is ambiguous");
    const auto* movedLane = wave::findLane(moveScenario, "lane-request");
    const auto valueAt = [](const wave::Lane& lane, const wave::Tick tick) {
        const auto segment = std::find_if(
            lane.segments.begin(),
            lane.segments.end(),
            [tick](const wave::Segment& candidate) {
                return candidate.start <= tick && tick < candidate.end;
            });
        return segment == lane.segments.end()
            ? std::string{}
            : segment->value;
    };
    expect(
        movedLane
            && valueAt(*movedLane, 85'000).empty()
            && valueAt(*movedLane, 135'000) == "1"
            && valueAt(*movedLane, 175'000) == "1",
        "range move did not clear the source and preserve the complete payload");
    const auto movedStableEvent = std::find_if(
        moveScenario.events.begin(),
        moveScenario.events.end(),
        [&movedEventId](const wave::Event& event) {
            return event.id == movedEventId;
        });
    expect(
        movedStableEvent != moveScenario.events.end()
            && movedStableEvent->tick == 130'000
            && movedStableEvent->linkedSegmentId == movedSegmentId
            && moveScenario.relations.size() == relationCountBeforeMove,
        "range move did not carry a fully selected Segment/Event identity");
    const auto moveAfter = moveScenario;
    expect(moveStack.undo(), "range move undo failed");
    expectEqual(moveScenario, moveBefore, "range move undo was not exact");
    expect(
        !moveStack.execute(std::make_unique<wave::TransferRangeCommand>(
            moveScenario,
            movedRanges,
            moveSourceStart,
            moveSourceStart,
            moveDuration,
            wave::RangeTransferMode::Move)),
        "same-position range move reported an effect");
    expect(moveStack.canRedo(), "same-position range move discarded the real Redo branch");
    expect(moveStack.redo(), "range move Redo was unavailable");
    expectEqual(moveScenario, moveAfter, "range move Redo was not exact");

    auto copyScenario = wave::makeDemonstrationProject().scenarios.front();
    const auto copyBefore = copyScenario;
    const auto* copySourceLane = wave::findLane(copyScenario, "lane-data");
    expect(copySourceLane != nullptr, "range copy source lane is missing");
    const auto copySourceSegment = std::find_if(
        copySourceLane->segments.begin(),
        copySourceLane->segments.end(),
        [](const wave::Segment& segment) {
            return segment.start <= 80'000 && 130'000 <= segment.end;
        });
    expect(
        copySourceSegment != copySourceLane->segments.end(),
        "range copy source segment is missing");
    auto copiedRelative = *copySourceSegment;
    copiedRelative.id.clear();
    copiedRelative.start = 0;
    copiedRelative.end = 50'000;
    std::vector<wave::CopiedLaneRange> copiedRanges{
        {"lane-data", {copiedRelative}},
    };
    wave::CommandStack copyStack;
    expect(
        copyStack.execute(std::make_unique<wave::TransferRangeCommand>(
            copyScenario,
            copiedRanges,
            80'000,
            150'000,
            50'000,
            wave::RangeTransferMode::Copy)),
        "range copy reported no effect");
    expectEqual(
        copyStack.undoDescription(),
        std::string{"Copy selected range"},
        "range copy history description is ambiguous");
    const auto* copiedLane = wave::findLane(copyScenario, "lane-data");
    expect(
        copiedLane
            && valueAt(*copiedLane, 105'000) == copiedRelative.value
            && valueAt(*copiedLane, 175'000) == copiedRelative.value,
        "range copy did not keep the source and write the target");
    const auto copiedTarget = copiedLane
        ? std::find_if(
              copiedLane->segments.begin(),
              copiedLane->segments.end(),
              [](const wave::Segment& segment) {
                  return segment.start <= 175'000 && 175'000 < segment.end;
              })
        : std::vector<wave::Segment>::const_iterator{};
    expect(
        copiedLane
            && copiedTarget != copiedLane->segments.end()
            && copiedTarget->extensions == copiedRelative.extensions,
        "range copy did not preserve Segment extensions");
    const auto copyAfter = copyScenario;
    expect(copyStack.undo(), "range copy undo failed");
    expectEqual(copyScenario, copyBefore, "range copy undo was not exact");
    expect(copyStack.redo(), "range copy redo failed");
    expectEqual(copyScenario, copyAfter, "range copy redo was not exact");

    bool overlappingCopyRejected = false;
    try {
        wave::TransferRangeCommand invalidCopy(
            copyScenario,
            copiedRanges,
            80'000,
            105'000,
            50'000,
            wave::RangeTransferMode::Copy);
    } catch (const std::invalid_argument&) {
        overlappingCopyRejected = true;
    }
    expect(overlappingCopyRejected, "overlapping range copy was accepted");

    auto crossCopyScenario = wave::makeDemonstrationProject().scenarios.front();
    const auto crossCopyBefore = crossCopyScenario;
    auto crossCopiedRelative = movedRelative;
    crossCopiedRelative.id.clear();
    wave::CopiedLaneRange crossCopiedRange;
    crossCopiedRange.laneId = "lane-ack";
    crossCopiedRange.sourceLaneId = "lane-request";
    crossCopiedRange.relativeSegments = {crossCopiedRelative};
    wave::CommandStack crossCopyStack;
    expect(
        crossCopyStack.execute(std::make_unique<wave::TransferRangeCommand>(
            crossCopyScenario,
            std::vector<wave::CopiedLaneRange>{crossCopiedRange},
            moveSourceStart,
            moveSourceStart,
            moveDuration,
            wave::RangeTransferMode::Copy)),
        "same-time cross-lane range copy reported no effect");
    const auto* requestAfterCrossCopy =
        wave::findLane(crossCopyScenario, "lane-request");
    const auto* acknowledgeAfterCrossCopy =
        wave::findLane(crossCopyScenario, "lane-ack");
    expect(
        requestAfterCrossCopy
            && acknowledgeAfterCrossCopy
            && valueAt(*requestAfterCrossCopy, 85'000) == "1"
            && valueAt(*acknowledgeAfterCrossCopy, 85'000) == "1",
        "cross-lane range copy did not keep the source and write the target");
    const auto crossCopyAfter = crossCopyScenario;
    expect(crossCopyStack.undo(), "cross-lane range copy undo failed");
    expectEqual(
        crossCopyScenario,
        crossCopyBefore,
        "cross-lane range copy undo was not exact");
    expect(crossCopyStack.redo(), "cross-lane range copy redo failed");
    expectEqual(
        crossCopyScenario,
        crossCopyAfter,
        "cross-lane range copy redo was not exact");

    auto crossMoveScenario = wave::makeDemonstrationProject().scenarios.front();
    const auto crossMoveBefore = crossMoveScenario;
    wave::CopiedLaneRange crossMovedRange;
    crossMovedRange.laneId = "lane-ack";
    crossMovedRange.sourceLaneId = "lane-request";
    crossMovedRange.relativeSegments = {movedRelative};
    wave::CommandStack crossMoveStack;
    expect(
        crossMoveStack.execute(std::make_unique<wave::TransferRangeCommand>(
            crossMoveScenario,
            std::vector<wave::CopiedLaneRange>{crossMovedRange},
            moveSourceStart,
            moveSourceStart,
            moveDuration,
            wave::RangeTransferMode::Move)),
        "same-time cross-lane range move reported no effect");
    const auto* requestAfterCrossMove =
        wave::findLane(crossMoveScenario, "lane-request");
    const auto* acknowledgeAfterCrossMove =
        wave::findLane(crossMoveScenario, "lane-ack");
    const auto movedAcrossLane = acknowledgeAfterCrossMove
        ? std::find_if(
              acknowledgeAfterCrossMove->segments.begin(),
              acknowledgeAfterCrossMove->segments.end(),
              [](const wave::Segment& segment) {
                  return segment.start <= 85'000 && 85'000 < segment.end;
              })
        : std::vector<wave::Segment>::const_iterator{};
    expect(
        requestAfterCrossMove
            && acknowledgeAfterCrossMove
            && valueAt(*requestAfterCrossMove, 85'000).empty()
            && valueAt(*acknowledgeAfterCrossMove, 85'000) == "1"
            && movedAcrossLane != acknowledgeAfterCrossMove->segments.end()
            && movedAcrossLane->id != movedSegmentId
            && std::none_of(
                crossMoveScenario.events.begin(),
                crossMoveScenario.events.end(),
                [&movedSegmentId](const wave::Event& event) {
                    return event.linkedSegmentId == movedSegmentId;
                })
            && crossMoveScenario.relations.size()
                < crossMoveBefore.relations.size(),
        "cross-lane range move did not clear source dependencies, write the target, or renew identity");
    const auto crossMoveAfter = crossMoveScenario;
    expect(crossMoveStack.undo(), "cross-lane range move undo failed");
    expectEqual(
        crossMoveScenario,
        crossMoveBefore,
        "cross-lane range move undo was not exact");
    expect(crossMoveStack.redo(), "cross-lane range move redo failed");
    expectEqual(
        crossMoveScenario,
        crossMoveAfter,
        "cross-lane range move redo was not exact");

    wave::Scenario chainedMoveScenario;
    chainedMoveScenario.id = "scenario-chained-range-move";
    chainedMoveScenario.name = "Chained range move";
    chainedMoveScenario.duration = 100;
    wave::Lane chainedA;
    chainedA.id = "lane-a";
    chainedA.name = "a";
    chainedA.kind = wave::LaneKind::Bit;
    wave::Segment chainedASegment;
    chainedASegment.id = "segment-a";
    chainedASegment.start = 10;
    chainedASegment.end = 20;
    chainedASegment.value = "1";
    chainedA.segments.push_back(chainedASegment);
    wave::Lane chainedB;
    chainedB.id = "lane-b";
    chainedB.name = "b";
    chainedB.kind = wave::LaneKind::Bit;
    wave::Segment chainedBSegment;
    chainedBSegment.id = "segment-b";
    chainedBSegment.start = 10;
    chainedBSegment.end = 20;
    chainedBSegment.value = "X";
    chainedB.segments.push_back(chainedBSegment);
    wave::Lane chainedC;
    chainedC.id = "lane-c";
    chainedC.name = "c";
    chainedC.kind = wave::LaneKind::Bit;
    chainedMoveScenario.lanes = {chainedA, chainedB, chainedC};
    auto relativeA = chainedASegment;
    relativeA.start = 0;
    relativeA.end = 10;
    auto relativeB = chainedBSegment;
    relativeB.start = 0;
    relativeB.end = 10;
    std::vector<wave::CopiedLaneRange> chainedRanges{
        {"lane-b", {relativeA}, "lane-a"},
        {"lane-c", {relativeB}, "lane-b"},
    };
    wave::CommandStack chainedMoveStack;
    expect(
        chainedMoveStack.execute(
            std::make_unique<wave::TransferRangeCommand>(
                chainedMoveScenario,
                chainedRanges,
                10,
                10,
                10,
                wave::RangeTransferMode::Move)),
        "overlapping cross-lane block move reported no effect");
    const auto* chainedAfterA =
        wave::findLane(chainedMoveScenario, "lane-a");
    const auto* chainedAfterB =
        wave::findLane(chainedMoveScenario, "lane-b");
    const auto* chainedAfterC =
        wave::findLane(chainedMoveScenario, "lane-c");
    expect(
        chainedAfterA
            && chainedAfterB
            && chainedAfterC
            && valueAt(*chainedAfterA, 15).empty()
            && valueAt(*chainedAfterB, 15) == "1"
            && valueAt(*chainedAfterC, 15) == "X",
        "cross-lane block move cleared a target after writing it");

    bool incompatibleCrossLaneRejected = false;
    try {
        auto incompatible = crossCopiedRange;
        incompatible.laneId = "lane-data";
        wave::TransferRangeCommand invalidCrossLane(
            crossCopyScenario,
            std::vector<wave::CopiedLaneRange>{incompatible},
            moveSourceStart,
            moveSourceStart,
            moveDuration,
            wave::RangeTransferMode::Copy);
    } catch (const std::invalid_argument&) {
        incompatibleCrossLaneRejected = true;
    }
    expect(
        incompatibleCrossLaneRejected,
        "incompatible cross-lane range target was accepted");

    auto transferEndScenario = wave::makeDemonstrationProject().scenarios.front();
    const auto transferEndBefore = transferEndScenario;
    const auto transferOriginalEnd = transferEndScenario.duration;
    wave::CommandStack transferEndStack;
    expect(
        transferEndStack.execute(std::make_unique<wave::TransferRangeCommand>(
            transferEndScenario,
            movedRanges,
            moveSourceStart,
            transferOriginalEnd,
            moveDuration,
            wave::RangeTransferMode::Copy)),
        "range copy at End did not extend the Scenario");
    expectEqual(
        transferEndScenario.duration,
        transferOriginalEnd + moveDuration,
        "range copy at End truncated the selected width");
    expect(
        transferEndStack.undo(),
        "extended range copy undo failed");
    expectEqual(
        transferEndScenario,
        transferEndBefore,
        "extended range copy undo did not restore End and waveform");
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

    auto recoveryScenario = scenario;
    const auto recoveryMarker = std::find_if(
        recoveryScenario.markers.begin(),
        recoveryScenario.markers.end(),
        [](const wave::Marker& candidate) {
            return candidate.id == "marker-test";
        });
    expect(
        recoveryMarker != recoveryScenario.markers.end(),
        "indexed marker recovery fixture is missing");
    const auto recoveryIndex = static_cast<std::size_t>(
        std::distance(
            recoveryScenario.markers.begin(),
            recoveryMarker));
    const auto expectedRecoveryMarker = *recoveryMarker;
    auto replacementRecoveryMarker = expectedRecoveryMarker;
    replacementRecoveryMarker.id = "marker-test-recovered";
    replacementRecoveryMarker.note = "recovered";
    wave::CommandStack recoveryStack;
    expect(
        recoveryStack.execute(
            std::make_unique<wave::ChangeMarkerAtIndexCommand>(
                recoveryScenario,
                recoveryIndex,
                expectedRecoveryMarker,
                replacementRecoveryMarker)),
        "indexed marker recovery command reported no effect");
    expect(
        recoveryScenario.markers.at(recoveryIndex)
            == replacementRecoveryMarker,
        "indexed marker recovery command changed the wrong Marker");
    expect(
        recoveryStack.undo()
            && recoveryScenario.markers.at(recoveryIndex)
                == expectedRecoveryMarker,
        "indexed marker recovery undo was not exact");
    expect(
        recoveryStack.redo()
            && recoveryScenario.markers.at(recoveryIndex)
                == replacementRecoveryMarker,
        "indexed marker recovery redo was not exact");
    expect(
        recoveryStack.execute(
            std::make_unique<wave::RemoveMarkerAtIndexCommand>(
                recoveryScenario,
                recoveryIndex,
                replacementRecoveryMarker)),
        "indexed marker removal command reported no effect");
    expectEqual(
        recoveryScenario.markers.size(),
        scenario.markers.size() - 1,
        "indexed marker removal changed the wrong count");
    expect(
        recoveryStack.undo()
            && recoveryScenario.markers.at(recoveryIndex)
                == replacementRecoveryMarker,
        "indexed marker removal undo was not exact");
    expect(
        recoveryStack.redo(),
        "indexed marker removal redo failed");

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

    auto dontCareProject = maskedProject;
    auto* dontCareData = wave::findLane(
        dontCareProject.scenarios.front(),
        "lane-data");
    expect(dontCareData != nullptr, "don't-care data lane is missing");
    dontCareData->segments.at(1).extensions["waveWorkbench.busPreset"] =
        "\"dont-care\"";
    dataWindow.defaultRule.busMask.clear();
    const auto dontCareResult = wave::compareScenario(
        dontCareProject,
        dontCareProject.scenarios.front(),
        trace,
        reference,
        dataWindow);
    expect(dontCareResult.matches(),
           "semantic don't-care segment did not ignore its compare interval");

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
                return difference.kind
                    == wave::CompareDifferenceKind::UnmappedSignal
                    && difference.laneId == "lane-request"
                    && difference.traceSignalId.empty()
                    && difference.message.find(
                           "No actual signal mapping")
                        != std::string::npos;
            }),
        "unconfigured signal mapping was not reported distinctly");
    expect(
        std::any_of(
            diagnostics.differences.begin(),
            diagnostics.differences.end(),
            [](const wave::CompareDifference& difference) {
                return difference.kind == wave::CompareDifferenceKind::WidthMismatch;
            }),
        "width mismatch was not reported");

    auto staleReference = reference;
    staleReference.signalMapping["lane-request"] =
        "tb.dut.removed_request";
    const auto staleMapping = wave::compareScenario(
        project,
        project.scenarios.front(),
        trace,
        staleReference);
    expect(
        std::any_of(
            staleMapping.differences.begin(),
            staleMapping.differences.end(),
            [](const wave::CompareDifference& difference) {
                return difference.kind
                    == wave::CompareDifferenceKind::MissingSignal
                    && difference.laneId == "lane-request"
                    && difference.traceSignalId
                        == "tb.dut.removed_request"
                    && difference.actual
                        == "tb.dut.removed_request"
                    && difference.message.find(
                           "does not exist in the loaded trace")
                        != std::string::npos;
            }),
        "stale mapped signal was not reported with its configured target");

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

void testAutomationContracts()
{
    auto source = wave::makeDemonstrationProject();
    source.scenarios.front().lanes.front().extensions.emplace(
        "automationMeta",
        R"({"enabled":true,"rank":2})");
    const auto sourceBefore = source;
    expect(!source.scenarios.empty(), "automation fixture has no scenario");
    const auto& sourceScenario = source.scenarios.front();
    const auto bit = std::find_if(
        sourceScenario.lanes.begin(),
        sourceScenario.lanes.end(),
        [](const wave::Lane& lane) {
            return lane.kind == wave::LaneKind::Bit;
        });
    expect(bit != sourceScenario.lanes.end(), "automation fixture has no Bit lane");

    const auto capabilities = wave::describeAutomationCapabilities();
    std::set<std::string> capabilityCommands;
    for (const auto& value :
         capabilities.json.value(QStringLiteral("commands")).toArray()) {
        capabilityCommands.insert(
            value.toObject()
                .value(QStringLiteral("name"))
                .toString()
                .toStdString());
    }
    std::set<std::string> capabilityOperations;
    for (const auto& value :
         capabilities.json.value(QStringLiteral("operations")).toArray()) {
        capabilityOperations.insert(
            value.toObject()
                .value(QStringLiteral("name"))
                .toString()
                .toStdString());
    }
    const auto durationCapabilities =
        capabilities.json.value(
            QStringLiteral("scenarioDuration")).toObject();
    expect(
        capabilities.ok()
            && capabilities.json.value(QStringLiteral("schema")).toString()
                == QString::fromLatin1(
                    wave::AutomationCapabilitiesSchema)
            && capabilities.json.value(
                   QStringLiteral("commandCount")).toInt()
                == 11
            && capabilities.json.value(
                   QStringLiteral("operationCount")).toInt()
                == 33
            && capabilityCommands.contains("edges")
            && capabilityCommands.contains("markers")
            && capabilityCommands.contains("relations")
            && capabilityOperations.size() == 33
            && capabilityOperations.contains("update-group")
            && capabilityOperations.contains("move-group")
            && capabilityOperations.contains("delete-group")
            && capabilityOperations.contains("duplicate-signal")
            && capabilityOperations.contains("delete-event")
            && capabilityOperations.contains("repair-event-link")
            && capabilityOperations.contains("clear-event-cycle")
            && capabilityOperations.contains("repair-event-clock")
            && capabilityOperations.contains("repair-relation-clock")
            && capabilityOperations.contains("repair-trace-identity")
            && capabilityOperations.contains("repair-trace-reference")
            && capabilityOperations.contains("repair-trace-mapping")
            && capabilityOperations.contains("repair-lane-clock")
            && capabilityOperations.contains("repair-lane-group")
            && durationCapabilities.value(
                   QStringLiteral("supportsExtension")).toBool()
            && durationCapabilities.value(
                   QStringLiteral("supportsSafeShrink")).toBool()
            && durationCapabilities.value(
                   QStringLiteral("destructiveShrinkOptInField")).toString()
                == QStringLiteral("truncate")
            && durationCapabilities.value(
                   QStringLiteral("reportsTruncationCounts")).toBool()
            && capabilityOperations.contains("add-relation")
            && capabilityOperations.contains("add-marker")
            && capabilities.json
                   .value(QStringLiteral("operationBatch"))
                   .toObject()
                   .value(QStringLiteral("atomic"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("structuredErrors"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("structuralIdentityValidation"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("markerIntegrityValidation"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("actionableValidationReferences"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("structuredRelationValidation"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("structuredWaveformValidation"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("structuredEventValidation"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("eventDeletion"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("eventLinkRepair"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("eventCycleRepair"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("eventClockRepair"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("relationClockRepair"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("traceIdentityValidation"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("traceIdentityRepair"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("traceReferenceValidation"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("traceReferenceRepair"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("traceMappingValidation"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("traceMappingRepair"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("traceRepairReference"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("laneClockRepair"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("laneGroupRepair"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("nonRegressiveApplyValidation"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("relationReadyEdgeQuery"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("relationEndpointRepair"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("relationRepairReference"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("compactMarkerQuery"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("markerRepairReference"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("features"))
                   .toObject()
                   .value(QStringLiteral("compactRelationQuery"))
                   .toBool()
            && capabilities.json
                   .value(QStringLiteral("values"))
                   .toObject()
                   .value(QStringLiteral("edgeKinds"))
                   .toArray()
                   .size()
                == 4,
        "automation capabilities are incomplete or inconsistent");

    wave::AutomationNewProjectOptions newOptions;
    newOptions.projectName = "CLI sequence";
    newOptions.scenarioName = "Stimulus";
    newOptions.duration = 60'000;
    const auto created =
        wave::createProjectForAutomation(newOptions);
    const auto repeatedCreated =
        wave::createProjectForAutomation(newOptions);
    expect(created.ok(), created.error.toStdString());
    expect(
        repeatedCreated.ok()
            && wave::serializeProject(*created.project)
                == wave::serializeProject(*repeatedCreated.project)
            && created.project->name == "CLI sequence"
            && created.project->scenarios.size() == 1
            && created.project->scenarios.front().name == "Stimulus"
            && created.project->scenarios.front().duration == 60'000
            && created.project->scenarios.front().lanes.empty(),
        "automation new-project result is not deterministic or blank");
    auto invalidNewOptions = newOptions;
    invalidNewOptions.projectName = " ";
    expect(
        !wave::createProjectForAutomation(invalidNewOptions).ok(),
        "automation accepted an empty new-project name");

    const QJsonObject groupLifecycleBatch{
        {QStringLiteral("schema"),
         QString::fromLatin1(wave::AutomationBatchSchema)},
        {QStringLiteral("operations"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("add-group")},
                 {QStringLiteral("id"), QStringLiteral("group-alpha")},
                 {QStringLiteral("name"), QStringLiteral("Alpha")},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("add-signal")},
                 {QStringLiteral("id"), QStringLiteral("lane-member")},
                 {QStringLiteral("name"), QStringLiteral("member")},
                 {QStringLiteral("kind"), QStringLiteral("bit")},
                 {QStringLiteral("groupId"), QStringLiteral("ALPHA")},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("add-group")},
                 {QStringLiteral("id"), QStringLiteral("group-beta")},
                 {QStringLiteral("name"), QStringLiteral("Beta")},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("update-group")},
                 {QStringLiteral("groupId"), QStringLiteral("alpha")},
                 {QStringLiteral("name"), QStringLiteral("Timing")},
                 {QStringLiteral("color"), QStringLiteral("#123ABC")},
                 {QStringLiteral("height"), 64},
                 {QStringLiteral("visible"), false},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("move-group")},
                 {QStringLiteral("groupId"), QStringLiteral("TIMING")},
                 {QStringLiteral("afterLaneId"), QStringLiteral("BETA")},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("update-group")},
                 {QStringLiteral("groupId"), QStringLiteral("Timing")},
                 {QStringLiteral("name"), QStringLiteral("Timing")},
                 {QStringLiteral("color"), QStringLiteral("#123abc")},
                 {QStringLiteral("height"), 64},
                 {QStringLiteral("visible"), false},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("move-group")},
                 {QStringLiteral("groupId"), QStringLiteral("timing")},
                 {QStringLiteral("destinationIndex"), 2},
             },
         }},
    };
    const auto grouped = wave::applyAutomationBatch(
        *created.project, groupLifecycleBatch);
    const auto repeatedGrouped = wave::applyAutomationBatch(
        *created.project, groupLifecycleBatch);
    expect(grouped.ok(), grouped.error.toStdString());
    const auto* timingGroup = wave::findLane(
        grouped.project->scenarios.front(), "group-alpha");
    const auto* memberLane = wave::findLane(
        grouped.project->scenarios.front(), "lane-member");
    expect(
        repeatedGrouped.ok()
            && wave::serializeProject(*grouped.project)
                == wave::serializeProject(*repeatedGrouped.project)
            && timingGroup
            && timingGroup->kind == wave::LaneKind::Group
            && timingGroup->name == "Timing"
            && timingGroup->color == "#123abc"
            && timingGroup->height == 64
            && !timingGroup->visible
            && memberLane
            && memberLane->groupId == "group-alpha"
            && grouped.project->scenarios.front().lanes.at(2).id
                == "group-alpha"
            && !grouped.json
                    .value(QStringLiteral("operations"))
                    .toArray()
                    .at(5)
                    .toObject()
                    .value(QStringLiteral("changed"))
                    .toBool()
            && !grouped.json
                    .value(QStringLiteral("operations"))
                    .toArray()
                    .at(6)
                    .toObject()
                    .value(QStringLiteral("changed"))
                    .toBool(),
        "automation Group lifecycle is not deterministic or idempotent");

    const auto removedGroup = wave::applyAutomationBatch(
        *grouped.project,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"),
                      QStringLiteral("delete-group")},
                     {QStringLiteral("groupId"),
                      QStringLiteral("TIMING")},
                 },
             }},
        });
    expect(removedGroup.ok(), removedGroup.error.toStdString());
    const auto removedReport =
        removedGroup.json
            .value(QStringLiteral("operations"))
            .toArray()
            .at(0)
            .toObject();
    const auto* ungroupedMember = wave::findLane(
        removedGroup.project->scenarios.front(), "lane-member");
    expect(
        !wave::findLane(
            removedGroup.project->scenarios.front(), "group-alpha")
            && ungroupedMember
            && ungroupedMember->groupId.empty()
            && removedReport
                   .value(QStringLiteral("groupId"))
                   .toString()
                == QStringLiteral("group-alpha")
            && removedReport
                   .value(QStringLiteral("ungroupedSignalCount"))
                   .toInt()
                == 1
            && removedReport
                   .value(QStringLiteral("ungroupedLaneIds"))
                   .toArray()
                   .at(0)
                   .toString()
                == QStringLiteral("lane-member"),
        "automation delete-group did not report and preserve members");

    const auto invalidGroupBatch = wave::applyAutomationBatch(
        *grouped.project,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"),
                      QStringLiteral("update-group")},
                     {QStringLiteral("groupId"),
                      QStringLiteral("Timing")},
                     {QStringLiteral("visible"), true},
                 },
                 QJsonObject{
                     {QStringLiteral("op"),
                      QStringLiteral("delete-group")},
                     {QStringLiteral("groupId"),
                      QStringLiteral("member")},
                 },
             }},
        });
    const auto conflictingGroupName = wave::applyAutomationBatch(
        *grouped.project,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"),
                      QStringLiteral("update-group")},
                     {QStringLiteral("groupId"),
                      QStringLiteral("Timing")},
                     {QStringLiteral("name"),
                      QStringLiteral("Beta")},
                 },
             }},
        });
    expect(
        !invalidGroupBatch.ok()
            && invalidGroupBatch.failedOperation == 1
            && !invalidGroupBatch.project
            && !conflictingGroupName.ok()
            && conflictingGroupName.failedOperation == 0
            && !conflictingGroupName.project
            && wave::serializeProject(*grouped.project)
                == wave::serializeProject(*repeatedGrouped.project),
        "invalid Group lifecycle batch was not rejected atomically");

    const auto busSource = std::find_if(
        sourceScenario.lanes.begin(),
        sourceScenario.lanes.end(),
        [](const wave::Lane& lane) {
            return lane.kind == wave::LaneKind::Bus;
        });
    const auto clockSource = std::find_if(
        sourceScenario.lanes.begin(),
        sourceScenario.lanes.end(),
        [](const wave::Lane& lane) {
            return lane.kind == wave::LaneKind::Clock;
        });
    expect(
        busSource != sourceScenario.lanes.end()
            && clockSource != sourceScenario.lanes.end(),
        "automation fixture has no Bus or Clock lane to duplicate");
    const auto* sourceClockDomain =
        wave::findClock(source, clockSource->clockDomainId);
    expect(
        sourceClockDomain != nullptr,
        "automation Clock fixture has no ClockDomain");

    const QJsonObject duplicateSignalBatch{
        {QStringLiteral("schema"),
         QString::fromLatin1(wave::AutomationBatchSchema)},
        {QStringLiteral("operations"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral("duplicate-signal")},
                 {QStringLiteral("laneId"),
                  QString::fromStdString(busSource->name).toUpper()},
                 {QStringLiteral("name"),
                  QStringLiteral("payload_copy")},
                 {QStringLiteral("groupId"), QString{}},
             },
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral("duplicate-signal")},
                 {QStringLiteral("laneId"),
                  QString::fromStdString(clockSource->id)},
                 {QStringLiteral("name"),
                  QStringLiteral("clk_copy_cli")},
                 {QStringLiteral("newClockId"),
                  QStringLiteral("clock-copy-cli")},
                 {QStringLiteral("insertionIndex"), 0},
             },
         }},
    };
    const auto duplicated = wave::applyAutomationBatch(
        source, duplicateSignalBatch);
    const auto repeatedDuplicated = wave::applyAutomationBatch(
        source, duplicateSignalBatch);
    expect(duplicated.ok(), duplicated.error.toStdString());
    const auto duplicateReports =
        duplicated.json
            .value(QStringLiteral("operations"))
            .toArray();
    const auto busDuplicateId =
        duplicateReports.at(0)
            .toObject()
            .value(QStringLiteral("laneId"))
            .toString()
            .toStdString();
    const auto clockDuplicateId =
        duplicateReports.at(1)
            .toObject()
            .value(QStringLiteral("laneId"))
            .toString()
            .toStdString();
    const auto* duplicatedBus = wave::findLane(
        duplicated.project->scenarios.front(), busDuplicateId);
    const auto* duplicatedClockLane = wave::findLane(
        duplicated.project->scenarios.front(), clockDuplicateId);
    const auto* duplicatedClockDomain = wave::findClock(
        *duplicated.project, "clock-copy-cli");
    const auto segmentsMatchExceptIdentity =
        [](const wave::Lane& left, const wave::Lane& right) {
            if (left.segments.size() != right.segments.size()) {
                return false;
            }
            for (std::size_t index = 0;
                 index < left.segments.size();
                 ++index) {
                const auto& a = left.segments.at(index);
                const auto& b = right.segments.at(index);
                if (a.id == b.id
                    || a.start != b.start
                    || a.end != b.end
                    || a.value != b.value
                    || a.extensions != b.extensions) {
                    return false;
                }
            }
            return true;
        };
    const auto noDuplicateMappings = std::none_of(
        duplicated.project->importedTraces.begin(),
        duplicated.project->importedTraces.end(),
        [&busDuplicateId, &clockDuplicateId](
            const wave::ImportedTrace& trace) {
            return trace.signalMapping.contains(busDuplicateId)
                || trace.signalMapping.contains(clockDuplicateId);
        });
    const auto noDuplicateEvents = std::none_of(
        duplicated.project->scenarios.front().events.begin(),
        duplicated.project->scenarios.front().events.end(),
        [&busDuplicateId, &clockDuplicateId](
            const wave::Event& event) {
            return event.laneId == busDuplicateId
                || event.laneId == clockDuplicateId;
        });
    expect(
        repeatedDuplicated.ok()
            && wave::serializeProject(*duplicated.project)
                == wave::serializeProject(*repeatedDuplicated.project)
            && duplicatedBus
            && duplicatedClockLane
            && duplicatedClockDomain
            && duplicatedBus->name == "payload_copy"
            && duplicatedBus->kind == busSource->kind
            && duplicatedBus->width == busSource->width
            && duplicatedBus->isSigned == busSource->isSigned
            && duplicatedBus->radix == busSource->radix
            && duplicatedBus->enumMap == busSource->enumMap
            && duplicatedBus->clockDomainId
                == busSource->clockDomainId
            && duplicatedBus->groupId.empty()
            && duplicatedBus->visible
            && duplicatedBus->color != busSource->color
            && segmentsMatchExceptIdentity(
                *busSource, *duplicatedBus)
            && duplicatedClockLane->name == "clk_copy_cli"
            && duplicatedClockLane->clockDomainId
                == "clock-copy-cli"
            && duplicatedClockLane->color != clockSource->color
            && segmentsMatchExceptIdentity(
                *clockSource, *duplicatedClockLane)
            && duplicatedClockDomain->id == "clock-copy-cli"
            && duplicatedClockDomain->name == "clk_copy_cli"
            && duplicatedClockDomain->period
                == sourceClockDomain->period
            && duplicatedClockDomain->phase
                == sourceClockDomain->phase
            && duplicatedClockDomain->dutyCycle
                == sourceClockDomain->dutyCycle
            && duplicatedClockDomain->activeEdge
                == sourceClockDomain->activeEdge
            && duplicatedClockDomain->resetRelation
                == sourceClockDomain->resetRelation
            && duplicated.project->clockDomains.size()
                == source.clockDomains.size() + 1
            && duplicated.project->scenarios.front().events.size()
                == sourceScenario.events.size()
            && duplicated.project->scenarios.front().relations.size()
                == sourceScenario.relations.size()
            && noDuplicateEvents
            && noDuplicateMappings
            && duplicateReports.at(0)
                   .toObject()
                   .value(QStringLiteral("copiedEventCount"))
                   .toInt()
                == 0
            && duplicateReports.at(1)
                   .toObject()
                   .value(QStringLiteral("independentClockDomain"))
                   .toBool(),
        "automation duplicate-signal did not preserve safe copy semantics");

    const QJsonObject automaticDuplicateNameBatch{
        {QStringLiteral("schema"),
         QString::fromLatin1(wave::AutomationBatchSchema)},
        {QStringLiteral("operations"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral("duplicate-signal")},
                 {QStringLiteral("laneId"),
                  QString::fromStdString(busSource->id)},
             },
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral("duplicate-signal")},
                 {QStringLiteral("laneId"),
                  QString::fromStdString(busSource->id)},
             },
         }},
    };
    const auto automaticDuplicateNames =
        wave::applyAutomationBatch(
            source, automaticDuplicateNameBatch);
    const auto repeatedAutomaticDuplicateNames =
        wave::applyAutomationBatch(
            source, automaticDuplicateNameBatch);
    expect(
        automaticDuplicateNames.ok()
            && repeatedAutomaticDuplicateNames.ok()
            && wave::serializeProject(
                   *automaticDuplicateNames.project)
                == wave::serializeProject(
                    *repeatedAutomaticDuplicateNames.project)
            && automaticDuplicateNames.json
                   .value(QStringLiteral("operations"))
                   .toArray()
                   .at(0)
                   .toObject()
                   .value(QStringLiteral("name"))
                   .toString()
                == QString::fromStdString(
                    busSource->name + "_copy")
            && automaticDuplicateNames.json
                   .value(QStringLiteral("operations"))
                   .toArray()
                   .at(1)
                   .toObject()
                   .value(QStringLiteral("name"))
                   .toString()
                == QString::fromStdString(
                    busSource->name + "_copy_2"),
        "automation duplicate-signal did not assign unique deterministic names");

    const auto invalidDuplicateClockField =
        wave::applyAutomationBatch(
            source,
            QJsonObject{
                {QStringLiteral("schema"),
                 QString::fromLatin1(wave::AutomationBatchSchema)},
                {QStringLiteral("operations"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("op"),
                          QStringLiteral("duplicate-signal")},
                         {QStringLiteral("laneId"),
                          QString::fromStdString(busSource->id)},
                         {QStringLiteral("newClockId"),
                          QStringLiteral("not-a-bus-clock")},
                     },
                 }},
            });
    const auto conflictingDuplicateName =
        wave::applyAutomationBatch(
            source,
            QJsonObject{
                {QStringLiteral("schema"),
                 QString::fromLatin1(wave::AutomationBatchSchema)},
                {QStringLiteral("operations"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("op"),
                          QStringLiteral("duplicate-signal")},
                         {QStringLiteral("laneId"),
                          QString::fromStdString(busSource->id)},
                         {QStringLiteral("name"),
                          QString::fromStdString(busSource->name)},
                     },
                 }},
            });
    const auto invalidDuplicateGroupBatch =
        wave::applyAutomationBatch(
            *grouped.project,
            QJsonObject{
                {QStringLiteral("schema"),
                 QString::fromLatin1(wave::AutomationBatchSchema)},
                {QStringLiteral("operations"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("op"),
                          QStringLiteral("duplicate-signal")},
                         {QStringLiteral("laneId"),
                          QStringLiteral("member")},
                     },
                     QJsonObject{
                         {QStringLiteral("op"),
                          QStringLiteral("duplicate-signal")},
                         {QStringLiteral("laneId"),
                          QStringLiteral("Timing")},
                     },
                 }},
            });
    expect(
        !invalidDuplicateClockField.ok()
            && invalidDuplicateClockField.failedOperation == 0
            && !invalidDuplicateClockField.project
            && !conflictingDuplicateName.ok()
            && conflictingDuplicateName.failedOperation == 0
            && !conflictingDuplicateName.project
            && !invalidDuplicateGroupBatch.ok()
            && invalidDuplicateGroupBatch.failedOperation == 1
            && !invalidDuplicateGroupBatch.project
            && wave::serializeProject(*grouped.project)
                == wave::serializeProject(*repeatedGrouped.project),
        "invalid duplicate-signal batch was not rejected atomically");

    const QJsonObject sequenceBatch{
        {QStringLiteral("schema"),
         QString::fromLatin1(wave::AutomationBatchSchema)},
        {QStringLiteral("operations"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("add-signal")},
                 {QStringLiteral("id"), QStringLiteral("lane-clk")},
                 {QStringLiteral("clockId"), QStringLiteral("clock-main")},
                 {QStringLiteral("name"), QStringLiteral("clk")},
                 {QStringLiteral("kind"), QStringLiteral("clock")},
                 {QStringLiteral("period"), QStringLiteral("10 ns")},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("add-signal")},
                 {QStringLiteral("id"), QStringLiteral("lane-request")},
                 {QStringLiteral("name"), QStringLiteral("request")},
                 {QStringLiteral("kind"), QStringLiteral("bit")},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("add-signal")},
                 {QStringLiteral("id"), QStringLiteral("lane-data")},
                 {QStringLiteral("name"), QStringLiteral("data")},
                 {QStringLiteral("kind"), QStringLiteral("bus")},
                 {QStringLiteral("width"), 8},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("set-sequence")},
                 {QStringLiteral("start"), QStringLiteral("cycle 0")},
                 {QStringLiteral("stepCycles"), 1},
                 {QStringLiteral("repeat"), 2},
                 {QStringLiteral("sequences"),
                  QJsonArray{
                      QJsonObject{
                          {QStringLiteral("laneId"),
                           QStringLiteral("lane-request")},
                          {QStringLiteral("values"),
                           QJsonArray{
                               QStringLiteral("0"),
                               QStringLiteral("1"),
                               QStringLiteral("1"),
                               QStringLiteral("0"),
                           }},
                      },
                      QJsonObject{
                          {QStringLiteral("laneId"),
                           QStringLiteral("lane-data")},
                          {QStringLiteral("values"),
                           QJsonArray{
                               QStringLiteral("0x00"),
                               QStringLiteral("0x12"),
                               QStringLiteral("0x34"),
                               QStringLiteral("0x00"),
                           }},
                      },
                  }},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("assert-value")},
                 {QStringLiteral("laneId"),
                  QStringLiteral("lane-request")},
                 {QStringLiteral("at"), QStringLiteral("cycle 1")},
                 {QStringLiteral("value"), QStringLiteral("1")},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("assert-value")},
                 {QStringLiteral("laneId"), QStringLiteral("lane-data")},
                 {QStringLiteral("at"), QStringLiteral("cycle 2")},
                 {QStringLiteral("value"), QStringLiteral("0x34")},
             },
         }},
    };
    const auto sequenced = wave::applyAutomationBatch(
        *created.project, sequenceBatch);
    const auto repeatedSequenced = wave::applyAutomationBatch(
        *created.project, sequenceBatch);
    expect(sequenced.ok(), sequenced.error.toStdString());
    const auto& sequencedScenario =
        sequenced.project->scenarios.front();
    const auto sequenceReport =
        sequenced.json.value(QStringLiteral("operations"))
            .toArray()
            .at(3)
            .toObject();
    const auto changeSummary =
        sequenced.json.value(QStringLiteral("changes")).toObject();
    const auto scenarioChanges =
        changeSummary.value(QStringLiteral("scenarios")).toArray();
    const auto sequenceRequest = wave::sampleProjectForAutomation(
        *sequenced.project,
        10'000,
        sequencedScenario.id,
        {"lane-request"});
    const auto sequenceData = wave::sampleProjectForAutomation(
        *sequenced.project,
        20'000,
        sequencedScenario.id,
        {"lane-data"});
    expect(
        repeatedSequenced.ok()
            && wave::serializeProject(*sequenced.project)
                == wave::serializeProject(*repeatedSequenced.project)
            && sequencedScenario.duration == 80'000
            && sequencedScenario.lanes.size() == 3
            && sequenceReport.value(QStringLiteral("stepTick")).toString()
                == QStringLiteral("10000")
            && sequenceReport.value(QStringLiteral("valueCellCount")).toString()
                == QStringLiteral("16")
            && changeSummary
                    .value(QStringLiteral("addedClockIds"))
                    .toArray()
                    .size()
                == 1
            && scenarioChanges.size() == 1
            && scenarioChanges.at(0)
                    .toObject()
                    .value(QStringLiteral("addedLaneIds"))
                    .toArray()
                    .size()
                == 3
            && sequenceRequest.ok()
            && sequenceRequest.json
                    .value(QStringLiteral("samples"))
                    .toArray()
                    .at(0)
                    .toObject()
                    .value(QStringLiteral("value"))
                    .toString()
                == QStringLiteral("1")
            && sequenceData.ok()
            && sequenceData.json
                    .value(QStringLiteral("samples"))
                    .toArray()
                    .at(0)
                    .toObject()
                    .value(QStringLiteral("value"))
                    .toString()
                == QStringLiteral("0x34"),
        "new-project sequence or concise change summary is incorrect");

    auto namedOperations =
        sequenceBatch.value(QStringLiteral("operations")).toArray();
    for (const auto index : {0, 1, 2}) {
        auto operation = namedOperations.at(index).toObject();
        operation.remove(QStringLiteral("id"));
        if (index == 0) operation.remove(QStringLiteral("clockId"));
        if (index == 2) {
            operation.insert(
                QStringLiteral("clockDomainId"),
                QStringLiteral("CLK"));
        }
        namedOperations.replace(index, operation);
    }
    auto namedSequence = namedOperations.at(3).toObject();
    namedSequence.insert(
        QStringLiteral("clockId"), QStringLiteral("clk"));
    auto namedSequences =
        namedSequence.value(QStringLiteral("sequences")).toArray();
    auto namedRequest = namedSequences.at(0).toObject();
    namedRequest.insert(
        QStringLiteral("laneId"), QStringLiteral("REQUEST"));
    namedSequences.replace(0, namedRequest);
    auto namedData = namedSequences.at(1).toObject();
    namedData.insert(
        QStringLiteral("laneId"), QStringLiteral("Data"));
    namedSequences.replace(1, namedData);
    namedSequence.insert(QStringLiteral("sequences"), namedSequences);
    namedOperations.replace(3, namedSequence);
    auto namedRequestAssertion = namedOperations.at(4).toObject();
    namedRequestAssertion.insert(
        QStringLiteral("laneId"), QStringLiteral("request"));
    namedRequestAssertion.insert(
        QStringLiteral("clockId"), QStringLiteral("CLK"));
    namedOperations.replace(4, namedRequestAssertion);
    auto namedDataAssertion = namedOperations.at(5).toObject();
    namedDataAssertion.insert(
        QStringLiteral("laneId"), QStringLiteral("DATA"));
    namedOperations.replace(5, namedDataAssertion);
    const QJsonObject namedSequenceBatch{
        {QStringLiteral("schema"),
         QString::fromLatin1(wave::AutomationBatchSchema)},
        {QStringLiteral("scenarioId"), QStringLiteral("STIMULUS")},
        {QStringLiteral("operations"), namedOperations},
    };
    const auto namedSequenced = wave::applyAutomationBatch(
        *created.project, namedSequenceBatch);
    const auto repeatedNamedSequenced = wave::applyAutomationBatch(
        *created.project, namedSequenceBatch);
    expect(namedSequenced.ok(), namedSequenced.error.toStdString());
    const auto& namedLanes =
        namedSequenced.project->scenarios.front().lanes;
    const auto namedRequestLane = std::find_if(
        namedLanes.begin(),
        namedLanes.end(),
        [](const wave::Lane& lane) {
            return lane.name == "request";
        });
    const auto namedDataLane = std::find_if(
        namedLanes.begin(),
        namedLanes.end(),
        [](const wave::Lane& lane) {
            return lane.name == "data";
        });
    expect(
        namedRequestLane != namedLanes.end()
            && namedDataLane != namedLanes.end(),
        "name-addressed operations did not create both target Lanes");
    const auto namedRequestSample =
        wave::sampleProjectForAutomation(
            *namedSequenced.project,
            10'000,
            namedSequenced.project->scenarios.front().id,
            {namedRequestLane->id});
    const auto namedDataSample =
        wave::sampleProjectForAutomation(
            *namedSequenced.project,
            20'000,
            namedSequenced.project->scenarios.front().id,
            {namedDataLane->id});
    expect(
        repeatedNamedSequenced.ok()
            && wave::serializeProject(*namedSequenced.project)
                == wave::serializeProject(*repeatedNamedSequenced.project)
            && namedRequestLane != namedLanes.end()
            && namedDataLane != namedLanes.end()
            && !namedRequestLane->id.empty()
            && !namedDataLane->id.empty()
            && namedRequestSample.ok()
            && namedRequestSample.json
                    .value(QStringLiteral("samples"))
                    .toArray()
                    .at(0)
                    .toObject()
                    .value(QStringLiteral("value"))
                    .toString()
                == QStringLiteral("1")
            && namedDataSample.ok()
            && namedDataSample.json
                    .value(QStringLiteral("samples"))
                    .toArray()
                    .at(0)
                    .toObject()
                    .value(QStringLiteral("value"))
                    .toString()
                == QStringLiteral("0x34"),
        "name-addressed operations were not canonicalized deterministically");

    const QJsonObject namedEditBatch{
        {QStringLiteral("schema"),
         QString::fromLatin1(wave::AutomationBatchSchema)},
        {QStringLiteral("scenarioId"),
         QStringLiteral("REQUEST / ACKNOWLEDGE")},
        {QStringLiteral("operations"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("set-range")},
                 {QStringLiteral("assignments"),
                  QJsonArray{
                      QJsonObject{
                          {QStringLiteral("laneId"),
                           QStringLiteral("REQ")},
                          {QStringLiteral("value"),
                           QStringLiteral("1")},
                      },
                      QJsonObject{
                          {QStringLiteral("laneId"),
                           QStringLiteral("ack")},
                          {QStringLiteral("value"),
                           QStringLiteral("1")},
                      },
                  }},
                 {QStringLiteral("start"), QStringLiteral("50 ns")},
                 {QStringLiteral("end"), QStringLiteral("60 ns")},
                 {QStringLiteral("clockId"), QStringLiteral("CLK")},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("clear-range")},
                 {QStringLiteral("laneIds"),
                  QJsonArray{
                      QStringLiteral("req"),
                      QStringLiteral("ACK"),
                  }},
                 {QStringLiteral("start"), QStringLiteral("50 ns")},
                 {QStringLiteral("end"), QStringLiteral("60 ns")},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("add-signal")},
                 {QStringLiteral("id"),
                  QStringLiteral("lane-name-target")},
                 {QStringLiteral("name"),
                  QStringLiteral("target_by_name")},
                 {QStringLiteral("kind"), QStringLiteral("bus")},
                 {QStringLiteral("width"), 8},
                 {QStringLiteral("clockDomainId"),
                  QStringLiteral("clk")},
             },
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral("transfer-range")},
                 {QStringLiteral("mode"), QStringLiteral("copy")},
                 {QStringLiteral("mappings"),
                  QJsonArray{
                      QJsonObject{
                          {QStringLiteral("sourceLaneId"),
                           QStringLiteral("DATA[7:0]")},
                          {QStringLiteral("targetLaneId"),
                           QStringLiteral("TARGET_BY_NAME")},
                      },
                  }},
                 {QStringLiteral("sourceStart"),
                  QStringLiteral("80 ns")},
                 {QStringLiteral("sourceEnd"),
                  QStringLiteral("90 ns")},
                 {QStringLiteral("destination"),
                  QStringLiteral("10 ns")},
             },
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral("update-signal")},
                 {QStringLiteral("laneId"),
                  QStringLiteral("target_by_name")},
                 {QStringLiteral("name"),
                  QStringLiteral("target_named")},
                 {QStringLiteral("clockDomainId"),
                  QStringLiteral("CLK")},
                 {QStringLiteral("groupId"),
                  QStringLiteral("HANDSHAKE SIGNALS")},
                 {QStringLiteral("color"),
                  QStringLiteral("#123456")},
             },
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral("move-signal")},
                 {QStringLiteral("laneId"),
                  QStringLiteral("TARGET_NAMED")},
                 {QStringLiteral("afterLaneId"),
                  QStringLiteral("req")},
             },
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral("rename-lane")},
                 {QStringLiteral("laneId"),
                  QStringLiteral("target_named")},
                 {QStringLiteral("name"),
                  QStringLiteral("target_final")},
             },
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral("set-duration")},
                 {QStringLiteral("duration"),
                  QStringLiteral("230 ns")},
                 {QStringLiteral("clockId"),
                  QStringLiteral("clk")},
             },
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral("update-clock")},
                 {QStringLiteral("clockId"),
                  QStringLiteral("CLK")},
                 {QStringLiteral("name"),
                  QStringLiteral("clk_updated")},
             },
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral("delete-signal")},
                 {QStringLiteral("laneId"),
                  QStringLiteral("TARGET_FINAL")},
             },
         }},
    };
    const auto namedEdit =
        wave::applyAutomationBatch(source, namedEditBatch);
    expect(namedEdit.ok(), namedEdit.error.toStdString());
    const auto namedEditReports =
        namedEdit.json.value(QStringLiteral("operations")).toArray();
    expect(
        namedEditReports.size() == 10
            && namedEditReports.at(0)
                    .toObject()
                    .value(QStringLiteral("laneIds"))
                    .toArray()
                    .at(0)
                    .toString()
                == QStringLiteral("lane-request")
            && namedEditReports.at(3)
                    .toObject()
                    .value(QStringLiteral("mappings"))
                    .toArray()
                    .at(0)
                    .toObject()
                    .value(QStringLiteral("sourceLaneId"))
                    .toString()
                == QStringLiteral("lane-data")
            && namedEditReports.at(5)
                    .toObject()
                    .value(QStringLiteral("laneId"))
                    .toString()
                == QStringLiteral("lane-name-target")
            && namedEditReports.at(8)
                    .toObject()
                    .value(QStringLiteral("clockId"))
                    .toString()
                == QStringLiteral("clock-main")
            && namedEdit.project->scenarios.front().duration == 230'000
            && namedEdit.project->clockDomains.front().name
                == "clk_updated"
            && !wave::findLane(
                namedEdit.project->scenarios.front(),
                "lane-name-target"),
        "name selectors were not applied across editing operations");

    const QJsonObject symbolicBatch{
        {QStringLiteral("schema"),
         QString::fromLatin1(wave::AutomationBatchSchema)},
        {QStringLiteral("scenarioId"), QStringLiteral("STIMULUS")},
        {QStringLiteral("operations"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("add-group")},
                 {QStringLiteral("name"), QStringLiteral("Control")},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("add-signal")},
                 {QStringLiteral("name"), QStringLiteral("clk")},
                 {QStringLiteral("kind"), QStringLiteral("clock")},
                 {QStringLiteral("period"), QStringLiteral("10 ns")},
                 {QStringLiteral("groupId"), QStringLiteral("CONTROL")},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("add-signal")},
                 {QStringLiteral("name"), QStringLiteral("enable")},
                 {QStringLiteral("kind"), QStringLiteral("bit")},
                 {QStringLiteral("groupId"), QStringLiteral("control")},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("add-signal")},
                 {QStringLiteral("name"), QStringLiteral("state")},
                 {QStringLiteral("kind"), QStringLiteral("enum")},
                 {QStringLiteral("width"), 2},
                 {QStringLiteral("enumMap"),
                  QJsonObject{
                      {QStringLiteral("IDLE"), QStringLiteral("0")},
                      {QStringLiteral("BUSY"), QStringLiteral("1")},
                      {QStringLiteral("DONE"), QStringLiteral("2")},
                  }},
                 {QStringLiteral("clockDomainId"),
                  QStringLiteral("CLK")},
                 {QStringLiteral("groupId"),
                  QStringLiteral("Control")},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("set-sequence")},
                 {QStringLiteral("start"), QStringLiteral("cycle 0")},
                 {QStringLiteral("stepCycles"), 1},
                 {QStringLiteral("clockId"), QStringLiteral("clk")},
                 {QStringLiteral("sequences"),
                  QJsonArray{
                      QJsonObject{
                          {QStringLiteral("laneId"),
                           QStringLiteral("ENABLE")},
                          {QStringLiteral("values"),
                           QJsonArray{
                               QStringLiteral("0"),
                               QStringLiteral("1"),
                               QStringLiteral("1"),
                               QStringLiteral("0"),
                           }},
                      },
                      QJsonObject{
                          {QStringLiteral("laneId"),
                           QStringLiteral("STATE")},
                          {QStringLiteral("values"),
                           QJsonArray{
                               QStringLiteral("IDLE"),
                               QStringLiteral("BUSY"),
                               QStringLiteral("DONE"),
                               QStringLiteral("IDLE"),
                           }},
                      },
                  }},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("assert-value")},
                 {QStringLiteral("laneId"), QStringLiteral("state")},
                 {QStringLiteral("at"), QStringLiteral("cycle 2")},
                 {QStringLiteral("value"), QStringLiteral("DONE")},
             },
         }},
    };
    const auto symbolic = wave::applyAutomationBatch(
        *created.project, symbolicBatch);
    const auto repeatedSymbolic = wave::applyAutomationBatch(
        *created.project, symbolicBatch);
    expect(symbolic.ok(), symbolic.error.toStdString());
    const auto& symbolicScenario = symbolic.project->scenarios.front();
    const auto symbolicGroup = std::find_if(
        symbolicScenario.lanes.begin(),
        symbolicScenario.lanes.end(),
        [](const wave::Lane& lane) {
            return lane.kind == wave::LaneKind::Group
                && lane.name == "Control";
        });
    const auto symbolicState = std::find_if(
        symbolicScenario.lanes.begin(),
        symbolicScenario.lanes.end(),
        [](const wave::Lane& lane) {
            return lane.kind == wave::LaneKind::Enum
                && lane.name == "state";
        });
    const auto symbolicStateSample = wave::sampleProjectForAutomation(
        *symbolic.project,
        20'000,
        symbolicScenario.id,
        {"STATE"});
    const auto symbolicReports =
        symbolic.json.value(QStringLiteral("operations")).toArray();
    expect(
        repeatedSymbolic.ok()
            && wave::serializeProject(*symbolic.project)
                == wave::serializeProject(*repeatedSymbolic.project)
            && symbolicScenario.lanes.size() == 4
            && symbolicGroup != symbolicScenario.lanes.end()
            && symbolicState != symbolicScenario.lanes.end()
            && symbolicState->width == 2
            && symbolicState->enumMap.size() == 3
            && std::all_of(
                symbolicScenario.lanes.begin() + 1,
                symbolicScenario.lanes.end(),
                [&symbolicGroup](const wave::Lane& lane) {
                    return lane.groupId == symbolicGroup->id;
                })
            && symbolicReports.at(0)
                    .toObject()
                    .value(QStringLiteral("kind"))
                    .toString()
                == QStringLiteral("group")
            && symbolicReports.at(3)
                    .toObject()
                    .value(QStringLiteral("enumSymbolCount"))
                    .toInt()
                == 3
            && symbolicStateSample.ok()
            && symbolicStateSample.json
                    .value(QStringLiteral("samples"))
                    .toArray()
                    .at(0)
                    .toObject()
                    .value(QStringLiteral("value"))
                    .toString()
                == QStringLiteral("DONE"),
        "symbolic project creation is not deterministic or grouped");

    const QJsonObject symbolicUpdateBatch{
        {QStringLiteral("schema"),
         QString::fromLatin1(wave::AutomationBatchSchema)},
        {QStringLiteral("scenarioId"), QStringLiteral("Stimulus")},
        {QStringLiteral("operations"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral("update-signal")},
                 {QStringLiteral("laneId"), QStringLiteral("STATE")},
                 {QStringLiteral("enumMap"),
                  QJsonObject{
                      {QStringLiteral("IDLE"), QStringLiteral("0")},
                      {QStringLiteral("BUSY"), QStringLiteral("1")},
                      {QStringLiteral("DONE"), QStringLiteral("2")},
                      {QStringLiteral("ERROR"), QStringLiteral("3")},
                  }},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("set-range")},
                 {QStringLiteral("laneId"), QStringLiteral("state")},
                 {QStringLiteral("start"), QStringLiteral("cycle 3")},
                 {QStringLiteral("end"), QStringLiteral("cycle 4")},
                 {QStringLiteral("value"), QStringLiteral("ERROR")},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("assert-value")},
                 {QStringLiteral("laneId"), QStringLiteral("state")},
                 {QStringLiteral("at"), QStringLiteral("cycle 3")},
                 {QStringLiteral("value"), QStringLiteral("ERROR")},
             },
         }},
    };
    const auto symbolicUpdate = wave::applyAutomationBatch(
        *symbolic.project, symbolicUpdateBatch);
    expect(symbolicUpdate.ok(), symbolicUpdate.error.toStdString());
    const auto updatedStateSample = wave::sampleProjectForAutomation(
        *symbolicUpdate.project,
        30'000,
        symbolicUpdate.project->scenarios.front().id,
        {"state"});
    const auto symbolicUpdateReports =
        symbolicUpdate.json.value(QStringLiteral("operations")).toArray();
    expect(
        symbolicUpdateReports.at(0)
                    .toObject()
                    .value(QStringLiteral("enumSymbolCount"))
                    .toInt()
                == 4
            && updatedStateSample.ok()
            && updatedStateSample.json
                    .value(QStringLiteral("samples"))
                    .toArray()
                    .at(0)
                    .toObject()
                    .value(QStringLiteral("value"))
                    .toString()
                == QStringLiteral("ERROR"),
        "Enum map update or symbolic range write failed");

    const auto invalidEnumOverflow = wave::applyAutomationBatch(
        *symbolic.project,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"),
                      QStringLiteral("update-signal")},
                     {QStringLiteral("laneId"),
                      QStringLiteral("state")},
                     {QStringLiteral("enumMap"),
                      QJsonObject{
                          {QStringLiteral("IDLE"),
                           QStringLiteral("0")},
                          {QStringLiteral("BUSY"),
                           QStringLiteral("1")},
                          {QStringLiteral("DONE"),
                           QStringLiteral("2")},
                          {QStringLiteral("OVERFLOW"),
                           QStringLiteral("4")},
                      }},
                 },
             }},
        });
    const auto invalidEnumRemoval = wave::applyAutomationBatch(
        *symbolic.project,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"),
                      QStringLiteral("update-signal")},
                     {QStringLiteral("laneId"),
                      QStringLiteral("state")},
                     {QStringLiteral("enumMap"),
                      QJsonObject{
                          {QStringLiteral("IDLE"),
                           QStringLiteral("0")},
                          {QStringLiteral("DONE"),
                           QStringLiteral("2")},
                      }},
                 },
             }},
        });
    const auto invalidGroupTarget = wave::applyAutomationBatch(
        *symbolic.project,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"),
                      QStringLiteral("add-signal")},
                     {QStringLiteral("name"),
                      QStringLiteral("bad_member")},
                     {QStringLiteral("kind"),
                      QStringLiteral("bit")},
                     {QStringLiteral("groupId"),
                      QStringLiteral("state")},
                 },
             }},
        });
    expect(
        !invalidEnumOverflow.ok()
            && invalidEnumOverflow.failedOperation == 0
            && !invalidEnumOverflow.project
            && !invalidEnumRemoval.ok()
            && invalidEnumRemoval.failedOperation == 0
            && !invalidEnumRemoval.project
            && !invalidGroupTarget.ok()
            && invalidGroupTarget.failedOperation == 0
            && !invalidGroupTarget.project
            && symbolicState->enumMap.size() == 3
            && symbolicScenario.lanes.size() == 4,
        "invalid Enum or Group edits exposed a partial project");

    const QJsonObject intentBatch{
        {QStringLiteral("schema"),
         QString::fromLatin1(wave::AutomationBatchSchema)},
        {QStringLiteral("scenarioId"), QStringLiteral("Stimulus")},
        {QStringLiteral("operations"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral("add-relation")},
                 {QStringLiteral("id"),
                  QStringLiteral("relation-enable-state")},
                 {QStringLiteral("sourceLaneId"),
                  QStringLiteral("ENABLE")},
                 {QStringLiteral("sourceAt"),
                  QStringLiteral("cycle 0")},
                 {QStringLiteral("targetLaneId"),
                  QStringLiteral("state")},
                 {QStringLiteral("targetAt"),
                  QStringLiteral("cycle 2")},
                 {QStringLiteral("minimumDelayCycles"), 2},
                 {QStringLiteral("maximumDelayCycles"), 2},
                 {QStringLiteral("clockId"),
                  QStringLiteral("CLK")},
                 {QStringLiteral("severity"),
                  QStringLiteral("error")},
                 {QStringLiteral("description"),
                  QStringLiteral(
                      "state changes after enable")},
             },
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral("add-marker")},
                 {QStringLiteral("name"),
                  QStringLiteral("Active")},
                 {QStringLiteral("start"),
                  QStringLiteral("cycle 1")},
                 {QStringLiteral("end"),
                  QStringLiteral("cycle 3")},
                 {QStringLiteral("clockId"),
                  QStringLiteral("clk")},
                 {QStringLiteral("kind"),
                  QStringLiteral("phase")},
                 {QStringLiteral("note"),
                  QStringLiteral("Active interval")},
             },
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral("update-relation")},
                 {QStringLiteral("relationId"),
                  QStringLiteral("relation-enable-state")},
                 {QStringLiteral("sourceAt"),
                  QStringLiteral("cycle 1")},
                 {QStringLiteral("minimumDelayCycles"), 1},
                 {QStringLiteral("maximumDelayCycles"), 2},
                 {QStringLiteral("severity"),
                  QStringLiteral("warning")},
                 {QStringLiteral("description"),
                  QStringLiteral(
                      "state changes within two cycles")},
             },
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral("update-marker")},
                 {QStringLiteral("markerId"),
                  QStringLiteral("ACTIVE")},
                 {QStringLiteral("name"),
                  QStringLiteral("Control window")},
                 {QStringLiteral("end"),
                  QStringLiteral("cycle 4")},
                 {QStringLiteral("clockId"),
                  QStringLiteral("clk")},
                 {QStringLiteral("note"),
                  QStringLiteral("Complete interval")},
             },
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral("update-relation")},
                 {QStringLiteral("relationId"),
                  QStringLiteral("relation-enable-state")},
                 {QStringLiteral("maximumDelayCycles"), 2},
                 {QStringLiteral("severity"),
                  QStringLiteral("warning")},
                 {QStringLiteral("description"),
                  QStringLiteral(
                      "state changes within two cycles")},
             },
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral("update-marker")},
                 {QStringLiteral("markerId"),
                  QStringLiteral("Control window")},
                 {QStringLiteral("name"),
                  QStringLiteral("Control window")},
                 {QStringLiteral("end"),
                  QStringLiteral("cycle 4")},
                 {QStringLiteral("clockId"),
                  QStringLiteral("clk")},
                 {QStringLiteral("kind"),
                  QStringLiteral("phase")},
                 {QStringLiteral("note"),
                  QStringLiteral("Complete interval")},
             },
         }},
    };
    const auto intent = wave::applyAutomationBatch(
        *symbolic.project, intentBatch);
    const auto repeatedIntent = wave::applyAutomationBatch(
        *symbolic.project, intentBatch);
    expect(intent.ok(), intent.error.toStdString());
    const auto& intentScenario = intent.project->scenarios.front();
    const auto* intentRelation =
        wave::findRelation(intentScenario, "relation-enable-state");
    const auto intentMarker = std::find_if(
        intentScenario.markers.begin(),
        intentScenario.markers.end(),
        [](const wave::Marker& marker) {
            return marker.name == "Control window";
        });
    const auto* intentSource = intentRelation
        ? wave::findEvent(
              intentScenario, intentRelation->sourceEventId)
        : nullptr;
    const auto* intentTarget = intentRelation
        ? wave::findEvent(
              intentScenario, intentRelation->targetEventId)
        : nullptr;
    const auto intentReports =
        intent.json.value(QStringLiteral("operations")).toArray();
    const auto intentValidation =
        intent.json.value(QStringLiteral("validation")).toObject();
    expect(
        repeatedIntent.ok()
            && wave::serializeProject(*intent.project)
                == wave::serializeProject(*repeatedIntent.project)
            && intentScenario.relations.size() == 1
            && intentScenario.markers.size() == 1
            && intentRelation
            && intentRelation->minimumDelay == 10'000
            && intentRelation->maximumDelay == 20'000
            && intentRelation->severity
                == wave::Severity::Warning
            && intentRelation->clockDomainId
                == symbolic.project->clockDomains.front().id
            && intentSource
            && intentSource->laneId == symbolicScenario.lanes.at(2).id
            && intentSource->tick == 10'000
            && intentTarget
            && intentTarget->laneId == symbolicScenario.lanes.at(3).id
            && intentTarget->tick == 20'000
            && intentMarker != intentScenario.markers.end()
            && intentMarker->start == 10'000
            && intentMarker->end == 40'000
            && intentMarker->kind == wave::MarkerKind::Phase
            && intentReports.size() == 6
            && !intentReports.at(4)
                    .toObject()
                    .value(QStringLiteral("changed"))
                    .toBool()
            && !intentReports.at(5)
                    .toObject()
                    .value(QStringLiteral("changed"))
                    .toBool()
            && intentReports.at(3)
                    .toObject()
                    .value(QStringLiteral("markerId"))
                    .toString()
                == QString::fromStdString(intentMarker->id)
            && intentValidation
                    .value(QStringLiteral("summary"))
                    .toObject()
                    .value(QStringLiteral("errors"))
                    .toString()
                == QStringLiteral("0"),
        "Relation/Marker intent batch is not deterministic or coherent");

    const QJsonObject automaticRelationBatch{
        {QStringLiteral("schema"),
         QString::fromLatin1(wave::AutomationBatchSchema)},
        {QStringLiteral("operations"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral("add-relation")},
                 {QStringLiteral("sourceLaneId"),
                  QStringLiteral("enable")},
                 {QStringLiteral("sourceAt"),
                  QStringLiteral("cycle 1")},
                 {QStringLiteral("targetLaneId"),
                  QStringLiteral("state")},
                 {QStringLiteral("targetAt"),
                  QStringLiteral("cycle 2")},
                 {QStringLiteral("minimumDelayCycles"), 1},
                 {QStringLiteral("maximumDelayCycles"), 2},
                 {QStringLiteral("clockId"),
                  QStringLiteral("clk")},
             },
         }},
    };
    const auto automaticRelation = wave::applyAutomationBatch(
        *symbolic.project, automaticRelationBatch);
    const auto repeatedAutomaticRelation = wave::applyAutomationBatch(
        *symbolic.project, automaticRelationBatch);
    expect(
        automaticRelation.ok()
            && repeatedAutomaticRelation.ok()
            && wave::serializeProject(*automaticRelation.project)
                == wave::serializeProject(
                    *repeatedAutomaticRelation.project),
        "automatic Relation identity is not deterministic");
    const auto automaticRelationId =
        automaticRelation.json
            .value(QStringLiteral("operations"))
            .toArray()
            .at(0)
            .toObject()
            .value(QStringLiteral("relationId"))
            .toString();
    const auto duplicateAutomaticRelation = wave::applyAutomationBatch(
        *automaticRelation.project, automaticRelationBatch);
    expect(
        automaticRelationId.startsWith(
            QStringLiteral("relation-auto-"))
            && duplicateAutomaticRelation.ok()
            && !duplicateAutomaticRelation.changed
            && !duplicateAutomaticRelation.json
                    .value(QStringLiteral("operations"))
                    .toArray()
                    .at(0)
                    .toObject()
                    .value(QStringLiteral("changed"))
                    .toBool()
            && duplicateAutomaticRelation.json
                    .value(QStringLiteral("operations"))
                    .toArray()
                    .at(0)
                    .toObject()
                    .value(QStringLiteral("relationId"))
                    .toString()
                == automaticRelationId,
        "duplicate automatic Relation was not idempotent");

    const auto cleanupIntent = wave::applyAutomationBatch(
        *intent.project,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"),
                      QStringLiteral("delete-relation")},
                     {QStringLiteral("relationId"),
                      QStringLiteral("relation-enable-state")},
                 },
                 QJsonObject{
                     {QStringLiteral("op"),
                      QStringLiteral("delete-marker")},
                     {QStringLiteral("markerId"),
                      QStringLiteral("CONTROL WINDOW")},
                 },
             }},
        });
    expect(
        cleanupIntent.ok()
            && cleanupIntent.project->scenarios.front().relations.empty()
            && cleanupIntent.project->scenarios.front().markers.empty()
            && cleanupIntent.json
                    .value(QStringLiteral("operations"))
                    .toArray()
                    .at(1)
                    .toObject()
                    .value(QStringLiteral("markerId"))
                    .toString()
                == QString::fromStdString(intentMarker->id),
        "Relation/Marker cleanup did not use canonical identities");

    const auto invalidEdgeIntent = wave::applyAutomationBatch(
        *symbolic.project,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"),
                      QStringLiteral("add-marker")},
                     {QStringLiteral("name"),
                      QStringLiteral("Must roll back")},
                     {QStringLiteral("at"),
                      QStringLiteral("cycle 1")},
                     {QStringLiteral("clockId"),
                      QStringLiteral("clk")},
                 },
                 QJsonObject{
                     {QStringLiteral("op"),
                      QStringLiteral("add-relation")},
                     {QStringLiteral("sourceLaneId"),
                      QStringLiteral("enable")},
                     {QStringLiteral("sourceAt"),
                      QStringLiteral("cycle 2")},
                     {QStringLiteral("targetLaneId"),
                      QStringLiteral("state")},
                     {QStringLiteral("targetAt"),
                      QStringLiteral("cycle 3")},
                     {QStringLiteral("minimumDelayCycles"), 1},
                     {QStringLiteral("maximumDelayCycles"), 2},
                     {QStringLiteral("clockId"),
                      QStringLiteral("clk")},
                 },
             }},
        });
    const auto invalidDelayIntent = wave::applyAutomationBatch(
        *symbolic.project,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"),
                      QStringLiteral("add-relation")},
                     {QStringLiteral("sourceLaneId"),
                      QStringLiteral("enable")},
                     {QStringLiteral("sourceAt"),
                      QStringLiteral("cycle 1")},
                     {QStringLiteral("targetLaneId"),
                      QStringLiteral("state")},
                     {QStringLiteral("targetAt"),
                      QStringLiteral("cycle 2")},
                     {QStringLiteral("minimumDelayCycles"), 2},
                     {QStringLiteral("maximumDelayCycles"), 1},
                     {QStringLiteral("clockId"),
                      QStringLiteral("clk")},
                 },
             }},
        });
    const auto invalidMarkerIntent = wave::applyAutomationBatch(
        *symbolic.project,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"),
                      QStringLiteral("add-marker")},
                     {QStringLiteral("name"),
                      QStringLiteral("Bad point")},
                     {QStringLiteral("start"),
                      QStringLiteral("cycle 1")},
                     {QStringLiteral("end"),
                      QStringLiteral("cycle 2")},
                     {QStringLiteral("clockId"),
                      QStringLiteral("clk")},
                     {QStringLiteral("kind"),
                      QStringLiteral("point")},
                 },
             }},
        });
    expect(
        !invalidEdgeIntent.ok()
            && invalidEdgeIntent.failedOperation == 1
            && !invalidEdgeIntent.project
            && !invalidDelayIntent.ok()
            && invalidDelayIntent.failedOperation == 0
            && !invalidDelayIntent.project
            && !invalidMarkerIntent.ok()
            && invalidMarkerIntent.failedOperation == 0
            && !invalidMarkerIntent.project
            && symbolicScenario.relations.empty()
            && symbolicScenario.markers.empty(),
        "invalid Relation/Marker edits exposed a partial project");

    const auto invalidSequence = wave::applyAutomationBatch(
        *created.project,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"), QStringLiteral("add-signal")},
                     {QStringLiteral("id"), QStringLiteral("lane-bit")},
                     {QStringLiteral("name"), QStringLiteral("bit")},
                     {QStringLiteral("kind"), QStringLiteral("bit")},
                 },
                 QJsonObject{
                     {QStringLiteral("op"),
                      QStringLiteral("set-sequence")},
                     {QStringLiteral("laneId"),
                      QStringLiteral("lane-bit")},
                     {QStringLiteral("start"), QStringLiteral("0 ns")},
                     {QStringLiteral("step"), QStringLiteral("10 ns")},
                     {QStringLiteral("values"),
                      QJsonArray{
                          QStringLiteral("0"),
                          QStringLiteral("2"),
                          QStringLiteral("1"),
                      }},
                 },
             }},
        });
    expect(
        !invalidSequence.ok()
            && invalidSequence.failedOperation == 1
            && !invalidSequence.project
            && created.project->scenarios.front().lanes.empty(),
        "invalid sequence exposed a partially created signal");
    const auto ambiguousCycleSequence = wave::applyAutomationBatch(
        *created.project,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"), QStringLiteral("add-signal")},
                     {QStringLiteral("id"), QStringLiteral("lane-bit")},
                     {QStringLiteral("name"), QStringLiteral("bit")},
                     {QStringLiteral("kind"), QStringLiteral("bit")},
                 },
                 QJsonObject{
                     {QStringLiteral("op"),
                      QStringLiteral("set-sequence")},
                     {QStringLiteral("laneId"),
                      QStringLiteral("lane-bit")},
                     {QStringLiteral("start"), QStringLiteral("0 ns")},
                     {QStringLiteral("stepCycles"), 1},
                     {QStringLiteral("values"),
                      QJsonArray{
                          QStringLiteral("0"),
                          QStringLiteral("1"),
                      }},
                 },
             }},
        });
    expect(
        !ambiguousCycleSequence.ok()
            && ambiguousCycleSequence.failedOperation == 1
            && !ambiguousCycleSequence.project
            && created.project->scenarios.front().lanes.empty(),
        "cycle sequence without a Clock exposed a partial project");

    const auto inspection = wave::inspectProjectForAutomation(
        source, sourceScenario.id);
    expect(inspection.ok(), inspection.error.toStdString());
    expectEqual(
        inspection.json.value(QStringLiteral("schema")).toString(),
        QString::fromLatin1(wave::AutomationReportSchema),
        "automation inspect schema is incorrect");
    expectEqual(
        inspection.json.value(QStringLiteral("command")).toString(),
        QStringLiteral("inspect"),
        "automation inspect command is incorrect");
    const auto inspectedProject =
        inspection.json.value(QStringLiteral("project")).toObject();
    const auto inspectedScenarios =
        inspectedProject.value(QStringLiteral("scenarios")).toArray();
    expectEqual(
        inspectedScenarios.size(),
        qsizetype{1},
        "scenario-filtered inspect returned the wrong scenario count");
    expectEqual(
        inspectedScenarios.at(0)
            .toObject()
            .value(QStringLiteral("durationTick"))
            .toString(),
        QString::number(sourceScenario.duration),
        "inspect did not preserve an int64 tick as a string");
    const auto inspectedLanes =
        inspectedScenarios.at(0).toObject().value(QStringLiteral("lanes")).toArray();
    const auto inspectedExtension =
        inspectedLanes.at(0)
            .toObject()
            .value(QStringLiteral("extensions"))
            .toObject()
            .value(QStringLiteral("automationMeta"))
            .toObject();
    expect(
        inspectedExtension.value(QStringLiteral("enabled")).toBool()
            && inspectedExtension.value(QStringLiteral("rank")).toInt() == 2,
        "inspect exposed encoded extension JSON instead of its value");
    expect(
        !wave::inspectProjectForAutomation(source, std::string{"missing"}).ok(),
        "inspect accepted an unknown scenario ID");
    const auto summaryInspection = wave::inspectProjectForAutomation(
        source,
        sourceScenario.id,
        wave::AutomationInspectDetail::Summary);
    const auto summaryScenario =
        summaryInspection.json
            .value(QStringLiteral("project"))
            .toObject()
            .value(QStringLiteral("scenarios"))
            .toArray()
            .at(0)
            .toObject();
    expect(
        summaryInspection.ok()
            && summaryInspection.json.value(QStringLiteral("detail")).toString()
                == QStringLiteral("summary")
            && !summaryScenario.contains(QStringLiteral("events"))
            && !summaryScenario.contains(QStringLiteral("relations"))
            && !summaryScenario.value(QStringLiteral("lanes"))
                    .toArray()
                    .at(0)
                    .toObject()
                    .contains(QStringLiteral("segments")),
        "summary inspect retained full waveform payloads");

    wave::AutomationSignalQueryOptions signalQuery;
    signalQuery.match = "REQ";
    signalQuery.kind = wave::LaneKind::Bit;
    signalQuery.limit = 10;
    const auto foundSignals = wave::findSignalsForAutomation(
        source, signalQuery, std::string{"REQUEST / ACKNOWLEDGE"});
    const auto foundSignalArray =
        foundSignals.json.value(QStringLiteral("signals")).toArray();
    wave::AutomationSignalQueryOptions limitedQuery;
    limitedQuery.limit = 2;
    const auto limitedSignals = wave::findSignalsForAutomation(
        source, limitedQuery, sourceScenario.id);
    limitedQuery.limit = 0;
    const auto invalidSignalLimit = wave::findSignalsForAutomation(
        source, limitedQuery, sourceScenario.id);
    expect(
        foundSignals.ok()
            && foundSignals.json
                    .value(QStringLiteral("scenarioId"))
                    .toString()
                == QStringLiteral("scenario-handshake")
            && foundSignals.json
                    .value(QStringLiteral("matchCount"))
                    .toInt()
                == 1
            && foundSignalArray.size() == 1
            && foundSignalArray.at(0)
                    .toObject()
                    .value(QStringLiteral("id"))
                    .toString()
                == QStringLiteral("lane-request")
            && !foundSignalArray.at(0)
                    .toObject()
                    .contains(QStringLiteral("segments"))
            && limitedSignals.ok()
            && limitedSignals.json
                    .value(QStringLiteral("returnedCount"))
                    .toInt()
                == 2
            && limitedSignals.json
                    .value(QStringLiteral("matchCount"))
                    .toInt()
                == static_cast<int>(sourceScenario.lanes.size())
            && limitedSignals.json
                    .value(QStringLiteral("truncated"))
                    .toBool()
            && !invalidSignalLimit.ok(),
        "signal lookup did not filter or bound its compact result");
    auto idPrecedenceProject = source;
    auto* idPrecedenceAck =
        wave::findLane(idPrecedenceProject.scenarios.front(), "lane-ack");
    expect(
        idPrecedenceAck != nullptr,
        "stable-ID precedence fixture has no ack Lane");
    idPrecedenceAck->name = "lane-request";
    const auto idPrecedenceSample = wave::sampleProjectForAutomation(
        idPrecedenceProject,
        80'000,
        sourceScenario.id,
        {"lane-request"});
    const auto idPrecedenceValues =
        idPrecedenceSample.json.value(QStringLiteral("samples")).toArray();
    expect(
        idPrecedenceSample.ok() && idPrecedenceValues.size() == 1
            && idPrecedenceValues.at(0)
                    .toObject()
                    .value(QStringLiteral("laneId"))
                    .toString()
                == QStringLiteral("lane-request")
            && idPrecedenceValues.at(0)
                    .toObject()
                    .value(QStringLiteral("value"))
                    .toString()
                == QStringLiteral("1"),
        "an exact stable ID did not take precedence over a matching name");
    auto ambiguousNames = source;
    auto* ambiguousAck =
        wave::findLane(ambiguousNames.scenarios.front(), "lane-ack");
    expect(ambiguousAck != nullptr, "ambiguous-name fixture has no ack Lane");
    ambiguousAck->name = "req";
    signalQuery.exact = true;
    const auto ambiguousLookup = wave::findSignalsForAutomation(
        ambiguousNames, signalQuery, sourceScenario.id);
    const auto ambiguousSample = wave::sampleProjectForAutomation(
        ambiguousNames,
        80'000,
        sourceScenario.id,
        {"req"});
    const auto ambiguousApply = wave::applyAutomationBatch(
        ambiguousNames,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("scenarioId"),
             QStringLiteral("Request / acknowledge")},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"),
                      QStringLiteral("set-range")},
                     {QStringLiteral("laneId"),
                      QStringLiteral("req")},
                     {QStringLiteral("start"),
                      QStringLiteral("0 ns")},
                     {QStringLiteral("end"),
                      QStringLiteral("10 ns")},
                     {QStringLiteral("value"),
                      QStringLiteral("1")},
                 },
             }},
        });
    expect(
        ambiguousLookup.ok()
            && ambiguousLookup.json
                    .value(QStringLiteral("matchCount"))
                    .toInt()
                == 2
            && !ambiguousSample.ok()
            && ambiguousSample.error.contains(
                QStringLiteral("ambiguous"))
            && !ambiguousApply.ok()
            && ambiguousApply.failedOperation == 0
            && !ambiguousApply.project,
        "ambiguous names were guessed instead of requiring a stable ID");

    const auto physicalTime =
        wave::parseAutomationTime(source, QStringLiteral("37.5 ns"));
    const auto cycleTime =
        wave::parseAutomationTime(
            source, QStringLiteral("cycle 8"), std::string{"CLK"});
    expect(
        physicalTime.ok() && *physicalTime.tick == 37'500
            && cycleTime.ok() && *cycleTime.tick == 80'000
            && !wave::parseAutomationTime(
                    source, QStringLiteral("1.5 tick")).ok(),
        "automation time parsing is not exact or cycle-aware");
    const auto samples = wave::sampleProjectForAutomation(
        source,
        80'000,
        std::string{"request / acknowledge"},
        {"REQ", "DATA[7:0]"});
    const auto sampleValues =
        samples.json.value(QStringLiteral("samples")).toArray();
    expect(
        samples.ok() && sampleValues.size() == 2
            && sampleValues.at(0)
                    .toObject()
                    .value(QStringLiteral("value"))
                    .toString()
                == QStringLiteral("1")
            && sampleValues.at(1)
                    .toObject()
                    .value(QStringLiteral("value"))
                    .toString()
                == QStringLiteral("0x35")
            && !wave::sampleProjectForAutomation(
                    source,
                    80'000,
                    sourceScenario.id,
                    {"missing-lane"}).ok(),
        "automation sampling did not return exact filtered lane values");

    const auto window = wave::inspectProjectWindowForAutomation(
        source,
        70'000,
        130'000,
        std::string{"Request / acknowledge"},
        {"req", "DATA[7:0]"});
    const auto windowLanes =
        window.json.value(QStringLiteral("lanes")).toArray();
    const auto windowEvents =
        window.json.value(QStringLiteral("events")).toArray();
    const auto expectedWindowMarkers = std::count_if(
        sourceScenario.markers.begin(),
        sourceScenario.markers.end(),
        [](const wave::Marker& marker) {
            return (marker.start >= 70'000 && marker.start < 130'000)
                || (marker.start < 130'000 && marker.end > 70'000);
        });
    expect(window.ok(), window.error.toStdString());
    expectEqual(
        window.json.value(QStringLiteral("command")).toString(),
        QStringLiteral("window"),
        "automation window command is incorrect");
    expectEqual(
        window.json.value(QStringLiteral("laneCount")).toInt(),
        2,
        "automation window returned the wrong Lane count");
    expectEqual(
        window.json.value(QStringLiteral("windowEventCount")).toInt(),
        2,
        "automation window returned the wrong local Event count");
    expectEqual(
        windowEvents.size(),
        qsizetype{3},
        "automation window did not include Relation endpoint context");
    expectEqual(
        window.json.value(QStringLiteral("relationCount")).toInt(),
        1,
        "automation window returned the wrong Relation count");
    expectEqual(
        window.json.value(QStringLiteral("markerCount")).toInt(),
        static_cast<int>(expectedWindowMarkers),
        "automation window returned the wrong Marker count");
    expect(
        windowLanes.at(0)
                    .toObject()
                    .value(QStringLiteral("startValue"))
                    .toString()
                == QStringLiteral("0")
            && windowLanes.at(0)
                    .toObject()
                    .value(QStringLiteral("endValue"))
                    .toString()
                == QStringLiteral("1")
            && windowLanes.at(1)
                    .toObject()
                    .value(QStringLiteral("segments"))
                    .toArray()
                    .at(1)
                    .toObject()
                    .value(QStringLiteral("clipped"))
                    .toBool()
            && !wave::inspectProjectWindowForAutomation(
                    source,
                    130'000,
                    70'000,
                    sourceScenario.id,
                    {}).ok()
            && !wave::inspectProjectWindowForAutomation(
                    source,
                    70'000,
                    130'000,
                    sourceScenario.id,
                    {"lane-request", "lane-request"}).ok(),
        "automation window did not return compact local waveform and dependency context");

    wave::AutomationEdgeQueryOptions edgeQuery;
    edgeQuery.start = 70'000;
    edgeQuery.end = 130'000;
    const auto edges = wave::findWaveformEdgesForAutomation(
        source,
        edgeQuery,
        std::string{"Request / acknowledge"},
        {"req", "ack"});
    const auto edgeRows =
        edges.json.value(QStringLiteral("edges")).toArray();
    edgeQuery.edge = wave::AutomationEdgeKind::Rising;
    const auto risingEdges = wave::findWaveformEdgesForAutomation(
        source,
        edgeQuery,
        sourceScenario.id,
        {"lane-request", "lane-ack"});
    expect(
        edges.ok()
            && edges.json.value(QStringLiteral("command")).toString()
                == QStringLiteral("edges")
            && edges.json.value(QStringLiteral("laneCount")).toInt() == 2
            && edges.json.value(QStringLiteral("matchCount")).toInt() == 2
            && edges.json.value(
                   QStringLiteral("ambiguousEndpointCount")).toInt()
                == 0
            && edgeRows.size() == 2
            && edgeRows.at(0)
                    .toObject()
                    .value(QStringLiteral("laneId"))
                    .toString()
                == QStringLiteral("lane-request")
            && edgeRows.at(0)
                    .toObject()
                    .value(QStringLiteral("timeTick"))
                    .toString()
                == QStringLiteral("80000")
            && edgeRows.at(0)
                    .toObject()
                    .value(QStringLiteral("edge"))
                    .toString()
                == QStringLiteral("rising")
            && edgeRows.at(0)
                    .toObject()
                    .value(QStringLiteral("previousValue"))
                    .toString()
                == QStringLiteral("0")
            && edgeRows.at(0)
                    .toObject()
                    .value(QStringLiteral("value"))
                    .toString()
                == QStringLiteral("1")
            && edgeRows.at(0)
                    .toObject()
                    .value(QStringLiteral("relationEndpoint"))
                    .toBool()
            && edgeRows.at(0)
                    .toObject()
                    .value(QStringLiteral("eventIdCount"))
                    .toInt()
                == 1
            && !edgeRows.at(0)
                    .toObject()
                    .contains(QStringLiteral("eventId"))
            && !edgeRows.at(0)
                    .toObject()
                    .contains(QStringLiteral("segmentId"))
            && edgeRows.at(1)
                    .toObject()
                    .value(QStringLiteral("laneId"))
                    .toString()
                == QStringLiteral("lane-ack")
            && edgeRows.at(1)
                    .toObject()
                    .value(QStringLiteral("timeTick"))
                    .toString()
                == QStringLiteral("110000")
            && risingEdges.ok()
            && risingEdges.json.value(QStringLiteral("matchCount")).toInt()
                == 2,
        "edge lookup did not return compact relation-ready rising edges");

    wave::AutomationEdgeQueryOptions fallingQuery;
    fallingQuery.edge = wave::AutomationEdgeKind::Falling;
    fallingQuery.limit = 1;
    const auto fallingEdges = wave::findWaveformEdgesForAutomation(
        source,
        fallingQuery,
        sourceScenario.id,
        {"req", "ack"});
    const auto fallingRows =
        fallingEdges.json.value(QStringLiteral("edges")).toArray();
    wave::AutomationEdgeQueryOptions initialQuery;
    initialQuery.edge = wave::AutomationEdgeKind::Initial;
    const auto initialEdges = wave::findWaveformEdgesForAutomation(
        source,
        initialQuery,
        sourceScenario.id,
        {"req", "ack"});
    wave::AutomationEdgeQueryOptions invalidEdgeQuery;
    invalidEdgeQuery.limit = 0;
    expect(
        fallingEdges.ok()
            && fallingEdges.json.value(QStringLiteral("matchCount")).toInt()
                == 2
            && fallingEdges.json.value(
                   QStringLiteral("returnedCount")).toInt()
                == 1
            && fallingEdges.json.value(QStringLiteral("truncated")).toBool()
            && fallingRows.at(0)
                    .toObject()
                    .value(QStringLiteral("timeTick"))
                    .toString()
                == QStringLiteral("130000")
            && initialEdges.ok()
            && initialEdges.json.value(QStringLiteral("matchCount")).toInt()
                == 2
            && !wave::findWaveformEdgesForAutomation(
                    source,
                    invalidEdgeQuery,
                    sourceScenario.id,
                    {}).ok()
            && !wave::findWaveformEdgesForAutomation(
                    source,
                    {},
                    sourceScenario.id,
                    {"req", "lane-request"}).ok()
            && !wave::findWaveformEdgesForAutomation(
                    source,
                    {},
                    sourceScenario.id,
                    {"clk"}).ok()
            && !wave::findWaveformEdgesForAutomation(
                    source,
                    {},
                    sourceScenario.id,
                    {"Handshake signals"}).ok(),
        "edge lookup did not filter, bound, or reject invalid targets");

    auto ambiguousEdgesProject = source;
    const auto requestEdge = std::find_if(
        ambiguousEdgesProject.scenarios.front().events.begin(),
        ambiguousEdgesProject.scenarios.front().events.end(),
        [](const wave::Event& event) {
            return event.laneId == "lane-request"
                && event.tick == 80'000
                && event.waveformLinked;
        });
    expect(
        requestEdge
            != ambiguousEdgesProject.scenarios.front().events.end(),
        "ambiguous edge fixture has no request edge");
    auto duplicateEdge = *requestEdge;
    duplicateEdge.id = "event-req-high-duplicate";
    ambiguousEdgesProject.scenarios.front().events.push_back(
        std::move(duplicateEdge));
    const auto ambiguousEdges = wave::findWaveformEdgesForAutomation(
        ambiguousEdgesProject,
        edgeQuery,
        sourceScenario.id,
        {"req"});
    const auto ambiguousEdgeRows =
        ambiguousEdges.json.value(QStringLiteral("edges")).toArray();
    expect(
        ambiguousEdges.ok()
            && ambiguousEdges.json.value(
                   QStringLiteral("ambiguousEndpointCount")).toInt()
                == 1
            && ambiguousEdgeRows.size() == 1
            && ambiguousEdgeRows.at(0)
                    .toObject()
                    .value(QStringLiteral("candidateCount"))
                    .toInt()
                == 2
            && !ambiguousEdgeRows.at(0)
                    .toObject()
                    .value(QStringLiteral("relationEndpoint"))
                    .toBool(),
        "edge lookup did not identify an ambiguous Relation endpoint");

    wave::AutomationRelationQueryOptions relationQuery;
    relationQuery.match = "WITHIN";
    relationQuery.severity = wave::Severity::Error;
    relationQuery.start = 90'000;
    relationQuery.end = 120'000;
    const auto relations = wave::findRelationsForAutomation(
        source,
        relationQuery,
        std::string{"Request / acknowledge"},
        {"ack"});
    const auto relationRows =
        relations.json.value(QStringLiteral("relations")).toArray();
    const auto relationRow = relationRows.at(0).toObject();
    const auto sourceEndpoint =
        relationRow.value(QStringLiteral("source")).toObject();
    const auto targetEndpoint =
        relationRow.value(QStringLiteral("target")).toObject();
    const auto encodedRelationRow =
        QJsonDocument(relationRow).toJson(QJsonDocument::Compact);
    expect(
        relations.ok()
            && relations.json.value(QStringLiteral("command")).toString()
                == QStringLiteral("relations")
            && relations.json.value(
                   QStringLiteral("scenarioRelationCount")).toInt()
                == 1
            && relations.json.value(QStringLiteral("matchCount")).toInt()
                == 1
            && relations.json.value(QStringLiteral("readyCount")).toInt()
                == 1
            && relations.json.value(
                   QStringLiteral("endpointIssueCount")).toInt()
                == 0
            && relationRows.size() == 1
            && relationRow.value(QStringLiteral("relationId")).toString()
                == QStringLiteral("relation-req-ack")
            && relationRow.value(
                   QStringLiteral("observedDelayTick")).toString()
                == QStringLiteral("30000")
            && relationRow.value(
                   QStringLiteral("timingWithinRange")).toBool()
            && relationRow.value(
                   QStringLiteral("endpointsReady")).toBool()
            && sourceEndpoint.value(
                   QStringLiteral("laneId")).toString()
                == QStringLiteral("lane-request")
            && sourceEndpoint.value(
                   QStringLiteral("timeTick")).toString()
                == QStringLiteral("80000")
            && sourceEndpoint.value(
                   QStringLiteral("edge")).toString()
                == QStringLiteral("rising")
            && sourceEndpoint.value(
                   QStringLiteral("relationEndpoint")).toBool()
            && targetEndpoint.value(
                   QStringLiteral("laneId")).toString()
                == QStringLiteral("lane-ack")
            && targetEndpoint.value(
                   QStringLiteral("timeTick")).toString()
                == QStringLiteral("110000")
            && !encodedRelationRow.contains("sourceEventId")
            && !encodedRelationRow.contains("targetEventId")
            && !encodedRelationRow.contains("linkedSegmentId"),
        "Relation query did not return a compact endpoint-centered result");

    wave::AutomationRelationQueryOptions exactRelationQuery;
    exactRelationQuery.match = "RELATION-REQ-ACK";
    exactRelationQuery.exact = true;
    const auto exactRelations = wave::findRelationsForAutomation(
        source,
        exactRelationQuery,
        sourceScenario.id);
    auto multipleRelationsProject = source;
    auto secondRelation =
        multipleRelationsProject.scenarios.front().relations.front();
    secondRelation.id = "relation-req-ack-secondary";
    secondRelation.description = "secondary timing intent";
    multipleRelationsProject.scenarios.front().relations.push_back(
        std::move(secondRelation));
    wave::AutomationRelationQueryOptions limitedRelationQuery;
    limitedRelationQuery.limit = 1;
    const auto limitedRelations = wave::findRelationsForAutomation(
        multipleRelationsProject,
        limitedRelationQuery,
        sourceScenario.id);
    wave::AutomationRelationQueryOptions outsideRelationQuery;
    outsideRelationQuery.start = 151'000;
    outsideRelationQuery.end = 170'000;
    const auto outsideRelations = wave::findRelationsForAutomation(
        source,
        outsideRelationQuery,
        sourceScenario.id);
    wave::AutomationRelationQueryOptions invalidRelationQuery;
    invalidRelationQuery.limit = 0;
    wave::AutomationRelationQueryOptions invalidExactRelationQuery;
    invalidExactRelationQuery.exact = true;
    expect(
        exactRelations.ok()
            && exactRelations.json.value(
                   QStringLiteral("matchCount")).toInt()
                == 1
            && limitedRelations.ok()
            && limitedRelations.json.value(
                   QStringLiteral("matchCount")).toInt()
                == 2
            && limitedRelations.json.value(
                   QStringLiteral("returnedCount")).toInt()
                == 1
            && limitedRelations.json.value(
                   QStringLiteral("truncated")).toBool()
            && outsideRelations.ok()
            && outsideRelations.json.value(
                   QStringLiteral("matchCount")).toInt()
                == 0
            && !wave::findRelationsForAutomation(
                    source,
                    invalidRelationQuery,
                    sourceScenario.id).ok()
            && !wave::findRelationsForAutomation(
                    source,
                    invalidExactRelationQuery,
                    sourceScenario.id).ok()
            && !wave::findRelationsForAutomation(
                    source,
                    {},
                    sourceScenario.id,
                    {"req", "lane-request"}).ok(),
        "Relation query did not filter, bound, or reject invalid input");

    auto markerProject = source;
    markerProject.scenarios.front().markers = {
        {
            "marker-reset-release",
            "Reset released",
            40'000,
            40'000,
            wave::MarkerKind::Point,
            "Reset is no longer asserted",
            {},
        },
        {
            "marker-transfer",
            "Transfer",
            80'000,
            150'000,
            wave::MarkerKind::Phase,
            "Request/acknowledge window",
            {{"privateMarkerState", R"({"selected":true})"}},
        },
        {
            "marker-late-check",
            "Late check",
            110'000,
            120'000,
            wave::MarkerKind::Error,
            "Inspect acknowledgement",
            {},
        },
    };
    wave::AutomationMarkerQueryOptions markerQuery;
    markerQuery.match = "REQUEST";
    markerQuery.kind = wave::MarkerKind::Phase;
    markerQuery.start = 90'000;
    markerQuery.end = 120'000;
    const auto markers = wave::findMarkersForAutomation(
        markerProject,
        markerQuery,
        std::string{"Request / acknowledge"});
    const auto markerRows =
        markers.json.value(QStringLiteral("markers")).toArray();
    const auto markerRow = markerRows.at(0).toObject();
    const auto encodedMarkerRow =
        QJsonDocument(markerRow).toJson(QJsonDocument::Compact);
    expect(
        markers.ok()
            && markers.json.value(QStringLiteral("command")).toString()
                == QStringLiteral("markers")
            && markers.json.value(
                   QStringLiteral("scenarioMarkerCount")).toInt()
                == 3
            && markers.json.value(QStringLiteral("matchCount")).toInt()
                == 1
            && markers.json.value(QStringLiteral("validCount")).toInt()
                == 1
            && markers.json.value(
                   QStringLiteral("addressableCount")).toInt()
                == 1
            && markers.json.value(QStringLiteral("issueCount")).toInt()
                == 0
            && markerRows.size() == 1
            && markerRow.value(QStringLiteral("markerId")).toString()
                == QStringLiteral("marker-transfer")
            && markerRow.value(QStringLiteral("markerRef"))
                   .toString()
                   .startsWith(QStringLiteral("marker-ref-v1:"))
            && markerRow.value(QStringLiteral("name")).toString()
                == QStringLiteral("Transfer")
            && markerRow.value(QStringLiteral("kind")).toString()
                == QStringLiteral("phase")
            && markerRow.value(QStringLiteral("startTick")).toString()
                == QStringLiteral("80000")
            && markerRow.value(QStringLiteral("endTick")).toString()
                == QStringLiteral("150000")
            && markerRow.value(QStringLiteral("durationTick")).toString()
                == QStringLiteral("70000")
            && !markerRow.value(QStringLiteral("point")).toBool()
            && markerRow.value(QStringLiteral("addressable")).toBool()
            && markerRow.value(QStringLiteral("valid")).toBool()
            && markerRow.value(QStringLiteral("issues")).toArray().isEmpty()
            && !encodedMarkerRow.contains("extensions")
            && !encodedMarkerRow.contains("privateMarkerState"),
        "Marker query did not return a compact, editable interval");

    wave::AutomationMarkerQueryOptions exactMarkerQuery;
    exactMarkerQuery.match = "MARKER-TRANSFER";
    exactMarkerQuery.exact = true;
    const auto exactMarkers = wave::findMarkersForAutomation(
        markerProject,
        exactMarkerQuery,
        sourceScenario.id);
    wave::AutomationMarkerQueryOptions limitedMarkerQuery;
    limitedMarkerQuery.limit = 1;
    const auto limitedMarkers = wave::findMarkersForAutomation(
        markerProject,
        limitedMarkerQuery,
        sourceScenario.id);
    wave::AutomationMarkerQueryOptions outsideMarkerQuery;
    outsideMarkerQuery.start = 151'000;
    outsideMarkerQuery.end = 170'000;
    const auto outsideMarkers = wave::findMarkersForAutomation(
        markerProject,
        outsideMarkerQuery,
        sourceScenario.id);
    wave::AutomationMarkerQueryOptions invalidMarkerQuery;
    invalidMarkerQuery.limit = 0;
    wave::AutomationMarkerQueryOptions invalidExactMarkerQuery;
    invalidExactMarkerQuery.exact = true;
    expect(
        exactMarkers.ok()
            && exactMarkers.json.value(
                   QStringLiteral("matchCount")).toInt()
                == 1
            && limitedMarkers.ok()
            && limitedMarkers.json.value(
                   QStringLiteral("matchCount")).toInt()
                == 3
            && limitedMarkers.json.value(
                   QStringLiteral("returnedCount")).toInt()
                == 1
            && limitedMarkers.json.value(
                   QStringLiteral("truncated")).toBool()
            && outsideMarkers.ok()
            && outsideMarkers.json.value(
                   QStringLiteral("matchCount")).toInt()
                == 0
            && !wave::findMarkersForAutomation(
                    markerProject,
                    invalidMarkerQuery,
                    sourceScenario.id).ok()
            && !wave::findMarkersForAutomation(
                    markerProject,
                    invalidExactMarkerQuery,
                    sourceScenario.id).ok(),
        "Marker query did not filter, bound, or reject invalid input");

    auto brokenMarkerProject = markerProject;
    auto duplicateMarker =
        brokenMarkerProject.scenarios.front().markers.at(1);
    duplicateMarker.name = "Transfer copy";
    duplicateMarker.start = 160'000;
    duplicateMarker.end = 170'000;
    brokenMarkerProject.scenarios.front().markers.push_back(
        std::move(duplicateMarker));
    brokenMarkerProject.scenarios.front().markers.push_back({
        "marker-duplicate-name",
        "transfer",
        170'000,
        180'000,
        wave::MarkerKind::Note,
        "Duplicate display name",
        {},
    });
    brokenMarkerProject.scenarios.front().markers.push_back({
        "marker-broken",
        "Broken point",
        -1,
        230'000,
        wave::MarkerKind::Point,
        "Invalid imported marker",
        {},
    });
    wave::AutomationMarkerQueryOptions duplicateMarkerQuery;
    duplicateMarkerQuery.match = "marker-transfer";
    duplicateMarkerQuery.exact = true;
    const auto duplicateMarkers = wave::findMarkersForAutomation(
        brokenMarkerProject,
        duplicateMarkerQuery,
        sourceScenario.id);
    const auto duplicateMarkerRows =
        duplicateMarkers.json.value(QStringLiteral("markers")).toArray();
    wave::AutomationMarkerQueryOptions brokenMarkerQuery;
    brokenMarkerQuery.match = "marker-broken";
    brokenMarkerQuery.exact = true;
    const auto brokenMarkers = wave::findMarkersForAutomation(
        brokenMarkerProject,
        brokenMarkerQuery,
        sourceScenario.id);
    const auto brokenMarkerRow =
        brokenMarkers.json
            .value(QStringLiteral("markers"))
            .toArray()
            .at(0)
            .toObject();
    const auto brokenMarkerIssues =
        brokenMarkerRow.value(QStringLiteral("issues")).toArray();
    const auto firstDuplicateMarkerRef =
        duplicateMarkerRows.at(0)
            .toObject()
            .value(QStringLiteral("markerRef"))
            .toString();
    const auto secondDuplicateMarkerRef =
        duplicateMarkerRows.at(1)
            .toObject()
            .value(QStringLiteral("markerRef"))
            .toString();
    expect(
        duplicateMarkers.ok()
            && duplicateMarkerRows.size() == 2
            && duplicateMarkers.json.value(
                   QStringLiteral("addressableCount")).toInt()
                == 0
            && duplicateMarkerRows.at(0)
                    .toObject()
                    .value(QStringLiteral("idCount"))
                    .toInt()
                == 2
            && !duplicateMarkerRows.at(0)
                    .toObject()
                    .value(QStringLiteral("addressable"))
                    .toBool()
            && !firstDuplicateMarkerRef.isEmpty()
            && !secondDuplicateMarkerRef.isEmpty()
            && firstDuplicateMarkerRef
                != secondDuplicateMarkerRef
            && brokenMarkers.ok()
            && brokenMarkers.json.value(
                   QStringLiteral("validCount")).toInt()
                == 0
            && brokenMarkers.json.value(
                   QStringLiteral("issueCount")).toInt()
                == 3
            && brokenMarkerRow.value(
                   QStringLiteral("durationTick")).isNull()
            && brokenMarkerIssues.contains(
                QStringLiteral("start-before-scenario"))
            && brokenMarkerIssues.contains(
                QStringLiteral("end-after-scenario"))
            && brokenMarkerIssues.contains(
                QStringLiteral("point-has-range")),
        "Marker query did not diagnose unsafe identities or geometry");

    const auto markerOperationBatch =
        [&sourceScenario](QJsonObject operation) {
            return QJsonObject{
                {QStringLiteral("schema"),
                 QString::fromLatin1(wave::AutomationBatchSchema)},
                {QStringLiteral("scenarioId"),
                 QString::fromStdString(sourceScenario.id)},
                {QStringLiteral("operations"),
                 QJsonArray{std::move(operation)}},
            };
        };
    const auto duplicateIdUpdate =
        wave::applyAutomationBatch(
            brokenMarkerProject,
            markerOperationBatch(
                QJsonObject{
                    {QStringLiteral("op"),
                     QStringLiteral("update-marker")},
                    {QStringLiteral("markerId"),
                     QStringLiteral("marker-transfer")},
                    {QStringLiteral("note"),
                     QStringLiteral("Must not select the first duplicate")},
                }));
    const auto duplicateIdDelete =
        wave::applyAutomationBatch(
            brokenMarkerProject,
            markerOperationBatch(
                QJsonObject{
                    {QStringLiteral("op"),
                     QStringLiteral("delete-marker")},
                    {QStringLiteral("markerId"),
                     QStringLiteral("marker-transfer")},
                }));
    const auto duplicateIdByUniqueName =
        wave::applyAutomationBatch(
            brokenMarkerProject,
            markerOperationBatch(
                QJsonObject{
                    {QStringLiteral("op"),
                     QStringLiteral("update-marker")},
                    {QStringLiteral("markerId"),
                     QStringLiteral("Transfer copy")},
                    {QStringLiteral("note"),
                     QStringLiteral("Name must not bypass duplicate ID")},
                }));
    expect(
        !duplicateIdUpdate.ok()
            && duplicateIdUpdate.failedOperation == 0
            && !duplicateIdUpdate.project
            && duplicateIdUpdate.error.contains(
                QStringLiteral("stable ID"))
            && duplicateIdUpdate.error.contains(
                QStringLiteral("ambiguous"))
            && !duplicateIdDelete.ok()
            && duplicateIdDelete.failedOperation == 0
            && !duplicateIdDelete.project
            && !duplicateIdByUniqueName.ok()
            && duplicateIdByUniqueName.failedOperation == 0
            && !duplicateIdByUniqueName.project
            && duplicateIdByUniqueName.error.contains(
                QStringLiteral("resolves to ambiguous stable ID")),
        "Marker operations silently selected the first duplicate stable ID");

    const auto duplicateMarkerRepairBatch =
        markerOperationBatch(
            QJsonObject{
                {QStringLiteral("op"),
                 QStringLiteral("update-marker")},
                {QStringLiteral("markerRef"),
                 secondDuplicateMarkerRef},
                {QStringLiteral("newId"),
                 QStringLiteral("marker-transfer-copy")},
                {QStringLiteral("note"),
                 QStringLiteral("Recovered duplicate identity")},
            });
    const auto duplicateMarkerRepair =
        wave::applyAutomationBatch(
            brokenMarkerProject,
            duplicateMarkerRepairBatch);
    const auto repeatedDuplicateMarkerRepair =
        wave::applyAutomationBatch(
            brokenMarkerProject,
            duplicateMarkerRepairBatch);
    expect(
        duplicateMarkerRepair.ok(),
        duplicateMarkerRepair.error.toStdString());
    expect(
        repeatedDuplicateMarkerRepair.ok(),
        repeatedDuplicateMarkerRepair.error.toStdString());
    const auto& duplicateRepairScenario =
        duplicateMarkerRepair.project->scenarios.front();
    const auto repairedDuplicateMarker = std::find_if(
        duplicateRepairScenario.markers.begin(),
        duplicateRepairScenario.markers.end(),
        [](const wave::Marker& marker) {
            return marker.id == "marker-transfer-copy";
        });
    const auto duplicateRepairReport =
        duplicateMarkerRepair.json
            .value(QStringLiteral("operations"))
            .toArray()
            .at(0)
            .toObject();
    expect(
        repairedDuplicateMarker
            != duplicateRepairScenario.markers.end()
            && repairedDuplicateMarker->name
                == "Transfer copy"
            && repairedDuplicateMarker->start == 160'000
            && repairedDuplicateMarker->end == 170'000
            && repairedDuplicateMarker->note
                == "Recovered duplicate identity"
            && repairedDuplicateMarker->extensions.contains(
                "privateMarkerState")
            && duplicateRepairReport.value(
                   QStringLiteral("selectedByRepairRef")).toBool()
            && duplicateRepairReport.value(
                   QStringLiteral("repairedIdentity")).toBool()
            && !duplicateRepairReport.value(
                    QStringLiteral("repairedGeometry")).toBool()
            && duplicateRepairReport.value(
                   QStringLiteral("previousMarkerId")).toString()
                == QStringLiteral("marker-transfer")
            && duplicateRepairReport.value(
                   QStringLiteral("markerId")).toString()
                == QStringLiteral("marker-transfer-copy")
            && duplicateRepairReport.value(
                   QStringLiteral("markerRef")).toString()
                != secondDuplicateMarkerRef
            && wave::serializeProject(
                   *duplicateMarkerRepair.project)
                == wave::serializeProject(
                    *repeatedDuplicateMarkerRepair.project),
        "markerRef did not deterministically repair one duplicate identity");

    const auto staleMarkerRepair =
        wave::applyAutomationBatch(
            *duplicateMarkerRepair.project,
            duplicateMarkerRepairBatch);
    const auto collidingMarkerRepair =
        wave::applyAutomationBatch(
            brokenMarkerProject,
            markerOperationBatch(
                QJsonObject{
                    {QStringLiteral("op"),
                     QStringLiteral("update-marker")},
                    {QStringLiteral("markerRef"),
                     secondDuplicateMarkerRef},
                    {QStringLiteral("newId"),
                     QStringLiteral("marker-broken")},
                }));
    expect(
        !staleMarkerRepair.ok()
            && staleMarkerRepair.failedOperation == 0
            && !staleMarkerRepair.project
            && staleMarkerRepair.error.contains(
                QStringLiteral("stale"))
            && !collidingMarkerRepair.ok()
            && collidingMarkerRepair.failedOperation == 0
            && !collidingMarkerRepair.project
            && collidingMarkerRepair.error.contains(
                QStringLiteral("already exists")),
        "markerRef accepted stale state or a colliding replacement ID");

    const auto deleteDuplicateByRef =
        wave::applyAutomationBatch(
            brokenMarkerProject,
            markerOperationBatch(
                QJsonObject{
                    {QStringLiteral("op"),
                     QStringLiteral("delete-marker")},
                    {QStringLiteral("markerRef"),
                     secondDuplicateMarkerRef},
                }));
    expect(
        deleteDuplicateByRef.ok(),
        deleteDuplicateByRef.error.toStdString());
    const auto remainingTransferIds = std::count_if(
        deleteDuplicateByRef.project
            ->scenarios.front().markers.begin(),
        deleteDuplicateByRef.project
            ->scenarios.front().markers.end(),
        [](const wave::Marker& marker) {
            return marker.id == "marker-transfer";
        });
    expect(
        remainingTransferIds == 1
            && deleteDuplicateByRef.json
                   .value(QStringLiteral("operations"))
                   .toArray()
                   .at(0)
                   .toObject()
                   .value(QStringLiteral("selectedByRepairRef"))
                   .toBool(),
        "delete-marker did not remove exactly one referenced duplicate");

    const auto brokenGeometryRef =
        brokenMarkerRow.value(
            QStringLiteral("markerRef")).toString();
    const auto repairedMarkerGeometry =
        wave::applyAutomationBatch(
            brokenMarkerProject,
            markerOperationBatch(
                QJsonObject{
                    {QStringLiteral("op"),
                     QStringLiteral("update-marker")},
                    {QStringLiteral("markerRef"),
                     brokenGeometryRef},
                    {QStringLiteral("at"),
                     QStringLiteral("120 ns")},
                    {QStringLiteral("name"),
                     QStringLiteral("Recovered point")},
                }));
    expect(
        repairedMarkerGeometry.ok(),
        repairedMarkerGeometry.error.toStdString());
    const auto repairedGeometryReport =
        repairedMarkerGeometry.json
            .value(QStringLiteral("operations"))
            .toArray()
            .at(0)
            .toObject();
    const auto repairedGeometryMarker = std::find_if(
        repairedMarkerGeometry.project
            ->scenarios.front().markers.begin(),
        repairedMarkerGeometry.project
            ->scenarios.front().markers.end(),
        [](const wave::Marker& marker) {
            return marker.id == "marker-broken";
        });
    expect(
        repairedGeometryMarker
            != repairedMarkerGeometry.project
                   ->scenarios.front().markers.end()
            && repairedGeometryMarker->start == 120'000
            && repairedGeometryMarker->end == 120'000
            && repairedGeometryMarker->kind
                == wave::MarkerKind::Point
            && repairedGeometryReport.value(
                   QStringLiteral("repairedGeometry")).toBool()
            && !repairedGeometryReport.value(
                    QStringLiteral("repairedIdentity")).toBool(),
        "markerRef did not repair invalid point geometry");

    const auto partialGeometryRepair =
        wave::applyAutomationBatch(
            brokenMarkerProject,
            markerOperationBatch(
                QJsonObject{
                    {QStringLiteral("op"),
                     QStringLiteral("update-marker")},
                    {QStringLiteral("markerRef"),
                     brokenGeometryRef},
                    {QStringLiteral("startTick"),
                     QStringLiteral("0")},
                }));
    expect(
        !partialGeometryRepair.ok()
            && partialGeometryRepair.failedOperation == 0
            && !partialGeometryRepair.project,
        "update-marker accepted a still-invalid partial geometry repair");

    auto emptyMarkerIdProject = markerProject;
    emptyMarkerIdProject.scenarios.front().markers.push_back({
        "",
        "Needs ID",
        180'000,
        180'000,
        wave::MarkerKind::Note,
        "Imported without identity",
        {},
    });
    wave::AutomationMarkerQueryOptions emptyMarkerIdQuery;
    emptyMarkerIdQuery.match = "Needs ID";
    emptyMarkerIdQuery.exact = true;
    const auto emptyMarkerIdResults =
        wave::findMarkersForAutomation(
            emptyMarkerIdProject,
            emptyMarkerIdQuery,
            sourceScenario.id);
    const auto emptyMarkerIdRow =
        emptyMarkerIdResults.json
            .value(QStringLiteral("markers"))
            .toArray()
            .at(0)
            .toObject();
    const auto emptyMarkerIdRepair =
        wave::applyAutomationBatch(
            emptyMarkerIdProject,
            markerOperationBatch(
                QJsonObject{
                    {QStringLiteral("op"),
                     QStringLiteral("update-marker")},
                    {QStringLiteral("markerRef"),
                     emptyMarkerIdRow.value(
                         QStringLiteral("markerRef"))},
                    {QStringLiteral("newId"),
                     QStringLiteral("marker-needs-id")},
                }));
    expect(
        emptyMarkerIdResults.ok()
            && !emptyMarkerIdRow.value(
                    QStringLiteral("addressable")).toBool()
            && emptyMarkerIdRow.value(
                   QStringLiteral("issues"))
                   .toArray()
                   .contains(QStringLiteral("missing-id"))
            && emptyMarkerIdRepair.ok()
            && emptyMarkerIdRepair.json
                   .value(QStringLiteral("operations"))
                   .toArray()
                   .at(0)
                   .toObject()
                   .value(QStringLiteral("repairedIdentity"))
                   .toBool(),
        "markerRef did not recover a Marker with an empty stable ID");

    auto brokenRelationProject = source;
    brokenRelationProject.scenarios.front()
        .relations.front()
        .targetEventId = "missing-target-event";
    const auto brokenRelations = wave::findRelationsForAutomation(
        brokenRelationProject,
        {},
        sourceScenario.id);
    const auto brokenRelationRow =
        brokenRelations.json
            .value(QStringLiteral("relations"))
            .toArray()
            .at(0)
            .toObject();
    const auto brokenTarget =
        brokenRelationRow.value(QStringLiteral("target")).toObject();
    auto lateRelationProject = source;
    lateRelationProject.scenarios.front()
        .relations.front()
        .maximumDelay = 20'000;
    const auto lateRelations = wave::findRelationsForAutomation(
        lateRelationProject,
        {},
        sourceScenario.id);
    auto duplicateEventIdProject = source;
    const auto relationSourceEvent = wave::findEvent(
        duplicateEventIdProject.scenarios.front(),
        duplicateEventIdProject.scenarios.front()
            .relations.front()
            .sourceEventId);
    expect(
        relationSourceEvent != nullptr,
        "duplicate Relation endpoint fixture has no source Event");
    const auto duplicateRelationSourceEvent = *relationSourceEvent;
    duplicateEventIdProject.scenarios.front().events.push_back(
        duplicateRelationSourceEvent);
    const auto ambiguousEndpointRelations =
        wave::findRelationsForAutomation(
            duplicateEventIdProject,
            {},
            sourceScenario.id);
    const auto ambiguousRelationSource =
        ambiguousEndpointRelations.json
            .value(QStringLiteral("relations"))
            .toArray()
            .at(0)
            .toObject()
            .value(QStringLiteral("source"))
            .toObject();
    auto detachedEndpointProject = source;
    auto* detachedSourceEvent = wave::findEvent(
        detachedEndpointProject.scenarios.front(),
        detachedEndpointProject.scenarios.front()
            .relations.front()
            .sourceEventId);
    expect(
        detachedSourceEvent != nullptr,
        "detached Relation endpoint fixture has no source Event");
    detachedSourceEvent->waveformLinked = false;
    const auto detachedEndpointRelations =
        wave::findRelationsForAutomation(
            detachedEndpointProject,
            {},
            sourceScenario.id);
    const auto detachedRelationSource =
        detachedEndpointRelations.json
            .value(QStringLiteral("relations"))
            .toArray()
            .at(0)
            .toObject()
            .value(QStringLiteral("source"))
            .toObject();
    auto missingEventIdProject = source;
    auto& missingEventIdRelation =
        missingEventIdProject.scenarios.front().relations.front();
    auto* missingIdSourceEvent = wave::findEvent(
        missingEventIdProject.scenarios.front(),
        missingEventIdRelation.sourceEventId);
    expect(
        missingIdSourceEvent != nullptr,
        "missing Event ID fixture has no source Event");
    missingIdSourceEvent->id.clear();
    missingEventIdRelation.sourceEventId.clear();
    const auto missingEventIdRelations =
        wave::findRelationsForAutomation(
            missingEventIdProject,
            {},
            sourceScenario.id);
    const auto missingEventIdSource =
        missingEventIdRelations.json
            .value(QStringLiteral("relations"))
            .toArray()
            .at(0)
            .toObject()
            .value(QStringLiteral("source"))
            .toObject();
    expect(
        brokenRelations.ok()
            && brokenRelations.json.value(
                   QStringLiteral("readyCount")).toInt()
                == 0
            && brokenRelations.json.value(
                   QStringLiteral("endpointIssueCount")).toInt()
                == 1
            && !brokenRelationRow.value(
                    QStringLiteral("endpointsReady")).toBool()
            && !brokenTarget.value(
                    QStringLiteral("resolved")).toBool()
            && brokenTarget.value(
                   QStringLiteral("issue")).toString()
                == QStringLiteral("missing-event")
            && !lateRelations.json
                    .value(QStringLiteral("relations"))
                    .toArray()
                    .at(0)
                    .toObject()
                    .value(QStringLiteral("timingWithinRange"))
                    .toBool()
            && ambiguousEndpointRelations.ok()
            && ambiguousRelationSource.value(
                   QStringLiteral("eventIdCount")).toInt()
                == 2
            && ambiguousRelationSource.value(
                   QStringLiteral("issue")).toString()
                == QStringLiteral("ambiguous-event-id")
            && detachedEndpointRelations.ok()
            && detachedRelationSource.value(
                   QStringLiteral("issue")).toString()
                == QStringLiteral("detached-event")
            && !detachedRelationSource.value(
                    QStringLiteral("relationEndpoint")).toBool()
            && missingEventIdRelations.ok()
            && missingEventIdSource.value(
                   QStringLiteral("issue")).toString()
                == QStringLiteral("missing-event-id")
            && !missingEventIdSource.value(
                    QStringLiteral("relationEndpoint")).toBool(),
        "Relation query did not surface broken endpoints or timing");

    const auto relationUpdateBatch =
        [&sourceScenario](QJsonObject operation) {
            return QJsonObject{
                {QStringLiteral("schema"),
                 QString::fromLatin1(wave::AutomationBatchSchema)},
                {QStringLiteral("scenarioId"),
                 QString::fromStdString(sourceScenario.id)},
                {QStringLiteral("operations"),
                 QJsonArray{std::move(operation)}},
            };
        };
    const auto resetRepairEdge = std::find_if(
        sourceScenario.events.begin(),
        sourceScenario.events.end(),
        [](const wave::Event& event) {
            return event.waveformLinked
                && event.laneId == "lane-reset"
                && event.tick == 40'000;
        });
    const auto acknowledgementRepairEdge = std::find_if(
        sourceScenario.events.begin(),
        sourceScenario.events.end(),
        [](const wave::Event& event) {
            return event.waveformLinked
                && event.laneId == "lane-ack"
                && event.tick == 110'000;
        });
    const auto dataRepairEdge = std::find_if(
        sourceScenario.events.begin(),
        sourceScenario.events.end(),
        [](const wave::Event& event) {
            return event.waveformLinked
                && event.laneId == "lane-data"
                && event.tick == 80'000;
        });
    expect(
        resetRepairEdge != sourceScenario.events.end()
            && acknowledgementRepairEdge
                != sourceScenario.events.end()
            && dataRepairEdge != sourceScenario.events.end(),
        "Relation repair fixture has no addressable replacement edges");
    const auto targetRepairBatch = relationUpdateBatch(
        QJsonObject{
            {QStringLiteral("op"),
             QStringLiteral("update-relation")},
            {QStringLiteral("relationId"),
             QStringLiteral("relation-req-ack")},
            {QStringLiteral("targetLaneId"),
             QStringLiteral("ack")},
            {QStringLiteral("targetAt"),
             QStringLiteral("110 ns")},
            {QStringLiteral("description"),
             QStringLiteral("Repaired request acknowledgement")},
        });
    const auto repairedMissingTarget =
        wave::applyAutomationBatch(
            brokenRelationProject, targetRepairBatch);
    const auto repeatedMissingTargetRepair =
        wave::applyAutomationBatch(
            brokenRelationProject, targetRepairBatch);
    expect(
        repairedMissingTarget.ok(),
        repairedMissingTarget.error.toStdString());
    expect(
        repeatedMissingTargetRepair.ok(),
        repeatedMissingTargetRepair.error.toStdString());
    const auto& repairedMissingScenario =
        repairedMissingTarget.project->scenarios.front();
    const auto* repairedMissingRelation = wave::findRelation(
        repairedMissingScenario, "relation-req-ack");
    const auto repairedMissingReport =
        repairedMissingTarget.json
            .value(QStringLiteral("operations"))
            .toArray()
            .at(0)
            .toObject();
    const auto repairedMissingQuery =
        wave::findRelationsForAutomation(
            *repairedMissingTarget.project,
            {},
            sourceScenario.id);
    expect(
        repairedMissingRelation
            && repairedMissingRelation->id == "relation-req-ack"
            && repairedMissingRelation->sourceEventId
                == sourceScenario.relations.front().sourceEventId
            && repairedMissingRelation->targetEventId
                == acknowledgementRepairEdge->id
            && repairedMissingRelation->description
                == "Repaired request acknowledgement"
            && repairedMissingReport.value(
                   QStringLiteral("changed")).toBool()
            && !repairedMissingReport.value(
                    QStringLiteral("repairedSourceEndpoint")).toBool()
            && repairedMissingReport.value(
                   QStringLiteral("repairedTargetEndpoint")).toBool()
            && repairedMissingReport.value(
                   QStringLiteral("targetLaneId")).toString()
                == QStringLiteral("lane-ack")
            && repairedMissingReport.value(
                   QStringLiteral("targetAtTick")).toString()
                == QStringLiteral("110000")
            && repairedMissingQuery.ok()
            && repairedMissingQuery.json.value(
                   QStringLiteral("readyCount")).toInt()
                == 1
            && repairedMissingQuery.json.value(
                   QStringLiteral("endpointIssueCount")).toInt()
                == 0
            && wave::serializeProject(
                   *repairedMissingTarget.project)
                == wave::serializeProject(
                    *repeatedMissingTargetRepair.project)
            && brokenRelationProject.scenarios.front()
                   .relations.front()
                   .targetEventId
                == "missing-target-event",
        "update-relation did not atomically repair an explicitly replaced target");

    const auto partialTargetRepair =
        wave::applyAutomationBatch(
            brokenRelationProject,
            relationUpdateBatch(
                QJsonObject{
                    {QStringLiteral("op"),
                     QStringLiteral("update-relation")},
                    {QStringLiteral("relationId"),
                     QStringLiteral("relation-req-ack")},
                    {QStringLiteral("targetLaneId"),
                     QStringLiteral("ack")},
                }));
    const auto metadataOnlyBrokenUpdate =
        wave::applyAutomationBatch(
            brokenRelationProject,
            relationUpdateBatch(
                QJsonObject{
                    {QStringLiteral("op"),
                     QStringLiteral("update-relation")},
                    {QStringLiteral("relationId"),
                     QStringLiteral("relation-req-ack")},
                    {QStringLiteral("description"),
                     QStringLiteral("Cannot bypass repair")},
                }));
    expect(
        !partialTargetRepair.ok()
            && partialTargetRepair.failedOperation == 0
            && !partialTargetRepair.project
            && partialTargetRepair.error.contains(
                QStringLiteral("'targetLaneId'"))
            && partialTargetRepair.error.contains(
                QStringLiteral("'targetAtTick'"))
            && !metadataOnlyBrokenUpdate.ok()
            && metadataOnlyBrokenUpdate.failedOperation == 0
            && !metadataOnlyBrokenUpdate.project
            && metadataOnlyBrokenUpdate.error.contains(
                QStringLiteral("missing-event")),
        "update-relation guessed or bypassed a broken target endpoint");

    const auto ambiguousSourceRepair =
        wave::applyAutomationBatch(
            duplicateEventIdProject,
            relationUpdateBatch(
                QJsonObject{
                    {QStringLiteral("op"),
                     QStringLiteral("update-relation")},
                    {QStringLiteral("relationId"),
                     QStringLiteral("relation-req-ack")},
                    {QStringLiteral("sourceLaneId"),
                     QStringLiteral("data[7:0]")},
                    {QStringLiteral("sourceAtTick"),
                     QStringLiteral("80000")},
                }));
    expect(
        ambiguousSourceRepair.ok(),
        ambiguousSourceRepair.error.toStdString());
    const auto* ambiguousSourceRepairedRelation =
        wave::findRelation(
            ambiguousSourceRepair.project->scenarios.front(),
            "relation-req-ack");
    const auto ambiguousSourceRepairReport =
        ambiguousSourceRepair.json
            .value(QStringLiteral("operations"))
            .toArray()
            .at(0)
            .toObject();
    expect(
        ambiguousSourceRepairedRelation
            && ambiguousSourceRepairedRelation->sourceEventId
                == dataRepairEdge->id
            && ambiguousSourceRepairReport.value(
                   QStringLiteral("repairedSourceEndpoint")).toBool()
            && !ambiguousSourceRepairReport.value(
                    QStringLiteral("repairedTargetEndpoint")).toBool(),
        "update-relation did not replace an ambiguous source Event ID");

    const auto detachedSourceRepair =
        wave::applyAutomationBatch(
            detachedEndpointProject,
            relationUpdateBatch(
                QJsonObject{
                    {QStringLiteral("op"),
                     QStringLiteral("update-relation")},
                    {QStringLiteral("relationId"),
                     QStringLiteral("relation-req-ack")},
                    {QStringLiteral("sourceLaneId"),
                     QStringLiteral("data[7:0]")},
                    {QStringLiteral("sourceAt"),
                     QStringLiteral("80 ns")},
                }));
    expect(
        detachedSourceRepair.ok(),
        detachedSourceRepair.error.toStdString());
    expect(
        wave::findRelation(
                   detachedSourceRepair.project->scenarios.front(),
                   "relation-req-ack")
                   ->sourceEventId
                == dataRepairEdge->id
            && detachedSourceRepair.json
                   .value(QStringLiteral("operations"))
                   .toArray()
                   .at(0)
                   .toObject()
                   .value(QStringLiteral("repairedSourceEndpoint"))
                   .toBool(),
        "update-relation did not replace a detached source Event");

    const auto missingEventIdRepair =
        wave::applyAutomationBatch(
            missingEventIdProject,
            relationUpdateBatch(
                QJsonObject{
                    {QStringLiteral("op"),
                     QStringLiteral("update-relation")},
                    {QStringLiteral("relationId"),
                     QStringLiteral("relation-req-ack")},
                    {QStringLiteral("sourceLaneId"),
                     QStringLiteral("data[7:0]")},
                    {QStringLiteral("sourceAt"),
                     QStringLiteral("80 ns")},
                }));
    expect(
        missingEventIdRepair.ok()
            && wave::findRelation(
                   missingEventIdRepair.project->scenarios.front(),
                   "relation-req-ack")
                   ->sourceEventId
                == dataRepairEdge->id
            && missingEventIdRepair.json
                   .value(QStringLiteral("operations"))
                   .toArray()
                   .at(0)
                   .toObject()
                   .value(QStringLiteral("repairedSourceEndpoint"))
                   .toBool(),
        "update-relation did not replace a source Event without a stable ID");

    auto bothEndpointsBrokenProject = brokenRelationProject;
    bothEndpointsBrokenProject.scenarios.front()
        .relations.front()
        .sourceEventId = "missing-source-event";
    const auto bothEndpointRepair =
        wave::applyAutomationBatch(
            bothEndpointsBrokenProject,
            relationUpdateBatch(
                QJsonObject{
                    {QStringLiteral("op"),
                     QStringLiteral("update-relation")},
                    {QStringLiteral("relationId"),
                     QStringLiteral("relation-req-ack")},
                    {QStringLiteral("sourceLaneId"),
                     QStringLiteral("data[7:0]")},
                    {QStringLiteral("sourceAt"),
                     QStringLiteral("80 ns")},
                    {QStringLiteral("targetLaneId"),
                     QStringLiteral("ack")},
                    {QStringLiteral("targetAt"),
                     QStringLiteral("110 ns")},
                }));
    expect(
        bothEndpointRepair.ok(),
        bothEndpointRepair.error.toStdString());
    const auto bothEndpointRepairReport =
        bothEndpointRepair.json
            .value(QStringLiteral("operations"))
            .toArray()
            .at(0)
            .toObject();
    expect(
        bothEndpointRepairReport.value(
            QStringLiteral("repairedSourceEndpoint")).toBool()
            && bothEndpointRepairReport.value(
                QStringLiteral("repairedTargetEndpoint")).toBool(),
        "update-relation did not repair both explicitly replaced endpoints");

    auto ambiguousReplacementProject = brokenRelationProject;
    auto duplicateReplacementTarget =
        *acknowledgementRepairEdge;
    duplicateReplacementTarget.id = "event-ack-high-copy";
    ambiguousReplacementProject.scenarios.front().events.push_back(
        std::move(duplicateReplacementTarget));
    const auto rejectedAmbiguousReplacement =
        wave::applyAutomationBatch(
            ambiguousReplacementProject,
            targetRepairBatch);
    auto duplicateReplacementIdProject = brokenRelationProject;
    auto duplicateReplacementId =
        *acknowledgementRepairEdge;
    duplicateReplacementId.laneId = "lane-reset";
    duplicateReplacementId.tick = 0;
    duplicateReplacementId.linkedSegmentId =
        "segment-reset-low";
    duplicateReplacementIdProject.scenarios.front().events.push_back(
        std::move(duplicateReplacementId));
    const auto rejectedDuplicateReplacementId =
        wave::applyAutomationBatch(
            duplicateReplacementIdProject,
            targetRepairBatch);
    wave::AutomationEdgeQueryOptions replacementEdgeQuery;
    replacementEdgeQuery.start = 110'000;
    replacementEdgeQuery.end = 111'000;
    const auto duplicateIdEdges =
        wave::findWaveformEdgesForAutomation(
            duplicateReplacementIdProject,
            replacementEdgeQuery,
            sourceScenario.id,
            {"ack"});
    const auto duplicateIdEdge =
        duplicateIdEdges.json
            .value(QStringLiteral("edges"))
            .toArray()
            .at(0)
            .toObject();
    expect(
        !rejectedAmbiguousReplacement.ok()
            && rejectedAmbiguousReplacement.failedOperation == 0
            && !rejectedAmbiguousReplacement.project
            && rejectedAmbiguousReplacement.error.contains(
                QStringLiteral("Multiple waveform events"))
            && !rejectedDuplicateReplacementId.ok()
            && rejectedDuplicateReplacementId.failedOperation == 0
            && !rejectedDuplicateReplacementId.project
            && rejectedDuplicateReplacementId.error.contains(
                QStringLiteral("Event ID"))
            && rejectedDuplicateReplacementId.error.contains(
                QStringLiteral("ambiguous"))
            && duplicateIdEdges.ok()
            && duplicateIdEdges.json.value(
                   QStringLiteral("ambiguousEndpointCount")).toInt()
                == 1
            && duplicateIdEdge.value(
                   QStringLiteral("candidateCount")).toInt()
                == 1
            && duplicateIdEdge.value(
                   QStringLiteral("eventIdCount")).toInt()
                == 2
            && !duplicateIdEdge.value(
                    QStringLiteral("relationEndpoint")).toBool(),
        "update-relation accepted or edge lookup advertised an ambiguous replacement");

    const auto validation = wave::validateProjectForAutomation(
        source, sourceScenario.id);
    expect(validation.ok(), validation.error.toStdString());
    expectEqual(
        validation.json.value(QStringLiteral("command")).toString(),
        QStringLiteral("validate"),
        "automation validation command is incorrect");
    expect(
        validation.json.value(QStringLiteral("summary")).isObject()
            && validation.json.value(QStringLiteral("issues")).isArray(),
        "automation validation report is not machine-readable");

    QJsonArray operations;
    operations.append(QJsonObject{
        {QStringLiteral("op"), QStringLiteral("add-signal")},
        {QStringLiteral("id"), QStringLiteral("lane-automation-data")},
        {QStringLiteral("name"), QStringLiteral("automation_data")},
        {QStringLiteral("kind"), QStringLiteral("bus")},
        {QStringLiteral("width"), 8},
        {QStringLiteral("color"), QStringLiteral("#4fc3f7")},
    });
    operations.append(QJsonObject{
        {QStringLiteral("op"), QStringLiteral("set-range")},
        {QStringLiteral("laneId"), QStringLiteral("lane-automation-data")},
        {QStringLiteral("startTick"), QStringLiteral("0")},
        {QStringLiteral("endTick"), QStringLiteral("10000")},
        {QStringLiteral("value"), QStringLiteral("0xa5")},
    });
    operations.append(QJsonObject{
        {QStringLiteral("op"), QStringLiteral("rename-lane")},
        {QStringLiteral("laneId"), QString::fromStdString(bit->id)},
        {QStringLiteral("name"), QStringLiteral("automation_bit")},
    });
    operations.append(QJsonObject{
        {QStringLiteral("op"), QStringLiteral("set-duration")},
        {QStringLiteral("durationTick"),
         QString::number(sourceScenario.duration + 1'000)},
    });
    const QJsonObject batch{
        {QStringLiteral("schema"),
         QString::fromLatin1(wave::AutomationBatchSchema)},
        {QStringLiteral("expectedProjectId"),
         QString::fromStdString(source.id)},
        {QStringLiteral("scenarioId"),
         QString::fromStdString(sourceScenario.id)},
        {QStringLiteral("operations"), operations},
    };
    const auto applied = wave::applyAutomationBatch(source, batch);
    const auto repeatedApplied = wave::applyAutomationBatch(source, batch);
    expect(applied.ok(), applied.error.toStdString());
    expect(
        repeatedApplied.ok()
            && wave::serializeProject(*applied.project)
                == wave::serializeProject(*repeatedApplied.project),
        "repeated automation apply generated unstable segment or event identities");
    expect(applied.changed, "automation batch did not report its edits");
    expectEqual(source, sourceBefore, "automation mutated its source project");
    expectEqual(
        applied.json.value(QStringLiteral("operationCount")).toInt(),
        4,
        "automation batch reported the wrong operation count");
    expect(
        applied.json.value(QStringLiteral("validation")).isObject(),
        "automation apply omitted post-edit validation");
    const auto& resultScenario = applied.project->scenarios.front();
    expectEqual(
        resultScenario.duration,
        sourceScenario.duration + 1'000,
        "automation did not extend the scenario duration");
    const auto* added = wave::findLane(resultScenario, "lane-automation-data");
    const auto* renamed = wave::findLane(resultScenario, bit->id);
    expect(
        added && added->kind == wave::LaneKind::Bus && added->width == 8,
        "automation did not add the requested Bus signal");
    expect(
        added && !added->segments.empty()
            && added->segments.front().start == 0
            && added->segments.front().end == 10'000
            && added->segments.front().value == "0xa5",
        "automation did not set the requested Bus range");
    expect(
        renamed && renamed->name == "automation_bit",
        "automation did not rename the requested signal");
    const auto roundTrip = wave::deserializeProject(
        wave::serializeProject(*applied.project));
    expect(
        roundTrip.ok() && *roundTrip.project == *applied.project,
        "automation result did not survive project serialization");

    const QJsonObject advancedBatch{
        {QStringLiteral("schema"),
         QString::fromLatin1(wave::AutomationBatchSchema)},
        {QStringLiteral("scenarioId"),
         QString::fromStdString(sourceScenario.id)},
        {QStringLiteral("operations"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("set-range")},
                 {QStringLiteral("laneId"), QStringLiteral("lane-request")},
                 {QStringLiteral("start"), QStringLiteral("cycle 5")},
                 {QStringLiteral("end"), QStringLiteral("60 ns")},
                 {QStringLiteral("value"), QStringLiteral("1")},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("update-signal")},
                 {QStringLiteral("laneId"), QStringLiteral("lane-data")},
                 {QStringLiteral("name"), QStringLiteral("payload")},
                 {QStringLiteral("radix"), QStringLiteral("binary")},
                 {QStringLiteral("color"), QStringLiteral("#4fc3f7")},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("move-signal")},
                 {QStringLiteral("laneId"), QStringLiteral("lane-data")},
                 {QStringLiteral("beforeLaneId"),
                  QStringLiteral("lane-request")},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("update-clock")},
                 {QStringLiteral("clockId"), QStringLiteral("clock-main")},
                 {QStringLiteral("period"), QStringLiteral("12 ns")},
                 {QStringLiteral("activeEdge"), QStringLiteral("falling")},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("delete-signal")},
                 {QStringLiteral("laneId"), QStringLiteral("lane-ack")},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("set-duration")},
                 {QStringLiteral("duration"), QStringLiteral("230 ns")},
             },
         }},
    };
    const auto advanced =
        wave::applyAutomationBatch(source, advancedBatch);
    const auto repeatedAdvanced =
        wave::applyAutomationBatch(source, advancedBatch);
    expect(advanced.ok(), advanced.error.toStdString());
    expect(repeatedAdvanced.ok(), repeatedAdvanced.error.toStdString());
    const auto& advancedScenario = advanced.project->scenarios.front();
    const auto* advancedPayload =
        wave::findLane(advancedScenario, "lane-data");
    const auto* advancedClock =
        wave::findClock(*advanced.project, "clock-main");
    const auto advancedSample = wave::sampleProjectForAutomation(
        *advanced.project,
        55'000,
        advancedScenario.id,
        {"lane-request"});
    expect(
        advanced.ok() && repeatedAdvanced.ok()
            && wave::serializeProject(*advanced.project)
                == wave::serializeProject(*repeatedAdvanced.project)
            && advancedScenario.duration == 230'000
            && advancedScenario.lanes.at(3).id == "lane-data"
            && advancedPayload
            && advancedPayload->name == "payload"
            && advancedPayload->radix == wave::Radix::Binary
            && !wave::findLane(advancedScenario, "lane-ack")
            && advancedScenario.relations.empty()
            && advancedClock && advancedClock->period == 12'000
            && advancedClock->activeEdge == wave::ClockEdge::Falling
            && advancedSample.ok()
            && advancedSample.json
                    .value(QStringLiteral("samples"))
                    .toArray()
                    .at(0)
                    .toObject()
                    .value(QStringLiteral("value"))
                    .toString()
                == QStringLiteral("1"),
        "advanced automation operations did not form one deterministic batch");

    const QJsonObject rangeWorkflowBatch{
        {QStringLiteral("schema"),
         QString::fromLatin1(wave::AutomationBatchSchema)},
        {QStringLiteral("scenarioId"),
         QString::fromStdString(sourceScenario.id)},
        {QStringLiteral("operations"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("assert-value")},
                 {QStringLiteral("laneId"),
                  QStringLiteral("lane-request")},
                 {QStringLiteral("at"), QStringLiteral("70 ns")},
                 {QStringLiteral("value"), QStringLiteral("0")},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("set-range")},
                 {QStringLiteral("start"), QStringLiteral("50 ns")},
                 {QStringLiteral("end"), QStringLiteral("60 ns")},
                 {QStringLiteral("assignments"),
                  QJsonArray{
                      QJsonObject{
                          {QStringLiteral("laneId"),
                           QStringLiteral("lane-request")},
                          {QStringLiteral("value"), QStringLiteral("1")},
                      },
                      QJsonObject{
                          {QStringLiteral("laneId"),
                           QStringLiteral("lane-ack")},
                          {QStringLiteral("value"), QStringLiteral("1")},
                      },
                  }},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("add-signal")},
                 {QStringLiteral("id"), QStringLiteral("lane-data-mirror")},
                 {QStringLiteral("name"), QStringLiteral("data_mirror")},
                 {QStringLiteral("kind"), QStringLiteral("bus")},
                 {QStringLiteral("width"), 8},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("add-signal")},
                 {QStringLiteral("id"), QStringLiteral("lane-request-moved")},
                 {QStringLiteral("name"), QStringLiteral("request_moved")},
                 {QStringLiteral("kind"), QStringLiteral("bit")},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("transfer-range")},
                 {QStringLiteral("mode"), QStringLiteral("copy")},
                 {QStringLiteral("sourceStart"), QStringLiteral("80 ns")},
                 {QStringLiteral("sourceEnd"), QStringLiteral("100 ns")},
                 {QStringLiteral("destination"), QStringLiteral("0 ns")},
                 {QStringLiteral("mappings"),
                  QJsonArray{
                      QJsonObject{
                          {QStringLiteral("sourceLaneId"),
                           QStringLiteral("lane-data")},
                          {QStringLiteral("targetLaneId"),
                           QStringLiteral("lane-data-mirror")},
                      },
                  }},
             },
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("transfer-range")},
                 {QStringLiteral("mode"), QStringLiteral("move")},
                 {QStringLiteral("sourceStart"), QStringLiteral("80 ns")},
                 {QStringLiteral("sourceEnd"), QStringLiteral("100 ns")},
                 {QStringLiteral("destination"), QStringLiteral("0 ns")},
                 {QStringLiteral("mappings"),
                  QJsonArray{
                      QJsonObject{
                          {QStringLiteral("sourceLaneId"),
                           QStringLiteral("lane-request")},
                          {QStringLiteral("targetLaneId"),
                           QStringLiteral("lane-request-moved")},
                      },
                  }},
             },
         }},
    };
    const auto rangeWorkflow =
        wave::applyAutomationBatch(source, rangeWorkflowBatch);
    const auto repeatedRangeWorkflow =
        wave::applyAutomationBatch(source, rangeWorkflowBatch);
    expect(rangeWorkflow.ok(), rangeWorkflow.error.toStdString());
    expect(
        repeatedRangeWorkflow.ok()
            && wave::serializeProject(*rangeWorkflow.project)
                == wave::serializeProject(*repeatedRangeWorkflow.project),
        "range workflow generated unstable results");
    const auto& rangeScenario = rangeWorkflow.project->scenarios.front();
    const auto rangeSamples = wave::sampleProjectForAutomation(
        *rangeWorkflow.project,
        55'000,
        rangeScenario.id,
        {"lane-request", "lane-ack"});
    const auto transferSamples = wave::sampleProjectForAutomation(
        *rangeWorkflow.project,
        10'000,
        rangeScenario.id,
        {"lane-data-mirror", "lane-request-moved"});
    const auto clearedSource = wave::sampleProjectForAutomation(
        *rangeWorkflow.project,
        90'000,
        rangeScenario.id,
        {"lane-request"});
    expect(
        rangeWorkflow.ok()
            && rangeSamples.ok()
            && rangeSamples.json
                    .value(QStringLiteral("samples"))
                    .toArray()
                    .at(0)
                    .toObject()
                    .value(QStringLiteral("value"))
                    .toString()
                == QStringLiteral("1")
            && rangeSamples.json
                    .value(QStringLiteral("samples"))
                    .toArray()
                    .at(1)
                    .toObject()
                    .value(QStringLiteral("value"))
                    .toString()
                == QStringLiteral("1")
            && transferSamples.ok()
            && transferSamples.json
                    .value(QStringLiteral("samples"))
                    .toArray()
                    .at(0)
                    .toObject()
                    .value(QStringLiteral("value"))
                    .toString()
                == QStringLiteral("0x35")
            && transferSamples.json
                    .value(QStringLiteral("samples"))
                    .toArray()
                    .at(1)
                    .toObject()
                    .value(QStringLiteral("value"))
                    .toString()
                == QStringLiteral("1")
            && clearedSource.ok()
            && clearedSource.json
                    .value(QStringLiteral("samples"))
                    .toArray()
                    .at(0)
                    .toObject()
                    .value(QStringLiteral("value"))
                    .toString()
                == QStringLiteral("0"),
        "multi-lane assignment or cross-lane range transfer is incorrect");

    const auto protectedTransfer = wave::applyAutomationBatch(
        source,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("scenarioId"),
             QString::fromStdString(sourceScenario.id)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"),
                      QStringLiteral("transfer-range")},
                     {QStringLiteral("mode"), QStringLiteral("copy")},
                     {QStringLiteral("laneId"),
                      QStringLiteral("lane-data")},
                     {QStringLiteral("sourceStart"),
                      QStringLiteral("80 ns")},
                     {QStringLiteral("sourceEnd"),
                      QStringLiteral("100 ns")},
                     {QStringLiteral("destination"),
                      QStringLiteral("0 ns")},
                 },
             }},
        });
    expect(
        !protectedTransfer.ok()
            && protectedTransfer.error.contains(QStringLiteral("overwrite"))
            && !protectedTransfer.project,
        "range transfer silently replaced explicit target content");

    const auto confirmedTransfer = wave::applyAutomationBatch(
        source,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("scenarioId"),
             QString::fromStdString(sourceScenario.id)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"),
                      QStringLiteral("transfer-range")},
                     {QStringLiteral("mode"), QStringLiteral("copy")},
                     {QStringLiteral("laneId"),
                      QStringLiteral("lane-data")},
                     {QStringLiteral("sourceStart"),
                      QStringLiteral("80 ns")},
                     {QStringLiteral("sourceEnd"),
                      QStringLiteral("100 ns")},
                     {QStringLiteral("destination"),
                      QStringLiteral("0 ns")},
                     {QStringLiteral("overwrite"), true},
                 },
             }},
        });
    const auto confirmedTransferSample = confirmedTransfer.ok()
        ? wave::sampleProjectForAutomation(
              *confirmedTransfer.project,
              10'000,
              sourceScenario.id,
              {"lane-data"})
        : wave::AutomationDocument{};
    expect(
        confirmedTransfer.ok() && confirmedTransferSample.ok()
            && confirmedTransferSample.json
                    .value(QStringLiteral("samples"))
                    .toArray()
                    .at(0)
                    .toObject()
                    .value(QStringLiteral("value"))
                    .toString()
                == QStringLiteral("0x35"),
        "explicit overwrite confirmation did not transfer the range");

    const auto failedAssertion = wave::applyAutomationBatch(
        source,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("scenarioId"),
             QString::fromStdString(sourceScenario.id)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"), QStringLiteral("rename-lane")},
                     {QStringLiteral("laneId"),
                      QStringLiteral("lane-request")},
                     {QStringLiteral("name"),
                      QStringLiteral("must_not_escape")},
                 },
                 QJsonObject{
                     {QStringLiteral("op"),
                      QStringLiteral("assert-value")},
                     {QStringLiteral("laneId"),
                      QStringLiteral("lane-request")},
                     {QStringLiteral("at"), QStringLiteral("80 ns")},
                     {QStringLiteral("value"), QStringLiteral("0")},
                 },
             }},
        });
    expect(
        !failedAssertion.ok() && failedAssertion.failedOperation == 1
            && failedAssertion.error.contains(
                QStringLiteral("expected '0', actual '1'"))
            && !failedAssertion.project && source == sourceBefore,
        "failed value assertion exposed earlier batch edits");

    QJsonArray clockOperations;
    clockOperations.append(QJsonObject{
        {QStringLiteral("op"), QStringLiteral("add-signal")},
        {QStringLiteral("id"), QStringLiteral("lane-automation-clock")},
        {QStringLiteral("clockId"), QStringLiteral("clock-automation")},
        {QStringLiteral("name"), QStringLiteral("automation_clk")},
        {QStringLiteral("kind"), QStringLiteral("clock")},
        {QStringLiteral("period"), QStringLiteral("12 ns")},
        {QStringLiteral("phase"), QStringLiteral("2.5 ns")},
        {QStringLiteral("activeEdge"), QStringLiteral("falling")},
    });
    const auto clockApplied = wave::applyAutomationBatch(
        source,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("scenarioId"),
             QString::fromStdString(sourceScenario.id)},
            {QStringLiteral("operations"), clockOperations},
        });
    const auto* addedClock = clockApplied.ok()
        ? wave::findClock(*clockApplied.project, "clock-automation")
        : nullptr;
    const auto* addedClockLane = clockApplied.ok()
        ? wave::findLane(
              clockApplied.project->scenarios.front(),
              "lane-automation-clock")
        : nullptr;
    expect(clockApplied.ok(), clockApplied.error.toStdString());
    expect(
        addedClock && addedClock->period == 12'000
            && addedClock->phase == 2'500
            && addedClock->activeEdge == wave::ClockEdge::Falling
            && addedClockLane
            && addedClockLane->clockDomainId == "clock-automation",
        "automation did not atomically add a Clock lane and domain");

    const QJsonObject implicitIdentityBatch{
        {QStringLiteral("schema"),
         QString::fromLatin1(wave::AutomationBatchSchema)},
        {QStringLiteral("scenarioId"),
         QString::fromStdString(sourceScenario.id)},
        {QStringLiteral("operations"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("add-signal")},
                 {QStringLiteral("name"), QStringLiteral("implicit_identity")},
                 {QStringLiteral("kind"), QStringLiteral("bit")},
             },
         }},
    };
    const auto firstImplicit =
        wave::applyAutomationBatch(source, implicitIdentityBatch);
    const auto secondImplicit =
        wave::applyAutomationBatch(source, implicitIdentityBatch);
    expect(
        firstImplicit.ok() && secondImplicit.ok()
            && wave::serializeProject(*firstImplicit.project)
                == wave::serializeProject(*secondImplicit.project),
        "implicit automation IDs or colors made repeated dry-runs unstable");

    const auto cleared = wave::applyAutomationBatch(
        source,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("scenarioId"),
             QString::fromStdString(sourceScenario.id)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"), QStringLiteral("clear-range")},
                     {QStringLiteral("laneId"),
                      QStringLiteral("lane-request")},
                     {QStringLiteral("startTick"), QStringLiteral("80000")},
                     {QStringLiteral("endTick"), QStringLiteral("90000")},
                 },
             }},
        });
    const auto* clearedRequest = cleared.ok()
        ? wave::findLane(
              cleared.project->scenarios.front(),
              "lane-request")
        : nullptr;
    expect(
        cleared.ok() && clearedRequest
            && std::none_of(
                clearedRequest->segments.begin(),
                clearedRequest->segments.end(),
                [](const wave::Segment& segment) {
                    return segment.start <= 85'000
                        && 85'000 < segment.end;
                })
            && cleared.project->scenarios.front().relations.empty(),
        "automation clear-range did not clear the interval and its dependency");

    const auto partiallyInvalid = wave::applyAutomationBatch(
        source,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("scenarioId"),
             QString::fromStdString(sourceScenario.id)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"), QStringLiteral("rename-lane")},
                     {QStringLiteral("laneId"),
                      QString::fromStdString(bit->id)},
                     {QStringLiteral("name"),
                      QStringLiteral("must_not_escape")},
                 },
                 QJsonObject{
                     {QStringLiteral("op"), QStringLiteral("set-range")},
                     {QStringLiteral("laneId"),
                      QStringLiteral("missing-lane")},
                     {QStringLiteral("startTick"), QStringLiteral("0")},
                     {QStringLiteral("endTick"), QStringLiteral("10000")},
                     {QStringLiteral("value"), QStringLiteral("1")},
                 },
             }},
        });
    expect(
        !partiallyInvalid.ok() && partiallyInvalid.failedOperation == 1
            && !partiallyInvalid.project && source == sourceBefore,
        "failed automation batch exposed a partial edit");

    const auto invalid = wave::applyAutomationBatch(
        source,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("scenarioId"),
             QString::fromStdString(sourceScenario.id)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"), QStringLiteral("unknown-op")},
                 },
             }},
        });
    expect(
        !invalid.ok() && invalid.failedOperation == 0
            && invalid.error.contains(QStringLiteral("Unknown operation"))
            && !invalid.project,
        "unknown automation operation was not rejected atomically");

    const auto conflictingTimeFields = wave::applyAutomationBatch(
        source,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("scenarioId"),
             QString::fromStdString(sourceScenario.id)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"), QStringLiteral("set-range")},
                     {QStringLiteral("laneId"),
                      QStringLiteral("lane-request")},
                     {QStringLiteral("startTick"), QStringLiteral("0")},
                     {QStringLiteral("start"), QStringLiteral("0 ns")},
                     {QStringLiteral("endTick"), QStringLiteral("10000")},
                     {QStringLiteral("value"), QStringLiteral("1")},
                 },
             }},
        });
    expect(
        !conflictingTimeFields.ok()
            && conflictingTimeFields.failedOperation == 0
            && conflictingTimeFields.error.contains(
                QStringLiteral("exactly one"))
            && !conflictingTimeFields.project,
        "automation accepted conflicting tick and physical-time fields");

    auto ambiguousClockSource = source;
    auto secondClock = ambiguousClockSource.clockDomains.front();
    secondClock.id = "clock-secondary";
    secondClock.name = "secondary";
    ambiguousClockSource.clockDomains.push_back(std::move(secondClock));
    const auto ambiguousCycle = wave::applyAutomationBatch(
        ambiguousClockSource,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("scenarioId"),
             QString::fromStdString(sourceScenario.id)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"), QStringLiteral("set-duration")},
                     {QStringLiteral("duration"),
                      QStringLiteral("cycle 30")},
                 },
             }},
        });
    expect(
        !ambiguousCycle.ok() && ambiguousCycle.failedOperation == 0
            && ambiguousCycle.error.contains(
                QStringLiteral("unambiguous clock"))
            && !ambiguousCycle.project,
        "cycle-based automation guessed between multiple clocks");

    const auto invalidBitWidth = wave::applyAutomationBatch(
        source,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("scenarioId"),
             QString::fromStdString(sourceScenario.id)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"), QStringLiteral("update-signal")},
                     {QStringLiteral("laneId"),
                      QString::fromStdString(bit->id)},
                     {QStringLiteral("width"), 8},
                 },
             }},
        });
    expect(
        !invalidBitWidth.ok() && invalidBitWidth.failedOperation == 0
            && invalidBitWidth.error.contains(QStringLiteral("Bus/Enum"))
            && !invalidBitWidth.project && source == sourceBefore,
        "invalid Bit width escaped an atomic automation batch");

    const auto guardedShrink = wave::applyAutomationBatch(
        source,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("scenarioId"),
             QString::fromStdString(sourceScenario.id)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"), QStringLiteral("rename-lane")},
                     {QStringLiteral("laneId"), QStringLiteral("lane-request")},
                     {QStringLiteral("name"), QStringLiteral("renamed-before-shrink")},
                 },
                 QJsonObject{
                     {QStringLiteral("op"), QStringLiteral("set-duration")},
                     {QStringLiteral("duration"), QStringLiteral("120 ns")},
                 },
             }},
        });
    expect(
        !guardedShrink.ok() && guardedShrink.failedOperation == 1
            && guardedShrink.error.contains(QStringLiteral("truncate"))
            && guardedShrink.error.contains(QStringLiteral("clip"))
            && !guardedShrink.project && source == sourceBefore,
        "destructive duration shrink was not rejected atomically");

    const auto invalidTruncateType = wave::applyAutomationBatch(
        source,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("scenarioId"),
             QString::fromStdString(sourceScenario.id)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"), QStringLiteral("set-duration")},
                     {QStringLiteral("duration"), QStringLiteral("120 ns")},
                     {QStringLiteral("truncate"), QStringLiteral("true")},
                 },
             }},
        });
    expect(
        !invalidTruncateType.ok()
            && invalidTruncateType.failedOperation == 0
            && invalidTruncateType.error.contains(QStringLiteral("boolean"))
            && !invalidTruncateType.project
            && source == sourceBefore,
        "non-boolean duration truncation opt-in was accepted");

    const QJsonObject truncationBatch{
        {QStringLiteral("schema"),
         QString::fromLatin1(wave::AutomationBatchSchema)},
        {QStringLiteral("scenarioId"),
         QString::fromStdString(sourceScenario.id)},
        {QStringLiteral("operations"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("op"), QStringLiteral("set-duration")},
                 {QStringLiteral("duration"), QStringLiteral("120 ns")},
                 {QStringLiteral("truncate"), true},
             },
         }},
    };
    const auto truncated =
        wave::applyAutomationBatch(source, truncationBatch);
    const auto repeatedTruncated =
        wave::applyAutomationBatch(source, truncationBatch);
    expect(truncated.ok(), truncated.error.toStdString());
    expect(
        repeatedTruncated.ok()
            && wave::serializeProject(*truncated.project)
                == wave::serializeProject(*repeatedTruncated.project),
        "duration truncation is not deterministic");
    const auto truncationReport =
        truncated.json.value(QStringLiteral("operations"))
            .toArray()
            .at(0)
            .toObject();
    const auto& truncatedScenario =
        truncated.project->scenarios.front();
    expect(
        truncated.changed
            && truncationReport.value(
                   QStringLiteral("direction")).toString()
                == QStringLiteral("shrink")
            && truncationReport.value(
                   QStringLiteral("truncateRequested")).toBool()
            && truncationReport.value(
                   QStringLiteral("contentTruncated")).toBool()
            && truncationReport.value(
                   QStringLiteral("clippedSegmentCount")).toInt()
                == 5
            && truncationReport.value(
                   QStringLiteral("removedSegmentCount")).toInt()
                == 6
            && truncationReport.value(
                   QStringLiteral("removedEventCount")).toInt()
                == 4
            && truncationReport.value(
                   QStringLiteral("removedRelationCount")).toInt()
                == 0
            && truncationReport.value(
                   QStringLiteral("clippedMarkerCount")).toInt()
                == 0
            && truncationReport.value(
                   QStringLiteral("removedMarkerCount")).toInt()
                == 0,
        "duration truncation report does not describe the removed content: "
            + QJsonDocument(truncationReport)
                  .toJson(QJsonDocument::Compact)
                  .toStdString());
    expect(
        truncatedScenario.duration == 120'000
            && std::all_of(
                truncatedScenario.lanes.begin(),
                truncatedScenario.lanes.end(),
                [](const wave::Lane& lane) {
                    return std::all_of(
                        lane.segments.begin(),
                        lane.segments.end(),
                        [](const wave::Segment& segment) {
                            return segment.start < 120'000
                                && segment.end <= 120'000;
                        });
                })
            && std::all_of(
                truncatedScenario.events.begin(),
                truncatedScenario.events.end(),
                [](const wave::Event& event) {
                    return event.tick < 120'000;
                })
            && std::all_of(
                truncatedScenario.relations.begin(),
                truncatedScenario.relations.end(),
                [&truncatedScenario](const wave::Relation& relation) {
                    return wave::findEvent(
                               truncatedScenario,
                               relation.sourceEventId)
                        && wave::findEvent(
                            truncatedScenario,
                            relation.targetEventId);
                })
            && std::all_of(
                truncatedScenario.markers.begin(),
                truncatedScenario.markers.end(),
                [](const wave::Marker& marker) {
                    return marker.start <= 120'000
                        && marker.end <= 120'000;
                }),
        "duration truncation left content beyond the new End");

    const auto safeShrink = wave::applyAutomationBatch(
        *created.project,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("scenarioId"),
             QString::fromStdString(
                 created.project->scenarios.front().id)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"), QStringLiteral("set-duration")},
                     {QStringLiteral("duration"), QStringLiteral("40 ns")},
                 },
             }},
        });
    const auto safeShrinkReport =
        safeShrink.json.value(QStringLiteral("operations"))
            .toArray()
            .at(0)
            .toObject();
    expect(
        safeShrink.ok() && safeShrink.changed
            && safeShrink.project->scenarios.front().duration == 40'000
            && safeShrinkReport.value(
                   QStringLiteral("direction")).toString()
                == QStringLiteral("shrink")
            && !safeShrinkReport.value(
                    QStringLiteral("truncateRequested")).toBool()
            && !safeShrinkReport.value(
                    QStringLiteral("contentTruncated")).toBool()
            && safeShrinkReport.value(
                   QStringLiteral("clippedSegmentCount")).toInt()
                == 0
            && safeShrinkReport.value(
                   QStringLiteral("removedEventCount")).toInt()
                == 0,
        "empty timeline tail could not be shortened without destructive opt-in");

    const auto unchangedDuration = wave::applyAutomationBatch(
        source,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("scenarioId"),
             QString::fromStdString(sourceScenario.id)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"), QStringLiteral("set-duration")},
                     {QStringLiteral("durationTick"),
                      QString::number(sourceScenario.duration)},
                 },
             }},
        });
    expect(
        unchangedDuration.ok() && !unchangedDuration.changed
            && !unchangedDuration.json
                    .value(QStringLiteral("operations"))
                    .toArray()
                    .at(0)
                    .toObject()
                    .value(QStringLiteral("changed"))
                    .toBool(),
        "unchanged scenario duration created a false-positive edit");

    const auto bitIndex = static_cast<std::size_t>(
        std::distance(sourceScenario.lanes.begin(), bit));
    const auto noEffectMove = wave::applyAutomationBatch(
        source,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("scenarioId"),
             QString::fromStdString(sourceScenario.id)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"), QStringLiteral("move-signal")},
                     {QStringLiteral("laneId"),
                      QString::fromStdString(bit->id)},
                     {QStringLiteral("destinationIndex"),
                      static_cast<qint64>(bitIndex)},
                 },
             }},
        });
    expect(
        noEffectMove.ok() && !noEffectMove.changed
            && *noEffectMove.project == source,
        "same-index signal move changed the project");

    const auto noEffect = wave::applyAutomationBatch(
        source,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("scenarioId"),
             QString::fromStdString(sourceScenario.id)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"), QStringLiteral("rename-lane")},
                     {QStringLiteral("laneId"),
                      QString::fromStdString(bit->id)},
                     {QStringLiteral("name"),
                      QString::fromStdString(bit->name)},
                 },
             }},
        });
    expect(
        noEffect.ok() && !noEffect.changed
            && *noEffect.project == source,
        "no-effect automation batch changed the project");
}

void testAutomationRelationRepairReferenceContracts()
{
    auto project = wave::makeDemonstrationProject();
    auto& scenario = project.scenarios.front();
    expect(
        scenario.relations.size() == 1,
        "Relation repair fixture is missing its base Relation");
    scenario.relations.front().extensions = {
        {"privateRelationState", R"({"selected":true})"},
    };
    auto duplicate = scenario.relations.front();
    duplicate.targetEventId = "missing-target-event";
    duplicate.description = "Imported duplicate identity";
    scenario.relations.push_back(duplicate);

    wave::AutomationRelationQueryOptions exactOptions;
    exactOptions.match = "relation-req-ack";
    exactOptions.exact = true;
    const auto duplicateQuery =
        wave::findRelationsForAutomation(
            project, exactOptions, scenario.id);
    const auto duplicateRows =
        duplicateQuery.json
            .value(QStringLiteral("relations"))
            .toArray();
    const auto firstRow = duplicateRows.at(0).toObject();
    const auto secondRow = duplicateRows.at(1).toObject();
    const auto firstRef =
        firstRow.value(QStringLiteral("relationRef")).toString();
    const auto secondRef =
        secondRow.value(QStringLiteral("relationRef")).toString();
    const auto encodedRows =
        QJsonDocument(duplicateRows)
            .toJson(QJsonDocument::Compact);
    expect(
        duplicateQuery.ok()
            && duplicateQuery.json
                   .value(QStringLiteral("matchCount")).toInt()
                == 2
            && duplicateQuery.json
                   .value(QStringLiteral("readyCount")).toInt()
                == 1
            && duplicateQuery.json
                   .value(QStringLiteral("addressableCount")).toInt()
                == 0
            && duplicateQuery.json
                   .value(QStringLiteral("identityIssueCount")).toInt()
                == 2
            && duplicateQuery.json
                   .value(QStringLiteral("endpointIssueCount")).toInt()
                == 1
            && duplicateRows.size() == 2
            && firstRow.value(QStringLiteral("idCount")).toInt() == 2
            && secondRow.value(QStringLiteral("idCount")).toInt() == 2
            && !firstRow.value(
                    QStringLiteral("addressable")).toBool()
            && !secondRow.value(
                    QStringLiteral("addressable")).toBool()
            && firstRow.value(
                   QStringLiteral("identityIssues")).toArray().contains(
                       QStringLiteral("duplicate-id"))
            && secondRow.value(
                   QStringLiteral("identityIssues")).toArray().contains(
                       QStringLiteral("duplicate-id"))
            && firstRef.startsWith(
                QStringLiteral("relation-ref-v1:"))
            && secondRef.startsWith(
                QStringLiteral("relation-ref-v1:"))
            && firstRef != secondRef
            && !encodedRows.contains("extensions")
            && !encodedRows.contains("privateRelationState"),
        "Relation query did not diagnose duplicate identity or expose distinct repair references");

    const auto operationBatch =
        [&scenario](QJsonObject operation) {
            return QJsonObject{
                {QStringLiteral("schema"),
                 QString::fromLatin1(
                     wave::AutomationBatchSchema)},
                {QStringLiteral("scenarioId"),
                 QString::fromStdString(scenario.id)},
                {QStringLiteral("operations"),
                 QJsonArray{std::move(operation)}},
            };
        };
    const auto ambiguousUpdate =
        wave::applyAutomationBatch(
            project,
            operationBatch(QJsonObject{
                {QStringLiteral("op"),
                 QStringLiteral("update-relation")},
                {QStringLiteral("relationId"),
                 QStringLiteral("relation-req-ack")},
                {QStringLiteral("description"),
                 QStringLiteral("Must not select the first duplicate")},
            }));
    const auto ambiguousDelete =
        wave::applyAutomationBatch(
            project,
            operationBatch(QJsonObject{
                {QStringLiteral("op"),
                 QStringLiteral("delete-relation")},
                {QStringLiteral("relationId"),
                 QStringLiteral("relation-req-ack")},
            }));
    expect(
        !ambiguousUpdate.ok()
            && ambiguousUpdate.failedOperation == 0
            && !ambiguousUpdate.project
            && ambiguousUpdate.error.contains(
                QStringLiteral("stable ID"))
            && ambiguousUpdate.error.contains(
                QStringLiteral("ambiguous"))
            && !ambiguousDelete.ok()
            && ambiguousDelete.failedOperation == 0
            && !ambiguousDelete.project,
        "Relation operations silently selected a duplicate stable ID");

    const auto repairBatch = operationBatch(QJsonObject{
        {QStringLiteral("op"),
         QStringLiteral("update-relation")},
        {QStringLiteral("relationRef"), secondRef},
        {QStringLiteral("newId"),
         QStringLiteral("relation-req-ack-follow-up")},
        {QStringLiteral("targetLaneId"),
         QStringLiteral("ack")},
        {QStringLiteral("targetAt"),
         QStringLiteral("110 ns")},
        {QStringLiteral("description"),
         QStringLiteral("Recovered duplicate identity and endpoint")},
    });
    const auto repaired =
        wave::applyAutomationBatch(project, repairBatch);
    expect(repaired.ok(), repaired.error.toStdString());
    const auto repairReport =
        repaired.json
            .value(QStringLiteral("operations"))
            .toArray()
            .at(0)
            .toObject();
    const auto& repairedScenario =
        repaired.project->scenarios.front();
    expect(
        repaired.changed
            && repairedScenario.relations.size() == 2
            && repairedScenario.relations.at(0)
                   == scenario.relations.at(0)
            && repairedScenario.relations.at(1).id
                == "relation-req-ack-follow-up"
            && repairedScenario.relations.at(1).targetEventId
                == scenario.relations.front().targetEventId
            && repairedScenario.relations.at(1).description
                == "Recovered duplicate identity and endpoint"
            && repairedScenario.relations.at(1).extensions
                == scenario.relations.at(1).extensions
            && repairReport.value(
                   QStringLiteral("selectedByRepairRef")).toBool()
            && repairReport.value(
                   QStringLiteral("repairedIdentity")).toBool()
            && !repairReport.value(
                    QStringLiteral("repairedSourceEndpoint")).toBool()
            && repairReport.value(
                   QStringLiteral("repairedTargetEndpoint")).toBool()
            && repairReport.value(
                   QStringLiteral("previousRelationId")).toString()
                == QStringLiteral("relation-req-ack")
            && repairReport.value(
                   QStringLiteral("relationId")).toString()
                == QStringLiteral(
                    "relation-req-ack-follow-up")
            && repairReport.value(
                   QStringLiteral("relationRef")).toString()
                != secondRef,
        "relationRef did not atomically repair identity and endpoint");

    wave::AutomationRelationQueryOptions oldIdOptions;
    oldIdOptions.match = "relation-req-ack";
    oldIdOptions.exact = true;
    wave::AutomationRelationQueryOptions newIdOptions;
    newIdOptions.match = "relation-req-ack-follow-up";
    newIdOptions.exact = true;
    const auto oldIdQuery =
        wave::findRelationsForAutomation(
            *repaired.project,
            oldIdOptions,
            repairedScenario.id);
    const auto newIdQuery =
        wave::findRelationsForAutomation(
            *repaired.project,
            newIdOptions,
            repairedScenario.id);
    expect(
        oldIdQuery.ok()
            && oldIdQuery.json
                   .value(QStringLiteral("matchCount")).toInt()
                == 1
            && oldIdQuery.json
                   .value(QStringLiteral("addressableCount")).toInt()
                == 1
            && newIdQuery.ok()
            && newIdQuery.json
                   .value(QStringLiteral("matchCount")).toInt()
                == 1
            && newIdQuery.json
                   .value(QStringLiteral("addressableCount")).toInt()
                == 1
            && newIdQuery.json
                   .value(QStringLiteral("readyCount")).toInt()
                == 1,
        "Relation identity repair did not make both Relations uniquely addressable");

    const auto staleRepair =
        wave::applyAutomationBatch(
            *repaired.project, repairBatch);
    const auto collidingRepair =
        wave::applyAutomationBatch(
            project,
            operationBatch(QJsonObject{
                {QStringLiteral("op"),
                 QStringLiteral("update-relation")},
                {QStringLiteral("relationRef"), secondRef},
                {QStringLiteral("newId"),
                 QStringLiteral("relation-req-ack")},
                {QStringLiteral("targetLaneId"),
                 QStringLiteral("ack")},
                {QStringLiteral("targetAt"),
                 QStringLiteral("110 ns")},
            }));
    expect(
        !staleRepair.ok()
            && staleRepair.failedOperation == 0
            && !staleRepair.project
            && staleRepair.error.contains(
                QStringLiteral("stale"))
            && !collidingRepair.ok()
            && collidingRepair.failedOperation == 0
            && !collidingRepair.project
            && collidingRepair.error.contains(
                QStringLiteral("already exists")),
        "Relation repair reference accepted stale state or a colliding ID");

    const auto deleted =
        wave::applyAutomationBatch(
            project,
            operationBatch(QJsonObject{
                {QStringLiteral("op"),
                 QStringLiteral("delete-relation")},
                {QStringLiteral("relationRef"), secondRef},
            }));
    expect(deleted.ok(), deleted.error.toStdString());
    const auto deleteReport =
        deleted.json
            .value(QStringLiteral("operations"))
            .toArray()
            .at(0)
            .toObject();
    expect(
        deleted.project->scenarios.front().relations.size() == 1
            && deleted.project->scenarios.front().relations.front()
                == scenario.relations.front()
            && deleteReport.value(
                   QStringLiteral("selectedByRepairRef")).toBool()
            && deleteReport.value(
                   QStringLiteral("relationRef")).toString()
                == secondRef,
        "delete-relation by relationRef did not remove exactly one duplicate");

    auto emptyIdProject = wave::makeDemonstrationProject();
    auto& emptyIdScenario =
        emptyIdProject.scenarios.front();
    emptyIdScenario.relations.front().id.clear();
    const auto emptyIdQuery =
        wave::findRelationsForAutomation(
            emptyIdProject, {}, emptyIdScenario.id);
    const auto emptyIdRow =
        emptyIdQuery.json
            .value(QStringLiteral("relations"))
            .toArray()
            .at(0)
            .toObject();
    const auto emptyIdRef =
        emptyIdRow.value(
            QStringLiteral("relationRef")).toString();
    const auto emptyIdRepair =
        wave::applyAutomationBatch(
            emptyIdProject,
            QJsonObject{
                {QStringLiteral("schema"),
                 QString::fromLatin1(
                     wave::AutomationBatchSchema)},
                {QStringLiteral("scenarioId"),
                 QString::fromStdString(emptyIdScenario.id)},
                {QStringLiteral("operations"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("op"),
                          QStringLiteral("update-relation")},
                         {QStringLiteral("relationRef"), emptyIdRef},
                         {QStringLiteral("newId"),
                          QStringLiteral("relation-recovered")},
                     },
                 }},
            });
    expect(
        emptyIdQuery.ok()
            && emptyIdQuery.json
                   .value(QStringLiteral("addressableCount")).toInt()
                == 0
            && emptyIdQuery.json
                   .value(QStringLiteral("identityIssueCount")).toInt()
                == 1
            && emptyIdRow.value(
                   QStringLiteral("identityIssues")).toArray().contains(
                       QStringLiteral("missing-id"))
            && !emptyIdRef.isEmpty()
            && emptyIdRepair.ok()
            && emptyIdRepair.project->scenarios.front()
                   .relations.front().id
                == "relation-recovered"
            && emptyIdRepair.json
                   .value(QStringLiteral("operations"))
                   .toArray()
                   .at(0)
                   .toObject()
                   .value(QStringLiteral("repairedIdentity"))
                   .toBool(),
        "relationRef did not recover a Relation with an empty stable ID");

    auto commandProject = wave::makeDemonstrationProject();
    auto& commandScenario =
        commandProject.scenarios.front();
    const auto commandBefore = commandScenario;
    const auto expected =
        commandScenario.relations.front();
    auto replacement = expected;
    replacement.id = "relation-indexed";
    replacement.description = "Indexed replacement";
    wave::CommandStack changeStack;
    expect(
        changeStack.execute(
            std::make_unique<
                wave::ChangeRelationAtIndexCommand>(
                    commandScenario,
                    0,
                    expected,
                    replacement))
            && commandScenario.relations.front()
                == replacement
            && changeStack.undo()
            && commandScenario == commandBefore
            && changeStack.redo()
            && commandScenario.relations.front()
                == replacement,
        "indexed Relation change did not preserve Undo/Redo");

    auto removeProject = wave::makeDemonstrationProject();
    auto& removeScenario = removeProject.scenarios.front();
    const auto removeBefore = removeScenario;
    const auto removeExpected =
        removeScenario.relations.front();
    wave::CommandStack removeStack;
    expect(
        removeStack.execute(
            std::make_unique<
                wave::RemoveRelationAtIndexCommand>(
                    removeScenario,
                    0,
                    removeExpected))
            && removeScenario.relations.empty()
            && removeStack.undo()
            && removeScenario == removeBefore
            && removeStack.redo()
            && removeScenario.relations.empty(),
        "indexed Relation removal did not preserve Undo/Redo");
}

void testAutomationStructuralIdentityValidationContracts()
{
    const auto healthyProject =
        wave::makeDemonstrationProject();
    const auto healthy =
        wave::validateProjectForAutomation(
            healthyProject,
            healthyProject.scenarios.front().id);
    const auto healthyIdentity =
        healthy.json
            .value(QStringLiteral("identitySummary"))
            .toObject();
    expect(
        healthy.ok()
            && healthy.json.value(
                   QStringLiteral("valid")).toBool()
            && healthy.json.value(
                   QStringLiteral("identityIssueCount")).toString()
                == QStringLiteral("0")
            && healthyIdentity.value(
                   QStringLiteral("valid")).toBool()
            && healthyIdentity.value(
                   QStringLiteral("issueCount")).toString()
                == QStringLiteral("0"),
        "healthy project did not pass structural identity validation");

    auto broken = wave::makeDemonstrationProject();
    broken.id.clear();
    auto duplicateClock = broken.clockDomains.front();
    duplicateClock.name = "clk_duplicate";
    broken.clockDomains.push_back(duplicateClock);
    auto& scenario = broken.scenarios.front();

    const auto sourceLane = std::find_if(
        scenario.lanes.begin(),
        scenario.lanes.end(),
        [](const wave::Lane& lane) {
            return lane.id == "lane-reset";
        });
    expect(
        sourceLane != scenario.lanes.end()
            && sourceLane->segments.size() >= 2,
        "identity validation fixture has no multi-segment Lane");
    auto duplicateLane = *sourceLane;
    duplicateLane.name = "reset_duplicate";
    for (auto& segment : duplicateLane.segments) {
        segment.id = "segment-duplicate-identity";
    }
    scenario.lanes.push_back(std::move(duplicateLane));

    expect(
        scenario.events.size() >= 2
            && !scenario.relations.empty(),
        "identity validation fixture lacks Event or Relation data");
    scenario.events.at(1).id =
        scenario.events.at(0).id;
    auto duplicateRelation =
        scenario.relations.front();
    duplicateRelation.description =
        "Duplicate Relation identity";
    scenario.relations.push_back(
        std::move(duplicateRelation));
    scenario.markers.push_back({
        {},
        "Missing Marker identity",
        10'000,
        10'000,
        wave::MarkerKind::Point,
        {},
        {},
    });

    wave::Scenario duplicateScenario;
    duplicateScenario.id = scenario.id;
    duplicateScenario.name = "Duplicate Scenario identity";
    duplicateScenario.duration = scenario.duration;
    broken.scenarios.push_back(
        std::move(duplicateScenario));

    const auto validation =
        wave::validateProjectForAutomation(broken);
    const auto identitySummary =
        validation.json
            .value(QStringLiteral("identitySummary"))
            .toObject();
    const auto issues =
        validation.json
            .value(QStringLiteral("issues"))
            .toArray();
    const auto countIssues =
        [&issues](
            const QString& code,
            const QString& objectKind) {
            return std::count_if(
                issues.begin(),
                issues.end(),
                [&code, &objectKind](
                    const QJsonValue& value) {
                    const auto issue = value.toObject();
                    return issue.value(
                               QStringLiteral("code")).toString()
                            == code
                        && issue.value(
                               QStringLiteral("objectKind")).toString()
                            == objectKind;
                });
        };
    const auto relationIssue = std::find_if(
        issues.begin(),
        issues.end(),
        [](const QJsonValue& value) {
            const auto issue = value.toObject();
            return issue.value(
                       QStringLiteral("code")).toString()
                    == QStringLiteral("duplicate-stable-id")
                && issue.value(
                       QStringLiteral("objectKind")).toString()
                    == QStringLiteral("relation");
        });
    expect(
        validation.ok()
            && !validation.json.value(
                    QStringLiteral("valid")).toBool()
            && !validation.json.value(
                    QStringLiteral("ok")).toBool()
            && validation.json.value(
                   QStringLiteral("identityIssueCount")).toString()
                == QStringLiteral("14")
            && identitySummary.value(
                   QStringLiteral("valid")).toBool()
                == false
            && identitySummary.value(
                   QStringLiteral("issueCount")).toString()
                == QStringLiteral("14")
            && identitySummary.value(
                   QStringLiteral("affectedObjectCount")).toString()
                == QStringLiteral("14")
            && identitySummary.value(
                   QStringLiteral("missingStableIdCount")).toString()
                == QStringLiteral("2")
            && identitySummary.value(
                   QStringLiteral("duplicateStableIdCount")).toString()
                == QStringLiteral("12")
            && countIssues(
                   QStringLiteral("missing-stable-id"),
                   QStringLiteral("project"))
                == 1
            && countIssues(
                   QStringLiteral("duplicate-stable-id"),
                   QStringLiteral("clock-domain"))
                == 2
            && countIssues(
                   QStringLiteral("duplicate-stable-id"),
                   QStringLiteral("scenario"))
                == 2
            && countIssues(
                   QStringLiteral("duplicate-stable-id"),
                   QStringLiteral("lane"))
                == 2
            && countIssues(
                   QStringLiteral("duplicate-stable-id"),
                   QStringLiteral("segment"))
                == 2
            && countIssues(
                   QStringLiteral("duplicate-stable-id"),
                   QStringLiteral("event"))
                == 2
            && countIssues(
                   QStringLiteral("duplicate-stable-id"),
                   QStringLiteral("relation"))
                == 2
            && countIssues(
                   QStringLiteral("missing-stable-id"),
                   QStringLiteral("marker"))
                == 1
            && countIssues(
                   QStringLiteral("lane-clock-domain-invalid"),
                   QStringLiteral("lane"))
                == 1
            && countIssues(
                   QStringLiteral(
                       "relation-clock-domain-invalid"),
                   QStringLiteral("relation"))
                == 2
            && relationIssue != issues.end()
            && relationIssue->toObject()
                   .value(QStringLiteral("path")).toString()
                   .startsWith(
                       QStringLiteral(
                           "scenarios[0].relations["))
            && relationIssue->toObject()
                   .value(QStringLiteral("idCount")).toInt()
                == 2
            && relationIssue->toObject().contains(
                QStringLiteral("scenarioIndex"))
            && validation.json
                   .value(QStringLiteral("summary"))
                   .toObject()
                   .value(QStringLiteral("errors"))
                   .toString()
                == QStringLiteral("17"),
        "structural identity validation did not diagnose every ambiguous or missing stable ID: "
            + QJsonDocument(validation.json)
                  .toJson(QJsonDocument::Compact)
                  .toStdString());
}

void testAutomationMarkerIntegrityValidationContracts()
{
    auto healthy = wave::makeDemonstrationProject();
    auto& healthyScenario = healthy.scenarios.front();
    healthyScenario.markers = {
        {
            "marker-window",
            "Window",
            10'000,
            20'000,
            wave::MarkerKind::Note,
            "Valid Marker",
            {},
        },
    };
    const auto healthyValidation =
        wave::validateProjectForAutomation(
            healthy, healthyScenario.id);
    const auto healthyMarkerSummary =
        healthyValidation.json
            .value(QStringLiteral("markerSummary"))
            .toObject();
    const auto healthySemanticIssueCount =
        healthyValidation.json
            .value(QStringLiteral("semanticIssueCount"))
            .toString()
            .toULongLong();
    expect(
        healthyValidation.ok()
            && healthyValidation.json
                   .value(QStringLiteral("valid")).toBool()
            && healthyValidation.json
                   .value(QStringLiteral("markerIssueCount"))
                   .toString()
                == QStringLiteral("0")
            && healthyMarkerSummary.value(
                   QStringLiteral("valid")).toBool()
            && healthyMarkerSummary.value(
                   QStringLiteral("affectedMarkerCount"))
                   .toString()
                == QStringLiteral("0")
            && healthyMarkerSummary.value(
                   QStringLiteral("nameIssueCount"))
                   .toString()
                == QStringLiteral("0")
            && healthyMarkerSummary.value(
                   QStringLiteral("geometryIssueCount"))
                   .toString()
                == QStringLiteral("0"),
        "valid Marker integrity produced validation errors");

    auto broken = healthy;
    auto& brokenScenario = broken.scenarios.front();
    const auto duration = brokenScenario.duration;
    brokenScenario.markers = {
        {
            "marker-window-a",
            "Window",
            10'000,
            20'000,
            wave::MarkerKind::Note,
            {},
            {},
        },
        {
            "marker-window-b",
            " window ",
            30'000,
            40'000,
            wave::MarkerKind::Note,
            {},
            {},
        },
        {
            "marker-empty",
            " ",
            50'000,
            50'000,
            wave::MarkerKind::Point,
            {},
            {},
        },
        {
            "marker-before",
            "Before",
            -1,
            10'000,
            wave::MarkerKind::Note,
            {},
            {},
        },
        {
            "marker-reverse",
            "Reverse",
            70'000,
            60'000,
            wave::MarkerKind::Note,
            {},
            {},
        },
        {
            "marker-after",
            "After",
            duration - 10'000,
            duration + 10'000,
            wave::MarkerKind::Note,
            {},
            {},
        },
        {
            "marker-point",
            "Point",
            90'000,
            100'000,
            wave::MarkerKind::Point,
            {},
            {},
        },
    };
    const auto brokenBefore = broken;
    const auto validation =
        wave::validateProjectForAutomation(
            broken, brokenScenario.id);
    const auto markerSummary =
        validation.json
            .value(QStringLiteral("markerSummary"))
            .toObject();
    const auto issues =
        validation.json
            .value(QStringLiteral("issues"))
            .toArray();
    const auto hasIssue =
        [&issues](
            const QString& code,
            const QString& path) {
            return std::any_of(
                issues.begin(),
                issues.end(),
                [&code, &path](const QJsonValue& value) {
                    const auto issue = value.toObject();
                    return issue.value(
                               QStringLiteral("code")).toString()
                            == code
                        && issue.value(
                               QStringLiteral("objectKind")).toString()
                            == QStringLiteral("marker")
                        && issue.value(
                               QStringLiteral("path")).toString()
                            == path;
                });
        };
    expect(
        validation.ok()
            && !validation.json
                    .value(QStringLiteral("valid")).toBool()
            && validation.json
                   .value(QStringLiteral("identityIssueCount"))
                   .toString()
                == QStringLiteral("0")
            && validation.json
                   .value(QStringLiteral("markerIssueCount"))
                   .toString()
                == QStringLiteral("7")
            && validation.json
                   .value(QStringLiteral("semanticIssueCount"))
                   .toString()
                   .toULongLong()
                == healthySemanticIssueCount + 7
            && validation.json
                   .value(QStringLiteral("summary"))
                   .toObject()
                   .value(QStringLiteral("errors"))
                   .toString()
                == QStringLiteral("7")
            && !markerSummary.value(
                    QStringLiteral("valid")).toBool()
            && markerSummary.value(
                   QStringLiteral("affectedMarkerCount"))
                   .toString()
                == QStringLiteral("7")
            && markerSummary.value(
                   QStringLiteral("nameIssueCount"))
                   .toString()
                == QStringLiteral("3")
            && markerSummary.value(
                   QStringLiteral("geometryIssueCount"))
                   .toString()
                == QStringLiteral("4")
            && hasIssue(
                QStringLiteral("duplicate-name"),
                QStringLiteral(
                    "scenarios[0].markers[0].name"))
            && hasIssue(
                QStringLiteral("duplicate-name"),
                QStringLiteral(
                    "scenarios[0].markers[1].name"))
            && hasIssue(
                QStringLiteral("empty-name"),
                QStringLiteral(
                    "scenarios[0].markers[2].name"))
            && hasIssue(
                QStringLiteral("start-before-scenario"),
                QStringLiteral(
                    "scenarios[0].markers[3].startTick"))
            && hasIssue(
                QStringLiteral("end-before-start"),
                QStringLiteral(
                    "scenarios[0].markers[4].endTick"))
            && hasIssue(
                QStringLiteral("end-after-scenario"),
                QStringLiteral(
                    "scenarios[0].markers[5].endTick"))
            && hasIssue(
                QStringLiteral("point-has-range"),
                QStringLiteral(
                    "scenarios[0].markers[6].kind")),
        "Marker integrity validation did not return complete machine diagnostics");

    const auto query = wave::findMarkersForAutomation(
        broken, {}, brokenScenario.id);
    expect(
        query.ok()
            && query.json.value(
                   QStringLiteral("matchCount")).toInt()
                == 7
            && query.json.value(
                   QStringLiteral("validCount")).toInt()
                == 0
            && query.json.value(
                   QStringLiteral("addressableCount")).toInt()
                == 7
            && query.json.value(
                   QStringLiteral("issueCount")).toInt()
                == 7,
        "Marker query and project validation disagree on integrity issues");

    const QJsonObject unrelatedEdit{
        {QStringLiteral("schema"),
         QString::fromLatin1(wave::AutomationBatchSchema)},
        {QStringLiteral("scenarioId"),
         QString::fromStdString(brokenScenario.id)},
        {QStringLiteral("operations"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral("rename-lane")},
                 {QStringLiteral("laneId"),
                  QString::fromStdString(
                      brokenScenario.lanes.front().id)},
                 {QStringLiteral("name"),
                  QStringLiteral("marker_guard_probe")},
             },
         }},
    };
    const auto rejected =
        wave::applyAutomationBatch(broken, unrelatedEdit);
    const auto rejectedGuard =
        rejected.json
            .value(QStringLiteral("validationGuard"))
            .toObject();
    expect(
        !rejected.ok()
            && !rejected.project
            && rejectedGuard.value(
                   QStringLiteral("reason")).toString()
                == QStringLiteral("invalid-candidate")
            && rejectedGuard.value(
                   QStringLiteral("sourceErrorCount")).toString()
                == QStringLiteral("7")
            && rejectedGuard.value(
                   QStringLiteral("candidateErrorCount")).toString()
                == QStringLiteral("7")
            && rejectedGuard.value(
                   QStringLiteral("newErrorCount")).toString()
                == QStringLiteral("0")
            && broken == brokenBefore,
        "unrelated edit bypassed Marker integrity validation");

    wave::AutomationMarkerQueryOptions duplicateNameQuery;
    duplicateNameQuery.match = "marker-window-b";
    duplicateNameQuery.exact = true;
    const auto duplicateNameResult =
        wave::findMarkersForAutomation(
            broken,
            duplicateNameQuery,
            brokenScenario.id);
    const auto duplicateNameRows =
        duplicateNameResult.json
            .value(QStringLiteral("markers"))
            .toArray();
    expect(
        duplicateNameResult.ok()
            && duplicateNameRows.size() == 1,
        "duplicate-name Marker could not be queried for repair");
    const auto markerRef =
        duplicateNameRows.at(0)
            .toObject()
            .value(QStringLiteral("markerRef"))
            .toString();
    const auto repaired =
        wave::applyAutomationBatch(
            broken,
            QJsonObject{
                {QStringLiteral("schema"),
                 QString::fromLatin1(
                     wave::AutomationBatchSchema)},
                {QStringLiteral("scenarioId"),
                 QString::fromStdString(
                     brokenScenario.id)},
                {QStringLiteral("operations"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("op"),
                          QStringLiteral("update-marker")},
                         {QStringLiteral("markerRef"),
                          markerRef},
                         {QStringLiteral("name"),
                          QStringLiteral("Other window")},
                     },
                 }},
            });
    const auto repairGuard =
        repaired.json
            .value(QStringLiteral("validationGuard"))
            .toObject();
    const auto repairValidation =
        repaired.json
            .value(QStringLiteral("validation"))
            .toObject();
    expect(
        repaired.ok()
            && repaired.project
            && repairGuard.value(
                   QStringLiteral("reason")).toString()
                == QStringLiteral("progressive-repair")
            && !repairGuard.value(
                    QStringLiteral("repairOperationReported")).toBool()
            && repairGuard.value(
                   QStringLiteral("sourceErrorCount")).toString()
                == QStringLiteral("7")
            && repairGuard.value(
                   QStringLiteral("candidateErrorCount")).toString()
                == QStringLiteral("5")
            && repairGuard.value(
                   QStringLiteral("newErrorCount")).toString()
                == QStringLiteral("0")
            && repairValidation.value(
                   QStringLiteral("markerIssueCount"))
                   .toString()
                == QStringLiteral("5")
            && broken == brokenBefore,
        "Marker name repair was not accepted as non-regressive progress");
}

void testAutomationActionableValidationReferenceContracts()
{
    const auto healthy =
        wave::makeDemonstrationProject();
    const auto healthyValidation =
        wave::validateProjectForAutomation(healthy);
    expect(
        healthyValidation.ok()
            && healthyValidation.json
                   .value(QStringLiteral("repairableIssueCount"))
                   .toString()
                == QStringLiteral("0")
            && healthyValidation.json
                   .value(QStringLiteral("repairTargetCount"))
                   .toString()
                == QStringLiteral("0"),
        "healthy validation exposed spurious repair targets");

    auto broken = healthy;
    auto& scenario = broken.scenarios.front();
    expect(
        !scenario.relations.empty(),
        "actionable validation fixture has no Relation");
    auto duplicateRelation = scenario.relations.front();
    duplicateRelation.description =
        "Imported duplicate Relation";
    scenario.relations.push_back(
        std::move(duplicateRelation));
    scenario.markers = {
        {
            "marker-actionable",
            "Imported point",
            -1,
            scenario.duration + 10'000,
            wave::MarkerKind::Point,
            "Imported invalid Marker",
            {},
        },
    };
    const auto brokenBefore = broken;
    const auto validation =
        wave::validateProjectForAutomation(broken);
    const auto issues =
        validation.json
            .value(QStringLiteral("issues"))
            .toArray();
    const auto issueAtPath =
        [&issues](const QString& path) {
            return std::find_if(
                issues.begin(),
                issues.end(),
                [&path](const QJsonValue& value) {
                    return value.toObject()
                               .value(QStringLiteral("path"))
                               .toString()
                        == path;
                });
        };
    const auto firstRelationIssue =
        issueAtPath(QStringLiteral(
            "scenarios[0].relations[0].id"));
    const auto secondRelationIssue =
        issueAtPath(QStringLiteral(
            "scenarios[0].relations[1].id"));
    const auto markerIssue =
        issueAtPath(QStringLiteral(
            "scenarios[0].markers[0].startTick"));
    const auto satisfiedIssue =
        std::find_if(
            issues.begin(),
            issues.end(),
            [](const QJsonValue& value) {
                return value.toObject()
                           .value(QStringLiteral("code"))
                           .toString()
                    == QStringLiteral("relation-satisfied");
            });
    expect(
        validation.ok()
            && !validation.json
                    .value(QStringLiteral("valid")).toBool()
            && validation.json
                   .value(QStringLiteral("identityIssueCount"))
                   .toString()
                == QStringLiteral("2")
            && validation.json
                   .value(QStringLiteral("markerIssueCount"))
                   .toString()
                == QStringLiteral("3")
            && validation.json
                   .value(QStringLiteral("repairableIssueCount"))
                   .toString()
                == QStringLiteral("5")
            && validation.json
                   .value(QStringLiteral("repairTargetCount"))
                   .toString()
                == QStringLiteral("3")
            && firstRelationIssue != issues.end()
            && secondRelationIssue != issues.end()
            && markerIssue != issues.end()
            && satisfiedIssue != issues.end(),
        "validate did not expose the expected actionable damage");

    const auto firstRelationObject =
        firstRelationIssue->toObject();
    const auto secondRelationObject =
        secondRelationIssue->toObject();
    const auto markerObject =
        markerIssue->toObject();
    const auto firstRelationRef =
        firstRelationObject
            .value(QStringLiteral("relationRef"))
            .toString();
    const auto secondRelationRef =
        secondRelationObject
            .value(QStringLiteral("relationRef"))
            .toString();
    const auto markerRef =
        markerObject
            .value(QStringLiteral("markerRef"))
            .toString();
    const QJsonArray expectedRelationOperations{
        QStringLiteral("update-relation"),
        QStringLiteral("delete-relation"),
    };
    const QJsonArray expectedMarkerOperations{
        QStringLiteral("update-marker"),
        QStringLiteral("delete-marker"),
    };
    expect(
        firstRelationRef.startsWith(
            QStringLiteral("relation-ref-v1:"))
            && secondRelationRef.startsWith(
                QStringLiteral("relation-ref-v1:"))
            && firstRelationRef != secondRelationRef
            && markerRef.startsWith(
                QStringLiteral("marker-ref-v1:"))
            && firstRelationObject.value(
                   QStringLiteral("repairOperations"))
                   .toArray()
                == expectedRelationOperations
            && markerObject.value(
                   QStringLiteral("repairOperations"))
                   .toArray()
                == expectedMarkerOperations
            && !satisfiedIssue->toObject().contains(
                QStringLiteral("relationRef"))
            && !satisfiedIssue->toObject().contains(
                QStringLiteral("repairOperations")),
        "actionable validation references are ambiguous or over-reported");

    for (const auto& value : issues) {
        const auto issue = value.toObject();
        if (issue.value(
                QStringLiteral("objectKind")).toString()
            != QStringLiteral("marker")) {
            continue;
        }
        expect(
            issue.value(
                       QStringLiteral("markerRef")).toString()
                == markerRef,
            "one damaged Marker produced inconsistent repair references");
    }

    const auto repaired =
        wave::applyAutomationBatch(
            broken,
            QJsonObject{
                {QStringLiteral("schema"),
                 QString::fromLatin1(
                     wave::AutomationBatchSchema)},
                {QStringLiteral("scenarioId"),
                 QString::fromStdString(scenario.id)},
                {QStringLiteral("operations"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("op"),
                          QStringLiteral(
                              "update-relation")},
                         {QStringLiteral("relationRef"),
                          secondRelationRef},
                         {QStringLiteral("newId"),
                          QStringLiteral(
                              "relation-actionable-recovered")},
                     },
                     QJsonObject{
                         {QStringLiteral("op"),
                          QStringLiteral("update-marker")},
                         {QStringLiteral("markerRef"),
                          markerRef},
                         {QStringLiteral("at"),
                          QStringLiteral("120 ns")},
                     },
                 }},
            });
    const auto repairValidation =
        repaired.json
            .value(QStringLiteral("validation"))
            .toObject();
    const auto repairGuard =
        repaired.json
            .value(QStringLiteral("validationGuard"))
            .toObject();
    expect(
        repaired.ok()
            && repaired.project
            && repairValidation.value(
                   QStringLiteral("valid")).toBool()
            && repairValidation.value(
                   QStringLiteral("repairableIssueCount"))
                   .toString()
                == QStringLiteral("0")
            && repairValidation.value(
                   QStringLiteral("repairTargetCount"))
                   .toString()
                == QStringLiteral("0")
            && repairGuard.value(
                   QStringLiteral("reason")).toString()
                == QStringLiteral("valid-candidate")
            && repairGuard.value(
                   QStringLiteral("sourceErrorCount")).toString()
                == QStringLiteral("5")
            && repairGuard.value(
                   QStringLiteral("candidateErrorCount")).toString()
                == QStringLiteral("0")
            && broken == brokenBefore,
        "validate references could not directly repair their source snapshot");

    auto endpointBroken = healthy;
    auto& endpointScenario =
        endpointBroken.scenarios.front();
    endpointScenario.relations.front().targetEventId =
        "missing-actionable-target";
    const auto endpointBefore = endpointBroken;
    const auto endpointValidation =
        wave::validateProjectForAutomation(endpointBroken);
    const auto endpointIssues =
        endpointValidation.json
            .value(QStringLiteral("issues"))
            .toArray();
    const auto endpointIssue =
        std::find_if(
            endpointIssues.begin(),
            endpointIssues.end(),
            [](const QJsonValue& value) {
                return value.toObject()
                           .value(QStringLiteral("code"))
                           .toString()
                    == QStringLiteral("missing-target-event");
            });
    expect(
        endpointIssue != endpointIssues.end()
            && endpointValidation.json
                   .value(QStringLiteral("repairableIssueCount"))
                   .toString()
                == QStringLiteral("1")
            && endpointValidation.json
                   .value(QStringLiteral("repairTargetCount"))
                   .toString()
                == QStringLiteral("1")
            && endpointIssue->toObject()
                   .value(QStringLiteral("relationRef"))
                   .toString()
                   .startsWith(
                       QStringLiteral("relation-ref-v1:"))
            && endpointIssue->toObject()
                   .value(QStringLiteral("repairOperations"))
                   .toArray()
                == expectedRelationOperations,
        "Relation endpoint validation did not expose a direct repair reference");
    const auto endpointRepaired =
        wave::applyAutomationBatch(
            endpointBroken,
            QJsonObject{
                {QStringLiteral("schema"),
                 QString::fromLatin1(
                     wave::AutomationBatchSchema)},
                {QStringLiteral("scenarioId"),
                 QString::fromStdString(
                     endpointScenario.id)},
                {QStringLiteral("operations"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("op"),
                          QStringLiteral(
                              "update-relation")},
                         {QStringLiteral("relationRef"),
                          endpointIssue->toObject()
                              .value(QStringLiteral(
                                  "relationRef"))},
                         {QStringLiteral("targetLaneId"),
                          QStringLiteral("lane-ack")},
                         {QStringLiteral("targetAt"),
                          QStringLiteral("110 ns")},
                     },
                 }},
            });
    expect(
        endpointRepaired.ok()
            && endpointRepaired.project
            && endpointRepaired.json
                   .value(QStringLiteral("validation"))
                   .toObject()
                   .value(QStringLiteral("valid"))
                   .toBool()
            && endpointBroken == endpointBefore,
        "semantic Relation validation reference could not repair its endpoint");
}

void testAutomationStructuredRelationValidationContracts()
{
    const auto healthy =
        wave::makeDemonstrationProject();
    const auto issueByCode =
        [](const wave::AutomationDocument& validation,
           const QString& code) {
            const auto issues =
                validation.json
                    .value(QStringLiteral("issues"))
                    .toArray();
            const auto iterator =
                std::find_if(
                    issues.begin(),
                    issues.end(),
                    [&code](const QJsonValue& value) {
                        return value.toObject()
                                   .value(QStringLiteral("code"))
                                   .toString()
                            == code;
                    });
            return iterator == issues.end()
                ? QJsonObject{}
                : iterator->toObject();
        };

    auto missingTarget = healthy;
    auto& missingScenario =
        missingTarget.scenarios.front();
    missingScenario.relations.front().targetEventId =
        "missing-structured-target";
    const auto missingValidation =
        wave::validateProjectForAutomation(missingTarget);
    const auto missingIssue =
        issueByCode(
            missingValidation,
            QStringLiteral("missing-target-event"));
    const auto missingContext =
        missingIssue
            .value(QStringLiteral("relationContext"))
            .toObject();
    const auto missingSource =
        missingContext
            .value(QStringLiteral("source"))
            .toObject();
    const auto missingTargetContext =
        missingContext
            .value(QStringLiteral("target"))
            .toObject();
    wave::AutomationRelationQueryOptions queryOptions;
    queryOptions.match = "relation-req-ack";
    queryOptions.exact = true;
    const auto relationQuery =
        wave::findRelationsForAutomation(
            missingTarget,
            queryOptions,
            missingScenario.id);
    const auto queryRelation =
        relationQuery.json
            .value(QStringLiteral("relations"))
            .toArray()
            .at(0)
            .toObject();
    const auto encodedMissingContext =
        QJsonDocument(missingContext)
            .toJson(QJsonDocument::Compact);
    expect(
        !missingIssue.isEmpty()
            && missingIssue.value(
                   QStringLiteral("objectKind")).toString()
                == QStringLiteral("relation")
            && missingIssue.value(
                   QStringLiteral("scenarioIndex")).toInt()
                == 0
            && missingIssue.value(
                   QStringLiteral("objectIndex")).toInt()
                == 0
            && missingIssue.value(
                   QStringLiteral("path")).toString()
                == QStringLiteral(
                    "scenarios[0].relations[0].targetEventId")
            && missingIssue.value(
                   QStringLiteral("paths")).toArray()
                == QJsonArray{
                    QStringLiteral(
                        "scenarios[0].relations[0].targetEventId"),
                }
            && missingIssue.value(
                   QStringLiteral("repairProperties")).toArray()
                == QJsonArray{
                    QStringLiteral("target-endpoint"),
                }
            && missingSource.value(
                   QStringLiteral("relationEndpoint")).toBool()
            && missingSource.value(
                   QStringLiteral("laneId")).toString()
                == QStringLiteral("lane-request")
            && missingSource.value(
                   QStringLiteral("timeTick")).toString()
                == QStringLiteral("80000")
            && !missingTargetContext.value(
                    QStringLiteral("relationEndpoint")).toBool()
            && missingTargetContext.value(
                   QStringLiteral("issue")).toString()
                == QStringLiteral("missing-event")
            && missingContext.value(
                   QStringLiteral("minimumDelayTick")).toString()
                == QStringLiteral("10000")
            && missingContext.value(
                   QStringLiteral("maximumDelayTick")).toString()
                == QStringLiteral("40000")
            && missingContext.value(
                   QStringLiteral("observedDelayTick")).isNull()
            && !missingContext.value(
                    QStringLiteral("endpointsReady")).toBool()
            && missingContext == queryRelation
            && !encodedMissingContext.contains("sourceEventId")
            && !encodedMissingContext.contains("targetEventId")
            && !encodedMissingContext.contains("extensions"),
        "missing Relation target did not expose a compact structured context");

    auto violated = healthy;
    violated.scenarios.front()
        .relations.front().maximumDelay = 20'000;
    const auto violatedIssue =
        issueByCode(
            wave::validateProjectForAutomation(violated),
            QStringLiteral("relation-violated"));
    const auto violatedContext =
        violatedIssue
            .value(QStringLiteral("relationContext"))
            .toObject();
    expect(
        violatedIssue.value(
                   QStringLiteral("path")).toString()
                == QStringLiteral(
                    "scenarios[0].relations[0].minimumDelayTick")
            && violatedIssue.value(
                   QStringLiteral("paths")).toArray()
                == QJsonArray{
                    QStringLiteral(
                        "scenarios[0].relations[0].minimumDelayTick"),
                    QStringLiteral(
                        "scenarios[0].relations[0].maximumDelayTick"),
                }
            && violatedIssue.value(
                   QStringLiteral("repairProperties")).toArray()
                == QJsonArray{
                    QStringLiteral("delay-range"),
                }
            && violatedContext.value(
                   QStringLiteral("observedDelayTick")).toString()
                == QStringLiteral("30000")
            && !violatedContext.value(
                    QStringLiteral("timingWithinRange")).toBool()
            && violatedContext.value(
                   QStringLiteral("endpointsReady")).toBool(),
        "Relation timing violation did not identify its delay range");

    auto invalidCondition = healthy;
    invalidCondition.scenarios.front()
        .relations.front().condition = "req ==";
    const auto conditionIssue =
        issueByCode(
            wave::validateProjectForAutomation(
                invalidCondition),
            QStringLiteral("invalid-relation-condition"));
    expect(
        conditionIssue.value(
                   QStringLiteral("path")).toString()
                == QStringLiteral(
                    "scenarios[0].relations[0].condition")
            && conditionIssue.value(
                   QStringLiteral("repairProperties")).toArray()
                == QJsonArray{
                    QStringLiteral("condition"),
                }
            && conditionIssue.value(
                   QStringLiteral("relationContext"))
                   .toObject()
                   .value(QStringLiteral("condition"))
                   .toString()
                == QStringLiteral("req =="),
        "invalid Relation condition did not identify its property");

    auto clockMismatch = healthy;
    clockMismatch.scenarios.front()
        .relations.front().clockDomainId =
        "clock-missing";
    const auto requestLane =
        std::find_if(
            clockMismatch.scenarios.front().lanes.begin(),
            clockMismatch.scenarios.front().lanes.end(),
            [](const wave::Lane& lane) {
                return lane.id == "lane-request";
            });
    expect(
        requestLane
            != clockMismatch.scenarios.front().lanes.end(),
        "structured Relation fixture has no request Lane");
    requestLane->clockDomainId = "clock-main";
    const auto clockIssue =
        issueByCode(
            wave::validateProjectForAutomation(
                clockMismatch),
            QStringLiteral("clock-domain-mismatch"));
    expect(
        clockIssue.value(
                   QStringLiteral("path")).toString()
                == QStringLiteral(
                    "scenarios[0].relations[0].clockDomainId")
            && clockIssue.value(
                   QStringLiteral("repairProperties")).toArray()
                == QJsonArray{
                    QStringLiteral("clock"),
                }
            && clockIssue.value(
                   QStringLiteral("relationContext"))
                   .toObject()
                   .value(QStringLiteral("clockDomainId"))
                   .toString()
                == QStringLiteral("clock-missing"),
        QStringLiteral(
            "Relation clock mismatch did not identify its clock property: %1")
            .arg(QString::fromUtf8(
                QJsonDocument(clockIssue)
                    .toJson(QJsonDocument::Compact)))
            .toStdString());

    auto insufficient = healthy;
    insufficient.scenarios.front()
        .relations.front().maximumDelay = 150'000;
    const auto insufficientValidation =
        wave::validateProjectForAutomation(insufficient);
    const auto insufficientIssue =
        issueByCode(
            insufficientValidation,
            QStringLiteral("insufficient-time-range"));
    expect(
        insufficientValidation.json
            .value(QStringLiteral("valid")).toBool()
            && insufficientIssue.value(
                   QStringLiteral("path")).toString()
                == QStringLiteral(
                    "scenarios[0].durationTick")
            && insufficientIssue.value(
                   QStringLiteral("paths")).toArray()
                == QJsonArray{
                    QStringLiteral(
                        "scenarios[0].durationTick"),
                    QStringLiteral(
                        "scenarios[0].relations[0].maximumDelayTick"),
                }
            && insufficientIssue.value(
                   QStringLiteral("repairProperties")).toArray()
                == QJsonArray{
                    QStringLiteral("scenario-duration"),
                    QStringLiteral("delay-range"),
                }
            && insufficientValidation.json
                   .value(QStringLiteral("repairableIssueCount"))
                   .toString()
                == QStringLiteral("1"),
        "insufficient Relation observation window did not expose both repair paths");
}

void testAutomationStructuredWaveformValidationContracts()
{
    const auto issueByCode =
        [](const wave::AutomationDocument& validation,
           const QString& code) {
            const auto issues =
                validation.json
                    .value(QStringLiteral("issues"))
                    .toArray();
            const auto iterator =
                std::find_if(
                    issues.begin(),
                    issues.end(),
                    [&code](const QJsonValue& value) {
                        return value.toObject()
                                   .value(QStringLiteral("code"))
                                   .toString()
                            == code;
                    });
            return iterator == issues.end()
                ? QJsonObject{}
                : iterator->toObject();
        };

    auto invalidBus =
        wave::makeDemonstrationProject();
    auto* data =
        wave::findLane(
            invalidBus.scenarios.front(),
            "lane-data");
    expect(
        data && data->segments.size() == 3,
        "structured waveform fixture has no data Segments");
    data->segments.at(1).value = "0x1ff";
    const auto invalidBusBefore = invalidBus;
    const auto invalidValidation =
        wave::validateProjectForAutomation(invalidBus);
    const auto invalidIssue =
        issueByCode(
            invalidValidation,
            QStringLiteral("invalid-bus-value"));
    const auto invalidContext =
        invalidIssue
            .value(QStringLiteral("waveformContext"))
            .toObject();
    const auto invalidRange =
        invalidIssue
            .value(QStringLiteral("repairRange"))
            .toObject();
    const auto encodedInvalidContext =
        QJsonDocument(invalidContext)
            .toJson(QJsonDocument::Compact);
    expect(
        !invalidValidation.json
             .value(QStringLiteral("valid")).toBool()
            && invalidValidation.json
                   .value(QStringLiteral("repairableIssueCount"))
                   .toString()
                == QStringLiteral("1")
            && invalidValidation.json
                   .value(QStringLiteral("repairTargetCount"))
                   .toString()
                == QStringLiteral("1")
            && invalidIssue.value(
                   QStringLiteral("objectKind")).toString()
                == QStringLiteral("segment")
            && invalidIssue.value(
                   QStringLiteral("scenarioIndex")).toInt()
                == 0
            && invalidIssue.value(
                   QStringLiteral("laneIndex")).toInt()
                == 5
            && invalidIssue.value(
                   QStringLiteral("objectIndex")).toInt()
                == 1
            && invalidIssue.value(
                   QStringLiteral("path")).toString()
                == QStringLiteral(
                    "scenarios[0].lanes[5].segments[1].value")
            && invalidIssue.value(
                   QStringLiteral("paths")).toArray()
                == QJsonArray{
                    QStringLiteral(
                        "scenarios[0].lanes[5].segments[1].value"),
                }
            && invalidIssue.value(
                   QStringLiteral("repairProperties")).toArray()
                == QJsonArray{
                    QStringLiteral("value"),
                }
            && invalidIssue.value(
                   QStringLiteral("repairOperations")).toArray()
                == QJsonArray{
                    QStringLiteral("set-range"),
                    QStringLiteral("clear-range"),
                }
            && invalidRange.value(
                   QStringLiteral("laneId")).toString()
                == QStringLiteral("lane-data")
            && invalidRange.value(
                   QStringLiteral("startTick")).toString()
                == QStringLiteral("80000")
            && invalidRange.value(
                   QStringLiteral("endTick")).toString()
                == QStringLiteral("150000")
            && invalidContext.value(
                   QStringLiteral("laneName")).toString()
                == QStringLiteral("data[7:0]")
            && invalidContext.value(
                   QStringLiteral("kind")).toString()
                == QStringLiteral("bus")
            && invalidContext.value(
                   QStringLiteral("width")).toInt()
                == 8
            && invalidContext.value(
                   QStringLiteral("radix")).toString()
                == QStringLiteral("hexadecimal")
            && invalidContext.value(
                   QStringLiteral("segmentId")).toString()
                == QStringLiteral("segment-data-payload")
            && invalidContext.value(
                   QStringLiteral("value")).toString()
                == QStringLiteral("0x1ff")
            && !invalidContext.value(
                    QStringLiteral("undefined")).toBool()
            && !encodedInvalidContext.contains("segments")
            && !encodedInvalidContext.contains("enumMap")
            && !encodedInvalidContext.contains("extensions"),
        "invalid Bus Segment did not expose a compact actionable range");

    const auto repairedBus =
        wave::applyAutomationBatch(
            invalidBus,
            QJsonObject{
                {QStringLiteral("schema"),
                 QString::fromLatin1(
                     wave::AutomationBatchSchema)},
                {QStringLiteral("scenarioId"),
                 QStringLiteral("scenario-handshake")},
                {QStringLiteral("operations"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("op"),
                          QStringLiteral("set-range")},
                         {QStringLiteral("laneId"),
                          invalidRange.value(
                              QStringLiteral("laneId"))},
                         {QStringLiteral("startTick"),
                          invalidRange.value(
                              QStringLiteral("startTick"))},
                         {QStringLiteral("endTick"),
                          invalidRange.value(
                              QStringLiteral("endTick"))},
                         {QStringLiteral("value"),
                          QStringLiteral("0x35")},
                     },
                 }},
            });
    expect(
        repairedBus.ok()
            && repairedBus.project
            && wave::validateProjectForAutomation(
                   *repairedBus.project)
                   .json
                   .value(QStringLiteral("valid"))
                   .toBool()
            && repairedBus.json
                   .value(QStringLiteral("validationGuard"))
                   .toObject()
                   .value(QStringLiteral("sourceErrorCount"))
                   .toString()
                == QStringLiteral("1")
            && repairedBus.json
                   .value(QStringLiteral("validationGuard"))
                   .toObject()
                   .value(QStringLiteral("candidateErrorCount"))
                   .toString()
                == QStringLiteral("0")
            && invalidBus == invalidBusBefore,
        "invalid Bus Segment could not be repaired from its validation range");

    auto invalidEnum =
        wave::makeDemonstrationProject();
    auto* state =
        wave::findLane(
            invalidEnum.scenarios.front(),
            "lane-state");
    expect(
        state && state->segments.size() == 3,
        "structured waveform fixture has no Enum Segments");
    state->segments.at(1).value = "BROKEN";
    const auto enumIssue =
        issueByCode(
            wave::validateProjectForAutomation(
                invalidEnum),
            QStringLiteral("invalid-bus-value"));
    const auto enumContext =
        enumIssue
            .value(QStringLiteral("waveformContext"))
            .toObject();
    expect(
        enumIssue.value(
                   QStringLiteral("path")).toString()
                == QStringLiteral(
                    "scenarios[0].lanes[6].segments[1].value")
            && enumContext.value(
                   QStringLiteral("kind")).toString()
                == QStringLiteral("enum")
            && enumContext.value(
                   QStringLiteral("width")).toInt()
                == 2
            && enumContext.value(
                   QStringLiteral("value")).toString()
                == QStringLiteral("BROKEN"),
        "invalid Enum Segment did not expose its exact value path");

    auto undefined =
        wave::makeDemonstrationProject();
    auto* request =
        wave::findLane(
            undefined.scenarios.front(),
            "lane-request");
    expect(
        request,
        "structured waveform fixture has no request Lane");
    wave::clearSegmentRange(
        *request, 10'000, 20'000);
    const auto undefinedBefore = undefined;
    const auto undefinedValidation =
        wave::validateProjectForAutomation(undefined);
    const auto undefinedIssue =
        issueByCode(
            undefinedValidation,
            QStringLiteral("undefined-region"));
    const auto undefinedContext =
        undefinedIssue
            .value(QStringLiteral("waveformContext"))
            .toObject();
    const auto undefinedRange =
        undefinedIssue
            .value(QStringLiteral("repairRange"))
            .toObject();
    expect(
        undefinedValidation.json
            .value(QStringLiteral("valid")).toBool()
            && undefinedIssue.value(
                   QStringLiteral("objectKind")).toString()
                == QStringLiteral("lane")
            && undefinedIssue.value(
                   QStringLiteral("laneIndex")).toInt()
                == 3
            && undefinedIssue.value(
                   QStringLiteral("objectIndex")).toInt()
                == 3
            && undefinedIssue.value(
                   QStringLiteral("path")).toString()
                == QStringLiteral(
                    "scenarios[0].lanes[3].segments")
            && undefinedIssue.value(
                   QStringLiteral("repairProperties")).toArray()
                == QJsonArray{
                    QStringLiteral("waveform-range"),
                }
            && undefinedIssue.value(
                   QStringLiteral("repairOperations")).toArray()
                == QJsonArray{
                    QStringLiteral("set-range"),
                }
            && undefinedRange.value(
                   QStringLiteral("laneId")).toString()
                == QStringLiteral("lane-request")
            && undefinedRange.value(
                   QStringLiteral("startTick")).toString()
                == QStringLiteral("10000")
            && undefinedRange.value(
                   QStringLiteral("endTick")).toString()
                == QStringLiteral("20000")
            && undefinedContext.value(
                   QStringLiteral("laneName")).toString()
                == QStringLiteral("req")
            && undefinedContext.value(
                   QStringLiteral("kind")).toString()
                == QStringLiteral("bit")
            && undefinedContext.value(
                   QStringLiteral("undefined")).toBool()
            && undefinedValidation.json
                   .value(QStringLiteral("repairableIssueCount"))
                   .toString()
                == QStringLiteral("1")
            && undefinedValidation.json
                   .value(QStringLiteral("repairTargetCount"))
                   .toString()
                == QStringLiteral("1"),
        "undefined waveform gap did not expose its exact repair range");

    const auto filledGap =
        wave::applyAutomationBatch(
            undefined,
            QJsonObject{
                {QStringLiteral("schema"),
                 QString::fromLatin1(
                     wave::AutomationBatchSchema)},
                {QStringLiteral("scenarioId"),
                 QStringLiteral("scenario-handshake")},
                {QStringLiteral("operations"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("op"),
                          QStringLiteral("set-range")},
                         {QStringLiteral("laneId"),
                          undefinedRange.value(
                              QStringLiteral("laneId"))},
                         {QStringLiteral("startTick"),
                          undefinedRange.value(
                              QStringLiteral("startTick"))},
                         {QStringLiteral("endTick"),
                          undefinedRange.value(
                              QStringLiteral("endTick"))},
                         {QStringLiteral("value"),
                          QStringLiteral("0")},
                     },
                 }},
            });
    expect(
        filledGap.ok()
            && filledGap.project
            && issueByCode(
                   wave::validateProjectForAutomation(
                       *filledGap.project),
                   QStringLiteral("undefined-region"))
                   .isEmpty()
            && undefined == undefinedBefore,
        "undefined waveform gap could not be filled from its validation range");
}

void testAutomationStructuredEventValidationContracts()
{
    const auto issueByCode =
        [](const wave::AutomationDocument& validation,
           const QString& code) {
            const auto issues =
                validation.json
                    .value(QStringLiteral("issues"))
                    .toArray();
            const auto iterator =
                std::find_if(
                    issues.begin(),
                    issues.end(),
                    [&code](const QJsonValue& value) {
                        return value.toObject()
                                   .value(QStringLiteral("code"))
                                   .toString()
                            == code;
                    });
            return iterator == issues.end()
                ? QJsonObject{}
                : iterator->toObject();
        };

    auto missingLane =
        wave::makeDemonstrationProject();
    auto& missingScenario =
        missingLane.scenarios.front();
    const auto missingEventIterator =
        std::find_if(
            missingScenario.events.begin(),
            missingScenario.events.end(),
            [](const wave::Event& event) {
                return event.linkedSegmentId
                    == "segment-ack-high";
            });
    expect(
        missingEventIterator
            != missingScenario.events.end(),
        "structured Event fixture has no ack Event");
    auto* missingEvent = &*missingEventIterator;
    const auto missingEventId =
        missingEvent->id;
    const auto missingEventIndex =
        static_cast<std::size_t>(
            missingEvent
            - missingScenario.events.data());
    missingEvent->laneId = "lane-missing";
    const auto missingBefore = missingLane;
    const auto missingValidation =
        wave::validateProjectForAutomation(
            missingLane);
    const auto missingIssue =
        issueByCode(
            missingValidation,
            QStringLiteral("missing-lane"));
    const auto missingContext =
        missingIssue
            .value(QStringLiteral("eventContext"))
            .toObject();
    const auto expectedMissingPath =
        QStringLiteral(
            "scenarios[0].events[%1].laneId")
            .arg(
                static_cast<qulonglong>(
                    missingEventIndex));
    const auto encodedMissingContext =
        QJsonDocument(missingContext)
            .toJson(QJsonDocument::Compact);
    expect(
        !missingValidation.json
             .value(QStringLiteral("valid")).toBool()
            && missingValidation.json
                   .value(QStringLiteral("repairableIssueCount"))
                   .toString()
                == QStringLiteral("1")
            && missingValidation.json
                   .value(QStringLiteral("repairTargetCount"))
                   .toString()
                == QStringLiteral("1")
            && missingIssue.value(
                   QStringLiteral("objectKind")).toString()
                == QStringLiteral("event")
            && missingIssue.value(
                   QStringLiteral("scenarioIndex")).toInt()
                == 0
            && missingIssue.value(
                   QStringLiteral("objectIndex")).toInt()
                == static_cast<int>(missingEventIndex)
            && missingIssue.value(
                   QStringLiteral("path")).toString()
                == expectedMissingPath
            && missingIssue.value(
                   QStringLiteral("paths")).toArray()
                == QJsonArray{expectedMissingPath}
            && missingIssue.value(
                   QStringLiteral("repairProperties")).toArray()
                == QJsonArray{
                    QStringLiteral("event-link"),
                    QStringLiteral("event"),
                }
            && missingIssue.value(
                   QStringLiteral("repairOperations")).toArray()
                == QJsonArray{
                    QStringLiteral("repair-event-link"),
                    QStringLiteral("delete-event"),
                }
            && missingContext.value(
                   QStringLiteral("eventId")).toString()
                == QString::fromStdString(
                    missingEventId)
            && missingContext.value(
                   QStringLiteral("idCount")).toInt()
                == 1
            && missingContext.value(
                   QStringLiteral("addressable")).toBool()
            && missingContext.value(
                   QStringLiteral("laneId")).toString()
                == QStringLiteral("lane-missing")
            && !missingContext.value(
                    QStringLiteral("laneResolved")).toBool()
            && missingContext.value(
                   QStringLiteral("timeTick")).toString()
                == QStringLiteral("110000")
            && missingContext.value(
                   QStringLiteral("time")).toString()
                == QStringLiteral("110 ns")
            && missingContext.value(
                   QStringLiteral("withinScenario")).toBool()
            && missingContext.value(
                   QStringLiteral("action")).toString()
                == QStringLiteral("drive")
            && missingContext.value(
                   QStringLiteral("value")).toString()
                == QStringLiteral("1")
            && missingContext.value(
                   QStringLiteral("waveformLinked")).toBool()
            && !missingContext.value(
                    QStringLiteral("linkedSegmentResolved")).toBool()
            && missingContext.value(
                   QStringLiteral("linkedSegmentOwnerResolved")).toBool()
            && missingContext.value(
                   QStringLiteral("linkedLaneId")).toString()
                == QStringLiteral("lane-ack")
            && missingContext.value(
                   QStringLiteral("linkedSegmentStartTick")).toString()
                == QStringLiteral("110000")
            && missingContext.value(
                   QStringLiteral("linkedSegmentValue")).toString()
                == QStringLiteral("1")
            && !missingContext.value(
                    QStringLiteral("linkConsistent")).toBool()
            && missingContext.value(
                   QStringLiteral("linkRepairable")).toBool()
            && !encodedMissingContext.contains(
                "linkedSegmentId")
            && !encodedMissingContext.contains(
                "extensions"),
        "missing Event Lane did not expose a compact actionable context");

    const auto removedEvent =
        wave::applyAutomationBatch(
            missingLane,
            QJsonObject{
                {QStringLiteral("schema"),
                 QString::fromLatin1(
                     wave::AutomationBatchSchema)},
                {QStringLiteral("scenarioId"),
                 QStringLiteral("scenario-handshake")},
                {QStringLiteral("operations"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("op"),
                          QStringLiteral("delete-event")},
                         {QStringLiteral("eventId"),
                          missingIssue.value(
                              QStringLiteral("eventId"))},
                     },
                 }},
            });
    const auto removedReport =
        removedEvent.json
            .value(QStringLiteral("operations"))
            .toArray()
            .at(0)
            .toObject();
    expect(
        removedEvent.ok()
            && removedEvent.project
            && removedReport.value(
                   QStringLiteral("changed")).toBool()
            && removedReport.value(
                   QStringLiteral("removedEventCount")).toInt()
                == 1
            && removedReport.value(
                   QStringLiteral("removedRelationCount")).toInt()
                == 1
            && removedReport.value(
                   QStringLiteral("removedSegmentCount")).toInt()
                == 0
            && !wave::findEvent(
                removedEvent.project
                    ->scenarios.front(),
                missingEventId)
            && removedEvent.project
                   ->scenarios.front()
                   .relations.empty()
            && wave::validateProjectForAutomation(
                   *removedEvent.project)
                   .json
                   .value(QStringLiteral("valid"))
                   .toBool()
            && missingLane == missingBefore,
        "delete-event did not remove an orphan Event and its dependent Relation");

    auto healthyDeletion =
        wave::makeDemonstrationProject();
    const auto healthyDeletionBefore =
        healthyDeletion;
    const auto healthyEventId =
        healthyDeletion.scenarios.front()
            .events.front().id;
    const auto rejectedHealthyDeletion =
        wave::applyAutomationBatch(
            healthyDeletion,
            QJsonObject{
                {QStringLiteral("schema"),
                 QString::fromLatin1(
                     wave::AutomationBatchSchema)},
                {QStringLiteral("scenarioId"),
                 QStringLiteral("scenario-handshake")},
                {QStringLiteral("operations"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("op"),
                          QStringLiteral("delete-event")},
                         {QStringLiteral("eventId"),
                          QString::fromStdString(
                              healthyEventId)},
                     },
                 }},
            });
    expect(
        !rejectedHealthyDeletion.ok()
            && !rejectedHealthyDeletion.project
            && rejectedHealthyDeletion.failedOperation == 0
            && rejectedHealthyDeletion.error.contains(
                QStringLiteral(
                    "not reported by validate as safely deletable"))
            && healthyDeletion
                == healthyDeletionBefore,
        "delete-event accepted a healthy Event outside validation recovery");

    auto boundary =
        wave::makeDemonstrationProject();
    auto& boundaryScenario =
        boundary.scenarios.front();
    wave::Event boundaryEvent;
    boundaryEvent.id = "event-at-end";
    boundaryEvent.tick =
        boundaryScenario.duration;
    boundaryEvent.action =
        wave::EventAction::Note;
    boundaryEvent.description =
        "Imported boundary note";
    boundaryScenario.events.push_back(
        boundaryEvent);
    const auto boundaryBefore = boundary;
    const auto boundaryIssue =
        issueByCode(
            wave::validateProjectForAutomation(
                boundary),
            QStringLiteral(
                "event-outside-scenario"));
    const auto boundaryContext =
        boundaryIssue
            .value(QStringLiteral("eventContext"))
            .toObject();
    const auto boundaryEventIndex =
        boundaryScenario.events.size() - 1;
    const auto expectedBoundaryPath =
        QStringLiteral(
            "scenarios[0].events[%1].timeTick")
            .arg(
                static_cast<qulonglong>(
                    boundaryEventIndex));
    expect(
        boundaryIssue.value(
                   QStringLiteral("objectKind")).toString()
                == QStringLiteral("event")
            && boundaryIssue.value(
                   QStringLiteral("objectIndex")).toInt()
                == static_cast<int>(
                    boundaryEventIndex)
            && boundaryIssue.value(
                   QStringLiteral("path")).toString()
                == expectedBoundaryPath
            && boundaryIssue.value(
                   QStringLiteral("repairProperties")).toArray()
                == QJsonArray{
                    QStringLiteral("scenario-duration"),
                    QStringLiteral("event"),
                }
            && boundaryIssue.value(
                   QStringLiteral("repairOperations")).toArray()
                == QJsonArray{
                    QStringLiteral("set-duration"),
                    QStringLiteral("delete-event"),
                }
            && boundaryIssue.value(
                   QStringLiteral("minimumDurationTick"))
                   .toString()
                == QStringLiteral("220001")
            && boundaryContext.value(
                   QStringLiteral("timeTick")).toString()
                == QStringLiteral("220000")
            && !boundaryContext.value(
                    QStringLiteral("withinScenario")).toBool()
            && boundaryContext.value(
                   QStringLiteral("scenarioDurationTick"))
                   .toString()
                == QStringLiteral("220000")
            && boundaryContext.value(
                   QStringLiteral("action")).toString()
                == QStringLiteral("note"),
        "Event at Scenario End was not reported with an extension option");

    const auto extended =
        wave::applyAutomationBatch(
            boundary,
            QJsonObject{
                {QStringLiteral("schema"),
                 QString::fromLatin1(
                     wave::AutomationBatchSchema)},
                {QStringLiteral("scenarioId"),
                 QStringLiteral("scenario-handshake")},
                {QStringLiteral("operations"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("op"),
                          QStringLiteral("set-duration")},
                         {QStringLiteral("durationTick"),
                          boundaryIssue.value(
                              QStringLiteral(
                                  "minimumDurationTick"))},
                     },
                 }},
            });
    expect(
        extended.ok()
            && extended.project
            && extended.project
                   ->scenarios.front().duration
                == 220'001
            && wave::findEvent(
                extended.project
                    ->scenarios.front(),
                "event-at-end")
            && wave::validateProjectForAutomation(
                   *extended.project)
                   .json
                   .value(QStringLiteral("valid"))
                   .toBool()
            && boundary == boundaryBefore,
        "Event at Scenario End could not be repaired by the reported duration");

    auto negative =
        wave::makeDemonstrationProject();
    wave::Event negativeEvent;
    negativeEvent.id = "event-negative";
    negativeEvent.tick = -1;
    negativeEvent.action =
        wave::EventAction::Note;
    negative.scenarios.front().events.push_back(
        negativeEvent);
    const auto negativeIssue =
        issueByCode(
            wave::validateProjectForAutomation(
                negative),
            QStringLiteral(
                "event-outside-scenario"));
    expect(
        negativeIssue.value(
                   QStringLiteral("repairOperations")).toArray()
                == QJsonArray{
                    QStringLiteral("delete-event"),
                }
            && negativeIssue.value(
                   QStringLiteral("repairProperties")).toArray()
                == QJsonArray{
                    QStringLiteral("event"),
                }
            && !negativeIssue.contains(
                QStringLiteral("minimumDurationTick")),
        "negative Event incorrectly advertised a duration-only repair");

    auto maximum =
        wave::makeDemonstrationProject();
    wave::Event maximumEvent;
    maximumEvent.id = "event-maximum";
    maximumEvent.tick =
        std::numeric_limits<wave::Tick>::max();
    maximumEvent.action =
        wave::EventAction::Note;
    maximum.scenarios.front().events.push_back(
        maximumEvent);
    const auto maximumIssue =
        issueByCode(
            wave::validateProjectForAutomation(
                maximum),
            QStringLiteral(
                "event-outside-scenario"));
    expect(
        maximumIssue.value(
                   QStringLiteral("repairOperations")).toArray()
                == QJsonArray{
                    QStringLiteral("delete-event"),
                }
            && maximumIssue.value(
                   QStringLiteral("repairProperties")).toArray()
                == QJsonArray{
                    QStringLiteral("event"),
                }
            && !maximumIssue.contains(
                QStringLiteral("minimumDurationTick")),
        "maximum-tick Event advertised an overflowing duration repair");

    auto ambiguous =
        wave::makeDemonstrationProject();
    auto duplicate =
        ambiguous.scenarios.front().events.front();
    ambiguous.scenarios.front()
        .events.front().laneId = "lane-ambiguous";
    duplicate.laneId = "lane-ambiguous";
    ambiguous.scenarios.front().events.push_back(
        duplicate);
    const auto ambiguousValidation =
        wave::validateProjectForAutomation(
            ambiguous);
    const auto ambiguousIssues =
        ambiguousValidation.json
            .value(QStringLiteral("issues"))
            .toArray();
    expect(
        std::none_of(
            ambiguousIssues.begin(),
            ambiguousIssues.end(),
            [](const QJsonValue& value) {
                const auto issue = value.toObject();
                return issue.value(
                           QStringLiteral("code"))
                           .toString()
                           == QStringLiteral("missing-lane")
                    && issue.value(
                           QStringLiteral(
                               "repairOperations"))
                           .toArray()
                           .contains(
                               QStringLiteral("delete-event"));
            }),
        "ambiguous Event identity exposed an unsafe delete operation");
}

void testAutomationEventLinkRepairContracts()
{
    const auto issueByCode =
        [](const wave::AutomationDocument& validation,
           const QString& code) {
            const auto issues =
                validation.json
                    .value(QStringLiteral("issues"))
                    .toArray();
            const auto iterator =
                std::find_if(
                    issues.begin(),
                    issues.end(),
                    [&code](const QJsonValue& value) {
                        return value.toObject()
                                   .value(QStringLiteral("code"))
                                   .toString()
                            == code;
                    });
            return iterator == issues.end()
                ? QJsonObject{}
                : iterator->toObject();
        };

    auto broken =
        wave::makeDemonstrationProject();
    auto& scenario =
        broken.scenarios.front();
    const auto eventIterator =
        std::find_if(
            scenario.events.begin(),
            scenario.events.end(),
            [](const wave::Event& event) {
                return event.linkedSegmentId
                    == "segment-ack-high";
            });
    expect(
        eventIterator != scenario.events.end(),
        "Event link repair fixture has no ack Event");
    auto* event = &*eventIterator;
    const auto eventId = event->id;
    const auto eventIndex =
        static_cast<std::size_t>(
            event - scenario.events.data());
    expect(
        !scenario.relations.empty()
            && scenario.relations.front()
                   .targetEventId
                == eventId,
        "Event link repair fixture has no dependent Relation");
    event->laneId = "lane-reset";
    event->tick = 111'000;
    event->value = "0";
    const auto brokenBefore = broken;

    const auto validation =
        wave::validateProjectForAutomation(
            broken);
    const auto issue =
        issueByCode(
            validation,
            QStringLiteral(
                "event-waveform-mismatch"));
    const auto context =
        issue.value(
                 QStringLiteral("eventContext"))
            .toObject();
    const auto basePath =
        QStringLiteral(
            "scenarios[0].events[%1]")
            .arg(
                static_cast<qulonglong>(
                    eventIndex));
    expect(
        !validation.json
             .value(QStringLiteral("valid"))
             .toBool()
            && validation.json
                   .value(QStringLiteral("summary"))
                   .toObject()
                   .value(QStringLiteral("errors"))
                   .toString()
                == QStringLiteral("1")
            && issue.value(
                   QStringLiteral("objectKind"))
                   .toString()
                == QStringLiteral("event")
            && issue.value(
                   QStringLiteral("path"))
                   .toString()
                == basePath
                    + QStringLiteral(".laneId")
            && issue.value(
                   QStringLiteral("paths"))
                   .toArray()
                == QJsonArray{
                    basePath
                        + QStringLiteral(".laneId"),
                    basePath
                        + QStringLiteral(".timeTick"),
                    basePath
                        + QStringLiteral(".value"),
                }
            && issue.value(
                   QStringLiteral(
                       "repairProperties"))
                   .toArray()
                == QJsonArray{
                    QStringLiteral("event-link"),
                    QStringLiteral("event"),
                }
            && issue.value(
                   QStringLiteral(
                       "repairOperations"))
                   .toArray()
                == QJsonArray{
                    QStringLiteral(
                        "repair-event-link"),
                    QStringLiteral("delete-event"),
                }
            && context.value(
                   QStringLiteral("laneId"))
                   .toString()
                == QStringLiteral("lane-reset")
            && context.value(
                   QStringLiteral("laneResolved"))
                   .toBool()
            && !context.value(
                    QStringLiteral(
                        "linkedSegmentResolved"))
                    .toBool()
            && context.value(
                   QStringLiteral(
                       "linkedSegmentOwnerResolved"))
                   .toBool()
            && context.value(
                   QStringLiteral("linkedLaneId"))
                   .toString()
                == QStringLiteral("lane-ack")
            && context.value(
                   QStringLiteral(
                       "linkedSegmentStartTick"))
                   .toString()
                == QStringLiteral("110000")
            && context.value(
                   QStringLiteral(
                       "linkedSegmentEndTick"))
                   .toString()
                == QStringLiteral("150000")
            && context.value(
                   QStringLiteral(
                       "linkedSegmentValue"))
                   .toString()
                == QStringLiteral("1")
            && context.value(
                   QStringLiteral(
                       "linkedEventCount"))
                   .toInt()
                == 1
            && context.value(
                   QStringLiteral(
                       "linkRepairable"))
                   .toBool()
            && !context.value(
                    QStringLiteral(
                        "linkConsistent"))
                    .toBool(),
        "Event waveform mismatch did not expose its exact repair target");

    auto commandScenario =
        broken.scenarios.front();
    const auto commandBefore =
        commandScenario;
    wave::CommandStack commandStack;
    expect(
        commandStack.execute(
            std::make_unique<
                wave::RepairWaveformEventLinkCommand>(
                    broken,
                    commandScenario,
                    eventId)),
        "Event link repair command reported no effect");
    const auto* commandEvent =
        wave::findEvent(
            commandScenario, eventId);
    expect(
        commandEvent
            && commandEvent->laneId == "lane-ack"
            && commandEvent->tick == 110'000
            && commandEvent->value == "1"
            && commandScenario.relations.size() == 1
            && commandScenario.relations.front()
                   .targetEventId
                == eventId,
        "Event link repair command did not preserve Event identity and Relation");
    const auto commandAfter =
        commandScenario;
    expect(
        commandStack.undo()
            && commandScenario == commandBefore
            && commandStack.redo()
            && commandScenario == commandAfter,
        "Event link repair command did not preserve exact Undo/Redo");

    const auto repaired =
        wave::applyAutomationBatch(
            broken,
            QJsonObject{
                {QStringLiteral("schema"),
                 QString::fromLatin1(
                     wave::AutomationBatchSchema)},
                {QStringLiteral("scenarioId"),
                 QStringLiteral(
                     "scenario-handshake")},
                {QStringLiteral("operations"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("op"),
                          QStringLiteral(
                              "repair-event-link")},
                         {QStringLiteral("eventId"),
                          QString::fromStdString(
                              eventId)},
                     },
                 }},
            });
    const auto report =
        repaired.json
            .value(QStringLiteral("operations"))
            .toArray()
            .at(0)
            .toObject();
    const auto* repairedEvent =
        repaired.project
        ? wave::findEvent(
              repaired.project
                  ->scenarios.front(),
              eventId)
        : nullptr;
    expect(
        repaired.ok()
            && repaired.project
            && report.value(
                   QStringLiteral("changed"))
                   .toBool()
            && report.value(
                   QStringLiteral(
                       "updatedEventCount"))
                   .toInt()
                == 1
            && report.value(
                   QStringLiteral(
                       "createdEventCount"))
                   .toInt()
                == 0
            && report.value(
                   QStringLiteral(
                       "removedEventCount"))
                   .toInt()
                == 0
            && report.value(
                   QStringLiteral(
                       "removedRelationCount"))
                   .toInt()
                == 0
            && report.value(
                   QStringLiteral(
                       "removedSegmentCount"))
                   .toInt()
                == 0
            && !report.value(
                    QStringLiteral(
                        "beforeEventContext"))
                    .toObject()
                    .value(
                        QStringLiteral(
                            "linkConsistent"))
                    .toBool()
            && report.value(
                   QStringLiteral(
                       "eventContext"))
                   .toObject()
                   .value(
                       QStringLiteral(
                           "linkConsistent"))
                   .toBool()
            && repairedEvent
            && repairedEvent->laneId
                == "lane-ack"
            && repairedEvent->tick == 110'000
            && repairedEvent->value == "1"
            && repaired.project
                   ->scenarios.front()
                   .relations.size()
                == 1
            && repaired.project
                   ->scenarios.front()
                   .relations.front()
                   .targetEventId
                == eventId
            && wave::validateProjectForAutomation(
                   *repaired.project)
                   .json
                   .value(QStringLiteral("valid"))
                   .toBool()
            && broken == brokenBefore,
        "repair-event-link did not restore the Event without collateral changes");

    const auto healthy =
        wave::makeDemonstrationProject();
    const auto healthyEvent =
        std::find_if(
            healthy.scenarios.front()
                .events.begin(),
            healthy.scenarios.front()
                .events.end(),
            [](const wave::Event& candidate) {
                return candidate.linkedSegmentId
                    == "segment-ack-high";
            });
    expect(
        healthyEvent
            != healthy.scenarios.front()
                   .events.end(),
        "healthy Event link fixture is absent");
    const auto rejectedHealthy =
        wave::applyAutomationBatch(
            healthy,
            QJsonObject{
                {QStringLiteral("schema"),
                 QString::fromLatin1(
                     wave::AutomationBatchSchema)},
                {QStringLiteral("scenarioId"),
                 QStringLiteral(
                     "scenario-handshake")},
                {QStringLiteral("operations"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("op"),
                         QStringLiteral(
                              "repair-event-link")},
                         {QStringLiteral("eventId"),
                          QString::fromStdString(
                              healthyEvent->id)},
                     },
                 }},
            });
    expect(
        !rejectedHealthy.ok()
            && !rejectedHealthy.project
            && rejectedHealthy.failedOperation
                == 0
            && rejectedHealthy.error.contains(
                QStringLiteral(
                    "not reported by validate")),
        "repair-event-link accepted an already consistent Event");

    auto missingSegment = broken;
    auto* missingSegmentEvent =
        wave::findEvent(
            missingSegment.scenarios.front(),
            eventId);
    expect(
        missingSegmentEvent,
        "missing Segment Event fixture is absent");
    missingSegmentEvent->linkedSegmentId =
        "segment-missing";
    const auto missingSegmentIssue =
        issueByCode(
            wave::validateProjectForAutomation(
                missingSegment),
            QStringLiteral(
                "event-waveform-mismatch"));
    expect(
        !missingSegmentIssue.value(
             QStringLiteral("eventContext"))
             .toObject()
             .value(
                 QStringLiteral(
                     "linkRepairable"))
             .toBool()
            && missingSegmentIssue.value(
                   QStringLiteral(
                       "repairOperations"))
                   .toArray()
                == QJsonArray{
                    QStringLiteral("delete-event"),
                },
        "missing linked Segment exposed an unsafe link repair");
}

void testAutomationEventCycleRepairContracts()
{
    const auto issueByCode =
        [](const wave::AutomationDocument& validation,
           const QString& code) {
            const auto issues =
                validation.json
                    .value(QStringLiteral("issues"))
                    .toArray();
            const auto iterator =
                std::find_if(
                    issues.begin(),
                    issues.end(),
                    [&code](const QJsonValue& value) {
                        return value.toObject()
                                   .value(QStringLiteral("code"))
                                   .toString()
                            == code;
                    });
            return iterator == issues.end()
                ? QJsonObject{}
                : iterator->toObject();
        };

    auto broken =
        wave::makeDemonstrationProject();
    auto& scenario =
        broken.scenarios.front();
    const auto eventIterator =
        std::find_if(
            scenario.events.begin(),
            scenario.events.end(),
            [](const wave::Event& event) {
                return event.linkedSegmentId
                    == "segment-ack-high";
            });
    expect(
        eventIterator != scenario.events.end(),
        "Event cycle fixture has no ack Event");
    auto* event = &*eventIterator;
    const auto eventId = event->id;
    const auto eventIndex =
        static_cast<std::size_t>(
            event - scenario.events.data());
    event->clockDomainId = "clock-main";
    event->cycle = 8;
    const auto brokenBefore = broken;

    const auto validation =
        wave::validateProjectForAutomation(
            broken);
    const auto issue =
        issueByCode(
            validation,
            QStringLiteral(
                "event-cycle-mismatch"));
    const auto context =
        issue.value(
                 QStringLiteral("eventContext"))
            .toObject();
    const auto basePath =
        QStringLiteral(
            "scenarios[0].events[%1]")
            .arg(
                static_cast<qulonglong>(
                    eventIndex));
    expect(
        !validation.json
             .value(QStringLiteral("valid"))
             .toBool()
            && validation.json
                   .value(QStringLiteral("summary"))
                   .toObject()
                   .value(QStringLiteral("errors"))
                   .toString()
                == QStringLiteral("1")
            && issue.value(
                   QStringLiteral("objectKind"))
                   .toString()
                == QStringLiteral("event")
            && issue.value(
                   QStringLiteral("path"))
                   .toString()
                == basePath
                    + QStringLiteral(".cycle")
            && issue.value(
                   QStringLiteral("paths"))
                   .toArray()
                == QJsonArray{
                    basePath
                        + QStringLiteral(".cycle"),
                    basePath
                        + QStringLiteral(".timeTick"),
                }
            && issue.value(
                   QStringLiteral(
                       "repairProperties"))
                   .toArray()
                == QJsonArray{
                    QStringLiteral("event-cycle"),
                }
            && issue.value(
                   QStringLiteral(
                       "repairOperations"))
                   .toArray()
                == QJsonArray{
                    QStringLiteral(
                        "clear-event-cycle"),
                }
            && context.value(
                   QStringLiteral("cycle"))
                   .toString()
                == QStringLiteral("8")
            && context.value(
                   QStringLiteral("cyclePresent"))
                   .toBool()
            && context.value(
                   QStringLiteral(
                       "cycleClockSource"))
                   .toString()
                == QStringLiteral("event")
            && context.value(
                   QStringLiteral(
                       "effectiveClockDomainId"))
                   .toString()
                == QStringLiteral("clock-main")
            && context.value(
                   QStringLiteral(
                       "cycleClockMatchCount"))
                   .toInt()
                == 1
            && context.value(
                   QStringLiteral(
                       "cycleClockResolved"))
                   .toBool()
            && context.value(
                   QStringLiteral(
                       "cycleExpectedTimeTick"))
                   .toString()
                == QStringLiteral("80000")
            && context.value(
                   QStringLiteral(
                       "cycleExpectedTime"))
                   .toString()
                == QStringLiteral("80 ns")
            && !context.value(
                    QStringLiteral(
                        "cycleConsistent"))
                    .toBool()
            && context.value(
                   QStringLiteral(
                       "cycleRepairable"))
                   .toBool()
            && context.value(
                   QStringLiteral("timeTick"))
                   .toString()
                == QStringLiteral("110000")
            && context.value(
                   QStringLiteral(
                       "linkConsistent"))
                   .toBool(),
        "Event cycle mismatch did not expose its exact timing conflict");

    auto commandScenario =
        broken.scenarios.front();
    const auto commandBefore =
        commandScenario;
    wave::CommandStack commandStack;
    expect(
        commandStack.execute(
            std::make_unique<
                wave::ClearEventCycleCommand>(
                    commandScenario,
                    eventId)),
        "Event cycle clear command reported no effect");
    const auto* commandEvent =
        wave::findEvent(
            commandScenario, eventId);
    expect(
        commandEvent
            && !commandEvent->cycle
            && commandEvent->tick == 110'000
            && commandEvent->clockDomainId
                == "clock-main"
            && commandEvent->linkedSegmentId
                == "segment-ack-high"
            && commandScenario.relations.size() == 1
            && commandScenario.relations.front()
                   .targetEventId
                == eventId,
        "Event cycle clear changed current Event or Relation semantics");
    const auto commandAfter =
        commandScenario;
    expect(
        commandStack.undo()
            && commandScenario == commandBefore
            && commandStack.redo()
            && commandScenario == commandAfter,
        "Event cycle clear command did not preserve exact Undo/Redo");

    const auto repaired =
        wave::applyAutomationBatch(
            broken,
            QJsonObject{
                {QStringLiteral("schema"),
                 QString::fromLatin1(
                     wave::AutomationBatchSchema)},
                {QStringLiteral("scenarioId"),
                 QStringLiteral(
                     "scenario-handshake")},
                {QStringLiteral("operations"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("op"),
                          QStringLiteral(
                              "clear-event-cycle")},
                         {QStringLiteral("eventId"),
                          QString::fromStdString(
                              eventId)},
                     },
                 }},
            });
    const auto report =
        repaired.json
            .value(QStringLiteral("operations"))
            .toArray()
            .at(0)
            .toObject();
    const auto beforeContext =
        report.value(
                  QStringLiteral(
                      "beforeEventContext"))
            .toObject();
    const auto afterContext =
        report.value(
                  QStringLiteral("eventContext"))
            .toObject();
    const auto* repairedEvent =
        repaired.project
        ? wave::findEvent(
              repaired.project
                  ->scenarios.front(),
              eventId)
        : nullptr;
    expect(
        repaired.ok()
            && repaired.project
            && report.value(
                   QStringLiteral("changed"))
                   .toBool()
            && report.value(
                   QStringLiteral(
                       "updatedEventCount"))
                   .toInt()
                == 1
            && report.value(
                   QStringLiteral(
                       "createdEventCount"))
                   .toInt()
                == 0
            && report.value(
                   QStringLiteral(
                       "removedEventCount"))
                   .toInt()
                == 0
            && report.value(
                   QStringLiteral(
                       "removedRelationCount"))
                   .toInt()
                == 0
            && report.value(
                   QStringLiteral(
                       "removedSegmentCount"))
                   .toInt()
                == 0
            && beforeContext.value(
                   QStringLiteral("cyclePresent"))
                   .toBool()
            && !beforeContext.value(
                    QStringLiteral(
                        "cycleConsistent"))
                    .toBool()
            && !afterContext.value(
                    QStringLiteral("cyclePresent"))
                    .toBool()
            && afterContext.value(
                   QStringLiteral(
                       "cycleConsistent"))
                   .toBool()
            && !afterContext.value(
                    QStringLiteral(
                        "cycleRepairable"))
                    .toBool()
            && repairedEvent
            && !repairedEvent->cycle
            && repairedEvent->tick == 110'000
            && repairedEvent->linkedSegmentId
                == "segment-ack-high"
            && repaired.project
                   ->scenarios.front()
                   .relations.size()
                == 1
            && repaired.project
                   ->scenarios.front()
                   .relations.front()
                   .targetEventId
                == eventId
            && wave::validateProjectForAutomation(
                   *repaired.project)
                   .json
                   .value(QStringLiteral("valid"))
                   .toBool()
            && broken == brokenBefore,
        "clear-event-cycle did not preserve current timing and intent");

    const auto healthy =
        wave::makeDemonstrationProject();
    const auto healthyEvent =
        std::find_if(
            healthy.scenarios.front()
                .events.begin(),
            healthy.scenarios.front()
                .events.end(),
            [](const wave::Event& candidate) {
                return candidate.linkedSegmentId
                    == "segment-ack-high";
            });
    expect(
        healthyEvent
            != healthy.scenarios.front()
                   .events.end(),
        "healthy Event cycle fixture is absent");
    const auto rejectedHealthy =
        wave::applyAutomationBatch(
            healthy,
            QJsonObject{
                {QStringLiteral("schema"),
                 QString::fromLatin1(
                     wave::AutomationBatchSchema)},
                {QStringLiteral("scenarioId"),
                 QStringLiteral(
                     "scenario-handshake")},
                {QStringLiteral("operations"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("op"),
                          QStringLiteral(
                              "clear-event-cycle")},
                         {QStringLiteral("eventId"),
                          QString::fromStdString(
                              healthyEvent->id)},
                     },
                 }},
            });
    expect(
        !rejectedHealthy.ok()
            && !rejectedHealthy.project
            && rejectedHealthy.failedOperation
                == 0
            && rejectedHealthy.error.contains(
                QStringLiteral(
                    "not reported by validate")),
        "clear-event-cycle accepted an Event without inconsistent cycle metadata");

    auto missingClock =
        wave::makeDemonstrationProject();
    auto missingClockIterator =
        std::find_if(
            missingClock.scenarios.front()
                .events.begin(),
            missingClock.scenarios.front()
                .events.end(),
            [](const wave::Event& candidate) {
                return candidate.linkedSegmentId
                    == "segment-ack-high";
            });
    auto* missingClockEvent =
        missingClockIterator
                == missingClock.scenarios.front()
                       .events.end()
            ? nullptr
            : &*missingClockIterator;
    expect(
        missingClockEvent,
        "missing-clock cycle fixture is absent");
    missingClockEvent->clockDomainId.clear();
    const auto* missingClockLane =
        wave::findLane(
            missingClock.scenarios.front(),
            missingClockEvent->laneId);
    expect(
        missingClockLane,
        "missing-clock cycle fixture has no Lane");
    auto* mutableMissingClockLane =
        wave::findLane(
            missingClock.scenarios.front(),
            missingClockEvent->laneId);
    mutableMissingClockLane
        ->clockDomainId.clear();
    missingClockEvent->cycle = 11;
    const auto missingClockIssue =
        issueByCode(
            wave::validateProjectForAutomation(
                missingClock),
            QStringLiteral(
                "event-cycle-mismatch"));
    const auto missingClockContext =
        missingClockIssue.value(
                             QStringLiteral(
                                 "eventContext"))
            .toObject();
    expect(
        missingClockIssue.value(
                   QStringLiteral("paths"))
                   .toArray()
                == QJsonArray{
                    basePath
                        + QStringLiteral(".cycle"),
                }
            && missingClockContext.value(
                   QStringLiteral(
                       "cycleClockSource"))
                   .toString()
                == QStringLiteral("none")
            && missingClockContext.value(
                   QStringLiteral(
                       "cycleClockMatchCount"))
                   .toInt()
                == 0
            && missingClockContext.value(
                   QStringLiteral(
                       "cycleExpectedTimeTick"))
                   .isNull(),
        "unresolved cycle clock did not expose a bounded repair path");

    auto negativeCycle =
        wave::makeDemonstrationProject();
    auto negativeIterator =
        std::find_if(
            negativeCycle.scenarios.front()
                .events.begin(),
            negativeCycle.scenarios.front()
                .events.end(),
            [](const wave::Event& candidate) {
                return candidate.linkedSegmentId
                    == "segment-ack-high";
            });
    auto* negativeEvent =
        negativeIterator
                == negativeCycle.scenarios.front()
                       .events.end()
            ? nullptr
            : &*negativeIterator;
    expect(
        negativeEvent,
        "negative-cycle fixture is absent");
    negativeEvent->cycle = -1;
    const auto negativeIssue =
        issueByCode(
            wave::validateProjectForAutomation(
                negativeCycle),
            QStringLiteral(
                "event-cycle-mismatch"));
    expect(
        negativeIssue.value(
                 QStringLiteral("eventContext"))
            .toObject()
            .value(QStringLiteral("cycle"))
            .toString()
                == QStringLiteral("-1")
            && negativeIssue.value(
                   QStringLiteral("paths"))
                   .toArray()
                == QJsonArray{
                    basePath
                        + QStringLiteral(".cycle"),
                },
        "negative Event cycle did not remain diagnosable");

    auto ambiguous =
        broken;
    ambiguous.scenarios.front()
        .events.push_back(
            *wave::findEvent(
                ambiguous.scenarios.front(),
                eventId));
    const auto ambiguousRepair =
        wave::applyAutomationBatch(
            ambiguous,
            QJsonObject{
                {QStringLiteral("schema"),
                 QString::fromLatin1(
                     wave::AutomationBatchSchema)},
                {QStringLiteral("scenarioId"),
                 QStringLiteral(
                     "scenario-handshake")},
                {QStringLiteral("operations"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("op"),
                          QStringLiteral(
                              "clear-event-cycle")},
                         {QStringLiteral("eventId"),
                          QString::fromStdString(
                              eventId)},
                     },
                 }},
            });
    expect(
        !ambiguousRepair.ok()
            && !ambiguousRepair.project
            && ambiguousRepair.error.contains(
                QStringLiteral("ambiguous")),
        "clear-event-cycle accepted an ambiguous Event identity");

    auto linkRepair =
        wave::makeDemonstrationProject();
    auto& linkScenario =
        linkRepair.scenarios.front();
    auto linkedIterator =
        std::find_if(
            linkScenario.events.begin(),
            linkScenario.events.end(),
            [](const wave::Event& candidate) {
                return candidate.linkedSegmentId
                    == "segment-ack-high";
            });
    auto* linkedEvent =
        linkedIterator == linkScenario.events.end()
        ? nullptr
        : &*linkedIterator;
    expect(
        linkedEvent,
        "cycle-aware link repair fixture is absent");
    linkedEvent->tick = 80'000;
    linkedEvent->clockDomainId =
        "clock-main";
    linkedEvent->cycle = 8;
    const auto linkedEventId =
        linkedEvent->id;
    wave::CommandStack linkStack;
    expect(
        linkStack.execute(
            std::make_unique<
                wave::RepairWaveformEventLinkCommand>(
                    linkRepair,
                    linkScenario,
                    linkedEventId)),
        "cycle-aware Event link repair reported no effect");
    linkedEvent =
        wave::findEvent(
            linkScenario, linkedEventId);
    expect(
        linkedEvent
            && linkedEvent->tick == 110'000
            && !linkedEvent->cycle,
        "Event link repair retained a cycle that no longer matched its target");

    auto preservedCycle =
        wave::makeDemonstrationProject();
    auto& preservedScenario =
        preservedCycle.scenarios.front();
    auto preservedIterator =
        std::find_if(
            preservedScenario.events.begin(),
            preservedScenario.events.end(),
            [](const wave::Event& candidate) {
                return candidate.linkedSegmentId
                    == "segment-ack-high";
            });
    auto* preservedEvent =
        preservedIterator
                == preservedScenario.events.end()
            ? nullptr
            : &*preservedIterator;
    expect(
        preservedEvent,
        "cycle-preserving link repair fixture is absent");
    preservedEvent->tick = 80'000;
    preservedEvent->clockDomainId =
        "clock-main";
    preservedEvent->cycle = 11;
    const auto preservedEventId =
        preservedEvent->id;
    wave::CommandStack preservedStack;
    expect(
        preservedStack.execute(
            std::make_unique<
                wave::RepairWaveformEventLinkCommand>(
                    preservedCycle,
                    preservedScenario,
                    preservedEventId)),
        "cycle-preserving Event link repair reported no effect");
    preservedEvent =
        wave::findEvent(
            preservedScenario,
            preservedEventId);
    expect(
        preservedEvent
            && preservedEvent->tick == 110'000
            && preservedEvent->cycle
            && *preservedEvent->cycle == 11,
        "Event link repair discarded cycle metadata that matches its target");
}

void testAutomationEventClockRepairContracts()
{
    const auto issueByCode =
        [](const wave::AutomationDocument& validation,
           const QString& code) {
            const auto issues =
                validation.json
                    .value(QStringLiteral("issues"))
                    .toArray();
            const auto iterator =
                std::find_if(
                    issues.begin(),
                    issues.end(),
                    [&code](const QJsonValue& value) {
                        return value.toObject()
                                   .value(QStringLiteral("code"))
                                   .toString()
                            == code;
                    });
            return iterator == issues.end()
                ? QJsonObject{}
                : iterator->toObject();
        };
    const auto findLinkedEvent =
        [](wave::Scenario& scenario,
           const std::string_view segmentId)
            -> wave::Event* {
            const auto iterator =
                std::find_if(
                    scenario.events.begin(),
                    scenario.events.end(),
                    [segmentId](
                        const wave::Event& event) {
                        return event.linkedSegmentId
                            == segmentId;
                    });
            return iterator == scenario.events.end()
                ? nullptr
                : &*iterator;
        };

    auto broken =
        wave::makeDemonstrationProject();
    auto& scenario =
        broken.scenarios.front();
    auto* event =
        findLinkedEvent(
            scenario, "segment-ack-high");
    auto* sourceEvent =
        findLinkedEvent(
            scenario, "segment-req-high");
    auto* lane =
        wave::findLane(
            scenario, "lane-ack");
    auto* sourceLane =
        wave::findLane(
            scenario, "lane-request");
    expect(
        event && sourceEvent
            && lane && sourceLane,
        "Event clock repair fixture is incomplete");
    event->clockDomainId =
        "clock-missing";
    lane->clockDomainId =
        "clock-main";
    sourceEvent->clockDomainId =
        "clock-main";
    sourceLane->clockDomainId =
        "clock-main";
    const auto eventId = event->id;
    const auto eventIndex =
        static_cast<std::size_t>(
            event - scenario.events.data());
    const auto brokenBefore = broken;

    const auto validation =
        wave::validateProjectForAutomation(
            broken);
    const auto issue =
        issueByCode(
            validation,
            QStringLiteral(
                "event-clock-domain-invalid"));
    const auto context =
        issue.value(
                 QStringLiteral("eventContext"))
            .toObject();
    const auto basePath =
        QStringLiteral(
            "scenarios[0].events[%1].clockDomainId")
            .arg(
                static_cast<qulonglong>(
                    eventIndex));
    expect(
        !validation.json
             .value(QStringLiteral("valid"))
             .toBool()
            && validation.json
                   .value(QStringLiteral("summary"))
                   .toObject()
                   .value(QStringLiteral("errors"))
                   .toString()
                == QStringLiteral("2")
            && issue.value(
                   QStringLiteral("objectKind"))
                   .toString()
                == QStringLiteral("event")
            && issue.value(
                   QStringLiteral("path"))
                   .toString()
                == basePath
            && issue.value(
                   QStringLiteral("paths"))
                   .toArray()
                == QJsonArray{basePath}
            && issue.value(
                   QStringLiteral(
                       "repairProperties"))
                   .toArray()
                == QJsonArray{
                    QStringLiteral("event-clock"),
                }
            && issue.value(
                   QStringLiteral(
                       "repairOperations"))
                   .toArray()
                == QJsonArray{
                    QStringLiteral(
                        "repair-event-clock"),
                }
            && context.value(
                   QStringLiteral("clockDomainId"))
                   .toString()
                == QStringLiteral("clock-missing")
            && context.value(
                   QStringLiteral(
                       "clockDomainMatchCount"))
                   .toInt()
                == 0
            && !context.value(
                    QStringLiteral(
                        "clockReferenceValid"))
                    .toBool()
            && context.value(
                   QStringLiteral(
                       "laneClockDomainId"))
                   .toString()
                == QStringLiteral("clock-main")
            && context.value(
                   QStringLiteral(
                       "laneClockMatchCount"))
                   .toInt()
                == 1
            && context.value(
                   QStringLiteral(
                       "clockRepairAction"))
                   .toString()
                == QStringLiteral(
                    "use-lane-clock")
            && context.value(
                   QStringLiteral(
                       "replacementClockDomainId"))
                   .toString()
                == QStringLiteral("clock-main")
            && context.value(
                   QStringLiteral(
                       "clockRepairable"))
                   .toBool(),
        "invalid Event ClockDomain did not expose its safe Lane fallback");

    auto commandScenario =
        broken.scenarios.front();
    const auto commandBefore =
        commandScenario;
    wave::CommandStack commandStack;
    expect(
        commandStack.execute(
            std::make_unique<
                wave::RepairEventClockReferenceCommand>(
                    broken,
                    commandScenario,
                    eventId)),
        "Event clock repair command reported no effect");
    const auto* commandEvent =
        wave::findEvent(
            commandScenario, eventId);
    expect(
        commandEvent
            && commandEvent->clockDomainId
                == "clock-main"
            && commandEvent->tick == 110'000
            && commandEvent->linkedSegmentId
                == "segment-ack-high"
            && commandScenario.relations.size() == 1
            && commandScenario.relations.front()
                   .targetEventId
                == eventId,
        "Event clock repair changed current timing or Relation identity");
    const auto commandAfter =
        commandScenario;
    expect(
        commandStack.undo()
            && commandScenario == commandBefore
            && commandStack.redo()
            && commandScenario == commandAfter,
        "Event clock repair command did not preserve exact Undo/Redo");

    const auto repaired =
        wave::applyAutomationBatch(
            broken,
            QJsonObject{
                {QStringLiteral("schema"),
                 QString::fromLatin1(
                     wave::AutomationBatchSchema)},
                {QStringLiteral("scenarioId"),
                 QStringLiteral(
                     "scenario-handshake")},
                {QStringLiteral("operations"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("op"),
                          QStringLiteral(
                              "repair-event-clock")},
                         {QStringLiteral("eventId"),
                          QString::fromStdString(
                              eventId)},
                     },
                 }},
            });
    const auto report =
        repaired.json
            .value(QStringLiteral("operations"))
            .toArray()
            .at(0)
            .toObject();
    const auto beforeContext =
        report.value(
                  QStringLiteral(
                      "beforeEventContext"))
            .toObject();
    const auto afterContext =
        report.value(
                  QStringLiteral("eventContext"))
            .toObject();
    const auto* repairedEvent =
        repaired.project
        ? wave::findEvent(
              repaired.project
                  ->scenarios.front(),
              eventId)
        : nullptr;
    expect(
        repaired.ok()
            && repaired.project
            && repaired.json
                   .value(
                       QStringLiteral(
                           "validationGuard"))
                   .toObject()
                   .value(
                       QStringLiteral(
                           "sourceErrorCount"))
                   .toString()
                == QStringLiteral("2")
            && repaired.json
                   .value(
                       QStringLiteral(
                           "validationGuard"))
                   .toObject()
                   .value(
                       QStringLiteral(
                           "candidateErrorCount"))
                   .toString()
                == QStringLiteral("0")
            && report.value(
                   QStringLiteral("changed"))
                   .toBool()
            && report.value(
                   QStringLiteral(
                       "updatedEventCount"))
                   .toInt()
                == 1
            && report.value(
                   QStringLiteral(
                       "createdEventCount"))
                   .toInt()
                == 0
            && report.value(
                   QStringLiteral(
                       "removedEventCount"))
                   .toInt()
                == 0
            && report.value(
                   QStringLiteral(
                       "removedRelationCount"))
                   .toInt()
                == 0
            && report.value(
                   QStringLiteral(
                       "removedSegmentCount"))
                   .toInt()
                == 0
            && !beforeContext.value(
                    QStringLiteral(
                        "clockReferenceValid"))
                    .toBool()
            && afterContext.value(
                   QStringLiteral(
                       "clockReferenceValid"))
                   .toBool()
            && afterContext.value(
                   QStringLiteral("clockDomainId"))
                   .toString()
                == QStringLiteral("clock-main")
            && repairedEvent
            && repairedEvent->tick == 110'000
            && repairedEvent->value == "1"
            && repairedEvent->linkedSegmentId
                == "segment-ack-high"
            && repaired.project
                   ->scenarios.front()
                   .relations.size()
                == 1
            && repaired.project
                   ->scenarios.front()
                   .relations.front()
                   .targetEventId
                == eventId
            && wave::validateProjectForAutomation(
                   *repaired.project)
                   .json
                   .value(QStringLiteral("valid"))
                   .toBool()
            && broken == brokenBefore,
        "repair-event-clock did not restore the Event and dependent Relation");

    auto clearFallback =
        wave::makeDemonstrationProject();
    auto& clearScenario =
        clearFallback.scenarios.front();
    auto* clearEvent =
        findLinkedEvent(
            clearScenario,
            "segment-ack-high");
    expect(
        clearEvent,
        "clear Event clock fallback fixture is absent");
    clearEvent->clockDomainId =
        "clock-missing";
    const auto clearEventId =
        clearEvent->id;
    const auto clearIssue =
        issueByCode(
            wave::validateProjectForAutomation(
                clearFallback),
            QStringLiteral(
                "event-clock-domain-invalid"));
    expect(
        clearIssue.value(
                 QStringLiteral("eventContext"))
            .toObject()
            .value(
                QStringLiteral(
                    "clockRepairAction"))
            .toString()
                == QStringLiteral(
                    "clear-event-clock"),
        "Event without a Lane clock did not expose explicit-clock clearing");
    wave::CommandStack clearStack;
    expect(
        clearStack.execute(
            std::make_unique<
                wave::RepairEventClockReferenceCommand>(
                    clearFallback,
                    clearScenario,
                    clearEventId))
            && wave::findEvent(
                   clearScenario,
                   clearEventId)
                   ->clockDomainId.empty(),
        "Event clock repair did not clear an unusable explicit reference");

    auto cyclePreserved =
        broken;
    auto* preservedEvent =
        wave::findEvent(
            cyclePreserved.scenarios.front(),
            eventId);
    expect(
        preservedEvent,
        "clock repair cycle-preservation fixture is absent");
    preservedEvent->tick = 110'000;
    preservedEvent->cycle = 11;
    wave::CommandStack preservedStack;
    expect(
        preservedStack.execute(
            std::make_unique<
                wave::RepairEventClockReferenceCommand>(
                    cyclePreserved,
                    cyclePreserved
                        .scenarios.front(),
                    eventId))
            && wave::findEvent(
                   cyclePreserved
                       .scenarios.front(),
                   eventId)
                   ->cycle
            && *wave::findEvent(
                    cyclePreserved
                        .scenarios.front(),
                    eventId)
                    ->cycle == 11,
        "Event clock repair discarded a cycle valid under the Lane clock");

    auto cycleCleared =
        broken;
    auto* clearedCycleEvent =
        wave::findEvent(
            cycleCleared.scenarios.front(),
            eventId);
    expect(
        clearedCycleEvent,
        "clock repair cycle-clearing fixture is absent");
    clearedCycleEvent->cycle = 8;
    wave::CommandStack cycleClearStack;
    expect(
        cycleClearStack.execute(
            std::make_unique<
                wave::RepairEventClockReferenceCommand>(
                    cycleCleared,
                    cycleCleared
                        .scenarios.front(),
                    eventId))
            && !wave::findEvent(
                    cycleCleared
                        .scenarios.front(),
                    eventId)
                    ->cycle,
        "Event clock repair retained a cycle invalid under the Lane clock");

    const auto healthy =
        wave::makeDemonstrationProject();
    const auto healthyEvent =
        std::find_if(
            healthy.scenarios.front()
                .events.begin(),
            healthy.scenarios.front()
                .events.end(),
            [](const wave::Event& candidate) {
                return candidate.linkedSegmentId
                    == "segment-ack-high";
            });
    expect(
        healthyEvent
            != healthy.scenarios.front()
                   .events.end(),
        "healthy Event clock fixture is absent");
    const auto rejectedHealthy =
        wave::applyAutomationBatch(
            healthy,
            QJsonObject{
                {QStringLiteral("schema"),
                 QString::fromLatin1(
                     wave::AutomationBatchSchema)},
                {QStringLiteral("scenarioId"),
                 QStringLiteral(
                     "scenario-handshake")},
                {QStringLiteral("operations"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("op"),
                          QStringLiteral(
                              "repair-event-clock")},
                         {QStringLiteral("eventId"),
                          QString::fromStdString(
                              healthyEvent->id)},
                     },
                 }},
            });
    expect(
        !rejectedHealthy.ok()
            && !rejectedHealthy.project
            && rejectedHealthy.error.contains(
                QStringLiteral(
                    "not reported by validate")),
        "repair-event-clock accepted a healthy Event");

    auto ambiguousClock =
        broken;
    auto unrelatedClock =
        ambiguousClock.clockDomains.front();
    unrelatedClock.id = "clock-other";
    unrelatedClock.name = "other";
    ambiguousClock.clockDomains.push_back(
        unrelatedClock);
    ambiguousClock.clockDomains.push_back(
        unrelatedClock);
    const auto ambiguousClockValidation =
        wave::validateProjectForAutomation(
            ambiguousClock);
    const auto ambiguousClockIssue =
        issueByCode(
            ambiguousClockValidation,
            QStringLiteral(
                "event-clock-domain-invalid"));
    expect(
        ambiguousClockIssue.value(
                   QStringLiteral("eventContext"))
                   .toObject()
                   .value(
                       QStringLiteral(
                           "clockDomainMatchCount"))
                   .toInt()
                == 0
            && ambiguousClockIssue.value(
                   QStringLiteral(
                       "repairOperations"))
                   .toArray()
                == QJsonArray{
                    QStringLiteral(
                        "repair-event-clock"),
                },
        "unrelated duplicate Clock changed a missing Event clock repair");

    auto unsafeFallback =
        wave::makeDemonstrationProject();
    auto& unsafeScenario =
        unsafeFallback.scenarios.front();
    auto* unsafeEvent =
        findLinkedEvent(
            unsafeScenario,
            "segment-ack-high");
    auto* unsafeLane =
        wave::findLane(
            unsafeScenario, "lane-ack");
    expect(
        unsafeEvent && unsafeLane,
        "unsafe Event clock fallback fixture is incomplete");
    unsafeEvent->clockDomainId =
        "clock-missing";
    unsafeLane->clockDomainId =
        "clock-also-missing";
    const auto unsafeIssue =
        issueByCode(
            wave::validateProjectForAutomation(
                unsafeFallback),
            QStringLiteral(
                "event-clock-domain-invalid"));
    expect(
        !unsafeIssue.value(
             QStringLiteral("eventContext"))
             .toObject()
             .value(
                 QStringLiteral(
                     "clockRepairable"))
             .toBool()
            && unsafeIssue.value(
                   QStringLiteral(
                       "repairOperations"))
                   .toArray()
                   .isEmpty(),
        "Event clock repair advertised an invalid Lane fallback");

    auto ambiguousEvent =
        broken;
    ambiguousEvent.scenarios.front()
        .events.push_back(
            *wave::findEvent(
                ambiguousEvent
                    .scenarios.front(),
                eventId));
    const auto ambiguousEventRepair =
        wave::applyAutomationBatch(
            ambiguousEvent,
            QJsonObject{
                {QStringLiteral("schema"),
                 QString::fromLatin1(
                     wave::AutomationBatchSchema)},
                {QStringLiteral("scenarioId"),
                 QStringLiteral(
                     "scenario-handshake")},
                {QStringLiteral("operations"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("op"),
                          QStringLiteral(
                              "repair-event-clock")},
                         {QStringLiteral("eventId"),
                          QString::fromStdString(
                              eventId)},
                     },
                 }},
            });
    expect(
        !ambiguousEventRepair.ok()
            && !ambiguousEventRepair.project
            && ambiguousEventRepair.error.contains(
                QStringLiteral("ambiguous")),
        "repair-event-clock accepted an ambiguous Event identity");
}

void testAutomationLaneClockRepairContracts()
{
    const auto issueForLane =
        [](const wave::AutomationDocument& validation,
           const QString& laneId) {
            const auto issues =
                validation.json
                    .value(QStringLiteral("issues"))
                    .toArray();
            const auto iterator =
                std::find_if(
                    issues.begin(),
                    issues.end(),
                    [&laneId](const QJsonValue& value) {
                        const auto issue =
                            value.toObject();
                        return issue
                                   .value(
                                       QStringLiteral(
                                           "code"))
                                   .toString()
                                == QStringLiteral(
                                    "lane-clock-domain-invalid")
                            && issue
                                   .value(
                                       QStringLiteral(
                                           "laneId"))
                                   .toString()
                                == laneId;
                    });
            return iterator == issues.end()
                ? QJsonObject{}
                : iterator->toObject();
        };
    const auto operationBatch =
        [](const std::string& scenarioId,
           const std::string& laneId) {
            return QJsonObject{
                {QStringLiteral("schema"),
                 QString::fromLatin1(
                     wave::AutomationBatchSchema)},
                {QStringLiteral("scenarioId"),
                 QString::fromStdString(
                     scenarioId)},
                {QStringLiteral("operations"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("op"),
                          QStringLiteral(
                              "repair-lane-clock")},
                         {QStringLiteral("laneId"),
                          QString::fromStdString(
                              laneId)},
                     },
                 }},
            };
        };
    const auto linkedEvent =
        [](wave::Scenario& scenario,
           const std::string_view segmentId)
            -> wave::Event* {
            const auto iterator =
                std::find_if(
                    scenario.events.begin(),
                    scenario.events.end(),
                    [segmentId](
                        const wave::Event& event) {
                        return event.linkedSegmentId
                            == segmentId;
                    });
            return iterator == scenario.events.end()
                ? nullptr
                : &*iterator;
        };

    auto broken =
        wave::makeDemonstrationProject();
    auto& scenario =
        broken.scenarios.front();
    auto* lane =
        wave::findLane(
            scenario, "lane-ack");
    expect(
        lane,
        "Lane clock repair fixture has no ack Lane");
    lane->clockDomainId =
        "clock-missing";
    const auto laneId = lane->id;
    const auto laneIndex =
        static_cast<std::size_t>(
            lane - scenario.lanes.data());
    const auto brokenBefore = broken;

    const auto validation =
        wave::validateProjectForAutomation(
            broken);
    const auto issue =
        issueForLane(
            validation,
            QString::fromStdString(
                laneId));
    const auto context =
        issue.value(
                 QStringLiteral("laneContext"))
            .toObject();
    const auto path =
        QStringLiteral(
            "scenarios[0].lanes[%1].clockDomainId")
            .arg(
                static_cast<qulonglong>(
                    laneIndex));
    expect(
        !validation.json
             .value(QStringLiteral("valid"))
             .toBool()
            && validation.json
                   .value(QStringLiteral("summary"))
                   .toObject()
                   .value(QStringLiteral("errors"))
                   .toString()
                == QStringLiteral("2")
            && issue.value(
                   QStringLiteral("objectKind"))
                   .toString()
                == QStringLiteral("lane")
            && issue.value(
                   QStringLiteral("path"))
                   .toString()
                == path
            && issue.value(
                   QStringLiteral("paths"))
                   .toArray()
                == QJsonArray{path}
            && issue.value(
                   QStringLiteral(
                       "repairProperties"))
                   .toArray()
                == QJsonArray{
                    QStringLiteral("lane-clock"),
                }
            && issue.value(
                   QStringLiteral(
                       "repairOperations"))
                   .toArray()
                == QJsonArray{
                    QStringLiteral(
                        "repair-lane-clock"),
                }
            && context.value(
                   QStringLiteral(
                       "clockDomainId"))
                   .toString()
                == QStringLiteral(
                    "clock-missing")
            && context.value(
                   QStringLiteral(
                       "clockDomainMatchCount"))
                   .toInt()
                == 0
            && !context.value(
                    QStringLiteral(
                        "clockReferenceValid"))
                    .toBool()
            && context.value(
                   QStringLiteral(
                       "clockRepairAction"))
                   .toString()
                == QStringLiteral(
                    "clear-lane-clock")
            && context.value(
                   QStringLiteral(
                       "replacementClockDomainId"))
                   .toString()
                   .isEmpty()
            && context.value(
                   QStringLiteral(
                       "clockRepairable"))
                   .toBool(),
        "missing optional Lane clock did not expose a safe clear repair: "
            + QJsonDocument(validation.json)
                  .toJson(QJsonDocument::Compact)
                  .toStdString());

    auto commandScenario =
        broken.scenarios.front();
    const auto commandBefore =
        commandScenario;
    const auto eventsBefore =
        commandScenario.events;
    const auto relationsBefore =
        commandScenario.relations;
    wave::CommandStack commandStack;
    expect(
        commandStack.execute(
            std::make_unique<
                wave::RepairLaneClockReferenceCommand>(
                    broken,
                    commandScenario,
                    laneId)),
        "Lane clock repair command reported no effect");
    const auto* commandLane =
        wave::findLane(
            commandScenario, laneId);
    expect(
        commandLane
            && commandLane
                   ->clockDomainId.empty()
            && commandScenario.events
                == eventsBefore
            && commandScenario.relations
                == relationsBefore,
        "Lane clock repair changed unrelated current data");
    const auto commandAfter =
        commandScenario;
    expect(
        commandStack.undo()
            && commandScenario
                == commandBefore
            && commandStack.redo()
            && commandScenario
                == commandAfter,
        "Lane clock repair command did not preserve exact Undo/Redo");

    const auto repaired =
        wave::applyAutomationBatch(
            broken,
            operationBatch(
                scenario.id,
                laneId));
    const auto report =
        repaired.json
            .value(
                QStringLiteral("operations"))
            .toArray()
            .at(0)
            .toObject();
    const auto beforeContext =
        report.value(
                  QStringLiteral(
                      "beforeLaneContext"))
            .toObject();
    const auto afterContext =
        report.value(
                  QStringLiteral(
                      "laneContext"))
            .toObject();
    const auto* repairedLane =
        repaired.project
        ? wave::findLane(
              repaired.project
                  ->scenarios.front(),
              laneId)
        : nullptr;
    expect(
        repaired.ok()
            && repaired.project
            && repaired.changed
            && repaired.json
                   .value(
                       QStringLiteral(
                           "validationGuard"))
                   .toObject()
                   .value(
                       QStringLiteral(
                           "sourceErrorCount"))
                   .toString()
                == QStringLiteral("2")
            && repaired.json
                   .value(
                       QStringLiteral(
                           "validationGuard"))
                   .toObject()
                   .value(
                       QStringLiteral(
                           "candidateErrorCount"))
                   .toString()
                == QStringLiteral("0")
            && report.value(
                   QStringLiteral("changed"))
                   .toBool()
            && report.value(
                   QStringLiteral(
                       "updatedLaneCount"))
                   .toInt()
                == 1
            && report.value(
                   QStringLiteral(
                       "clearedEventCycleCount"))
                   .toInt()
                == 0
            && report.value(
                   QStringLiteral(
                       "updatedEventCount"))
                   .toInt()
                == 0
            && report.value(
                   QStringLiteral(
                       "createdEventCount"))
                   .toInt()
                == 0
            && report.value(
                   QStringLiteral(
                       "removedEventCount"))
                   .toInt()
                == 0
            && report.value(
                   QStringLiteral(
                       "removedRelationCount"))
                   .toInt()
                == 0
            && report.value(
                   QStringLiteral(
                       "removedSegmentCount"))
                   .toInt()
                == 0
            && !beforeContext.value(
                    QStringLiteral(
                        "clockReferenceValid"))
                    .toBool()
            && afterContext.value(
                   QStringLiteral(
                       "clockReferenceValid"))
                   .toBool()
            && repairedLane
            && repairedLane
                   ->clockDomainId.empty()
            && wave::validateProjectForAutomation(
                   *repaired.project)
                   .json
                   .value(
                       QStringLiteral("valid"))
                   .toBool()
            && broken == brokenBefore,
        "repair-lane-clock did not clear the missing optional reference atomically");

    auto inheritedCycle =
        wave::makeDemonstrationProject();
    auto& inheritedScenario =
        inheritedCycle.scenarios.front();
    auto* inheritedLane =
        wave::findLane(
            inheritedScenario,
            "lane-ack");
    auto* inheritedEvent =
        linkedEvent(
            inheritedScenario,
            "segment-ack-high");
    expect(
        inheritedLane && inheritedEvent,
        "inherited Event cycle fixture is incomplete");
    inheritedLane->clockDomainId =
        "clock-missing";
    inheritedEvent->clockDomainId.clear();
    inheritedEvent->cycle = 11;
    const auto inheritedEventId =
        inheritedEvent->id;
    const auto inheritedIssue =
        issueForLane(
            wave::validateProjectForAutomation(
                inheritedCycle),
            QStringLiteral("lane-ack"));
    const auto inheritedContext =
        inheritedIssue
            .value(
                QStringLiteral("laneContext"))
            .toObject();
    expect(
        inheritedContext.value(
                   QStringLiteral(
                       "cycleEventCount"))
                   .toInt()
                == 1
            && inheritedContext.value(
                   QStringLiteral(
                       "preservedCycleEventCount"))
                   .toInt()
                == 0
            && inheritedContext.value(
                   QStringLiteral(
                       "clearedCycleEventCount"))
                   .toInt()
                == 1,
        "Lane clock diagnosis did not disclose inherited cycle cleanup");
    const auto cycleRepaired =
        wave::applyAutomationBatch(
            inheritedCycle,
            operationBatch(
                inheritedScenario.id,
                inheritedLane->id));
    const auto cycleReport =
        cycleRepaired.json
            .value(
                QStringLiteral("operations"))
            .toArray()
            .at(0)
            .toObject();
    const auto* cycleEventAfter =
        cycleRepaired.project
        ? wave::findEvent(
              cycleRepaired.project
                  ->scenarios.front(),
              inheritedEventId)
        : nullptr;
    expect(
        cycleRepaired.ok()
            && cycleEventAfter
            && !cycleEventAfter->cycle
            && cycleReport.value(
                   QStringLiteral(
                       "clearedEventCycleCount"))
                   .toInt()
                == 1
            && cycleReport.value(
                   QStringLiteral(
                       "updatedEventCount"))
                   .toInt()
                == 1
            && cycleReport.value(
                   QStringLiteral(
                       "removedEventCount"))
                   .toInt()
                == 0
            && cycleReport.value(
                   QStringLiteral(
                       "removedRelationCount"))
                   .toInt()
                == 0
            && wave::validateProjectForAutomation(
                   *cycleRepaired.project)
                   .json
                   .value(
                       QStringLiteral("valid"))
                   .toBool(),
        "Lane clock repair retained unusable inherited cycle metadata");

    auto missingClockLane =
        wave::makeDemonstrationProject();
    auto& missingClockScenario =
        missingClockLane.scenarios.front();
    auto* clockLane =
        wave::findLane(
            missingClockScenario,
            "lane-clk");
    expect(
        clockLane,
        "Clock Lane repair fixture is absent");
    clockLane->clockDomainId.clear();
    const auto clockIssue =
        issueForLane(
            wave::validateProjectForAutomation(
                missingClockLane),
            QStringLiteral("lane-clk"));
    const auto clockContext =
        clockIssue.value(
                  QStringLiteral(
                      "laneContext"))
            .toObject();
    expect(
        clockContext.value(
                   QStringLiteral(
                       "clockRepairAction"))
                   .toString()
                == QStringLiteral(
                    "use-only-project-clock")
            && clockContext.value(
                   QStringLiteral(
                       "replacementClockDomainId"))
                   .toString()
                == QStringLiteral(
                    "clock-main")
            && clockIssue.value(
                   QStringLiteral(
                       "repairOperations"))
                   .toArray()
                   .contains(
                       QStringLiteral(
                           "repair-lane-clock")),
        "Clock Lane did not expose its sole unambiguous project clock");
    const auto clockRepair =
        wave::applyAutomationBatch(
            missingClockLane,
            operationBatch(
                missingClockScenario.id,
                clockLane->id));
    const auto* clockLaneAfter =
        clockRepair.project
        ? wave::findLane(
              clockRepair.project
                  ->scenarios.front(),
              "lane-clk")
        : nullptr;
    expect(
        clockRepair.ok()
            && clockLaneAfter
            && clockLaneAfter
                   ->clockDomainId
                == "clock-main",
        "repair-lane-clock did not adopt the sole project clock");

    auto ambiguousClock =
        wave::makeDemonstrationProject();
    auto otherClock =
        ambiguousClock.clockDomains.front();
    otherClock.id = "clock-other";
    otherClock.name = "other";
    ambiguousClock.clockDomains.push_back(
        std::move(otherClock));
    auto& ambiguousScenario =
        ambiguousClock.scenarios.front();
    auto* ambiguousLane =
        wave::findLane(
            ambiguousScenario,
            "lane-clk");
    expect(
        ambiguousLane,
        "ambiguous Clock Lane fixture is absent");
    ambiguousLane->clockDomainId =
        "clock-missing";
    const auto ambiguousBefore =
        ambiguousClock;
    const auto ambiguousIssue =
        issueForLane(
            wave::validateProjectForAutomation(
                ambiguousClock),
            QStringLiteral("lane-clk"));
    const auto ambiguousContext =
        ambiguousIssue
            .value(
                QStringLiteral("laneContext"))
            .toObject();
    const auto rejectedAmbiguous =
        wave::applyAutomationBatch(
            ambiguousClock,
            operationBatch(
                ambiguousScenario.id,
                ambiguousLane->id));
    expect(
        ambiguousContext.value(
                   QStringLiteral(
                       "projectClockDomainCount"))
                   .toInt()
                == 2
            && ambiguousContext.value(
                   QStringLiteral(
                       "clockRepairAction"))
                   .toString()
                == QStringLiteral("none")
            && !ambiguousContext.value(
                    QStringLiteral(
                        "clockRepairable"))
                    .toBool()
            && ambiguousIssue.value(
                   QStringLiteral(
                       "repairOperations"))
                   .toArray()
                   .isEmpty()
            && !rejectedAmbiguous.ok()
            && !rejectedAmbiguous.project
            && rejectedAmbiguous.error.contains(
                QStringLiteral(
                    "not reported by validate"))
            && ambiguousClock
                == ambiguousBefore,
        "Lane clock repair guessed between multiple project clocks");

    const auto healthy =
        wave::makeDemonstrationProject();
    const auto rejectedHealthy =
        wave::applyAutomationBatch(
            healthy,
            operationBatch(
                healthy.scenarios.front().id,
                "lane-reset"));
    expect(
        !rejectedHealthy.ok()
            && !rejectedHealthy.project
            && rejectedHealthy.error.contains(
                QStringLiteral(
                    "not reported by validate")),
        "repair-lane-clock accepted a healthy Lane");

    auto duplicateLane = broken;
    duplicateLane.scenarios.front()
        .lanes.push_back(
            *wave::findLane(
                duplicateLane
                    .scenarios.front(),
                laneId));
    const auto duplicateBefore =
        duplicateLane;
    const auto rejectedDuplicate =
        wave::applyAutomationBatch(
            duplicateLane,
            operationBatch(
                duplicateLane
                    .scenarios.front().id,
                laneId));
    expect(
        !rejectedDuplicate.ok()
            && !rejectedDuplicate.project
            && rejectedDuplicate.error.contains(
                QStringLiteral("ambiguous"))
            && duplicateLane
                == duplicateBefore,
        "repair-lane-clock accepted an ambiguous Lane identity");
}

void testAutomationLaneGroupRepairContracts()
{
    const auto issueForLane =
        [](const wave::AutomationDocument& validation,
           const QString& laneId) {
            const auto issues =
                validation.json
                    .value(QStringLiteral("issues"))
                    .toArray();
            const auto iterator =
                std::find_if(
                    issues.begin(),
                    issues.end(),
                    [&laneId](const QJsonValue& value) {
                        const auto issue =
                            value.toObject();
                        return issue
                                   .value(
                                       QStringLiteral(
                                           "code"))
                                   .toString()
                                == QStringLiteral(
                                    "lane-group-reference-invalid")
                            && issue
                                   .value(
                                       QStringLiteral(
                                           "laneId"))
                                   .toString()
                                == laneId;
                    });
            return iterator == issues.end()
                ? QJsonObject{}
                : iterator->toObject();
        };
    const auto operationBatch =
        [](const std::string& scenarioId,
           const std::string& laneId) {
            return QJsonObject{
                {QStringLiteral("schema"),
                 QString::fromLatin1(
                     wave::AutomationBatchSchema)},
                {QStringLiteral("scenarioId"),
                 QString::fromStdString(
                     scenarioId)},
                {QStringLiteral("operations"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("op"),
                          QStringLiteral(
                              "repair-lane-group")},
                         {QStringLiteral("laneId"),
                          QString::fromStdString(
                              laneId)},
                     },
                 }},
            };
        };

    auto broken =
        wave::makeDemonstrationProject();
    auto& scenario =
        broken.scenarios.front();
    auto* lane =
        wave::findLane(
            scenario, "lane-ack");
    expect(
        lane,
        "Lane group repair fixture has no ack Lane");
    lane->groupId =
        "group-missing";
    const auto laneId = lane->id;
    const auto laneIndex =
        static_cast<std::size_t>(
            lane - scenario.lanes.data());
    const auto brokenBefore = broken;

    const auto validation =
        wave::validateProjectForAutomation(
            broken);
    const auto issue =
        issueForLane(
            validation,
            QString::fromStdString(
                laneId));
    const auto context =
        issue.value(
                 QStringLiteral("laneContext"))
            .toObject();
    const auto path =
        QStringLiteral(
            "scenarios[0].lanes[%1].groupId")
            .arg(
                static_cast<qulonglong>(
                    laneIndex));
    expect(
        !validation.json
             .value(QStringLiteral("valid"))
             .toBool()
            && validation.json
                   .value(QStringLiteral("summary"))
                   .toObject()
                   .value(QStringLiteral("errors"))
                   .toString()
                == QStringLiteral("1")
            && issue.value(
                   QStringLiteral("objectKind"))
                   .toString()
                == QStringLiteral("lane")
            && issue.value(
                   QStringLiteral("path"))
                   .toString()
                == path
            && issue.value(
                   QStringLiteral("paths"))
                   .toArray()
                == QJsonArray{path}
            && issue.value(
                   QStringLiteral(
                       "repairProperties"))
                   .toArray()
                == QJsonArray{
                    QStringLiteral("lane-group"),
                }
            && issue.value(
                   QStringLiteral(
                       "repairOperations"))
                   .toArray()
                == QJsonArray{
                    QStringLiteral(
                        "repair-lane-group"),
                }
            && context.value(
                   QStringLiteral("groupId"))
                   .toString()
                == QStringLiteral(
                    "group-missing")
            && context.value(
                   QStringLiteral(
                       "groupIdMatchCount"))
                   .toInt()
                == 0
            && context.value(
                   QStringLiteral(
                       "groupLaneMatchCount"))
                   .toInt()
                == 0
            && !context.value(
                    QStringLiteral(
                        "groupReferenceValid"))
                    .toBool()
            && !context.value(
                    QStringLiteral(
                        "targetResolved"))
                    .toBool()
            && context.value(
                   QStringLiteral(
                       "targetLaneIndex"))
                   .isNull()
            && context.value(
                   QStringLiteral(
                       "groupRepairAction"))
                   .toString()
                == QStringLiteral(
                    "clear-lane-group")
            && context.value(
                   QStringLiteral(
                       "replacementGroupId"))
                   .toString()
                   .isEmpty()
            && context.value(
                   QStringLiteral(
                       "groupRepairable"))
                   .toBool(),
        "missing Lane Group did not expose a safe ungroup repair: "
            + QJsonDocument(validation.json)
                  .toJson(QJsonDocument::Compact)
                  .toStdString());

    auto commandScenario =
        broken.scenarios.front();
    const auto commandBefore =
        commandScenario;
    const auto eventsBefore =
        commandScenario.events;
    const auto relationsBefore =
        commandScenario.relations;
    wave::CommandStack commandStack;
    expect(
        commandStack.execute(
            std::make_unique<
                wave::RepairLaneGroupReferenceCommand>(
                    commandScenario,
                    laneId)),
        "Lane group repair command reported no effect");
    const auto* commandLane =
        wave::findLane(
            commandScenario, laneId);
    expect(
        commandLane
            && commandLane
                   ->groupId.empty()
            && commandLane->segments
                == lane->segments
            && commandScenario.events
                == eventsBefore
            && commandScenario.relations
                == relationsBefore,
        "Lane group repair changed waveform or timing intent");
    const auto commandAfter =
        commandScenario;
    expect(
        commandStack.undo()
            && commandScenario
                == commandBefore
            && commandStack.redo()
            && commandScenario
                == commandAfter,
        "Lane group repair command did not preserve exact Undo/Redo");

    const auto repaired =
        wave::applyAutomationBatch(
            broken,
            operationBatch(
                scenario.id,
                laneId));
    const auto report =
        repaired.json
            .value(
                QStringLiteral("operations"))
            .toArray()
            .at(0)
            .toObject();
    const auto beforeContext =
        report.value(
                  QStringLiteral(
                      "beforeLaneContext"))
            .toObject();
    const auto afterContext =
        report.value(
                  QStringLiteral(
                      "laneContext"))
            .toObject();
    const auto* repairedLane =
        repaired.project
        ? wave::findLane(
              repaired.project
                  ->scenarios.front(),
              laneId)
        : nullptr;
    expect(
        repaired.ok()
            && repaired.project
            && repaired.changed
            && repaired.json
                   .value(
                       QStringLiteral(
                           "validationGuard"))
                   .toObject()
                   .value(
                       QStringLiteral(
                           "sourceErrorCount"))
                   .toString()
                == QStringLiteral("1")
            && repaired.json
                   .value(
                       QStringLiteral(
                           "validationGuard"))
                   .toObject()
                   .value(
                       QStringLiteral(
                           "candidateErrorCount"))
                   .toString()
                == QStringLiteral("0")
            && report.value(
                   QStringLiteral(
                       "updatedLaneCount"))
                   .toInt()
                == 1
            && report.value(
                   QStringLiteral(
                       "updatedEventCount"))
                   .toInt()
                == 0
            && report.value(
                   QStringLiteral(
                       "createdEventCount"))
                   .toInt()
                == 0
            && report.value(
                   QStringLiteral(
                       "removedEventCount"))
                   .toInt()
                == 0
            && report.value(
                   QStringLiteral(
                       "removedRelationCount"))
                   .toInt()
                == 0
            && report.value(
                   QStringLiteral(
                       "removedSegmentCount"))
                   .toInt()
                == 0
            && !beforeContext.value(
                    QStringLiteral(
                        "groupReferenceValid"))
                    .toBool()
            && afterContext.value(
                   QStringLiteral(
                       "groupReferenceValid"))
                   .toBool()
            && repairedLane
            && repairedLane
                   ->groupId.empty()
            && wave::validateProjectForAutomation(
                   *repaired.project)
                   .json
                   .value(
                       QStringLiteral("valid"))
                   .toBool()
            && broken == brokenBefore,
        "repair-lane-group did not ungroup the Lane atomically");

    auto wrongKind =
        wave::makeDemonstrationProject();
    auto& wrongKindScenario =
        wrongKind.scenarios.front();
    auto* wrongKindLane =
        wave::findLane(
            wrongKindScenario,
            "lane-ack");
    expect(
        wrongKindLane,
        "wrong-kind Group fixture is absent");
    wrongKindLane->groupId =
        "lane-reset";
    const auto wrongKindIssue =
        issueForLane(
            wave::validateProjectForAutomation(
                wrongKind),
            QStringLiteral("lane-ack"));
    const auto wrongKindContext =
        wrongKindIssue
            .value(
                QStringLiteral("laneContext"))
            .toObject();
    const auto wrongKindRepair =
        wave::applyAutomationBatch(
            wrongKind,
            operationBatch(
                wrongKindScenario.id,
                wrongKindLane->id));
    expect(
        wrongKindContext.value(
                   QStringLiteral(
                       "groupIdMatchCount"))
                   .toInt()
                == 1
            && wrongKindContext.value(
                   QStringLiteral(
                       "groupLaneMatchCount"))
                   .toInt()
                == 0
            && wrongKindContext.value(
                   QStringLiteral(
                       "targetResolved"))
                   .toBool()
            && wrongKindContext.value(
                   QStringLiteral(
                       "targetLaneName"))
                   .toString()
                == QStringLiteral("reset_n")
            && wrongKindContext.value(
                   QStringLiteral(
                       "targetLaneKind"))
                   .toString()
                == QStringLiteral("bit")
            && wrongKindRepair.ok()
            && wave::findLane(
                   wrongKindRepair.project
                       ->scenarios.front(),
                   "lane-ack")
                   ->groupId.empty(),
        "non-Group target was not diagnosed and safely cleared");

    auto nestedGroup =
        wave::makeDemonstrationProject();
    auto& nestedScenario =
        nestedGroup.scenarios.front();
    auto* nestedLane =
        wave::findLane(
            nestedScenario,
            "group-handshake");
    expect(
        nestedLane,
        "nested Group fixture is absent");
    nestedLane->groupId =
        "group-handshake";
    const auto nestedIssue =
        issueForLane(
            wave::validateProjectForAutomation(
                nestedGroup),
            QStringLiteral(
                "group-handshake"));
    const auto nestedContext =
        nestedIssue
            .value(
                QStringLiteral("laneContext"))
            .toObject();
    wave::CommandStack nestedStack;
    expect(
        nestedContext.value(
                   QStringLiteral(
                       "targetIsSelf"))
                   .toBool()
            && nestedContext.value(
                   QStringLiteral(
                       "groupRepairable"))
                   .toBool()
            && nestedStack.execute(
                std::make_unique<
                    wave::RepairLaneGroupReferenceCommand>(
                        nestedScenario,
                        nestedLane->id))
            && wave::findLane(
                   nestedScenario,
                   "group-handshake")
                   ->groupId.empty(),
        "nested or self-referencing Group was not safely ungrouped");

    auto ambiguousGroup =
        wave::makeDemonstrationProject();
    auto& ambiguousScenario =
        ambiguousGroup.scenarios.front();
    const auto* sourceGroup =
        wave::findLane(
            ambiguousScenario,
            "group-handshake");
    expect(
        sourceGroup,
        "ambiguous Group fixture is absent");
    auto duplicateGroup = *sourceGroup;
    duplicateGroup.name =
        "Handshake duplicate";
    ambiguousScenario.lanes.push_back(
        std::move(duplicateGroup));
    const auto ambiguousBefore =
        ambiguousGroup;
    const auto ambiguousIssue =
        issueForLane(
            wave::validateProjectForAutomation(
                ambiguousGroup),
            QStringLiteral("lane-ack"));
    const auto ambiguousContext =
        ambiguousIssue
            .value(
                QStringLiteral("laneContext"))
            .toObject();
    const auto ambiguousRepair =
        wave::applyAutomationBatch(
            ambiguousGroup,
            operationBatch(
                ambiguousScenario.id,
                "lane-ack"));
    const auto ambiguousGuard =
        ambiguousRepair.json
            .value(
                QStringLiteral(
                    "validationGuard"))
            .toObject();
    expect(
        ambiguousContext.value(
                   QStringLiteral(
                       "groupIdMatchCount"))
                   .toInt()
                == 2
            && ambiguousContext.value(
                   QStringLiteral(
                       "groupLaneMatchCount"))
                   .toInt()
                == 2
            && !ambiguousContext.value(
                    QStringLiteral(
                        "targetResolved"))
                    .toBool()
            && ambiguousRepair.ok()
            && ambiguousRepair.project
            && ambiguousGuard.value(
                   QStringLiteral("reason"))
                   .toString()
                == QStringLiteral(
                    "progressive-repair")
            && ambiguousGuard.value(
                   QStringLiteral(
                       "sourceErrorCount"))
                   .toString()
                == QStringLiteral("6")
            && ambiguousGuard.value(
                   QStringLiteral(
                       "candidateErrorCount"))
                   .toString()
                == QStringLiteral("5")
            && wave::findLane(
                   ambiguousRepair.project
                       ->scenarios.front(),
                   "lane-ack")
                   ->groupId.empty()
            && ambiguousGroup
                == ambiguousBefore,
        "ambiguous Group target was guessed or could not be progressively cleared");

    const auto healthy =
        wave::makeDemonstrationProject();
    const auto rejectedHealthy =
        wave::applyAutomationBatch(
            healthy,
            operationBatch(
                healthy.scenarios.front().id,
                "lane-ack"));
    expect(
        !rejectedHealthy.ok()
            && !rejectedHealthy.project
            && rejectedHealthy.error.contains(
                QStringLiteral(
                    "not reported by validate")),
        "repair-lane-group accepted a healthy Lane");

    auto duplicateLane = broken;
    duplicateLane.scenarios.front()
        .lanes.push_back(
            *wave::findLane(
                duplicateLane
                    .scenarios.front(),
                laneId));
    const auto duplicateBefore =
        duplicateLane;
    const auto rejectedDuplicate =
        wave::applyAutomationBatch(
            duplicateLane,
            operationBatch(
                duplicateLane
                    .scenarios.front().id,
                laneId));
    expect(
        !rejectedDuplicate.ok()
            && !rejectedDuplicate.project
            && rejectedDuplicate.error.contains(
                QStringLiteral("ambiguous"))
            && duplicateLane
                == duplicateBefore,
        "repair-lane-group accepted an ambiguous Lane identity");
}

void testAutomationRelationClockRepairContracts()
{
    const auto issueForRelation =
        [](const wave::AutomationDocument& validation,
           const QString& relationId) {
            const auto issues =
                validation.json
                    .value(QStringLiteral("issues"))
                    .toArray();
            const auto iterator =
                std::find_if(
                    issues.begin(),
                    issues.end(),
                    [&relationId](const QJsonValue& value) {
                        const auto issue =
                            value.toObject();
                        return issue
                                   .value(
                                       QStringLiteral("code"))
                                   .toString()
                                == QStringLiteral(
                                    "relation-clock-domain-invalid")
                            && issue
                                   .value(
                                       QStringLiteral(
                                           "relationId"))
                                   .toString()
                                == relationId;
                    });
            return iterator == issues.end()
                ? QJsonObject{}
                : iterator->toObject();
        };
    const auto operationBatch =
        [](const std::string& scenarioId,
           const std::string& relationId) {
            return QJsonObject{
                {QStringLiteral("schema"),
                 QString::fromLatin1(
                     wave::AutomationBatchSchema)},
                {QStringLiteral("scenarioId"),
                 QString::fromStdString(
                     scenarioId)},
                {QStringLiteral("operations"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("op"),
                          QStringLiteral(
                              "repair-relation-clock")},
                         {QStringLiteral("relationId"),
                          QString::fromStdString(
                              relationId)},
                     },
                 }},
            };
        };

    auto broken =
        wave::makeDemonstrationProject();
    auto& scenario =
        broken.scenarios.front();
    expect(
        scenario.relations.size() == 1,
        "Relation clock repair fixture has no unique Relation");
    auto& relation =
        scenario.relations.front();
    auto* sourceEvent =
        wave::findEvent(
            scenario, relation.sourceEventId);
    auto* targetEvent =
        wave::findEvent(
            scenario, relation.targetEventId);
    expect(
        sourceEvent && targetEvent,
        "Relation clock repair fixture has no endpoints");
    sourceEvent->clockDomainId =
        "clock-main";
    targetEvent->clockDomainId =
        "clock-main";
    relation.clockDomainId =
        "clock-missing";
    const auto relationId =
        relation.id;
    const auto relationIndex =
        std::size_t{0};
    const auto brokenBefore = broken;

    const auto validation =
        wave::validateProjectForAutomation(
            broken);
    const auto issue =
        issueForRelation(
            validation,
            QString::fromStdString(
                relationId));
    const auto context =
        issue.value(
                 QStringLiteral(
                     "relationClockContext"))
            .toObject();
    const auto path =
        QStringLiteral(
            "scenarios[0].relations[0].clockDomainId");
    expect(
        !validation.json
             .value(QStringLiteral("valid"))
             .toBool()
            && validation.json
                   .value(QStringLiteral("summary"))
                   .toObject()
                   .value(QStringLiteral("errors"))
                   .toString()
                == QStringLiteral("2")
            && issue.value(
                   QStringLiteral("objectKind"))
                   .toString()
                == QStringLiteral("relation")
            && issue.value(
                   QStringLiteral("path"))
                   .toString()
                == path
            && issue.value(
                   QStringLiteral("paths"))
                   .toArray()
                == QJsonArray{path}
            && issue.value(
                   QStringLiteral(
                       "repairProperties"))
                   .toArray()
                == QJsonArray{
                    QStringLiteral(
                        "relation-clock"),
                }
            && issue.value(
                   QStringLiteral(
                       "repairOperations"))
                   .toArray()
                == QJsonArray{
                    QStringLiteral(
                        "repair-relation-clock"),
                }
            && context.value(
                   QStringLiteral(
                       "clockDomainId"))
                   .toString()
                == QStringLiteral(
                    "clock-missing")
            && context.value(
                   QStringLiteral(
                       "clockDomainMatchCount"))
                   .toInt()
                == 0
            && !context.value(
                    QStringLiteral(
                        "clockReferenceValid"))
                    .toBool()
            && context.value(
                   QStringLiteral(
                       "sourceEffectiveClockDomainId"))
                   .toString()
                == QStringLiteral("clock-main")
            && context.value(
                   QStringLiteral(
                       "targetEffectiveClockDomainId"))
                   .toString()
                == QStringLiteral("clock-main")
            && context.value(
                   QStringLiteral(
                       "sourceClockDomainMatchCount"))
                   .toInt()
                == 1
            && context.value(
                   QStringLiteral(
                       "targetClockDomainMatchCount"))
                   .toInt()
                == 1
            && context.value(
                   QStringLiteral(
                       "sourceClockContextReady"))
                   .toBool()
            && context.value(
                   QStringLiteral(
                       "targetClockContextReady"))
                   .toBool()
            && !context.value(
                    QStringLiteral(
                        "endpointClockConflict"))
                    .toBool()
            && context.value(
                   QStringLiteral(
                       "clockRepairAction"))
                   .toString()
                == QStringLiteral(
                    "use-endpoint-clock")
            && context.value(
                   QStringLiteral(
                       "replacementClockDomainId"))
                   .toString()
                == QStringLiteral("clock-main")
            && context.value(
                   QStringLiteral(
                       "clockRepairable"))
                   .toBool(),
        "missing Relation ClockDomain did not expose endpoint-derived repair: "
            + QJsonDocument(validation.json)
                  .toJson(QJsonDocument::Compact)
                  .toStdString());

    auto commandProject = broken;
    auto& commandScenario =
        commandProject.scenarios.front();
    const auto commandBefore =
        commandScenario;
    const auto eventsBefore =
        commandScenario.events;
    const auto lanesBefore =
        commandScenario.lanes;
    wave::CommandStack commandStack;
    expect(
        commandStack.execute(
            std::make_unique<
                wave::RepairRelationClockReferenceCommand>(
                commandProject,
                commandScenario,
                relationId)),
        "Relation clock repair command reported no effect");
    expect(
        commandScenario.relations.at(relationIndex)
                .clockDomainId
            == "clock-main"
            && commandScenario.events
                == eventsBefore
            && commandScenario.lanes
                == lanesBefore,
        "Relation clock repair changed endpoints or waveform");
    const auto commandAfter =
        commandScenario;
    expect(
        commandStack.undo()
            && commandScenario
                == commandBefore
            && commandStack.redo()
            && commandScenario
                == commandAfter,
        "Relation clock repair command did not preserve exact Undo/Redo");

    const auto repaired =
        wave::applyAutomationBatch(
            broken,
            operationBatch(
                scenario.id,
                relationId));
    const auto report =
        repaired.json
            .value(QStringLiteral("operations"))
            .toArray()
            .at(0)
            .toObject();
    const auto beforeContext =
        report.value(
                  QStringLiteral(
                      "beforeRelationClockContext"))
            .toObject();
    const auto afterContext =
        report.value(
                  QStringLiteral(
                      "relationClockContext"))
            .toObject();
    expect(
        repaired.ok()
            && repaired.project
            && repaired.changed
            && repaired.json
                   .value(
                       QStringLiteral(
                           "validationGuard"))
                   .toObject()
                   .value(
                       QStringLiteral(
                           "sourceErrorCount"))
                   .toString()
                == QStringLiteral("2")
            && repaired.json
                   .value(
                       QStringLiteral(
                           "validationGuard"))
                   .toObject()
                   .value(
                       QStringLiteral(
                           "candidateErrorCount"))
                   .toString()
                == QStringLiteral("0")
            && report.value(
                   QStringLiteral(
                       "updatedRelationCount"))
                   .toInt()
                == 1
            && report.value(
                   QStringLiteral(
                       "createdEventCount"))
                   .toInt()
                == 0
            && report.value(
                   QStringLiteral(
                       "updatedEventCount"))
                   .toInt()
                == 0
            && report.value(
                   QStringLiteral(
                       "removedEventCount"))
                   .toInt()
                == 0
            && report.value(
                   QStringLiteral(
                       "removedRelationCount"))
                   .toInt()
                == 0
            && report.value(
                   QStringLiteral(
                       "removedSegmentCount"))
                   .toInt()
                == 0
            && beforeContext.value(
                   QStringLiteral(
                       "clockRepairAction"))
                   .toString()
                == QStringLiteral(
                    "use-endpoint-clock")
            && afterContext.value(
                   QStringLiteral(
                       "clockReferenceValid"))
                   .toBool()
            && repaired.project
                   ->scenarios.front()
                   .relations.front()
                   .clockDomainId
                == "clock-main"
            && repaired.project
                   ->scenarios.front().events
                == scenario.events
            && broken == brokenBefore,
        "repair-relation-clock did not repair only the Relation clock");

    auto noClock =
        wave::makeDemonstrationProject();
    noClock.scenarios.front()
        .relations.front().clockDomainId =
        "clock-missing";
    const auto noClockIssue =
        issueForRelation(
            wave::validateProjectForAutomation(
                noClock),
            QStringLiteral(
                "relation-req-ack"));
    const auto noClockContext =
        noClockIssue.value(
                        QStringLiteral(
                            "relationClockContext"))
            .toObject();
    const auto cleared =
        wave::applyAutomationBatch(
            noClock,
            operationBatch(
                noClock.scenarios.front().id,
                "relation-req-ack"));
    expect(
        noClockContext.value(
                   QStringLiteral(
                       "sourceEffectiveClockDomainId"))
                   .toString()
                   .isEmpty()
            && noClockContext.value(
                   QStringLiteral(
                       "targetEffectiveClockDomainId"))
                   .toString()
                   .isEmpty()
            && noClockContext.value(
                   QStringLiteral(
                       "clockRepairAction"))
                   .toString()
                == QStringLiteral(
                    "clear-relation-clock")
            && cleared.ok()
            && cleared.project
            && cleared.project
                   ->scenarios.front()
                   .relations.front()
                   .clockDomainId.empty(),
        "unclocked Relation did not safely clear its missing clock reference");

    auto conflict =
        wave::makeDemonstrationProject();
    auto otherClock =
        conflict.clockDomains.front();
    otherClock.id = "clock-other";
    otherClock.name = "other";
    conflict.clockDomains.push_back(
        std::move(otherClock));
    auto& conflictScenario =
        conflict.scenarios.front();
    auto& conflictRelation =
        conflictScenario.relations.front();
    wave::findEvent(
        conflictScenario,
        conflictRelation.sourceEventId)
        ->clockDomainId = "clock-main";
    wave::findEvent(
        conflictScenario,
        conflictRelation.targetEventId)
        ->clockDomainId = "clock-other";
    conflictRelation.clockDomainId =
        "clock-missing";
    const auto conflictBefore =
        conflict;
    const auto conflictIssue =
        issueForRelation(
            wave::validateProjectForAutomation(
                conflict),
            QStringLiteral(
                "relation-req-ack"));
    const auto conflictContext =
        conflictIssue.value(
                         QStringLiteral(
                             "relationClockContext"))
            .toObject();
    const auto conflictOperations =
        conflictIssue.value(
                         QStringLiteral(
                             "repairOperations"))
            .toArray();
    const auto rejectedConflict =
        wave::applyAutomationBatch(
            conflict,
            operationBatch(
                conflictScenario.id,
                conflictRelation.id));
    expect(
        conflictContext.value(
                   QStringLiteral(
                       "endpointClockConflict"))
                   .toBool()
            && conflictContext.value(
                   QStringLiteral(
                       "clockRepairAction"))
                   .toString()
                == QStringLiteral("none")
            && !conflictContext.value(
                    QStringLiteral(
                        "clockRepairable"))
                    .toBool()
            && !conflictOperations.contains(
                QStringLiteral(
                    "repair-relation-clock"))
            && !rejectedConflict.ok()
            && !rejectedConflict.project
            && rejectedConflict.error.contains(
                QStringLiteral(
                    "not reported by validate"))
            && conflict == conflictBefore,
        "Relation clock repair guessed between conflicting endpoints");

    auto ambiguousClock =
        wave::makeDemonstrationProject();
    ambiguousClock.clockDomains.push_back(
        ambiguousClock.clockDomains.front());
    const auto ambiguousClockBefore =
        ambiguousClock;
    const auto ambiguousIssue =
        issueForRelation(
            wave::validateProjectForAutomation(
                ambiguousClock),
            QStringLiteral(
                "relation-req-ack"));
    const auto ambiguousContext =
        ambiguousIssue.value(
                          QStringLiteral(
                              "relationClockContext"))
            .toObject();
    const auto rejectedAmbiguous =
        wave::applyAutomationBatch(
            ambiguousClock,
            operationBatch(
                ambiguousClock
                    .scenarios.front().id,
                "relation-req-ack"));
    expect(
        ambiguousContext.value(
                   QStringLiteral(
                       "clockDomainMatchCount"))
                   .toInt()
                == 2
            && !ambiguousContext.value(
                    QStringLiteral(
                        "clockRepairable"))
                    .toBool()
            && !rejectedAmbiguous.ok()
            && !rejectedAmbiguous.project
            && ambiguousClock
                == ambiguousClockBefore,
        "Relation clock repair guessed an ambiguous project ClockDomain");

    const auto healthy =
        wave::makeDemonstrationProject();
    const auto rejectedHealthy =
        wave::applyAutomationBatch(
            healthy,
            operationBatch(
                healthy.scenarios.front().id,
                "relation-req-ack"));
    expect(
        !rejectedHealthy.ok()
            && !rejectedHealthy.project
            && rejectedHealthy.error.contains(
                QStringLiteral(
                    "not reported by validate")),
        "repair-relation-clock accepted a healthy Relation");

    auto duplicateRelation = broken;
    duplicateRelation.scenarios.front()
        .relations.push_back(
            duplicateRelation
                .scenarios.front()
                .relations.front());
    const auto duplicateBefore =
        duplicateRelation;
    const auto rejectedDuplicate =
        wave::applyAutomationBatch(
            duplicateRelation,
            operationBatch(
                duplicateRelation
                    .scenarios.front().id,
                relationId));
    expect(
        !rejectedDuplicate.ok()
            && !rejectedDuplicate.project
            && rejectedDuplicate.error.contains(
                QStringLiteral("ambiguous"))
            && duplicateRelation
                == duplicateBefore,
        "repair-relation-clock accepted an ambiguous Relation identity");
}

void testAutomationTraceMappingRepairContracts()
{
    auto broken =
        wave::makeDemonstrationProject();
    wave::ImportedTrace trace;
    trace.id = "trace-local";
    trace.path = "trace.vcd";
    trace.format = "vcd";
    trace.signalMapping = {
        {"lane-request", "tb.req"},
        {"lane-stale", "tb.removed"},
        {"lane-data", ""},
    };
    broken.importedTraces = {trace};
    const auto brokenBefore = broken;

    const auto validation =
        wave::validateProjectForAutomation(broken);
    const auto issues =
        validation.json
            .value(QStringLiteral("issues"))
            .toArray();
    const auto issueForLane =
        [&issues](const QString& laneId) {
            const auto iterator =
                std::find_if(
                    issues.begin(),
                    issues.end(),
                    [&laneId](
                        const QJsonValue& value) {
                        const auto issue =
                            value.toObject();
                        return issue.value(
                                   QStringLiteral(
                                       "code"))
                                       .toString()
                                == QStringLiteral(
                                    "trace-mapping-invalid")
                            && issue.value(
                                   QStringLiteral(
                                       "laneId"))
                                       .toString()
                                == laneId;
                    });
            return iterator == issues.end()
                ? QJsonObject{}
                : iterator->toObject();
        };
    const auto staleIssue =
        issueForLane(
            QStringLiteral("lane-stale"));
    const auto emptySignalIssue =
        issueForLane(
            QStringLiteral("lane-data"));
    const auto staleContext =
        staleIssue.value(
                      QStringLiteral(
                          "traceMappingContext"))
            .toObject();
    const auto emptySignalContext =
        emptySignalIssue.value(
                            QStringLiteral(
                                "traceMappingContext"))
            .toObject();
    const auto traceContext =
        staleIssue.value(
                      QStringLiteral(
                          "traceContext"))
            .toObject();
    const auto traceRef =
        staleIssue.value(
                      QStringLiteral("traceRef"))
            .toString();
    expect(
        validation.ok()
            && !validation.json.value(
                    QStringLiteral("valid"))
                    .toBool()
            && validation.json.value(
                   QStringLiteral(
                       "traceMappingIssueCount"))
                       .toString()
                == QStringLiteral("2")
            && validation.json.value(
                   QStringLiteral("summary"))
                   .toObject()
                   .value(QStringLiteral("errors"))
                   .toString()
                == QStringLiteral("2")
            && validation.json.value(
                   QStringLiteral(
                       "traceMappingSummary"))
                   .toObject()
                   .value(QStringLiteral(
                       "affectedTraceCount"))
                   .toString()
                == QStringLiteral("1")
            && validation.json.value(
                   QStringLiteral(
                       "traceMappingSummary"))
                   .toObject()
                   .value(QStringLiteral(
                       "invalidMappingCount"))
                   .toString()
                == QStringLiteral("2")
            && validation.json.value(
                   QStringLiteral(
                       "traceMappingSummary"))
                   .toObject()
                   .value(QStringLiteral(
                       "missingLaneReferenceCount"))
                   .toString()
                == QStringLiteral("1")
            && validation.json.value(
                   QStringLiteral(
                       "traceMappingSummary"))
                   .toObject()
                   .value(QStringLiteral(
                       "emptyActualSignalIdCount"))
                   .toString()
                == QStringLiteral("1")
            && !traceRef.isEmpty()
            && traceRef
                == emptySignalIssue.value(
                    QStringLiteral("traceRef"))
                       .toString()
            && staleIssue.value(
                   QStringLiteral(
                       "repairOperations"))
                   .toArray()
                   .contains(
                       QStringLiteral(
                           "repair-trace-mapping"))
            && staleIssue.value(
                   QStringLiteral(
                       "repairProperties"))
                   .toArray()
                   .contains(
                       QStringLiteral(
                           "signal-mapping"))
            && traceContext.value(
                   QStringLiteral(
                       "traceIdMatchCount"))
                   .toInt()
                == 1
            && traceContext.value(
                   QStringLiteral(
                       "mappingCount"))
                   .toInt()
                == 3
            && staleContext.value(
                   QStringLiteral(
                       "expectedLaneMatchCount"))
                   .toInt()
                == 0
            && !staleContext.value(
                    QStringLiteral(
                        "expectedLaneReferenceValid"))
                    .toBool()
            && staleContext.value(
                   QStringLiteral(
                       "actualSignalIdPresent"))
                   .toBool()
            && staleContext.value(
                   QStringLiteral("problems"))
                   .toArray()
                   .contains(
                       QStringLiteral(
                           "expected-lane-not-found"))
            && staleContext.value(
                   QStringLiteral(
                       "repairAction"))
                   .toString()
                == QStringLiteral(
                    "remove-invalid-mapping")
            && staleContext.value(
                   QStringLiteral("repairable"))
                   .toBool()
            && emptySignalContext.value(
                   QStringLiteral(
                       "expectedLaneMatchCount"))
                   .toInt()
                == 1
            && emptySignalContext.value(
                   QStringLiteral(
                       "expectedLaneReferenceValid"))
                   .toBool()
            && !emptySignalContext.value(
                    QStringLiteral(
                        "actualSignalIdPresent"))
                    .toBool()
            && emptySignalContext.value(
                   QStringLiteral("problems"))
                   .toArray()
                   .contains(
                       QStringLiteral(
                           "empty-actual-signal-id")),
        "validate did not expose actionable Imported Trace mapping damage");

    const auto summaryInspect =
        wave::inspectProjectForAutomation(
            broken,
            std::nullopt,
            wave::AutomationInspectDetail::Summary);
    const auto fullInspect =
        wave::inspectProjectForAutomation(
            broken,
            std::nullopt,
            wave::AutomationInspectDetail::Full);
    const auto summaryTrace =
        summaryInspect.json
            .value(QStringLiteral("project"))
            .toObject()
            .value(QStringLiteral(
                "importedTraces"))
            .toArray()
            .at(0)
            .toObject();
    const auto fullTrace =
        fullInspect.json
            .value(QStringLiteral("project"))
            .toObject()
            .value(QStringLiteral(
                "importedTraces"))
            .toArray()
            .at(0)
            .toObject();
    expect(
        !summaryTrace.contains(
            QStringLiteral("signalMapping"))
            && summaryTrace.value(
                   QStringLiteral("mappingCount"))
                   .toInt()
                == 3
            && fullTrace.value(
                   QStringLiteral("traceRef"))
                   .toString()
                == traceRef
            && fullTrace.value(
                   QStringLiteral("signalMapping"))
                   .toObject()
                   .value(
                       QStringLiteral("lane-stale"))
                   .toString()
                == QStringLiteral("tb.removed"),
        "full inspect did not expose Imported Trace mappings compactly");

    wave::CommandStack commandStack;
    auto commandProject = broken;
    expect(
        commandStack.execute(
            std::make_unique<
                wave::RemoveTraceMappingAtIndexCommand>(
                commandProject,
                0,
                commandProject
                    .importedTraces.front(),
                "lane-stale"))
            && !commandProject
                    .importedTraces.front()
                    .signalMapping.contains(
                        "lane-stale"),
        "trace mapping command did not remove its selected mapping");
    expect(
        commandStack.undo()
            && commandProject == broken
            && commandStack.redo()
            && !commandProject
                    .importedTraces.front()
                    .signalMapping.contains(
                        "lane-stale"),
        "trace mapping command did not preserve exact Undo/Redo");

    const QJsonObject batch{
        {QStringLiteral("schema"),
         QString::fromLatin1(
             wave::AutomationBatchSchema)},
        {QStringLiteral("scenarioId"),
         QString::fromStdString(
             broken.scenarios.front().id)},
        {QStringLiteral("operations"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral(
                      "repair-trace-mapping")},
                 {QStringLiteral("traceRef"),
                  traceRef},
                 {QStringLiteral("laneId"),
                  QStringLiteral("lane-stale")},
             },
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral(
                      "repair-trace-mapping")},
                 {QStringLiteral("traceId"),
                  QStringLiteral("trace-local")},
                 {QStringLiteral("laneId"),
                  QStringLiteral("lane-data")},
             },
         }},
    };
    auto staleReferenceBatch = batch;
    auto staleReferenceOperations =
        staleReferenceBatch.value(
            QStringLiteral("operations"))
            .toArray();
    auto staleReferenceOperation =
        staleReferenceOperations.at(1)
            .toObject();
    staleReferenceOperation.remove(
        QStringLiteral("traceId"));
    staleReferenceOperation.insert(
        QStringLiteral("traceRef"),
        traceRef);
    staleReferenceOperations.replace(
        1, staleReferenceOperation);
    staleReferenceBatch.insert(
        QStringLiteral("operations"),
        staleReferenceOperations);
    const auto rejectedStaleReference =
        wave::applyAutomationBatch(
            broken, staleReferenceBatch);
    expect(
        !rejectedStaleReference.ok()
            && !rejectedStaleReference.project
            && rejectedStaleReference
                   .failedOperation
                == 1
            && rejectedStaleReference.error.contains(
                QStringLiteral("stale"))
            && broken == brokenBefore,
        "Trace repair reference did not reject a changed trace snapshot atomically");

    const auto repaired =
        wave::applyAutomationBatch(
            broken, batch);
    const auto operationReports =
        repaired.json.value(
            QStringLiteral("operations"))
            .toArray();
    const auto firstReport =
        operationReports.at(0).toObject();
    const auto secondReport =
        operationReports.at(1).toObject();
    expect(
        repaired.ok()
            && repaired.project
            && repaired.changed
            && repaired.json.value(
                   QStringLiteral(
                       "validationGuard"))
                   .toObject()
                   .value(QStringLiteral(
                       "sourceErrorCount"))
                   .toString()
                == QStringLiteral("2")
            && repaired.json.value(
                   QStringLiteral(
                       "validationGuard"))
                   .toObject()
                   .value(QStringLiteral(
                       "candidateErrorCount"))
                   .toString()
                == QStringLiteral("0")
            && repaired.json.value(
                   QStringLiteral("changes"))
                   .toObject()
                   .value(QStringLiteral(
                       "importedTracesChanged"))
                   .toBool()
            && firstReport.value(
                   QStringLiteral(
                       "selectedByRepairRef"))
                   .toBool()
            && firstReport.value(
                   QStringLiteral(
                       "removedTraceMappingCount"))
                   .toInt()
                == 1
            && firstReport.value(
                   QStringLiteral(
                       "updatedTraceCount"))
                   .toInt()
                == 1
            && firstReport.value(
                   QStringLiteral(
                       "beforeTraceMappingContext"))
                   .toObject()
                   .value(QStringLiteral(
                       "repairAction"))
                   .toString()
                == QStringLiteral(
                    "remove-invalid-mapping")
            && secondReport.value(
                   QStringLiteral(
                       "removedTraceMappingCount"))
                   .toInt()
                == 1
            && repaired.project
                   ->importedTraces.front()
                   .signalMapping.size()
                == 1
            && repaired.project
                   ->importedTraces.front()
                   .signalMapping.at(
                       "lane-request")
                == "tb.req"
            && wave::validateProjectForAutomation(
                   *repaired.project)
                   .ok()
            && broken == brokenBefore,
        "repair-trace-mapping did not remove only invalid mappings");

    const QJsonObject healthyBatch{
        {QStringLiteral("schema"),
         QString::fromLatin1(
             wave::AutomationBatchSchema)},
        {QStringLiteral("scenarioId"),
         QString::fromStdString(
             broken.scenarios.front().id)},
        {QStringLiteral("operations"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral(
                      "repair-trace-mapping")},
                 {QStringLiteral("traceId"),
                  QStringLiteral("trace-local")},
                 {QStringLiteral("laneId"),
                  QStringLiteral(
                      "lane-request")},
             },
         }},
    };
    const auto healthyBefore =
        *repaired.project;
    const auto rejectedHealthy =
        wave::applyAutomationBatch(
            healthyBefore, healthyBatch);
    expect(
        !rejectedHealthy.ok()
            && !rejectedHealthy.project
            && rejectedHealthy.error.contains(
                QStringLiteral(
                    "not reported by validate"))
            && *repaired.project
                == healthyBefore,
        "repair-trace-mapping removed a valid mapping");

    auto duplicateTraceId = broken;
    auto secondTrace = trace;
    secondTrace.signalMapping = {
        {"lane-request", "tb.other_req"},
    };
    duplicateTraceId.importedTraces.push_back(
        std::move(secondTrace));
    const auto duplicateBefore =
        duplicateTraceId;
    const auto duplicateValidation =
        wave::validateProjectForAutomation(
            duplicateTraceId);
    const auto duplicateIssues =
        duplicateValidation.json
            .value(QStringLiteral("issues"))
            .toArray();
    const auto duplicateIssue =
        std::find_if(
            duplicateIssues.begin(),
            duplicateIssues.end(),
            [](const QJsonValue& value) {
                const auto issue =
                    value.toObject();
                return issue.value(
                           QStringLiteral("code"))
                               .toString()
                        == QStringLiteral(
                            "trace-mapping-invalid")
                    && issue.value(
                           QStringLiteral("laneId"))
                               .toString()
                        == QStringLiteral(
                            "lane-stale");
            });
    expect(
        duplicateIssue
            != duplicateIssues.end(),
        "duplicate trace fixture omitted its mapping issue");
    const auto duplicateTraceRef =
        duplicateIssue->toObject()
            .value(QStringLiteral("traceRef"))
            .toString();
    const QJsonObject duplicateIdBatch{
        {QStringLiteral("schema"),
         QString::fromLatin1(
             wave::AutomationBatchSchema)},
        {QStringLiteral("scenarioId"),
         QString::fromStdString(
             duplicateTraceId
                 .scenarios.front().id)},
        {QStringLiteral("operations"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral(
                      "repair-trace-mapping")},
                 {QStringLiteral("traceId"),
                  QStringLiteral("trace-local")},
                 {QStringLiteral("laneId"),
                  QStringLiteral("lane-stale")},
             },
         }},
    };
    auto duplicateRefBatch =
        duplicateIdBatch;
    auto duplicateRefOperations =
        duplicateRefBatch.value(
            QStringLiteral("operations"))
            .toArray();
    auto duplicateRefOperation =
        duplicateRefOperations.at(0)
            .toObject();
    duplicateRefOperation.remove(
        QStringLiteral("traceId"));
    duplicateRefOperation.insert(
        QStringLiteral("traceRef"),
        duplicateTraceRef);
    duplicateRefOperations.replace(
        0, duplicateRefOperation);
    duplicateRefBatch.insert(
        QStringLiteral("operations"),
        duplicateRefOperations);
    const auto rejectedAmbiguous =
        wave::applyAutomationBatch(
            duplicateTraceId,
            duplicateIdBatch);
    const auto repairedByRef =
        wave::applyAutomationBatch(
            duplicateTraceId,
            duplicateRefBatch);
    expect(
        !rejectedAmbiguous.ok()
            && !rejectedAmbiguous.project
            && rejectedAmbiguous.error.contains(
                QStringLiteral("ambiguous"))
            && repairedByRef.ok()
            && repairedByRef.project
            && !repairedByRef.project
                    ->importedTraces.at(0)
                    .signalMapping.contains(
                        "lane-stale")
            && repairedByRef.project
                   ->importedTraces.at(1)
                   .signalMapping.at(
                       "lane-request")
                == "tb.other_req"
            && duplicateTraceId
                == duplicateBefore,
        "traceRef did not safely disambiguate duplicate Imported Trace IDs");
}

void testAutomationTraceIdentityRepairContracts()
{
    auto broken =
        wave::makeDemonstrationProject();
    wave::ImportedTrace first;
    first.id = "trace-duplicate";
    first.path = "first.vcd";
    first.format = "vcd";
    first.signalMapping = {
        {"lane-request", "tb.first.req"},
    };
    first.extensions = {
        {"privateTraceState", R"({"source":"first"})"},
    };
    auto second = first;
    second.path = "second.csv";
    second.format = "csv";
    second.offset = 12'000;
    second.signalMapping = {
        {"lane-request", "second.req"},
    };
    second.extensions = {
        {"privateTraceState", R"({"source":"second"})"},
    };
    broken.importedTraces = {first, second};
    const auto brokenBefore = broken;

    const auto validation =
        wave::validateProjectForAutomation(broken);
    const auto issues =
        validation.json
            .value(QStringLiteral("issues"))
            .toArray();
    std::vector<QJsonObject> traceIdentityIssues;
    for (const auto& value : issues) {
        const auto issue = value.toObject();
        if (issue.value(
                QStringLiteral("objectKind")).toString()
                == QStringLiteral("imported-trace")
            && issue.value(
                   QStringLiteral("code")).toString()
                == QStringLiteral(
                    "duplicate-stable-id")) {
            traceIdentityIssues.push_back(issue);
        }
    }
    expect(
        validation.ok()
            && !validation.json.value(
                    QStringLiteral("valid")).toBool()
            && validation.json.value(
                   QStringLiteral(
                       "identityIssueCount"))
                   .toString()
                == QStringLiteral("2")
            && validation.json.value(
                   QStringLiteral(
                       "repairableIssueCount"))
                   .toString()
                == QStringLiteral("2")
            && validation.json.value(
                   QStringLiteral(
                       "repairTargetCount"))
                   .toString()
                == QStringLiteral("2")
            && traceIdentityIssues.size() == 2
            && traceIdentityIssues.at(0)
                   .value(QStringLiteral("path"))
                   .toString()
                == QStringLiteral(
                    "importedTraces[0].id")
            && traceIdentityIssues.at(1)
                   .value(QStringLiteral("path"))
                   .toString()
                == QStringLiteral(
                    "importedTraces[1].id")
            && traceIdentityIssues.at(0)
                   .value(QStringLiteral("traceRef"))
                   .toString()
                != traceIdentityIssues.at(1)
                       .value(QStringLiteral("traceRef"))
                       .toString()
            && traceIdentityIssues.at(1)
                   .value(QStringLiteral(
                       "repairOperations"))
                   .toArray()
                   .contains(
                       QStringLiteral(
                           "repair-trace-identity"))
            && traceIdentityIssues.at(1)
                   .value(QStringLiteral(
                       "traceContext"))
                   .toObject()
                   .value(QStringLiteral(
                       "traceIdMatchCount"))
                   .toInt()
                == 2,
        "validate did not expose distinct repair references for duplicate Imported Trace IDs");

    wave::CommandStack commandStack;
    auto commandProject = broken;
    const auto commandBefore = commandProject;
    expect(
        commandStack.execute(
            std::make_unique<
                wave::ChangeTraceIdentityAtIndexCommand>(
                commandProject,
                1,
                commandProject
                    .importedTraces.at(1),
                "trace-second"))
            && commandProject
                   .importedTraces.at(0).id
                == "trace-duplicate"
            && commandProject
                   .importedTraces.at(1).id
                == "trace-second",
        "trace identity command did not update only its indexed snapshot");
    const auto commandAfter = commandProject;
    expect(
        commandStack.undo()
            && commandProject == commandBefore
            && commandStack.redo()
            && commandProject == commandAfter,
        "trace identity command did not preserve exact Undo/Redo");

    const auto secondTraceRef =
        traceIdentityIssues.at(1)
            .value(QStringLiteral("traceRef"))
            .toString();
    const QJsonObject repairBatch{
        {QStringLiteral("schema"),
         QString::fromLatin1(
             wave::AutomationBatchSchema)},
        {QStringLiteral("scenarioId"),
         QString::fromStdString(
             broken.scenarios.front().id)},
        {QStringLiteral("operations"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral(
                      "repair-trace-identity")},
                 {QStringLiteral("traceRef"),
                  secondTraceRef},
             },
         }},
    };
    const auto repaired =
        wave::applyAutomationBatch(
            broken, repairBatch);
    const auto repeated =
        wave::applyAutomationBatch(
            broken, repairBatch);
    const auto report =
        repaired.json
            .value(QStringLiteral("operations"))
            .toArray()
            .at(0)
            .toObject();
    expect(
        repaired.ok()
            && repaired.project
            && repeated.ok()
            && repeated.project
            && repaired.changed
            && report.value(
                   QStringLiteral("generatedId"))
                   .toBool()
            && report.value(
                   QStringLiteral(
                       "selectedByRepairRef"))
                   .toBool()
            && report.value(
                   QStringLiteral(
                       "beforeTraceId"))
                   .toString()
                == QStringLiteral(
                    "trace-duplicate")
            && report.value(
                   QStringLiteral(
                       "beforeTraceIdMatchCount"))
                   .toInt()
                == 2
            && report.value(
                   QStringLiteral(
                       "updatedTraceCount"))
                   .toInt()
                == 1
            && report.value(
                   QStringLiteral(
                       "repairedIdentity"))
                   .toBool()
            && repaired.project
                   ->importedTraces.at(0)
                == first
            && repaired.project
                   ->importedTraces.at(1).id
                   .starts_with("trace-auto-")
            && repaired.project
                   ->importedTraces.at(1).path
                == second.path
            && repaired.project
                   ->importedTraces.at(1).format
                == second.format
            && repaired.project
                   ->importedTraces.at(1).offset
                == second.offset
            && repaired.project
                   ->importedTraces.at(1)
                   .signalMapping
                == second.signalMapping
            && repaired.project
                   ->importedTraces.at(1)
                   .extensions
                == second.extensions
            && wave::serializeProject(
                   *repaired.project)
                == wave::serializeProject(
                    *repeated.project)
            && wave::validateProjectForAutomation(
                   *repaired.project)
                   .json.value(
                       QStringLiteral(
                           "identityIssueCount"))
                   .toString()
                == QStringLiteral("0")
            && broken == brokenBefore,
        "repair-trace-identity was not deterministic or changed unrelated Trace state");

    auto staleProject = broken;
    staleProject.importedTraces.at(1).offset += 1;
    const auto staleBefore = staleProject;
    const auto rejectedStale =
        wave::applyAutomationBatch(
            staleProject, repairBatch);
    expect(
        !rejectedStale.ok()
            && !rejectedStale.project
            && rejectedStale.error.contains(
                QStringLiteral("stale"))
            && staleProject == staleBefore,
        "repair-trace-identity accepted a stale Trace snapshot");

    auto missing = broken;
    missing.importedTraces.resize(1);
    missing.importedTraces.front().id.clear();
    const auto missingValidation =
        wave::validateProjectForAutomation(
            missing);
    const auto missingIssues =
        missingValidation.json
            .value(QStringLiteral("issues"))
            .toArray();
    const auto missingIssue =
        std::find_if(
            missingIssues.begin(),
            missingIssues.end(),
            [](const QJsonValue& value) {
                const auto issue =
                    value.toObject();
                return issue.value(
                           QStringLiteral(
                               "objectKind"))
                               .toString()
                        == QStringLiteral(
                            "imported-trace")
                    && issue.value(
                           QStringLiteral("code"))
                               .toString()
                        == QStringLiteral(
                            "missing-stable-id");
            });
    expect(
        missingIssue != missingIssues.end(),
        "validate omitted an empty Imported Trace identity");
    const QJsonObject customBatch{
        {QStringLiteral("schema"),
         QString::fromLatin1(
             wave::AutomationBatchSchema)},
        {QStringLiteral("scenarioId"),
         QString::fromStdString(
             missing.scenarios.front().id)},
        {QStringLiteral("operations"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral(
                      "repair-trace-identity")},
                 {QStringLiteral("traceRef"),
                  missingIssue->toObject()
                      .value(QStringLiteral(
                          "traceRef"))},
                 {QStringLiteral("newId"),
                  QStringLiteral(
                      "trace-restored")},
             },
         }},
    };
    const auto custom =
        wave::applyAutomationBatch(
            missing, customBatch);
    expect(
        custom.ok()
            && custom.project
            && custom.project
                   ->importedTraces.front().id
                == "trace-restored"
            && !custom.json
                    .value(QStringLiteral(
                        "operations"))
                    .toArray()
                    .at(0)
                    .toObject()
                    .value(QStringLiteral(
                        "generatedId"))
                    .toBool(),
        "repair-trace-identity did not accept an explicit unique identity");

    const auto healthyBefore =
        *repaired.project;
    const QJsonObject healthyBatch{
        {QStringLiteral("schema"),
         QString::fromLatin1(
             wave::AutomationBatchSchema)},
        {QStringLiteral("scenarioId"),
         QString::fromStdString(
             healthyBefore.scenarios.front().id)},
        {QStringLiteral("operations"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral(
                      "repair-trace-identity")},
                 {QStringLiteral("traceRef"),
                  report.value(
                      QStringLiteral("traceRef"))},
             },
         }},
    };
    const auto rejectedHealthy =
        wave::applyAutomationBatch(
            healthyBefore, healthyBatch);
    expect(
        !rejectedHealthy.ok()
            && !rejectedHealthy.project
            && rejectedHealthy.error.contains(
                QStringLiteral(
                    "not reported by validate"))
            && *repaired.project
                == healthyBefore,
        "repair-trace-identity accepted a healthy Trace identity");
}

void testAutomationTraceReferenceRepairContracts()
{
    auto broken =
        wave::makeDemonstrationProject();
    wave::ImportedTrace trace;
    trace.id = "trace-source";
    trace.path = "   ";
    trace.format = "fst";
    trace.offset = 23'000;
    trace.signalMapping = {
        {"lane-request", "tb.req"},
    };
    trace.extensions = {
        {"privateTraceState",
         R"({"parser":"external"})"},
    };
    broken.importedTraces = {trace};
    const auto brokenBefore = broken;

    const auto validation =
        wave::validateProjectForAutomation(broken);
    const auto issues =
        validation.json
            .value(QStringLiteral("issues"))
            .toArray();
    const auto issueIterator =
        std::find_if(
            issues.begin(),
            issues.end(),
            [](const QJsonValue& value) {
                return value.toObject()
                           .value(
                               QStringLiteral(
                                   "code"))
                           .toString()
                    == QStringLiteral(
                        "trace-reference-invalid");
            });
    expect(
        issueIterator != issues.end(),
        "validate omitted an invalid Imported Trace reference");
    const auto issue =
        issueIterator->toObject();
    const auto traceRef =
        issue.value(
            QStringLiteral("traceRef")).toString();
    const auto context =
        issue.value(
            QStringLiteral(
                "traceReferenceContext"))
            .toObject();
    const auto summary =
        validation.json
            .value(QStringLiteral(
                "traceReferenceSummary"))
            .toObject();
    expect(
        validation.ok()
            && !validation.json.value(
                    QStringLiteral("valid")).toBool()
            && validation.json.value(
                   QStringLiteral(
                       "traceReferenceIssueCount"))
                   .toString()
                == QStringLiteral("1")
            && issue.value(
                   QStringLiteral(
                       "objectKind")).toString()
                == QStringLiteral(
                    "imported-trace-reference")
            && issue.value(
                   QStringLiteral("path")).toString()
                == QStringLiteral(
                    "importedTraces[0].path")
            && issue.value(
                   QStringLiteral("paths"))
                   .toArray()
                == QJsonArray{
                       QStringLiteral(
                           "importedTraces[0].path"),
                       QStringLiteral(
                           "importedTraces[0].format"),
                   }
            && issue.value(
                   QStringLiteral("problems"))
                   .toArray()
                == QJsonArray{
                       QStringLiteral("empty-path"),
                       QStringLiteral(
                           "unsupported-format"),
                   }
            && issue.value(
                   QStringLiteral(
                       "repairProperties"))
                   .toArray()
                == QJsonArray{
                       QStringLiteral("path"),
                       QStringLiteral("format"),
                   }
            && issue.value(
                   QStringLiteral(
                       "repairOperations"))
                   .toArray()
                   .contains(
                       QStringLiteral(
                           "repair-trace-reference"))
            && !traceRef.isEmpty()
            && !context.value(
                    QStringLiteral(
                        "pathPresent")).toBool()
            && !context.value(
                    QStringLiteral(
                        "formatSupported")).toBool()
            && context.value(
                   QStringLiteral(
                       "normalizedFormat")).toString()
                == QStringLiteral("fst")
            && context.value(
                   QStringLiteral(
                       "supportedFormats"))
                   .toArray()
                == QJsonArray{
                       QStringLiteral("vcd"),
                       QStringLiteral("csv"),
                   }
            && context.value(
                   QStringLiteral(
                       "filesystemVerification"))
                   .toString()
                == QStringLiteral(
                    "not-performed")
            && summary.value(
                   QStringLiteral(
                       "affectedTraceCount"))
                   .toString()
                == QStringLiteral("1")
            && summary.value(
                   QStringLiteral(
                       "emptyPathCount"))
                   .toString()
                == QStringLiteral("1")
            && summary.value(
                   QStringLiteral(
                       "unsupportedFormatCount"))
                   .toString()
                == QStringLiteral("1"),
        "invalid Imported Trace reference diagnostics are incomplete");

    wave::CommandStack commandStack;
    auto commandProject = broken;
    const auto commandBefore = commandProject;
    expect(
        commandStack.execute(
            std::make_unique<
                wave::ChangeTraceSourceAtIndexCommand>(
                commandProject,
                0,
                commandProject
                    .importedTraces.front(),
                "capture.vcd",
                "VCD"))
            && commandProject
                   .importedTraces.front().path
                == "capture.vcd"
            && commandProject
                   .importedTraces.front().format
                == "VCD",
        "trace source command did not replace only the indexed source");
    const auto commandAfter = commandProject;
    expect(
        commandStack.undo()
            && commandProject == commandBefore
            && commandStack.redo()
            && commandProject == commandAfter,
        "trace source command did not preserve exact Undo/Redo");

    const QJsonObject repairBatch{
        {QStringLiteral("schema"),
         QString::fromLatin1(
             wave::AutomationBatchSchema)},
        {QStringLiteral("scenarioId"),
         QString::fromStdString(
             broken.scenarios.front().id)},
        {QStringLiteral("operations"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral(
                      "repair-trace-reference")},
                 {QStringLiteral("traceRef"),
                  traceRef},
                 {QStringLiteral("path"),
                  QStringLiteral(
                      "traces/capture.vcd")},
                 {QStringLiteral("format"),
                  QStringLiteral("VCD")},
             },
         }},
    };
    const auto repaired =
        wave::applyAutomationBatch(
            broken, repairBatch);
    const auto repeated =
        wave::applyAutomationBatch(
            broken, repairBatch);
    expect(
        repaired.ok()
            && repaired.project
            && repeated.ok()
            && repeated.project,
        "repair-trace-reference rejected a complete explicit repair");
    const auto report =
        repaired.json
            .value(QStringLiteral("operations"))
            .toArray()
            .at(0)
            .toObject();
    const auto& repairedTrace =
        repaired.project
            ->importedTraces.front();
    expect(
        repaired.changed
            && report.value(
                   QStringLiteral(
                       "selectedByRepairRef"))
                   .toBool()
            && report.value(
                   QStringLiteral(
                       "beforeTraceRef"))
                   .toString()
                == traceRef
            && report.value(
                   QStringLiteral(
                       "changedProperties"))
                   .toArray()
                == QJsonArray{
                       QStringLiteral("path"),
                       QStringLiteral("format"),
                   }
            && report.value(
                   QStringLiteral(
                       "updatedTraceCount"))
                   .toInt()
                == 1
            && report.value(
                   QStringLiteral(
                       "repairedTraceReference"))
                   .toBool()
            && report.value(
                   QStringLiteral(
                       "traceReferenceContext"))
                   .toObject()
                   .value(QStringLiteral(
                       "structurallyValid"))
                   .toBool()
            && repairedTrace.path
                == "traces/capture.vcd"
            && repairedTrace.format == "vcd"
            && repairedTrace.id == trace.id
            && repairedTrace.offset
                == trace.offset
            && repairedTrace.signalMapping
                == trace.signalMapping
            && repairedTrace.extensions
                == trace.extensions
            && wave::serializeProject(
                   *repaired.project)
                == wave::serializeProject(
                    *repeated.project)
            && wave::validateProjectForAutomation(
                   *repaired.project)
                   .json.value(
                       QStringLiteral(
                           "traceReferenceIssueCount"))
                   .toString()
                == QStringLiteral("0")
            && broken == brokenBefore,
        "repair-trace-reference was not deterministic or changed unrelated Trace state");

    auto staleProject = broken;
    staleProject.importedTraces.front()
        .offset += 1;
    const auto staleBefore = staleProject;
    const auto rejectedStale =
        wave::applyAutomationBatch(
            staleProject, repairBatch);
    expect(
        !rejectedStale.ok()
            && !rejectedStale.project
            && rejectedStale.error.contains(
                QStringLiteral("stale"))
            && staleProject == staleBefore,
        "repair-trace-reference accepted a stale Trace snapshot");

    auto partialBatch = repairBatch;
    auto partialOperations =
        partialBatch.value(
            QStringLiteral("operations"))
            .toArray();
    auto partialOperation =
        partialOperations.at(0).toObject();
    partialOperation.remove(
        QStringLiteral("format"));
    partialOperations.replace(
        0, partialOperation);
    partialBatch.insert(
        QStringLiteral("operations"),
        partialOperations);
    const auto rejectedPartial =
        wave::applyAutomationBatch(
            broken, partialBatch);
    expect(
        !rejectedPartial.ok()
            && !rejectedPartial.project
            && rejectedPartial.error.contains(
                QStringLiteral(
                    "remains invalid"))
            && broken == brokenBefore,
        "repair-trace-reference accepted an incomplete repair");

    auto unsupportedBatch = repairBatch;
    auto unsupportedOperations =
        unsupportedBatch.value(
            QStringLiteral("operations"))
            .toArray();
    auto unsupportedOperation =
        unsupportedOperations.at(0).toObject();
    unsupportedOperation.insert(
        QStringLiteral("format"),
        QStringLiteral("fst"));
    unsupportedOperations.replace(
        0, unsupportedOperation);
    unsupportedBatch.insert(
        QStringLiteral("operations"),
        unsupportedOperations);
    const auto rejectedUnsupported =
        wave::applyAutomationBatch(
            broken, unsupportedBatch);
    expect(
        !rejectedUnsupported.ok()
            && !rejectedUnsupported.project
            && rejectedUnsupported.error.contains(
                QStringLiteral("VCD or CSV"))
            && broken == brokenBefore,
        "repair-trace-reference accepted an unsupported format");

    const auto healthyBefore =
        *repaired.project;
    const QJsonObject healthyBatch{
        {QStringLiteral("schema"),
         QString::fromLatin1(
             wave::AutomationBatchSchema)},
        {QStringLiteral("scenarioId"),
         QString::fromStdString(
             healthyBefore
                 .scenarios.front().id)},
        {QStringLiteral("operations"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral(
                      "repair-trace-reference")},
                 {QStringLiteral("traceRef"),
                  report.value(
                      QStringLiteral(
                          "traceRef"))},
                 {QStringLiteral("path"),
                  QStringLiteral(
                      "traces/other.vcd")},
             },
         }},
    };
    const auto rejectedHealthy =
        wave::applyAutomationBatch(
            healthyBefore, healthyBatch);
    expect(
        !rejectedHealthy.ok()
            && !rejectedHealthy.project
            && rejectedHealthy.error.contains(
                QStringLiteral(
                    "not reported by validate"))
            && *repaired.project
                == healthyBefore,
        "repair-trace-reference accepted a healthy Trace reference");
}

void testAutomationPostEditValidationGuardContracts()
{
    auto broken = wave::makeDemonstrationProject();
    auto& scenario = broken.scenarios.front();
    expect(
        !scenario.relations.empty(),
        "post-edit validation fixture has no Relation");
    auto duplicate = scenario.relations.front();
    duplicate.description = "Imported duplicate identity";
    scenario.relations.push_back(std::move(duplicate));
    const auto brokenBefore = broken;

    const QJsonObject unrelatedEdit{
        {QStringLiteral("schema"),
         QString::fromLatin1(wave::AutomationBatchSchema)},
        {QStringLiteral("scenarioId"),
         QString::fromStdString(scenario.id)},
        {QStringLiteral("operations"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("op"),
                  QStringLiteral("rename-lane")},
                 {QStringLiteral("laneId"),
                  QStringLiteral("lane-reset")},
                 {QStringLiteral("name"),
                  QStringLiteral("reset_guard_probe")},
             },
         }},
    };
    const auto rejected =
        wave::applyAutomationBatch(broken, unrelatedEdit);
    const auto rejectedValidation =
        rejected.json
            .value(QStringLiteral("validation"))
            .toObject();
    const auto rejectedError =
        rejected.json
            .value(QStringLiteral("error"))
            .toObject();
    const auto rejectedGuard =
        rejected.json
            .value(QStringLiteral("validationGuard"))
            .toObject();
    expect(
        !rejected.ok()
            && !rejected.project
            && rejected.failedOperation == -1
            && !rejected.changed
            && rejected.error.contains(
                QStringLiteral("Post-edit validation"))
            && !rejected.json.value(
                    QStringLiteral("ok")).toBool()
            && rejected.json.value(
                   QStringLiteral("candidateChanged")).toBool()
            && rejected.json.value(
                   QStringLiteral("operationCount")).toInt()
                == 1
            && rejected.json.value(
                   QStringLiteral("operations")).toArray().size()
                == 1
            && rejectedError.value(
                   QStringLiteral("code")).toString()
                == QStringLiteral("post-validation-failed")
            && !rejectedGuard.value(
                    QStringLiteral("accepted")).toBool()
            && rejectedGuard.value(
                   QStringLiteral("reason")).toString()
                == QStringLiteral("invalid-candidate")
            && rejectedGuard.value(
                   QStringLiteral("noNewErrors")).toBool()
            && rejectedGuard.value(
                   QStringLiteral("newErrorCount")).toString()
                == QStringLiteral("0")
            && rejectedGuard.value(
                   QStringLiteral("resolvedErrorCount")).toString()
                == QStringLiteral("0")
            && !rejectedValidation.value(
                    QStringLiteral("valid")).toBool()
            && rejectedValidation.value(
                   QStringLiteral("identityIssueCount")).toString()
                == QStringLiteral("2")
            && rejectedValidation.value(
                   QStringLiteral("summary")).toObject()
                   .value(QStringLiteral("errors")).toString()
                == QStringLiteral("2")
            && broken == brokenBefore,
        "automation exposed a candidate project that failed structural post-edit validation");

    wave::AutomationRelationQueryOptions queryOptions;
    queryOptions.match = "relation-req-ack";
    queryOptions.exact = true;
    const auto query = wave::findRelationsForAutomation(
        broken, queryOptions, scenario.id);
    const auto rows =
        query.json.value(QStringLiteral("relations")).toArray();
    expect(
        query.ok() && rows.size() == 2,
        "post-edit validation fixture did not expose repair references");
    const auto repairRef =
        rows.at(1).toObject()
            .value(QStringLiteral("relationRef"))
            .toString();
    const auto repaired = wave::applyAutomationBatch(
        broken,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("scenarioId"),
             QString::fromStdString(scenario.id)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"),
                      QStringLiteral("update-relation")},
                     {QStringLiteral("relationRef"), repairRef},
                     {QStringLiteral("newId"),
                      QStringLiteral(
                          "relation-req-ack-follow-up")},
                 },
             }},
        });
    expect(
        repaired.ok()
            && repaired.project
            && repaired.changed
            && repaired.json
                   .value(QStringLiteral("validation"))
                   .toObject()
                   .value(QStringLiteral("valid"))
                   .toBool()
            && repaired.json
                   .value(QStringLiteral("validationGuard"))
                   .toObject()
                   .value(QStringLiteral("reason")).toString()
                == QStringLiteral("valid-candidate"),
        "post-edit validation guard blocked a batch that repaired the complete candidate");

    auto progressivelyBroken = broken;
    auto& progressivelyBrokenScenario =
        progressivelyBroken.scenarios.front();
    progressivelyBrokenScenario.markers.push_back({
        "marker-progressive",
        "Progressive repair A",
        10'000,
        10'000,
        wave::MarkerKind::Point,
        {},
        {},
    });
    progressivelyBrokenScenario.markers.push_back({
        "marker-progressive",
        "Progressive repair B",
        20'000,
        20'000,
        wave::MarkerKind::Point,
        {},
        {},
    });
    const auto progressiveQuery =
        wave::findRelationsForAutomation(
            progressivelyBroken,
            queryOptions,
            progressivelyBrokenScenario.id);
    const auto progressiveRef =
        progressiveQuery.json
            .value(QStringLiteral("relations"))
            .toArray()
            .at(1)
            .toObject()
            .value(QStringLiteral("relationRef"))
            .toString();
    const auto progressive = wave::applyAutomationBatch(
        progressivelyBroken,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("scenarioId"),
             QString::fromStdString(
                 progressivelyBrokenScenario.id)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"),
                      QStringLiteral("update-relation")},
                     {QStringLiteral("relationRef"),
                      progressiveRef},
                     {QStringLiteral("newId"),
                      QStringLiteral(
                          "relation-progressively-repaired")},
                 },
             }},
        });
    const auto progressiveGuard =
        progressive.json
            .value(QStringLiteral("validationGuard"))
            .toObject();
    expect(
        progressive.ok()
            && progressive.project
            && !progressive.json
                    .value(QStringLiteral("validation"))
                    .toObject()
                    .value(QStringLiteral("valid")).toBool()
            && progressiveGuard.value(
                   QStringLiteral("accepted")).toBool()
            && progressiveGuard.value(
                   QStringLiteral("progressiveRepair")).toBool()
            && progressiveGuard.value(
                   QStringLiteral("noNewErrors")).toBool()
            && progressiveGuard.value(
                   QStringLiteral("reason")).toString()
                == QStringLiteral("progressive-repair")
            && progressiveGuard.value(
                   QStringLiteral("sourceErrorCount")).toString()
                == QStringLiteral("4")
            && progressiveGuard.value(
                   QStringLiteral("candidateErrorCount")).toString()
                == QStringLiteral("2")
            && progressiveGuard.value(
                   QStringLiteral("newErrorCount")).toString()
                == QStringLiteral("0")
            && progressiveGuard.value(
                   QStringLiteral("resolvedErrorCount")).toString()
                == QStringLiteral("2"),
        "post-edit validation guard blocked a non-regressive progressive repair");

    auto regressiveRepair = wave::makeDemonstrationProject();
    auto& regressiveScenario =
        regressiveRepair.scenarios.front();
    const auto* originalSourceEvent = wave::findEvent(
        regressiveScenario,
        regressiveScenario.relations.front().sourceEventId);
    expect(
        originalSourceEvent != nullptr,
        "regressive repair fixture has no source Event");
    regressiveScenario.events.push_back(*originalSourceEvent);
    const auto regressiveBefore = regressiveRepair;
    const auto regressive = wave::applyAutomationBatch(
        regressiveRepair,
        QJsonObject{
            {QStringLiteral("schema"),
             QString::fromLatin1(wave::AutomationBatchSchema)},
            {QStringLiteral("scenarioId"),
             QString::fromStdString(regressiveScenario.id)},
            {QStringLiteral("operations"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("op"),
                      QStringLiteral("update-relation")},
                     {QStringLiteral("relationId"),
                      QStringLiteral("relation-req-ack")},
                     {QStringLiteral("sourceLaneId"),
                      QStringLiteral("reset_n")},
                     {QStringLiteral("sourceAt"),
                      QStringLiteral("40 ns")},
                 },
             }},
        });
    const auto regressiveGuard =
        regressive.json
            .value(QStringLiteral("validationGuard"))
            .toObject();
    expect(
        !regressive.ok()
            && !regressive.project
            && regressive.failedOperation == -1
            && regressiveGuard.value(
                   QStringLiteral("repairOperationReported")).toBool()
            && !regressiveGuard.value(
                    QStringLiteral("noNewErrors")).toBool()
            && regressiveGuard.value(
                   QStringLiteral("newErrorCount")).toString()
                == QStringLiteral("1")
            && regressiveGuard.value(
                   QStringLiteral("resolvedErrorCount")).toString()
                == QStringLiteral("1")
            && regressiveRepair == regressiveBefore,
        "post-edit validation guard accepted a repair that traded one error for another");

    auto semanticBroken = wave::makeDemonstrationProject();
    semanticBroken.scenarios.front()
        .relations.front().targetEventId =
        "missing-target-event";
    const auto semanticBefore = semanticBroken;
    const auto semanticRejected =
        wave::applyAutomationBatch(
            semanticBroken, unrelatedEdit);
    const auto semanticValidation =
        semanticRejected.json
            .value(QStringLiteral("validation"))
            .toObject();
    expect(
        !semanticRejected.ok()
            && !semanticRejected.project
            && semanticRejected.json.value(
                   QStringLiteral("candidateChanged")).toBool()
            && semanticValidation.value(
                   QStringLiteral("identityIssueCount")).toString()
                == QStringLiteral("0")
            && semanticValidation.value(
                   QStringLiteral("semanticIssueCount")).toString()
                   .toULongLong()
                > 0
            && semanticValidation.value(
                   QStringLiteral("summary")).toObject()
                   .value(QStringLiteral("errors")).toString()
                   .toULongLong()
                > 0
            && semanticBroken == semanticBefore,
        "automation exposed a candidate project that failed semantic post-edit validation");

    auto crossScenarioBroken =
        wave::makeDemonstrationProject();
    auto secondScenario =
        crossScenarioBroken.scenarios.front();
    secondScenario.id = "scenario-cross-scope";
    secondScenario.name = "Broken secondary scenario";
    secondScenario.markers.push_back({
        {},
        "Missing cross-scope identity",
        10'000,
        10'000,
        wave::MarkerKind::Point,
        {},
        {},
    });
    crossScenarioBroken.scenarios.push_back(
        std::move(secondScenario));
    const auto crossScenarioBefore =
        crossScenarioBroken;
    const auto crossScenarioRejected =
        wave::applyAutomationBatch(
            crossScenarioBroken, unrelatedEdit);
    const auto crossScenarioValidation =
        crossScenarioRejected.json
            .value(QStringLiteral("validation"))
            .toObject();
    const auto crossScenarioGuard =
        crossScenarioRejected.json
            .value(QStringLiteral("validationGuard"))
            .toObject();
    const auto crossScenarioIssues =
        crossScenarioValidation
            .value(QStringLiteral("issues"))
            .toArray();
    const auto crossScenarioIssue =
        std::find_if(
            crossScenarioIssues.begin(),
            crossScenarioIssues.end(),
            [](const QJsonValue& value) {
                return value.toObject()
                           .value(QStringLiteral("path"))
                           .toString()
                    == QStringLiteral(
                        "scenarios[1].markers[0].id");
            });
    expect(
        !crossScenarioRejected.ok()
            && !crossScenarioRejected.project
            && crossScenarioGuard.value(
                   QStringLiteral("scope")).toString()
                == QStringLiteral("project")
            && crossScenarioGuard.value(
                   QStringLiteral("sourceErrorCount")).toString()
                == QStringLiteral("1")
            && crossScenarioGuard.value(
                   QStringLiteral("candidateErrorCount")).toString()
                == QStringLiteral("1")
            && crossScenarioIssue
                != crossScenarioIssues.end()
            && crossScenarioBroken == crossScenarioBefore,
        "post-edit validation guard ignored damage outside the edited Scenario");
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
        {"batch lane cleanup", testBatchLaneCleanup},
        {"lane display reordering", testLaneReordering},
        {"batch lane display reordering", testBatchLaneReordering},
        {"batch lane duplication", testBatchLaneDuplication},
        {"direct lane group assignment", testLaneGroupAssignment},
        {"create group with first signal", testCreateGroupWithLane},
        {"batch lane grouping", testBatchLaneGrouping},
        {"lane values and segment merge/split", testLaneValuesAndSegments},
        {"undo and redo", testUndoRedo},
        {"single-lane sequence command", testLaneSequenceCommand},
        {"multi-lane sequence command", testMultiLaneSequenceCommand},
        {"multi-lane range assignment command", testMultiLaneRangeAssignmentCommand},
        {"command replacement, cancel, and duration", testCommandStackReplacementAndDuration},
        {"scenario duration truncation", testScenarioDurationTruncation},
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
        {"headless automation JSON contracts", testAutomationContracts},
        {"Relation repair reference contracts", testAutomationRelationRepairReferenceContracts},
        {"structural identity validation contracts", testAutomationStructuralIdentityValidationContracts},
        {"Marker integrity validation contracts", testAutomationMarkerIntegrityValidationContracts},
        {"actionable validation reference contracts", testAutomationActionableValidationReferenceContracts},
        {"structured Relation validation contracts", testAutomationStructuredRelationValidationContracts},
        {"structured waveform validation contracts", testAutomationStructuredWaveformValidationContracts},
        {"structured Event validation contracts", testAutomationStructuredEventValidationContracts},
        {"Event waveform link repair contracts", testAutomationEventLinkRepairContracts},
        {"Event cycle repair contracts", testAutomationEventCycleRepairContracts},
        {"Event ClockDomain repair contracts", testAutomationEventClockRepairContracts},
        {"Lane ClockDomain repair contracts", testAutomationLaneClockRepairContracts},
        {"Lane Group repair contracts", testAutomationLaneGroupRepairContracts},
        {"Relation ClockDomain repair contracts", testAutomationRelationClockRepairContracts},
        {"Imported Trace mapping repair contracts", testAutomationTraceMappingRepairContracts},
        {"Imported Trace identity repair contracts", testAutomationTraceIdentityRepairContracts},
        {"Imported Trace reference repair contracts", testAutomationTraceReferenceRepairContracts},
        {"post-edit validation guard contracts", testAutomationPostEditValidationGuardContracts},
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
