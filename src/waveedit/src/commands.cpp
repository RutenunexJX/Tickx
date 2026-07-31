#include "wave/commands.h"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>

namespace wave {
namespace {

bool isBlankText(const std::string_view value)
{
    return std::all_of(
        value.begin(),
        value.end(),
        [](const unsigned char character) {
            return std::isspace(character) != 0;
        });
}

bool isSupportedTraceFormat(
    const std::string_view value)
{
    auto begin = value.begin();
    auto end = value.end();
    while (begin != end
           && std::isspace(
                  static_cast<unsigned char>(*begin))
               != 0) {
        ++begin;
    }
    while (end != begin
           && std::isspace(
                  static_cast<unsigned char>(
                      *(end - 1)))
               != 0) {
        --end;
    }
    std::string normalized(begin, end);
    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](const unsigned char character) {
            return static_cast<char>(
                std::tolower(character));
        });
    return normalized == "vcd"
        || normalized == "csv";
}

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

bool rangeAlreadyEquals(
    const Lane& lane,
    const Tick start,
    const Tick end,
    const std::string_view value,
    const JsonExtensions& extensions)
{
    auto coveredUntil = start;
    for (const auto& segment : lane.segments) {
        if (segment.end <= coveredUntil) continue;
        if (segment.start > coveredUntil
            || segment.value != value
            || segment.extensions != extensions) {
            return false;
        }
        coveredUntil = std::min(end, segment.end);
        if (coveredUntil >= end) return true;
    }
    return false;
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

bool CommandStack::execute(std::unique_ptr<EditCommand> command)
{
    if (!command) {
        throw std::invalid_argument("command is null");
    }
    command->redo();
    if (!command->hasEffect()) return false;
    if (cursor_ < commands_.size()) {
        commands_.erase(commands_.begin() + static_cast<std::ptrdiff_t>(cursor_), commands_.end());
        stateIds_.erase(
            stateIds_.begin() + static_cast<std::ptrdiff_t>(cursor_ + 1),
            stateIds_.end());
    }
    commands_.push_back(std::move(command));
    stateIds_.push_back(nextStateId_++);
    cursor_ = commands_.size();
    return true;
}

void CommandStack::replaceLast(std::unique_ptr<EditCommand> command)
{
    if (!command) throw std::invalid_argument("command is null");
    if (commands_.empty() || cursor_ != commands_.size()) {
        throw std::logic_error("no executed command is available for replacement");
    }

    auto previous = std::move(commands_.back());
    const auto previousStateId = stateIds_.back();
    commands_.pop_back();
    stateIds_.pop_back();
    --cursor_;
    previous->undo();
    try {
        command->redo();
    } catch (...) {
        previous->redo();
        commands_.push_back(std::move(previous));
        stateIds_.push_back(previousStateId);
        cursor_ = commands_.size();
        throw;
    }
    commands_.push_back(std::move(command));
    stateIds_.push_back(nextStateId_++);
    cursor_ = commands_.size();
}

bool CommandStack::discardLast()
{
    if (commands_.empty() || cursor_ != commands_.size()) return false;
    commands_.back()->undo();
    commands_.pop_back();
    stateIds_.pop_back();
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
    stateIds_.erase(
        stateIds_.begin() + static_cast<std::ptrdiff_t>(cursor_ + 1),
        stateIds_.end());
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
    stateIds_.resize(1);
    stateIds_.front() = nextStateId_++;
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

std::uint64_t CommandStack::stateId() const noexcept
{
    return stateIds_[cursor_];
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

bool ScenarioTruncationSummary::changesContent() const noexcept
{
    return clippedSegmentCount > 0
        || removedSegmentCount > 0
        || removedEventCount > 0
        || removedRelationCount > 0
        || clippedMarkerCount > 0
        || removedMarkerCount > 0;
}

TruncateScenarioDurationCommand::TruncateScenarioDurationCommand(
    Scenario& scenario,
    const Tick duration)
    : scenario_(&scenario)
    , before_(scenario)
    , after_(scenario)
{
    if (duration <= 0 || duration >= scenario.duration) {
        throw std::invalid_argument(
            "truncated scenario duration must be positive and smaller than the current duration");
    }
    after_.duration = duration;

    for (auto& lane : after_.lanes) {
        for (auto& segment : lane.segments) {
            if (segment.start < duration && segment.end > duration) {
                segment.end = duration;
                ++summary_.clippedSegmentCount;
            }
        }
        const auto beforeCount = lane.segments.size();
        std::erase_if(
            lane.segments,
            [duration](const Segment& segment) {
                return segment.start >= duration;
            });
        summary_.removedSegmentCount += beforeCount - lane.segments.size();
    }

    std::vector<std::string> removedEventIds;
    for (const auto& event : after_.events) {
        if (event.tick >= duration) removedEventIds.push_back(event.id);
    }
    std::erase_if(
        after_.events,
        [duration](const Event& event) {
            return event.tick >= duration;
        });
    summary_.removedEventCount =
        before_.events.size() - after_.events.size();

    const auto relationCount = after_.relations.size();
    std::erase_if(
        after_.relations,
        [&removedEventIds](const Relation& relation) {
            const auto removed = [&removedEventIds](
                                     const std::string& eventId) {
                return std::find(
                           removedEventIds.begin(),
                           removedEventIds.end(),
                           eventId)
                    != removedEventIds.end();
            };
            return removed(relation.sourceEventId)
                || removed(relation.targetEventId);
        });
    summary_.removedRelationCount =
        relationCount - after_.relations.size();

    for (auto& marker : after_.markers) {
        if (marker.start < duration && marker.end > duration) {
            marker.end = duration;
            ++summary_.clippedMarkerCount;
        }
    }
    const auto markerCount = after_.markers.size();
    std::erase_if(
        after_.markers,
        [duration](const Marker& marker) {
            if (marker.start == marker.end) {
                return marker.start > duration;
            }
            return marker.start >= duration;
        });
    summary_.removedMarkerCount =
        markerCount - after_.markers.size();
}

void TruncateScenarioDurationCommand::redo()
{
    *scenario_ = after_;
}

void TruncateScenarioDurationCommand::undo()
{
    *scenario_ = before_;
}

std::string TruncateScenarioDurationCommand::description() const
{
    return "Truncate scenario duration";
}

const ScenarioTruncationSummary&
TruncateScenarioDurationCommand::summary() const noexcept
{
    return summary_;
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
    const auto validation = validateLaneValue(*lane, value_);
    if (!validation.valid) throw std::invalid_argument(validation.error);
    value_ = validation.normalizedValue;
    before_ = lane->segments;
    eventsBefore_ = scenario.events;
    relationsBefore_ = scenario.relations;
}

void SetLaneRangeCommand::redo()
{
    auto* lane = findLane(*scenario_, laneId_);
    if (!lane) {
        throw std::runtime_error("lane was removed before command execution");
    }
    if (!initialized_) {
        if (rangeAlreadyEquals(*lane, start_, end_, value_, extensions_)) {
            after_ = before_;
            eventsAfter_ = eventsBefore_;
            relationsAfter_ = relationsBefore_;
            initialized_ = true;
            return;
        }
        setSegmentRange(*lane, start_, end_, value_, {}, extensions_);
        if (lane->kind == LaneKind::Bit
            || lane->kind == LaneKind::Bus
            || lane->kind == LaneKind::Enum) {
            synchronizeLaneEventsFromSegments(*scenario_, laneId_);
        }
        after_ = lane->segments;
        eventsAfter_ = scenario_->events;
        relationsAfter_ = scenario_->relations;
        initialized_ = true;
        return;
    }
    lane->segments = after_;
    scenario_->events = eventsAfter_;
    scenario_->relations = relationsAfter_;
}

void SetLaneRangeCommand::undo()
{
    auto* lane = findLane(*scenario_, laneId_);
    if (!lane) {
        throw std::runtime_error("lane was removed before undo");
    }
    lane->segments = before_;
    scenario_->events = eventsBefore_;
    scenario_->relations = relationsBefore_;
}

std::string SetLaneRangeCommand::description() const
{
    return "Set lane range";
}

bool SetLaneRangeCommand::hasEffect() const noexcept
{
    return initialized_
        && (before_ != after_
            || eventsBefore_ != eventsAfter_
            || relationsBefore_ != relationsAfter_);
}

SetLaneSequenceCommand::SetLaneSequenceCommand(
    Scenario& scenario,
    std::string laneId,
    std::vector<LaneSequenceStep> steps)
    : scenario_(&scenario)
    , laneId_(std::move(laneId))
    , steps_(std::move(steps))
    , before_(scenario)
    , after_(scenario)
{
    if (steps_.empty() || steps_.size() > 100'000) {
        throw std::invalid_argument(
            "lane sequence must contain from 1 to 100000 values");
    }
    const auto* sourceLane = findLane(scenario, laneId_);
    auto* targetLane = findLane(after_, laneId_);
    if (!sourceLane || !targetLane || sourceLane->kind == LaneKind::Group) {
        throw std::invalid_argument("lane sequence target does not exist");
    }
    if (steps_.front().start < 0
        || steps_.front().start >= scenario.duration) {
        throw std::invalid_argument(
            "lane sequence start is outside the scenario");
    }

    auto expectedStart = steps_.front().start;
    for (auto& step : steps_) {
        if (step.start != expectedStart || step.end <= step.start) {
            throw std::invalid_argument(
                "lane sequence ranges must be positive and contiguous");
        }
        const auto validation =
            validateLaneValue(*sourceLane, step.value);
        if (!validation.valid) {
            throw std::invalid_argument(validation.error);
        }
        step.value = validation.normalizedValue;
        expectedStart = step.end;
    }

    after_.duration = std::max(after_.duration, steps_.back().end);
    for (const auto& step : steps_) {
        if (rangeAlreadyEquals(
                *targetLane,
                step.start,
                step.end,
                step.value,
                step.extensions)) {
            continue;
        }
        setSegmentRange(
            *targetLane,
            step.start,
            step.end,
            step.value,
            {},
            step.extensions);
    }
    if (targetLane->kind == LaneKind::Bit
        || targetLane->kind == LaneKind::Bus
        || targetLane->kind == LaneKind::Enum) {
        synchronizeLaneEventsFromSegments(after_, laneId_);
    }
}

void SetLaneSequenceCommand::redo()
{
    *scenario_ = after_;
}

void SetLaneSequenceCommand::undo()
{
    *scenario_ = before_;
}

std::string SetLaneSequenceCommand::description() const
{
    return "Set lane sequence";
}

bool SetLaneSequenceCommand::hasEffect() const noexcept
{
    return before_ != after_;
}

SetLaneSequencesCommand::SetLaneSequencesCommand(
    Scenario& scenario,
    std::vector<LaneSequenceAssignment> assignments)
    : scenario_(&scenario)
    , assignments_(std::move(assignments))
    , before_(scenario)
    , after_(scenario)
{
    if (assignments_.empty()) {
        throw std::invalid_argument(
            "lane sequence batch contains no assignments");
    }

    std::vector<std::string> laneIds;
    laneIds.reserve(assignments_.size());
    std::size_t totalStepCount = 0;
    for (auto& assignment : assignments_) {
        if (assignment.laneId.empty()
            || std::find(
                   laneIds.begin(),
                   laneIds.end(),
                   assignment.laneId)
                != laneIds.end()) {
            throw std::invalid_argument(
                "lane sequence batch contains an invalid or duplicate target");
        }
        if (assignment.steps.empty()) {
            throw std::invalid_argument(
                "lane sequence batch contains an empty sequence");
        }
        if (totalStepCount > 100'000
            || assignment.steps.size()
                > 100'000 - totalStepCount) {
            throw std::invalid_argument(
                "lane sequence batch exceeds 100000 values");
        }
        totalStepCount += assignment.steps.size();

        const auto* sourceLane =
            findLane(scenario, assignment.laneId);
        auto* targetLane =
            findLane(after_, assignment.laneId);
        if (!sourceLane || !targetLane
            || sourceLane->kind == LaneKind::Group) {
            throw std::invalid_argument(
                "lane sequence batch target does not exist");
        }
        if (assignment.steps.front().start < 0
            || assignment.steps.front().start
                >= scenario.duration) {
            throw std::invalid_argument(
                "lane sequence batch start is outside the scenario");
        }

        auto expectedStart =
            assignment.steps.front().start;
        for (auto& step : assignment.steps) {
            if (step.start != expectedStart
                || step.end <= step.start) {
                throw std::invalid_argument(
                    "lane sequence batch ranges must be positive and contiguous");
            }
            const auto validation =
                validateLaneValue(*sourceLane, step.value);
            if (!validation.valid) {
                throw std::invalid_argument(validation.error);
            }
            step.value = validation.normalizedValue;
            expectedStart = step.end;
        }

        after_.duration = std::max(
            after_.duration,
            assignment.steps.back().end);
        for (const auto& step : assignment.steps) {
            if (rangeAlreadyEquals(
                    *targetLane,
                    step.start,
                    step.end,
                    step.value,
                    step.extensions)) {
                continue;
            }
            setSegmentRange(
                *targetLane,
                step.start,
                step.end,
                step.value,
                {},
                step.extensions);
        }
        if (targetLane->kind == LaneKind::Bit
            || targetLane->kind == LaneKind::Bus
            || targetLane->kind == LaneKind::Enum) {
            synchronizeLaneEventsFromSegments(
                after_,
                assignment.laneId);
        }
        laneIds.push_back(assignment.laneId);
    }
}

void SetLaneSequencesCommand::redo()
{
    *scenario_ = after_;
}

void SetLaneSequencesCommand::undo()
{
    *scenario_ = before_;
}

std::string SetLaneSequencesCommand::description() const
{
    return "Set lane sequences";
}

bool SetLaneSequencesCommand::hasEffect() const noexcept
{
    return before_ != after_;
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
        if (rangeAlreadyEquals(
                *lane,
                start_,
                end_,
                assignment.value,
                assignment.extensions)) {
            continue;
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

bool SetLaneRangesCommand::hasEffect() const noexcept
{
    return before_ && after_ && *before_ != *after_;
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

bool ClearLaneRangesCommand::hasEffect() const noexcept
{
    return before_ && after_ && *before_ != *after_;
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

bool ClearLaneRangeCommand::hasEffect() const noexcept
{
    return before_ && after_ && *before_ != *after_;
}

EditSegmentCommand::EditSegmentCommand(
    Scenario& scenario,
    std::string laneId,
    std::string segmentId,
    const Tick start,
    const Tick end,
    std::string value,
    std::optional<JsonExtensions> extensions)
    : scenario_(&scenario)
    , laneId_(std::move(laneId))
    , segmentId_(std::move(segmentId))
    , start_(start)
    , end_(end)
    , value_(std::move(value))
    , extensions_(std::move(extensions))
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
        if (extensions_) segment->extensions = *extensions_;
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

bool EditSegmentCommand::hasEffect() const noexcept
{
    return before_ && after_ && *before_ != *after_;
}

CopySegmentCommand::CopySegmentCommand(
    Scenario& scenario,
    std::string laneId,
    std::string sourceSegmentId,
    const Tick start,
    const Tick end)
    : scenario_(&scenario)
    , laneId_(std::move(laneId))
    , start_(start)
    , end_(end)
{
    auto* lane = findLane(scenario, laneId_);
    if (!lane) throw std::invalid_argument("lane does not exist");
    const auto* source = findLinkedSegment(*lane, sourceSegmentId);
    if (!source) throw std::invalid_argument("source segment does not exist");
    if (lane->kind == LaneKind::Clock || lane->kind == LaneKind::Group) {
        throw std::invalid_argument("this lane does not support segment copy");
    }
    if (start_ < 0 || end_ <= start_ || end_ > scenario.duration) {
        throw std::invalid_argument("copied segment interval is invalid");
    }
    value_ = source->value;
    extensions_ = source->extensions;
}

void CopySegmentCommand::redo()
{
    snapshotRedo(*scenario_, before_, after_, [this] {
        auto* lane = findLane(*scenario_, laneId_);
        if (!lane) throw std::runtime_error("lane was removed before segment copy");
        if (rangeAlreadyEquals(*lane, start_, end_, value_, extensions_)) return;
        setSegmentRange(*lane, start_, end_, value_, {}, extensions_);
        if (lane->kind == LaneKind::Bit
            || lane->kind == LaneKind::Bus
            || lane->kind == LaneKind::Enum) {
            synchronizeLaneEventsFromSegments(*scenario_, laneId_);
        }
    });
}

void CopySegmentCommand::undo()
{
    if (!before_) throw std::runtime_error("segment copy command has not been executed");
    *scenario_ = *before_;
}

std::string CopySegmentCommand::description() const
{
    return "Copy segment";
}

bool CopySegmentCommand::hasEffect() const noexcept
{
    return before_ && after_ && *before_ != *after_;
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

AddLaneCommand::AddLaneCommand(
    Scenario& scenario,
    Lane lane,
    const std::optional<std::size_t> insertionIndex)
    : scenario_(&scenario)
    , lane_(std::move(lane))
    , insertionIndex_(insertionIndex.value_or(scenario.lanes.size()))
{
    if (lane_.id.empty()) {
        lane_.id = makeStableId("lane");
    }
}

AddLaneCommand::AddLaneCommand(
    Project& project,
    Scenario& scenario,
    Lane lane,
    ClockDomain clockDomain,
    const std::optional<std::size_t> insertionIndex)
    : AddLaneCommand(scenario, std::move(lane), insertionIndex)
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
    return lane_.kind == LaneKind::Group ? "Add group" : "Add lane";
}

CreateGroupWithLaneCommand::CreateGroupWithLaneCommand(
    Scenario& scenario,
    Lane group,
    std::string laneId)
    : scenario_(&scenario)
    , laneId_(std::move(laneId))
    , before_(scenario.lanes)
    , after_(before_)
{
    if (group.id.empty()) {
        throw std::invalid_argument("group id is empty");
    }
    if (group.name.empty() || isBlankText(group.name)) {
        throw std::invalid_argument("group name is empty");
    }
    if (group.kind != LaneKind::Group) {
        throw std::invalid_argument("new lane is not a group");
    }
    if (group.height < 30 || group.height > 240) {
        throw std::invalid_argument("group height must be between 30 and 240");
    }
    if (group.width != 1
        || group.isSigned
        || !group.enumMap.empty()
        || !group.groupId.empty()
        || !group.clockDomainId.empty()
        || !group.segments.empty()) {
        throw std::invalid_argument("group contains unsupported lane data");
    }
    if (std::any_of(
            after_.begin(),
            after_.end(),
            [&group](const Lane& candidate) {
                return candidate.id == group.id;
            })) {
        throw std::invalid_argument("group id already exists");
    }

    const auto laneMatches = std::count_if(
        after_.begin(),
        after_.end(),
        [this](const Lane& lane) {
            return lane.id == laneId_;
        });
    if (laneMatches != 1) {
        throw std::invalid_argument(
            laneMatches == 0
                ? "lane does not exist"
                : "lane id is ambiguous");
    }
    const auto source = std::find_if(
        after_.begin(),
        after_.end(),
        [this](const Lane& lane) {
            return lane.id == laneId_;
        });
    if (source->kind == LaneKind::Group) {
        throw std::invalid_argument("a group cannot be the first member of another group");
    }
    const auto insertionIndex = static_cast<std::size_t>(
        std::distance(after_.begin(), source));
    source->groupId = group.id;
    after_.insert(
        after_.begin() + static_cast<std::ptrdiff_t>(insertionIndex),
        std::move(group));
}

void CreateGroupWithLaneCommand::redo()
{
    scenario_->lanes = after_;
}

void CreateGroupWithLaneCommand::undo()
{
    scenario_->lanes = before_;
}

std::string CreateGroupWithLaneCommand::description() const
{
    return "Create group with signal";
}

CreateGroupWithLanesCommand::CreateGroupWithLanesCommand(
    Scenario& scenario,
    Lane group,
    std::vector<std::string> laneIds)
    : scenario_(&scenario)
    , laneIds_(std::move(laneIds))
    , before_(scenario.lanes)
{
    if (group.id.empty()) {
        throw std::invalid_argument("group id is empty");
    }
    if (group.name.empty() || isBlankText(group.name)) {
        throw std::invalid_argument("group name is empty");
    }
    if (group.kind != LaneKind::Group) {
        throw std::invalid_argument("new lane is not a group");
    }
    if (group.height < 30 || group.height > 240) {
        throw std::invalid_argument("group height must be between 30 and 240");
    }
    if (group.width != 1
        || group.isSigned
        || !group.enumMap.empty()
        || !group.groupId.empty()
        || !group.clockDomainId.empty()
        || !group.segments.empty()) {
        throw std::invalid_argument("group contains unsupported lane data");
    }
    if (std::any_of(
            before_.begin(),
            before_.end(),
            [&group](const Lane& candidate) {
                return candidate.id == group.id;
            })) {
        throw std::invalid_argument("group id already exists");
    }
    if (laneIds_.empty()) {
        throw std::invalid_argument("group requires at least one member signal");
    }
    for (auto index = std::size_t{0}; index < laneIds_.size(); ++index) {
        if (laneIds_[index].empty()) {
            throw std::invalid_argument("member lane id is empty");
        }
        if (std::find(
                laneIds_.begin(),
                laneIds_.begin() + static_cast<std::ptrdiff_t>(index),
                laneIds_[index])
            != laneIds_.begin() + static_cast<std::ptrdiff_t>(index)) {
            throw std::invalid_argument("member lane id is duplicated");
        }
        const auto matches = std::count_if(
            before_.begin(),
            before_.end(),
            [this, index](const Lane& lane) {
                return lane.id == laneIds_[index];
            });
        if (matches != 1) {
            throw std::invalid_argument(
                matches == 0
                    ? "member lane does not exist"
                    : "member lane id is ambiguous");
        }
        const auto* source = findLane(scenario, laneIds_[index]);
        if (!source || source->kind == LaneKind::Group) {
            throw std::invalid_argument(
                "a group cannot be a member of another group");
        }
    }

    const auto selected = [this](const Lane& lane) {
        return std::find(laneIds_.begin(), laneIds_.end(), lane.id)
            != laneIds_.end();
    };
    const auto firstMember = std::find_if(
        before_.begin(),
        before_.end(),
        selected);
    if (firstMember == before_.end()) {
        throw std::invalid_argument("group member signals are unavailable");
    }

    std::vector<Lane> members;
    members.reserve(laneIds_.size());
    for (const auto& lane : before_) {
        if (!selected(lane)) continue;
        auto member = lane;
        member.groupId = group.id;
        members.push_back(std::move(member));
    }

    after_.reserve(before_.size() + 1);
    for (auto iterator = before_.begin(); iterator != before_.end(); ++iterator) {
        if (iterator == firstMember) {
            after_.push_back(group);
            after_.insert(
                after_.end(),
                std::make_move_iterator(members.begin()),
                std::make_move_iterator(members.end()));
        }
        if (!selected(*iterator)) after_.push_back(*iterator);
    }
}

void CreateGroupWithLanesCommand::redo()
{
    scenario_->lanes = after_;
}

void CreateGroupWithLanesCommand::undo()
{
    scenario_->lanes = before_;
}

std::string CreateGroupWithLanesCommand::description() const
{
    return laneIds_.size() == 1
        ? "Create group with signal"
        : "Create group with selected signals";
}

DuplicateLaneCommand::DuplicateLaneCommand(
    Scenario& scenario,
    Lane lane,
    const std::size_t insertionIndex)
    : addLaneCommand_(scenario, std::move(lane), insertionIndex)
{
}

DuplicateLaneCommand::DuplicateLaneCommand(
    Project& project,
    Scenario& scenario,
    Lane lane,
    ClockDomain clockDomain,
    const std::size_t insertionIndex)
    : addLaneCommand_(
          project,
          scenario,
          std::move(lane),
          std::move(clockDomain),
          insertionIndex)
{
}

void DuplicateLaneCommand::redo()
{
    addLaneCommand_.redo();
}

void DuplicateLaneCommand::undo()
{
    addLaneCommand_.undo();
}

std::string DuplicateLaneCommand::description() const
{
    return "Duplicate lane";
}

DuplicateLanesCommand::DuplicateLanesCommand(
    Project& project,
    Scenario& scenario,
    std::vector<Lane> lanes,
    std::vector<ClockDomain> clockDomains,
    const std::size_t insertionIndex)
    : project_(&project)
    , scenario_(&scenario)
    , beforeLanes_(scenario.lanes)
    , afterLanes_(beforeLanes_)
    , beforeClockDomains_(project.clockDomains)
    , afterClockDomains_(beforeClockDomains_)
    , duplicateCount_(lanes.size())
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
    if (lanes.empty()) {
        throw std::invalid_argument(
            "lane duplication requires at least one signal");
    }
    if (insertionIndex > beforeLanes_.size()) {
        throw std::invalid_argument(
            "lane insertion index is outside the scenario");
    }

    std::vector<std::string> newLaneIds;
    std::vector<std::string> newSegmentIds;
    newLaneIds.reserve(lanes.size());
    for (const auto& lane : lanes) {
        if (lane.id.empty()) {
            throw std::invalid_argument("duplicate lane id is empty");
        }
        if (lane.name.empty() || isBlankText(lane.name)) {
            throw std::invalid_argument("duplicate lane name is empty");
        }
        if (lane.kind == LaneKind::Group) {
            throw std::invalid_argument(
                "batch lane duplication accepts signals only");
        }
        if (std::find(
                newLaneIds.begin(),
                newLaneIds.end(),
                lane.id)
            != newLaneIds.end()) {
            throw std::invalid_argument(
                "duplicate lane identity is repeated");
        }
        const auto identityAlreadyUsed = std::any_of(
            project.scenarios.begin(),
            project.scenarios.end(),
            [&lane](const Scenario& candidate) {
                return findLane(candidate, lane.id) != nullptr;
            });
        if (identityAlreadyUsed) {
            throw std::invalid_argument(
                "duplicate lane identity is already in use");
        }
        if (!lane.groupId.empty()) {
            const auto groupMatches = std::count_if(
                scenario.lanes.begin(),
                scenario.lanes.end(),
                [&lane](const Lane& candidate) {
                    return candidate.id == lane.groupId
                        && candidate.kind == LaneKind::Group;
                });
            if (groupMatches != 1) {
                throw std::invalid_argument(
                    "duplicate lane group is unavailable");
            }
        }
        if (lane.kind != LaneKind::Clock
            && !lane.clockDomainId.empty()
            && !findClock(project, lane.clockDomainId)) {
            throw std::invalid_argument(
                "duplicate lane clock domain is unavailable");
        }
        for (const auto& segment : lane.segments) {
            if (segment.id.empty()) {
                throw std::invalid_argument(
                    "duplicate segment id is empty");
            }
            const auto segmentAlreadyUsed = std::any_of(
                scenario.lanes.begin(),
                scenario.lanes.end(),
                [&segment](const Lane& existingLane) {
                    return std::any_of(
                        existingLane.segments.begin(),
                        existingLane.segments.end(),
                        [&segment](const Segment& existingSegment) {
                            return existingSegment.id == segment.id;
                        });
                });
            if (segmentAlreadyUsed
                || std::find(
                       newSegmentIds.begin(),
                       newSegmentIds.end(),
                       segment.id)
                    != newSegmentIds.end()) {
                throw std::invalid_argument(
                    "duplicate segment identity is already in use");
            }
            newSegmentIds.push_back(segment.id);
        }
        newLaneIds.push_back(lane.id);
    }

    std::vector<std::string> newClockIds;
    newClockIds.reserve(clockDomains.size());
    for (const auto& clock : clockDomains) {
        if (clock.id.empty()) {
            throw std::invalid_argument(
                "duplicate clock domain id is empty");
        }
        if (!clock.isValid()) {
            throw std::invalid_argument(
                "duplicate clock domain is invalid");
        }
        if (findClock(project, clock.id)
            || std::find(
                   newClockIds.begin(),
                   newClockIds.end(),
                   clock.id)
                != newClockIds.end()) {
            throw std::invalid_argument(
                "duplicate clock domain identity is already in use");
        }
        const auto matchingClockLaneCount = std::count_if(
            lanes.begin(),
            lanes.end(),
            [&clock](const Lane& lane) {
                return lane.kind == LaneKind::Clock
                    && lane.clockDomainId == clock.id;
            });
        if (matchingClockLaneCount != 1) {
            throw std::invalid_argument(
                "duplicate clock domain must belong to one clock signal");
        }
        newClockIds.push_back(clock.id);
    }
    for (const auto& lane : lanes) {
        if (lane.kind != LaneKind::Clock) continue;
        const auto matchingClockDomainCount = std::count_if(
            clockDomains.begin(),
            clockDomains.end(),
            [&lane](const ClockDomain& clock) {
                return clock.id == lane.clockDomainId;
            });
        if (matchingClockDomainCount != 1) {
            throw std::invalid_argument(
                "duplicate clock signal requires an independent clock domain");
        }
    }

    afterLanes_.insert(
        afterLanes_.begin()
            + static_cast<std::ptrdiff_t>(insertionIndex),
        lanes.begin(),
        lanes.end());
    afterClockDomains_.insert(
        afterClockDomains_.end(),
        clockDomains.begin(),
        clockDomains.end());
}

void DuplicateLanesCommand::redo()
{
    scenario_->lanes = afterLanes_;
    project_->clockDomains = afterClockDomains_;
}

void DuplicateLanesCommand::undo()
{
    scenario_->lanes = beforeLanes_;
    project_->clockDomains = beforeClockDomains_;
}

std::string DuplicateLanesCommand::description() const
{
    return duplicateCount_ == 1
        ? "Duplicate lane"
        : "Duplicate selected signals";
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
    removesGroup_ = lane->kind == LaneKind::Group;

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
    return removesGroup_ ? "Remove group" : "Remove lane";
}

RemoveLanesCommand::RemoveLanesCommand(
    Project& project,
    Scenario& scenario,
    std::vector<std::string> laneIds)
    : project_(&project)
    , scenario_(&scenario)
    , laneIds_(std::move(laneIds))
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
    if (laneIds_.empty()) {
        throw std::invalid_argument("at least one lane is required");
    }
    std::vector<std::string> uniqueIds;
    uniqueIds.reserve(laneIds_.size());
    for (const auto& laneId : laneIds_) {
        if (std::find(uniqueIds.begin(), uniqueIds.end(), laneId)
            != uniqueIds.end()) {
            throw std::invalid_argument("lane identity is duplicated");
        }
        const auto matchingLaneCount = static_cast<std::size_t>(std::count_if(
            scenario.lanes.begin(),
            scenario.lanes.end(),
            [&laneId](const Lane& lane) {
                return lane.id == laneId;
            }));
        if (matchingLaneCount != 1) {
            throw std::invalid_argument(
                matchingLaneCount == 0
                    ? "lane does not exist"
                    : "lane identity is ambiguous");
        }
        const auto* lane = findLane(scenario, laneId);
        if (lane->kind == LaneKind::Group) {
            throw std::invalid_argument(
                "batch lane removal does not accept groups");
        }
        uniqueIds.push_back(laneId);
    }

    const auto selected = [this](const std::string& laneId) {
        return std::find(laneIds_.begin(), laneIds_.end(), laneId)
            != laneIds_.end();
    };
    std::vector<std::string> removedEventIds;
    for (const auto& event : afterScenario_.events) {
        if (selected(event.laneId)) removedEventIds.push_back(event.id);
    }
    std::erase_if(
        afterScenario_.events,
        [&selected](const Event& event) {
            return selected(event.laneId);
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
        [&selected](const Lane& lane) {
            return selected(lane.id);
        });

    for (const auto& laneId : laneIds_) {
        const auto usedByAnotherScenario = std::any_of(
            project.scenarios.begin(),
            project.scenarios.end(),
            [&scenario, &laneId](const Scenario& candidate) {
                return &candidate != &scenario
                    && findLane(candidate, laneId);
            });
        if (usedByAnotherScenario) continue;
        for (const auto& trace : project.importedTraces) {
            const auto mapping = trace.signalMapping.find(laneId);
            if (mapping != trace.signalMapping.end()) {
                removedTraceMappings_.push_back({
                    trace.id,
                    laneId,
                    mapping->second,
                });
            }
        }
    }
}

void RemoveLanesCommand::redo()
{
    *scenario_ = afterScenario_;
    for (auto& trace : project_->importedTraces) {
        for (const auto& mapping : removedTraceMappings_) {
            if (mapping.traceId == trace.id) {
                trace.signalMapping.erase(mapping.laneId);
            }
        }
    }
}

void RemoveLanesCommand::undo()
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
            trace->signalMapping[mapping.laneId] = mapping.signalId;
        }
    }
}

std::string RemoveLanesCommand::description() const
{
    return laneIds_.size() == 1
        ? "Remove lane"
        : "Remove selected signals";
}

RemoveTraceMappingAtIndexCommand::
    RemoveTraceMappingAtIndexCommand(
        Project& project,
        const std::size_t traceIndex,
        ImportedTrace expected,
        std::string expectedLaneId)
    : project_(&project)
    , traceIndex_(traceIndex)
    , expected_(std::move(expected))
    , expectedLaneId_(std::move(expectedLaneId))
{
}

void RemoveTraceMappingAtIndexCommand::redo()
{
    if (after_) {
        project_->importedTraces = *after_;
        return;
    }
    before_ = project_->importedTraces;
    if (traceIndex_ >= project_->importedTraces.size()
        || project_->importedTraces.at(traceIndex_)
            != expected_) {
        throw std::invalid_argument(
            "imported trace repair reference is stale");
    }
    auto& trace = project_->importedTraces.at(traceIndex_);
    if (trace.signalMapping.erase(expectedLaneId_) != 1) {
        throw std::invalid_argument(
            "imported trace mapping does not exist");
    }
    after_ = project_->importedTraces;
}

void RemoveTraceMappingAtIndexCommand::undo()
{
    if (!before_) {
        throw std::runtime_error(
            "imported trace mapping command has not been executed");
    }
    project_->importedTraces = *before_;
}

std::string RemoveTraceMappingAtIndexCommand::description() const
{
    return "Remove invalid trace mapping";
}

bool RemoveTraceMappingAtIndexCommand::hasEffect() const noexcept
{
    return before_ && after_ && *before_ != *after_;
}

ChangeTraceIdentityAtIndexCommand::
    ChangeTraceIdentityAtIndexCommand(
        Project& project,
        const std::size_t traceIndex,
        ImportedTrace expected,
        std::string replacementId)
    : project_(&project)
    , traceIndex_(traceIndex)
    , expected_(std::move(expected))
    , replacementId_(std::move(replacementId))
{
}

void ChangeTraceIdentityAtIndexCommand::redo()
{
    if (after_) {
        project_->importedTraces = *after_;
        return;
    }
    before_ = project_->importedTraces;
    if (traceIndex_ >= project_->importedTraces.size()
        || project_->importedTraces.at(traceIndex_)
            != expected_) {
        throw std::invalid_argument(
            "imported trace repair reference is stale");
    }
    if (replacementId_.empty()) {
        throw std::invalid_argument(
            "replacement imported trace identity is empty");
    }
    const auto duplicate = std::any_of(
        project_->importedTraces.begin(),
        project_->importedTraces.end(),
        [this](const ImportedTrace& trace) {
            return &trace
                    != &project_->importedTraces.at(traceIndex_)
                && trace.id == replacementId_;
        });
    if (duplicate) {
        throw std::invalid_argument(
            "replacement imported trace identity is already in use");
    }
    project_->importedTraces.at(traceIndex_).id =
        replacementId_;
    after_ = project_->importedTraces;
}

void ChangeTraceIdentityAtIndexCommand::undo()
{
    if (!before_) {
        throw std::runtime_error(
            "imported trace identity command has not been executed");
    }
    project_->importedTraces = *before_;
}

std::string ChangeTraceIdentityAtIndexCommand::description() const
{
    return "Repair imported trace identity";
}

bool ChangeTraceIdentityAtIndexCommand::hasEffect() const noexcept
{
    return before_ && after_ && *before_ != *after_;
}

ChangeTraceSourceAtIndexCommand::
    ChangeTraceSourceAtIndexCommand(
        Project& project,
        const std::size_t traceIndex,
        ImportedTrace expected,
        std::string replacementPath,
        std::string replacementFormat)
    : project_(&project)
    , traceIndex_(traceIndex)
    , expected_(std::move(expected))
    , replacementPath_(std::move(replacementPath))
    , replacementFormat_(std::move(replacementFormat))
{
}

void ChangeTraceSourceAtIndexCommand::redo()
{
    if (after_) {
        project_->importedTraces = *after_;
        return;
    }
    before_ = project_->importedTraces;
    if (traceIndex_ >= project_->importedTraces.size()
        || project_->importedTraces.at(traceIndex_)
            != expected_) {
        throw std::invalid_argument(
            "imported trace repair reference is stale");
    }
    if (replacementPath_.empty()
        || isBlankText(replacementPath_)) {
        throw std::invalid_argument(
            "replacement imported trace path is empty");
    }
    if (!isSupportedTraceFormat(
            replacementFormat_)) {
        throw std::invalid_argument(
            "replacement imported trace format is unsupported");
    }
    auto& trace =
        project_->importedTraces.at(traceIndex_);
    trace.path = replacementPath_;
    trace.format = replacementFormat_;
    after_ = project_->importedTraces;
}

void ChangeTraceSourceAtIndexCommand::undo()
{
    if (!before_) {
        throw std::runtime_error(
            "imported trace source command has not been executed");
    }
    project_->importedTraces = *before_;
}

std::string ChangeTraceSourceAtIndexCommand::description() const
{
    return "Repair imported trace source";
}

bool ChangeTraceSourceAtIndexCommand::hasEffect() const noexcept
{
    return before_ && after_ && *before_ != *after_;
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
    movesGroup_ = iterator->kind == LaneKind::Group;
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
    return movesGroup_ ? "Move group" : "Move lane";
}

MoveLanesCommand::MoveLanesCommand(
    Scenario& scenario,
    std::vector<std::string> laneIds,
    const std::size_t insertionSlot)
    : scenario_(&scenario)
    , laneIds_(std::move(laneIds))
    , before_(scenario.lanes)
    , after_(before_)
{
    if (laneIds_.empty()) {
        throw std::invalid_argument(
            "lane move requires at least one signal");
    }
    if (insertionSlot > before_.size()) {
        throw std::invalid_argument(
            "lane insertion slot is outside the scenario");
    }
    for (auto index = std::size_t{0}; index < laneIds_.size(); ++index) {
        const auto& laneId = laneIds_[index];
        if (laneId.empty()) {
            throw std::invalid_argument("lane id is empty");
        }
        if (std::find(
                laneIds_.begin(),
                laneIds_.begin() + static_cast<std::ptrdiff_t>(index),
                laneId)
            != laneIds_.begin() + static_cast<std::ptrdiff_t>(index)) {
            throw std::invalid_argument("lane id is duplicated");
        }
        const auto matches = std::count_if(
            before_.begin(),
            before_.end(),
            [&laneId](const Lane& lane) {
                return lane.id == laneId;
            });
        if (matches != 1) {
            throw std::invalid_argument(
                matches == 0
                    ? "lane does not exist"
                    : "lane id is ambiguous");
        }
        const auto* lane = findLane(scenario, laneId);
        if (!lane || lane->kind == LaneKind::Group) {
            throw std::invalid_argument(
                "batch lane move accepts signals only");
        }
    }

    const auto selected = [this](const Lane& lane) {
        return std::find(
                   laneIds_.begin(),
                   laneIds_.end(),
                   lane.id)
            != laneIds_.end();
    };
    std::vector<Lane> moved;
    moved.reserve(laneIds_.size());
    auto selectedBeforeSlot = std::size_t{0};
    for (auto index = std::size_t{0}; index < before_.size(); ++index) {
        if (!selected(before_[index])) continue;
        moved.push_back(before_[index]);
        if (index < insertionSlot) ++selectedBeforeSlot;
    }
    std::erase_if(after_, selected);
    const auto adjustedSlot = insertionSlot - selectedBeforeSlot;
    if (adjustedSlot > after_.size()) {
        throw std::invalid_argument(
            "lane insertion slot cannot be resolved");
    }
    after_.insert(
        after_.begin() + static_cast<std::ptrdiff_t>(adjustedSlot),
        std::make_move_iterator(moved.begin()),
        std::make_move_iterator(moved.end()));
}

void MoveLanesCommand::redo()
{
    scenario_->lanes = after_;
}

void MoveLanesCommand::undo()
{
    scenario_->lanes = before_;
}

std::string MoveLanesCommand::description() const
{
    return laneIds_.size() == 1
        ? "Move lane"
        : "Move selected signals";
}

bool MoveLanesCommand::hasEffect() const noexcept
{
    return before_ != after_;
}

SetLaneGroupCommand::SetLaneGroupCommand(
    Scenario& scenario,
    std::string laneId,
    std::string groupId)
    : scenario_(&scenario)
    , laneId_(std::move(laneId))
    , groupId_(std::move(groupId))
    , before_(scenario.lanes)
    , after_(before_)
{
    const auto laneMatches = std::count_if(
        after_.begin(),
        after_.end(),
        [this](const Lane& lane) {
            return lane.id == laneId_;
        });
    if (laneMatches != 1) {
        throw std::invalid_argument(
            laneMatches == 0
                ? "lane does not exist"
                : "lane id is ambiguous");
    }
    const auto laneIterator = std::find_if(
        after_.begin(),
        after_.end(),
        [this](const Lane& lane) {
            return lane.id == laneId_;
        });
    if (laneIterator->kind == LaneKind::Group) {
        throw std::invalid_argument("group lanes cannot belong to another group");
    }

    if (groupId_.empty()) {
        laneIterator->groupId.clear();
        return;
    }

    const auto groupMatches = std::count_if(
        after_.begin(),
        after_.end(),
        [this](const Lane& lane) {
            return lane.id == groupId_;
        });
    if (groupMatches != 1) {
        throw std::invalid_argument(
            groupMatches == 0
                ? "group does not exist"
                : "group id is ambiguous");
    }
    const auto groupIterator = std::find_if(
        after_.begin(),
        after_.end(),
        [this](const Lane& lane) {
            return lane.id == groupId_;
        });
    if (groupIterator->kind != LaneKind::Group) {
        throw std::invalid_argument("lane group target is not a group");
    }
    if (laneIterator->groupId == groupId_) return;

    Lane moved = std::move(*laneIterator);
    moved.groupId = groupId_;
    after_.erase(laneIterator);

    const auto target = std::find_if(
        after_.begin(),
        after_.end(),
        [this](const Lane& lane) {
            return lane.id == groupId_;
        });
    auto insertion = std::next(target);
    for (auto candidate = std::next(target);
         candidate != after_.end();
         ++candidate) {
        if (candidate->groupId == groupId_) {
            insertion = std::next(candidate);
        }
    }
    after_.insert(insertion, std::move(moved));
}

void SetLaneGroupCommand::redo()
{
    scenario_->lanes = after_;
}

void SetLaneGroupCommand::undo()
{
    scenario_->lanes = before_;
}

std::string SetLaneGroupCommand::description() const
{
    return groupId_.empty()
        ? "Remove signal from group"
        : "Move signal to group";
}

bool SetLaneGroupCommand::hasEffect() const noexcept
{
    return before_ != after_;
}

SetLanesGroupCommand::SetLanesGroupCommand(
    Scenario& scenario,
    std::vector<std::string> laneIds,
    std::string groupId)
    : scenario_(&scenario)
    , laneIds_(std::move(laneIds))
    , groupId_(std::move(groupId))
    , before_(scenario.lanes)
    , after_(before_)
{
    if (laneIds_.empty()) {
        throw std::invalid_argument("group assignment requires at least one signal");
    }
    for (auto index = std::size_t{0}; index < laneIds_.size(); ++index) {
        if (laneIds_[index].empty()) {
            throw std::invalid_argument("lane id is empty");
        }
        if (std::find(
                laneIds_.begin(),
                laneIds_.begin() + static_cast<std::ptrdiff_t>(index),
                laneIds_[index])
            != laneIds_.begin() + static_cast<std::ptrdiff_t>(index)) {
            throw std::invalid_argument("lane id is duplicated");
        }
        const auto matches = std::count_if(
            after_.begin(),
            after_.end(),
            [this, index](const Lane& lane) {
                return lane.id == laneIds_[index];
            });
        if (matches != 1) {
            throw std::invalid_argument(
                matches == 0
                    ? "lane does not exist"
                    : "lane id is ambiguous");
        }
        const auto* lane = findLane(scenario, laneIds_[index]);
        if (!lane || lane->kind == LaneKind::Group) {
            throw std::invalid_argument(
                "group lanes cannot belong to another group");
        }
    }

    const auto selected = [this](const Lane& lane) {
        return std::find(laneIds_.begin(), laneIds_.end(), lane.id)
            != laneIds_.end();
    };
    if (groupId_.empty()) {
        for (auto& lane : after_) {
            if (selected(lane)) lane.groupId.clear();
        }
        return;
    }

    const auto groupMatches = std::count_if(
        after_.begin(),
        after_.end(),
        [this](const Lane& lane) {
            return lane.id == groupId_;
        });
    if (groupMatches != 1) {
        throw std::invalid_argument(
            groupMatches == 0
                ? "group does not exist"
                : "group id is ambiguous");
    }
    const auto* group = findLane(scenario, groupId_);
    if (!group || group->kind != LaneKind::Group) {
        throw std::invalid_argument("lane group target is not a group");
    }

    std::vector<Lane> moved;
    moved.reserve(laneIds_.size());
    for (const auto& lane : after_) {
        if (selected(lane) && lane.groupId != groupId_) {
            auto member = lane;
            member.groupId = groupId_;
            moved.push_back(std::move(member));
        }
    }
    if (moved.empty()) return;
    std::erase_if(
        after_,
        [this, &selected](const Lane& lane) {
            return selected(lane) && lane.groupId != groupId_;
        });

    const auto target = std::find_if(
        after_.begin(),
        after_.end(),
        [this](const Lane& lane) {
            return lane.id == groupId_;
        });
    auto insertion = std::next(target);
    for (auto candidate = std::next(target);
         candidate != after_.end();
         ++candidate) {
        if (candidate->groupId == groupId_) {
            insertion = std::next(candidate);
        }
    }
    after_.insert(
        insertion,
        std::make_move_iterator(moved.begin()),
        std::make_move_iterator(moved.end()));
}

void SetLanesGroupCommand::redo()
{
    scenario_->lanes = after_;
}

void SetLanesGroupCommand::undo()
{
    scenario_->lanes = before_;
}

std::string SetLanesGroupCommand::description() const
{
    if (laneIds_.size() == 1) {
        return groupId_.empty()
            ? "Remove signal from group"
            : "Move signal to group";
    }
    return groupId_.empty()
        ? "Remove selected signals from groups"
        : "Move selected signals to group";
}

bool SetLanesGroupCommand::hasEffect() const noexcept
{
    return before_ != after_;
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
    return before_.kind == LaneKind::Group ? "Change group" : "Change lane";
}

bool ChangeLaneCommand::hasEffect() const noexcept
{
    return before_ != after_;
}

RepairLaneClockReferenceCommand::
    RepairLaneClockReferenceCommand(
        const Project& project,
        Scenario& scenario,
        std::string laneId)
    : project_(&project)
    , scenario_(&scenario)
    , laneId_(std::move(laneId))
{
}

void RepairLaneClockReferenceCommand::redo()
{
    snapshotRedo(*scenario_, before_, after_, [this] {
        Lane* lane = nullptr;
        std::size_t laneMatches = 0;
        for (auto& candidate :
             scenario_->lanes) {
            if (candidate.id != laneId_) continue;
            lane = &candidate;
            ++laneMatches;
        }
        if (laneMatches != 1 || !lane) {
            throw std::invalid_argument(
                laneMatches == 0
                    ? "lane does not exist"
                    : "lane stable ID is ambiguous");
        }

        const auto currentClockMatches =
            lane->clockDomainId.empty()
            ? std::size_t{0}
            : static_cast<std::size_t>(
                  std::count_if(
                      project_->clockDomains.begin(),
                      project_->clockDomains.end(),
                      [lane](const ClockDomain& clock) {
                          return clock.id
                              == lane->clockDomainId;
                      }));
        const auto invalidGroupReference =
            lane->kind == LaneKind::Group
            && !lane->clockDomainId.empty();
        const auto invalidClockReference =
            lane->kind == LaneKind::Clock
            && (lane->clockDomainId.empty()
                || currentClockMatches != 1);
        const auto invalidOptionalReference =
            lane->kind != LaneKind::Clock
            && lane->kind != LaneKind::Group
            && !lane->clockDomainId.empty()
            && currentClockMatches != 1;
        if (!invalidGroupReference
            && !invalidClockReference
            && !invalidOptionalReference) {
            throw std::invalid_argument(
                "lane clock reference is already valid");
        }

        if (lane->kind == LaneKind::Clock) {
            if (project_->clockDomains.size() != 1
                || project_->clockDomains.front()
                       .id.empty()) {
                throw std::invalid_argument(
                    "Clock Lane has no unambiguous replacement ClockDomain");
            }
            lane->clockDomainId =
                project_->clockDomains.front().id;
        } else {
            if (lane->kind != LaneKind::Group
                && currentClockMatches > 1) {
                throw std::invalid_argument(
                    "lane clock reference is ambiguous");
            }
            lane->clockDomainId.clear();
        }

        for (auto& event :
             scenario_->events) {
            if (event.laneId != lane->id
                || !event.clockDomainId.empty()
                || !event.cycle) {
                continue;
            }
            const ClockDomain* clock = nullptr;
            std::size_t clockMatches = 0;
            for (const auto& candidate :
                 project_->clockDomains) {
                if (candidate.id
                    != lane->clockDomainId) {
                    continue;
                }
                clock = &candidate;
                ++clockMatches;
            }
            const auto cycleTick =
                clockMatches == 1 && clock
                && *event.cycle >= 0
                ? tickAtCycle(
                      *clock,
                      *event.cycle,
                      clock->activeEdge)
                : std::optional<Tick>{};
            if (!cycleTick
                || *cycleTick != event.tick) {
                event.cycle.reset();
            }
        }
    });
}

void RepairLaneClockReferenceCommand::undo()
{
    if (!before_) {
        throw std::runtime_error(
            "lane clock repair has not been executed");
    }
    *scenario_ = *before_;
}

std::string RepairLaneClockReferenceCommand::
    description() const
{
    return "Repair lane clock reference";
}

bool RepairLaneClockReferenceCommand::
    hasEffect() const noexcept
{
    return before_ && after_ && *before_ != *after_;
}

RepairLaneGroupReferenceCommand::
    RepairLaneGroupReferenceCommand(
        Scenario& scenario,
        std::string laneId)
    : scenario_(&scenario)
    , laneId_(std::move(laneId))
{
}

void RepairLaneGroupReferenceCommand::redo()
{
    snapshotRedo(*scenario_, before_, after_, [this] {
        Lane* lane = nullptr;
        std::size_t laneMatches = 0;
        for (auto& candidate : scenario_->lanes) {
            if (candidate.id != laneId_) continue;
            lane = &candidate;
            ++laneMatches;
        }
        if (laneMatches != 1 || !lane) {
            throw std::invalid_argument(
                laneMatches == 0
                    ? "lane does not exist"
                    : "lane stable ID is ambiguous");
        }
        if (lane->groupId.empty()) {
            throw std::invalid_argument(
                "lane group reference is already valid");
        }

        const Lane* target = nullptr;
        std::size_t targetMatches = 0;
        for (const auto& candidate : scenario_->lanes) {
            if (candidate.id != lane->groupId) continue;
            target = &candidate;
            ++targetMatches;
        }
        const auto valid =
            lane->kind != LaneKind::Group
            && targetMatches == 1
            && target
            && target->kind == LaneKind::Group
            && target->id != lane->id;
        if (valid) {
            throw std::invalid_argument(
                "lane group reference is already valid");
        }
        lane->groupId.clear();
    });
}

void RepairLaneGroupReferenceCommand::undo()
{
    if (!before_) {
        throw std::runtime_error(
            "lane group repair has not been executed");
    }
    *scenario_ = *before_;
}

std::string RepairLaneGroupReferenceCommand::
    description() const
{
    return "Repair lane group reference";
}

bool RepairLaneGroupReferenceCommand::
    hasEffect() const noexcept
{
    return before_ && after_ && *before_ != *after_;
}

HideLaneCommand::HideLaneCommand(Scenario& scenario, std::string laneId)
    : scenario_(&scenario)
    , laneId_(std::move(laneId))
{
    const auto* lane = findLane(scenario, laneId_);
    if (!lane) throw std::invalid_argument("lane does not exist");
    wasVisible_ = lane->visible;
    hidesGroup_ = lane->kind == LaneKind::Group;
}

void HideLaneCommand::redo()
{
    auto* lane = findLane(*scenario_, laneId_);
    if (!lane) throw std::runtime_error("lane was removed before command execution");
    lane->visible = false;
}

void HideLaneCommand::undo()
{
    auto* lane = findLane(*scenario_, laneId_);
    if (!lane) throw std::runtime_error("lane was removed before undo");
    lane->visible = wasVisible_;
}

std::string HideLaneCommand::description() const
{
    return hidesGroup_ ? "Hide group" : "Hide lane";
}

bool HideLaneCommand::hasEffect() const noexcept
{
    return wasVisible_;
}

HideLanesCommand::HideLanesCommand(
    Scenario& scenario,
    std::vector<std::string> laneIds)
    : scenario_(&scenario)
{
    if (laneIds.empty()) {
        throw std::invalid_argument("at least one lane is required");
    }
    before_.reserve(laneIds.size());
    for (auto& laneId : laneIds) {
        if (std::any_of(
                before_.begin(),
                before_.end(),
                [&laneId](const Visibility& visibility) {
                    return visibility.laneId == laneId;
                })) {
            throw std::invalid_argument("lane identity is duplicated");
        }
        const auto matchingLaneCount = static_cast<std::size_t>(std::count_if(
            scenario.lanes.begin(),
            scenario.lanes.end(),
            [&laneId](const Lane& lane) {
                return lane.id == laneId;
            }));
        if (matchingLaneCount != 1) {
            throw std::invalid_argument(
                matchingLaneCount == 0
                    ? "lane does not exist"
                    : "lane identity is ambiguous");
        }
        const auto* lane = findLane(scenario, laneId);
        if (lane->kind == LaneKind::Group) {
            throw std::invalid_argument(
                "batch lane hiding does not accept groups");
        }
        before_.push_back({std::move(laneId), lane->visible});
    }
}

void HideLanesCommand::redo()
{
    for (const auto& visibility : before_) {
        if (!findLane(*scenario_, visibility.laneId)) {
            throw std::runtime_error(
                "lane was removed before command execution");
        }
    }
    for (const auto& visibility : before_) {
        findLane(*scenario_, visibility.laneId)->visible = false;
    }
}

void HideLanesCommand::undo()
{
    for (const auto& visibility : before_) {
        if (!findLane(*scenario_, visibility.laneId)) {
            throw std::runtime_error("lane was removed before undo");
        }
    }
    for (const auto& visibility : before_) {
        findLane(*scenario_, visibility.laneId)->visible =
            visibility.visible;
    }
}

std::string HideLanesCommand::description() const
{
    return before_.size() == 1
        ? "Hide lane"
        : "Hide selected signals";
}

bool HideLanesCommand::hasEffect() const noexcept
{
    return std::any_of(
        before_.begin(),
        before_.end(),
        [](const Visibility& visibility) {
            return visibility.visible;
        });
}

ShowLaneCommand::ShowLaneCommand(Scenario& scenario, std::string laneId)
    : scenario_(&scenario)
    , laneId_(std::move(laneId))
{
    const auto* lane = findLane(scenario, laneId_);
    if (!lane) throw std::invalid_argument("lane does not exist");
    wasVisible_ = lane->visible;
    showsGroup_ = lane->kind == LaneKind::Group;
}

void ShowLaneCommand::redo()
{
    auto* lane = findLane(*scenario_, laneId_);
    if (!lane) throw std::runtime_error("lane was removed before command execution");
    lane->visible = true;
}

void ShowLaneCommand::undo()
{
    auto* lane = findLane(*scenario_, laneId_);
    if (!lane) throw std::runtime_error("lane was removed before undo");
    lane->visible = wasVisible_;
}

std::string ShowLaneCommand::description() const
{
    return showsGroup_ ? "Show group" : "Show lane";
}

bool ShowLaneCommand::hasEffect() const noexcept
{
    return !wasVisible_;
}

ShowHiddenLanesCommand::ShowHiddenLanesCommand(Scenario& scenario)
    : scenario_(&scenario)
{
    for (const auto& lane : scenario.lanes) {
        if (!lane.visible) laneIds_.push_back(lane.id);
    }
}

void ShowHiddenLanesCommand::redo()
{
    for (const auto& laneId : laneIds_) {
        if (auto* lane = findLane(*scenario_, laneId)) lane->visible = true;
    }
}

void ShowHiddenLanesCommand::undo()
{
    for (const auto& laneId : laneIds_) {
        if (auto* lane = findLane(*scenario_, laneId)) lane->visible = false;
    }
}

std::string ShowHiddenLanesCommand::description() const
{
    return "Show hidden items";
}

bool ShowHiddenLanesCommand::hasEffect() const noexcept
{
    return !laneIds_.empty();
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

bool ChangeClockCommand::hasEffect() const noexcept
{
    return beforeClock_ != afterClock_ || beforeScenarios_ != afterScenarios_;
}

TransferRangeCommand::TransferRangeCommand(
    Scenario& scenario,
    std::vector<CopiedLaneRange> lanes,
    const Tick source,
    const Tick destination,
    const Tick duration,
    const RangeTransferMode mode)
    : scenario_(&scenario)
    , lanes_(std::move(lanes))
    , source_(source)
    , destination_(destination)
    , duration_(duration)
    , mode_(mode)
{
    if (lanes_.empty()) throw std::invalid_argument("range transfer contains no lanes");
    if (source_ < 0
        || duration_ <= 0
        || source_ > scenario.duration
        || duration_ > scenario.duration - source_
        || destination_ < 0
        || destination_ > scenario.duration
        || duration_ > std::numeric_limits<Tick>::max() - destination_) {
        throw std::invalid_argument("range transfer is outside the scenario");
    }
    const auto sourceEnd = source_ + duration_;
    const auto destinationEnd = destination_ + duration_;
    std::vector<std::string> uniqueSourceLaneIds;
    std::vector<std::string> uniqueTargetLaneIds;
    uniqueSourceLaneIds.reserve(lanes_.size());
    uniqueTargetLaneIds.reserve(lanes_.size());
    for (auto& copiedLane : lanes_) {
        const auto& sourceLaneId = copiedLane.sourceLaneId.empty()
            ? copiedLane.laneId
            : copiedLane.sourceLaneId;
        if (copiedLane.laneId.empty()
            || std::find(
                   uniqueTargetLaneIds.begin(),
                   uniqueTargetLaneIds.end(),
                   copiedLane.laneId)
                != uniqueTargetLaneIds.end()
            || std::find(
                   uniqueSourceLaneIds.begin(),
                   uniqueSourceLaneIds.end(),
                   sourceLaneId)
                != uniqueSourceLaneIds.end()) {
            throw std::invalid_argument(
                "range transfer contains an invalid or duplicate lane");
        }
        const auto* sourceLane = findLane(scenario, sourceLaneId);
        const auto* targetLane = findLane(scenario, copiedLane.laneId);
        if (!sourceLane
            || !targetLane
            || sourceLane->kind == LaneKind::Group
            || targetLane->kind == LaneKind::Group) {
            throw std::invalid_argument("range transfer target lane does not exist");
        }
        if (sourceLane->kind != targetLane->kind
            || ((sourceLane->kind == LaneKind::Bus
                 || sourceLane->kind == LaneKind::Enum)
                && sourceLane->width != targetLane->width)) {
            throw std::invalid_argument(
                "range transfer source and target lanes are incompatible");
        }
        uniqueSourceLaneIds.push_back(sourceLaneId);
        uniqueTargetLaneIds.push_back(copiedLane.laneId);
        for (auto& segment : copiedLane.relativeSegments) {
            if (segment.start < 0
                || segment.end <= segment.start
                || segment.end > duration_) {
                throw std::invalid_argument(
                    "transferred segment is outside the selected range");
            }
            const auto validation = validateLaneValue(*targetLane, segment.value);
            if (!validation.valid) {
                throw std::invalid_argument(
                    "transferred value is invalid for the target lane");
            }
            segment.value = validation.normalizedValue;
        }
    }
    if (mode_ == RangeTransferMode::Copy
        && destination_ < sourceEnd
        && destinationEnd > source_) {
        const auto sharesLane = std::any_of(
            uniqueTargetLaneIds.begin(),
            uniqueTargetLaneIds.end(),
            [&uniqueSourceLaneIds](const std::string& targetLaneId) {
                return std::find(
                           uniqueSourceLaneIds.begin(),
                           uniqueSourceLaneIds.end(),
                           targetLaneId)
                    != uniqueSourceLaneIds.end();
            });
        if (sharesLane) {
            throw std::invalid_argument(
                "range copy target overlaps the source");
        }
    }
}

void TransferRangeCommand::redo()
{
    if (after_) {
        *scenario_ = *after_;
        return;
    }

    before_ = *scenario_;
    const auto sameLaneMapping = std::all_of(
        lanes_.begin(),
        lanes_.end(),
        [](const CopiedLaneRange& copiedLane) {
            return copiedLane.sourceLaneId.empty()
                || copiedLane.sourceLaneId == copiedLane.laneId;
        });
    if (destination_ == source_ && sameLaneMapping) {
        after_ = *scenario_;
        return;
    }

    auto candidate = *scenario_;
    const auto sourceEnd = source_ + duration_;
    const auto destinationEnd = destination_ + duration_;
    candidate.duration = std::max(candidate.duration, destinationEnd);
    std::vector<std::string> affectedLaneIds;
    affectedLaneIds.reserve(lanes_.size() * 2);
    const auto rememberAffected = [&affectedLaneIds](const std::string& laneId) {
        if (std::find(
                affectedLaneIds.begin(),
                affectedLaneIds.end(),
                laneId)
            == affectedLaneIds.end()) {
            affectedLaneIds.push_back(laneId);
        }
    };
    if (mode_ == RangeTransferMode::Move) {
        for (const auto& copiedLane : lanes_) {
            const auto& sourceLaneId = copiedLane.sourceLaneId.empty()
                ? copiedLane.laneId
                : copiedLane.sourceLaneId;
            auto* sourceLane = findLane(candidate, sourceLaneId);
            if (!sourceLane) {
                throw std::runtime_error(
                    "range transfer source lane was removed");
            }
            clearSegmentRange(*sourceLane, source_, sourceEnd);
            rememberAffected(sourceLaneId);
        }
    }
    for (const auto& copiedLane : lanes_) {
        auto* lane = findLane(candidate, copiedLane.laneId);
        if (!lane) throw std::runtime_error("range transfer target lane was removed");
        clearSegmentRange(*lane, destination_, destinationEnd);
        rememberAffected(copiedLane.laneId);
    }
    for (const auto& copiedLane : lanes_) {
        auto* lane = findLane(candidate, copiedLane.laneId);
        if (!lane) throw std::runtime_error("range transfer target lane was removed");
        const auto& sourceLaneId = copiedLane.sourceLaneId.empty()
            ? copiedLane.laneId
            : copiedLane.sourceLaneId;
        for (const auto& relative : copiedLane.relativeSegments) {
            const auto start = destination_ + relative.start;
            const auto end = destination_ + relative.end;
            if (end <= start) continue;

            auto segmentId = makeStableId("segment");
            if (mode_ == RangeTransferMode::Move
                && sourceLaneId == copiedLane.laneId
                && !relative.id.empty()) {
                const auto idStillUsed = std::any_of(
                    lane->segments.begin(),
                    lane->segments.end(),
                    [&relative](const Segment& segment) {
                        return segment.id == relative.id;
                    });
                if (!idStillUsed) segmentId = relative.id;
            }
            setSegmentRange(
                *lane,
                start,
                end,
                relative.value,
                std::move(segmentId));
            if (!relative.extensions.empty()) {
                const auto transferred = std::find_if(
                    lane->segments.begin(),
                    lane->segments.end(),
                    [start, end, &relative](const Segment& segment) {
                        return segment.start <= start
                            && segment.end >= end
                            && segment.value == relative.value;
                    });
                if (transferred != lane->segments.end()) {
                    transferred->extensions = relative.extensions;
                }
            }
        }
    }
    for (const auto& laneId : affectedLaneIds) {
        auto* lane = findLane(candidate, laneId);
        if (!lane) continue;
        if (lane->kind == LaneKind::Bit
            || lane->kind == LaneKind::Bus
            || lane->kind == LaneKind::Enum) {
            synchronizeLaneEventsFromSegments(candidate, lane->id);
        }
    }
    after_ = std::move(candidate);
    *scenario_ = *after_;
}

void TransferRangeCommand::undo()
{
    if (!before_) throw std::runtime_error("range transfer command was not initialized");
    *scenario_ = *before_;
}

std::string TransferRangeCommand::description() const
{
    return mode_ == RangeTransferMode::Copy
        ? "Copy selected range"
        : "Move selected range";
}

bool TransferRangeCommand::hasEffect() const noexcept
{
    return before_ && after_ && *before_ != *after_;
}

PasteRangeCommand::PasteRangeCommand(
    Scenario& scenario,
    std::vector<CopiedLaneRange> lanes,
    const Tick destination,
    const Tick duration,
    std::string description)
    : scenario_(&scenario)
    , lanes_(std::move(lanes))
    , destination_(destination)
    , duration_(duration)
    , description_(std::move(description))
{
    if (lanes_.empty()) throw std::invalid_argument("paste contains no lanes");
    if (description_.empty()) throw std::invalid_argument("paste description is empty");
    if (destination_ < 0 || duration_ <= 0 || destination_ > scenario.duration
        || duration_ > std::numeric_limits<Tick>::max() - destination_) {
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
    const auto pasteEnd = destination_ + duration_;
    scenario_->duration = std::max(scenario_->duration, pasteEnd);
    for (const auto& copiedLane : lanes_) {
        auto* lane = findLane(*scenario_, copiedLane.laneId);
        if (!lane) throw std::runtime_error("paste target lane was removed");
        clearSegmentRange(*lane, destination_, pasteEnd);
        for (const auto& relative : copiedLane.relativeSegments) {
            const auto start = destination_ + relative.start;
            const auto end = destination_ + relative.end;
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
    return description_;
}

bool PasteRangeCommand::hasEffect() const noexcept
{
    return before_ && after_ && *before_ != *after_;
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

RepairWaveformEventLinkCommand::RepairWaveformEventLinkCommand(
    const Project& project,
    Scenario& scenario,
    std::string eventId)
    : project_(&project)
    , scenario_(&scenario)
    , eventId_(std::move(eventId))
{
}

void RepairWaveformEventLinkCommand::redo()
{
    snapshotRedo(*scenario_, before_, after_, [this] {
        Event* event = nullptr;
        std::size_t eventMatches = 0;
        for (auto& candidate : scenario_->events) {
            if (candidate.id != eventId_) continue;
            event = &candidate;
            ++eventMatches;
        }
        if (eventMatches != 1 || !event) {
            throw std::invalid_argument(
                eventMatches == 0
                    ? "event does not exist"
                    : "event stable ID is ambiguous");
        }
        if (!event->waveformLinked
            || event->linkedSegmentId.empty()) {
            throw std::invalid_argument(
                "event has no waveform link to repair");
        }
        if (!actionControlsWaveform(event->action)) {
            throw std::invalid_argument(
                "event action cannot control waveform content");
        }

        Lane* linkedLane = nullptr;
        Segment* linkedSegment = nullptr;
        std::size_t segmentMatches = 0;
        for (auto& lane : scenario_->lanes) {
            for (auto& segment : lane.segments) {
                if (segment.id
                    != event->linkedSegmentId) {
                    continue;
                }
                linkedLane = &lane;
                linkedSegment = &segment;
                ++segmentMatches;
            }
        }
        if (segmentMatches != 1
            || !linkedLane
            || !linkedSegment) {
            throw std::invalid_argument(
                segmentMatches == 0
                    ? "linked segment does not exist"
                    : "linked segment stable ID is ambiguous");
        }
        if (linkedLane->kind == LaneKind::Clock
            || linkedLane->kind == LaneKind::Group) {
            throw std::invalid_argument(
                "linked segment belongs to a non-editable lane");
        }
        const auto validation =
            validateLaneValue(
                *linkedLane,
                linkedSegment->value);
        if (!validation.valid) {
            throw std::invalid_argument(
                "linked segment value is invalid");
        }
        const auto linkedEventCount =
            std::count_if(
                scenario_->events.begin(),
                scenario_->events.end(),
                [event](const Event& candidate) {
                    return candidate.waveformLinked
                        && candidate.linkedSegmentId
                            == event->linkedSegmentId;
                });
        if (linkedEventCount != 1) {
            throw std::invalid_argument(
                "linked segment is referenced by multiple waveform events");
        }

        event->laneId = linkedLane->id;
        event->tick = linkedSegment->start;
        event->value = validation.normalizedValue;
        if (event->cycle) {
            const auto clockDomainId =
                !event->clockDomainId.empty()
                ? event->clockDomainId
                : linkedLane->clockDomainId;
            const ClockDomain* clock = nullptr;
            std::size_t clockMatches = 0;
            for (const auto& candidate :
                 project_->clockDomains) {
                if (candidate.id
                    != clockDomainId) {
                    continue;
                }
                clock = &candidate;
                ++clockMatches;
            }
            const auto cycleTick =
                clockMatches == 1 && clock
                && *event->cycle >= 0
                ? tickAtCycle(
                      *clock,
                      *event->cycle,
                      clock->activeEdge)
                : std::optional<Tick>{};
            if (!cycleTick
                || *cycleTick != event->tick) {
                event->cycle.reset();
            }
        }
        std::stable_sort(
            scenario_->events.begin(),
            scenario_->events.end(),
            [](const Event& left, const Event& right) {
                return left.tick < right.tick
                    || (left.tick == right.tick
                        && left.id < right.id);
            });
    });
}

void RepairWaveformEventLinkCommand::undo()
{
    if (!before_) {
        throw std::runtime_error(
            "event link repair has not been executed");
    }
    *scenario_ = *before_;
}

std::string RepairWaveformEventLinkCommand::description() const
{
    return "Repair event waveform link";
}

bool RepairWaveformEventLinkCommand::hasEffect() const noexcept
{
    return before_ && after_ && *before_ != *after_;
}

ClearEventCycleCommand::ClearEventCycleCommand(
    Scenario& scenario,
    std::string eventId)
    : scenario_(&scenario)
    , eventId_(std::move(eventId))
{
}

void ClearEventCycleCommand::redo()
{
    snapshotRedo(*scenario_, before_, after_, [this] {
        Event* event = nullptr;
        std::size_t eventMatches = 0;
        for (auto& candidate : scenario_->events) {
            if (candidate.id != eventId_) continue;
            event = &candidate;
            ++eventMatches;
        }
        if (eventMatches != 1 || !event) {
            throw std::invalid_argument(
                eventMatches == 0
                    ? "event does not exist"
                    : "event stable ID is ambiguous");
        }
        if (!event->cycle) {
            throw std::invalid_argument(
                "event has no cycle metadata to clear");
        }
        event->cycle.reset();
    });
}

void ClearEventCycleCommand::undo()
{
    if (!before_) {
        throw std::runtime_error(
            "event cycle clear has not been executed");
    }
    *scenario_ = *before_;
}

std::string ClearEventCycleCommand::description() const
{
    return "Clear event cycle";
}

bool ClearEventCycleCommand::hasEffect() const noexcept
{
    return before_ && after_ && *before_ != *after_;
}

RepairEventClockReferenceCommand::
    RepairEventClockReferenceCommand(
        const Project& project,
        Scenario& scenario,
        std::string eventId)
    : project_(&project)
    , scenario_(&scenario)
    , eventId_(std::move(eventId))
{
}

void RepairEventClockReferenceCommand::redo()
{
    snapshotRedo(*scenario_, before_, after_, [this] {
        Event* event = nullptr;
        std::size_t eventMatches = 0;
        for (auto& candidate : scenario_->events) {
            if (candidate.id != eventId_) continue;
            event = &candidate;
            ++eventMatches;
        }
        if (eventMatches != 1 || !event) {
            throw std::invalid_argument(
                eventMatches == 0
                    ? "event does not exist"
                    : "event stable ID is ambiguous");
        }
        if (event->clockDomainId.empty()) {
            throw std::invalid_argument(
                "event has no explicit clock reference to repair");
        }
        const auto currentClockMatches =
            std::count_if(
                project_->clockDomains.begin(),
                project_->clockDomains.end(),
                [event](const ClockDomain& clock) {
                    return clock.id
                        == event->clockDomainId;
                });
        if (currentClockMatches != 0) {
            throw std::invalid_argument(
                currentClockMatches == 1
                    ? "event clock reference is already valid"
                    : "event clock reference is ambiguous");
        }

        Lane* lane = nullptr;
        std::size_t laneMatches = 0;
        if (!event->laneId.empty()) {
            for (auto& candidate :
                 scenario_->lanes) {
                if (candidate.id
                    != event->laneId) {
                    continue;
                }
                lane = &candidate;
                ++laneMatches;
            }
        }
        if (laneMatches > 1) {
            throw std::invalid_argument(
                "event lane stable ID is ambiguous");
        }

        std::string replacementClockDomainId;
        if (lane
            && !lane->clockDomainId.empty()) {
            const auto fallbackMatches =
                std::count_if(
                    project_->clockDomains.begin(),
                    project_->clockDomains.end(),
                    [lane](const ClockDomain& clock) {
                        return clock.id
                            == lane->clockDomainId;
                    });
            if (fallbackMatches != 1) {
                throw std::invalid_argument(
                    fallbackMatches == 0
                        ? "event lane clock reference is also missing"
                        : "event lane clock reference is ambiguous");
            }
            replacementClockDomainId =
                lane->clockDomainId;
        }
        event->clockDomainId =
            replacementClockDomainId;

        if (event->cycle) {
            const auto effectiveClockDomainId =
                !event->clockDomainId.empty()
                ? event->clockDomainId
                : lane
                ? lane->clockDomainId
                : std::string{};
            const ClockDomain* clock = nullptr;
            std::size_t clockMatches = 0;
            for (const auto& candidate :
                 project_->clockDomains) {
                if (candidate.id
                    != effectiveClockDomainId) {
                    continue;
                }
                clock = &candidate;
                ++clockMatches;
            }
            const auto cycleTick =
                clockMatches == 1 && clock
                && *event->cycle >= 0
                ? tickAtCycle(
                      *clock,
                      *event->cycle,
                      clock->activeEdge)
                : std::optional<Tick>{};
            if (!cycleTick
                || *cycleTick != event->tick) {
                event->cycle.reset();
            }
        }
    });
}

void RepairEventClockReferenceCommand::undo()
{
    if (!before_) {
        throw std::runtime_error(
            "event clock repair has not been executed");
    }
    *scenario_ = *before_;
}

std::string RepairEventClockReferenceCommand::
    description() const
{
    return "Repair event clock reference";
}

bool RepairEventClockReferenceCommand::
    hasEffect() const noexcept
{
    return before_ && after_ && *before_ != *after_;
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

ChangeMarkerAtIndexCommand::ChangeMarkerAtIndexCommand(
    Scenario& scenario,
    const std::size_t markerIndex,
    Marker expected,
    Marker replacement)
    : scenario_(&scenario)
    , markerIndex_(markerIndex)
    , expected_(std::move(expected))
    , replacement_(std::move(replacement))
{
}

void ChangeMarkerAtIndexCommand::redo()
{
    snapshotRedo(*scenario_, before_, after_, [this] {
        if (markerIndex_ >= scenario_->markers.size()
            || scenario_->markers.at(markerIndex_) != expected_) {
            throw std::invalid_argument(
                "marker repair reference is stale");
        }
        if (replacement_.start < 0
            || replacement_.end < replacement_.start
            || replacement_.end > scenario_->duration) {
            throw std::invalid_argument("marker interval is invalid");
        }
        scenario_->markers.at(markerIndex_) = replacement_;
    });
}

void ChangeMarkerAtIndexCommand::undo()
{
    if (!before_) {
        throw std::runtime_error(
            "marker command has not been executed");
    }
    *scenario_ = *before_;
}

std::string ChangeMarkerAtIndexCommand::description() const
{
    return "Change marker";
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

RemoveMarkerAtIndexCommand::RemoveMarkerAtIndexCommand(
    Scenario& scenario,
    const std::size_t markerIndex,
    Marker expected)
    : scenario_(&scenario)
    , markerIndex_(markerIndex)
    , expected_(std::move(expected))
{
}

void RemoveMarkerAtIndexCommand::redo()
{
    snapshotRedo(*scenario_, before_, after_, [this] {
        if (markerIndex_ >= scenario_->markers.size()
            || scenario_->markers.at(markerIndex_) != expected_) {
            throw std::invalid_argument(
                "marker repair reference is stale");
        }
        scenario_->markers.erase(
            scenario_->markers.begin()
            + static_cast<std::ptrdiff_t>(markerIndex_));
    });
}

void RemoveMarkerAtIndexCommand::undo()
{
    if (!before_) {
        throw std::runtime_error(
            "marker command has not been executed");
    }
    *scenario_ = *before_;
}

std::string RemoveMarkerAtIndexCommand::description() const
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

RepairRelationClockReferenceCommand::
    RepairRelationClockReferenceCommand(
        const Project& project,
        Scenario& scenario,
        std::string relationId)
    : project_(&project)
    , scenario_(&scenario)
    , relationId_(std::move(relationId))
{
}

void RepairRelationClockReferenceCommand::redo()
{
    snapshotRedo(*scenario_, before_, after_, [this] {
        if (relationId_.empty()) {
            throw std::invalid_argument(
                "relation stable ID is missing");
        }
        Relation* relation = nullptr;
        std::size_t relationMatches = 0;
        for (auto& candidate : scenario_->relations) {
            if (candidate.id != relationId_) continue;
            relation = &candidate;
            ++relationMatches;
        }
        if (relationMatches != 1 || !relation) {
            throw std::invalid_argument(
                relationMatches == 0
                    ? "relation does not exist"
                    : "relation stable ID is ambiguous");
        }
        if (relation->clockDomainId.empty()) {
            throw std::invalid_argument(
                "relation has no explicit clock reference to repair");
        }
        const auto currentClockMatches =
            std::count_if(
                project_->clockDomains.begin(),
                project_->clockDomains.end(),
                [relation](const ClockDomain& clock) {
                    return clock.id
                        == relation->clockDomainId;
                });
        if (currentClockMatches != 0) {
            throw std::invalid_argument(
                currentClockMatches == 1
                    ? "relation clock reference is already valid"
                    : "relation clock reference is ambiguous");
        }

        const auto endpointClock =
            [this](const std::string& eventId) {
                if (eventId.empty()) {
                    throw std::invalid_argument(
                        "relation endpoint Event ID is missing");
                }
                const Event* event = nullptr;
                std::size_t eventMatches = 0;
                for (const auto& candidate :
                     scenario_->events) {
                    if (candidate.id != eventId) continue;
                    event = &candidate;
                    ++eventMatches;
                }
                if (eventMatches != 1 || !event) {
                    throw std::invalid_argument(
                        eventMatches == 0
                            ? "relation endpoint Event does not exist"
                            : "relation endpoint Event ID is ambiguous");
                }

                const Lane* lane = nullptr;
                std::size_t laneMatches = 0;
                for (const auto& candidate :
                     scenario_->lanes) {
                    if (candidate.id != event->laneId) {
                        continue;
                    }
                    lane = &candidate;
                    ++laneMatches;
                }
                if (laneMatches != 1 || !lane) {
                    throw std::invalid_argument(
                        laneMatches == 0
                            ? "relation endpoint Lane does not exist"
                            : "relation endpoint Lane ID is ambiguous");
                }
                if (lane->kind == LaneKind::Clock
                    || lane->kind == LaneKind::Group) {
                    throw std::invalid_argument(
                        "relation endpoint Lane is not a signal");
                }

                const auto clockDomainId =
                    !event->clockDomainId.empty()
                    ? event->clockDomainId
                    : lane->clockDomainId;
                if (clockDomainId.empty()) {
                    return std::string{};
                }
                const auto clockMatches =
                    std::count_if(
                        project_->clockDomains.begin(),
                        project_->clockDomains.end(),
                        [&clockDomainId](
                            const ClockDomain& clock) {
                            return clock.id
                                == clockDomainId;
                        });
                if (clockMatches != 1) {
                    throw std::invalid_argument(
                        clockMatches == 0
                            ? "relation endpoint clock reference does not exist"
                            : "relation endpoint clock reference is ambiguous");
                }
                return clockDomainId;
            };

        const auto sourceClock =
            endpointClock(relation->sourceEventId);
        const auto targetClock =
            endpointClock(relation->targetEventId);
        if (!sourceClock.empty()
            && !targetClock.empty()
            && sourceClock != targetClock) {
            throw std::invalid_argument(
                "relation endpoint clock domains conflict");
        }
        relation->clockDomainId =
            !sourceClock.empty()
            ? sourceClock
            : targetClock;
    });
}

void RepairRelationClockReferenceCommand::undo()
{
    if (!before_) {
        throw std::runtime_error(
            "relation clock repair has not been executed");
    }
    *scenario_ = *before_;
}

std::string RepairRelationClockReferenceCommand::
    description() const
{
    return "Repair relation clock reference";
}

bool RepairRelationClockReferenceCommand::
    hasEffect() const noexcept
{
    return before_ && after_ && *before_ != *after_;
}

ChangeRelationAtIndexCommand::ChangeRelationAtIndexCommand(
    Scenario& scenario,
    const std::size_t relationIndex,
    Relation expected,
    Relation replacement)
    : scenario_(&scenario)
    , relationIndex_(relationIndex)
    , expected_(std::move(expected))
    , replacement_(std::move(replacement))
{
}

void ChangeRelationAtIndexCommand::redo()
{
    snapshotRedo(*scenario_, before_, after_, [this] {
        if (relationIndex_ >= scenario_->relations.size()
            || scenario_->relations.at(relationIndex_) != expected_) {
            throw std::invalid_argument(
                "relation repair reference is stale");
        }
        if (!findEvent(*scenario_, replacement_.sourceEventId)
            || (!replacement_.targetEventId.empty()
                && !findEvent(
                    *scenario_, replacement_.targetEventId))) {
            throw std::invalid_argument(
                "relation event reference does not exist");
        }
        if (replacement_.minimumDelay < 0
            || replacement_.maximumDelay
                < replacement_.minimumDelay) {
            throw std::invalid_argument(
                "relation delay range is invalid");
        }
        scenario_->relations.at(relationIndex_) = replacement_;
    });
}

void ChangeRelationAtIndexCommand::undo()
{
    if (!before_) {
        throw std::runtime_error(
            "relation command has not been executed");
    }
    *scenario_ = *before_;
}

std::string ChangeRelationAtIndexCommand::description() const
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

RemoveRelationAtIndexCommand::RemoveRelationAtIndexCommand(
    Scenario& scenario,
    const std::size_t relationIndex,
    Relation expected)
    : scenario_(&scenario)
    , relationIndex_(relationIndex)
    , expected_(std::move(expected))
{
}

void RemoveRelationAtIndexCommand::redo()
{
    snapshotRedo(*scenario_, before_, after_, [this] {
        if (relationIndex_ >= scenario_->relations.size()
            || scenario_->relations.at(relationIndex_) != expected_) {
            throw std::invalid_argument(
                "relation repair reference is stale");
        }
        scenario_->relations.erase(
            scenario_->relations.begin()
            + static_cast<std::ptrdiff_t>(relationIndex_));
    });
}

void RemoveRelationAtIndexCommand::undo()
{
    if (!before_) {
        throw std::runtime_error(
            "relation command has not been executed");
    }
    *scenario_ = *before_;
}

std::string RemoveRelationAtIndexCommand::description() const
{
    return "Remove relation";
}

} // namespace wave
