#pragma once

#include <algorithm>
#include <cstdint>

namespace ichigo {

// Only the five procedural hair joints, not head_root or the cap/accessories.
// hairL2 is a child of hairL1: keep its extra bend smaller than the root bend.
constexpr std::int16_t damp_hair_angle(int joint, std::int16_t angle) {
    if (joint < 1 || joint > 5) return angle;
    const int divisor = joint == 2 ? 10 : 5;
    const int limit = joint == 2 ? 546 : 1092;  // About 3 / 6 degrees per axis.
    return static_cast<std::int16_t>(std::clamp(static_cast<int>(angle) / divisor,
                                             -limit, limit));
}

}  // namespace ichigo
