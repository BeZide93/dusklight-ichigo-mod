# Reduced Ichigo hair model

The normal Kmdl `al_head.bmd` uses fewer triangles and vertex records to reduce
pressure on Dusklight's per-frame geometry buffers when Link and Dark Link both
use Ichigo's appearance. This does not guarantee that every mod combination fits
within the renderer's fixed limits.

| Stored drawing data | Before | After | Reduction |
|---|---:|---:|---:|
| Triangles expanded from GX strips | 38,927 | 23,639 | 39.3% |
| Submitted vertex records | 52,049 | 32,795 | 37.0% |
| Raw vertex-stream bytes per full draw | 468,243 | 294,957 | 37.0% |
| Triangle index bytes per full draw | 233,562 | 141,834 | 39.3% |
| BMD file bytes | 1,749,280 | 1,570,016 | 10.2% |

Counts include degenerate triangles in strips because the renderer expands them
into indices too. Actual frame savings depend on which shapes and passes render;
these are file-derived drawing counts, not measured frame-buffer occupancy.
Textures remain unchanged, so the reduction in file size is smaller.

Only Kmdl's `al_head.bmd` changes. Other clothing variants and the body, face,
hands and weapons remain untouched. The small first shape is retained exactly.
The larger hair shape is simplified separately within each matrix packet.

- Positions, normals, both UV sets, joints, skin weights, materials and textures
  retain their original binary data. The new drawing lists reference a subset of
  the original complete vertex records.
- Packet borders and vertices on edges crossing different skinning-matrix
  assignments are locked. Nondegenerate triangles spanning those assignments are
  verified to survive with the same winding.
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
vertex-array indices and matrix-palette indices; CI runs it on Linux.

Front, side and rear geometry previews were compared offline. Android/in-game
animation, textured appearance and Dark Link crash testing remain necessary.
