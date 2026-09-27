#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cctype>
#include <iostream>
#include "../src/update_service.cpp"

ModContext* mod_ctx = nullptr;
namespace {
ConfigService config{};
HttpService http{};
LogService logService{};
UiService ui{};
HostService host{};
bool enabled = false;
ConfigChangedFn change = nullptr;
HttpCompleteFn complete = nullptr;
HttpRequestHandle nextRequest = 0, lastRequest = 0;
int requests = 0, cancellations = 0, dialogs = 0;
std::string requestUrl, requestPath, dataDir, dialogTitle;
UiDialogActionFn primaryAction = nullptr;
std::filesystem::path root;

std::string release_json(const char* tag = "v1.1.0") {
    return nlohmann::json{
        {"tag_name", tag}, {"draft", false}, {"prerelease", false},
        {"assets", {{{"name", "ichigo_mod.dusk"}, {"size", std::uint64_t(8)},
                    {"browser_download_url", std::string(ichigo::kAssetPrefix) + tag + "/ichigo_mod.dusk"}}}}
    }.dump();
}

void respond(int code, std::string body = {}, HttpError error = HTTP_ERROR_NONE) {
    HttpResult result{};
    result.error = error;
    result.status_code = code;
    result.body = body.data();
    result.body_size = body.size();
    result.download_path = requestPath.empty() ? nullptr : requestPath.c_str();
    complete(mod_ctx, lastRequest, &result, nullptr);
}

void toggle(bool value) {
    enabled = value;
    ConfigVarValue setting{};
    setting.bool_value = value;
    change(mod_ctx, 1, &setting, nullptr, nullptr);
}

void show_available() {
    respond(200, release_json());
    ichigo::s_detected -= std::chrono::seconds(2);
    ichigo::update_update_service();
    assert(dialogTitle == "Ichigo Mod Update Available");
}

void write(const std::filesystem::path& path, std::string_view data) {
    std::ofstream file(path, std::ios::binary);
    file.write(data.data(), static_cast<std::streamsize>(data.size()));
}

std::string read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}
}
const ConfigService* svc_config = &config;
const HttpService* svc_http = &http;
const LogService* svc_log = &logService;
const UiService* svc_ui = &ui;
const HostService* svc_host = &host;

int main() {
    using namespace ichigo;
    using namespace std::filesystem;
    assert(newer_release(release_json(), "1.0.0"));
    assert(!newer_release(release_json(), "1.1.0"));
    assert(!newer_release(release_json(), "2.0.0"));
    assert(!newer_release("not JSON", "1.0.0"));
    assert(!newer_release(release_json("v1.2.0-rc1"), "1.0.0"));
    assert(!version("1.2.3<script>"));
    assert(!version("1.2"));
    assert(!version("99999999999999999999.2.3"));
    auto metadata = nlohmann::json::parse(release_json());
    metadata["body"] = "\"tag_name\": \"v9.0.0\", \"browser_download_url\": \"evil\"";
    assert(newer_release(metadata.dump(), "1.0.0")->tag == "v1.1.0");
    metadata["assets"][0]["name"] = "other.dusk";
    assert(!newer_release(metadata.dump(), "1.0.0"));
    metadata = nlohmann::json::parse(release_json());
    metadata["assets"][0]["browser_download_url"] = "https://example.com/ichigo_mod.dusk";
    assert(!newer_release(metadata.dump(), "1.0.0"));
    metadata = nlohmann::json::parse(release_json());
    metadata["prerelease"] = true;
    assert(!newer_release(metadata.dump(), "1.0.0"));

    root = temp_directory_path() / ("ichigo-updater-test-" +
        std::to_string(Clock::now().time_since_epoch().count()));
    const auto data = root / "mod_data" / "dev.bezide.ichigo_mod";
    create_directories(data); create_directories(root / "mods");
    dataDir = data.string();
    const auto target = root / "mods" / kPackageName;
    write(target, "original");
    host.mod_version = +[](ModContext*) { return "1.0.0"; };
    host.data_dir = +[](ModContext*, const char** out) { *out = dataDir.c_str(); return MOD_OK; };
    logService.info = logService.error = +[](ModContext*, const char*) {};
    config.register_var = +[](ModContext*, const ConfigVarDesc* desc, ConfigVarHandle* out) {
        assert(!desc->default_bool); assert(std::string(desc->name) == "check-for-updates");
        *out = 1; return MOD_OK;
    };
    config.get_bool = +[](ModContext*, ConfigVarHandle, bool* out) { *out = enabled; return MOD_OK; };
    config.subscribe = +[](ModContext*, ConfigVarHandle, ConfigChangedFn fn, void*, ConfigSubscriptionHandle* out) {
        change = fn; *out = 1; return MOD_OK;
    };
    config.unsubscribe = +[](ModContext*, ConfigSubscriptionHandle) { return MOD_OK; };
    ui.register_mods_panel = +[](ModContext*, const UiModsPanelDesc*) { return MOD_OK; };
    ui.dialog_close = +[](ModContext*, UiDialogHandle) { return MOD_OK; };
    ui.push_toast = +[](ModContext*, const UiToastDesc*) { return MOD_OK; };
    ui.dialog_push = +[](ModContext*, const UiDialogDesc* desc, UiDialogHandle* out) {
        ++dialogs; dialogTitle = desc->title; primaryAction = desc->actions[0].on_pressed;
        *out = 1; return MOD_OK;
    };
    http.request = +[](ModContext*, const HttpRequestDesc* desc, HttpCompleteFn fn, void*, HttpRequestHandle* out) {
        // Dusklight owns these transport headers and rejects caller overrides.
        // Apply its constraint to both the release request and the download.
        for (std::uint32_t i = 0; i < desc->header_count; ++i) {
            std::string name = desc->headers[i].name;
            for (char& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            for (const char* reserved : {"user-agent", "host", "content-length", "connection",
                                         "accept-encoding", "range", "if-range"}) {
                if (name == reserved) return MOD_INVALID_ARGUMENT;
            }
        }
        ++requests; requestUrl = desc->url; requestPath = desc->download_path ? desc->download_path : "";
        complete = fn; lastRequest = *out = ++nextRequest; return MOD_OK;
    };
    http.cancel = +[](ModContext*, HttpRequestHandle) { ++cancellations; return MOD_OK; };

    assert(init_update_service() == MOD_OK);
    for (int i = 0; i < 100; ++i) update_update_service();
    assert(requests == 0 && dialogs == 0); // Off means no network request.
    toggle(true); assert(requests == 1 && requestUrl == kReleaseApi);
    start_check(true); assert(requests == 1); // Never duplicate an active check.
    show_available(); assert(dialogs == 1 && requests == 1 && read(target) == "original");
    update_update_service(); assert(dialogs == 1); // One prompt per check.
    primaryAction(mod_ctx, 1, nullptr); assert(requests == 2 && !requestPath.empty());
    write(requestPath, "broken"); respond(200); // Truncated download cannot replace original.
    assert(s_state == State::Failed && read(target) == "original");

    start_check(true); show_available(); primaryAction(mod_ctx, 1, nullptr);
    write(requestPath, "PK\003\004new!");
    respond(200, {}, HTTP_ERROR_NETWORK);
    assert(s_state == State::Failed && read(target) == "original");
    start_check(true); show_available(); primaryAction(mod_ctx, 1, nullptr);
    write(requestPath, "PK\003\004new!");
    toggle(false); assert(cancellations == 1);
    respond(200); assert(read(target) == "original"); // Canceled callback cannot install.
    toggle(true); respond(404); assert(s_state == State::Idle); // No releases yet.
    start_check(true); show_available(); primaryAction(mod_ctx, 1, nullptr);
    write(requestPath, "PK\003\004new!"); respond(200);
    assert(s_state == State::Installed && read(target) == "PK\003\004new!");
    update_update_service(); assert(dialogTitle == "Update Complete");
    assert(!exists(target.string() + ".update-backup"));
    shutdown_update_service();
    assert(init_update_service() == MOD_OK && s_state == State::Checking); // Persisted On.
    shutdown_update_service(); const int before = dialogs;
    respond(200, release_json()); update_update_service(); assert(dialogs == before);
    assert(read(target) == "PK\003\004new!");
    remove_all(root);
    std::cout << "Updater tests passed: releases, opt-in, confirmation, cancellation, install safety and reload.\n";
}
