#include "hair_motion.hpp"
#include "hair_motion_limits.hpp"
#include "hair_model_identity.hpp"

#include <algorithm>
#include <bit>
#include <string_view>
#include <vector>
#include "service_imports.hpp"

#include "d/actor/d_a_alink.h"
#include "mods/service.hpp"
#include "mods/svc/hook.hpp"

namespace ichigo {
namespace {

DEFINE_HOOK(&daAlink_c::setMatrixWorldAxisRot, IchigoHairRotationHook);
DEFINE_HOOK(&daAlink_c::changeLink, IchigoHeadReloadHook);

std::vector<HairGeometry> s_heads;
HairIdentityCache s_identity;

HookAction before_head_reload(ModContext*, void*, void*, void*) {
    s_identity.clear();
    return HOOK_CONTINUE;
}

bool loaded_ichigo_head(J3DModelData* data) {
    auto& vertices = data->getVertexData();
    return s_identity.matches(data, data->getRawData(), data->getVtxPosArray(), [&] {
        const auto* format = vertices.getVtxAttrFmtList();
        if (!format) return false;
        for (int i = 0; i < 16 && format[i].attr != GX_VA_NULL; ++i) {
            if (format[i].attr != GX_VA_POS) continue;
            if (format[i].cnt != GX_POS_XYZ) return false;
            const auto geometry = hair_geometry(
                {static_cast<const std::uint8_t*>(data->getVtxPosArray()),
                 vertices.getVtxArrByteSize(GX_VA_POS)},
                data->getVtxNum(), format[i].type, format[i].frac,
                std::endian::native == std::endian::little);
            return geometry && std::find(s_heads.begin(), s_heads.end(), geometry) != s_heads.end();
        }
        return false;
    });
}

ModResult load_head_signatures(ModError* error) {
    struct Resource { const char* label; const char* path; };
#define ICHIGO_MODEL(key, group, label, path) {label, "models/" path},
    static constexpr Resource resources[] = {
#include "model_overlays.inc"
    };
#undef ICHIGO_MODEL
    for (const auto& resource : resources) {
        if (!std::string_view(resource.label).ends_with("_head.bmd")) continue;
        ResourceBuffer buffer = RESOURCE_BUFFER_INIT;
        const auto result = svc_resource->load(mod_ctx, resource.path, &buffer);
        if (result != MOD_OK) {
            return mods::set_error(error, result, "failed to read Ichigo head geometry");
        }
        const auto geometry = packaged_hair_geometry(
            {static_cast<const std::uint8_t*>(buffer.data), buffer.size});
        svc_resource->free(mod_ctx, &buffer);
        if (!geometry) {
            return mods::set_error(error, MOD_ERROR, "unsupported Ichigo head vertex layout");
        }
        s_heads.push_back(geometry);
    }
    return MOD_OK;
}

HookAction before_world_axis_rotation(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    const auto matrix = mods::arg<MtxP>(args, 1);
    if (!link || link != daAlink_getAlinkActorClass() || !matrix ||
        link->checkWolf() || link->checkStatusWindowDraw()) {
        return HOOK_CONTINUE;
    }

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
        // Identify the geometry currently in memory, including cached heads
        // after a setting change and replacements supplied by other mods.
        if (!loaded_ichigo_head(head->getModelData())) return HOOK_CONTINUE;
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
    shutdown_hair_motion();
    auto result = load_head_signatures(error);
    if (result != MOD_OK) return result;
    result = mods::hook::add_pre<IchigoHeadReloadHook>(svc_hook, before_head_reload);
    if (result != MOD_OK) {
        return mods::set_error(error, result, "failed to install Ichigo head reload hook");
    }
    result = mods::hook::add_pre<IchigoHairRotationHook>(
        svc_hook, before_world_axis_rotation);
    if (result != MOD_OK) {
        return mods::set_error(error, result, "failed to install Ichigo hair motion hook");
    }
    return MOD_OK;
}

void shutdown_hair_motion() {
    s_identity.clear();
    s_heads.clear();
}

}  // namespace ichigo
