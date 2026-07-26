#pragma once

#include "wave/time.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wave {

enum class LaneKind {
    Clock,
    Bit,
    Bus,
    Enum,
    Transaction,
    Event,
    Group,
};

enum class Radix {
    Binary,
    Octal,
    Decimal,
    Hexadecimal,
};

enum class LaneValueEncoding {
    ProjectLiteral,
    BinaryTrace,
};

enum class ClockOverrideMode {
    Gated,
    Disabled,
};

enum class EventAction {
    Drive,
    Expect,
    Pulse,
    Toggle,
    SendFrame,
    ReceiveFrame,
    Marker,
    Note,
    WaitCondition,
};

enum class MarkerKind {
    Point,
    Interval,
    Phase,
    Error,
    Note,
};

enum class Severity {
    Information,
    Warning,
    Error,
};

struct Segment {
    std::string id;
    Tick start{0};
    Tick end{0};
    std::string value;
    JsonExtensions extensions;

    [[nodiscard]] bool operator==(const Segment&) const = default;
};

struct Lane {
    std::string id;
    std::string name;
    LaneKind kind{LaneKind::Bit};
    std::uint32_t width{1};
    bool isSigned{false};
    Radix radix{Radix::Hexadecimal};
    std::map<std::string, std::string> enumMap;
    std::string clockDomainId;
    std::string color{"#4fc3f7"};
    int height{56};
    bool visible{true};
    std::string groupId;
    std::vector<Segment> segments;
    JsonExtensions extensions;

    [[nodiscard]] bool operator==(const Lane&) const = default;
};

struct Event {
    std::string id;
    std::string laneId;
    Tick tick{0};
    EventAction action{EventAction::Drive};
    std::string value;
    std::string expectedResult;
    std::string clockDomainId;
    std::optional<std::int64_t> cycle;
    std::string description;
    std::string linkedSegmentId;
    bool waveformLinked{false};
    JsonExtensions extensions;

    [[nodiscard]] bool operator==(const Event&) const = default;
};

struct Relation {
    std::string id;
    std::string sourceEventId;
    std::string targetEventId;
    Tick minimumDelay{0};
    Tick maximumDelay{0};
    std::string clockDomainId;
    std::string condition;
    Severity severity{Severity::Error};
    std::string description;
    JsonExtensions extensions;

    [[nodiscard]] bool operator==(const Relation&) const = default;
};

struct Marker {
    std::string id;
    std::string name;
    Tick start{0};
    Tick end{0};
    MarkerKind kind{MarkerKind::Point};
    std::string note;
    JsonExtensions extensions;

    [[nodiscard]] bool operator==(const Marker&) const = default;
};

struct Scenario {
    std::string id;
    std::string name;
    Tick duration{0};
    std::vector<Lane> lanes;
    std::vector<Event> events;
    std::vector<Relation> relations;
    std::vector<Marker> markers;
    JsonExtensions extensions;

    [[nodiscard]] bool operator==(const Scenario&) const = default;
};

struct ImportedTrace {
    std::string id;
    std::string path;
    std::string format;
    Tick offset{0};
    std::map<std::string, std::string> signalMapping;
    JsonExtensions extensions;

    [[nodiscard]] bool operator==(const ImportedTrace&) const = default;
};

struct LinkedResource {
    std::string kind;
    std::string path;
    std::string stableId;
    std::string contentHash;
    std::string summary;
    JsonExtensions extensions;

    [[nodiscard]] bool operator==(const LinkedResource&) const = default;
};

struct Project {
    static constexpr int CurrentSchemaVersion = 1;

    int schemaVersion{CurrentSchemaVersion};
    std::string id;
    std::string name;
    TimeBase timeBase;
    std::vector<ClockDomain> clockDomains;
    std::vector<Scenario> scenarios;
    std::vector<ImportedTrace> importedTraces;
    std::vector<LinkedResource> linkedResources;
    JsonExtensions exportSettings;
    JsonExtensions extensions;

    [[nodiscard]] bool operator==(const Project&) const = default;
};

struct ValueValidation {
    bool valid{false};
    std::string normalizedValue;
    std::string error;
};

[[nodiscard]] std::string makeStableId(std::string_view prefix);

[[nodiscard]] Lane* findLane(Scenario& scenario, std::string_view laneId) noexcept;
[[nodiscard]] const Lane* findLane(const Scenario& scenario, std::string_view laneId) noexcept;
[[nodiscard]] ClockDomain* findClock(Project& project, std::string_view clockId) noexcept;
[[nodiscard]] const ClockDomain* findClock(const Project& project, std::string_view clockId) noexcept;
[[nodiscard]] Event* findEvent(Scenario& scenario, std::string_view eventId) noexcept;
[[nodiscard]] const Event* findEvent(const Scenario& scenario, std::string_view eventId) noexcept;
[[nodiscard]] Relation* findRelation(Scenario& scenario, std::string_view relationId) noexcept;
[[nodiscard]] const Relation* findRelation(const Scenario& scenario, std::string_view relationId) noexcept;

[[nodiscard]] ValueValidation validateLaneValue(const Lane& lane, std::string_view value);
[[nodiscard]] std::optional<std::string> laneValueBits(
    const Lane& lane,
    std::string_view value,
    LaneValueEncoding encoding = LaneValueEncoding::ProjectLiteral);
[[nodiscard]] std::optional<ClockOverrideMode> clockOverrideModeFromString(
    std::string_view text) noexcept;
[[nodiscard]] std::string_view toString(ClockOverrideMode mode) noexcept;
[[nodiscard]] std::optional<ClockOverrideMode> clockOverrideAt(
    const Lane& clockLane,
    Tick tick) noexcept;
[[nodiscard]] char clockValueAt(
    const ClockDomain& clock,
    const Lane& clockLane,
    Tick tick) noexcept;

void normalizeSegments(Lane& lane);
void setSegmentRange(
    Lane& lane,
    Tick start,
    Tick end,
    std::string value,
    std::string stableId = {},
    JsonExtensions extensions = {});
void clearSegmentRange(Lane& lane, Tick start, Tick end);
void synchronizeLaneEventsFromSegments(
    Scenario& scenario,
    std::string_view laneId,
    EventAction action = EventAction::Drive);

[[nodiscard]] std::string_view toString(LaneKind kind) noexcept;
[[nodiscard]] std::optional<LaneKind> laneKindFromString(std::string_view text) noexcept;
[[nodiscard]] std::string_view toString(Radix radix) noexcept;
[[nodiscard]] std::optional<Radix> radixFromString(std::string_view text) noexcept;
[[nodiscard]] std::string_view toString(ClockEdge edge) noexcept;
[[nodiscard]] std::optional<ClockEdge> clockEdgeFromString(std::string_view text) noexcept;
[[nodiscard]] std::string_view toString(EventAction action) noexcept;
[[nodiscard]] std::optional<EventAction> eventActionFromString(std::string_view text) noexcept;
[[nodiscard]] std::string_view toString(MarkerKind kind) noexcept;
[[nodiscard]] std::optional<MarkerKind> markerKindFromString(std::string_view text) noexcept;
[[nodiscard]] std::string_view toString(Severity severity) noexcept;
[[nodiscard]] std::optional<Severity> severityFromString(std::string_view text) noexcept;

[[nodiscard]] Project makeDemonstrationProject();

} // namespace wave
