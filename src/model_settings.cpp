#include "model_settings.hpp"
#include "service_imports.hpp"
#include <cstring>

namespace ichigo {
namespace {
struct ModelSetting {
    const char* key;
    const char* group;
    const char* label;
    const char* discPath;
    const char* bundlePath;
    ConfigVarHandle config = 0;
    ConfigSubscriptionHandle subscription = 0;
    OverlayHandle overlay = 0;
};
#define ICHIGO_MODEL(key, group, label, path) {key, group, label, "/res/" path, "res/models/" path},
ModelSetting s_models[] = {
#include "model_overlays.inc"
};
#undef ICHIGO_MODEL

ModResult apply(ModelSetting& model, bool enabled) {
    if (enabled && model.overlay == 0) {
        return svc_overlay->add_file(mod_ctx, model.discPath, model.bundlePath, &model.overlay);
    }
    if (!enabled && model.overlay != 0) {
        const auto result = svc_overlay->remove(mod_ctx, model.overlay);
        if (result != MOD_OK) return result;
        model.overlay = 0;
    }
    return MOD_OK;
}

void changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value,
             const ConfigVarValue*, void* user) {
    if (!value || !user) return;
    auto& model = *static_cast<ModelSetting*>(user);
    const auto result = apply(model, value->bool_value);
    UiToastDesc toast = UI_TOAST_DESC_INIT;
    toast.title_rml = "Ichigo Model Settings";
    toast.duration_ms = 6000;
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, "Could not change Ichigo model overlay.");
        // ConfigService suppresses recursive notification of this same variable.
        svc_config->set_bool(mod_ctx, model.config, model.overlay != 0);
        toast.body_rml = "Could not apply this model setting. The previous setting was restored.";
    } else {
        toast.body_rml = "Setting saved. Restart Dusklight to reload models already in memory.";
    }
    svc_ui->push_toast(mod_ctx, &toast);
}
}

ModResult init_model_settings() {
    for (auto& model : s_models) {
        ConfigVarDesc desc = CONFIG_VAR_DESC_INIT;
        desc.name = model.key;
        desc.default_bool = true;
        auto result = svc_config->register_var(mod_ctx, &desc, &model.config);
        bool enabled = true;
        if (result == MOD_OK) result = svc_config->get_bool(mod_ctx, model.config, &enabled);
        if (result == MOD_OK) result = apply(model, enabled);
        if (result == MOD_OK) {
            result = svc_config->subscribe(mod_ctx, model.config, changed, &model, &model.subscription);
        }
        if (result != MOD_OK) {
            shutdown_model_settings();
            return result;
        }
    }
    return MOD_OK;
}

ModResult build_model_settings(ModContext* ctx, UiElementHandle pane) {
    auto result = svc_ui->pane_add_section(ctx, pane, "Model Overlays");
    if (result != MOD_OK) return result;
    result = svc_ui->pane_add_text(ctx, pane,
        "Each BMD can be enabled separately. All are On by default. "
        "Restart Dusklight after changes to reload cached models. "
        "Off uses the original game model, or a replacement supplied by another mod.", nullptr);
    if (result != MOD_OK) return result;
    const char* group = nullptr;
    for (const auto& model : s_models) {
        if (!group || std::strcmp(group, model.group) != 0) {
            group = model.group;
            result = svc_ui->pane_add_section(ctx, pane, group);
            if (result != MOD_OK) return result;
        }
        UiControlDesc toggle = UI_CONTROL_DESC_INIT;
        toggle.kind = UI_CONTROL_TOGGLE;
        toggle.label = model.label;
        toggle.help_rml = "<p>Toggle this Ichigo model replacement. Restart Dusklight after changing it.</p>";
        toggle.binding = UI_BINDING_CONFIG_VAR;
        toggle.config_var = model.config;
        result = svc_ui->pane_add_control(ctx, pane, &toggle, nullptr);
        if (result != MOD_OK) return result;
    }
    return MOD_OK;
}

void shutdown_model_settings() {
    for (auto& model : s_models) {
        if (model.subscription) svc_config->unsubscribe(mod_ctx, model.subscription);
        if (model.overlay) svc_overlay->remove(mod_ctx, model.overlay);
        if (model.config) svc_config->unregister_var(mod_ctx, model.config);
        model.subscription = 0;
        model.overlay = 0;
        model.config = 0;
    }
}
}
