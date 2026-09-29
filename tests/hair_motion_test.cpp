#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdlib>
#include <initializer_list>
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
            assert(std::abs(result) <= (joint == 2 ? 819 : 1638));
            assert(std::abs(result) <= std::abs(angle));
            assert((angle >= 0 && result >= 0) || (angle < 0 && result <= 0));
            previous = result;
            if (angle != -32768) assert(result == -damp_hair_angle(joint, -angle));
        }
        assert(damp_hair_angle(joint, 0) == 0);
        assert(damp_hair_angle(joint, 1000) > 0);  // Retain visible movement.
    }
    // More movement than the initial patch, without a flat hard-clamp knee.
    assert(damp_hair_angle(1, 1000) > 200);
    assert(damp_hair_angle(2, 1000) > 100);
    for (int joint = 1; joint <= 5; ++joint) {
        for (int angle : {5300, 5460, 5600, 10000}) {
            const int before = damp_hair_angle(joint, angle) - damp_hair_angle(joint, angle - 100);
            const int after = damp_hair_angle(joint, angle + 100) - damp_hair_angle(joint, angle);
            assert(before > 0 && after > 0);
            assert(std::abs(before - after) <= 2);
        }
    }
    assert(damp_hair_angle(2, 5000) < damp_hair_angle(1, 5000));
    // Root and accessory joints must pass through without changing their pose.
    for (int joint : {-1, 0, 6, 7, 8, 9}) {
        for (int angle = -32768; angle <= 32767; ++angle) {
            assert(damp_hair_angle(joint, angle) == angle);
        }
    }
}
