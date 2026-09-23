# WaveWorkbench Plan

Current version: `0.12.0`.

## Paired backgrounds — completed locally (2026-09-23)

- [x] All ten signal presets have matching light/dark workspaces: Academic paper-white ruling, Cute pastel dotted grid, Minimal sparse grid, Engineering technical grid, Blueprint graph grid, Retro Terminal phosphor ruling, Oscilloscope graticule, Paper warm ruling, High Contrast strong grid and Faceted Tech segmented grid.
- [x] View → Signal and background styles pairs the project-wide canvas, ruler, signal/Group headers, grid, alternating cycle bands and selection. Per-signal overrides do not recolor the workspace; unconfigured/invalid legacy settings retain the existing theme. The host/application palette remains untouched.
- [x] Bands and grid intersections follow clock phase and active edge, retain time anchoring through zoom/scroll, and coarsen by whole clock cycles. Group collapse/expand preserves background alignment. Numeric/symbolic/undefined Bus fills remain opaque above background details.
- [x] Existing appearance extensions provide persistence and undo/redo without schema, waveform or time-data changes. Ten distinct backgrounds in each light/dark scheme pass text contrast checks (primary ≥7:1, secondary ≥4.5:1) and preset waveform strokes ≥3:1 against the canvas/bands.

Acceptance: Release `build/ela-migration` succeeded; targeted tests **5/5**, full CTest **114/114** (21.45 seconds). Each `wave-ui-followup-*` at 100/125/150/200% passed **13 QtTest slots**, including the new background contrast/persistence/fallback and phase/scroll/isolation regressions. Screenshots cover all 20 preset/scheme combinations. Logs: `background-build.log`, `background-targeted-ctest.log`, `background-full-ctest.log` under the build directory; previews: `ui-followup-previews/<scale>/style-<preset>-<light|dark>.png`.

This extends the preceding uncommitted UI work only. `git diff --check` and the version check passed. No desktop mouse control, commit, push, formal packaging or AppPackage replacement was performed.

Publication follow-up: commit, push and replacement of the formal WaveWorkbench component were authorized on 2026-09-23. The revision-specific receipt in `build/releases/0.12.0-<commit>/RELEASE-RECEIPT.json` records the resulting package validation, remote revision and recoverable AppPackage deployment. Other applications, the shared runtime and the suite manifest remain outside the replacement boundary.

## Recorded UI requests — completed locally (2026-09-23)

- [x] Inline Clock/Bit/Bus creation has no creation caption, apply/cancel explanation or bottom-bar guidance. Parameter inputs, inline errors, Enter/click-to-apply and Esc remain.
- [x] The Bus editor separates context/navigation, value/radix/recent input and actions. Reserved, Don't care, X and Z are direct icon-only actions with accessible names/tooltips; the `0` shortcut is removed. Zero remains available as ordinary input.
- [x] Reserved uses a closed stepped guard, hatching and a lock; Don't care uses a beveled dashed boundary and an eye-off icon. No full-word state labels are needed, and the structures remain distinguishable in monochrome.
- [x] Numeric, symbolic and undefined Bus blocks have opaque fills underneath texture, boundaries and symbols. Grid/clock stripes do not leak through their interiors; custom fill colors receive contrasting value text and semantic icons.
- [x] The top-bar Quick Bit waveform action opens a non-modal binary preview and confirms a draggable fragment, without changing the document during input/preview. A bound target defaults to one clock period per bit; an unbound initial target defaults to 10 ns, editable in the preview. Sync drops snap to the target clock, while unbound drops retain their tick position. The explicitly labeled default replaces only the target range, preserves outside data and uses one undoable command; it does not shift subsequent data. Exact cross-timebase conversion, invalid input, active edits and End bounds are guarded. Maximum fragment length is 4096 bits.
- [x] Standalone typography uses Segoe UI with Microsoft YaHei UI/Noto Sans CJK SC fallbacks: 10.5 pt body, 11.5 pt medium signal names, semibold Group names and 9.5 pt canvas captions/values. Embedded host typography remains isolated. Latin/numeric/CJK coverage and four DPI scales are exercised.
- [x] Inline signal/Group renaming uses the same name-font helper, weight, centered baseline and zero text margins as the resting header; DPI tests compare the editor font explicitly.
- [x] Group headers use a compact flat section, disclosure chevron, member count and hierarchy guides; member rows retain single-line names, consistent indentation and selection/hover surfaces. Group/collapse/reorder/undo and member waveform-fill regressions remain covered.
- [x] Creation and signal properties expose an explicit clock binding or Async/no clock, including the single-clock case. Stable clock IDs, persistence and undo/redo are retained.
- [x] Per-signal Sync/Async glyphs and bound-clock tooltips distinguish timing association without restoring second-line descriptions. The existing toolbar timing toggle still controls grid-versus-arbitrary-tick editing.
- [x] Scenario End is at the far right of the toolbar, retaining validation and undo behavior.
- [x] Icon-only Zoom in, Zoom out and Fit are at the left of the toolbar.
- [x] Go to time uses a compact numeric input plus ps/ns/us/ms dropdown, initially ns. Decimal conversion stays exact; Ctrl+G, range-edge/width editing and invalid-subtick rejection remain covered.
- [x] File/Edit/View share the Ela title layer, above one content-toolbar row. Existing actions/shortcuts are reused; Windows native events are forwarded through Ela's title-bar handling.
- [x] No bottom status/save-state UI remains; title `*` indicates changes. A hidden compatibility message channel supplies inline errors, transient non-modal notices and the external-file conflict bar above the canvas.
- [x] Per-Bit square or trapezoidal edges are appearance-only. Logical intervals, event locations and durations are unchanged.
- [x] Window resize/maximize/restore triggers a 100 ms debounced 0-to-End Fit, retaining cursor, selection and project data.
- [x] Ten usable presets are available: Academic (学术), Cute (可爱), Minimal (极简), Engineering (工程), Blueprint (蓝图), Retro Terminal (复古终端), Oscilloscope (示波器), Paper (纸本), High Contrast (高对比) and Faceted Tech (棱角科技). View → Signal style defaults and per-signal properties configure preset, palette, width, Bit edges, corners and opaque fill. Settings round-trip through the existing `waveWorkbench.signalAppearance` extension without changing the core schema or waveform data.

Acceptance on baseline `f7df273e092a7a6b6b0940766b09f511b01fb7dd`:

- Release build `build/ela-migration` succeeded; complete CTest **114/114** passed (20.69 seconds).
- Added four `wave-ui-followup-*` tests at 100/125/150/200%; each covers 11 QtTest slots, including preview/drop/undo, exact units, resize/maximize/restore, clock binding, style persistence and opaque/monochrome rendering.
- Existing editing, Group, recovery, external reload/file conflict, CLI, embedding and ABI tests remain passing.
- `git diff --check`, the 18-item checklist/version check and historical-plan-tail preservation check passed. Current CMake/plan version remains `0.12.0`.
- Logs: `build/ela-migration/ui-followup-build.log`, `ui-followup-full-ctest.log`, `ui-followup-<scale>.txt`. Screenshots: `build/ela-migration/ui-followup-previews/<scale>/` (workspace, Bus editor, rename, composer, properties and all ten light/dark presets).
- Validation was offscreen. No desktop mouse control or physical-window drag/DWM acceptance was performed. No commit, push, formal package or AppPackage replacement is part of this local implementation round.

## Current follow-up — completed (2026-09-22)

The ten compact-UI requests below and all 39 application action-icon designs are
implemented on baseline `fd9491557a49c350ffb7efe0d09f7dfc20e79ab4`. Icons use
ZeroSlack's rounded monochrome visual language; ZeroSlack itself is unchanged.
Release build `build/ela-migration` succeeded and all 110/110 CTest tests passed
(18.27 seconds), including 100/125/150/200% UI scales, shortcut/text undo routing,
inline-draft protection, the complete user journey and embedding/ABI checks.
Evidence: `build/ela-migration/compact-toolbar-ctest.log` and
`build/ela-migration/ela-previews/<scale>/workspace-{light,dark}.png`.
At implementation acceptance this follow-up was local and uncommitted; no push,
formal package or AppPackage replacement had been performed. Publication and
replacement of the formal WaveWorkbench component were subsequently authorized.
The revision-specific receipt in `build/releases/0.12.0-<commit>/RELEASE-RECEIPT.json`
records the publication and deployment result. Tests were offscreen; no mouse
control was used.

## Current baseline

The ElaWidgetTools migration is implemented, with pinned in-tree source,
system/light/dark appearance, managed tooltips, ordinary text, color pickers,
action icons, non-blocking completion notices and
retained waveform/embedding contracts. Release validation on 2026-09-22 passed
109/109 CTest tests, each repeated twice. Commit, push and Windows x64 portable
packaging are authorized. Native platform acceptance remains pending;
see [migration scope and evidence](docs/ela-migration.md).

Scenario create/duplicate/rename/delete/reorder and semantic light/dark rendering are implemented.
The CLI publishes its current operation set through capabilities and versioned schemas; do not maintain
an independent fixed operation count in this plan.

## Completed usability requests (2026-09-22)

- [x] Remove the top-toolbar `Target: ...` summary, including signal/beat variants. Waveform selection and internal target semantics remain unchanged.
- [x] Make `Zoom in`, `Zoom out` and `Fit scenario` icon-only, retaining tooltips and behavior.
- [x] Use one icon-only Sync/Async toggle with distinct mode icons, checked state and current-mode tooltip.
- [x] Make `Measure` icon-only, retaining mode switching, active state and tooltip.
- [x] Replace `+ CLK`, `+ BIT` and `+ BUS` text with distinct waveform/add icons, retaining accessible names, tooltips and creation behavior.
- [x] Remove `Undo`/`Redo` from Edit without replacement buttons; retain focus-aware `Ctrl+Z`/`Ctrl+Y`.
- [x] Move `Edit relations` to a top-toolbar icon; retain mode state, tooltip and shortcut.
- [x] Replace menu `Find signal...` with a persistent Signals-header input; `Ctrl+F` focuses it, Enter/Down and Shift+Enter/Up navigate matches, Esc returns to the waveform.
- [x] Replace menu `Go to time...` with a persistent top-toolbar input; `Ctrl+G`, time parsing, validation and exact range editing remain available.
- [x] Remove only the `Add group...` menu entry; retain grouping functions and data.

## Remaining usability work

- Long lists still place + CLK / + BIT / + BUS at the content end. A persistent add entry remains a candidate.
- Select the next narrowly scoped usability change before implementation.

## Maintenance boundaries

- Retain stable identities, undo/redo, external revision checks, generation rejection and native ABI compatibility.
- Keep FST and SVA limitations documented in README; do not infer unsupported analog/string trace support.
- Update automation schemas and examples with contract changes and exercise affected CLI/UI tests.
- Release documentation must match the compiled version; publish test results with date and configuration.
