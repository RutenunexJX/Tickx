#include "wave/timeline_viewport.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace wave {
namespace {

constexpr double kMinimumScale = 1.0e-12;
constexpr double kMaximumScale = 1.0e6;

Tick roundedTick(const long double value) noexcept
{
    if (value <= static_cast<long double>(std::numeric_limits<Tick>::min())) {
        return std::numeric_limits<Tick>::min();
    }
    if (value >= static_cast<long double>(std::numeric_limits<Tick>::max())) {
        return std::numeric_limits<Tick>::max();
    }
    return static_cast<Tick>(std::llround(value));
}

double finiteOr(const double value, const double fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}

} // namespace

TimelineViewport::TimelineViewport(
    const Tick domainStart,
    const Tick domainEnd,
    const double pixelsPerTick,
    const double originPixel,
    const double pixelOffset) noexcept
{
    setDomain(domainStart, domainEnd);
    setPixelsPerTick(pixelsPerTick);
    setOriginPixel(originPixel);
    setPixelOffset(pixelOffset);
}

void TimelineViewport::setDomain(const Tick start, const Tick end) noexcept
{
    domainStart_ = std::min(start, end);
    domainEnd_ = std::max(start, end);
}

void TimelineViewport::setPixelsPerTick(const double value) noexcept
{
    pixelsPerTick_ = std::clamp(
        finiteOr(value, 1.0), kMinimumScale, kMaximumScale);
}

void TimelineViewport::setOriginPixel(const double value) noexcept
{
    originPixel_ = finiteOr(value, 0.0);
}

void TimelineViewport::setPixelOffset(const double value) noexcept
{
    pixelOffset_ = finiteOr(value, 0.0);
}

Tick TimelineViewport::domainStart() const noexcept
{
    return domainStart_;
}

Tick TimelineViewport::domainEnd() const noexcept
{
    return domainEnd_;
}

double TimelineViewport::pixelsPerTick() const noexcept
{
    return pixelsPerTick_;
}

double TimelineViewport::originPixel() const noexcept
{
    return originPixel_;
}

double TimelineViewport::pixelOffset() const noexcept
{
    return pixelOffset_;
}

double TimelineViewport::pixelForTick(const Tick tick) const noexcept
{
    return originPixel_
        + static_cast<double>(
            static_cast<long double>(tick)
            - static_cast<long double>(domainStart_))
            * pixelsPerTick_
        - pixelOffset_;
}

Tick TimelineViewport::tickAtPixel(const double pixel) const noexcept
{
    const auto relative =
        (static_cast<long double>(finiteOr(pixel, originPixel_))
         - static_cast<long double>(originPixel_)
         + static_cast<long double>(pixelOffset_))
        / static_cast<long double>(pixelsPerTick_);
    return std::clamp(
        roundedTick(static_cast<long double>(domainStart_) + relative),
        domainStart_,
        domainEnd_);
}

Tick TimelineViewport::visibleEnd(const double viewportRightPixel) const noexcept
{
    return tickAtPixel(viewportRightPixel);
}

double TimelineViewport::fittedPixelsPerTick(
    const Tick domainStart,
    const Tick domainEnd,
    const double drawablePixels) noexcept
{
    const auto duration = std::max<long double>(
        1.0L,
        static_cast<long double>(std::max(domainStart, domainEnd))
            - static_cast<long double>(std::min(domainStart, domainEnd)));
    return std::clamp(
        finiteOr(drawablePixels, 1.0) / static_cast<double>(duration),
        kMinimumScale,
        kMaximumScale);
}

} // namespace wave
