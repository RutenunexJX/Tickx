#include "wave/model.h"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cctype>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace wave {
namespace {

std::shared_ptr<ScenarioAddressState> makeScenarioAddressState(
    Scenario* scenario)
{
    auto state = std::make_shared<ScenarioAddressState>();
    state->current = scenario;
    return state;
}

std::string lowerCopy(const std::string_view text)
{
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

std::string upperCopy(const std::string_view text)
{
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(), [](const unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return result;
}

bool containsOnly(const std::string_view text, const std::string_view allowed)
{
    return !text.empty()
        && std::all_of(text.begin(), text.end(), [allowed](const char character) {
               return allowed.find(character) != std::string_view::npos;
           });
}

std::string removeSeparators(const std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    for (const auto character : text) {
        if (character != '_' && !std::isspace(static_cast<unsigned char>(character))) {
            result.push_back(character);
        }
    }
    return result;
}

ValueValidation validateBit(const std::string_view value)
{
    if (value.size() != 1) {
        return {false, {}, "bit value must contain exactly one symbol"};
    }
    const auto normalized = static_cast<char>(std::toupper(static_cast<unsigned char>(value.front())));
    if (normalized != '0' && normalized != '1' && normalized != 'X' && normalized != 'Z') {
        return {false, {}, "bit value must be 0, 1, X, or Z"};
    }
    return {true, std::string(1, normalized), {}};
}

std::optional<std::uint64_t> parseUnsigned(const std::string_view text, const int base)
{
    std::uint64_t value = 0;
    const auto [pointer, error] = std::from_chars(text.data(), text.data() + text.size(), value, base);
    if (error != std::errc{} || pointer != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::string> digitsToBits(
    std::string digits,
    const int base,
    const std::uint32_t width,
    const bool isSigned)
{
    digits = removeSeparators(digits);
    digits = upperCopy(digits);
    if (digits.empty() || width == 0) return std::nullopt;

    std::string bits;
    if (base == 2) {
        if (!containsOnly(lowerCopy(digits), "01xz")) return std::nullopt;
        bits = digits;
    } else if (base == 8 || base == 16) {
        const auto groupSize = base == 8 ? 3 : 4;
        for (const auto character : digits) {
            if (character == 'X' || character == 'Z') {
                bits.append(static_cast<std::size_t>(groupSize), character);
                continue;
            }
            const auto value = character >= '0' && character <= '9'
                ? static_cast<int>(character - '0')
                : character >= 'A' && character <= 'F'
                    ? static_cast<int>(character - 'A' + 10)
                    : -1;
            if (value < 0 || value >= base) return std::nullopt;
            for (int bit = groupSize - 1; bit >= 0; --bit) {
                bits.push_back(((value >> bit) & 1) != 0 ? '1' : '0');
            }
        }
    } else {
        if (width > 64) return std::nullopt;
        std::uint64_t value = 0;
        if (digits.front() == '-') {
            std::int64_t signedValue = 0;
            const auto conversion = std::from_chars(
                digits.data(),
                digits.data() + digits.size(),
                signedValue,
                10);
            if (conversion.ec != std::errc{}
                || conversion.ptr != digits.data() + digits.size()
                || !isSigned) {
                return std::nullopt;
            }
            value = static_cast<std::uint64_t>(signedValue);
        } else {
            if (digits.front() == '+') {
                digits.erase(digits.begin());
                if (digits.empty()) return std::nullopt;
            }
            const auto conversion = std::from_chars(
                digits.data(),
                digits.data() + digits.size(),
                value,
                10);
            if (conversion.ec != std::errc{}
                || conversion.ptr != digits.data() + digits.size()) {
                return std::nullopt;
            }
        }
        bits.reserve(width);
        for (std::uint32_t bit = 0; bit < width; ++bit) {
            const auto shift = width - bit - 1;
            bits.push_back(((value >> shift) & 1ULL) != 0 ? '1' : '0');
        }
    }

    if (bits.size() < width) {
        const auto pad = isSigned && !bits.empty() ? bits.front() : '0';
        bits.insert(bits.begin(), width - bits.size(), pad);
    } else if (bits.size() > width) {
        bits.erase(0, bits.size() - width);
    }
    return bits;
}

ValueValidation validateBus(const Lane& lane, const std::string_view rawValue)
{
    if (lane.kind == LaneKind::Bus) {
        if (const auto text = busTextLabel(rawValue)) {
            return text->empty()
                ? ValueValidation{false, {}, "bus text label is empty"}
                : ValueValidation{true, std::string(rawValue), {}};
        }
    }
    if (lane.kind == LaneKind::Enum) {
        const auto enumMatch = lane.enumMap.find(std::string(rawValue));
        if (enumMatch != lane.enumMap.end()) {
            return {true, std::string(rawValue), {}};
        }
    }

    auto value = removeSeparators(rawValue);
    if (value.empty()) {
        return {false, {}, "bus value is empty"};
    }

    auto lower = lowerCopy(value);
    int base = 10;
    std::size_t prefixLength = 0;
    std::uint64_t bitsPerDigit = 0;
    std::string_view allowed;
    if (lower.starts_with("0b")) {
        base = 2;
        prefixLength = 2;
        bitsPerDigit = 1;
        allowed = "01xz";
    } else if (lower.starts_with("0o")) {
        base = 8;
        prefixLength = 2;
        bitsPerDigit = 3;
        allowed = "01234567xz";
    } else if (lower.starts_with("0x")) {
        base = 16;
        prefixLength = 2;
        bitsPerDigit = 4;
        allowed = "0123456789abcdefxz";
    } else if (containsOnly(lower, "01xz") && lower.size() > 1) {
        base = 2;
        bitsPerDigit = 1;
        allowed = "01xz";
    }

    const auto digits = std::string_view(lower).substr(prefixLength);
    if (digits.empty()) {
        return {false, {}, "bus value has no digits"};
    }
    if (bitsPerDigit != 0) {
        if (!containsOnly(digits, allowed)) {
            return {false, {}, "bus value contains a digit invalid for its radix"};
        }
        auto significant = digits;
        while (significant.size() > 1 && significant.front() == '0') {
            significant.remove_prefix(1);
        }
        const auto maximumDigits =
            (static_cast<std::uint64_t>(lane.width) + bitsPerDigit - 1)
            / bitsPerDigit;
        if (significant.size() > maximumDigits) {
            return {false, {}, "bus value exceeds the lane width"};
        }
        const auto highBits = lane.width % bitsPerDigit;
        if (highBits != 0
            && significant.size() == maximumDigits
            && significant.front() != 'x'
            && significant.front() != 'z') {
            const auto character = significant.front();
            const auto highDigit = character >= '0' && character <= '9'
                ? static_cast<unsigned>(character - '0')
                : static_cast<unsigned>(character - 'a' + 10);
            if (highDigit >= (1U << highBits)) {
                return {false, {}, "bus value exceeds the lane width"};
            }
        }
        return {true, lower, {}};
    }

    bool negative = false;
    std::string_view decimalDigits = digits;
    if (decimalDigits.front() == '-') {
        negative = true;
        decimalDigits.remove_prefix(1);
    } else if (decimalDigits.front() == '+') {
        decimalDigits.remove_prefix(1);
    }
    if (decimalDigits.empty()) {
        return {false, {}, "decimal bus value has no digits"};
    }
    const auto parsed = parseUnsigned(decimalDigits, base);
    if (!parsed) {
        return {false, {}, "bus value is not a valid integer"};
    }

    if (negative && !lane.isSigned) {
        return {false, {}, "negative value is invalid for an unsigned lane"};
    }
    if (lane.width == 0 || lane.width > 64) {
        return {false, {}, "bus width must be between 1 and 64"};
    }

    if (negative) {
        const auto limit = lane.width == 64
            ? (std::uint64_t{1} << 63)
            : (std::uint64_t{1} << (lane.width - 1));
        if (*parsed > limit) {
            return {false, {}, "signed bus value exceeds the lane width"};
        }
    } else {
        const auto limit = lane.width == 64
            ? std::numeric_limits<std::uint64_t>::max()
            : (std::uint64_t{1} << lane.width) - 1;
        if (*parsed > limit) {
            return {false, {}, "bus value exceeds the lane width"};
        }
    }

    return {true, value, {}};
}

template<typename Enum>
std::optional<Enum> enumFromString(
    const std::string_view text,
    const std::initializer_list<std::pair<std::string_view, Enum>> values) noexcept
{
    for (const auto& [name, value] : values) {
        if (name == text) {
            return value;
        }
    }
    return std::nullopt;
}

} // namespace

ScenarioRef::ScenarioRef(std::nullptr_t) noexcept
{
}

ScenarioRef::ScenarioRef(Scenario* scenario) noexcept
    : state_(scenario ? scenario->addressState_ : nullptr)
{
}

ScenarioRef::ScenarioRef(Scenario& scenario) noexcept
    : ScenarioRef(&scenario)
{
}

ScenarioRef& ScenarioRef::operator=(Scenario* scenario) noexcept
{
    state_ = scenario ? scenario->addressState_ : nullptr;
    return *this;
}

ScenarioRef& ScenarioRef::operator=(std::nullptr_t) noexcept
{
    state_.reset();
    return *this;
}

Scenario* ScenarioRef::get() const noexcept
{
    return state_ ? state_->current : nullptr;
}

ScenarioRef::operator bool() const noexcept
{
    return get() != nullptr;
}

ScenarioRef::operator Scenario*() const noexcept
{
    return get();
}

Scenario& ScenarioRef::operator*() const
{
    auto* scenario = get();
    if (!scenario) {
        throw std::runtime_error("scenario reference is no longer available");
    }
    return *scenario;
}

Scenario* ScenarioRef::operator->() const noexcept
{
    return get();
}

bool ScenarioRef::operator==(const Scenario* scenario) const noexcept
{
    return get() == scenario;
}

bool ScenarioRef::operator!=(const Scenario* scenario) const noexcept
{
    return get() != scenario;
}

bool ScenarioRef::operator==(std::nullptr_t) const noexcept
{
    return get() == nullptr;
}

bool ScenarioRef::operator!=(std::nullptr_t) const noexcept
{
    return get() != nullptr;
}

Scenario::Scenario()
    : addressState_(makeScenarioAddressState(this))
{
}

Scenario::Scenario(
    std::string stableId,
    std::string displayName,
    const Tick scenarioDuration,
    std::vector<Lane> scenarioLanes,
    std::vector<Event> scenarioEvents,
    std::vector<Relation> scenarioRelations,
    std::vector<Marker> scenarioMarkers,
    JsonExtensions scenarioExtensions)
    : id(std::move(stableId))
    , name(std::move(displayName))
    , duration(scenarioDuration)
    , lanes(std::move(scenarioLanes))
    , events(std::move(scenarioEvents))
    , relations(std::move(scenarioRelations))
    , markers(std::move(scenarioMarkers))
    , extensions(std::move(scenarioExtensions))
    , addressState_(makeScenarioAddressState(this))
{
}

Scenario::Scenario(const Scenario& other)
    : id(other.id)
    , name(other.name)
    , duration(other.duration)
    , lanes(other.lanes)
    , events(other.events)
    , relations(other.relations)
    , markers(other.markers)
    , extensions(other.extensions)
    , addressState_(makeScenarioAddressState(this))
{
}

Scenario::Scenario(Scenario&& other) noexcept
    : id(std::move(other.id))
    , name(std::move(other.name))
    , duration(other.duration)
    , lanes(std::move(other.lanes))
    , events(std::move(other.events))
    , relations(std::move(other.relations))
    , markers(std::move(other.markers))
    , extensions(std::move(other.extensions))
    , addressState_(std::move(other.addressState_))
{
    if (!addressState_) addressState_ = makeScenarioAddressState(this);
    addressState_->current = this;
    other.addressState_ = makeScenarioAddressState(&other);
}

Scenario& Scenario::operator=(const Scenario& other)
{
    if (this == &other) return *this;
    id = other.id;
    name = other.name;
    duration = other.duration;
    lanes = other.lanes;
    events = other.events;
    relations = other.relations;
    markers = other.markers;
    extensions = other.extensions;
    return *this;
}

Scenario& Scenario::operator=(Scenario&& other) noexcept
{
    if (this == &other) return *this;
    if (addressState_ && addressState_->current == this) {
        addressState_->current = nullptr;
    }
    id = std::move(other.id);
    name = std::move(other.name);
    duration = other.duration;
    lanes = std::move(other.lanes);
    events = std::move(other.events);
    relations = std::move(other.relations);
    markers = std::move(other.markers);
    extensions = std::move(other.extensions);
    addressState_ = std::move(other.addressState_);
    if (!addressState_) addressState_ = makeScenarioAddressState(this);
    addressState_->current = this;
    other.addressState_ = makeScenarioAddressState(&other);
    return *this;
}

Scenario::~Scenario()
{
    if (addressState_ && addressState_->current == this) {
        addressState_->current = nullptr;
    }
}

bool Scenario::operator==(const Scenario& other) const
{
    return id == other.id
        && name == other.name
        && duration == other.duration
        && lanes == other.lanes
        && events == other.events
        && relations == other.relations
        && markers == other.markers
        && extensions == other.extensions;
}

std::string makeStableId(const std::string_view prefix)
{
    static std::atomic<std::uint64_t> counter{0};
    static thread_local std::mt19937_64 generator{std::random_device{}()};
    const auto now = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto uniqueCounter = counter.fetch_add(1, std::memory_order_relaxed);

    std::ostringstream stream;
    stream << prefix << '-' << std::hex << std::setfill('0')
           << std::setw(16) << (generator() ^ now)
           << std::setw(8) << uniqueCounter;
    return stream.str();
}

Lane* findLane(Scenario& scenario, const std::string_view laneId) noexcept
{
    const auto iterator = std::find_if(
        scenario.lanes.begin(),
        scenario.lanes.end(),
        [laneId](const Lane& lane) { return lane.id == laneId; });
    return iterator == scenario.lanes.end() ? nullptr : &*iterator;
}

const Lane* findLane(const Scenario& scenario, const std::string_view laneId) noexcept
{
    const auto iterator = std::find_if(
        scenario.lanes.begin(),
        scenario.lanes.end(),
        [laneId](const Lane& lane) { return lane.id == laneId; });
    return iterator == scenario.lanes.end() ? nullptr : &*iterator;
}

ClockDomain* findClock(Project& project, const std::string_view clockId) noexcept
{
    const auto iterator = std::find_if(
        project.clockDomains.begin(),
        project.clockDomains.end(),
        [clockId](const ClockDomain& clock) { return clock.id == clockId; });
    return iterator == project.clockDomains.end() ? nullptr : &*iterator;
}

const ClockDomain* findClock(const Project& project, const std::string_view clockId) noexcept
{
    const auto iterator = std::find_if(
        project.clockDomains.begin(),
        project.clockDomains.end(),
        [clockId](const ClockDomain& clock) { return clock.id == clockId; });
    return iterator == project.clockDomains.end() ? nullptr : &*iterator;
}

Event* findEvent(Scenario& scenario, const std::string_view eventId) noexcept
{
    const auto iterator = std::find_if(
        scenario.events.begin(),
        scenario.events.end(),
        [eventId](const Event& event) { return event.id == eventId; });
    return iterator == scenario.events.end() ? nullptr : &*iterator;
}

const Event* findEvent(const Scenario& scenario, const std::string_view eventId) noexcept
{
    const auto iterator = std::find_if(
        scenario.events.begin(),
        scenario.events.end(),
        [eventId](const Event& event) { return event.id == eventId; });
    return iterator == scenario.events.end() ? nullptr : &*iterator;
}

Relation* findRelation(Scenario& scenario, const std::string_view relationId) noexcept
{
    const auto iterator = std::find_if(
        scenario.relations.begin(),
        scenario.relations.end(),
        [relationId](const Relation& relation) { return relation.id == relationId; });
    return iterator == scenario.relations.end() ? nullptr : &*iterator;
}

const Relation* findRelation(const Scenario& scenario, const std::string_view relationId) noexcept
{
    const auto iterator = std::find_if(
        scenario.relations.begin(),
        scenario.relations.end(),
        [relationId](const Relation& relation) { return relation.id == relationId; });
    return iterator == scenario.relations.end() ? nullptr : &*iterator;
}

std::optional<std::string_view> busTextLabel(const std::string_view value) noexcept
{
    return value.starts_with(BusTextPrefix)
        ? std::optional{value.substr(BusTextPrefix.size())} : std::nullopt;
}

ValueValidation validateLaneValue(const Lane& lane, const std::string_view value)
{
    switch (lane.kind) {
    case LaneKind::Bit:
        return validateBit(value);
    case LaneKind::Bus:
    case LaneKind::Enum:
        return validateBus(lane, value);
    case LaneKind::Clock: {
        const auto mode = clockOverrideModeFromString(lowerCopy(value));
        return mode
            ? ValueValidation{true, std::string(toString(*mode)), {}}
            : ValueValidation{
                false,
                {},
                "clock override must be 'gated' or 'disabled'",
            };
    }
    case LaneKind::Transaction:
    case LaneKind::Event:
        return value.empty()
            ? ValueValidation{false, {}, "value is empty"}
            : ValueValidation{true, std::string(value), {}};
    case LaneKind::Group:
        return {false, {}, "group lanes do not contain value segments"};
    }
    return {false, {}, "unsupported lane kind"};
}

std::optional<std::string> laneValueBits(
    const Lane& lane,
    const std::string_view rawValue,
    const LaneValueEncoding encoding)
{
    if (lane.kind == LaneKind::Bus && busTextLabel(rawValue)) return std::nullopt;
    std::string value;
    if (encoding == LaneValueEncoding::ProjectLiteral) {
        const auto enumeration = lane.enumMap.find(std::string(rawValue));
        if (enumeration != lane.enumMap.end()) value = enumeration->second;
    }
    if (value.empty()) value = removeSeparators(rawValue);
    if (value.empty()) return std::nullopt;
    auto normalized = upperCopy(value);
    const auto width = lane.kind == LaneKind::Bit || lane.kind == LaneKind::Clock
        ? std::uint32_t{1}
        : lane.width;
    if (normalized.starts_with("0B")) {
        return digitsToBits(normalized.substr(2), 2, width, lane.isSigned);
    }
    if (normalized.starts_with("0O")) {
        return digitsToBits(normalized.substr(2), 8, width, lane.isSigned);
    }
    if (normalized.starts_with("0X")) {
        return digitsToBits(normalized.substr(2), 16, width, lane.isSigned);
    }
    if (encoding == LaneValueEncoding::BinaryTrace) {
        const auto binary = std::all_of(
            normalized.begin(),
            normalized.end(),
            [](const char character) {
                return character == '0' || character == '1'
                    || character == 'X' || character == 'Z';
            });
        if (binary) return digitsToBits(normalized, 2, width, false);
    }
    if (!normalized.empty()
        && (normalized.front() == '-' || normalized.front() == '+')) {
        return digitsToBits(normalized, 10, width, lane.isSigned);
    }
    int base = 10;
    switch (lane.radix) {
    case Radix::Binary: base = 2; break;
    case Radix::Octal: base = 8; break;
    case Radix::Decimal: base = 10; break;
    case Radix::Hexadecimal: base = 16; break;
    }
    return digitsToBits(normalized, base, width, lane.isSigned);
}

std::optional<ClockOverrideMode> clockOverrideModeFromString(
    const std::string_view text) noexcept
{
    return enumFromString<ClockOverrideMode>(text, {
        {"gated", ClockOverrideMode::Gated},
        {"gate", ClockOverrideMode::Gated},
        {"disabled", ClockOverrideMode::Disabled},
        {"disable", ClockOverrideMode::Disabled},
    });
}

std::string_view toString(const ClockOverrideMode mode) noexcept
{
    return mode == ClockOverrideMode::Gated ? "gated" : "disabled";
}

std::optional<ClockOverrideMode> clockOverrideAt(
    const Lane& clockLane,
    const Tick tick) noexcept
{
    if (clockLane.kind != LaneKind::Clock) return std::nullopt;
    const auto iterator = std::upper_bound(
        clockLane.segments.begin(),
        clockLane.segments.end(),
        tick,
        [](const Tick value, const Segment& segment) {
            return value < segment.start;
        });
    if (iterator == clockLane.segments.begin()) return std::nullopt;
    const auto& segment = *std::prev(iterator);
    if (tick < segment.start || tick >= segment.end) return std::nullopt;
    return clockOverrideModeFromString(segment.value);
}

char clockValueAt(
    const ClockDomain& clock,
    const Lane& clockLane,
    const Tick tick) noexcept
{
    if (!clock.isValid() || clockLane.kind != LaneKind::Clock) return 'X';
    if (const auto overrideMode = clockOverrideAt(clockLane, tick)) {
        return *overrideMode == ClockOverrideMode::Gated ? '0' : 'X';
    }
    if (tick < clock.phase) return '0';

    Tick relative = 0;
#if defined(__GNUC__) || defined(__clang__)
    if (__builtin_sub_overflow(tick, clock.phase, &relative)) return 'X';
#else
    if ((clock.phase > 0 && tick < std::numeric_limits<Tick>::min() + clock.phase)
        || (clock.phase < 0 && tick > std::numeric_limits<Tick>::max() + clock.phase)) {
        return 'X';
    }
    relative = tick - clock.phase;
#endif
    auto position = relative % clock.period;
    if (position < 0) position += clock.period;
    if (clock.dutyCycle.numerator
        > std::numeric_limits<Tick>::max() / clock.period) {
        return 'X';
    }
    const auto highTicks = clock.period * clock.dutyCycle.numerator
        / clock.dutyCycle.denominator;
    return position < highTicks ? '1' : '0';
}

void normalizeSegments(
    Lane& lane,
    const bool allowInvalidValues)
{
    for (auto& segment : lane.segments) {
        if (segment.id.empty()) {
            segment.id = makeStableId("segment");
        }
        if (segment.start < 0 || segment.end <= segment.start) {
            throw std::invalid_argument("segment interval is invalid");
        }
        const auto validation = validateLaneValue(lane, segment.value);
        if (!validation.valid) {
            if (!allowInvalidValues) {
                throw std::invalid_argument(validation.error);
            }
        } else {
            segment.value = validation.normalizedValue;
        }
    }

    std::sort(lane.segments.begin(), lane.segments.end(), [](const Segment& left, const Segment& right) {
        return left.start < right.start
            || (left.start == right.start && left.end < right.end);
    });

    std::vector<Segment> normalized;
    normalized.reserve(lane.segments.size());
    for (auto segment : lane.segments) {
        if (!normalized.empty() && segment.start < normalized.back().end) {
            throw std::invalid_argument("segments overlap");
        }
        if (!normalized.empty()
            && segment.start == normalized.back().end
            && segment.value == normalized.back().value
            && segment.extensions == normalized.back().extensions) {
            normalized.back().end = segment.end;
            continue;
        }
        normalized.push_back(std::move(segment));
    }
    lane.segments = std::move(normalized);
}

void setSegmentRange(
    Lane& lane,
    const Tick start,
    const Tick end,
    std::string value,
    std::string stableId,
    JsonExtensions extensions)
{
    if (start < 0 || end <= start) {
        throw std::invalid_argument("segment interval is invalid");
    }
    const auto validation = validateLaneValue(lane, value);
    if (!validation.valid) {
        throw std::invalid_argument(
            "invalid value '" + value + "' for lane '" + lane.name + "': " + validation.error);
    }
    value = validation.normalizedValue;

    std::vector<Segment> result;
    result.reserve(lane.segments.size() + 2);
    for (const auto& segment : lane.segments) {
        if (segment.end <= start || segment.start >= end) {
            result.push_back(segment);
            continue;
        }
        if (segment.start < start) {
            auto left = segment;
            left.end = start;
            result.push_back(std::move(left));
        }
        if (segment.end > end) {
            auto right = segment;
            right.id = makeStableId("segment");
            right.start = end;
            result.push_back(std::move(right));
        }
    }

    result.push_back({
        stableId.empty() ? makeStableId("segment") : std::move(stableId),
        start,
        end,
        std::move(value),
        std::move(extensions),
    });
    lane.segments = std::move(result);
    normalizeSegments(lane);
}

void clearSegmentRange(Lane& lane, const Tick start, const Tick end)
{
    if (start < 0 || end <= start) {
        throw std::invalid_argument("clear interval is invalid");
    }

    std::vector<Segment> result;
    result.reserve(lane.segments.size() + 1);
    for (const auto& segment : lane.segments) {
        if (segment.end <= start || segment.start >= end) {
            result.push_back(segment);
            continue;
        }
        if (segment.start < start) {
            auto left = segment;
            left.end = start;
            result.push_back(std::move(left));
        }
        if (segment.end > end) {
            auto right = segment;
            right.id = makeStableId("segment");
            right.start = end;
            result.push_back(std::move(right));
        }
    }
    lane.segments = std::move(result);
    normalizeSegments(lane);
}

void synchronizeLaneEventsFromSegments(
    Scenario& scenario,
    const std::string_view laneId,
    const EventAction action)
{
    auto* lane = findLane(scenario, laneId);
    if (!lane || lane->kind == LaneKind::Clock || lane->kind == LaneKind::Group) {
        return;
    }

    std::vector<Event> linkedEvents;
    std::vector<Event> retainedEvents;
    linkedEvents.reserve(lane->segments.size());
    retainedEvents.reserve(scenario.events.size() + lane->segments.size());
    for (auto& event : scenario.events) {
        if (event.waveformLinked && event.laneId == laneId) {
            linkedEvents.push_back(std::move(event));
        } else {
            retainedEvents.push_back(std::move(event));
        }
    }

    std::vector<bool> reused(linkedEvents.size(), false);
    const auto reusable = [&linkedEvents, &reused](const auto& predicate) {
        for (std::size_t index = 0; index < linkedEvents.size(); ++index) {
            if (!reused[index] && predicate(linkedEvents[index])) return index;
        }
        return linkedEvents.size();
    };
    const Segment* previousSegment = nullptr;
    for (const auto& segment : lane->segments) {
        const auto previousValue = previousSegment && previousSegment->end == segment.start
            ? std::string_view(previousSegment->value) : std::string_view("0");
        previousSegment = &segment;
        auto index = reusable([&segment](const Event& event) {
            return event.linkedSegmentId == segment.id;
        });
        if (index == linkedEvents.size()) {
            index = reusable([&segment](const Event& event) {
                return event.tick == segment.start && event.value == segment.value;
            });
        }
        if (index == linkedEvents.size()) {
            index = reusable([&segment](const Event& event) {
                return event.tick == segment.start;
            });
        }

        const auto eventAction = index == linkedEvents.size() ? action : linkedEvents[index].action;
        // Bit gaps are implicit 0. A same-value boundary is not a draggable
        // edge; keep the Segment, initial drive and explicit non-drive actions.
        if (lane->kind == LaneKind::Bit && segment.start > 0
            && segment.value == previousValue && eventAction == EventAction::Drive) {
            continue;
        }

        Event event;
        if (index != linkedEvents.size()) {
            reused[index] = true;
            event = std::move(linkedEvents[index]);
        } else {
            event.id = makeStableId("event");
            event.action = action;
            event.description = "Waveform segment";
        }
        event.laneId = std::string(laneId);
        event.tick = segment.start;
        event.value = segment.value;
        event.linkedSegmentId = segment.id;
        event.waveformLinked = true;
        retainedEvents.push_back(std::move(event));
    }

    std::unordered_set<std::string> removedEventIds;
    for (std::size_t index = 0; index < linkedEvents.size(); ++index) {
        if (!reused[index]) removedEventIds.insert(linkedEvents[index].id);
    }
    scenario.events = std::move(retainedEvents);
    if (!removedEventIds.empty()) {
        std::erase_if(
            scenario.relations,
            [&removedEventIds](const Relation& relation) {
                return removedEventIds.contains(relation.sourceEventId)
                    || removedEventIds.contains(relation.targetEventId);
            });
    }

    std::stable_sort(
        scenario.events.begin(),
        scenario.events.end(),
        [](const Event& left, const Event& right) {
            return left.tick < right.tick
                || (left.tick == right.tick && left.id < right.id);
        });
}

std::string_view toString(const LaneKind kind) noexcept
{
    switch (kind) {
    case LaneKind::Clock: return "clock";
    case LaneKind::Bit: return "bit";
    case LaneKind::Bus: return "bus";
    case LaneKind::Enum: return "enum";
    case LaneKind::Transaction: return "transaction";
    case LaneKind::Event: return "event";
    case LaneKind::Group: return "group";
    }
    return "bit";
}

std::optional<LaneKind> laneKindFromString(const std::string_view text) noexcept
{
    return enumFromString<LaneKind>(text, {
        {"clock", LaneKind::Clock},
        {"bit", LaneKind::Bit},
        {"bus", LaneKind::Bus},
        {"enum", LaneKind::Enum},
        {"transaction", LaneKind::Transaction},
        {"event", LaneKind::Event},
        {"group", LaneKind::Group},
    });
}

std::string_view toString(const Radix radix) noexcept
{
    switch (radix) {
    case Radix::Binary: return "binary";
    case Radix::Octal: return "octal";
    case Radix::Decimal: return "decimal";
    case Radix::Hexadecimal: return "hexadecimal";
    }
    return "hexadecimal";
}

std::optional<Radix> radixFromString(const std::string_view text) noexcept
{
    return enumFromString<Radix>(text, {
        {"binary", Radix::Binary},
        {"octal", Radix::Octal},
        {"decimal", Radix::Decimal},
        {"hexadecimal", Radix::Hexadecimal},
    });
}

std::string_view toString(const ClockEdge edge) noexcept
{
    return edge == ClockEdge::Rising ? "rising" : "falling";
}

std::optional<ClockEdge> clockEdgeFromString(const std::string_view text) noexcept
{
    return enumFromString<ClockEdge>(text, {
        {"rising", ClockEdge::Rising},
        {"falling", ClockEdge::Falling},
    });
}

std::string_view toString(const EventAction action) noexcept
{
    switch (action) {
    case EventAction::Drive: return "drive";
    case EventAction::Expect: return "expect";
    case EventAction::Pulse: return "pulse";
    case EventAction::Toggle: return "toggle";
    case EventAction::SendFrame: return "send-frame";
    case EventAction::ReceiveFrame: return "receive-frame";
    case EventAction::Marker: return "marker";
    case EventAction::Note: return "note";
    case EventAction::WaitCondition: return "wait-condition";
    }
    return "drive";
}

std::optional<EventAction> eventActionFromString(const std::string_view text) noexcept
{
    return enumFromString<EventAction>(text, {
        {"drive", EventAction::Drive},
        {"expect", EventAction::Expect},
        {"pulse", EventAction::Pulse},
        {"toggle", EventAction::Toggle},
        {"send-frame", EventAction::SendFrame},
        {"receive-frame", EventAction::ReceiveFrame},
        {"marker", EventAction::Marker},
        {"note", EventAction::Note},
        {"wait-condition", EventAction::WaitCondition},
    });
}

std::string_view toString(const MarkerKind kind) noexcept
{
    switch (kind) {
    case MarkerKind::Point: return "point";
    case MarkerKind::Interval: return "interval";
    case MarkerKind::Phase: return "phase";
    case MarkerKind::Error: return "error";
    case MarkerKind::Note: return "note";
    }
    return "point";
}

std::optional<MarkerKind> markerKindFromString(const std::string_view text) noexcept
{
    return enumFromString<MarkerKind>(text, {
        {"point", MarkerKind::Point},
        {"interval", MarkerKind::Interval},
        {"phase", MarkerKind::Phase},
        {"error", MarkerKind::Error},
        {"note", MarkerKind::Note},
    });
}

std::string_view toString(const Severity severity) noexcept
{
    switch (severity) {
    case Severity::Information: return "information";
    case Severity::Warning: return "warning";
    case Severity::Error: return "error";
    }
    return "error";
}

std::optional<Severity> severityFromString(const std::string_view text) noexcept
{
    return enumFromString<Severity>(text, {
        {"information", Severity::Information},
        {"warning", Severity::Warning},
        {"error", Severity::Error},
    });
}

Project makeDemonstrationProject()
{
    Project project;
    project.id = "project-wave-workbench-demo";
    project.name = "Handshake timing";
    project.timeBase = {1};
    project.clockDomains.push_back({
        "clock-main",
        "clk",
        10'000,
        0,
        {1, 2},
        ClockEdge::Rising,
        "reset_n == 0",
        {},
    });

    Scenario scenario;
    scenario.id = "scenario-handshake";
    scenario.name = "Request / acknowledge";
    scenario.duration = 220'000;

    Lane clock;
    clock.id = "lane-clk";
    clock.name = "clk";
    clock.kind = LaneKind::Clock;
    clock.clockDomainId = "clock-main";
    clock.color = "#80cbc4";
    setSegmentRange(clock, 170'000, 190'000, "gated", "segment-clk-gated");
    setSegmentRange(clock, 200'000, 210'000, "disabled", "segment-clk-disabled");

    Lane reset;
    reset.id = "lane-reset";
    reset.name = "reset_n";
    reset.kind = LaneKind::Bit;
    reset.color = "#ffb74d";
    setSegmentRange(reset, 0, 40'000, "0", "segment-reset-low");
    setSegmentRange(reset, 40'000, scenario.duration, "1", "segment-reset-high");

    Lane group;
    group.id = "group-handshake";
    group.name = "Handshake signals";
    group.kind = LaneKind::Group;
    group.color = "#90a4ae";
    group.height = 40;
    group.visible = false;

    Lane request;
    request.id = "lane-request";
    request.name = "req";
    request.kind = LaneKind::Bit;
    request.color = "#64b5f6";
    request.groupId = group.id;
    setSegmentRange(request, 0, 80'000, "0", "segment-req-low-a");
    setSegmentRange(request, 80'000, 130'000, "1", "segment-req-high");
    setSegmentRange(request, 130'000, scenario.duration, "0", "segment-req-low-b");

    Lane acknowledge;
    acknowledge.id = "lane-ack";
    acknowledge.name = "ack";
    acknowledge.kind = LaneKind::Bit;
    acknowledge.color = "#81c784";
    acknowledge.groupId = group.id;
    setSegmentRange(acknowledge, 0, 110'000, "0", "segment-ack-low-a");
    setSegmentRange(acknowledge, 110'000, 150'000, "1", "segment-ack-high");
    setSegmentRange(acknowledge, 150'000, scenario.duration, "0", "segment-ack-low-b");

    Lane data;
    data.id = "lane-data";
    data.name = "data[7:0]";
    data.kind = LaneKind::Bus;
    data.width = 8;
    data.radix = Radix::Hexadecimal;
    data.color = "#ce93d8";
    data.groupId = group.id;
    setSegmentRange(data, 0, 80'000, "0x00", "segment-data-idle-a");
    setSegmentRange(data, 80'000, 150'000, "0x35", "segment-data-payload");
    setSegmentRange(data, 150'000, scenario.duration, "0x00", "segment-data-idle-b");

    Lane state;
    state.id = "lane-state";
    state.name = "state";
    state.kind = LaneKind::Enum;
    state.width = 2;
    state.enumMap = {{"IDLE", "0"}, {"WAIT_ACK", "1"}, {"DONE", "2"}};
    state.color = "#ef9a9a";
    state.groupId = group.id;
    setSegmentRange(state, 0, 80'000, "IDLE", "segment-state-idle-a");
    setSegmentRange(state, 80'000, 150'000, "WAIT_ACK", "segment-state-wait");
    setSegmentRange(state, 150'000, scenario.duration, "DONE", "segment-state-done");

    scenario.lanes = {
        std::move(clock),
        std::move(reset),
        std::move(group),
        std::move(request),
        std::move(acknowledge),
        std::move(data),
        std::move(state),
    };
    for (const auto& lane : scenario.lanes) {
        synchronizeLaneEventsFromSegments(scenario, lane.id);
    }
    const auto source = std::find_if(
        scenario.events.begin(),
        scenario.events.end(),
        [](const Event& event) {
            return event.linkedSegmentId == "segment-req-high";
        });
    const auto target = std::find_if(
        scenario.events.begin(),
        scenario.events.end(),
        [](const Event& event) {
            return event.linkedSegmentId == "segment-ack-high";
        });
    if (source != scenario.events.end() && target != scenario.events.end()) {
        scenario.relations.push_back({
            "relation-req-ack",
            source->id,
            target->id,
            10'000,
            40'000,
            "clock-main",
            {},
            Severity::Error,
            "ack must rise within 1..4 cycles after req",
            {},
        });
    }
    project.scenarios.push_back(std::move(scenario));
    return project;
}

} // namespace wave
