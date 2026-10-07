"""Validate the shipped BMD's sections and GX display-list bounds (stdlib only)."""
import hashlib
import json
import struct
from pathlib import Path

root = Path(__file__).resolve().parents[1]
expectations = json.loads((root / 'tests/hair_model_expected.json').read_text())
u16 = lambda b, o: struct.unpack_from('>H', b, o)[0]
u32 = lambda b, o: struct.unpack_from('>I', b, o)[0]


def face_digest(faces):
    # Oriented full records include UVs, normals and draw-matrix assignments.
    # Reordering strips is allowed; changing a face, its winding or multiplicity
    # is not. Expected values come from the pre-optimization shipped asset.
    return dict(triangles=len(faces), sha256=hashlib.sha256(b''.join(sorted(faces))).hexdigest())

def inspect_model(d):
    assert d[:8] == b'J3D2bmd3' and u32(d, 8) == len(d)
    blocks = {}; offset = 32
    for _ in range(u32(d, 12)):
        size = u32(d, offset + 4)
        assert size >= 8 and size % 32 == 0 and offset + size <= len(d)
        blocks[d[offset:offset+4].decode()] = d[offset:offset+size]
        offset += size
    assert offset == len(d)
    s = blocks['SHP1']
    init, remap, desc, matrix, dl, mtx, draw = [u32(s, p) for p in (12,16,24,28,32,36,40)]
    assert dl % 32 == mtx % 32 == 0
    triangles = packets = vertices = 0
    vtx = blocks['VTX1']
    formats = []; offset = u32(vtx, 8)
    while u32(vtx, offset) != 255:
        attr, components, kind, fraction = struct.unpack_from('>IIIB', vtx, offset)
        assert attr in (9,10,13,14) and kind == 3  # GX_S16
        assert components == (0 if attr == 10 else 1)
        formats.append([attr, components, kind, fraction]); offset += 16
    shape_faces = []; shape_transitions = []
    for shape in range(u16(s, 8)):
        entry = init + u16(s, remap + shape*2)*40
        groups, vi, mi, di = struct.unpack_from('>4H', s, entry+2)
        faces = []; transitions = []
        attrs = []; offset = desc + vi
        while u32(s, offset) != 255:
            attr, kind = struct.unpack_from('>II', s, offset); offset += 8
            assert (attr == 0 and kind == 1) or (attr in (9,10,13,14) and kind == 3)
            attrs.append((attr, 1 if kind == 1 else 2))
        for group in range(groups):
            size, start = struct.unpack_from('>II', s, draw+(di+group)*8)
            assert start % 32 == size % 32 == 0 and dl+start+size <= mtx
            palette_size = u16(s, mtx+(mi+group)*8+2)
            assert 0 < palette_size <= 10
            palette_offset = u32(s, mtx+(mi+group)*8+4)
            palette = [u16(s, matrix+(palette_offset+i)*2) for i in range(palette_size)]
            assert all(bone < u16(blocks['DRW1'],8) for bone in palette)
            raw = s[dl+start:dl+start+size]; offset = 0
            while offset < len(raw):
                command = raw[offset]; offset += 1
                if command == 0: continue
                assert command in (0x90, 0x98, 0xa0), hex(command)
                assert offset + 2 <= len(raw)
                count = u16(raw, offset); offset += 2
                assert count >= 3
                if command == 0x90:
                    assert count % 3 == 0
                    ids = [(i, i+1, i+2) for i in range(0, count, 3)]
                elif command == 0xa0:
                    ids = [(0, i-1, i) for i in range(2, count)]
                else:
                    ids = [(i-2, i-1, i) if i % 2 == 0 else (i-1, i-2, i)
                           for i in range(2, count)]
                triangles += len(ids); vertices += count
                records = []; bones = []
                for _ in range(count):
                    values = bytearray(); bone = palette[0]
                    for attr, width in attrs:
                        assert offset+width <= len(raw)
                        index = raw[offset] if width == 1 else u16(raw,offset)
                        offset += width
                        if attr == 0:
                            assert index % 3 == 0 and index//3 < palette_size
                            bone = palette[index//3]
                        else:
                            base = u32(vtx,{9:12,10:16,13:32,14:36}[attr])
                            assert base > 0
                            components = 3 if attr in (9,10) else 2
                            following = [u32(vtx,p) for p in range(12,64,4) if u32(vtx,p)>base]
                            end = min(following, default=len(vtx))
                            assert base+(index+1)*components*2 <= end
                            start = base+index*components*2
                            values.extend(bytes([attr]) + vtx[start:start+components*2])
                    # Array indices and local palette slots change during lossless
                    # compaction. Compare exact values and the global matrix ID.
                    records.append(struct.pack('>H', bone) + values); bones.append(bone)
                for face_ids in ids:
                    t = [records[j] for j in face_ids]
                    # Only repeated complete records are always invisible. Do not
                    # reject faces by area in untransformed bone-local coordinates.
                    if len(set(t)) != 3: continue
                    face = b''.join(min(tuple(t[k:]+t[:k]) for k in range(3)))
                    faces.append(face)
                    if len({bones[j] for j in face_ids}) > 1: transitions.append(face)
            packets += 1
        shape_faces.append(face_digest(faces))
        shape_transitions.append(face_digest(transitions))
    # Packet/position counts are intentionally updated by the optimizer; the
    # remainder includes the scene hierarchy and must remain unchanged.
    inf = bytearray(blocks['INF1']); inf[12:20] = bytes(8)
    return dict(file_sha256=hashlib.sha256(d).hexdigest(), triangles=triangles,
                packets=packets, declared_packets=u32(blocks['INF1'], 12),
                shapes=u16(s, 8), vertices=vertices,
                unchanged_sections={tag: hashlib.sha256(blocks[tag]).hexdigest()
                                    for tag in ('EVP1','DRW1','JNT1','MAT3','TEX1')},
                hierarchy_sha256=hashlib.sha256(inf).hexdigest(),
                vertex_formats=formats, shape_faces=shape_faces,
                skinning_transitions=shape_transitions)


def validate_model(relative, expected):
    actual = inspect_model((root / 'res/models' / relative).read_bytes())
    assert actual['declared_packets'] == actual['packets'], f'{relative}: INF1 packet count'
    for field, value in expected.items():
        assert actual[field] == value, f'{relative}: {field} changed'
    print(f"{relative}: valid: {actual['triangles']} GX triangles, "
          f"{actual['vertices']} vertex records; faces/rig/materials/textures verified")


if __name__ == '__main__':
    shipped = {p.relative_to(root / 'res/models').as_posix()
               for p in (root / 'res/models').rglob('*_head.bmd')}
    assert shipped == set(expectations), 'Every shipped head must have expectations'
    for relative, expected in expectations.items():
        validate_model(relative, expected)
