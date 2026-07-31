#include "wave/time.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <limits>
#include <numeric>
#include <sstream>

namespace wave {
namespace {

constexpr std::int64_t kPsPerNanosecond = 1'000;
constexpr std::int64_t kPsPerMicrosecond = 1'000'000;
constexpr std::int64_t kPsPerMillisecond = 1'000'000'000;

std::int64_t unitScale(const TimeUnit unit) noexcept
{
    switch (unit) {
    case TimeUnit::Picosecond:
        return 1;
    case TimeUnit::Nanosecond:
        return kPsPerNanosecond;
    case TimeUnit::Microsecond:
        return kPsPerMicrosecond;
    case TimeUnit::Millisecond:
        return kPsPerMillisecond;
    }
    return 1;
}

bool parseUnsignedDecimal(
    const std::string_view digits,
    std::uint64_t& result) noexcept
{
    result = 0;
    if (digits.empty()) return false;
    for (const auto character : digits) {
        if (character < '0' || character > '9') return false;
        const auto digit = static_cast<std::uint64_t>(character - '0');
        if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) {
            return false;
        }
        result = result * 10 + digit;
    }
    return true;
}

std::uint64_t signedMagnitudeLimit(const bool negative) noexcept
{
    const auto maximum =
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    return negative ? maximum + 1U : maximum;
}

std::uint64_t unsignedMagnitude(const std::int64_t value) noexcept
{
    return value < 0
        ? static_cast<std::uint64_t>(-(value + 1)) + 1U
        : static_cast<std::uint64_t>(value);
}

bool checkedMultiply(
    const std::int64_t left,
    const std::int64_t right,
    std::int64_t& result) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_mul_overflow(left, right, &result);
#else
    if (left == 0 || right == 0) {
        result = 0;
        return true;
    }
    if (left == -1 && right == std::numeric_limits<std::int64_t>::min()) {
        return false;
    }
    if (right == -1 && left == std::numeric_limits<std::int64_t>::min()) {
        return false;
    }
    if (left > 0) {
        if ((right > 0 && left > std::numeric_limits<std::int64_t>::max() / right)
            || (right < 0 && right < std::numeric_limits<std::int64_t>::min() / left)) {
            return false;
        }
    } else if (left < 0) {
        if ((right > 0 && left < std::numeric_limits<std::int64_t>::min() / right)
            || (right < 0 && left < std::numeric_limits<std::int64_t>::max() / right)) {
            return false;
        }
    }
    result = left * right;
    return true;
#endif
}

bool checkedAdd(
    const std::int64_t left,
    const std::int64_t right,
    std::int64_t& result) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_add_overflow(left, right, &result);
#else
    if ((right > 0 && left > std::numeric_limits<std::int64_t>::max() - right)
        || (right < 0 && left < std::numeric_limits<std::int64_t>::min() - right)) {
        return false;
    }
    result = left + right;
    return true;
#endif
}

bool checkedSubtract(
    const std::int64_t left,
    const std::int64_t right,
    std::int64_t& result) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_sub_overflow(left, right, &result);
#else
    if ((right > 0 && left < std::numeric_limits<std::int64_t>::min() + right)
        || (right < 0 && left > std::numeric_limits<std::int64_t>::max() + right)) {
        return false;
    }
    result = left - right;
    return true;
#endif
}

std::int64_t floorDivide(const std::int64_t numerator, const std::int64_t denominator) noexcept
{
    auto quotient = numerator / denominator;
    const auto remainder = numerator % denominator;
    if (remainder != 0 && ((remainder < 0) != (denominator < 0))) {
        --quotient;
    }
    return quotient;
}

Tick nearestMultiple(const Tick input, const Tick step) noexcept
{
    if (step <= 0) {
        return input;
    }

    const auto lowerFactor = floorDivide(input, step);
    std::int64_t lower = 0;
    if (!checkedMultiply(lowerFactor, step, lower)) {
        return input;
    }

    if (lower > std::numeric_limits<Tick>::max() - step) {
        return lower;
    }
    const auto upper = lower + step;
    const auto distanceToLower = static_cast<std::uint64_t>(input - lower);
    const auto distanceToUpper = static_cast<std::uint64_t>(upper - input);
    return distanceToLower <= distanceToUpper ? lower : upper;
}

Tick nearestValue(const Tick input, const std::span<const Tick> values) noexcept
{
    if (values.empty()) {
        return input;
    }

    auto unsignedDistance = [](const Tick first, const Tick second) {
        if (first >= second) {
            return static_cast<std::uint64_t>(first) - static_cast<std::uint64_t>(second);
        }
        return static_cast<std::uint64_t>(second) - static_cast<std::uint64_t>(first);
    };

    const auto upper = std::lower_bound(values.begin(), values.end(), input);
    if (upper == values.begin()) return *upper;
    if (upper == values.end()) return values.back();
    const auto lower = std::prev(upper);
    return unsignedDistance(input, *upper) < unsignedDistance(input, *lower)
        ? *upper
        : *lower;
}

Tick snapClock(const Tick input, const ClockDomain& clock, const ClockEdge edge) noexcept
{
    if (!clock.isValid()) {
        return input;
    }

    Tick edgeOffset = 0;
    if (edge == ClockEdge::Falling) {
        std::int64_t product = 0;
        if (!checkedMultiply(clock.period, clock.dutyCycle.numerator, product)) {
            return input;
        }
        edgeOffset = product / clock.dutyCycle.denominator;
    }

    Tick origin = 0;
    if (!checkedAdd(clock.phase, edgeOffset, origin)) {
        return input;
    }
    Tick relative = 0;
    if (!checkedSubtract(input, origin, relative)) {
        return input;
    }
    const auto snappedRelative = nearestMultiple(relative, clock.period);
    Tick result = 0;
    if (!checkedAdd(origin, snappedRelative, result)) {
        return input;
    }
    return result;
}

} // namespace

bool Rational::isValid() const noexcept
{
    return denominator > 0 && numerator >= 0;
}

void Rational::normalize()
{
    if (denominator == 0) {
        return;
    }
    if (denominator < 0) {
        denominator = -denominator;
        numerator = -numerator;
    }
    const auto divisor = std::gcd(numerator, denominator);
    if (divisor != 0) {
        numerator /= divisor;
        denominator /= divisor;
    }
}

bool TimeBase::isValid() const noexcept
{
    return picosecondsPerTick > 0;
}

bool ClockDomain::isValid() const noexcept
{
    return !id.empty() && period > 0 && dutyCycle.isValid()
        && dutyCycle.numerator > 0
        && dutyCycle.numerator < dutyCycle.denominator;
}

std::optional<Tick> toTicks(
    const std::int64_t value,
    const TimeUnit unit,
    const TimeBase& timeBase) noexcept
{
    if (!timeBase.isValid()) {
        return std::nullopt;
    }

    std::int64_t picoseconds = 0;
    if (!checkedMultiply(value, unitScale(unit), picoseconds)) {
        return std::nullopt;
    }
    if (picoseconds % timeBase.picosecondsPerTick != 0) {
        return std::nullopt;
    }
    return picoseconds / timeBase.picosecondsPerTick;
}

std::optional<Tick> toTicks(
    std::string_view decimalValue,
    const TimeUnit unit,
    const TimeBase& timeBase) noexcept
{
    if (!timeBase.isValid() || decimalValue.empty()) return std::nullopt;

    auto negative = false;
    if (decimalValue.front() == '-' || decimalValue.front() == '+') {
        negative = decimalValue.front() == '-';
        decimalValue.remove_prefix(1);
    }
    if (decimalValue.empty()) return std::nullopt;

    const auto decimalPoint = decimalValue.find('.');
    if (decimalPoint != std::string_view::npos
        && decimalValue.find('.', decimalPoint + 1) != std::string_view::npos) {
        return std::nullopt;
    }
    auto wholeDigits = decimalPoint == std::string_view::npos
        ? decimalValue
        : decimalValue.substr(0, decimalPoint);
    auto fractionalDigits = decimalPoint == std::string_view::npos
        ? std::string_view{}
        : decimalValue.substr(decimalPoint + 1);
    if (wholeDigits.empty()) wholeDigits = std::string_view{"0"};
    if (fractionalDigits.empty() && decimalPoint != std::string_view::npos
        && decimalPoint == 0) {
        return std::nullopt;
    }
    while (!fractionalDigits.empty() && fractionalDigits.back() == '0') {
        fractionalDigits.remove_suffix(1);
    }

    std::uint64_t whole = 0;
    if (!parseUnsignedDecimal(wholeDigits, whole)) return std::nullopt;
    std::uint64_t fraction = 0;
    if (!fractionalDigits.empty()
        && !parseUnsignedDecimal(fractionalDigits, fraction)) {
        return std::nullopt;
    }

    const auto scale = static_cast<std::uint64_t>(unitScale(unit));
    auto fractionalScale = std::uint64_t{1};
    for (std::size_t index = 0; index < fractionalDigits.size(); ++index) {
        if (fractionalScale > scale / 10U) return std::nullopt;
        fractionalScale *= 10U;
    }
    if (scale % fractionalScale != 0) return std::nullopt;

    const auto fractionalPicoseconds =
        fraction * (scale / fractionalScale);
    const auto magnitudeLimit = signedMagnitudeLimit(negative);
    if (fractionalPicoseconds > magnitudeLimit
        || whole > (magnitudeLimit - fractionalPicoseconds) / scale) {
        return std::nullopt;
    }
    const auto picosecondMagnitude =
        whole * scale + fractionalPicoseconds;
    const auto timeBaseMagnitude =
        static_cast<std::uint64_t>(timeBase.picosecondsPerTick);
    if (picosecondMagnitude % timeBaseMagnitude != 0) {
        return std::nullopt;
    }
    const auto tickMagnitude = picosecondMagnitude / timeBaseMagnitude;
    if (negative
        && tickMagnitude
            == static_cast<std::uint64_t>(
                   std::numeric_limits<std::int64_t>::max())
                + 1U) {
        return std::numeric_limits<std::int64_t>::min();
    }
    const auto ticks = static_cast<std::int64_t>(tickMagnitude);
    return negative ? -ticks : ticks;
}

std::optional<std::int64_t> fromTicks(
    const Tick ticks,
    const TimeUnit unit,
    const TimeBase& timeBase) noexcept
{
    if (!timeBase.isValid()) {
        return std::nullopt;
    }

    std::int64_t picoseconds = 0;
    if (!checkedMultiply(ticks, timeBase.picosecondsPerTick, picoseconds)) {
        return std::nullopt;
    }
    const auto scale = unitScale(unit);
    if (picoseconds % scale != 0) {
        return std::nullopt;
    }
    return picoseconds / scale;
}

std::optional<Tick> tickAtCycle(
    const ClockDomain& clock,
    const std::int64_t cycle,
    const ClockEdge edge) noexcept
{
    if (!clock.isValid()) {
        return std::nullopt;
    }

    std::int64_t cycleOffset = 0;
    if (!checkedMultiply(cycle, clock.period, cycleOffset)) {
        return std::nullopt;
    }
    std::int64_t edgeOffset = 0;
    if (edge == ClockEdge::Falling) {
        std::int64_t product = 0;
        if (!checkedMultiply(clock.period, clock.dutyCycle.numerator, product)) {
            return std::nullopt;
        }
        edgeOffset = product / clock.dutyCycle.denominator;
    }

    Tick partial = 0;
    if (!checkedAdd(clock.phase, cycleOffset, partial)) {
        return std::nullopt;
    }
    Tick result = 0;
    if (!checkedAdd(partial, edgeOffset, result)) {
        return std::nullopt;
    }
    return result;
}

Tick snapTick(
    const Tick input,
    const SnapMode mode,
    const SnapContext& context) noexcept
{
    switch (mode) {
    case SnapMode::None:
        return input;
    case SnapMode::FixedGrid:
        return nearestMultiple(input, context.fixedGrid);
    case SnapMode::MajorTick:
        return nearestMultiple(input, context.majorTick);
    case SnapMode::ClockRising:
        return context.clock ? snapClock(input, *context.clock, ClockEdge::Rising) : input;
    case SnapMode::ClockFalling:
        return context.clock ? snapClock(input, *context.clock, ClockEdge::Falling) : input;
    case SnapMode::SignalEdge:
        return nearestValue(input, context.signalEdges);
    case SnapMode::Marker:
        return nearestValue(input, context.markers);
    }
    return input;
}

std::string formatTick(const Tick tick, const TimeBase& timeBase)
{
    if (!timeBase.isValid()) {
        return std::to_string(tick) + " tick";
    }

    std::int64_t picoseconds = 0;
    if (!checkedMultiply(tick, timeBase.picosecondsPerTick, picoseconds)) {
        return std::to_string(tick) + " tick";
    }
    if (picoseconds == 0) {
        return "0 ps";
    }

    struct DisplayUnit {
        TimeUnit unit;
        std::uint64_t scale;
        std::uint64_t fractionalQuantum;
    };
    constexpr std::array<DisplayUnit, 3> units{{
        {
            TimeUnit::Millisecond,
            static_cast<std::uint64_t>(kPsPerMillisecond),
            static_cast<std::uint64_t>(kPsPerMillisecond / 1'000),
        },
        {
            TimeUnit::Microsecond,
            static_cast<std::uint64_t>(kPsPerMicrosecond),
            static_cast<std::uint64_t>(kPsPerMicrosecond / 1'000),
        },
        {
            TimeUnit::Nanosecond,
            static_cast<std::uint64_t>(kPsPerNanosecond),
            1,
        },
    }};
    const auto magnitude = unsignedMagnitude(picoseconds);
    for (const auto& [unit, scale, fractionalQuantum] : units) {
        if (magnitude < scale || magnitude % fractionalQuantum != 0) continue;

        auto result = picoseconds < 0 ? std::string{"-"} : std::string{};
        result += std::to_string(magnitude / scale);
        auto fraction = (magnitude % scale) / fractionalQuantum;
        if (fraction != 0) {
            auto fractionText = std::to_string(fraction);
            fractionText.insert(
                fractionText.begin(),
                3U - fractionText.size(),
                '0');
            while (!fractionText.empty() && fractionText.back() == '0') {
                fractionText.pop_back();
            }
            result += "." + fractionText;
        }
        return result + " " + std::string(timeUnitSuffix(unit));
    }
    return std::to_string(picoseconds) + " ps";
}

std::string_view timeUnitSuffix(const TimeUnit unit) noexcept
{
    switch (unit) {
    case TimeUnit::Picosecond:
        return "ps";
    case TimeUnit::Nanosecond:
        return "ns";
    case TimeUnit::Microsecond:
        return "us";
    case TimeUnit::Millisecond:
        return "ms";
    }
    return "tick";
}

} // namespace wave
