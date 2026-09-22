# WaveWorkbench Plan

Current version: `0.12.0`.

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

## Remaining usability work

- Long lists still place + CLK / + BIT / + BUS at the content end. A persistent add entry remains a candidate.
- Select the next narrowly scoped usability change before implementation.

## Maintenance boundaries

- Retain stable identities, undo/redo, external revision checks, generation rejection and native ABI compatibility.
- Keep FST and SVA limitations documented in README; do not infer unsupported analog/string trace support.
- Update automation schemas and examples with contract changes and exercise affected CLI/UI tests.
- Release documentation must match the compiled version; publish test results with date and configuration.
