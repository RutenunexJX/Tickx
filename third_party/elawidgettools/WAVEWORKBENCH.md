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
Patch `patches/12-waveworkbench-native-lifetime-english.patch` follows patch 11.
It keeps the ComboBox shared style alive through native popup/view destruction
without recursively repolishing during teardown. The AppBar leaves Qt's mapped
state alone and recalculates the Windows client frame on first show; its window
buttons have stable accessible names. Icon buttons select their icon font while
painting so inherited text styles cannot erase the close glyph. Reachable line
and numeric editor context menus use English captions, including color-picker
children. The application retains its existing immediate ComboBox interaction;
the suite's interruptible-animation migration is separate and must preserve this
lifetime fix. Native regression windows remain hidden and send no desktop input.
Patch `patches/13-waveworkbench-interruptible-capabilities.patch` follows patch 12.
It incrementally integrates the applicable p26/p27 implementation from committed
ZeroSlack `75180fad5e5f5142684cf092649deffe5720994d`, including interruptible
ComboBox, Menu, DrawerArea, TabBar and ordinary-view smooth scrolling. Wave's
exported adapter styles, exact professional scrollbars, per-preview colors,
English captions, reduced motion and popup teardown fix remain intact. Focus
animations and ComboBox transitions reuse owned animation objects; drawer header
keyboard handling and splitter coordination preserve input and model contracts.
The ListView lifetime change comes from xIPs patch
`28-xips-list-style-lifetime.patch`, SHA-256
`5A4E736C208C3584EC637C417534B1C03A583BAEED818C4C0935F7E6A26DF579`.
Patch 13 also guards floating scrollbar origin/area lifetime after origin
replacement, with a targeted regression and the original color-picker tests.
The separate three-file reference fix was supplied to the suite coordinator;
Wave retains its private runtime and does not replace the shared suite DLL.
Patch 13 forward/index and reverse/worktree checks pass on the patch-12 baseline.
Patch `patches/14-waveworkbench-combo-popup-padding.patch` follows patch 13 and
adapts the coordinated ZeroSlack/RegMap source patch
`30-regmap-combo-popup-padding.patch` (ZeroSlack base `8f7abf6`), SHA-256
`e69b815ba035831e2a84484acb0c46f957c83f346fff230a8c2b3034d4a44046`.
It compensates the popup's vertical layout padding after Qt determines geometry,
clamps to the screen's available vertical bounds and settles repeated visible
show requests without accumulating height. Wave's animation reuse and destructor
remain intact; reduced-motion skips animation only after the same geometry fix.
Only ElaComboBox.cpp production code changes. Public signatures and the upstream
MIT/OFL license texts are unchanged. Forward/index and reverse/worktree checks
verify the local adaptation against patch 13.
See `docs/ela-migration.md` for scope,
verification and remaining platform checks. Re-audit these changes before any
dependency update; do not replace the tree with another application's working copy.
