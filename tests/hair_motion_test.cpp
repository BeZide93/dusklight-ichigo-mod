#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdlib>
#include <limits>
#include "../src/hair_motion_limits.hpp"

int main() {
    using ichigo::damp_hair_angle;
    // Sweep the full signed game-angle range: no overflow, reversed motion,
    // larger output, or jumps backward as the native angle increases.
    for (int joint = 1; joint <= 5; ++joint) {
        int previous = std::numeric_limits<std::int16_t>::min();
        for (int angle = -32768; angle <= 32767; ++angle) {
            const int result = damp_hair_angle(joint, static_cast<std::int16_t>(angle));
            assert(result >= previous);
            assert(std::abs(result) <= (joint == 2 ? 546 : 1092));
            assert(std::abs(result) <= std::abs(angle));
            assert((angle >= 0 && result >= 0) || (angle < 0 && result <= 0));
            previous = result;
            if (angle != -32768) assert(result == -damp_hair_angle(joint, -angle));
        }
        assert(damp_hair_angle(joint, 0) == 0);
        assert(damp_hair_angle(joint, 1000) > 0);  // Retain visible movement.
    }
    assert(damp_hair_angle(2, 5000) < damp_hair_angle(1, 5000));
    // Root and accessory joints must pass through without changing their pose.
    for (int joint : {-1, 0, 6, 7, 8, 9}) {
        for (int angle = -32768; angle <= 32767; ++angle) {
            assert(damp_hair_angle(joint, angle) == angle);
        }
    }
}
