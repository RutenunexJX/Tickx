# Complete embedded waveform editor

`wave-workbench.wave-editor/v1` adds seven C endpoints to `wavewidgets` ABI 1.
It reuses Tickx's Project, CommandStack, WaveCanvas, signal dialogs and styles.
The [fixed-input stimulus editor](stimulus-editor-v1.md) retains its contract.
No MainWindow, file watcher, autosave, simulator or runtime service is created.

## Deployment and lifetime

Use `wave/widgets.h` and resolve all seven symbols before enabling the feature.
The host must use the same Qt build (6.10.2) and compatible Windows x64 MinGW
13.1 runtime. Deploy the reviewed wavewidgets.dll and WaveWorkbenchEla.dll with
their notices. Qt Core/Gui/Widgets/Network/Svg and GCC runtimes come from the
matching host deployment. Ela fonts/resources are compiled into its DLL.

All calls run on the Qt GUI thread. The QWidget parent owns the component;
delete it before QApplication and keep the DLL loaded for its whole lifetime.
Reparent the same widget on moves; create independent instances for copies.

Functions return zero on success, otherwise a UTF-8 error. JSON inputs are
limited to 8 MiB and one positive-duration scenario, which may have no lanes.

## API

| Endpoint | Purpose |
| --- | --- |
| `wavewidgets_create_wave_editor_v1` | Create from project JSON, or null data/zero size for a blank 200 ns project; pass QT_VERSION_STR, parent and output widget. |
| `wavewidgets_wave_editor_project_v1` | Commit valid inline input and serialize the existing project format. |
| `wavewidgets_wave_editor_state_v1` | Read mode, history availability and view without committing input. |
| `wavewidgets_configure_wave_editor_v1` | Apply partial configuration JSON. |
| `wavewidgets_sync_wave_editor_v1` | Apply a same-identity project snapshot in place. |
| `wavewidgets_wave_editor_action_v1` | commit, undo, redo, fit, zoom-in, zoom-out, add-clock, add-bit, add-bus. |
| `wavewidgets_set_wave_editor_callback_v1` | Register/clear a callback and opaque context. |

Getters use null destination/zero capacity to query `required`, including NUL.
Allocate that many bytes and read again. A failed commit/snapshot is not saved.

## Configuration and history

Editors start in Preview. Configure Edit explicitly:

~~~json
{"mode":"edit","readOnly":false,"historyOwner":"host","canUndo":true,"canRedo":false,"theme":"system"}
~~~

`mode` is edit/preview; `readOnly` is the host document lock. Preview and locked
documents suppress content edits while retaining cursor navigation, zoom and
pan on the same canvas. Do not disable the widget or make it mouse transparent.
The host owns the Edit/Preview control. No new component shortcuts are bound.

`theme` is system/light/dark and affects only the component. The host can set
the QWidget font. Lane/background styles remain project extensions; outer
block presets must not tint the waveform. Size hints follow lane heights up to
a bounded height; the host chooses final height and the canvas scrolls.

Choose the history owner before editing:

- `component` (default): Tickx owns user undo. A different sync is one undoable
  external edit. The host must not also journal each waveform transaction.
- `host`: the document journal owns user undo. Each changed callback means one
  completed transaction; read and journal its snapshot once. Undo/Redo only
  emits undoRequested/redoRequested. The host performs document undo, syncs
  the restored snapshot and configures canUndo/canRedo. The internal command
  cache executes/cancels local edits and is cleared on external restores.

ZeroSlack uses host history with DocumentBlockModel. Multiple presentations
have independent components synchronized by the document model. Do not share
mutable Project pointers or recreate components on model echoes.

Inline signal creation is one transaction; Escape cancels without removing
the existing redo branch. Property dialogs and validation match standalone.
Signal headers support reorder, rename and deletion.

Sync requires matching project/scenario IDs and timebase. Equal projects are
no-ops. Different content updates the same objects and preserves compatible
selections, cursor, scroll and time range. Sync emits no changed callback.
Invalid inline input and active gestures/dialogs reject sync. If committing a
valid local draft conflicts with an incoming snapshot, publish/persist the
local edit and reconcile before retrying; never discard it silently.

## Events and view

The callback type is `void (*)(void*, const char*, std::size_t)`. Bytes live
only for the callback. Synchronous getters are allowed; queue mutations, sync
and destruction until after it returns. Clear it before destroying context.

Event JSON contains event, decimal-string revision, canUndo and canRedo:

- changed: completed content transaction.
- undoRequested / redoRequested: host-history requests.
- viewChanged: includes view, and does not create content history.
- preferredHeightChanged: includes preferredHeight.

State includes contract, mode, readOnly, effective editable, historyOwner,
revision, canUndo/canRedo, preferredHeight, actual devicePixelRatio, and view:

~~~json
{"view":{"cursorTick":"12000","startTick":"0","endTick":"40000","selectedLaneIds":["lane-id"],"activeLaneId":"lane-id","verticalScroll":0,"headerWidth":176}}
~~~

Configure accepts a partial view. Times are nonnegative decimal tick strings;
the visible range must be inside the scenario. Persist view separately from
content if desired. Memory undo history is not serialized. Dynamic properties
wavewidgets.contract, wavewidgets.stateRevision and wavewidgets.canUndo/canRedo
are observability metadata, not a second document store.

## Host transaction order

1. Create/configure, then register the callback.
2. On changed, read the project and persist/journal it exactly once.
3. Before leaving Edit, saving or moving, commit, snapshot, then complete host
   persistence. Only after success change mode or structure.
4. On failure keep the location, Edit state and inline draft; report the error.
5. Move/reparent the existing widget; copy/reopen from the saved project and
   configure any separately stored view.

The host owns cross-document assets, save failure recovery, block identity,
drag cancellation and paired document undo.
