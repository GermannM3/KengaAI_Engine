/**
 * @file Settings.h
 * @brief Runtime configuration loaded from settings.json
 */

#pragma once

#include <string>

namespace kenga {

struct Settings {
    // Window
    int window_width = 1280;
    int window_height = 720;
    bool fullscreen = false;
    bool vsync = true;

    // Rendering
    int shadow_resolution = 2048;
    bool bloom_enabled = true;
    bool ssr_enabled = true;
    bool fog_enabled = true;
    bool god_rays_enabled = true;

    // Audio
    float master_volume = 1.0f;
    float music_volume = 0.5f;
    float sfx_volume = 1.0f;

    // Physics
    float gravity = -9.81f;

    /// Load settings from JSON file. Missing fields use defaults.
    void load(const std::string& path);

    /// Save current settings to JSON file.
    void save(const std::string& path) const;
};

} // namespace kenga
