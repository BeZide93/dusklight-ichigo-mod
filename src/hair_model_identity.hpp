#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace ichigo {

struct HairGeometry {
    std::uint32_t vertices = 0, type = 0, fraction = 0;
    std::uint64_t positions = 0;
    bool operator==(const HairGeometry&) const = default;
    explicit operator bool() const { return vertices != 0; }
};

inline std::size_t position_component_size(std::uint32_t type) {
    return type <= 1 ? 1 : type <= 3 ? 2 : type == 4 ? 4 : 0;
}

// Normalize each position component to big endian. The loader swaps position
// arrays in place on little-endian hosts; the packaged BMD is always big endian.
inline HairGeometry hair_geometry(std::span<const std::uint8_t> bytes,
                                  std::uint32_t vertices, std::uint32_t type,
                                  std::uint32_t fraction, bool littleEndian) {
    const auto width = position_component_size(type);
    if (!width || !vertices || vertices > bytes.size() / (3 * width)) return {};
    std::uint64_t hash = 14695981039346656037ULL;
    for (std::size_t offset = 0; offset < std::size_t(vertices) * 3 * width; offset += width) {
        for (std::size_t i = 0; i < width; ++i) {
            hash ^= bytes[offset + (littleEndian ? width - 1 - i : i)];
            hash *= 1099511628211ULL;
        }
    }
    return {vertices, type, fraction, hash};
}

inline HairGeometry packaged_hair_geometry(std::span<const std::uint8_t> file) {
    if (file.size() < 32 || std::memcmp(file.data(), "J3D2bmd3", 8)) return {};
    const auto be32 = [](const std::uint8_t* p) {
        return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) |
               (std::uint32_t(p[2]) << 8) | p[3];
    };
    if (be32(file.data() + 8) != file.size()) return {};
    std::uint32_t vertices = 0;
    std::span<const std::uint8_t> vertexBlock;
    std::size_t offset = 32;
    const auto blocks = be32(file.data() + 12);
    for (std::uint32_t i = 0; i < blocks; ++i) {
        if (file.size() - offset < 8) return {};
        const auto* block = file.data() + offset;
        const auto size = be32(block + 4);
        if (size < 8 || size > file.size() - offset) return {};
        if (!std::memcmp(block, "INF1", 4)) {
            if (size < 20) return {};
            vertices = be32(block + 16);
        } else if (!std::memcmp(block, "VTX1", 4)) {
            vertexBlock = file.subspan(offset, size);
        }
        offset += size;
    }
    if (offset != file.size() || vertexBlock.size() < 64 || !vertices) return {};
    const auto* block = vertexBlock.data();
    std::size_t format = be32(block + 8);
    const std::size_t positions = be32(block + 12);
    if (format < 64 || positions < 64 || positions >= vertexBlock.size()) return {};
    // Find the position format. Header and format entries remain bounds checked.
    while (format <= vertexBlock.size() && vertexBlock.size() - format >= 16) {
        const auto* entry = block + format;
        const auto attribute = be32(entry);
        if (attribute == 255) break;
        if (attribute == 9) {  // GX_VA_POS, XYZ
            if (be32(entry + 4) != 1) return {};
            std::size_t end = vertexBlock.size();
            for (int field = 16; field < 64; field += 4) {
                const auto next = be32(block + field);
                if (next > positions && next < end) end = next;
            }
            return hair_geometry(vertexBlock.subspan(positions, end - positions),
                                 vertices, be32(entry + 8), entry[12], false);
        }
        format += 16;
    }
    return {};
}

// Cleared before changeLink constructs/rebinds a head, even if the allocator
// recycles all addresses. Config changes deliberately do not invalidate this.
struct HairIdentityCache {
    const void* model = nullptr;
    const void* raw = nullptr;
    const void* positions = nullptr;
    bool known = false, ichigo = false;

    void clear() { *this = {}; }
    template<class Classify>
    bool matches(const void* nextModel, const void* nextRaw, const void* nextPositions,
                 Classify classify) {
        if (!nextModel || !nextRaw || !nextPositions) { clear(); return false; }
        if (!known || model != nextModel || raw != nextRaw || positions != nextPositions) {
            model = nextModel; raw = nextRaw; positions = nextPositions;
            ichigo = classify(); known = true;
        }
        return ichigo;
    }
};

}  // namespace ichigo
