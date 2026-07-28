#include "wave/commands.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace wave {
namespace {

bool actionControlsWaveform(const EventAction action)
{
    return action == EventAction::Drive
        || action == EventAction::Expect
        || action == EventAction::Pulse
        || action == EventAction::Toggle;
}

void removeRelationsReferencingEvent(
    Scenario& scenario,
    const std::string_view eventId)
{
    std::erase_if(
        scenario.relations,
        [eventId](const Relation& relation) {
            return relation.sourceEventId == eventId
                || relation.targetEventId == eventId;
        });
}

Segment* findLinkedSegment(Lane& lane, const std::string_view segmentId)
{
    const auto iterator = std::find_if(
        lane.segments.begin(),
        lane.segments.end(),
        [segmentId](const Segment& segment) { return segment.id == segmentId; });
    return iterator == lane.segments.end() ? nullptr : &*iterator;
}

void moveLaneToIndex(
    Scenario& scenario,
    const std::string_view laneId,
    const std::size_t destinationIndex)
{
    if (destinationIndex >= scenario.lanes.size()) {
        throw std::runtime_error("lane destination index is outside the scenario");
    }
    const auto iterator = std::find_if(
        scenario.lanes.begin(),
        scenario.lanes.end(),
        [laneId](const Lane& lane) {
            return lane.id == laneId;
        });
    if (iterator == scenario.lanes.end()) {
        throw std::runtime_error("lane was removed before reordering");
    }
    Lane moved = std::move(*iterator);
    scenario.lanes.erase(iterator);
    scenario.lanes.insert(
        scenario.lanes.begin() + static_cast<std::ptrdiff_t>(destinationIndex),
        std::move(moved));
}

void applyNewEventToWaveform(Scenario& scenario, Event event)
{
    if (event.id.empty()) event.id = makeStableId("event");
    if (event.tick < 0 || event.tick >= scenario.duration) {
        throw std::invalid_argument("event time is outside the scenario");
    }
    if (!actionControlsWaveform(event.action) || event.laneId.empty()) {
        scenario.events.push_back(std::move(event));
        return;
    }

    auto* lane = findLane(scenario, event.laneId);
    if (!lane) throw std::invalid_argument("event target lane does not exist");
    const auto validation = validateLaneValue(*lane, event.value);
    if (!validation.valid) throw std::invalid_argument(validation.error);

    Tick end = scenario.duration;
    for (const auto& segment : lane->segments) {
        if (segment.start > event.tick) {
            end = std::min(end, segment.start);
            break;
        }
        if (segment.start <= event.tick && event.tick < segment.end) {
            end = segment.end;
            break;
        }
    }
    if (end <= event.tick) {
        throw std::invalid_argument("event has no writable waveform interval");
    }

    const auto newSegmentId = makeStableId("segment");
    setSegmentRange(*lane, event.tick, end, validation.normalizedValue, newSegmentId);
    const auto segment = std::find_if(
        lane->segments.begin(),
        lane->segments.end(),
        [eventTick = event.tick](const Segment& candidate) {
            return candidate.start == eventTick;
        });
    if (segment == lane->segments.end()) {
        throw std::invalid_argument("event value does not create a distinct waveform transition");
    }
    event.value = validation.normalizedValue;
    event.linkedSegmentId = segment->id;
    event.waveformLinked = true;
    scenario.events.push_back(std::move(event));
    synchronizeLaneEventsFromSegments(scenario, lane->id);
}

void applyChangedEventToWaveform(
    Scenario& scenario,
    const std::string_view eventId,
    Event replacement)
{
    auto* existing = findEvent(scenario, eventId);
    if (!existing) throw std::invalid_argument("event does not exist");
    replacement.id = existing->id;

    if (!existing->waveformLinked) {
        *existing = std::move(replacement);
        return;
    }
    if (!actionControlsWaveform(replacement.action)) {
        throw std::invalid_argument("a waveform-linked event must retain a waveform action");
    }

    auto* lane = findLane(scenario, existing->laneId);
    if (!lane) throw std::invalid_argument("linked lane does not exist");
    if (replacement.laneId != existing->laneId) {
        throw std::invalid_argument("moving linked events between lanes is not supported");
    }
    auto* segment = findLinkedSegment(*lane, existing->linkedSegmentId);
    if (!segment) throw std::invalid_argument("linked segment does not exist");
    const auto validation = validateLaneValue(*lane, replacement.value);
    if (!validation.valid) throw std::invalid_argument(validation.error);

    const auto segmentIndex = static_cast<std::size_t>(segment - lane->segments.data());
    const auto lowerBound = segmentIndex > 0
        ? lane->segments.at(segmentIndex - 1).start + 1
        : Tick{0};
    const auto upperBound = segment->end - 1;
    if (replacement.tick < lowerBound || replacement.tick > upperBound) {
        throw std::invalid_argument("event time would invert a waveform interval");
    }
    if (segmentIndex > 0
        && lane->segments.at(segmentIndex - 1).end == segment->start) {
        lane->segments.at(segmentIndex - 1).end = replacement.tick;
    }
    segment->start = replacement.tick;
    segment->value = validation.normalizedValue;
    replacement.value = validation.normalizedValue;
    replacement.linkedSegmentId = segment->id;
    replacement.waveformLinked = true;
    *existing = std::move(replacement);
    normalizeSegments(*lane);
    synchronizeLaneEventsFromSegments(scenario, lane->id);
}

void applyRemovedEventToWaveform(Scenario& scenario, const std::string_view eventId)
{
    const auto iterator = std::find_if(
        scenario.events.begin(),
        scenario.events.end(),
        [eventId](const Event& event) { return event.id == eventId; });
    if (iterator == scenario.events.end()) {
        throw std::invalid_argument("event does not exist");
    }
    const auto removedEventId = iterator->id;
    const auto laneId = iterator->laneId;
    const auto linkedSegmentId = iterator->linkedSegmentId;
    const auto linked = iterator->waveformLinked;
    scenario.events.erase(iterator);
    removeRelationsReferencingEvent(scenario, removedEventId);
    if (!linked) return;

    auto* lane = findLane(scenario, laneId);
    if (!lane) return;
    const auto segment = std::find_if(
        lane->segments.begin(),
        lane->segments.end(),
        [&linkedSegmentId](const Segment& candidate) {
            return candidate.id == linkedSegmentId;
        });
    if (segment != lane->segments.end()) {
        const auto start = segment->start;
        const auto end = segment->end;
        clearSegmentRange(*lane, start, end);
    }
    synchronizeLaneEventsFromSegments(scenario, laneId);
}

template<typename Mutation>
void snapshotRedo(
    Scenario& scenario,
    std::optional<Scenario>& before,
    std::optional<Scenario>& after,
    Mutation&& mutation)
{
    if (after) {
        scenario = *after;
        return;
    }
    before = scenario;
    mutation();
    after = scenario;
}

} // namespace

void CommandStack::execute(std::unique_ptr<EditCommand> command)
{
    if (!command) {
        throw std::invalid_argument("command is null");
    }
    if (cursor_ < commands_.size()) {
        commands_.erase(commands_.begin() + static_cast<std::ptrdiff_t>(cursor_), commands_.end());
    }
    command->redo();
    commands_.push_back(std::move(command));
    cursor_ = commands_.size();
}

void CommandStack::replaceLast(std::unique_ptr<EditCommand> command)
{
    if (!command) throw std::invalid_argument("command is null");
    if (commands_.empty() || cursor_ != commands_.size()) {
        throw std::logic_error("no executed command is available for replacement");
    }

    auto previous = std::move(commands_.back());
    commands_.pop_back();
    --cursor_;
    previous->undo();
    try {
        command->redo();
    } catch (...) {
        previous->redo();
        commands_.push_back(std::move(previous));
        cursor_ = commands_.size();
        throw;
    }
    commands_.push_back(std::move(command));
    cursor_ = commands_.size();
}

bool CommandStack::discardLast()
{
    if (commands_.empty() || cursor_ != commands_.size()) return false;
    commands_.back()->undo();
    commands_.pop_back();
    cursor_ = commands_.size();
    return true;
}

bool CommandStack::undoLastAfter(const std::size_t baseline)
{
    if (cursor_ <= baseline) return false;
    --cursor_;
    commands_[cursor_]->undo();
    commands_.erase(
        commands_.begin() + static_cast<std::ptrdiff_t>(cursor_),
        commands_.end());
    return true;
}

bool CommandStack::undo()
{
    if (!canUndo()) {
        return false;
    }
    --cursor_;
    commands_[cursor_]->undo();
    return true;
}

bool CommandStack::redo()
{
    if (!canRedo()) {
        return false;
    }
    commands_[cursor_]->redo();
    ++cursor_;
    return true;
}

void CommandStack::clear() noexcept
{
    commands_.clear();
    cursor_ = 0;
}

bool CommandStack::canUndo() const noexcept
{
    return cursor_ > 0;
}

bool CommandStack::canRedo() const noexcept
{
    return cursor_ < commands_.size();
}

std::string CommandStack::undoDescription() const
{
    return canUndo() ? commands_[cursor_ - 1]->description() : std::string{};
}

std::string CommandStack::redoDescription() const
{
    return canRedo() ? commands_[cursor_]->description() : std::string{};
}

std::size_t CommandStack::size() const noexcept
{
    return commands_.size();
}

ChangeScenarioDurationCommand::ChangeScenarioDurationCommand(
    Scenario& scenario,
    const Tick duration)
    : scenario_(&scenario)
    , before_(scenario.duration)
    , after_(duration)
{
    if (duration <= 0) throw std::invalid_argument("scenario duration must be positive");
}

void ChangeScenarioDurationCommand::redo()
{
    scenario_->duration = after_;
}

void ChangeScenarioDurationCommand::undo()
{
    scenario_->duration = before_;
}

std::string ChangeScenarioDurationCommand::description() const
{
    return "Change scenario duration";
}

SetLaneRangeCommand::SetLaneRangeCommand(
    Scenario& scenario,
    std::string laneId,
    const Tick start,
    const Tick end,
    std::string value,
    JsonExtensions extensions)
    : scenario_(&scenario)
    , laneId_(std::move(laneId))
    , start_(start)
    , end_(end)
    , value_(std::move(value))
    , extensions_(std::move(extensions))
{
    const auto* lane = findLane(*scenario_, laneId_);
    if (!lane) {
        throw std::invalid_argument("lane does not exist");
    }
    before_ = lane->segments;
    eventsBefore_ = scenario.events;
}

void SetLaneRangeCommand::redo()
{
    auto* lane = findLane(*scenario_, laneId_);
    if (!lane) {
        throw std::runtime_error("lane was removed before command execution");
    }
    if (!initialized_) {
        setSegmentRange(*lane, start_, end_, value_, {}, extensions_);
        if (lane->kind == LaneKind::Bit
            || lane->kind == LaneKind::Bus
            || lane->kind == LaneKind::Enum) {
            synchronizeLaneEventsFromSegments(*scenario_, laneId_);
        }
        after_ = lane->segments;
        eventsAfter_ = scenario_->events;
        initialized_ = true;
        return;
    }
    lane->segments = after_;
    scenario_->events = eventsAfter_;
}

void SetLaneRangeCommand::undo()
{
    auto* lane = findLane(*scenario_, laneId_);
    if (!lane) {
        throw std::runtime_error("lane was removed before undo");
    }
    lane->segments = before_;
    scenario_->events = eventsBefore_;
}

std::string SetLaneRangeCommand::description() const
{
    return "Set lane range";
}

SetLaneRangesCommand::SetLaneRangesCommand(
    Scenario& scenario,
    const Tick start,
    const Tick end,
    std::vector<LaneRangeAssignment> assignments)
    : scenario_(&scenario)
    , start_(start)
    , end_(end)
    , assignments_(std::move(assignments))
{
    if (assignments_.empty()) {
        throw std::invalid_argument("range assignment contains no lanes");
    }
    if (start_ < 0 || end_ <= start_ || end_ > scenario.duration) {
        throw std::invalid_argument("range assignment is outside the scenario");
    }

    std::vector<std::string> laneIds;
    laneIds.reserve(assignments_.size());
    for (auto& assignment : assignments_) {
        if (assignment.laneId.empty()
            || std::find(laneIds.begin(), laneIds.end(), assignment.laneId) != laneIds.end()) {
            throw std::invalid_argument("range assignment contains an invalid or duplicate lane");
        }
        const auto* lane = findLane(scenario, assignment.laneId);
        if (!lane || lane->kind == LaneKind::Group) {
            throw std::invalid_argument("range assignment target lane does not exist");
        }
        const auto validation = validateLaneValue(*lane, assignment.value);
        if (!validation.valid) throw std::invalid_argument(validation.error);
        assignment.value = validation.normalizedValue;
        laneIds.push_back(assignment.laneId);
    }
}

void SetLaneRangesCommand::redo()
{
    if (after_) {
        *scenario_ = *after_;
        return;
    }

    before_ = *scenario_;
    auto candidate = *scenario_;
    for (const auto& assignment : assignments_) {
        auto* lane = findLane(candidate, assignment.laneId);
        if (!lane) {
            throw std::runtime_error("range assignment target lane was removed");
        }
        setSegmentRange(
            *lane,
            start_,
            end_,
            assignment.value,
            {},
            assignment.extensions);
        if (lane->kind == LaneKind::Bit
            || lane->kind == LaneKind::Bus
            || lane->kind == LaneKind::Enum) {
            synchronizeLaneEventsFromSegments(candidate, lane->id);
        }
    }
    after_ = std::move(candidate);
    *scenario_ = *after_;
}

void SetLaneRangesCommand::undo()
{
    if (!before_) throw std::runtime_error("range assignment command was not initialized");
    *scenario_ = *before_;
}

std::string SetLaneRangesCommand::description() const
{
    return "Set selected ranges";
}

ClearLaneRangesCommand::ClearLaneRangesCommand(
    Scenario& scenario,
    const Tick start,
    const Tick end,
    std::vector<std::string> laneIds)
    : scenario_(&scenario)
    , start_(start)
    , end_(end)
    , laneIds_(std::move(laneIds))
{
    if (laneIds_.empty()) {
        throw std::invalid_argument("range clear contains no lanes");
    }
    if (start_ < 0 || end_ <= start_ || end_ > scenario.duration) {
        throw std::invalid_argument("range clear is outside the scenario");
    }

    std::vector<std::string> uniqueLaneIds;
    uniqueLaneIds.reserve(laneIds_.size());
    for (const auto& laneId : laneIds_) {
        if (laneId.empty()
            || std::find(uniqueLaneIds.begin(), uniqueLaneIds.end(), laneId)
                != uniqueLaneIds.end()) {
            throw std::invalid_argument("range clear contains an invalid or duplicate lane");
        }
        const auto* lane = findLane(scenario, laneId);
        if (!lane || lane->kind == LaneKind::Group) {
            throw std::invalid_argument("range clear target lane does not exist");
        }
        uniqueLaneIds.push_back(laneId);
    }
}

void ClearLaneRangesCommand::redo()
{
    if (after_) {
        *scenario_ = *after_;
        return;
    }

    before_ = *scenario_;
    auto candidate = *scenario_;
    for (const auto& laneId : laneIds_) {
        auto* lane = findLane(candidate, laneId);
        if (!lane) throw std::runtime_error("range clear target lane was removed");
        clearSegmentRange(*lane, start_, end_);
        if (lane->kind == LaneKind::Bit
            || lane->kind == LaneKind::Bus
            || lane->kind == LaneKind::Enum) {
            synchronizeLaneEventsFromSegments(candidate, lane->id);
        }
    }
    after_ = std::move(candidate);
    *scenario_ = *after_;
}

void ClearLaneRangesCommand::undo()
{
    if (!before_) throw std::runtime_error("range clear command was not initialized");
    *scenario_ = *before_;
}

std::string ClearLaneRangesCommand::description() const
{
    return "Clear selected ranges";
}

ClearLaneRangeCommand::ClearLaneRangeCommand(
    Scenario& scenario,
    std::string laneId,
    const Tick start,
    const Tick end)
    : scenario_(&scenario)
    , laneId_(std::move(laneId))
    , start_(start)
    , end_(end)
{
    const auto* lane = findLane(*scenario_, laneId_);
    if (!lane) throw std::invalid_argument("lane does not exist");
    if (start_ < 0 || end_ <= start_ || end_ > scenario.duration) {
        throw std::invalid_argument("clear range is outside the scenario");
    }
}

void ClearLaneRangeCommand::redo()
{
    if (after_) {
        *scenario_ = *after_;
        return;
    }

    before_ = *scenario_;
    auto candidate = *scenario_;
    auto* lane = findLane(candidate, laneId_);
    if (!lane) throw std::runtime_error("lane was removed before command execution");
    clearSegmentRange(*lane, start_, end_);
    if (lane->kind == LaneKind::Bit
        || lane->kind == LaneKind::Bus
        || lane->kind == LaneKind::Enum) {
        synchronizeLaneEventsFromSegments(candidate, laneId_);
    }
    after_ = std::move(candidate);
    *scenario_ = *after_;
}

void ClearLaneRangeCommand::undo()
{
    if (!before_) throw std::runtime_error("clear command was not initialized");
    *scenario_ = *before_;
}

std::string ClearLaneRangeCommand::description() const
{
    return "Clear lane range";
}

EditSegmentCommand::EditSegmentCommand(
    Scenario& scenario,
    std::string laneId,
    std::string segmentId,
    const Tick start,
    const Tick end,
    std::string value)
    : scenario_(&scenario)
    , laneId_(std::move(laneId))
    , segmentId_(std::move(segmentId))
    , start_(start)
    , end_(end)
    , value_(std::move(value))
{
}

void EditSegmentCommand::redo()
{
    snapshotRedo(*scenario_, before_, after_, [this] {
        auto* lane = findLane(*scenario_, laneId_);
        if (!lane) throw std::invalid_argument("lane does not exist");
        auto* segment = findLinkedSegment(*lane, segmentId_);
        if (!segment) throw std::invalid_argument("segment does not exist");
        if (start_ < 0 || end_ <= start_ || end_ > scenario_->duration) {
            throw std::invalid_argument("segment interval is invalid");
        }
        const auto validation = validateLaneValue(*lane, value_);
        if (!validation.valid) throw std::invalid_argument(validation.error);

        const auto index = static_cast<std::size_t>(segment - lane->segments.data());
        const auto oldStart = segment->start;
        const auto oldEnd = segment->end;
        auto* previous = index > 0 ? &lane->segments.at(index - 1) : nullptr;
        auto* next = index + 1 < lane->segments.size()
            ? &lane->segments.at(index + 1)
            : nullptr;
        const auto previousTouches = previous && previous->end == oldStart;
        const auto nextTouches = next && next->start == oldEnd;

        if (previousTouches) {
            if (start_ <= previous->start) {
                throw std::invalid_argument("segment start would consume its previous segment");
            }
        } else if (previous && start_ < previous->end) {
            throw std::invalid_argument("segment start overlaps its previous segment");
        }
        if (nextTouches) {
            if (end_ >= next->end) {
                throw std::invalid_argument("segment end would consume its next segment");
            }
        } else if (next && end_ > next->start) {
            throw std::invalid_argument("segment end overlaps its next segment");
        }

        if (previousTouches) previous->end = start_;
        if (nextTouches) next->start = end_;
        segment->start = start_;
        segment->end = end_;
        segment->value = validation.normalizedValue;
        normalizeSegments(*lane);
        if (lane->kind == LaneKind::Bit
            || lane->kind == LaneKind::Bus
            || lane->kind == LaneKind::Enum) {
            synchronizeLaneEventsFromSegments(*scenario_, laneId_);
        }
    });
}

void EditSegmentCommand::undo()
{
    if (!before_) throw std::runtime_error("segment command has not been executed");
    *scenario_ = *before_;
}

std::string EditSegmentCommand::description() const
{
    return "Edit segment";
}

ToggleBitRangeCommand::ToggleBitRangeCommand(
    Scenario& scenario,
    std::string laneId,
    std::vector<std::pair<Tick, Tick>> beatRanges)
    : scenario_(&scenario)
    , laneId_(std::move(laneId))
    , beatRanges_(std::move(beatRanges))
{
}

void ToggleBitRangeCommand::redo()
{
    snapshotRedo(*scenario_, before_, after_, [this] {
        auto* lane = findLane(*scenario_, laneId_);
        if (!lane) throw std::invalid_argument("lane does not exist");
        if (lane->kind != LaneKind::Bit) {
            throw std::invalid_argument("only bit lanes can toggle beats");
        }
        if (beatRanges_.empty()) {
            throw std::invalid_argument("toggle range is empty");
        }

        std::vector<std::string> replacements;
        replacements.reserve(beatRanges_.size());
        Tick previousEnd = -1;
        for (const auto& [start, end] : beatRanges_) {
            if (start < 0 || end <= start || end > scenario_->duration
                || (previousEnd >= 0 && start < previousEnd)) {
                throw std::invalid_argument("toggle beat interval is invalid");
            }
            previousEnd = end;
            const auto iterator = std::upper_bound(
                lane->segments.begin(),
                lane->segments.end(),
                start,
                [](const Tick tick, const Segment& candidate) {
                    return tick < candidate.start;
                });
            const Segment* covering = nullptr;
            if (iterator != lane->segments.begin()) {
                const auto& candidate = *std::prev(iterator);
                if (candidate.start <= start && start < candidate.end) {
                    covering = &candidate;
                }
            }
            if (!covering) {
                replacements.emplace_back("1");
            } else if (covering->value == "0") {
                replacements.emplace_back("1");
            } else if (covering->value == "1") {
                replacements.emplace_back("0");
            } else {
                throw std::invalid_argument("X/Z beats must be edited explicitly");
            }
        }

        for (std::size_t index = 0; index < beatRanges_.size(); ++index) {
            const auto [start, end] = beatRanges_.at(index);
            setSegmentRange(*lane, start, end, replacements.at(index));
        }
        synchronizeLaneEventsFromSegments(*scenario_, laneId_);
    });
}

void ToggleBitRangeCommand::undo()
{
    if (!before_) throw std::runtime_error("toggle command has not been executed");
    *scenario_ = *before_;
}

std::string ToggleBitRangeCommand::description() const
{
    return beatRanges_.size() == 1 ? "Toggle bit beat" : "Toggle bit beats";
}

AddLaneCommand::AddLaneCommand(Scenario& scenario, Lane lane)
    : scenario_(&scenario)
    , lane_(std::move(lane))
    , insertionIndex_(scenario.lanes.size())
{
    if (lane_.id.empty()) {
        lane_.id = makeStableId("lane");
    }
}

AddLaneCommand::AddLaneCommand(
    Project& project,
    Scenario& scenario,
    Lane lane,
    ClockDomain clockDomain)
    : AddLaneCommand(scenario, std::move(lane))
{
    project_ = &project;
    if (clockDomain.id.empty()) clockDomain.id = makeStableId("clock");
    if (clockDomain.name.empty()) clockDomain.name = lane_.name;
    if (!clockDomain.isValid()) {
        throw std::invalid_argument("clock domain is invalid");
    }

    if (lane_.kind != LaneKind::Clock) {
        throw std::invalid_argument("only a clock lane can create a clock domain");
    }
    if (lane_.clockDomainId.empty()) lane_.clockDomainId = clockDomain.id;
    if (lane_.clockDomainId != clockDomain.id) {
        throw std::invalid_argument("clock lane references a different clock domain");
    }
    clockDomain_ = std::move(clockDomain);
}

void AddLaneCommand::redo()
{
    if (findLane(*scenario_, lane_.id)) {
        throw std::runtime_error("lane already exists");
    }
    if (clockDomain_ && (!project_ || findClock(*project_, clockDomain_->id))) {
        throw std::runtime_error("clock domain already exists");
    }
    const auto index = std::min(insertionIndex_, scenario_->lanes.size());
    if (clockDomain_) project_->clockDomains.push_back(*clockDomain_);
    scenario_->lanes.insert(
        scenario_->lanes.begin() + static_cast<std::ptrdiff_t>(index),
        lane_);
}

void AddLaneCommand::undo()
{
    const auto iterator = std::find_if(
        scenario_->lanes.begin(),
        scenario_->lanes.end(),
        [this](const Lane& lane) { return lane.id == lane_.id; });
    if (iterator == scenario_->lanes.end()) {
        throw std::runtime_error("lane was removed before undo");
    }
    scenario_->lanes.erase(iterator);
    if (clockDomain_) {
        if (!project_) throw std::runtime_error("clock project is unavailable");
        const auto clock = std::find_if(
            project_->clockDomains.begin(),
            project_->clockDomains.end(),
            [this](const ClockDomain& candidate) {
                return candidate.id == clockDomain_->id;
            });
        if (clock == project_->clockDomains.end()) {
            throw std::runtime_error("clock domain was removed before undo");
        }
        project_->clockDomains.erase(clock);
    }
}

std::string AddLaneCommand::description() const
{
    return "Add lane";
}

RemoveLaneCommand::RemoveLaneCommand(
    Project& project,
    Scenario& scenario,
    std::string laneId)
    : project_(&project)
    , scenario_(&scenario)
    , laneId_(std::move(laneId))
    , beforeScenario_(scenario)
    , afterScenario_(scenario)
{
    const auto scenarioBelongsToProject = std::any_of(
        project.scenarios.begin(),
        project.scenarios.end(),
        [&scenario](const Scenario& candidate) {
            return &candidate == &scenario;
        });
    if (!scenarioBelongsToProject) {
        throw std::invalid_argument("scenario does not belong to project");
    }
    const auto* lane = findLane(scenario, laneId_);
    if (!lane) throw std::invalid_argument("lane does not exist");

    std::vector<std::string> removedEventIds;
    for (const auto& event : afterScenario_.events) {
        if (event.laneId == laneId_) removedEventIds.push_back(event.id);
    }
    std::erase_if(
        afterScenario_.events,
        [this](const Event& event) {
            return event.laneId == laneId_;
        });
    std::erase_if(
        afterScenario_.relations,
        [&removedEventIds](const Relation& relation) {
            const auto removed = [&removedEventIds](const std::string& eventId) {
                return std::find(
                           removedEventIds.begin(),
                           removedEventIds.end(),
                           eventId)
                    != removedEventIds.end();
            };
            return removed(relation.sourceEventId)
                || removed(relation.targetEventId);
        });
    std::erase_if(
        afterScenario_.lanes,
        [this](const Lane& candidate) {
            return candidate.id == laneId_;
        });
    if (lane->kind == LaneKind::Group) {
        for (auto& candidate : afterScenario_.lanes) {
            if (candidate.groupId == laneId_) candidate.groupId.clear();
        }
    }

    const auto usedByAnotherScenario = std::any_of(
        project.scenarios.begin(),
        project.scenarios.end(),
        [this, &scenario](const Scenario& candidate) {
            return &candidate != &scenario && findLane(candidate, laneId_);
        });
    if (!usedByAnotherScenario) {
        for (const auto& trace : project.importedTraces) {
            const auto mapping = trace.signalMapping.find(laneId_);
            if (mapping != trace.signalMapping.end()) {
                removedTraceMappings_.push_back({trace.id, mapping->second});
            }
        }
    }
}

void RemoveLaneCommand::redo()
{
    *scenario_ = afterScenario_;
    for (auto& trace : project_->importedTraces) {
        const auto removed = std::any_of(
            removedTraceMappings_.begin(),
            removedTraceMappings_.end(),
            [&trace](const TraceMapping& mapping) {
                return mapping.traceId == trace.id;
            });
        if (removed) trace.signalMapping.erase(laneId_);
    }
}

void RemoveLaneCommand::undo()
{
    *scenario_ = beforeScenario_;
    for (const auto& mapping : removedTraceMappings_) {
        const auto trace = std::find_if(
            project_->importedTraces.begin(),
            project_->importedTraces.end(),
            [&mapping](const ImportedTrace& candidate) {
                return candidate.id == mapping.traceId;
            });
        if (trace != project_->importedTraces.end()) {
            trace->signalMapping[laneId_] = mapping.signalId;
        }
    }
}

std::string RemoveLaneCommand::description() const
{
    return "Remove lane";
}

MoveLaneCommand::MoveLaneCommand(
    Scenario& scenario,
    std::string laneId,
    const std::size_t destinationIndex)
    : scenario_(&scenario)
    , laneId_(std::move(laneId))
    , beforeIndex_(0)
    , afterIndex_(destinationIndex)
{
    const auto iterator = std::find_if(
        scenario.lanes.begin(),
        scenario.lanes.end(),
        [this](const Lane& lane) {
            return lane.id == laneId_;
        });
    if (iterator == scenario.lanes.end()) {
        throw std::invalid_argument("lane does not exist");
    }
    if (destinationIndex >= scenario.lanes.size()) {
        throw std::invalid_argument("lane destination index is outside the scenario");
    }
    beforeIndex_ = static_cast<std::size_t>(
        std::distance(scenario.lanes.begin(), iterator));
    if (beforeIndex_ == afterIndex_) {
        throw std::invalid_argument("lane is already at the destination index");
    }
}

void MoveLaneCommand::redo()
{
    moveLaneToIndex(*scenario_, laneId_, afterIndex_);
}

void MoveLaneCommand::undo()
{
    moveLaneToIndex(*scenario_, laneId_, beforeIndex_);
}

std::string MoveLaneCommand::description() const
{
    return "Move lane";
}

ChangeLaneCommand::ChangeLaneCommand(
    const Project& project,
    Scenario& scenario,
    std::string laneId,
    Lane replacement)
    : scenario_(&scenario)
    , laneId_(std::move(laneId))
{
    const auto* existing = findLane(scenario, laneId_);
    if (!existing) throw std::invalid_argument("lane does not exist");
    before_ = *existing;
    replacement.id = laneId_;
    if (replacement.name.empty()) {
        throw std::invalid_argument("lane name is empty");
    }
    if (replacement.height < 30 || replacement.height > 240) {
        throw std::invalid_argument("lane height must be between 30 and 240");
    }

    if (replacement.kind == LaneKind::Group) {
        replacement.width = 1;
        replacement.isSigned = false;
        replacement.enumMap.clear();
        replacement.clockDomainId.clear();
        replacement.groupId.clear();
        if (!replacement.segments.empty()) {
            throw std::invalid_argument("group lanes cannot contain segments");
        }
    } else {
        if (replacement.kind == LaneKind::Clock
            || replacement.kind == LaneKind::Bit
            || replacement.kind == LaneKind::Transaction
            || replacement.kind == LaneKind::Event) {
            replacement.width = 1;
            replacement.isSigned = false;
        } else if (replacement.width == 0) {
            throw std::invalid_argument("bus and enum lane width must be positive");
        }
        if (replacement.kind != LaneKind::Enum) replacement.enumMap.clear();

        if (!replacement.clockDomainId.empty()
            && !findClock(project, replacement.clockDomainId)) {
            throw std::invalid_argument("lane references an unknown clock domain");
        }
        if (replacement.kind == LaneKind::Clock
            && replacement.clockDomainId.empty()) {
            throw std::invalid_argument("clock lane requires a clock domain");
        }
        if (!replacement.groupId.empty()) {
            const auto* group = findLane(scenario, replacement.groupId);
            if (!group || group->kind != LaneKind::Group || group->id == laneId_) {
                throw std::invalid_argument("lane references an invalid group");
            }
        }
    }

    if (before_.kind == LaneKind::Group
        && replacement.kind != LaneKind::Group
        && std::any_of(
            scenario.lanes.begin(),
            scenario.lanes.end(),
            [this](const Lane& lane) {
                return lane.groupId == laneId_;
            })) {
        throw std::invalid_argument(
            "a group with members cannot be converted to another lane kind");
    }
    if ((replacement.kind == LaneKind::Clock
         || replacement.kind == LaneKind::Group)
        && std::any_of(
            scenario.events.begin(),
            scenario.events.end(),
            [this](const Event& event) {
                return event.laneId == laneId_;
            })) {
        throw std::invalid_argument(
            "clock and group lanes cannot own scenario events");
    }

    normalizeSegments(replacement);
    for (const auto& event : scenario.events) {
        if (event.laneId != laneId_) continue;
        const auto validation = validateLaneValue(replacement, event.value);
        if (!validation.valid) {
            throw std::invalid_argument(
                "lane property change invalidates an event value: "
                + validation.error);
        }
    }
    after_ = std::move(replacement);
}

void ChangeLaneCommand::redo()
{
    auto* lane = findLane(*scenario_, laneId_);
    if (!lane) throw std::runtime_error("lane was removed before command execution");
    *lane = after_;
}

void ChangeLaneCommand::undo()
{
    auto* lane = findLane(*scenario_, laneId_);
    if (!lane) throw std::runtime_error("lane was removed before undo");
    *lane = before_;
}

std::string ChangeLaneCommand::description() const
{
    return "Change lane";
}

ChangeClockCommand::ChangeClockCommand(
    Project& project,
    Scenario& scenario,
    std::string clockId,
    ClockDomain replacement)
    : project_(&project)
    , clockId_(std::move(clockId))
    , beforeScenarios_(project.scenarios)
    , afterScenarios_(project.scenarios)
{
    const auto scenarioBelongsToProject = std::any_of(
        project.scenarios.begin(),
        project.scenarios.end(),
        [&scenario](const Scenario& candidate) {
            return &candidate == &scenario;
        });
    if (!scenarioBelongsToProject) {
        throw std::invalid_argument("active scenario does not belong to the project");
    }
    const auto* existing = findClock(project, clockId_);
    if (!existing) throw std::invalid_argument("clock domain does not exist");
    beforeClock_ = *existing;
    replacement.id = clockId_;
    replacement.dutyCycle.normalize();
    if (!replacement.isValid()) {
        throw std::invalid_argument("replacement clock domain is invalid");
    }
    afterClock_ = std::move(replacement);

    for (std::size_t scenarioIndex = 0;
         scenarioIndex < afterScenarios_.size();
         ++scenarioIndex) {
        const auto& beforeScenario = beforeScenarios_.at(scenarioIndex);
        auto& afterScenario = afterScenarios_.at(scenarioIndex);
        std::vector<std::pair<std::string, Tick>> retimedEvents;
        for (const auto& event : beforeScenario.events) {
            if (!event.cycle) continue;
            auto domainId = event.clockDomainId;
            if (domainId.empty()) {
                const auto* lane = findLane(beforeScenario, event.laneId);
                if (lane) domainId = lane->clockDomainId;
            }
            if (domainId != clockId_) continue;
            const auto tick = tickAtCycle(
                afterClock_,
                *event.cycle,
                afterClock_.activeEdge);
            if (!tick || *tick < 0 || *tick >= afterScenario.duration) {
                throw std::invalid_argument(
                    "clock change moves a cycle-based event outside a scenario");
            }
            retimedEvents.emplace_back(event.id, *tick);
        }

        std::vector<std::string> affectedLaneIds;
        for (const auto& [eventId, tick] : retimedEvents) {
            auto* event = findEvent(afterScenario, eventId);
            if (!event) {
                throw std::runtime_error(
                    "cycle-based event disappeared during clock retiming");
            }
            if (!event->waveformLinked) {
                event->tick = tick;
                continue;
            }
            if (!actionControlsWaveform(event->action)) {
                throw std::invalid_argument(
                    "a waveform-linked cycle event must retain a waveform action");
            }

            auto* lane = findLane(afterScenario, event->laneId);
            const auto* beforeLane = findLane(beforeScenario, event->laneId);
            if (!lane || !beforeLane) {
                throw std::invalid_argument(
                    "linked lane does not exist during clock retiming");
            }
            auto* segment = findLinkedSegment(*lane, event->linkedSegmentId);
            const auto beforeSegment = std::find_if(
                beforeLane->segments.begin(),
                beforeLane->segments.end(),
                [&event](const Segment& candidate) {
                    return candidate.id == event->linkedSegmentId;
                });
            if (!segment || beforeSegment == beforeLane->segments.end()) {
                throw std::invalid_argument(
                    "linked segment does not exist during clock retiming");
            }

            const auto segmentIndex = static_cast<std::size_t>(
                beforeSegment - beforeLane->segments.begin());
            if (segmentIndex > 0
                && beforeLane->segments.at(segmentIndex - 1).end
                    == beforeSegment->start) {
                lane->segments.at(segmentIndex - 1).end = tick;
            }
            segment->start = tick;
            event->tick = tick;
            if (std::find(
                    affectedLaneIds.begin(),
                    affectedLaneIds.end(),
                    lane->id)
                == affectedLaneIds.end()) {
                affectedLaneIds.push_back(lane->id);
            }
        }

        for (const auto& laneId : affectedLaneIds) {
            auto* lane = findLane(afterScenario, laneId);
            if (!lane) throw std::runtime_error("retimed lane disappeared");
            for (std::size_t index = 0; index < lane->segments.size(); ++index) {
                const auto& segment = lane->segments[index];
                if (segment.start < 0
                    || segment.end > afterScenario.duration
                    || segment.end <= segment.start
                    || (index > 0
                        && lane->segments[index - 1].end > segment.start)) {
                    throw std::invalid_argument(
                        "clock change would invert a waveform interval");
                }
            }
            synchronizeLaneEventsFromSegments(afterScenario, laneId);
        }
        std::stable_sort(
            afterScenario.events.begin(),
            afterScenario.events.end(),
            [](const Event& left, const Event& right) {
                return left.tick < right.tick
                    || (left.tick == right.tick && left.id < right.id);
            });
    }
}

void ChangeClockCommand::redo()
{
    auto* clock = findClock(*project_, clockId_);
    if (!clock) throw std::runtime_error("clock domain was removed before command execution");
    for (const auto& snapshot : afterScenarios_) {
        const auto iterator = std::find_if(
            project_->scenarios.begin(),
            project_->scenarios.end(),
            [&snapshot](const Scenario& scenario) {
                return scenario.id == snapshot.id;
            });
        if (iterator == project_->scenarios.end()) {
            throw std::runtime_error("scenario was removed before command execution");
        }
    }
    *clock = afterClock_;
    for (const auto& snapshot : afterScenarios_) {
        *std::find_if(
            project_->scenarios.begin(),
            project_->scenarios.end(),
            [&snapshot](const Scenario& scenario) {
                return scenario.id == snapshot.id;
            }) = snapshot;
    }
}

void ChangeClockCommand::undo()
{
    auto* clock = findClock(*project_, clockId_);
    if (!clock) throw std::runtime_error("clock domain was removed before undo");
    for (const auto& snapshot : beforeScenarios_) {
        const auto iterator = std::find_if(
            project_->scenarios.begin(),
            project_->scenarios.end(),
            [&snapshot](const Scenario& scenario) {
                return scenario.id == snapshot.id;
            });
        if (iterator == project_->scenarios.end()) {
            throw std::runtime_error("scenario was removed before clock undo");
        }
    }
    *clock = beforeClock_;
    for (const auto& snapshot : beforeScenarios_) {
        *std::find_if(
            project_->scenarios.begin(),
            project_->scenarios.end(),
            [&snapshot](const Scenario& scenario) {
                return scenario.id == snapshot.id;
            }) = snapshot;
    }
}

std::string ChangeClockCommand::description() const
{
    return "Change clock";
}

PasteRangeCommand::PasteRangeCommand(
    Scenario& scenario,
    std::vector<CopiedLaneRange> lanes,
    const Tick destination,
    const Tick duration)
    : scenario_(&scenario)
    , lanes_(std::move(lanes))
    , destination_(destination)
    , duration_(duration)
{
    if (lanes_.empty()) throw std::invalid_argument("paste contains no lanes");
    if (destination_ < 0 || duration_ <= 0 || destination_ >= scenario.duration) {
        throw std::invalid_argument("paste range is outside the scenario");
    }
    for (const auto& copiedLane : lanes_) {
        if (!findLane(scenario, copiedLane.laneId)) {
            throw std::invalid_argument("paste target lane does not exist");
        }
        for (const auto& segment : copiedLane.relativeSegments) {
            if (segment.start < 0 || segment.end <= segment.start
                || segment.end > duration_) {
                throw std::invalid_argument("copied segment is outside the copied range");
            }
        }
    }
}

void PasteRangeCommand::redo()
{
    if (after_) {
        *scenario_ = *after_;
        return;
    }
    before_ = *scenario_;
    const auto pasteEnd = std::min(
        scenario_->duration,
        destination_ + std::min(duration_, scenario_->duration - destination_));
    for (const auto& copiedLane : lanes_) {
        auto* lane = findLane(*scenario_, copiedLane.laneId);
        if (!lane) throw std::runtime_error("paste target lane was removed");
        clearSegmentRange(*lane, destination_, pasteEnd);
        for (const auto& relative : copiedLane.relativeSegments) {
            const auto available = pasteEnd - destination_;
            if (relative.start >= available) continue;
            const auto start = destination_ + relative.start;
            const auto end = destination_ + std::min(relative.end, available);
            if (end <= start) continue;
            setSegmentRange(
                *lane,
                start,
                end,
                relative.value,
                makeStableId("segment"));
            if (!relative.extensions.empty()) {
                const auto pasted = std::find_if(
                    lane->segments.begin(),
                    lane->segments.end(),
                    [start, end, &relative](const Segment& segment) {
                        return segment.start <= start
                            && segment.end >= end
                            && segment.value == relative.value;
                    });
                if (pasted != lane->segments.end()) pasted->extensions = relative.extensions;
            }
        }
        if (lane->kind == LaneKind::Bit
            || lane->kind == LaneKind::Bus
            || lane->kind == LaneKind::Enum) {
            synchronizeLaneEventsFromSegments(*scenario_, lane->id);
        }
    }
    after_ = *scenario_;
}

void PasteRangeCommand::undo()
{
    if (!before_) throw std::runtime_error("paste command was not initialized");
    *scenario_ = *before_;
}

std::string PasteRangeCommand::description() const
{
    return "Paste range";
}

AddEventCommand::AddEventCommand(Scenario& scenario, Event event)
    : scenario_(&scenario)
    , event_(std::move(event))
{
}

void AddEventCommand::redo()
{
    snapshotRedo(*scenario_, before_, after_, [this] {
        applyNewEventToWaveform(*scenario_, event_);
    });
}

void AddEventCommand::undo()
{
    if (!before_) throw std::runtime_error("event command has not been executed");
    *scenario_ = *before_;
}

std::string AddEventCommand::description() const
{
    return "Add event";
}

ChangeEventCommand::ChangeEventCommand(
    Scenario& scenario,
    std::string eventId,
    Event replacement)
    : scenario_(&scenario)
    , eventId_(std::move(eventId))
    , replacement_(std::move(replacement))
{
}

void ChangeEventCommand::redo()
{
    snapshotRedo(*scenario_, before_, after_, [this] {
        applyChangedEventToWaveform(*scenario_, eventId_, replacement_);
    });
}

void ChangeEventCommand::undo()
{
    if (!before_) throw std::runtime_error("event command has not been executed");
    *scenario_ = *before_;
}

std::string ChangeEventCommand::description() const
{
    return "Change event";
}

RemoveEventCommand::RemoveEventCommand(Scenario& scenario, std::string eventId)
    : scenario_(&scenario)
    , eventId_(std::move(eventId))
{
}

void RemoveEventCommand::redo()
{
    snapshotRedo(*scenario_, before_, after_, [this] {
        applyRemovedEventToWaveform(*scenario_, eventId_);
    });
}

void RemoveEventCommand::undo()
{
    if (!before_) throw std::runtime_error("event command has not been executed");
    *scenario_ = *before_;
}

std::string RemoveEventCommand::description() const
{
    return "Remove event";
}

AddMarkerCommand::AddMarkerCommand(Scenario& scenario, Marker marker)
    : scenario_(&scenario)
    , marker_(std::move(marker))
{
    if (marker_.id.empty()) marker_.id = makeStableId("marker");
}

void AddMarkerCommand::redo()
{
    snapshotRedo(*scenario_, before_, after_, [this] {
        if (marker_.start < 0 || marker_.end < marker_.start
            || marker_.end > scenario_->duration) {
            throw std::invalid_argument("marker interval is invalid");
        }
        scenario_->markers.push_back(marker_);
    });
}

void AddMarkerCommand::undo()
{
    if (!before_) throw std::runtime_error("marker command has not been executed");
    *scenario_ = *before_;
}

std::string AddMarkerCommand::description() const
{
    return "Add marker";
}

ChangeMarkerCommand::ChangeMarkerCommand(
    Scenario& scenario,
    std::string markerId,
    Marker replacement)
    : scenario_(&scenario)
    , markerId_(std::move(markerId))
    , replacement_(std::move(replacement))
{
    replacement_.id = markerId_;
}

void ChangeMarkerCommand::redo()
{
    snapshotRedo(*scenario_, before_, after_, [this] {
        const auto marker = std::find_if(
            scenario_->markers.begin(),
            scenario_->markers.end(),
            [this](const Marker& candidate) {
                return candidate.id == markerId_;
            });
        if (marker == scenario_->markers.end()) {
            throw std::invalid_argument("marker does not exist");
        }
        if (replacement_.start < 0
            || replacement_.end < replacement_.start
            || replacement_.end > scenario_->duration) {
            throw std::invalid_argument("marker interval is invalid");
        }
        *marker = replacement_;
    });
}

void ChangeMarkerCommand::undo()
{
    if (!before_) throw std::runtime_error("marker command has not been executed");
    *scenario_ = *before_;
}

std::string ChangeMarkerCommand::description() const
{
    return "Move marker";
}

RemoveMarkerCommand::RemoveMarkerCommand(
    Scenario& scenario,
    std::string markerId)
    : scenario_(&scenario)
    , markerId_(std::move(markerId))
{
}

void RemoveMarkerCommand::redo()
{
    snapshotRedo(*scenario_, before_, after_, [this] {
        const auto marker = std::find_if(
            scenario_->markers.begin(),
            scenario_->markers.end(),
            [this](const Marker& candidate) {
                return candidate.id == markerId_;
            });
        if (marker == scenario_->markers.end()) {
            throw std::invalid_argument("marker does not exist");
        }
        scenario_->markers.erase(marker);
    });
}

void RemoveMarkerCommand::undo()
{
    if (!before_) throw std::runtime_error("marker command has not been executed");
    *scenario_ = *before_;
}

std::string RemoveMarkerCommand::description() const
{
    return "Remove marker";
}

AddRelationCommand::AddRelationCommand(Scenario& scenario, Relation relation)
    : scenario_(&scenario)
    , relation_(std::move(relation))
{
    if (relation_.id.empty()) relation_.id = makeStableId("relation");
}

void AddRelationCommand::redo()
{
    snapshotRedo(*scenario_, before_, after_, [this] {
        if (!findEvent(*scenario_, relation_.sourceEventId)) {
            throw std::invalid_argument("relation source event does not exist");
        }
        if (!relation_.targetEventId.empty()
            && !findEvent(*scenario_, relation_.targetEventId)) {
            throw std::invalid_argument("relation target event does not exist");
        }
        if (relation_.minimumDelay < 0
            || relation_.maximumDelay < relation_.minimumDelay) {
            throw std::invalid_argument("relation delay range is invalid");
        }
        scenario_->relations.push_back(relation_);
    });
}

void AddRelationCommand::undo()
{
    if (!before_) throw std::runtime_error("relation command has not been executed");
    *scenario_ = *before_;
}

std::string AddRelationCommand::description() const
{
    return "Add relation";
}

ChangeRelationCommand::ChangeRelationCommand(
    Scenario& scenario,
    std::string relationId,
    Relation replacement)
    : scenario_(&scenario)
    , relationId_(std::move(relationId))
    , replacement_(std::move(replacement))
{
}

void ChangeRelationCommand::redo()
{
    snapshotRedo(*scenario_, before_, after_, [this] {
        auto* relation = findRelation(*scenario_, relationId_);
        if (!relation) throw std::invalid_argument("relation does not exist");
        if (!findEvent(*scenario_, replacement_.sourceEventId)
            || (!replacement_.targetEventId.empty()
                && !findEvent(*scenario_, replacement_.targetEventId))) {
            throw std::invalid_argument("relation event reference does not exist");
        }
        if (replacement_.minimumDelay < 0
            || replacement_.maximumDelay < replacement_.minimumDelay) {
            throw std::invalid_argument("relation delay range is invalid");
        }
        replacement_.id = relation->id;
        *relation = replacement_;
    });
}

void ChangeRelationCommand::undo()
{
    if (!before_) throw std::runtime_error("relation command has not been executed");
    *scenario_ = *before_;
}

std::string ChangeRelationCommand::description() const
{
    return "Change relation";
}

RemoveRelationCommand::RemoveRelationCommand(Scenario& scenario, std::string relationId)
    : scenario_(&scenario)
    , relationId_(std::move(relationId))
{
}

void RemoveRelationCommand::redo()
{
    snapshotRedo(*scenario_, before_, after_, [this] {
        const auto iterator = std::find_if(
            scenario_->relations.begin(),
            scenario_->relations.end(),
            [this](const Relation& relation) { return relation.id == relationId_; });
        if (iterator == scenario_->relations.end()) {
            throw std::invalid_argument("relation does not exist");
        }
        scenario_->relations.erase(iterator);
    });
}

void RemoveRelationCommand::undo()
{
    if (!before_) throw std::runtime_error("relation command has not been executed");
    *scenario_ = *before_;
}

std::string RemoveRelationCommand::description() const
{
    return "Remove relation";
}

} // namespace wave
