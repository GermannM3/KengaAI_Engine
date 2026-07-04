/**
 * @file GameSystem.h
 * @brief Demo game state: win/lose, restart, HUD
 *
 * PROJECT_RULES.md. Phase: Demo game.
 */

#pragma once

#include "ecs/ISystem.h"
#include "ecs/Registry.h"
#include "ecs/Entity.h"

#include <algorithm>

namespace kenga {

enum class GameState : int {
    menu,
    playing,
    won,
    lost,
};

class GameSystem : public ISystem {
public:
    void init(Registry& registry);
    void fixed_update(Registry& registry, double dt) override;
    void variable_update(Registry& registry, double dt) override;

    void set_camera_entity(Entity e) { m_camera_entity = e; }
    Entity camera_entity() const { return m_camera_entity; }

    GameState state() const { return m_state; }
    void set_state(GameState s) { m_state = s; }

    /// Current wave (1-based). When all enemies dead, advances; win when current_wave > waves_total.
    int current_wave() const { return m_current_wave; }
    int waves_total() const { return m_waves_total; }
    void set_waves_total(int n) { m_waves_total = std::max(1, n); }

    /// Score tracking
    int score() const { return m_score; }
    int enemies_killed() const { return m_enemies_killed; }
    void add_score(int points) { m_score += points; }
    void add_kill() { ++m_enemies_killed; }

    /// Restart demo game — reset player, wave, state; respawn is external (e.g. Application).
    void restart(Registry& registry);

    /// Draw HUD (health, ammo, wave, crosshair, win/lose overlay)
    void draw_hud(Registry& registry);

    /// Draw main menu
    void draw_menu();

private:
    Entity m_camera_entity = INVALID_ENTITY;
    GameState m_state = GameState::menu;
    int m_current_wave = 1;
    int m_waves_total = 10;
    bool m_had_enemies_this_wave = false;
    int m_score = 0;
    int m_enemies_killed = 0;

    int count_enemies(Registry& registry) const;
};

} // namespace kenga
