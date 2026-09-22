# WaveWorkbench Plan

Current version: `0.12.0`.

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
