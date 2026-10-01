#include "armor_audio.hpp"
#include "service_imports.hpp"

#include "Z2AudioLib/Z2LinkMgr.h"
#include "Z2AudioLib/Z2SeMgr.h"
#include "mods/svc/hook.hpp"

namespace ichigo {
namespace {

// MSVC cannot const-initialize the SDK's member-pointer metadata for this
// multiple-inheritance class. Resolve the same method by name instead.
DEFINE_HOOK_SYMBOL("Z2CreatureLink::startLinkSound",
                   Z2SoundHandlePool*(Z2CreatureLink*, JAISoundID, u32, s8), IchigoArmorSoundHook);

HookAction before_link_sound(ModContext*, void* args, void* result, void*) {
    auto* linkSound = mods::arg<Z2CreatureLink*>(args, 0);
    const auto sound = mods::arg<JAISoundID>(args, 1);
    if (linkSound == nullptr || linkSound != Z2GetLink() ||
        (sound != Z2SE_FN_ARMER_LIGHT_ADD && sound != Z2SE_FN_ARMER_HEAVY_ADD)) {
        return HOOK_CONTINUE;
    }

    // Z2LinkSoundStarter already plays the normal movement/footstep sound,
    // then layers this armor rattle on top for Link states 4 and 5. Hero's
    // Tunic uses the same base sound without either additive layer.
    *static_cast<Z2SoundHandlePool**>(result) = nullptr;
    return HOOK_SKIP_ORIGINAL;
}

}  // namespace

ModResult install_armor_audio_hooks(ModError* error) {
    const auto result = mods::hook::add_pre<IchigoArmorSoundHook>(svc_hook, before_link_sound);
    if (result != MOD_OK) {
        return mods::set_error(error, result, "failed to install Ichigo armor sound hook");
    }
    return MOD_OK;
}

}  // namespace ichigo
