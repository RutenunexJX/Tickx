#pragma once

#include "wave/model.h"
#include "wave/widgets_export.h"

#include <QByteArray>
#include <QString>

class QWidget;

namespace wave {
inline constexpr auto bitPatternMime = "application/x-wave-workbench-bit-pattern+json";
inline constexpr qsizetype maxQuickPatternBits = 4096;
struct WAVEWIDGETS_API QuickBitPattern {
    QString bits;
    Tick step{1};
    bool useTargetClock{true};
    TimeBase timeBase{1};
};
[[nodiscard]] WAVEWIDGETS_API bool validQuickBitPattern(const QuickBitPattern& pattern);
[[nodiscard]] WAVEWIDGETS_API std::optional<Tick> quickBitPatternStep(const QuickBitPattern& pattern, const TimeBase& target);
[[nodiscard]] WAVEWIDGETS_API QByteArray encodeQuickBitPattern(const QuickBitPattern& pattern);
[[nodiscard]] WAVEWIDGETS_API std::optional<QuickBitPattern> decodeQuickBitPattern(const QByteArray& bytes);
WAVEWIDGETS_API QWidget* createQuickWaveformComposer(
    const TimeBase& timeBase, Tick initialStep, bool boundClock, QWidget* parent);
} // namespace wave
