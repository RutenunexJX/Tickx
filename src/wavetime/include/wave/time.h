#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace wave {

using Tick = std::int64_t;
using JsonExtensions = std::map<std::string, std::string>;

enum class TimeUnit {
    Picosecond,
    Nanosecond,
    Microsecond,
    Millisecond,
};

struct Rational {
    std::int64_t numerator{1};
    std::int64_t denominator{1};

    [[nodiscard]] bool isValid() const noexcept;
    void normalize();
    [[nodiscard]] bool operator==(const Rational&) const = default;
};

struct TimeBase {
    std::int64_t picosecondsPerTick{1};

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] bool operator==(const TimeBase&) const = default;
};

enum class ClockEdge {
    Rising,
    Falling,
};

struct ClockDomain {
    std::string id;
    std::string name;
    Tick period{1};
    Tick phase{0};
    Rational dutyCycle{1, 2};
    ClockEdge activeEdge{ClockEdge::Rising};
    std::string resetRelation;
    JsonExtensions extensions;

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] bool operator==(const ClockDomain&) const = default;
};

enum class SnapMode {
    None,
    FixedGrid,
    MajorTick,
    ClockRising,
    ClockFalling,
    SignalEdge,
    Marker,
};

struct SnapContext {
    Tick fixedGrid{1};
    Tick majorTick{1};
    const ClockDomain* clock{nullptr};
    // Signal and marker spans must be sorted in ascending tick order.
    std::span<const Tick> signalEdges{};
    std::span<const Tick> markers{};
};

[[nodiscard]] std::optional<Tick> toTicks(
    std::int64_t value,
    TimeUnit unit,
    const TimeBase& timeBase) noexcept;

// Converts a decimal physical-unit value without using floating point.
// The result is returned only when both the picosecond value and project tick
// are exactly representable.
[[nodiscard]] std::optional<Tick> toTicks(
    std::string_view decimalValue,
    TimeUnit unit,
    const TimeBase& timeBase) noexcept;

[[nodiscard]] std::optional<std::int64_t> fromTicks(
    Tick ticks,
    TimeUnit unit,
    const TimeBase& timeBase) noexcept;

[[nodiscard]] std::optional<Tick> tickAtCycle(
    const ClockDomain& clock,
    std::int64_t cycle,
    ClockEdge edge) noexcept;

[[nodiscard]] Tick snapTick(
    Tick input,
    SnapMode mode,
    const SnapContext& context) noexcept;

[[nodiscard]] std::string formatTick(Tick tick, const TimeBase& timeBase);
[[nodiscard]] std::string_view timeUnitSuffix(TimeUnit unit) noexcept;

} // namespace wave
