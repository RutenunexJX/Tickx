# Third-party components

Original Tickx application code is Apache-2.0; see LICENSE and NOTICE.
The `wave-wellen-reader` helper retains its existing BSD-3-Clause declaration
and the full text in `tools/wellen_reader/LICENSE`. Dependencies and copied
fixtures retain their own licenses.

| Component | License / provenance | Full notices |
| --- | --- | --- |
| ElaWidgetTools | MIT; Liniyous, pinned upstream `454cac2d57a47d3cc28577dc817793aec1881ca7` | `third_party/elawidgettools/LICENSE`, `UPSTREAM-REVISION.md`, `WAVEWORKBENCH.md`, patches |
| ZeroSlack compatibility changes | Apache-2.0; imported revisions in Ela provenance | `third_party/elawidgettools/ZeroSlack-Apache-2.0.txt` |
| Font Awesome Free Solid 6.7.2 | SIL OFL 1.1; Fonticons, Inc. | `third_party/elawidgettools/Font/FontAwesome-LICENSE.txt` |
| Wellen 0.25.6 | BSD-3-Clause; University of California / Cornell University | `third_party/wellen-licenses/wellen-0.25.6/LICENSE` |
| Locked Rust dependencies | Individual licenses, including Unicode-3.0 in addition to MIT/Apache for unicode-ident | `third_party/wellen-licenses/README.md`, per-package full texts and checksum manifest |
| Rust standard library and bundled components | Individual licenses recorded by the installed Rust toolchain | `third_party/rust-runtime-notices/README.md`, `COPYRIGHT-library.html` and `licenses/` |
| Qt 6.10.2 | LGPLv3 for selected modules; bundled dependencies retain their own terms | `third_party/runtime-licenses/Qt-LGPLv3.txt`, `GCC-COPYING3.txt` |
| MinGW GCC / libstdc++ / libgcc | GPL with applicable GCC Runtime Library Exception | `third_party/runtime-licenses/GCC-*` |
| MinGW-w64 / winpthreads | Their original notices | `third_party/runtime-licenses/MinGW-w64-COPYING.txt`, `winpthreads-COPYING.txt` |

Wellen test fixtures are attributed in `tests/fixtures/traces/README.md`, with
the full BSD text alongside them. The helper's Cargo license field does not
replace dependency notices. The locked inventory includes build-time and
non-Windows dependencies as well; it is not a claim that every entry is linked
into every binary. Regenerate and review notices when Cargo.lock changes.

The optional SuiteApp SDK/runtime is separately supplied and is not licensed
by this repository. See [asset provenance](docs/ASSET-PROVENANCE.md) for the
application icon and screenshot privacy review.

Qt users retain their LGPL rights to modify and relink the libraries. Before
public binary distribution, the publisher must provide corresponding sources
and any required installation/relinking information for the actual Qt build,
and review notices of its bundled dependencies. License texts and source links
alone do not certify complete compliance. See
https://www.qt.io/development/open-source-lgpl-obligations and
[public-release review](docs/PUBLIC-RELEASE-REVIEW.md).
