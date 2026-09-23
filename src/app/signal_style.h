#pragma once

#include "wave/model.h"
#include "waveform_theme.h"

#include <QWidget>
#include <QPen>
#include <QStringList>

class QComboBox;
class QDoubleSpinBox;
class QLineEdit;

namespace wave {

struct WAVEWIDGETS_API SignalStyleSettings {
    QString preset{"inherit"};
    QString edge{"preset"};
    QString corners{"preset"};
    QString palette{"inherit"};
    double stroke{0};
    QColor fill;
    [[nodiscard]] bool operator==(const SignalStyleSettings&) const = default;
};

struct WAVEWIDGETS_API SignalStyle {
    QColor color;
    QColor fill;
    double stroke{1.8};
    double bevel{7};
    double ramp{3};
    bool trapezoid{false};
    bool rounded{false};
    bool doubleOutline{false};
    bool facets{false};
    bool halo{false};
    bool pixel{false};
    [[nodiscard]] QPen pen(bool preview = false) const;
};

struct WAVEWIDGETS_API SignalBackgroundStyle {
    enum class Ruling { None, Ruled, Graph, Crosses };
    WaveformTheme theme{waveformTheme(WaveformColorScheme::Light)};
    QColor cycleBand;
    Qt::PenStyle minorGrid{Qt::SolidLine};
    Ruling ruling{Ruling::None};
};

[[nodiscard]] WAVEWIDGETS_API QStringList signalStylePresetIds();
[[nodiscard]] WAVEWIDGETS_API QStringList signalStylePresetNames();
[[nodiscard]] WAVEWIDGETS_API SignalStyleSettings signalStyleSettings(const JsonExtensions& extensions);
WAVEWIDGETS_API void storeSignalStyleSettings(JsonExtensions& extensions, const SignalStyleSettings& settings);
[[nodiscard]] WAVEWIDGETS_API SignalStyle resolvedSignalStyle(
    const Project* project, const Lane& lane, WaveformColorScheme scheme);
[[nodiscard]] WAVEWIDGETS_API SignalBackgroundStyle resolvedSignalBackground(
    const Project* project, WaveformColorScheme scheme);

class WAVEWIDGETS_API SignalStyleEditor final : public QWidget {
public:
    SignalStyleEditor(const SignalStyleSettings& initial, bool allowInherit, QWidget* parent = nullptr);
    [[nodiscard]] SignalStyleSettings settings() const;
    [[nodiscard]] bool valid() const;
private:
    QComboBox* preset_;
    QComboBox* edge_;
    QComboBox* corners_;
    QComboBox* palette_;
    QDoubleSpinBox* stroke_;
    QLineEdit* fill_;
};

} // namespace wave
