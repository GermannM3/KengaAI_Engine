/**
 * @file GameSystem.cpp
 * @brief Demo game state: menu, win/lose, HUD, restart, score
 */

#include "game/GameSystem.h"
#include "ecs/Components.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace kenga {

void GameSystem::init(Registry& registry)
{
    (void)registry;
}

void GameSystem::fixed_update(Registry& registry, double dt)
{
    (void)registry;
    (void)dt;
}

void GameSystem::variable_update(Registry& registry, double dt)
{
    (void)dt;

    if (m_state != GameState::playing) return;

    // Check player death
    if (m_camera_entity != INVALID_ENTITY && registry.has_component<Player>(m_camera_entity)) {
        const auto& player = registry.get_component<Player>(m_camera_entity);
        if (player.health <= 0.0f) {
            m_state = GameState::lost;
        }
    }

    // Wave advance: only when this wave was fought (had enemies) and now all dead
    const int enemies = count_enemies(registry);
    if (enemies > 0) m_had_enemies_this_wave = true;
    if (enemies == 0 && m_had_enemies_this_wave) {
        m_had_enemies_this_wave = false;
        ++m_current_wave;
        if (m_current_wave > m_waves_total) {
            m_state = GameState::won;
        }
    }
}

int GameSystem::count_enemies(Registry& registry) const
{
    int n = 0;
    for (const Entity e : registry.view<Enemy>()) {
        (void)e;
        ++n;
    }
    return n;
}

void GameSystem::restart(Registry& registry)
{
    m_state = GameState::playing;
    m_current_wave = 1;
    m_had_enemies_this_wave = false;
    m_score = 0;
    m_enemies_killed = 0;

    if (m_camera_entity != INVALID_ENTITY && registry.has_component<Player>(m_camera_entity)) {
        auto& player = registry.get_component<Player>(m_camera_entity);
        player.health = player.max_health;
        player.ammo = player.max_ammo;
    }
}

void GameSystem::draw_menu()
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(display.x, display.y));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.08f, 0.95f));

    ImGui::Begin("MainMenu", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoSavedSettings);

    // Title
    const float title_y = display.y * 0.25f;
    ImGui::SetCursorScreenPos(ImVec2(display.x * 0.5f - 160, title_y));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.1f, 1.0f));
    ImGui::SetWindowFontScale(3.0f);
    ImGui::Text("ARENA ONE");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    // Subtitle
    ImGui::SetCursorScreenPos(ImVec2(display.x * 0.5f - 100, title_y + 60));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
    ImGui::Text("Kenga Engine Demo");
    ImGui::PopStyleColor();

    // Play button
    const float btn_w = 200.0f;
    const float btn_h = 50.0f;
    const float btn_x = display.x * 0.5f - btn_w * 0.5f;
    const float btn_y = display.y * 0.55f;

    ImGui::SetCursorScreenPos(ImVec2(btn_x, btn_y));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
    if (ImGui::Button("PLAY", ImVec2(btn_w, btn_h))) {
        m_state = GameState::playing;
    }
    ImGui::PopStyleColor(2);

    // Controls info
    ImGui::SetCursorScreenPos(ImVec2(display.x * 0.5f - 120, btn_y + 80));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    ImGui::Text("WASD - Move | Mouse - Look");
    ImGui::SetCursorScreenPos(ImVec2(display.x * 0.5f - 120, btn_y + 100));
    ImGui::Text("LMB - Shoot | R - Restart");
    ImGui::SetCursorScreenPos(ImVec2(display.x * 0.5f - 120, btn_y + 120));
    ImGui::Text("Tab - Cursor | Esc - Pause");
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void GameSystem::draw_hud(Registry& registry)
{
    const float pad = 10.0f;
    const ImVec2 display = ImGui::GetIO().DisplaySize;

    // Fullscreen overlay — no window
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(display.x, display.y));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::Begin("GameHUD", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);

    if (m_state == GameState::playing) {
        // Health bar (top-left)
        if (m_camera_entity != INVALID_ENTITY && registry.has_component<Player>(m_camera_entity)) {
            const auto& player = registry.get_component<Player>(m_camera_entity);
            const float bar_w = 200.0f;
            const float bar_h = 20.0f;
            ImVec2 tl(pad, pad);

            // Background
            ImGui::GetWindowDrawList()->AddRectFilled(
                tl, ImVec2(tl.x + bar_w, tl.y + bar_h),
                IM_COL32(80, 40, 40, 200), 3.0f);
            // Fill
            const float fill = std::clamp(player.health / player.max_health, 0.0f, 1.0f);
            ImU32 health_color = fill > 0.5f ? IM_COL32(60, 200, 60, 255) :
                                 fill > 0.25f ? IM_COL32(200, 200, 60, 255) :
                                                IM_COL32(200, 60, 60, 255);
            ImGui::GetWindowDrawList()->AddRectFilled(
                tl, ImVec2(tl.x + bar_w * fill, tl.y + bar_h),
                health_color, 3.0f);

            // Stats text
            ImGui::SetCursorScreenPos(ImVec2(tl.x + bar_w + pad, tl.y));
            ImGui::TextColored(ImVec4(1, 1, 1, 1), "HP: %.0f / %.0f  Ammo: %d / %d",
                              player.health, player.max_health, player.ammo, player.max_ammo);

            // Wave and score (top-right)
            ImGui::SetCursorScreenPos(ImVec2(display.x - 200.0f, pad));
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Wave %d / %d", m_current_wave, m_waves_total);
            ImGui::SetCursorScreenPos(ImVec2(display.x - 200.0f, pad + 20));
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Score: %d  Kills: %d", m_score, m_enemies_killed);

            // Enemy count (top-center)
            const int enemies_left = count_enemies(registry);
            ImGui::SetCursorScreenPos(ImVec2(display.x * 0.5f - 50.0f, pad));
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Enemies: %d", enemies_left);

            // Controls hint
            ImGui::SetCursorScreenPos(ImVec2(pad, display.y - 24.0f));
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 0.8f), "Esc - Pause | Tab - Cursor | R - Restart");
        }

        // Crosshair (center)
        const float cx = display.x * 0.5f;
        const float cy = display.y * 0.5f;
        const float size = 8.0f;
        ImU32 cross_color = IM_COL32(255, 255, 255, 220);
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddLine(ImVec2(cx - size, cy), ImVec2(cx + size, cy), cross_color, 2.0f);
        draw->AddLine(ImVec2(cx, cy - size), ImVec2(cx, cy + size), cross_color, 2.0f);
        // Center dot
        draw->AddCircleFilled(ImVec2(cx, cy), 2.0f, IM_COL32(255, 80, 80, 255));

    } else if (m_state == GameState::won) {
        // Victory screen with stats
        ImGui::SetCursorScreenPos(ImVec2(display.x * 0.5f - 120, display.y * 0.3f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.3f, 1.0f));
        ImGui::SetWindowFontScale(3.0f);
        ImGui::Text("VICTORY!");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(ImVec2(display.x * 0.5f - 80, display.y * 0.45f));
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Score: %d", m_score);
        ImGui::SetCursorScreenPos(ImVec2(display.x * 0.5f - 80, display.y * 0.50f));
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Enemies killed: %d", m_enemies_killed);

        ImGui::SetCursorScreenPos(ImVec2(display.x * 0.5f - 80, display.y * 0.60f));
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Press R to restart");

    } else if (m_state == GameState::lost) {
        // Game over screen with stats
        ImGui::SetCursorScreenPos(ImVec2(display.x * 0.5f - 140, display.y * 0.3f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
        ImGui::SetWindowFontScale(3.0f);
        ImGui::Text("GAME OVER");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(ImVec2(display.x * 0.5f - 80, display.y * 0.45f));
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Score: %d", m_score);
        ImGui::SetCursorScreenPos(ImVec2(display.x * 0.5f - 80, display.y * 0.50f));
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Wave reached: %d / %d", m_current_wave, m_waves_total);

        ImGui::SetCursorScreenPos(ImVec2(display.x * 0.5f - 80, display.y * 0.60f));
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Press R to restart");
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

} // namespace kenga
