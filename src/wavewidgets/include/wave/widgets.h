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
inline constexpr const char* kStimulusEditorContract = "wave-workbench.stimulus-editor/v1";

class WaveCanvas;
using StimulusCanvas = WaveCanvas;

} // namespace wave

extern "C" {

WAVEWIDGETS_API int wavewidgets_abi_version() noexcept;

// In-memory, single-scenario editor. The host owns persistence and simulation.
// No runtime service, file monitoring, autosave, or simulator is started.
WAVEWIDGETS_API int wavewidgets_create_stimulus_editor_v1(
    const char* projectJson, std::size_t projectSize, const char* hostQtVersion,
    QWidget* parent, QWidget** editor, char* error, std::size_t errorCapacity) noexcept;

// Query with destination=nullptr/capacity=0 first. required includes the NUL.
WAVEWIDGETS_API int wavewidgets_stimulus_project_v1(
    QWidget* editor, char* destination, std::size_t capacity,
    std::size_t* required, char* error, std::size_t errorCapacity) noexcept;

// Apply a new timing/duration snapshot as one undoable edit. Project, scenario
// and lane identities must match. Keeps the canvas, viewport and prior history.
// Unknown JSON extensions round-trip with the edit (e.g. host timing settings).
WAVEWIDGETS_API int wavewidgets_update_stimulus_editor_v1(
    QWidget* editor, const char* projectJson, std::size_t projectSize,
    char* error, std::size_t errorCapacity) noexcept;

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
