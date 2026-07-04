/**
 * @file AudioSystem.h
 * @brief Audio system using miniaudio (header-only, fire-and-forget playback)
 *
 * PROJECT_RULES.md. Фаза 5: Audio.
 */

#pragma once

#include "ecs/ISystem.h"

#include <miniaudio.h>
#include <string>

namespace kenga {

/**
 * @brief Wraps miniaudio ma_engine for sound playback.
 *
 * Call init() after construction, shutdown() before destruction.
 * play_step() and play_impact() are fire-and-forget: miniaudio
 * handles mixing and playback on a background thread.
 */
class AudioSystem : public ISystem {
public:
    AudioSystem() = default;
    ~AudioSystem() override;

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    /// Initialize miniaudio engine. Returns false on failure.
    bool init();

    /// Shut down miniaudio engine and release resources.
    void shutdown();

    void fixed_update(Registry& registry, double dt) override;
    void variable_update(Registry& registry, double dt) override;

    /// Play footstep sound (fire-and-forget).
    void play_step();

    /// Play collision impact sound (fire-and-forget).
    void play_impact();

    /// Play shooting sound (fire-and-forget).
    void play_shoot();

    /// Play enemy death sound (fire-and-forget).
    void play_enemy_death();

    /// Play victory jingle (fire-and-forget).
    void play_victory();

    /// Play defeat sound (fire-and-forget).
    void play_defeat();

    /// Start background music (loops). Pass empty path to stop.
    void play_music(const std::string& path);

    /// Stop background music.
    void stop_music();

    bool is_initialized() const { return m_initialized; }

private:
    ma_engine m_engine{};
    bool m_initialized = false;
};

} // namespace kenga
