#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iterator>
#include <string_view>
#include <vector>
#include "../src/hair_model_identity.hpp"

static std::uint32_t be32(const std::uint8_t* p) {
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) |
           (std::uint32_t(p[2]) << 8) | p[3];
}

int main(int argc, char** argv) {
    assert(argc == 2);
    struct Resource { const char* label; const char* path; };
#define ICHIGO_MODEL(key, group, label, path) {label, "res/models/" path},
    const Resource resources[] = {
#include "../src/model_overlays.inc"
    };
#undef ICHIGO_MODEL
    int checked = 0;
    for (const auto& r : resources) {
        if (!std::string_view(r.label).ends_with("_head.bmd")) continue;
        std::ifstream input(std::string(argv[1]) + "/" + r.path, std::ios::binary);
        assert(input);
        std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>(input), {});
        const auto expected = ichigo::packaged_hair_geometry(bytes);
        assert(expected);
        ++checked;
        // Independently simulate the game's in-place native endian conversion
        // of the actual shipped position data, including the sumo asset.
        std::size_t offset = 32;
        while (std::string_view(reinterpret_cast<char*>(bytes.data() + offset), 4) != "VTX1") {
            offset += be32(bytes.data() + offset + 4);
            assert(offset + 64 <= bytes.size());
        }
        const auto positionOffset = offset + be32(bytes.data() + offset + 12);
        const auto width = ichigo::position_component_size(expected.type);
        const auto length = std::size_t(expected.vertices) * 3 * width;
        assert(positionOffset + length <= bytes.size());
        std::vector<std::uint8_t> native(bytes.begin() + positionOffset, bytes.begin() + positionOffset + length);
        for (std::size_t i = 0; i < length; i += width) {
            std::reverse(native.begin() + i, native.begin() + i + width);
        }
        assert(ichigo::hair_geometry(native, expected.vertices, expected.type, expected.fraction, true) == expected);
        native[0] ^= 1;
        assert(ichigo::hair_geometry(native, expected.vertices, expected.type, expected.fraction, true) != expected);
        // Truncated or inconsistent inputs fail closed.
        assert(!ichigo::hair_geometry({}, expected.vertices, expected.type, expected.fraction, false));
        bytes.pop_back();
        assert(!ichigo::packaged_hair_geometry(bytes));
    }
    assert(checked == 5);
    for (std::size_t size = 0; size < 64; ++size) {
        std::vector<std::uint8_t> invalid(size);
        assert(!ichigo::packaged_hair_geometry(invalid));
    }

    // Loaded Ichigo -> toggle Off -> still Ichigo until a real reload.
    // Loaded vanilla -> toggle On -> stays vanilla until a real reload.
    // Recycled model/raw/position addresses must be reclassified on reload.
    ichigo::HairIdentityCache cache;
    int model, raw, positions, other;
    bool loadedIchigo = true;
    int classifications = 0;
    auto classify = [&] { ++classifications; return loadedIchigo; };
    assert(cache.matches(&model, &raw, &positions, classify));
    assert(cache.matches(&model, &raw, &positions, classify)); // Toggle does not alter geometry.
    assert(classifications == 1);
    loadedIchigo = false;
    cache.clear(); // changeLink, with all allocation addresses reused.
    assert(!cache.matches(&model, &raw, &positions, classify));
    assert(!cache.matches(&model, &raw, &positions, classify)); // Toggle On does not force true.
    assert(classifications == 2);
    loadedIchigo = true;
    cache.clear();
    assert(cache.matches(&model, &raw, &positions, classify));
    assert(classifications == 3);
    loadedIchigo = false;
    assert(!cache.matches(&model, &raw, &other, classify)); // Changed vertex storage.
    assert(classifications == 4);
    assert(!cache.matches(nullptr, &raw, &positions, classify));
    assert(!cache.known);
}
