# WaveWorkbench Goal

Current version: `0.12.0`.

Current UI migration status (2026-09-22): Ela general controls, managed tooltips,
ordinary text, color pickers, action icons and non-blocking completion notices
are implemented and approved for Windows x64 portable packaging. Release offscreen
validation passed 109/109 tests, each repeated twice. Native platform acceptance
remains separate from this release; see [migration evidence](docs/ela-migration.md).

Provide an independent desktop waveform design, stimulus, simulation and verification tool with a stable
automation CLI and embeddable waveform components. Project data remains the single model; GUI, CLI and
embedded views share parsing, validation and editing behavior.

Scenario lifecycle, simulation results, comparisons, source navigation, external reload and semantic themes
are implemented. New usability work should improve one concrete workflow at a time.

See [README](README.md), [interaction](docs/interaction.md) and [integration contracts](docs/integration-contracts.md).
