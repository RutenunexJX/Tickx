# WaveWorkbench integration provenance

- Upstream: https://github.com/Liniyous/ElaWidgetTools
- Pinned upstream revision: `454cac2d57a47d3cc28577dc817793aec1881ca7`.
- Import source: committed ZeroSlack revision
  `261c90e6d96d9738f7eed2eab27499a25768778d`, subtree
  `0ef278b2e9463c9c72bcbd830fb5d4bdde4ba18a`.
- Imported patches 01–07 and `UPSTREAM-REVISION.md` describe the baseline.
  No uncommitted ZeroSlack source or external build directory is required.
- Font Awesome Free Solid 6.7.2 SHA-256:
  `AF19D135D3A935B3EBFBD80320716FFE1202052C5F68DC2C5F1ABC57005AC605`.
  `Font/FontAwesome-LICENSE.txt` includes the SIL Open Font License.
- Ela source is MIT licensed; see `LICENSE`. Sample image assets are not included.
- Qt is pinned to 6.10.2 because the upstream library uses Qt private headers.
  The private runtime is named `WaveWorkbenchEla` to avoid colliding with a host's
  independently built `ElaWidgetTools` library.

WaveWorkbench changes are maintained in this vendored tree: resource-only
initialization for embedded clients; explicit style ownership/detachment; exported
style classes for contract-preserving Qt tree/table/tab and standard-dialog
adapters; immediate menu/popup lifecycle. Patch
`patches/08-waveworkbench-control-contracts.patch` reproduces the 26 changed
upstream source/header files on top of the import; `git apply --check` is part of
the migration audit. Patch context-prefix spaces are intentionally preserved.
Patch `patches/09-waveworkbench-scroll-contracts.patch` follows patch 08 and adds
per-widget scrollbar colors, opt-in Qt track-click semantics and list item font
painting from the Qt style option. The application
adapter retains Qt wheel handling and immediate values; no shared theme mutation
is needed for independently themed waveform previews.
Patch `patches/10-waveworkbench-feedback-contracts.patch` follows patch 09 and
adds opt-in Qt font/palette inheritance for ordinary ElaText labels, plus a
native-frame ElaColorDialog with accepted/rejected results, scrollable content,
stable English-translatable captions and safe slider-style teardown. Existing
frameless callers retain their constructor behavior. The application owns tooltip
timers and geometry; it does not enable Ela's parent Enter/Leave timer filter.
Patch `patches/11-waveworkbench-notification-contracts.patch` follows patch 10 and
adds a hidden, parent-owned ElaMessageBar surface. Its application adapter owns
content, bounds, deduplication and one timer per workspace; this mode does not
enter the upstream animation/active-message map or mutate the host font/theme.
The icon adapter renders with ElaIcon and the pinned Free font's Unicode values,
not the upstream custom-font PUA assignments; glyph coverage is checked in tests.
See `docs/ela-migration.md` for scope,
verification and remaining platform checks. Re-audit these changes before any
dependency update; do not replace the tree with another application's working copy.
