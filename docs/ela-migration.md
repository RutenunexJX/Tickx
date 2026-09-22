# ElaWidgetTools migration

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
All 39 existing application action-icon sites now use ElaIcon with verified
Unicode code points from the pinned Font Awesome Free font. The icon engine
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
Icon tests check all 29 glyphs, four pixel ratios, normal/disabled/selected/checked
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
