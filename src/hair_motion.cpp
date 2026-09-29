#include "hair_motion.hpp"
#include "hair_motion_limits.hpp"
#include "model_settings.hpp"
#include "service_imports.hpp"

#include "d/actor/d_a_alink.h"
#include "mods/service.hpp"
#include "mods/svc/hook.hpp"

namespace ichigo {
namespace {

DEFINE_HOOK(&daAlink_c::setMatrixWorldAxisRot, IchigoHairRotationHook);

HookAction before_world_axis_rotation(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    const auto matrix = mods::arg<MtxP>(args, 1);
    if (!link || link != daAlink_getAlinkActorClass() || !matrix ||
        link->checkWolf() || link->checkStatusWindowDraw()) {
        return HOOK_CONTINUE;
    }

    // Sumo swaps the head independently of the clothing/face archive. This
    // loaded-model flag outlives the sumo camera mode and covers the cutscenes.
    const char* group = link->checkNoResetFlg2(daPy_py_c::FLG2_UNK_80000)
                            ? "alSumou" : link->mArcName;
    if (!head_overlay_enabled(group)) return HOOK_CONTINUE;

    auto* head = link->mpLinkHatModel;
    if (!head || !head->getModelData() || !head->getMtxBuffer()) return HOOK_CONTINUE;

    // This helper also handles body/limb rotations. Match the exact head
    // matrix and the native hair call signature before adjusting anything.
    if (mods::arg<s16>(args, 3) != 0 || mods::arg<BOOL>(args, 5) != 0 ||
        mods::arg<const cXyz*>(args, 6) != nullptr) {
        return HOOK_CONTINUE;
    }
    const auto jointCount = head->getModelData()->getJointNum();
    for (int joint = 1; joint <= 5 && joint < jointCount; ++joint) {
        if (matrix != head->getAnmMtx(joint)) continue;
        auto& x = mods::arg_ref<s16>(args, 2);
        auto& z = mods::arg_ref<s16>(args, 4);
        x = damp_hair_angle(joint, x);
        z = damp_hair_angle(joint, z);
        break;
    }

    // Change only call arguments. The original updates the matrices, while
    // the native wind/idle state stays untouched (no cumulative damping).
    // Authored head BCKs and the separate cap callback path remain native.
    return HOOK_CONTINUE;
}

}  // namespace

ModResult install_hair_motion_hooks(ModError* error) {
    const auto result = mods::hook::add_pre<IchigoHairRotationHook>(
        svc_hook, before_world_axis_rotation);
    if (result != MOD_OK) {
        return mods::set_error(error, result, "failed to install Ichigo hair motion hook");
    }
    return MOD_OK;
}

}  // namespace ichigo
