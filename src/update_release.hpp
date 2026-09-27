#pragma once

#include <array>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <nlohmann/json.hpp>

namespace ichigo {
inline constexpr char kPackageName[] = "ichigo_mod.dusk";
inline constexpr char kReleaseApi[] =
    "https://api.github.com/repos/BeZide93/dusklight-ichigo-mod/releases/latest";
inline constexpr char kAssetPrefix[] =
    "https://github.com/BeZide93/dusklight-ichigo-mod/releases/download/";

inline std::optional<std::array<unsigned, 3>> version(std::string_view text) {
    if (!text.empty() && (text.front() == 'v' || text.front() == 'V')) text.remove_prefix(1);
    std::array<unsigned, 3> parts{};
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (text.empty()) return std::nullopt;
        const auto end = text.data() + text.size();
        const auto [next, error] = std::from_chars(text.data(), end, parts[i]);
        if (error != std::errc{}) return std::nullopt;
        if (i == parts.size() - 1) {
            if (next != end) return std::nullopt;
        } else {
            if (next == end || *next != '.') return std::nullopt;
            text.remove_prefix(static_cast<std::size_t>(next - text.data()) + 1);
        }
    }
    return parts;
}

struct Release {
    std::string tag;
    std::string url;
    std::uint64_t size;
};

inline std::optional<Release> newer_release(std::string_view body, std::string_view current) {
    const auto json = nlohmann::json::parse(body, nullptr, false);
    if (!json.is_object() || !json.contains("tag_name") || !json["tag_name"].is_string() ||
        !json.contains("assets") || !json["assets"].is_array() ||
        !json.contains("draft") || json["draft"] != false ||
        !json.contains("prerelease") || json["prerelease"] != false) return std::nullopt;
    const auto tag = json["tag_name"].get<std::string>();
    const auto latest = version(tag), installed = version(current);
    if (!latest || !installed || *latest <= *installed) return std::nullopt;
    const std::string expected = std::string(kAssetPrefix) + tag + "/" + kPackageName;
    for (const auto& asset : json["assets"]) {
        if (!asset.is_object() || !asset.contains("name") || asset["name"] != kPackageName ||
            !asset.contains("browser_download_url") || asset["browser_download_url"] != expected ||
            !asset.contains("size") || !asset["size"].is_number_unsigned()) continue;
        const auto size = asset["size"].get<std::uint64_t>();
        if (size > 0) return Release{tag, expected, size};
    }
    return std::nullopt;
}
}
