// Dawnlight's optional release-check / confirm / download / restart flow,
// adapted to the Ichigo package and Dusklight's managed asynchronous HTTP API.
#include "update_service.hpp"
#include "service_imports.hpp"
#include "update_release.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <utility>

namespace ichigo {
namespace {
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
enum class State { Idle, Checking, Available, Downloading, Installed, Failed };
State s_state = State::Idle;
bool s_enabled = false;
bool s_shown = false;
bool s_manual = false;
ConfigVarHandle s_setting = 0;
ConfigSubscriptionHandle s_subscription = 0;
HttpRequestHandle s_request = 0;
UiDialogHandle s_dialog = 0;
std::optional<Release> s_release;
std::string s_body;
fs::path s_downloadPath;
fs::path s_targetPath;
Clock::time_point s_detected{};

void fail(const char* message) {
    svc_log->error(mod_ctx, message);
    s_state = State::Failed;
    s_shown = false;
    s_body = message;
}

void dismiss(ModContext*, UiDialogHandle, void*) { s_dialog = 0; }

void cancel_pending() {
    const auto request = std::exchange(s_request, 0);
    if (request) svc_http->cancel(mod_ctx, request);
    const auto dialog = std::exchange(s_dialog, 0);
    if (dialog) svc_ui->dialog_close(mod_ctx, dialog);
}

HttpRequestDesc request_desc(const char* url) {
    static const HttpHeader headers[] = {
        {"User-Agent", "IchigoModUpdater"},
        {"Accept", "application/vnd.github+json"},
    };
    HttpRequestDesc desc = HTTP_REQUEST_DESC_INIT;
    desc.url = url;
    desc.headers = headers;
    desc.header_count = 2;
    desc.connect_timeout_ms = 10000;
    desc.idle_timeout_ms = 15000;
    desc.total_timeout_ms = 30000;
    return desc;
}

void checked(ModContext*, HttpRequestHandle request, const HttpResult* result, void*) {
    if (request != s_request || !s_enabled) return;
    s_request = 0;
    s_state = State::Idle;
    if (!result || result->error != HTTP_ERROR_NONE ||
        (result->status_code != 200 && result->status_code != 404)) {
        // Background checks do not interrupt play with an error dialog.
        svc_log->error(mod_ctx, "[Updater] Could not check Ichigo Mod releases.");
        if (s_manual) fail("Could not check for updates. Please try again later.");
        return;
    }
    if (result->status_code == 200 && result->body && result->body_size) {
        s_release = newer_release(
            std::string_view(static_cast<const char*>(result->body), result->body_size),
            svc_host->mod_version(mod_ctx));
    }
    if (s_release) {
        s_state = State::Available;
        s_detected = Clock::now();
        svc_log->info(mod_ctx, "[Updater] A newer Ichigo Mod release is available.");
    } else if (s_manual) {
        UiToastDesc toast = UI_TOAST_DESC_INIT;
        toast.title_rml = "Ichigo Mod";
        toast.body_rml = "No newer stable release with an Ichigo Mod bundle is available.";
        toast.duration_ms = 5000;
        svc_ui->push_toast(mod_ctx, &toast);
    }
}

void start_check(bool manual) {
    if (!s_enabled || s_request || s_state == State::Installed) return;
    cancel_pending();
    s_release.reset();
    s_shown = false;
    s_manual = manual;
    s_state = State::Checking;
    auto desc = request_desc(kReleaseApi);
    desc.max_body_bytes = 2 * 1024 * 1024;
    if (svc_http->request(mod_ctx, &desc, checked, nullptr, &s_request) != MOD_OK) {
        s_request = 0;
        s_state = State::Idle;
        svc_log->error(mod_ctx, "[Updater] Could not start release check.");
        if (manual) fail("Could not start the update check. Please try again later.");
    }
}

bool install_download() {
    std::error_code ec;
    if (!s_release || fs::file_size(s_downloadPath, ec) != s_release->size || ec) return false;
    // A release asset must be a complete ZIP-based .dusk, not an HTTP error page.
    {
        std::ifstream file(s_downloadPath, std::ios::binary);
        char signature[4]{};
        if (!file.read(signature, 4) || std::string_view(signature, 4) != "PK\003\004") return false;
    }
    if (!fs::is_regular_file(s_targetPath, ec) || ec) return false;
    fs::path backup = s_targetPath;
    backup += ".update-backup";
    if (fs::exists(backup, ec) || ec) return false;
    fs::rename(s_targetPath, backup, ec);
    if (ec) return false;
    fs::rename(s_downloadPath, s_targetPath, ec);
    if (ec) {
        std::error_code restoreError;
        fs::rename(backup, s_targetPath, restoreError);
        if (restoreError) svc_log->error(mod_ctx,
            "[Updater] Restore ichigo_mod.dusk.update-backup in the mods folder before restarting.");
        return false;
    }
    fs::remove(backup, ec);
    return true;
}

void downloaded(ModContext*, HttpRequestHandle request, const HttpResult* result, void*) {
    if (request != s_request || !s_enabled) return;
    s_request = 0;
    if (!result || result->error != HTTP_ERROR_NONE || result->status_code != 200 ||
        !result->download_path || !install_download()) {
        fail("The update could not be downloaded or installed. Please update Ichigo Mod manually.");
        return;
    }
    s_state = State::Installed;
    s_shown = false;
    svc_log->info(mod_ctx, "[Updater] Ichigo Mod updated. Restart Dusklight to load the new version.");
}

void confirm_download(ModContext*, UiDialogHandle, void*) {
    s_dialog = 0;
    if (!s_enabled || !s_release || s_state != State::Available || s_request) return;
    const char* dataDir = nullptr;
    if (svc_host->data_dir(mod_ctx, &dataDir) != MOD_OK || !dataDir || !*dataDir) {
        fail("The mod data folder is unavailable. Please update Ichigo Mod manually.");
        return;
    }
    // HostService uses <config>/mod_data/<mod-id>, including portable and mobile installs.
    // Stage the download inside our own data directory before replacing the confirmed package.
    const fs::path dataPath = fs::u8path(dataDir);
    s_downloadPath = dataPath / "ichigo-update.dusk";
    s_targetPath = dataPath.parent_path().parent_path() / "mods" / kPackageName;
    std::error_code ec;
    if (!fs::is_regular_file(s_targetPath, ec) || ec) {
        fail("The installed ichigo_mod.dusk could not be found. Please update renamed packages manually.");
        return;
    }
    const auto utf8 = s_downloadPath.u8string();
    const std::string downloadPath(utf8.begin(), utf8.end());
    auto desc = request_desc(s_release->url.c_str());
    desc.download_path = downloadPath.c_str();
    desc.total_timeout_ms = 300000;
    s_state = State::Downloading;
    if (svc_http->request(mod_ctx, &desc, downloaded, nullptr, &s_request) != MOD_OK) {
        s_request = 0;
        fail("Could not start the download. Please try again later.");
    }
}

void changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value,
             const ConfigVarValue*, void*) {
    s_enabled = value && value->bool_value;
    if (s_enabled) start_check(false);
    else {
        cancel_pending();
        if (s_state != State::Installed) s_state = State::Idle;
        s_release.reset();
        s_shown = false;
    }
}

void check_now(ModContext*, void*) { start_check(true); }
bool check_disabled(ModContext*, void*) {
    return !s_enabled || s_request != 0 || s_state == State::Installed;
}

ModResult build_panel(ModContext* ctx, UiElementHandle pane, void*, ModError*) {
    UiControlDesc toggle = UI_CONTROL_DESC_INIT;
    toggle.kind = UI_CONTROL_TOGGLE;
    toggle.label = "Check for Updates";
    toggle.help_rml = "<p>Check GitHub for new Ichigo Mod releases when enabled or loaded. "
                      "Downloading and installing an update requires confirmation.</p>";
    toggle.binding = UI_BINDING_CONFIG_VAR;
    toggle.config_var = s_setting;
    auto result = svc_ui->pane_add_control(ctx, pane, &toggle, nullptr);
    if (result != MOD_OK) return result;
    UiControlDesc button = UI_CONTROL_DESC_INIT;
    button.kind = UI_CONTROL_BUTTON;
    button.label = "Check Now";
    button.on_pressed = check_now;
    button.is_disabled = check_disabled;
    return svc_ui->pane_add_control(ctx, pane, &button, nullptr);
}
}

ModResult init_update_service() {
    s_enabled = false;
    s_shown = false;
    s_state = State::Idle;
    s_release.reset();
    ConfigVarDesc setting = CONFIG_VAR_DESC_INIT;
    setting.name = "check-for-updates";
    setting.default_bool = false;
    auto result = svc_config->register_var(mod_ctx, &setting, &s_setting);
    if (result != MOD_OK) return result;
    result = svc_config->get_bool(mod_ctx, s_setting, &s_enabled);
    if (result != MOD_OK) return result;
    result = svc_config->subscribe(mod_ctx, s_setting, changed, nullptr, &s_subscription);
    if (result != MOD_OK) return result;
    UiModsPanelDesc panel = UI_MODS_PANEL_DESC_INIT;
    panel.build = build_panel;
    result = svc_ui->register_mods_panel(mod_ctx, &panel);
    if (result != MOD_OK) return result;
    start_check(false);
    return MOD_OK;
}

void update_update_service() {
    if (!s_enabled || s_shown) return;
    UiDialogAction actions[2] = {UI_DIALOG_ACTION_INIT, UI_DIALOG_ACTION_INIT};
    actions[0].label = "OK";
    actions[0].on_pressed = dismiss;
    UiDialogDesc desc = UI_DIALOG_DESC_INIT;
    desc.on_dismiss = dismiss;
    desc.actions = actions;
    desc.action_count = 1;
    if (s_state == State::Available && s_release) {
        if (Clock::now() - s_detected < std::chrono::seconds(1)) return;
        // Accepted tags contain only a numeric version and optional v prefix.
        s_body = "A new version of <b>Ichigo Mod</b> is available: <b>" + s_release->tag +
                 "</b>.<br/><br/>Download and install the update now? "
                 "You will need to restart Dusklight afterwards.";
        desc.title = "Ichigo Mod Update Available";
        actions[0].label = "Update";
        actions[0].on_pressed = confirm_download;
        actions[1].label = "Later";
        actions[1].on_pressed = dismiss;
        desc.action_count = 2;
    } else if (s_state == State::Installed) {
        desc.title = "Update Complete";
        s_body = "Ichigo Mod has been updated. Restart Dusklight to load the new version.";
    } else if (s_state == State::Failed) {
        desc.title = "Update Failed";
        desc.variant = UI_DIALOG_DANGER;
    } else return;
    desc.body_rml = s_body.c_str();
    if (svc_ui->dialog_push(mod_ctx, &desc, &s_dialog) == MOD_OK) s_shown = true;
}

void shutdown_update_service() {
    s_enabled = false;
    cancel_pending();
    if (s_subscription) svc_config->unsubscribe(mod_ctx, std::exchange(s_subscription, 0));
    s_state = State::Idle;
    s_release.reset();
}
}
