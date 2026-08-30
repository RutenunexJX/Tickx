#pragma once

#include "wave/model.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace wave {

class EditCommand {
public:
    virtual ~EditCommand() = default;

    virtual void redo() = 0;
    virtual void undo() = 0;
    [[nodiscard]] virtual std::string description() const = 0;
    [[nodiscard]] virtual bool hasEffect() const noexcept { return true; }
};

class CommandStack {
public:
    bool execute(std::unique_ptr<EditCommand> command);
    void replaceLast(std::unique_ptr<EditCommand> command);
    bool discardLast();
    bool undoLastAfter(std::size_t baseline);
    bool undo();
    bool redo();
    void clear() noexcept;

    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;
    [[nodiscard]] std::string undoDescription() const;
    [[nodiscard]] std::string redoDescription() const;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::uint64_t stateId() const noexcept;

private:
    std::vector<std::unique_ptr<EditCommand>> commands_;
    std::vector<std::uint64_t> stateIds_{0};
    std::size_t cursor_{0};
    std::uint64_t nextStateId_{1};
};

class ChangeScenarioDurationCommand final : public EditCommand {
public:
    ChangeScenarioDurationCommand(Scenario& scenario, Tick duration);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Scenario* scenario_;
    Tick before_;
    Tick after_;
};

struct ScenarioTruncationSummary {
    std::size_t clippedSegmentCount{0};
    std::size_t removedSegmentCount{0};
    std::size_t removedEventCount{0};
    std::size_t removedRelationCount{0};
    std::size_t clippedMarkerCount{0};
    std::size_t removedMarkerCount{0};

    [[nodiscard]] bool changesContent() const noexcept;
};

class TruncateScenarioDurationCommand final : public EditCommand {
public:
    TruncateScenarioDurationCommand(Scenario& scenario, Tick duration);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] const ScenarioTruncationSummary& summary() const noexcept;

private:
    Scenario* scenario_;
    Scenario before_;
    Scenario after_;
    ScenarioTruncationSummary summary_;
};

class SetLaneRangeCommand final : public EditCommand {
public:
    SetLaneRangeCommand(
        Scenario& scenario,
        std::string laneId,
        Tick start,
        Tick end,
        std::string value,
        JsonExtensions extensions = {});

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    Scenario* scenario_;
    std::string laneId_;
    Tick start_;
    Tick end_;
    std::string value_;
    JsonExtensions extensions_;
    std::vector<Segment> before_;
    std::vector<Segment> after_;
    std::vector<Event> eventsBefore_;
    std::vector<Event> eventsAfter_;
    std::vector<Relation> relationsBefore_;
    std::vector<Relation> relationsAfter_;
    bool initialized_{false};
};

struct LaneSequenceStep {
    Tick start{0};
    Tick end{0};
    std::string value;
    JsonExtensions extensions;
    bool preserveExisting{false};

    LaneSequenceStep() = default;
    LaneSequenceStep(
        Tick startValue,
        Tick endValue,
        std::string valueValue,
        JsonExtensions extensionValues = {},
        bool preserve = false)
        : start(startValue)
        , end(endValue)
        , value(std::move(valueValue))
        , extensions(std::move(extensionValues))
        , preserveExisting(preserve)
    {
    }
};

struct LaneSequenceAssignment {
    std::string laneId;
    std::vector<LaneSequenceStep> steps;
};

class SetLaneSequenceCommand final : public EditCommand {
public:
    SetLaneSequenceCommand(
        Scenario& scenario,
        std::string laneId,
        std::vector<LaneSequenceStep> steps);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    Scenario* scenario_;
    std::string laneId_;
    std::vector<LaneSequenceStep> steps_;
    Scenario before_;
    Scenario after_;
};

class SetLaneSequencesCommand final : public EditCommand {
public:
    SetLaneSequencesCommand(
        Scenario& scenario,
        std::vector<LaneSequenceAssignment> assignments);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    Scenario* scenario_;
    std::vector<LaneSequenceAssignment> assignments_;
    Scenario before_;
    Scenario after_;
};

struct LaneRangeAssignment {
    std::string laneId;
    std::string value;
    JsonExtensions extensions;
};

class SetLaneRangesCommand final : public EditCommand {
public:
    SetLaneRangesCommand(
        Scenario& scenario,
        Tick start,
        Tick end,
        std::vector<LaneRangeAssignment> assignments);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    Scenario* scenario_;
    Tick start_;
    Tick end_;
    std::vector<LaneRangeAssignment> assignments_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class ClearLaneRangesCommand final : public EditCommand {
public:
    ClearLaneRangesCommand(
        Scenario& scenario,
        Tick start,
        Tick end,
        std::vector<std::string> laneIds);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    Scenario* scenario_;
    Tick start_;
    Tick end_;
    std::vector<std::string> laneIds_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class ClearLaneRangeCommand final : public EditCommand {
public:
    ClearLaneRangeCommand(
        Scenario& scenario,
        std::string laneId,
        Tick start,
        Tick end);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    Scenario* scenario_;
    std::string laneId_;
    Tick start_;
    Tick end_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class EditSegmentCommand final : public EditCommand {
public:
    EditSegmentCommand(
        Scenario& scenario,
        std::string laneId,
        std::string segmentId,
        Tick start,
        Tick end,
        std::string value,
        std::optional<JsonExtensions> extensions = std::nullopt);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    Scenario* scenario_;
    std::string laneId_;
    std::string segmentId_;
    Tick start_;
    Tick end_;
    std::string value_;
    std::optional<JsonExtensions> extensions_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class CopySegmentCommand final : public EditCommand {
public:
    CopySegmentCommand(
        Scenario& scenario,
        std::string laneId,
        std::string sourceSegmentId,
        Tick start,
        Tick end);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    Scenario* scenario_;
    std::string laneId_;
    std::string value_;
    JsonExtensions extensions_;
    Tick start_;
    Tick end_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class ToggleBitRangeCommand final : public EditCommand {
public:
    ToggleBitRangeCommand(
        Scenario& scenario,
        std::string laneId,
        std::vector<std::pair<Tick, Tick>> beatRanges);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Scenario* scenario_;
    std::string laneId_;
    std::vector<std::pair<Tick, Tick>> beatRanges_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class AddLaneCommand final : public EditCommand {
public:
    AddLaneCommand(
        Scenario& scenario,
        Lane lane,
        std::optional<std::size_t> insertionIndex = std::nullopt);
    AddLaneCommand(
        Project& project,
        Scenario& scenario,
        Lane lane,
        ClockDomain clockDomain,
        std::optional<std::size_t> insertionIndex = std::nullopt);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Project* project_{nullptr};
    Scenario* scenario_;
    Lane lane_;
    std::optional<ClockDomain> clockDomain_;
    std::size_t insertionIndex_{0};
};

class CreateGroupWithLaneCommand final : public EditCommand {
public:
    CreateGroupWithLaneCommand(
        Scenario& scenario,
        Lane group,
        std::string laneId);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Scenario* scenario_;
    std::string laneId_;
    std::vector<Lane> before_;
    std::vector<Lane> after_;
};

class CreateGroupWithLanesCommand final : public EditCommand {
public:
    CreateGroupWithLanesCommand(
        Scenario& scenario,
        Lane group,
        std::vector<std::string> laneIds);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Scenario* scenario_;
    std::vector<std::string> laneIds_;
    std::vector<Lane> before_;
    std::vector<Lane> after_;
};

class DuplicateLaneCommand final : public EditCommand {
public:
    DuplicateLaneCommand(
        Scenario& scenario,
        Lane lane,
        std::size_t insertionIndex);
    DuplicateLaneCommand(
        Project& project,
        Scenario& scenario,
        Lane lane,
        ClockDomain clockDomain,
        std::size_t insertionIndex);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    AddLaneCommand addLaneCommand_;
};

class DuplicateLanesCommand final : public EditCommand {
public:
    DuplicateLanesCommand(
        Project& project,
        Scenario& scenario,
        std::vector<Lane> lanes,
        std::vector<ClockDomain> clockDomains,
        std::size_t insertionIndex);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Project* project_;
    Scenario* scenario_;
    std::vector<Lane> beforeLanes_;
    std::vector<Lane> afterLanes_;
    std::vector<ClockDomain> beforeClockDomains_;
    std::vector<ClockDomain> afterClockDomains_;
    std::size_t duplicateCount_{0};
};

class RemoveLaneCommand final : public EditCommand {
public:
    RemoveLaneCommand(
        Project& project,
        Scenario& scenario,
        std::string laneId);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    struct TraceMapping {
        std::string traceId;
        std::string signalId;
    };

    Project* project_;
    Scenario* scenario_;
    std::string laneId_;
    Scenario beforeScenario_;
    Scenario afterScenario_;
    std::vector<TraceMapping> removedTraceMappings_;
    bool removesGroup_{false};
};

class RemoveLanesCommand final : public EditCommand {
public:
    RemoveLanesCommand(
        Project& project,
        Scenario& scenario,
        std::vector<std::string> laneIds);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    struct TraceMapping {
        std::string traceId;
        std::string laneId;
        std::string signalId;
    };

    Project* project_;
    Scenario* scenario_;
    std::vector<std::string> laneIds_;
    Scenario beforeScenario_;
    Scenario afterScenario_;
    std::vector<TraceMapping> removedTraceMappings_;
};

class RemoveTraceMappingAtIndexCommand final : public EditCommand {
public:
    RemoveTraceMappingAtIndexCommand(
        Project& project,
        std::size_t traceIndex,
        ImportedTrace expected,
        std::string expectedLaneId);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    Project* project_;
    std::size_t traceIndex_;
    ImportedTrace expected_;
    std::string expectedLaneId_;
    std::optional<std::vector<ImportedTrace>> before_;
    std::optional<std::vector<ImportedTrace>> after_;
};

class ChangeTraceIdentityAtIndexCommand final : public EditCommand {
public:
    ChangeTraceIdentityAtIndexCommand(
        Project& project,
        std::size_t traceIndex,
        ImportedTrace expected,
        std::string replacementId);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    Project* project_;
    std::size_t traceIndex_;
    ImportedTrace expected_;
    std::string replacementId_;
    std::optional<std::vector<ImportedTrace>> before_;
    std::optional<std::vector<ImportedTrace>> after_;
};

class ChangeTraceSourceAtIndexCommand final : public EditCommand {
public:
    ChangeTraceSourceAtIndexCommand(
        Project& project,
        std::size_t traceIndex,
        ImportedTrace expected,
        std::string replacementPath,
        std::string replacementFormat);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    Project* project_;
    std::size_t traceIndex_;
    ImportedTrace expected_;
    std::string replacementPath_;
    std::string replacementFormat_;
    std::optional<std::vector<ImportedTrace>> before_;
    std::optional<std::vector<ImportedTrace>> after_;
};

class MoveLaneCommand final : public EditCommand {
public:
    MoveLaneCommand(
        Scenario& scenario,
        std::string laneId,
        std::size_t destinationIndex);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Scenario* scenario_;
    std::string laneId_;
    std::size_t beforeIndex_;
    std::size_t afterIndex_;
    bool movesGroup_{false};
};

class MoveLanesCommand final : public EditCommand {
public:
    MoveLanesCommand(
        Scenario& scenario,
        std::vector<std::string> laneIds,
        std::size_t insertionSlot);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    Scenario* scenario_;
    std::vector<std::string> laneIds_;
    std::vector<Lane> before_;
    std::vector<Lane> after_;
};

class SetLaneGroupCommand final : public EditCommand {
public:
    SetLaneGroupCommand(
        Scenario& scenario,
        std::string laneId,
        std::string groupId);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    Scenario* scenario_;
    std::string laneId_;
    std::string groupId_;
    std::vector<Lane> before_;
    std::vector<Lane> after_;
};

class SetLanesGroupCommand final : public EditCommand {
public:
    SetLanesGroupCommand(
        Scenario& scenario,
        std::vector<std::string> laneIds,
        std::string groupId);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    Scenario* scenario_;
    std::vector<std::string> laneIds_;
    std::string groupId_;
    std::vector<Lane> before_;
    std::vector<Lane> after_;
};

class ChangeLaneCommand final : public EditCommand {
public:
    ChangeLaneCommand(
        const Project& project,
        Scenario& scenario,
        std::string laneId,
        Lane replacement);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    Scenario* scenario_;
    std::string laneId_;
    Lane before_;
    Lane after_;
};

class RepairLaneClockReferenceCommand final : public EditCommand {
public:
    RepairLaneClockReferenceCommand(
        const Project& project,
        Scenario& scenario,
        std::string laneId);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    const Project* project_;
    Scenario* scenario_;
    std::string laneId_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class RepairLaneGroupReferenceCommand final : public EditCommand {
public:
    RepairLaneGroupReferenceCommand(
        Scenario& scenario,
        std::string laneId);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    Scenario* scenario_;
    std::string laneId_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class HideLaneCommand final : public EditCommand {
public:
    HideLaneCommand(Scenario& scenario, std::string laneId);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    Scenario* scenario_;
    std::string laneId_;
    bool wasVisible_{false};
    bool hidesGroup_{false};
};

class HideLanesCommand final : public EditCommand {
public:
    HideLanesCommand(
        Scenario& scenario,
        std::vector<std::string> laneIds);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    struct Visibility {
        std::string laneId;
        bool visible{false};
    };

    Scenario* scenario_;
    std::vector<Visibility> before_;
};

class ShowLaneCommand final : public EditCommand {
public:
    ShowLaneCommand(Scenario& scenario, std::string laneId);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    Scenario* scenario_;
    std::string laneId_;
    bool wasVisible_{true};
    bool showsGroup_{false};
};

class ShowHiddenLanesCommand final : public EditCommand {
public:
    explicit ShowHiddenLanesCommand(Scenario& scenario);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    Scenario* scenario_;
    std::vector<std::string> laneIds_;
};

class ChangeClockCommand final : public EditCommand {
public:
    ChangeClockCommand(
        Project& project,
        Scenario& scenario,
        std::string clockId,
        ClockDomain replacement);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    Project* project_;
    std::string clockId_;
    ClockDomain beforeClock_;
    ClockDomain afterClock_;
    std::vector<Scenario> beforeScenarios_;
    std::vector<Scenario> afterScenarios_;
};

struct CopiedLaneRange {
    CopiedLaneRange() = default;
    CopiedLaneRange(
        std::string destinationLaneId,
        std::vector<Segment> segments,
        std::string source = {})
        : laneId(std::move(destinationLaneId))
        , relativeSegments(std::move(segments))
        , sourceLaneId(std::move(source))
    {
    }

    // laneId is the destination. An empty sourceLaneId means the same lane.
    std::string laneId;
    std::vector<Segment> relativeSegments;
    std::string sourceLaneId;
};

enum class RangeTransferMode {
    Move,
    Copy,
};

class TransferRangeCommand final : public EditCommand {
public:
    TransferRangeCommand(
        Scenario& scenario,
        std::vector<CopiedLaneRange> lanes,
        Tick source,
        Tick destination,
        Tick duration,
        RangeTransferMode mode);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    Scenario* scenario_;
    std::vector<CopiedLaneRange> lanes_;
    Tick source_;
    Tick destination_;
    Tick duration_;
    RangeTransferMode mode_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class PasteRangeCommand final : public EditCommand {
public:
    PasteRangeCommand(
        Scenario& scenario,
        std::vector<CopiedLaneRange> lanes,
        Tick destination,
        Tick duration,
        std::string description = "Paste range");

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    Scenario* scenario_;
    std::vector<CopiedLaneRange> lanes_;
    Tick destination_;
    Tick duration_;
    std::string description_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class AddEventCommand final : public EditCommand {
public:
    AddEventCommand(Scenario& scenario, Event event);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Scenario* scenario_;
    Event event_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class ChangeEventCommand final : public EditCommand {
public:
    ChangeEventCommand(Scenario& scenario, std::string eventId, Event replacement);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Scenario* scenario_;
    std::string eventId_;
    Event replacement_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class RepairWaveformEventLinkCommand final : public EditCommand {
public:
    RepairWaveformEventLinkCommand(
        const Project& project,
        Scenario& scenario,
        std::string eventId);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    const Project* project_;
    Scenario* scenario_;
    std::string eventId_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class ClearEventCycleCommand final : public EditCommand {
public:
    ClearEventCycleCommand(
        Scenario& scenario,
        std::string eventId);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    Scenario* scenario_;
    std::string eventId_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class RepairEventClockReferenceCommand final : public EditCommand {
public:
    RepairEventClockReferenceCommand(
        const Project& project,
        Scenario& scenario,
        std::string eventId);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    const Project* project_;
    Scenario* scenario_;
    std::string eventId_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class RemoveEventCommand final : public EditCommand {
public:
    RemoveEventCommand(Scenario& scenario, std::string eventId);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Scenario* scenario_;
    std::string eventId_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class AddMarkerCommand final : public EditCommand {
public:
    AddMarkerCommand(Scenario& scenario, Marker marker);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Scenario* scenario_;
    Marker marker_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class ChangeMarkerCommand final : public EditCommand {
public:
    ChangeMarkerCommand(
        Scenario& scenario,
        std::string markerId,
        Marker replacement);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Scenario* scenario_;
    std::string markerId_;
    Marker replacement_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class ChangeMarkerAtIndexCommand final : public EditCommand {
public:
    ChangeMarkerAtIndexCommand(
        Scenario& scenario,
        std::size_t markerIndex,
        Marker expected,
        Marker replacement);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Scenario* scenario_;
    std::size_t markerIndex_;
    Marker expected_;
    Marker replacement_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class RemoveMarkerCommand final : public EditCommand {
public:
    RemoveMarkerCommand(Scenario& scenario, std::string markerId);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Scenario* scenario_;
    std::string markerId_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class RemoveMarkersCommand final : public EditCommand {
public:
    RemoveMarkersCommand(
        Scenario& scenario,
        std::vector<std::string> markerIds);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Scenario* scenario_;
    std::vector<std::string> markerIds_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class RemoveMarkerAtIndexCommand final : public EditCommand {
public:
    RemoveMarkerAtIndexCommand(
        Scenario& scenario,
        std::size_t markerIndex,
        Marker expected);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Scenario* scenario_;
    std::size_t markerIndex_;
    Marker expected_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class AddRelationCommand final : public EditCommand {
public:
    AddRelationCommand(Scenario& scenario, Relation relation);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Scenario* scenario_;
    Relation relation_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class ChangeRelationCommand final : public EditCommand {
public:
    ChangeRelationCommand(
        Scenario& scenario,
        std::string relationId,
        Relation replacement);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Scenario* scenario_;
    std::string relationId_;
    Relation replacement_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class RepairRelationClockReferenceCommand final
    : public EditCommand {
public:
    RepairRelationClockReferenceCommand(
        const Project& project,
        Scenario& scenario,
        std::string relationId);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] bool hasEffect() const noexcept override;

private:
    const Project* project_;
    Scenario* scenario_;
    std::string relationId_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class ChangeRelationAtIndexCommand final : public EditCommand {
public:
    ChangeRelationAtIndexCommand(
        Scenario& scenario,
        std::size_t relationIndex,
        Relation expected,
        Relation replacement);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Scenario* scenario_;
    std::size_t relationIndex_;
    Relation expected_;
    Relation replacement_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class RemoveRelationCommand final : public EditCommand {
public:
    RemoveRelationCommand(Scenario& scenario, std::string relationId);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Scenario* scenario_;
    std::string relationId_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class RemoveRelationsCommand final : public EditCommand {
public:
    RemoveRelationsCommand(
        Scenario& scenario,
        std::vector<std::string> relationIds);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Scenario* scenario_;
    std::vector<std::string> relationIds_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

class RemoveRelationAtIndexCommand final : public EditCommand {
public:
    RemoveRelationAtIndexCommand(
        Scenario& scenario,
        std::size_t relationIndex,
        Relation expected);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Scenario* scenario_;
    std::size_t relationIndex_;
    Relation expected_;
    std::optional<Scenario> before_;
    std::optional<Scenario> after_;
};

} // namespace wave
