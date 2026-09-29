#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include "../src/model_settings.cpp"

ModContext* mod_ctx = nullptr;
namespace {
ConfigService config{};
OverlayService overlay{};
UiService ui{};
LogService logService{};
struct Variable {
    std::string key;
    ConfigChangedFn callback = nullptr;
    void* user = nullptr;
};
std::map<ConfigVarHandle, Variable> variables;
std::map<std::string, bool> saved;
std::map<OverlayHandle, std::string> active;
std::set<ConfigVarHandle> controls;
std::filesystem::path root;
unsigned nextConfig = 0, nextOverlay = 0;
bool failAdd = false, failRemove = false;
int failRegistration = -1;
int toasts = 0;
void toggle(ConfigVarHandle id, bool on) {
    auto& variable = variables.at(id);
    saved[variable.key] = on;
    ConfigVarValue value{};
    value.bool_value = on;
    variable.callback(mod_ctx, id, &value, nullptr, variable.user);
}
}
const ConfigService* svc_config = &config;
const OverlayService* svc_overlay = &overlay;
const UiService* svc_ui = &ui;
const LogService* svc_log = &logService;

int main(int argc, char** argv) {
    assert(argc == 2);
    root = argv[1];
    config.register_var = +[](ModContext*, const ConfigVarDesc* desc, ConfigVarHandle* out) {
        if (failRegistration == 0) return MOD_ERROR;
        if (failRegistration > 0) --failRegistration;
        assert(desc->type == CONFIG_VAR_BOOL && desc->default_bool);
        *out = ++nextConfig;
        variables[*out] = {desc->name};
        saved.try_emplace(desc->name, desc->default_bool);
        return MOD_OK;
    };
    config.get_bool = +[](ModContext*, ConfigVarHandle id, bool* value) {
        *value = saved.at(variables.at(id).key); return MOD_OK;
    };
    config.set_bool = +[](ModContext*, ConfigVarHandle id, bool value) {
        saved.at(variables.at(id).key) = value; return MOD_OK;
    };
    config.subscribe = +[](ModContext*, ConfigVarHandle id, ConfigChangedFn callback,
                          void* user, ConfigSubscriptionHandle* out) {
        variables.at(id).callback = callback; variables.at(id).user = user;
        *out = id; return MOD_OK;
    };
    config.unsubscribe = +[](ModContext*, ConfigSubscriptionHandle id) {
        variables.at(id).callback = nullptr; return MOD_OK;
    };
    config.unregister_var = +[](ModContext*, ConfigVarHandle id) {
        assert(variables.at(id).callback == nullptr);
        variables.erase(id); return MOD_OK;
    };
    overlay.add_file = +[](ModContext*, const char* disc, const char* bundle, OverlayHandle* out) {
        if (failAdd) return MOD_ERROR;
        assert(std::filesystem::is_regular_file(root / bundle));
        assert(std::string(disc) == "/res/" + std::string(bundle).substr(std::string("res/models/").size()));
        for (const auto& [_, path] : active) assert(path != disc);
        *out = ++nextOverlay; active[*out] = disc; return MOD_OK;
    };
    overlay.remove = +[](ModContext*, OverlayHandle id) {
        if (failRemove) return MOD_ERROR;
        assert(active.erase(id) == 1); return MOD_OK;
    };
    ui.pane_add_section = +[](ModContext*, UiElementHandle, const char*) { return MOD_OK; };
    ui.pane_add_text = +[](ModContext*, UiElementHandle, const char*, UiElementHandle*) { return MOD_OK; };
    ui.pane_add_control = +[](ModContext*, UiElementHandle, const UiControlDesc* desc, UiElementHandle*) {
        assert(desc->kind == UI_CONTROL_TOGGLE && desc->binding == UI_BINDING_CONFIG_VAR);
        assert(variables.count(desc->config_var));
        assert(controls.insert(desc->config_var).second); return MOD_OK;
    };
    ui.push_toast = +[](ModContext*, const UiToastDesc*) { ++toasts; return MOD_OK; };
    logService.error = +[](ModContext*, const char*) {};

    using namespace ichigo;
    std::set<std::string> assets, registered, keys;
    for (const auto& file : std::filesystem::recursive_directory_iterator(root / "res/models")) {
        if (file.path().extension() == ".bmd") assets.insert(std::filesystem::relative(file.path(), root).generic_string());
    }
    // Automatic overlays would make Off ineffective even if runtime removal works.
    if (std::filesystem::exists(root / "overlay")) {
        for (const auto& file : std::filesystem::recursive_directory_iterator(root / "overlay"))
            assert(file.path().extension() != ".bmd");
    }
    for (const auto& model : s_models) {
        assert(registered.insert(model.bundlePath).second);
        assert(keys.insert(model.key).second);
    }
    assert(assets == registered && assets.size() == 23);
    assert(init_model_settings() == MOD_OK && active.size() == assets.size());
    for (const char* group : {"Kmdl", "Bmdl", "Mmdl", "Zmdl"}) {
        assert(face_overlay_enabled(group));
    }
    // Sumo reuses the loaded clothing archive's face; alSumou has no face BMD.
    assert(!face_overlay_enabled("alSumou"));
    assert(!face_overlay_enabled("Wmdl") && !face_overlay_enabled(nullptr));
    assert(build_model_settings(mod_ctx, 1) == MOD_OK && controls.size() == assets.size());
    for (auto& model : s_models) {
        const auto old = active;
        toggle(model.config, false);
        assert(!model.overlay && active.size() + 1 == old.size());
        if (std::string(model.label).find("face.bmd") != std::string::npos) {
            assert(!face_overlay_enabled(model.group));
            for (const char* group : {"Kmdl", "Bmdl", "Mmdl", "Zmdl"}) {
                if (std::string(group) != model.group) assert(face_overlay_enabled(group));
            }
        }
        for (const auto& [id, path] : active) assert(old.at(id) == path);
        toggle(model.config, true);
        assert(model.overlay && active.size() == assets.size());
        for (const char* group : {"Kmdl", "Bmdl", "Mmdl", "Zmdl"}) {
            assert(face_overlay_enabled(group));
        }
        toggle(model.config, true); // Idempotent; no duplicated overlay.
    }
    // Sumo body/head/hand switches must not enable a disabled shared face.
    ModelSetting* casualFace = nullptr;
    for (auto& model : s_models) {
        if (std::string(model.key) == "model-bmdl-bmwr-al-face") casualFace = &model;
    }
    assert(casualFace);
    toggle(casualFace->config, false);
    unsigned sumoModels = 0;
    for (auto& model : s_models) {
        if (std::string(model.group) != "alSumou") continue;
        ++sumoModels;
        toggle(model.config, false);
        assert(!face_overlay_enabled("Bmdl") && face_overlay_enabled("Kmdl"));
        toggle(model.config, true);
        assert(!face_overlay_enabled("Bmdl") && face_overlay_enabled("Kmdl"));
    }
    assert(sumoModels == 3);
    toggle(casualFace->config, true);

    auto& first = s_models[0];
    failRemove = true; toggle(first.config, false); failRemove = false;
    assert(first.overlay && saved.at(first.key));
    toggle(first.config, false);
    failAdd = true; toggle(first.config, true); failAdd = false;
    assert(!first.overlay && !saved.at(first.key));
    // Different archives with the same filename must keep independent values.
    for (auto& model : s_models)
        if (std::string(model.group) == "Kmdl" && std::string(model.label) == "al_face.bmd") toggle(model.config, false);
    const auto count = active.size();
    shutdown_model_settings();
    assert(active.empty() && variables.empty());
    shutdown_model_settings();
    assert(init_model_settings() == MOD_OK && active.size() == count);
    for (const auto& model : s_models) assert((model.overlay != 0) == saved.at(model.key));
    shutdown_model_settings();
    failRegistration = 3;
    assert(init_model_settings() == MOD_ERROR);
    assert(active.empty() && variables.empty());
    assert(toasts > 0);
}
