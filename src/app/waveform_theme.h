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
};

[[nodiscard]] WAVEWIDGETS_API WaveformTheme waveformTheme(
    WaveformColorScheme scheme);
[[nodiscard]] WAVEWIDGETS_API WaveformColorScheme waveformColorScheme(
    const QPalette& palette);
[[nodiscard]] WAVEWIDGETS_API QString waveApplicationStyleSheet(
    WaveformColorScheme scheme);

} // namespace wave
