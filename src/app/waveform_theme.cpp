#include "waveform_theme.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QStyleHints>
#include <QVariant>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace wave {
namespace {

QString cssColor(const QColor& color)
{
    return color.name(QColor::HexRgb);
}

double relativeLuminance(const QColor& color) noexcept
{
    const auto linear = [](const double channel) {
        return channel <= 0.04045
            ? channel / 12.92
            : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.redF())
        + 0.7152 * linear(color.greenF())
        + 0.0722 * linear(color.blueF());
}

} // namespace

WaveformTheme waveformTheme(const WaveformColorScheme scheme)
{
    if (scheme == WaveformColorScheme::Dark) {
        return {
            QColor(QStringLiteral("#0B1020")),
            QColor(QStringLiteral("#0F172A")),
            QColor(QStringLiteral("#151F33")),
            QColor(QStringLiteral("#1D2940")),
            QColor(QStringLiteral("#3B4A64")),
            QColor(QStringLiteral("#273650")),
            QColor(QStringLiteral("#3B4A64")),
            QColor(QStringLiteral("#F1F5FB")),
            QColor(QStringLiteral("#A8B4C7")),
            QColor(QStringLiteral("#47D18C")),
            QColor(QStringLiteral("#4FA3FF")),
            QColor(QStringLiteral("#A99EFF")),
            QColor(QStringLiteral("#FFC857")),
            QColor(QStringLiteral("#47D18C")),
            QColor(QStringLiteral("#69B8FF")),
            QColor(QStringLiteral("#FF7A90")),
            QColor(QStringLiteral("#FFC857")),
            QColor(QStringLiteral("#332E6B")),
            QColor(QStringLiteral("#A99EFF")),
            QColor(QStringLiteral("#47D18C")),
            QColor(QStringLiteral("#FF7A90")),
            QColor(QStringLiteral("#8B7CFF")),
            QColor(QStringLiteral("#4FA3FF")),
            QColor(QStringLiteral("#FFFFFF")),
            QColor(QStringLiteral("#47D18C")),
            QColor(QStringLiteral("#FFC857")),
            QColor(QStringLiteral("#FF7A90")),
            QColor(QStringLiteral("#69B8FF")),
            QColor(QStringLiteral("#12392C")),
            QColor(QStringLiteral("#44330F")),
            QColor(QStringLiteral("#47202A")),
            QColor(QStringLiteral("#173451")),
        };
    }
    return {
        QColor(QStringLiteral("#EFF2F8")),
        QColor(QStringLiteral("#FBFCFF")),
        QColor(QStringLiteral("#F3F5FA")),
        QColor(QStringLiteral("#FFFFFF")),
        QColor(QStringLiteral("#C6CFDE")),
        QColor(QStringLiteral("#DCE3ED")),
        QColor(QStringLiteral("#C6CFDE")),
        QColor(QStringLiteral("#1C2433")),
        QColor(QStringLiteral("#59667A")),
        QColor(QStringLiteral("#18794E")),
        QColor(QStringLiteral("#2368D8")),
        QColor(QStringLiteral("#5B4DD6")),
        QColor(QStringLiteral("#8A5A00")),
        QColor(QStringLiteral("#18794E")),
        QColor(QStringLiteral("#165DA8")),
        QColor(QStringLiteral("#B4233A")),
        QColor(QStringLiteral("#8A5A00")),
        QColor(QStringLiteral("#E4E1FF")),
        QColor(QStringLiteral("#5141CF")),
        QColor(QStringLiteral("#18794E")),
        QColor(QStringLiteral("#B4233A")),
        QColor(QStringLiteral("#5B4DD6")),
        QColor(QStringLiteral("#2368D8")),
        QColor(QStringLiteral("#18152E")),
        QColor(QStringLiteral("#18794E")),
        QColor(QStringLiteral("#8A5A00")),
        QColor(QStringLiteral("#B4233A")),
        QColor(QStringLiteral("#165DA8")),
        QColor(QStringLiteral("#E2F5EB")),
        QColor(QStringLiteral("#FFF1CC")),
        QColor(QStringLiteral("#FDE8EC")),
        QColor(QStringLiteral("#E2F0FF")),
    };
}

WaveformColorScheme waveformColorScheme(const QPalette& palette)
{
    return palette.color(QPalette::Window).lightnessF() < 0.45
        ? WaveformColorScheme::Dark
        : WaveformColorScheme::Light;
}

WaveformMetrics waveformMetrics() noexcept
{
    return {};
}

double waveColorContrastRatio(
    const QColor& foreground,
    const QColor& background) noexcept
{
    if (!foreground.isValid() || !background.isValid()) return 0.0;
    const auto light = std::max(
        relativeLuminance(foreground),
        relativeLuminance(background));
    const auto dark = std::min(
        relativeLuminance(foreground),
        relativeLuminance(background));
    return (light + 0.05) / (dark + 0.05);
}

bool waveReducedMotionEnabled()
{
    const auto environment = qEnvironmentVariable(
        "WAVEWORKBENCH_REDUCED_MOTION").trimmed().toLower();
    if (!environment.isEmpty()) {
        return environment != QStringLiteral("0")
            && environment != QStringLiteral("false")
            && environment != QStringLiteral("no")
            && environment != QStringLiteral("off");
    }
    if (const auto* application = QCoreApplication::instance()) {
        const auto applicationPreference = application->property(
            "waveworkbench.reducedMotion");
        if (applicationPreference.isValid()) {
            return applicationPreference.toBool();
        }
    }
    if (const auto* hints = QGuiApplication::styleHints()) {
        const auto platformPreference = hints->property("reducedMotion");
        if (platformPreference.isValid()) return platformPreference.toBool();
    }
    return false;
}

QString waveApplicationStyleSheet(const WaveformColorScheme scheme)
{
    const auto theme = waveformTheme(scheme);
    const auto metrics = waveformMetrics();
    QString style = QStringLiteral(R"QSS(
QMainWindow, QDialog, QMessageBox { background: @APPLICATION@; color: @TEXT@; }
QWidget[waveSurface="canvas"] { background: @CANVAS@; color: @TEXT@; }
QWidget[waveSurface="panel"] { background: @PANEL@; color: @TEXT@; }
QWidget[waveSurface="raised"] { background: @RAISED@; color: @TEXT@; }
QDockWidget::title, QLabel[waveSectionHeader="true"], QLabel[waveRole="panelHeader"] {
  min-height: @HEADER_HEIGHT@px; background: @PANEL@; color: @TEXT@;
  border-bottom: 1px solid @BORDER@; padding: 0 @NOTICE_PADDING@px; font-weight: 600;
}
QFrame[wavePanel="floating"] {
  background: @RAISED@; color: @TEXT@; border: 1px solid @ACCENT_SECONDARY@;
  border-radius: @RADIUS@px;
}
QFrame[wavePanel="floating"] QLabel { color: @TEXT@; }
QLabel[waveRole="muted"] { color: @MUTED@; }
QLabel[waveRole="accent"] { color: @ACCENT@; font-weight: 600; }
QLabel[waveRole="fixedBar"] {
  min-height: @COMPACT_HEIGHT@px; color: @TEXT@; background: @RAISED@;
  border: 1px solid @BORDER@; border-radius: @RADIUS@px;
  padding: 0 @NOTICE_PADDING@px; font-weight: 600;
}
QLabel[waveState="empty"] { color: @MUTED@; font-style: italic; }
QLabel[waveState="loading"] { color: @INFORMATION@; font-weight: 600; }
QLabel[waveState="success"] { color: @SUCCESS@; font-weight: 600; }
QLabel[waveState="warning"] { color: @WARNING@; font-weight: 600; }
QLabel[waveState="error"] { color: @ERROR@; font-weight: 600; }
QFrame[waveState="empty"] { background: @PANEL@; color: @MUTED@; border: 1px dashed @BORDER@; }
QFrame[waveState="loading"] { background: @INFORMATION_SURFACE@; color: @INFORMATION@; border: 1px solid @INFORMATION@; }
QFrame[waveState="error"] { background: @ERROR_SURFACE@; color: @ERROR@; border: 1px solid @ERROR@; }
QFrame[waveNotice] {
  border-radius: @RADIUS@px; padding: @SPACING@px;
}
QFrame[waveNotice="information"] { background: @INFORMATION_SURFACE@; border: 1px solid @INFORMATION@; }
QFrame[waveNotice="success"] { background: @SUCCESS_SURFACE@; border: 1px solid @SUCCESS@; }
QFrame[waveNotice="warning"] { background: @WARNING_SURFACE@; border: 1px solid @WARNING@; }
QFrame[waveNotice="error"] { background: @ERROR_SURFACE@; border: 1px solid @ERROR@; }
QFrame[waveNotice="information"] QLabel { color: @INFORMATION@; }
QFrame[waveNotice="success"] QLabel { color: @SUCCESS@; }
QFrame[waveNotice="warning"] QLabel { color: @WARNING@; }
QFrame[waveNotice="error"] QLabel { color: @ERROR@; }
QLabel#SimulationStateLabel {
  min-height: @COMPACT_HEIGHT@px; border-radius: @RADIUS@px;
  padding: 0 @NOTICE_PADDING@px; font-weight: 600;
}
QLabel#SimulationStateLabel[simulationState="ready"] { color: @MUTED@; background: @RAISED@; border: 1px solid @BORDER@; }
QLabel#SimulationStateLabel[simulationState="compiling"],
QLabel#SimulationStateLabel[simulationState="running"] { color: @INFORMATION@; background: @INFORMATION_SURFACE@; border: 1px solid @INFORMATION@; }
QLabel#SimulationStateLabel[simulationState="current"] { color: @SUCCESS@; background: @SUCCESS_SURFACE@; border: 1px solid @SUCCESS@; }
QLabel#SimulationStateLabel[simulationState="stale"] { color: @WARNING@; background: @WARNING_SURFACE@; border: 1px solid @WARNING@; }
QLabel#SimulationStateLabel[simulationState="failed"] { color: @ERROR@; background: @ERROR_SURFACE@; border: 1px solid @ERROR@; }
QSplitter::handle { background: @BORDER@; }
QToolTip { color: @TEXT@; background: @RAISED@; border: 1px solid @BORDER@; padding: @SPACING@px; }
)QSS");

    const std::array<std::pair<QString, QColor>, 20> colors{{
        {QStringLiteral("@APPLICATION@"), theme.application},
        {QStringLiteral("@CANVAS@"), theme.canvas},
        {QStringLiteral("@PANEL@"), theme.panel},
        {QStringLiteral("@RAISED@"), theme.raised},
        {QStringLiteral("@BORDER@"), theme.border},
        {QStringLiteral("@TEXT@"), theme.text},
        {QStringLiteral("@MUTED@"), theme.mutedText},
        {QStringLiteral("@ACCENT@"), theme.accent},
        {QStringLiteral("@ACCENT_SECONDARY@"), theme.accentSecondary},
        {QStringLiteral("@SELECTION@"), theme.selection},
        {QStringLiteral("@SELECTION_TEXT@"), theme.selectionText},
        {QStringLiteral("@FOCUS@"), theme.focus},
        {QStringLiteral("@SUCCESS@"), theme.success},
        {QStringLiteral("@WARNING@"), theme.warning},
        {QStringLiteral("@ERROR@"), theme.error},
        {QStringLiteral("@INFORMATION@"), theme.information},
        {QStringLiteral("@SUCCESS_SURFACE@"), theme.successSurface},
        {QStringLiteral("@WARNING_SURFACE@"), theme.warningSurface},
        {QStringLiteral("@ERROR_SURFACE@"), theme.errorSurface},
        {QStringLiteral("@INFORMATION_SURFACE@"), theme.informationSurface},
    }};
    for (const auto& [name, color] : colors) {
        style.replace(name, cssColor(color));
    }
    style.replace(QStringLiteral("@SPACING@"), QString::number(metrics.spacingUnit));
    style.replace(QStringLiteral("@CONTROL_HEIGHT@"), QString::number(metrics.controlHeight));
    style.replace(QStringLiteral("@COMPACT_HEIGHT@"), QString::number(metrics.compactControlHeight));
    style.replace(QStringLiteral("@RADIUS@"), QString::number(metrics.radius));
    style.replace(QStringLiteral("@HEADER_HEIGHT@"), QString::number(metrics.panelHeaderHeight));
    style.replace(QStringLiteral("@NOTICE_PADDING@"), QString::number(metrics.noticePadding));
    style.replace(QStringLiteral("@FOCUS_WIDTH@"), QString::number(metrics.focusRingWidth));
    return style;
}

} // namespace wave
