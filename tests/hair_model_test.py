"""Validate the shipped BMD's sections and GX display-list bounds (stdlib only)."""
import hashlib
import json
import struct
from pathlib import Path

root = Path(__file__).resolve().parents[1]
# Support either side of the independent per-model settings PR's asset rename.
paths = [root / prefix / 'Object/Kmdl/archive/bmwr/al_head.bmd'
         for prefix in ('overlay/res', 'res/models')]
path = next(p for p in paths if p.is_file())
d = path.read_bytes()
expected = json.loads((root / 'tests/hair_model_expected.json').read_text())
u16 = lambda b, o: struct.unpack_from('>H', b, o)[0]
u32 = lambda b, o: struct.unpack_from('>I', b, o)[0]


def face_digest(faces):
    # Oriented full records include UVs, normals and draw-matrix assignments.
    # Reordering strips is allowed; changing a face, its winding or multiplicity
    # is not. Expected values come from the ORIGINAL asset, not this build.
    return dict(triangles=len(faces), sha256=hashlib.sha256(b''.join(sorted(faces))).hexdigest())

assert d[:8] == b'J3D2bmd3' and u32(d, 8) == len(d)
assert hashlib.sha256(d).hexdigest() == expected['file_sha256']
blocks = {}; offset = 32
for _ in range(u32(d, 12)):
    size = u32(d, offset + 4)
    assert size >= 8 and offset + size <= len(d)
    blocks[d[offset:offset+4].decode()] = d[offset:offset+size]
    offset += size
assert offset == len(d)
for tag, sha in expected['unchanged_sections'].items():
    assert hashlib.sha256(blocks[tag]).hexdigest() == sha, tag
s = blocks['SHP1']
init, remap, desc, matrix, dl, mtx, draw = [u32(s, p) for p in (12,16,24,28,32,36,40)]
assert u16(s, 8) == expected['shapes']
assert dl % 32 == mtx % 32 == 0
triangles = packets = vertices = 0
vtx = blocks['VTX1']
for shape in range(u16(s, 8)):
    entry = init + u16(s, remap + shape*2)*40
    groups, vi, mi, di = struct.unpack_from('>4H', s, entry+2)
    attrs = []; offset = desc + vi
    while u32(s, offset) != 255:
        attr, kind = struct.unpack_from('>II', s, offset); offset += 8
        assert (attr == 0 and kind == 1) or (attr in (9,10,13,14) and kind == 3)
        attrs.append((attr, 1 if kind == 1 else 2))
    for group in range(groups):
        size, start = struct.unpack_from('>II', s, draw+(di+group)*8)
        assert start % 32 == size % 32 == 0 and dl+start+size <= mtx
        palette_size = u16(s, mtx+(mi+group)*8+2)
        palette_offset = u32(s, mtx+(mi+group)*8+4)
        palette = [u16(s, matrix+(palette_offset+i)*2) for i in range(palette_size)]
        assert all(bone < u16(blocks['DRW1'],8) for bone in palette)
        faces = []; transitions = []
        raw = s[dl+start:dl+start+size]; offset = 0
        while offset < len(raw):
            command = raw[offset]; offset += 1
            if command == 0: continue
            assert command == 0x98
            count = u16(raw, offset); offset += 2
            assert count >= 3
            triangles += count-2; vertices += count
            records = []; bones = []
            for _ in range(count):
                record_start = offset; bone = palette[0]
                for attr, width in attrs:
                    assert offset+width <= len(raw)
                    index = raw[offset] if width == 1 else u16(raw,offset)
                    offset += width
                    if attr == 0:
                        assert index % 3 == 0 and index//3 < palette_size
                        bone = palette[index//3]
                    else:
                        base = u32(vtx,{9:12,10:16,13:32,14:36}[attr])
                        components = 3 if attr in (9,10) else 2
                        following = [u32(vtx,p) for p in range(12,64,4) if u32(vtx,p)>base]
                        end = min(following, default=len(vtx))
                        assert base+(index+1)*components*2 <= end
                records.append(raw[record_start:offset]); bones.append(bone)
            for i in range(2, count):
                ids = (i-2, i-1, i) if i % 2 == 0 else (i-1, i-2, i)
                t = [records[j] for j in ids]
                # Only repeated complete records are always invisible. Do not
                # reject faces by area in untransformed bone-local coordinates.
                if len(set(t)) != 3: continue
                face = b''.join(min(tuple(t[k:]+t[:k]) for k in range(3)))
                faces.append(face)
                if len({bones[j] for j in ids}) > 1: transitions.append(face)
        packet_id = di+group
        assert face_digest(transitions) == expected['skinning_transitions'][packet_id], 'Skinning transitions changed'
        if str(packet_id) in expected['unchanged_topology']:
            assert face_digest(faces) == expected['unchanged_topology'][str(packet_id)], 'Fine hair patches changed'
        packets += 1
assert triangles == expected['triangles'] and packets == expected['packets']
assert vertices == expected['vertices']
print(f'Hair BMD valid: {triangles} strip triangles, {vertices} vertex records; rig/material/texture sections unchanged')
