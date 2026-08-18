#include "wave/timeline_viewport.h"

#include <cmath>
#include <iostream>

namespace {
int failures = 0;

void check(const bool condition, const char* message)
{
    if (condition) return;
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
}
} // namespace

int main()
{
    wave::TimelineViewport viewport(100, 1'100, 0.5, 230.0, 50.0);
    check(std::abs(viewport.pixelForTick(300) - 280.0) < 0.001,
          "tick-to-pixel mapping includes domain, origin, and offset");
    check(viewport.tickAtPixel(280.0) == 300,
          "pixel-to-tick mapping is the exact inverse");
    check(viewport.tickAtPixel(-1000.0) == 100,
          "pixel mapping clamps to the domain start");
    check(viewport.tickAtPixel(10000.0) == 1'100,
          "pixel mapping clamps to the domain end");
    check(std::abs(wave::TimelineViewport::fittedPixelsPerTick(
                       100, 1'100, 500.0)
                   - 0.5)
              < 0.001,
          "fit scale uses the full domain");

    viewport.setDomain(20, -20);
    check(viewport.domainStart() == -20 && viewport.domainEnd() == 20,
          "reversed domains are normalized");
    viewport.setPixelsPerTick(0.0);
    check(viewport.pixelsPerTick() > 0.0,
          "invalid scales are clamped to a usable value");

    std::cout << "timeline viewport failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
