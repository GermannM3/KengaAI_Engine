/**
 * @file AudioSystem.cpp
 * @brief miniaudio-based audio system implementation
 *
 * PROJECT_RULES.md. Фаза 5: Audio.
 */

#define MINIAUDIO_IMPLEMENTATION
#include "audio/AudioSystem.h"
#include "core/LogManager.h"
#include "core/LoggerMacros.h"

namespace kenga {

AudioSystem::~AudioSystem()
{
    shutdown();
}

bool AudioSystem::init()
{
    ma_engine_config config = ma_engine_config_init();
    ma_result result = ma_engine_init(&config, &m_engine);
    if (result != MA_SUCCESS) {
        KNG_WARN("AudioSystem: ma_engine_init failed (error {})", static_cast<int>(result));
        return false;
    }
    m_initialized = true;
    KNG_INFO("AudioSystem initialized (miniaudio)");
    return true;
}

void AudioSystem::shutdown()
{
    if (!m_initialized) {
        return;
    }
    ma_engine_uninit(&m_engine);
    m_initialized = false;
    KNG_INFO("AudioSystem shutdown");
}

void AudioSystem::fixed_update(Registry& /*registry*/, double /*dt*/)
{
    // miniaudio runs its own mixing thread, nothing to do here
}

void AudioSystem::variable_update(Registry& /*registry*/, double /*dt*/)
{
    // miniaudio runs its own mixing thread, nothing to do here
}

void AudioSystem::play_step()
{
    if (!m_initialized) return;
    ma_engine_play_sound(&m_engine, "assets/sounds/step.wav", nullptr);
}

void AudioSystem::play_impact()
{
    if (!m_initialized) return;
    ma_engine_play_sound(&m_engine, "assets/sounds/impact.wav", nullptr);
}

void AudioSystem::play_shoot()
{
    if (!m_initialized) return;
    ma_engine_play_sound(&m_engine, "assets/sounds/shoot.wav", nullptr);
}

void AudioSystem::play_enemy_death()
{
    if (!m_initialized) return;
    ma_engine_play_sound(&m_engine, "assets/sounds/enemy_death.wav", nullptr);
}

void AudioSystem::play_victory()
{
    if (!m_initialized) return;
    ma_engine_play_sound(&m_engine, "assets/sounds/victory.wav", nullptr);
}

void AudioSystem::play_defeat()
{
    if (!m_initialized) return;
    ma_engine_play_sound(&m_engine, "assets/sounds/defeat.wav", nullptr);
}

void AudioSystem::play_music(const std::string& path)
{
    if (!m_initialized || path.empty()) return;
    ma_engine_play_sound(&m_engine, path.c_str(), nullptr);
}

void AudioSystem::stop_music()
{
    // miniaudio fire-and-forget doesn't support stopping individual sounds
    // This would require ma_sound objects instead of ma_engine_play_sound
}

} // namespace kenga
