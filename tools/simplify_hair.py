#!/usr/bin/env python3
"""Simplify this mod's al_head.bmd display lists without rewriting its rig or materials.

Requires numpy and a shared meshoptimizer library; see docs/hair-model.md.
Only existing vertex records are selected. UVs, normals, bone indices and all
non-SHP1 sections remain byte-identical. Packet borders and skinning transitions
are locked. The small first shape is kept intact.
"""
import argparse
import ctypes as C
import json
from collections import Counter
import struct
from pathlib import Path
import numpy as np


def u16(data, offset): return struct.unpack_from('>H', data, offset)[0]
def u32(data, offset): return struct.unpack_from('>I', data, offset)[0]
def padded(data): return data + bytes((-len(data)) % 32)
def oriented(tri):
    return min(tuple(tri[i:]) + tuple(tri[:i]) for i in range(3))


class Model:
    def __init__(self, data):
        assert data[:8] == b'J3D2bmd3' and u32(data, 8) == len(data)
        self.data = data
        self.sections = []
        offset = 32
        for _ in range(u32(data, 12)):
            size = u32(data, offset + 4)
            assert size >= 8 and offset + size <= len(data)
            self.sections.append(data[offset:offset + size])
            offset += size
        assert offset == len(data)
        self.blocks = {b[:4]: b for b in self.sections}
        self.shp = self.blocks[b'SHP1']
        s = self.shp
        assert u16(s, 8) == 2
        self.packets = []
        self.formats = {}
        vtx = self.blocks[b'VTX1']
        offset = u32(vtx, 8)
        while u32(vtx, offset) != 255:
            attr, count, typ = struct.unpack_from('>III', vtx, offset)
            self.formats[attr] = count, typ, vtx[offset + 12]
            offset += 16
        init, remap, desc, self.matrix, self.dl, self.minit, self.dinit = [u32(s, x) for x in (12, 16, 24, 28, 32, 36, 40)]
        for shape in range(2):
            entry = init + u16(s, remap + 2 * shape) * 40
            groups, vi, mi, di = struct.unpack_from('>4H', s, entry + 2)
            assert s[entry] == 3
            attrs = []
            offset = desc + vi
            while u32(s, offset) != 255:
                attr, kind = struct.unpack_from('>II', s, offset)
                assert (attr == 0 and kind == 1) or (attr in (9, 10, 13, 14) and kind == 3)
                attrs.append((attr, 1 if kind == 1 else 2))
                offset += 8
            stride = sum(width for _, width in attrs)
            for group in range(groups):
                size, start = struct.unpack_from('>II', s, self.dinit + 8 * (di + group))
                raw = s[self.dl + start:self.dl + start + size]
                assert len(raw) == size
                vertices, triangles, lookup = [], [], {}
                offset = 0
                while offset < len(raw):
                    command = raw[offset]; offset += 1
                    if command == 0: continue
                    assert command in (0x90, 0x98)
                    count = u16(raw, offset); offset += 2
                    assert offset + count * stride <= len(raw)
                    indices = []
                    for _ in range(count):
                        record = raw[offset:offset + stride]; offset += stride
                        if record not in lookup:
                            lookup[record] = len(vertices); vertices.append(record)
                        indices.append(lookup[record])
                    if command == 0x90:
                        assert count % 3 == 0
                        triangles.extend(zip(indices[::3], indices[1::3], indices[2::3]))
                    else:
                        for i in range(2, count):
                            triangles.append((indices[i-2], indices[i-1], indices[i]) if i % 2 == 0 else (indices[i-1], indices[i-2], indices[i]))
                mt = self.minit + 8 * (mi + group)
                palette = [u16(s, self.matrix + 2 * (u32(s, mt+4) + i)) for i in range(u16(s, mt+2))]
                self.packets.append(dict(shape=shape, index=di+group, raw=raw, records=vertices, attrs=attrs,
                    triangles=np.array(triangles, dtype=np.uint32), stride=stride, palette=palette))
        assert sorted(p['index'] for p in self.packets) == list(range(len(self.packets)))

    def decode(self, packet):
        vtx = self.blocks[b'VTX1']
        positions, attributes, bones = [], [], []
        for record in packet['records']:
            offset = 0; values = {}; bone = packet['palette'][0]
            for attr, width in packet['attrs']:
                index = record[offset] if width == 1 else u16(record, offset)
                offset += width
                if attr == 0:
                    assert index % 3 == 0 and index // 3 < len(packet['palette'])
                    bone = packet['palette'][index // 3]
                    continue
                count, typ, fraction = self.formats[attr]
                assert typ == 3 and ((attr == 10 and count == 0) or (attr != 10 and count == 1))
                components = 3 if attr in (9, 10) else 2
                base = u32(vtx, {9:12, 10:16, 13:32, 14:36}[attr])
                values[attr] = np.array(struct.unpack_from('>'+'h'*components, vtx, base+index*components*2)) / (2**fraction)
            positions.append(values[9]); bones.append(bone)
            attributes.append(np.concatenate([values[a] for a in (10,13,14) if a in values]))
        return np.array(positions, dtype=np.float32), np.array(attributes, dtype=np.float32), np.array(bones)


def simplify(model, library, ratio=0.3, error=0.005):
    lib = C.CDLL(str(library))
    f = lib.meshopt_simplifyWithAttributes
    f.restype = C.c_size_t
    f.argtypes = [C.c_void_p,C.c_void_p,C.c_size_t,C.c_void_p,C.c_size_t,C.c_size_t,C.c_void_p,C.c_size_t,C.c_void_p,C.c_size_t,C.c_void_p,C.c_size_t,C.c_float,C.c_uint,C.c_void_p]
    lib.meshopt_stripifyBound.argtypes = [C.c_size_t]; lib.meshopt_stripifyBound.restype = C.c_size_t
    lib.meshopt_stripify.argtypes = [C.c_void_p,C.c_void_p,C.c_size_t,C.c_size_t,C.c_uint]; lib.meshopt_stripify.restype = C.c_size_t
    report = []; outputs = []
    for p in model.packets:
        if p['shape'] == 0:
            outputs.append(p['raw']); continue
        pos, attr, bones = model.decode(p)
        indices = p['triangles'].ravel().copy(); dest = np.empty_like(indices)
        lock = np.zeros(len(pos), dtype=np.uint8)
        for tri in p['triangles']:
            if len(set(bones[tri])) > 1: lock[tri] = 1
        # meshoptimizer requires consistent locks for coincident vertices.
        locked = {tuple(pos[i]) for i in np.flatnonzero(lock)}
        for i, value in enumerate(pos):
            if tuple(value) in locked: lock[i] = 1
        weights = np.full(attr.shape[1], 0.1, dtype=np.float32)
        achieved = C.c_float()
        count = f(dest.ctypes.data, indices.ctypes.data, len(indices), pos.ctypes.data, len(pos), 12,
            attr.ctypes.data, attr.strides[0], weights.ctypes.data, attr.shape[1], lock.ctypes.data,
            int(len(indices)*ratio)//3*3, error, 1, C.byref(achieved))
        assert count > 0 and count % 3 == 0 and count <= len(indices)
        dest = dest[:count]
        # Stripify without connecting separate strips by degenerate triangles.
        strips = np.empty(lib.meshopt_stripifyBound(count), dtype=np.uint32)
        n = lib.meshopt_stripify(strips.ctypes.data, dest.ctypes.data, count, len(pos), 0xffffffff)
        raw = bytearray(); current = []
        for v in list(strips[:n]) + [0xffffffff]:
            if v == 0xffffffff:
                if current:
                    assert 3 <= len(current) <= 65535
                    raw.extend(struct.pack('>BH', 0x98, len(current)))
                    for i in current: raw.extend(p['records'][i])
                    current = []
            else: current.append(int(v))
        # Check strip output against the simplified triangle list, including winding.
        decoded = []
        offset = 0
        record_ids = {record: i for i, record in enumerate(p['records'])}
        while offset < len(raw):
            assert raw[offset] == 0x98
            size = u16(raw, offset + 1); offset += 3
            ids = []
            for _ in range(size):
                ids.append(record_ids[bytes(raw[offset:offset+p['stride']])]); offset += p['stride']
            for i in range(2, size):
                decoded.append((ids[i-2],ids[i-1],ids[i]) if i % 2 == 0 else (ids[i-1],ids[i-2],ids[i]))
        assert Counter(oriented(list(t)) for t in decoded if len(set(t)) == 3) == Counter(oriented(list(t)) for t in dest.reshape(-1,3) if len(set(t)) == 3)
        if len(padded(raw)) >= len(p['raw']) or len(decoded) >= len(p['triangles']):
            outputs.append(p['raw'])
            output_count = len(p['triangles'])
        else:
            outputs.append(padded(raw))
            output_count = len(decoded)
        report.append(dict(packet=p['index'], before=len(indices)//3, after=count//3,
            output_triangles=output_count, locked_vertices=int(lock.sum()), error=achieved.value))
    s = bytearray(model.shp[:model.dl]); draws = []
    for p, raw in zip(model.packets, outputs):
        draws.append((len(raw),len(s)-model.dl)); s.extend(raw)
    s = bytearray(padded(s)); new_minit = len(s)
    s.extend(model.shp[model.minit:model.dinit]); new_dinit = len(s)
    for size, offset in draws: s.extend(struct.pack('>II', size, offset))
    s = bytearray(padded(s))
    struct.pack_into('>I',s,4,len(s)); struct.pack_into('>II',s,36,new_minit,new_dinit)
    result = bytearray(model.data[:32])
    for block in model.sections: result.extend(s if block[:4] == b'SHP1' else block)
    struct.pack_into('>I',result,8,len(result))
    return bytes(result), report


def validate(original, output):
    before, after = Model(original), Model(output)
    for tag in before.blocks:
        if tag != b'SHP1': assert before.blocks[tag] == after.blocks[tag], tag
    assert len(before.packets) == len(after.packets)
    for old, new in zip(before.packets, after.packets):
        assert old['attrs'] == new['attrs'] and old['palette'] == new['palette']
        assert set(new['records']) <= set(old['records'])
        if old['shape'] == 0: assert old['raw'] == new['raw']
        pos, _, bones = before.decode(old)
        after.decode(new)  # Validate every retained attribute/palette reference.
        new_triangles = {oriented([new['records'][i] for i in t]) for t in new['triangles']}
        for tri in old['triangles']:
            area = np.linalg.norm(np.cross(pos[tri[1]]-pos[tri[0]],pos[tri[2]]-pos[tri[0]]))
            if len(set(bones[tri])) > 1 and area > 1e-8:
                assert oriented([old['records'][i] for i in tri]) in new_triangles
        assert len(new['triangles']) <= len(old['triangles'])
    return before, after


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path); parser.add_argument('output', type=Path)
    parser.add_argument('--library', required=True, type=Path)
    parser.add_argument('--ratio', type=float, default=0.3)
    parser.add_argument('--error', type=float, default=0.005)
    args = parser.parse_args()
    assert 0 < args.ratio <= 1 and 0 < args.error <= 0.01
    original = args.input.read_bytes()
    result, report = simplify(Model(original), args.library, args.ratio, args.error)
    before, after = validate(original, result)
    print(json.dumps(dict(packets=report, triangles_before=sum(len(p['triangles']) for p in before.packets),
        triangles_after=sum(len(p['triangles']) for p in after.packets), bytes_before=len(original), bytes_after=len(result)), indent=2))
    args.output.write_bytes(result)
