# Reduced Ichigo hair model

The normal Kmdl `al_head.bmd` uses fewer triangles and vertex records to reduce
pressure on Dusklight's per-frame geometry buffers when Link and Dark Link both
use Ichigo's appearance. This does not guarantee that every mod combination fits
within the renderer's fixed limits.

| Stored drawing data | Before | After | Reduction |
|---|---:|---:|---:|
| Triangles expanded from GX strips | 38,927 | 27,169 | 30.2% |
| Submitted vertex records | 52,049 | 37,589 | 27.8% |
| Raw vertex-stream bytes per full draw | 468,243 | 338,103 | 27.8% |
| Triangle index bytes per full draw | 233,562 | 163,014 | 30.2% |
| BMD file bytes | 1,749,280 | 1,615,072 | 7.7% |

Counts include degenerate triangles in strips because the renderer expands them
into indices too. Actual frame savings depend on which shapes and passes render;
these are file-derived drawing counts, not measured frame-buffer occupancy.
Textures remain unchanged, so the reduction in file size is smaller.

Only Kmdl's `al_head.bmd` changes. Other clothing variants and the body, face,
hands and weapons remain untouched. The small first shape is retained exactly.
The larger hair shape is simplified separately within each packet and draw-matrix
assignment. Small disconnected patches (up to 32 triangles, using complete vertex
records for connectivity) are retained exactly, including UV and normal seams.

The first reduced version allowed the simplifier to remove tiny surface patches,
leaving visible holes. It lost 960 triangles belonging to 935 entirely removed
record-connected components in the final two packets. This revision preserves
all 3,331 non-connector triangles in those two packets exactly. Their strip
encoding can still change to reduce buffer use. Protected details in the other
packets are also checked during generation; packet 4's entire surface is unchanged.
The fix restores detail at the cost of some of the initial reduction (previously 39.3% fewer strip triangles / 37.0% fewer records).

- Positions, normals, both UV sets, joints, skin weights, materials and textures
  retain their original binary data. The new drawing lists reference a subset of
  the original complete vertex records.
- Packet borders and vertices used by protected details or skinning transitions
  are locked. All protected triangles survive with the same winding and
  multiplicity. Only repeated complete vertex records identify invisible strip
  connectors; local-space triangle area is never used to discard a transition.
- Simplification does not compare or weld positions across different draw
  matrices. Drawing order is optimized for triangle strips after simplification;
  the decoded strips must reproduce exactly the selected oriented triangles.
- A packet keeps its original drawing list if the new strip encoding would
  increase its byte size or expanded triangle count.
- A conservative meshoptimizer error limit of 0.005 is used, with normal and UV
  attribute weights of 0.1. This is the simplifier's combined error metric, not a
  claim of a measured 0.5% maximum animated surface deviation.

## Reproduce

The source asset is the same path at commit
`146d708a6b92ed931d48b1050eb37727dbc7fd11` in this repository. Use
[meshoptimizer](https://github.com/zeux/meshoptimizer) at commit
`9e1f07b159d3cb777f1c67ed31fc11fd117986f4` and Python with NumPy:

```sh
git show 146d708a6b92ed931d48b1050eb37727dbc7fd11:overlay/res/Object/Kmdl/archive/bmwr/al_head.bmd > /tmp/ichigo-head-original.bmd
g++ -std=c++11 -O2 -shared -fPIC /path/to/meshoptimizer/src/*.cpp -o /tmp/libmeshoptimizer.so
python3 tools/simplify_hair.py /tmp/ichigo-head-original.bmd /tmp/ichigo-head-reduced.bmd --library /tmp/libmeshoptimizer.so
python3 tests/hair_model_test.py
```

The simplification tool fails on unsupported layouts instead of silently rewriting
other BMD variants. It verifies the stripified triangle winding and retained
skinning transitions before writing output. The standalone standard-library test
checks the shipped binary's section hashes, packet alignment, GX stream bounds,
vertex-array indices and matrix-palette indices. It also compares skinning
transitions and the complete detail-packet geometry against fingerprints from
the original asset, independently of the new file hash; CI runs it on Linux.

The user confirmed that the first reduction resolved the Dark Link crash. This
revision retains substantial buffer savings, but its textured appearance and
crash behavior still need in-game confirmation. Offline topology checks do not
replace that test.
