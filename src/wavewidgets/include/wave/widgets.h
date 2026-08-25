#pragma once

#include "wave/widgets_export.h"

#include <cstddef>

class QWidget;

namespace wave {

inline constexpr int kWaveWidgetsAbiVersion = 1;
inline constexpr const char* kSimulationWorkspaceContract =
    "wave-workbench.simulation-workspace/v1";
inline constexpr const char* kWaveformViewContract =
    "wave-workbench.waveform-view/v1";
inline constexpr const char* kWavePreviewPayloadContract = "wave-preview/v1";

class WaveCanvas;
using StimulusCanvas = WaveCanvas;

} // namespace wave

extern "C" {

WAVEWIDGETS_API int wavewidgets_abi_version() noexcept;

WAVEWIDGETS_API int wavewidgets_create_simulation_workspace_v1(
    const char* projectPathUtf8,
    QWidget* parent,
    QWidget** workspace,
    char* errorUtf8,
    std::size_t errorCapacity) noexcept;

WAVEWIDGETS_API int wavewidgets_create_waveform_view_v1(
    QWidget* parent,
    QWidget** view,
    char* errorUtf8,
    std::size_t errorCapacity) noexcept;

WAVEWIDGETS_API int wavewidgets_set_waveform_preview_v1(
    QWidget* view,
    const char* payloadUtf8,
    std::size_t payloadSize,
    char* errorUtf8,
    std::size_t errorCapacity) noexcept;

}
