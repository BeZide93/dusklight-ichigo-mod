#include "mods/svc/hook.hpp"
#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"
#include "service_imports.hpp"
#include "update_service.hpp"
#include "model_settings.hpp"
#include "eye_movement.hpp"
#include "hair_motion.hpp"

// Game includes
#include "d/d_item_data.h"
#include "f_op/f_op_actor_mng.h"

DEFINE_MOD();

IMPORT_SERVICE(LogService, svc_log);
IMPORT_SERVICE(HostService, svc_host);
IMPORT_SERVICE(HookService, svc_hook);
IMPORT_SERVICE(ConfigService, svc_config);
IMPORT_SERVICE(UiService, svc_ui);
IMPORT_SERVICE(HttpService, svc_http);
IMPORT_SERVICE(OverlayService, svc_overlay);
IMPORT_SERVICE(ResourceService, svc_resource);

// Example game hook: turn heart drops into green rupees.
DEFINE_HOOK(fopAcM_createItem, CreateItem);

static HookAction on_create_item_pre(ModContext*, void* args, void*, void*) {
    int& itemNo = mods::arg_ref<int>(args, 1);
    if (itemNo == dItemNo_HEART_e) {
        itemNo = dItemNo_GREEN_RUPEE_e;
    }
    return HOOK_CONTINUE;
}

extern "C" {
MOD_EXPORT ModResult mod_initialize(ModError* error) {
    // Installs a pre hook on fopAcM_createItem.
    ModResult result = mods::hook::add_pre<CreateItem>(on_create_item_pre);
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, "failed to install on_create_item_pre");
        return result;
    }

    svc_log->info(mod_ctx, "Ichigo Mod initialized");
    result = ichigo::init_model_settings();
    if (result != MOD_OK) return result;
    result = ichigo::install_eye_movement_hooks(error);
    if (result != MOD_OK) {
        ichigo::shutdown_model_settings();
        return result;
    }
    result = ichigo::install_hair_motion_hooks(error);
    if (result != MOD_OK) {
        ichigo::shutdown_model_settings();
        return result;
    }
    result = ichigo::init_update_service();
    if (result != MOD_OK) ichigo::shutdown_model_settings();
    return result;
}

MOD_EXPORT ModResult mod_update(ModError*) {
    ichigo::update_update_service();
    return MOD_OK;
}

MOD_EXPORT ModResult mod_shutdown(ModError*) {
    ichigo::shutdown_update_service();
    ichigo::shutdown_hair_motion();
    ichigo::shutdown_model_settings();
    return MOD_OK;
}
}
