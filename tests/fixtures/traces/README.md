# Trace fixtures

`wellen-counter.vcd` and `wellen-counter.fst` are the same waveform in two
formats. They are copied from the official Wellen repository's Surfer input
fixtures and are used to verify equivalent VCD and on-demand FST decoding.

Source: https://github.com/ekiwi/wellen

Wellen and these fixtures are distributed under the BSD 3-Clause License.

The complete copyright notice, conditions and disclaimer are retained in
[LICENSE](LICENSE). The local Git blobs match Wellen tag `v0.25.6` exactly:

| Local file | Upstream path | Git blob |
| --- | --- | --- |
| wellen-counter.vcd | wellen/inputs/surfer/counter.vcd | 6d9dd2290bd9108861be6f913e1a69aa2addb49a |
| wellen-counter.fst | wellen/inputs/surfer/counter.vcd.fst | f3f4a2ff5e448d355e9302a9458fb5e14455e0b0 |

Upstream license: https://github.com/ekiwi/wellen/blob/v0.25.6/LICENSE.
The package's Wellen helper notices are maintained separately in
`third_party/wellen-licenses`; these fixture files are not application-owned RTL.
