#pragma once

#include "wave/model.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace wave {

class EditCommand {
public:
    virtual ~EditCommand() = default;

    virtual void redo() = 0;
    virtual void undo() = 0;
    [[nodiscard]] virtual std::string description() const = 0;
};

class CommandStack {
public:
    void execute(std::unique_ptr<EditCommand> command);
    bool undo();
    bool redo();
    void clear() noexcept;

    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;
    [[nodiscard]] std::string undoDescription() const;
    [[nodiscard]] std::string redoDescription() const;
    [[nodiscard]] std::size_t size() const noexcept;

private:
    std::vector<std::unique_ptr<EditCommand>> commands_;
    std::size_t cursor_{0};
};

class SetLaneRangeCommand final : public EditCommand {
public:
    SetLaneRangeCommand(
        Scenario& scenario,
        std::string laneId,
        Tick start,
        Tick end,
        std::string value);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Scenario* scenario_;
    std::string laneId_;
    Tick start_;
    Tick end_;
    std::string value_;
    std::vector<Segment> before_;
    std::vector<Segment> after_;
    std::vector<Event> eventsBefore_;
    std::vector<Event> eventsAfter_;
    bool initialized_{false};
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

private:
    Scenario* scenario_;
    std::string laneId_;
    Tick start_;
    Tick end_;
    std::vector<Segment> before_;
    std::vector<Segment> after_;
    std::vector<Event> eventsBefore_;
    std::vector<Event> eventsAfter_;
    bool initialized_{false};
};

class AddLaneCommand final : public EditCommand {
public:
    AddLaneCommand(Scenario& scenario, Lane lane);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Scenario* scenario_;
    Lane lane_;
    std::size_t insertionIndex_{0};
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

private:
    Scenario* scenario_;
    std::string laneId_;
    Lane before_;
    Lane after_;
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

private:
    Project* project_;
    std::string clockId_;
    ClockDomain beforeClock_;
    ClockDomain afterClock_;
    std::vector<Scenario> beforeScenarios_;
    std::vector<Scenario> afterScenarios_;
};

struct CopiedLaneRange {
    std::string laneId;
    std::vector<Segment> relativeSegments;
};

class PasteRangeCommand final : public EditCommand {
public:
    PasteRangeCommand(
        Scenario& scenario,
        std::vector<CopiedLaneRange> lanes,
        Tick destination,
        Tick duration);

    void redo() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Scenario* scenario_;
    std::vector<CopiedLaneRange> lanes_;
    Tick destination_;
    Tick duration_;
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

} // namespace wave
