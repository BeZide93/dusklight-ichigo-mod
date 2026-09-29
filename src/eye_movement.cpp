#include "eye_movement.hpp"
#include "model_settings.hpp"
#include "service_imports.hpp"

#include "d/actor/d_a_alink.h"
#include "mods/service.hpp"
#include "mods/svc/hook.hpp"

namespace ichigo {
namespace {

// Adapted from Dawnlight's src/eye_movement.cpp. Scale the final texture
// translation so both BTK animation and procedural idle/target glances work.
DEFINE_HOOK(&daAlink_matAnm_c::calc, IchigoEyeMaterialCalcHook);
constexpr f32 kEyeMovementFactor = 0.4f;

void after_eye_material_calc(ModContext*, void* args, void*, void*) {
    auto* animation = mods::arg<const daAlink_matAnm_c*>(args, 0);
    auto* material = mods::arg<J3DMaterial*>(args, 1);
    auto* link = daAlink_getAlinkActorClass();
    if (!animation || !material || !link || link->checkWolf() ||
        link->checkStatusWindowDraw()) {
        return;
    }
    if (animation != link->field_0x2180[0] && animation != link->field_0x2180[1]) {
        return;
    }
    // changeLink loads the face from mArcName even when the body, hair and
    // hands come from alSumou. Follow that loaded archive, not clothing flags
    // or the sumo body toggle, so its face setting also governs the 40% range.
    if (!face_overlay_enabled(link->mArcName)) {
        return;
    }

    auto* texGen = material->getTexGenBlock();
    if (!texGen) return;
    for (u32 i = 0; i < 8; ++i) {
        if (!animation->getTexMtxAnm(i).getAnmFlag()) continue;
        auto* texMtx = texGen->getTexMtx(i);
        if (!texMtx) continue;

        // Each eye has one animated texture matrix. The original calc caches
        // its final, unscaled X/Y here, before any mod's post hooks run. Use
        // that source rather than multiplying Dawnlight's already-scaled SRT.
        // Leave the cached values and procedural offsets untouched, so morph
        // interpolation and later frames never accumulate the reduction.
        auto& srt = texMtx->getTexMtxInfo().mSRT;
        srt.mTranslationX = animation->field_0xf4 * kEyeMovementFactor;
        srt.mTranslationY = animation->field_0xf8 * kEyeMovementFactor;
    }
}

}  // namespace

ModResult install_eye_movement_hooks(ModError* error) {
    HookOptions options = HOOK_OPTIONS_INIT;
    // Higher priorities run first. Dawnlight uses priority 0; apply the fixed
    // Ichigo range afterward, independent of the two mods' load order.
    options.priority = -1000;
    const auto result = mods::hook::add_post<IchigoEyeMaterialCalcHook>(
        svc_hook, after_eye_material_calc, &options);
    if (result != MOD_OK) {
        return mods::set_error(error, result, "failed to install Ichigo eye movement hook");
    }
    return MOD_OK;
}

}  // namespace ichigo
