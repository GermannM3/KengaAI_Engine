/**
 * @file Settings.cpp
 * @brief Runtime configuration implementation
 */

#include "core/Settings.h"
#include "core/LoggerMacros.h"

#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace kenga {

void Settings::load(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        KNG_INFO("No settings file found at '{}', using defaults", path);
        return;
    }

    try {
        json j = json::parse(file);

        // Window
        if (j.contains("window")) {
            const auto& w = j["window"];
            window_width = w.value("width", window_width);
            window_height = w.value("height", window_height);
            fullscreen = w.value("fullscreen", fullscreen);
            vsync = w.value("vsync", vsync);
        }

        // Rendering
        if (j.contains("rendering")) {
            const auto& r = j["rendering"];
            shadow_resolution = r.value("shadow_resolution", shadow_resolution);
            bloom_enabled = r.value("bloom", bloom_enabled);
            ssr_enabled = r.value("ssr", ssr_enabled);
            fog_enabled = r.value("fog", fog_enabled);
            god_rays_enabled = r.value("god_rays", god_rays_enabled);
        }

        // Audio
        if (j.contains("audio")) {
            const auto& a = j["audio"];
            master_volume = a.value("master", master_volume);
            music_volume = a.value("music", music_volume);
            sfx_volume = a.value("sfx", sfx_volume);
        }

        // Physics
        if (j.contains("physics")) {
            const auto& p = j["physics"];
            gravity = p.value("gravity", gravity);
        }

        KNG_INFO("Settings loaded from '{}'", path);
    } catch (const json::parse_error& e) {
        KNG_WARN("Failed to parse settings file '{}': {}", path, e.what());
    }
}

void Settings::save(const std::string& path) const
{
    json j;

    j["window"]["width"] = window_width;
    j["window"]["height"] = window_height;
    j["window"]["fullscreen"] = fullscreen;
    j["window"]["vsync"] = vsync;

    j["rendering"]["shadow_resolution"] = shadow_resolution;
    j["rendering"]["bloom"] = bloom_enabled;
    j["rendering"]["ssr"] = ssr_enabled;
    j["rendering"]["fog"] = fog_enabled;
    j["rendering"]["god_rays"] = god_rays_enabled;

    j["audio"]["master"] = master_volume;
    j["audio"]["music"] = music_volume;
    j["audio"]["sfx"] = sfx_volume;

    j["physics"]["gravity"] = gravity;

    std::ofstream file(path);
    if (file.is_open()) {
        file << j.dump(4);
        KNG_INFO("Settings saved to '{}'", path);
    } else {
        KNG_WARN("Failed to save settings to '{}'", path);
    }
}

} // namespace kenga
