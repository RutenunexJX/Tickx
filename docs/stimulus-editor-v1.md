# Embedded stimulus editor

`wave-workbench.stimulus-editor/v1` is an additive interface in `wavewidgets` ABI 1.
The implementation and canvas remain in Tickx. It requires the same Qt
build and compatible C++ runtime as the host (currently Qt 6.10.2 / MinGW 13.1 on
Windows). Deploy `wavewidgets.dll`, `WaveWorkbenchEla.dll` and their Qt dependencies
beside the host executable. SuiteApp and SuiteRuntime are not required.

`wavewidgets_create_stimulus_editor_v1` takes project JSON bytes, their length,
the host's `QT_VERSION_STR`, a QWidget parent and an output QWidget pointer.
The project must contain exactly one nonempty scenario. Inputs larger than 8 MiB
and mismatched Qt versions are rejected. The parent owns the returned editor;
delete it on the GUI thread before destroying QApplication. Keep the DLL loaded
for the application lifetime. This API must be called on the GUI thread.

`wavewidgets_stimulus_project_v1` commits pending value edits and serializes the
current project. First call with null destination / zero capacity to obtain
`required`, which includes the trailing NUL, then provide that many bytes.
Nonzero return values carry an error message and must not be treated as saved.
The JSON uses the existing [project format](project-format.md).

The editor supports bit clicks, bus values, enum choices, edge dragging, range
editing, undo/redo and zoom. The small surface fixes the input definitions and
automatic clock; the host controls timing, port discovery, project persistence,
HDL generation and simulator execution. Display-only bus text is omitted. No
files, autosave, service registration or simulator process are started by this
component. Cancel by destroying the editor without saving its snapshot.

The dice toolbar button toggles random fill. Turning it on fills an existing
selection, or arms drag selection for subsequent ranges. Each clock beat gets
an independent bit/bus value or a declared enum member; unclocked inputs receive
one value across the range. Clocks are excluded from multi-row fills. A fill is
one undoable command, redo reuses its stored values, and ranges above 4096 values
are rejected before changing the project. Generated values remain ordinary
project segments and survive save/reopen.

The initial viewport shows up to twelve clock cycles. Fit shows the full scenario;
subpixel clocks use a density band labeled with their real period. Double-click
a read-only clock to return to a view with visible edges.

SimDock uses this component through the public C ABI. Its adapter maps supported
HDL input types to lanes and stores the drawing alongside its project settings.
