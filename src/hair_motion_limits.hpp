#pragma once

#include <cmath>
#include <cstdint>

namespace ichigo {

// Only the five procedural hair joints, not head_root or the cap/accessories.
// hairL2 is a child of hairL1: keep its extra bend smaller than the root bend.
inline std::int16_t damp_hair_angle(int joint, std::int16_t angle) {
    if (joint < 1 || joint > 5) return angle;
    const float gain = joint == 2 ? 0.15f : 0.30f;
    const float limit = joint == 2 ? 819.0f : 1638.0f;  // About 4.5 / 9 degrees.
    // Smooth saturation: continuous slope through the former hard-clamp knee.
    // No frame-history filter, so drawing twice cannot advance the motion.
    return static_cast<std::int16_t>(std::lround(limit * std::tanh(gain * angle / limit)));
}

}  // namespace ichigo
