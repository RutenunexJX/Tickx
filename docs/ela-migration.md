# ElaWidgetTools migration

## Popup padding follow-up (2026-09-24)

Wave patch 14 adapts the verified ZeroSlack/RegMap patch 30, source SHA-256
`e69b815ba035831e2a84484acb0c46f957c83f346fff230a8c2b3034d4a44046`.
The production change is confined to ElaComboBox.cpp: add vertical layout padding
after Qt computes popup geometry, clamp to available screen bounds and settle
repeated visible show requests without adding height again. Reduced motion uses
the same geometry correction before skipping animation; reused animation objects,
destruction ordering and public signatures remain unchanged.

The added regression first failed for all 12 combinations (1/3/5 rows, normal or
reduced motion, top or bottom placement), including a 35-pixel row in a 29-pixel
viewport. After correction, all rows fit, first/last choices work, and four reopen
cycles plus five repeated visible show calls per cycle retain stable geometry.
Incremental Release build and **10/10** targeted CTest tests passed (52.15 s):
control/popup offscreen and hidden Windows suites at scale factors 1/2, plus the
existing style, color-dialog and waveform UI suites. Capability suites now pass
23 offscreen / 22 hidden-native QtTest cases at each scale. No existing failure
case was deleted, no unrelated full audit was repeated, and no desktop input was
used. Logs: `popup-padding-before-native.txt`, `popup-padding-release-build.log`
and `popup-padding-targeted-ctest.log` under `build/ela-migration`.

Version stays 0.12.0. Earlier commits/tags/candidates remain unchanged; the new
StageOnly candidate records patch provenance in build-info.json and complete
per-file checksums. Formal directory replacement remains coordinator-owned.

## Capability acceptance (2026-09-24)

This is an incremental source integration from ZeroSlack
`75180fad5e5f5142684cf092649deffe5720994d` (p26/p27), plus xIPs p28 ListView
style-lifetime protection. Wave vendor patch 13 preserves patch 12's native
teardown/English fixes, exported adapter styles, independent preview colors and
Qt 6.10.2 private-header pin. No shared DLL or other repository is overwritten.

| Reachable surface | Current behavior and boundaries |
| --- | --- |
| ComboBox | Reusable, owned popup height/position and indicator animations; keyboard, mouse, hide, resize and destruction interrupt safely; selection remains Qt-owned. |
| LineEdit, SpinBox, DoubleSpinBox | One owned/reused focus animation per field, reduced-motion support; numeric width includes the actual editor, suffix and step controls. |
| Menus | Interruptible 160 ms native popup animation; keyboard/mouse/wheel and action changes settle immediately; QAction identity, submenus, checks and disabled states retained. QWidgetAction editors remain live. Snapshot cap: 8 MiB. |
| Ordinary tree/table/completion views | 160 ms smooth wheel movement; precision pixel deltas remain immediate. Navigation/selection/programmatic value changes interrupt pending movement. Tree expansion can settle before input. Models and delegates remain Qt-owned. |
| Professional canvases and embedded previews | Immediate Qt scrolling and exact timeline semantics retained, including per-preview theme. No global smooth-scroll substitution. |
| Comparison/check/batch tabs | Actual ElaTabBar with Qt geometry and smooth overflow; fixed review pages retain Qt ownership, with no document drag/close/reorder affordances. |
| Existing simulation sections | Actual ElaDrawerArea for Stimulus/Expected, Actual and Review; interruptible body-only transition, keyboard/focus support, saved collapsed and splitter state. Snapshot cap: 32 MiB, released on settle. |
| Dialogs, tooltips and notifications | Ela controls inside Qt dialog role maps; existing managed, scoped tooltip/notification lifetime remains. No global animation queue, generic window conversion, new docks or restored status surface. |

An original color-picker screenshot regression exposed an overlay use-after-free:
`QAbstractSlider::singleStep -> ElaScrollBarPrivate::_handleScrollBarGeometry`.
The overlay now uses guarded origin/area pointers and removes its event filter,
stops movement and hides when the origin is destroyed. Color adaptation touches
only QScrollArea form bodies, not ComboBox-owned popup views. The replacement,
deferred destruction, resize and grab regression is retained alongside the
original picker tests. No failed test was removed or weakened.

Release `build/ela-migration` and full CTest **122/122** passed (79.08 seconds).
Four color-picker/DPI suites also passed three successive executions each.
Capability suites cover 1x/2x offscreen and hidden Windows windows; the existing
native style suites cover scale factors 1/1.25/1.5/2. A native scale factor
multiplies the display's existing DPR and is not an absolute physical DPI claim.
No desktop input, physical dragging, mixed-monitor or compositor acceptance was
performed. Existing editing, persistence, conflict, CLI, embedding and ABI tests
remain passing.

### Measured cost and allocation bounds

The reproducible browsing fixture uses a 1200x800 window, 2112 tree rows, 2048
table cells and 12 input iterations. The baseline is commit `88b3942` with
immediate interaction; the final sample is from the full acceptance run.
Times are milliseconds; CPU is process kernel+user time, not wall-clock time.

| Offscreen DPR | Input dispatch median/max, before → after | Paints, before → after | CPU, before → after | Elapsed, before → after |
| --- | --- | --- | --- | --- |
| 1 | 0.8848/2.1176 → 0.6567/1.2218 | 158 → 767 | 234.375 → 1687.5 | 2797.34 → 2827.93 |
| 2 | 1.2024/2.0679 → 0.6237/1.1509 | 158 → 817 | 156.25 → 546.875 | 2798.50 → 2787.40 |

Layout requests stayed at 55. New continuous animation necessarily paints more
than the immediate baseline; these single-run, parallel-machine samples do not
demonstrate an overall CPU improvement or physical display frame rate.
Reusable popup/focus animation counts remain constant under repeated reversals.
Body-only drawer snapshots peaked at 1,716,480/6,865,920 bytes (DPR 1/2), with
preparation maxima 2.8569/4.2165 ms and total dispatch maxima 15.8568/16.5241 ms.
This bounds memory and avoids snapshotting the whole workspace on each frame.

Logs: `capability-final-ctest.log`, `capability-color-repeat-ctest.log`,
`capability-scroll-lifetime-ctest.log`, `color-regression-nodebugheap-gdb.log`,
`ui-capabilities-{1,2}.txt` and `ui-capabilities-before-{1,2}.txt`, all under
`build/ela-migration`. StageOnly packaging includes build-info.json, complete
per-file SHA256SUMS.txt, MIT/OFL notices and source patches. It creates neither
ZIP nor backup and does not replace the suite's formal directory or manifests.

## Current native follow-up (2026-09-24)

The inventory below describes the original migration baseline. Subsequent compact
UI work introduced an ElaAppBar title/menu layer, hid the bottom status surface
and moved Scenario End into the single toolbar. The 2026-09-24 follow-up fixes
native ComboBox destruction, first-show client-frame setup and the close glyph's
font; the unit selector now measures its text instead of forcing a narrow width.
Standalone file dialogs deliberately use Qt's English captions, while embedded
initialization leaves the host's dialog policy unchanged. User-supplied text is
not translated. Patch 12 records the vendor changes.

Release validation passed **118/118** CTest tests in 64.46 seconds, including four
hidden Windows-platform suites at scale factors 1/1.25/1.5/2. All ten style presets
are accepted/cancelled through real popup and button lifecycles; initial client
geometry, close-glyph pixels, English built-in captions and cancelled dirty close
are checked. Logs are under `build/ela-migration/style-startup-verified-ctest.log`
and `style-crash-*.log`. These checks do not constitute physical desktop dragging,
mixed-monitor DPI or compositor acceptance. No desktop input was injected.

The suite capability follow-up uses the committed ZeroSlack reference
`3f1c4afab0d0423c04c3c3af4bb3c3d9aabe5cde` incrementally, not a replacement vendor
tree. Professional timeline/paint/model/embedding contracts remain protected;
removed docks and status UI must not be reintroduced. Release staging for that
follow-up will omit ZIP/backup output, and the coordinator owns any later AppSuite
replacement and shared manifest changes.

## Original migration baseline

Implementation baseline: `e45660dd37024d0b1be3bebf2d1cd936602fc674` (`main`, clean
before work). Application version remains `0.12.0`. All changes are confined to
WaveWorkbench. Following implementation validation, commit, push and formal
Windows x64 portable packaging were authorized on 2026-09-22. Release artifacts
use a commit-specific directory under `build/releases`; checksums and a release
receipt accompany the ZIP. No shared-runtime installation is replaced.

## Reachable surface inventory

| Surface | Implementation | Preserved contracts |
| --- | --- | --- |
| File/Edit/View and canvas context menus | ElaMenuBar / ElaMenu | QAction identity, shortcuts, checked/disabled state, submenu ownership |
| Waveform, simulation, comparison, checks and batch toolbars | ElaToolBar, ElaToolButton and Ela-styled Qt action buttons | widgetForAction, toggles, popup modes, overflow and enablement |
| Status and external-file conflict actions | ElaStatusBar / ElaPushButton | non-modal Reload/Keep/Save As, save state, editor focus |
| Add CLK/BIT/BUS, hidden-lane restore, quick setup, rename, duration, bus preset and range controls | ElaToolButton / ElaLineEdit / ElaComboBox / ElaSpinBox / ElaCheckBox | stable object names, validation properties, editing and cancellation signals |
| Lane, clock, bus, group, export, relation and check parameter dialogs | Ela inputs and buttons; explicit Qt standard-dialog adapters | standard button roles, default/escape behavior, validation, focus, modal result |
| Scenario-name and choice dialogs, save/conflict/error messages | QInputDialog / QMessageBox with explicit Ela style adapters | Qt's internal standard-button maps and test/accessibility identities |
| Actual signal hierarchy and search | ElaLineEdit, ElaTreeViewStyle and ElaScrollBar around QTreeWidget | hierarchy model, check state, signal identity, filtering, reveal/navigation |
| Bus/range value completion popups | ElaListView under the existing QCompleter | original models, case-insensitive matching, keyboard acceptance/cancellation and popup ownership |
| Stimulus, trace and embeddable preview scrollbars | ElaScrollBar with Qt wheel/track semantics | immediate values, fractional deltas, page steps, RTL, range mapping and per-preview theme |
| Canvas and owned-widget hover help | managed ElaToolTip with document-backed ElaText | dynamic text/position, bounded screen geometry, interruptible lifetime, no focus activation or host-wide event filter |
| Ordinary titles, summaries, explanations and form captions | ElaText with explicit Qt inheritance mode | parent font/palette, rich text, wrapping, mnemonic buddies; semantic labels stay QLabel |
| Lane properties and quick Bit/Bus color fields | ElaColorDialog beside the original text editor | validation, cancel/escape, alpha preservation, one-step text undo, focus restoration and parent-dialog transaction |
| Existing action, toolbar and range-editor icons | ElaIcon-backed per-owner QIcon engine | live palette, disabled/selected state, light/dark themes, fractional DPI and QAction identity |
| Explicit save, successful exports and simulation/batch completion | managed ElaMessageBar | non-blocking, no focus activation or animation, one replaceable notice per workspace; status and diagnostic details retained |
| Comparison/check/batch tables and result tabs | ElaTableViewStyle / ElaTabBarStyle around Qt containers | model foreground/background/font roles, selection, delegates, tab/page lifetime |
| Appearance | View > Appearance: system/light/dark | persisted standalone preference; system updates; no host application palette/font/QSS mutation |

`createDocks()` remains an uncalled legacy definition. Its generic constructors
also use the adapters, but no Project/Inspector/Scenario docks were re-enabled.

`QMainWindow`, native window frames, standard dialog shells, native file pickers,
splitters, layout containers, semantic labels and standard-dialog internal labels
remain Qt infrastructure. Ela's frameless
window/app-bar/Mica behavior is deliberately not introduced. Standard-dialog
buttons retain Qt's private role maps: their surface uses Ela theme tokens while
Qt draws the mnemonic/icon/default/escape semantics. This is not a global widget
replacement filter.

## Protected implementation

`WaveCanvas` changes are limited to generic control/scrollbar construction,
action icons and popup/dialog entry points. `TraceCanvas` and `WaveformView` install the same
scrollbar adapter before their value-change connections; the preview also
repaints scrollbars on theme changes. Their paint, interaction, timeline
arithmetic and model operations remain intact. `TimelineViewport`, CLI, model,
schemas and public `wavewidgets` v1 headers are unchanged.

Generic control border/background QSS has been removed. Remaining rules describe
semantic panels, notices and labels; scoped font rules retain host typography.
Ela controls explicitly render error/warning/focus state without competing QSS.
Menus and combo popups have immediate, interruptible Qt lifecycles. Value-scroll
animation is bypassed using Qt wheel handling (Ela's range-animation flag alone
does not disable wheel animation); scrollbar expansion honors reduced motion.
Completion lists detach the owned style before destruction. Scrollbar paint
colors follow each view, including independently themed embedded previews,
without changing the shared Ela theme or application palette.
Completion popups track their editor's font, palette and layout direction so
larger text retains matching row geometry without a global event filter.
Ordinary ElaText labels opt into inheritance before construction applies any
vendor font or palette override. Error, warning, save-state and conflict labels
retain their semantic Qt styling. Form captions retain buddy relationships.
Tooltip filters are scoped to owned widget trees, exclude native file-dialog
subtrees, and use one restartable lifetime timer instead of delayed Enter/Leave
callbacks. Owner destruction, region exit and input dismiss the tooltip.
The native-frame color picker keeps its action buttons outside a scrollable
content area for small screens. Only acceptance edits the existing color text;
the parent dialog still owns validation and applying changes to the model.
The subsequent compact-toolbar update replaces application action glyphs with
39 palette-driven vector designs: a 24-unit grid and rounded 1.75-unit strokes,
following ZeroSlack's rounded monochrome visual language. The icon engine
resolves its owner's palette on each render and renders at the requested device
pixel ratio; it does not cache stale light/dark bitmaps. Application branding and
Qt standard-dialog internal icons retain their original roles.
Notifications are connected only to six low-frequency completion sites, not the
high-frequency status stream or autosave. Each workspace owns one parented bar
and one restartable timer. Identical visible messages are deduplicated without
extending their lifetime; newer messages replace older ones. Close, timeout or
owner hide/destroy dismisses the bar without activating or focusing a window.
Long messages are elided visually with full accessible text and escaped tooltips.
Export diagnostics, failures and external-file conflict interactions keep their
existing dialogs or controls; simulation and batch details remain available.

The shared-library path initializes only Ela resources, not its application-wide
font or native-window hooks. Embedded workspaces follow host palette and font
changes without changing the host. Ela's theme singleton is private to the
WaveWorkbench runtime; simultaneously assigning different color schemes to
multiple embedded workspaces in the same process is not supported.

## Dependency and deployment

The complete source snapshot is under `third_party/elawidgettools`, pinned to
upstream `454cac2d57a47d3cc28577dc817793aec1881ca7`. The import came from the
committed ZeroSlack snapshot recorded in
[`WAVEWORKBENCH.md`](../third_party/elawidgettools/WAVEWORKBENCH.md), never from its
dirty working tree. No other checkout, shared build or absolute source path is
needed to rebuild it. MIT and Font Awesome/SIL OFL notices are retained.

Qt is pinned to **6.10.2**, including WidgetsPrivate. The library is linked
privately as `WaveWorkbenchEla.dll`, avoiding collision with a host's independent
`ElaWidgetTools.dll`. Deploy the new runtime beside `wavewidgets.dll`; do not
replace only one DLL in an existing host installation. Portable install rules
include the runtime and licenses. The install contract tests only stage artifacts
under the isolated build directory, not a formal package or AppPackage tree.

## Validation

Validation uses `build/ela-migration`, Release, Qt 6.10.2 / MinGW 13.1, Wellen and
the existing SuiteApp integration. GUI tests run offscreen with isolated settings,
recovery data and fonts. No mouse or foreground-window operation is used.

The existing 105 tests remain registered. Four additional scale tests (1, 1.25,
1.5, 2) cover actual Ela instances, keyboard editing, undo/redo, checkbox states,
popup close/reopen/destruction, QAction state, standard dialog roles, error/focus
pixels, model role/selection colors, tab ownership, host isolation and appearance
switches. Painter warnings are failures. A focused embedded-window teardown test
also protects against application focus callbacks accessing destroyed actions.
The completion/scrollbar follow-up extends those four scale tests with actual
popup class checks, case-insensitive matching, keyboard confirmation/cancellation,
model replacement, enlarged editor fonts, visible-popup destruction, native Qt
wheel/page/RTL behavior, drag completion and independent preview scrollbar themes.
The feedback follow-up covers inherited and enlarged label fonts, semantic label
retention, tooltip focus/lifetime/owner destruction, negative-screen coordinates,
long-text bounds, dynamic child registration and unmanaged host tooltip isolation.
Color tests exercise real keyboard hex entry, acceptance/escape, invalid drafts,
8-bit RGBA preservation, text undo/redo and pinned actions on small screens.
Both themed Lane properties previews verify the real Ela button and caption.
Icon tests check all 39 vector designs, four pixel ratios, normal/disabled/selected/checked
colors, live palette updates, owner destruction and actual application actions.
Notification tests check actual ElaMessageBar ownership, focus and status
preservation, deduplication, replacement/timeout/close, long plain text, larger
fonts, small bounds, RTL, theme changes, independent owners, destruction and an
explicit project-save integration path.

On 2026-09-22, the final incremental Release build succeeded and all **109/109
CTest tests passed**, each repeated twice with six-way parallel execution
(218 passing executions, 22.64 seconds).
Logs are `build/ela-migration/build-ela-icons-final.log`,
`build/ela-migration/ctest-ela-icons-final.log` and
`build/ela-migration/Testing/Temporary/LastTest.log`. This includes all 105
existing tests, four new scale tests, the file-conflict/ABI/SuiteApp checks and
the isolated portable-install contract. Preview images
are generated by the scale tests in `build/ela-migration/ela-previews/<scale>/`:
`workspace-{light,dark}.png`, `parameters-{light,dark}.png`, and
`menu-{light,dark}.png`, `file-menu-{light,dark}.png` and
`notification-{light,dark}.png`, plus `completion-popup.png` with enlarged text and selection,
`tooltip.png`, `color-picker.png` and `lane-properties-{light,dark}.png`.

Initial six-way and four-way full runs each exposed a file-conflict smoke timeout:
its fixture watched the shared build output directory, whose other tests' writes
kept restarting the directory debounce. Three isolated reruns passed. The CTest
fixture now uses `test-projects/file-conflict/project.wave.json`; the watcher code,
3-second wait and all assertions are unchanged. The final repeated parallel run
above passed with this isolation. Earlier evidence remains in
`ctest-ela-feedback-parallel6.log`, `ctest-ela-feedback-parallel4.log` and
`ctest-ela-feedback-file-conflict.log` under the same build directory. Continuous
unrelated writes in a real project's directory can still postpone the existing
debounce; changing that product behavior is outside this UI migration.

The first full icon/notification run had one autosave smoke timeout. Three
immediate isolated repeats then passed (5.58 seconds each); the cause of that
timeout is unconfirmed. No autosave implementation, timeout or assertion was
changed. Evidence is retained in `ctest-ela-icons-first-full.log` and
`ctest-ela-icons-autosave.log` under the same build directory.

The local change boundary is build configuration, eleven application source/header
files (including the new `ui_controls`, `ui_feedback` and `ui_icons` adapters), five test files,
six documentation files, and 430 files in the pinned vendor tree. No public v1 headers, model/schema,
CLI implementation or other repository is changed. Tracked and new source files
pass whitespace checks; vendor patch 08 passes `git apply --check` against the
untouched imported snapshot, patch 09 passes against its post-08 source and
patch 10 passes against its post-09 source and patch 11 against its post-10 source.
At the end of pre-release validation, the index was empty and HEAD was the
implementation baseline above, one commit ahead of `origin/main`
`305d3e015651f8c86021da7b7d7cb633485f56d9`. The release receipt records the subsequent
source commit, remote verification, archive checksum and packaged-runtime checks.

Not verified in this run: native Windows mouse/IME/accessibility interaction,
multi-monitor DPI transitions, native file-picker integration, a new real
ZeroSlack host process, or a real Verilator run (not available in this configured
environment). Offscreen checks do not replace those platform acceptance checks.
Upstream Ela C++20 deprecation/pedantic warnings remain visible in build logs.

### Compact UI and rounded-icon follow-up (2026-09-22)

On baseline `fd9491557a49c350ffb7efe0d09f7dfc20e79ab4`, the ten recorded menu/toolbar
requests are complete. Application icons now use the 39 rounded vector designs
described above. Search is fixed in the Signals header and time entry is always
available on the top toolbar. Shortcuts, text undo, mode state, tooltips, inline
validation and exact range editing remain covered. Passive focus transfers while
an editor closes cannot commit an unrelated pending End draft.

The incremental Release build succeeded; the full CTest suite passed **110/110**
with four-way parallel execution in 18.27 seconds. This includes the existing
canvas edit regression test, all four UI scale tests and the complete user-journey
smoke. Log: `build/ela-migration/compact-toolbar-ctest.log`. Updated standalone
light/dark previews use the real appearance actions and are in the scale-specific
`ela-previews` directories described above.

This follow-up changes only application UI sources, UI regression assertions and
three documentation files. It does not change the model/schema, public v1 ABI,
CLI implementation or other repositories. At implementation acceptance it was
uncommitted and unpushed, without a formal package or AppPackage replacement.
Subsequent publication and component replacement are authorized separately;
the revision-specific release receipt records their result. Native platform
acceptance remains outside the offscreen validation evidence.
