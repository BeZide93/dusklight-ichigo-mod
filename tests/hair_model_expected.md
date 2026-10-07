# Hair model regression baseline

Run `python3 tests/hair_model_test.py` from a checkout (Python standard library only).

The five optimized heads were uploaded in commits `ec47d02` through `0476733`.
Their SHA-256 hashes, GX triangle counts (including degenerate strip connectors),
vertex submission counts and packet counts are pinned in `hair_model_expected.json`.

Geometry expectations were independently calculated from the shipped files at
`72aebbc8c1c8fce4ff4e4dd9daf213f290c51b3a`, before these lossless optimizations,
and compared with all five uploaded files. They cover every visible face of every
shape, including fine hair patches and accessories, plus skinning transitions.
Only triangles with repeated complete resolved vertex records are ignored.

Face records resolve array indices to exact position, normal and UV bytes, and
packet-local matrix slots to global DRW1 indices. Cyclic rotations are normalized;
winding and multiplicity are retained. Hashes are per shape because repacking
changes packet boundaries and local palettes. Vertex formats and fractions are
also pinned. Thus compaction/reordering does not change the geometry baseline.

EVP1, DRW1, JNT1, MAT3 and TEX1 must remain byte-identical to that earlier commit.
INF1 must remain identical except for its updated packet and position counts.
VTX1 is checked through resolved face values rather than its old storage layout.
The validator supports GX triangle lists, strips and fans and checks display-list,
vertex-array and matrix-palette bounds, including the ten-matrix hardware limit.

Do not regenerate geometry expectations from a modified asset merely to make CI
pass. Intentional visual changes need a separately reviewed geometry baseline.
