#pragma once

#include "wave/widgets_export.h"

#include <QColor>
#include <QPalette>
#include <QString>

namespace wave {

enum class WaveformColorScheme {
    Light,
    Dark
};

struct WAVEWIDGETS_API WaveformTheme {
    QColor application;
    QColor canvas;
    QColor panel;
    QColor raised;
    QColor gridMajor;
    QColor gridMinor;
    QColor border;
    QColor text;
    QColor mutedText;
    QColor bit;
    QColor bus;
    QColor clock;
    QColor unknown;
    QColor expected;
    QColor actual;
    QColor difference;
    QColor cursor;
    QColor selection;
    QColor focus;
    QColor status;
    QColor diagnostic;
    QColor accent;
    QColor accentSecondary;
    QColor selectionText;
    QColor success;
    QColor warning;
    QColor error;
    QColor information;
    QColor successSurface;
    QColor warningSurface;
    QColor errorSurface;
    QColor informationSurface;
};

struct WAVEWIDGETS_API WaveformMetrics {
    int spacingUnit{4};
    int controlHeight{32};
    int compactControlHeight{28};
    int radius{6};
    int panelHeaderHeight{32};
    int noticePadding{8};
    int focusRingWidth{2};
};

[[nodiscard]] WAVEWIDGETS_API WaveformTheme waveformTheme(
    WaveformColorScheme scheme);
[[nodiscard]] WAVEWIDGETS_API WaveformColorScheme waveformColorScheme(
    const QPalette& palette);
[[nodiscard]] WAVEWIDGETS_API WaveformMetrics waveformMetrics() noexcept;
[[nodiscard]] WAVEWIDGETS_API double waveColorContrastRatio(
    const QColor& foreground,
    const QColor& background) noexcept;
[[nodiscard]] WAVEWIDGETS_API bool waveReducedMotionEnabled();
[[nodiscard]] WAVEWIDGETS_API QString waveApplicationStyleSheet(
    WaveformColorScheme scheme);

} // namespace wave
