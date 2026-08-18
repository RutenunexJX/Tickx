#pragma once

#include "wave/time.h"

namespace wave {

class TimelineViewport
{
public:
    TimelineViewport() = default;
    TimelineViewport(
        Tick domainStart,
        Tick domainEnd,
        double pixelsPerTick,
        double originPixel = 0.0,
        double pixelOffset = 0.0) noexcept;

    void setDomain(Tick start, Tick end) noexcept;
    void setPixelsPerTick(double value) noexcept;
    void setOriginPixel(double value) noexcept;
    void setPixelOffset(double value) noexcept;

    [[nodiscard]] Tick domainStart() const noexcept;
    [[nodiscard]] Tick domainEnd() const noexcept;
    [[nodiscard]] double pixelsPerTick() const noexcept;
    [[nodiscard]] double originPixel() const noexcept;
    [[nodiscard]] double pixelOffset() const noexcept;

    [[nodiscard]] double pixelForTick(Tick tick) const noexcept;
    [[nodiscard]] Tick tickAtPixel(double pixel) const noexcept;
    [[nodiscard]] Tick visibleEnd(double viewportRightPixel) const noexcept;
    [[nodiscard]] static double fittedPixelsPerTick(
        Tick domainStart,
        Tick domainEnd,
        double drawablePixels) noexcept;

private:
    Tick domainStart_{0};
    Tick domainEnd_{0};
    double pixelsPerTick_{1.0};
    double originPixel_{0.0};
    double pixelOffset_{0.0};
};

} // namespace wave
