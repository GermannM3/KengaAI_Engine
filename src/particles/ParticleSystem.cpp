/**
 * @file ParticleSystem.cpp
 * @brief CPU particle system implementation
 *
 * PROJECT_RULES.md. Фаза 7: Particles + VFX.
 */

#include "particles/ParticleSystem.h"
#include "ecs/Components.h"
#include "ecs/Registry.h"
#include "core/LoggerMacros.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <random>

namespace kenga {

static std::mt19937& rng()
{
    static std::mt19937 gen{std::random_device{}()};
    return gen;
}

static float rand_range(float lo, float hi)
{
    return std::uniform_real_distribution<float>(lo, hi)(rng());
}

ParticleSystem::ParticleSystem()
{
    m_particles.resize(MAX_PARTICLES);
}

Particle& ParticleSystem::get_free_particle()
{
    // Find first dead particle
    for (auto& p : m_particles) {
        if (!p.alive) return p;
    }
    // All alive — overwrite oldest (first in array)
    return m_particles[0];
}

void ParticleSystem::fixed_update(Registry& registry, double dt)
{
    const float fdt = static_cast<float>(dt);

    // Update existing particles
    m_alive_count = 0;
    for (auto& p : m_particles) {
        if (!p.alive) continue;
        p.age += fdt;
        if (p.age >= p.lifetime) {
            p.alive = false;
            continue;
        }
        // Simple gravity
        p.velocity.y -= 9.81f * fdt;
        p.position += p.velocity * fdt;
        ++m_alive_count;
    }

    // Emit from ParticleEmitter components
    for (const Entity e : registry.view<ParticleEmitter>()) {
        auto& emitter = registry.get_component<ParticleEmitter>(e);
        if (!emitter.active) continue;

        glm::vec3 world_pos = emitter.position_offset;
        if (registry.has_component<Position>(e)) {
            const auto& pos = registry.get_component<Position>(e);
            world_pos += glm::vec3(pos.x, pos.y, pos.z);
        }

        m_emit_accumulator += emitter.emit_rate * fdt;
        const int to_emit = static_cast<int>(m_emit_accumulator);
        m_emit_accumulator -= static_cast<float>(to_emit);

        for (int i = 0; i < to_emit; ++i) {
            Particle& p = get_free_particle();
            p.alive = true;
            p.age = 0.0f;
            p.lifetime = rand_range(emitter.lifetime_min, emitter.lifetime_max);
            p.position = world_pos;
            p.velocity = glm::vec3(
                rand_range(emitter.velocity_min.x, emitter.velocity_max.x),
                rand_range(emitter.velocity_min.y, emitter.velocity_max.y),
                rand_range(emitter.velocity_min.z, emitter.velocity_max.z)
            );
            p.color_start = emitter.color_start;
            p.color_end = emitter.color_end;
            p.size_start = emitter.size_start;
            p.size_end = emitter.size_end;
        }
    }
}

void ParticleSystem::variable_update(Registry& /*registry*/, double /*dt*/)
{
    // Particles update in fixed_update for determinism
}

void ParticleSystem::draw_particles(const glm::mat4& view_proj, float screen_w, float screen_h)
{
    ImDrawList* draw_list = ImGui::GetForegroundDrawList();
    if (!draw_list) return;

    const float half_w = screen_w * 0.5f;
    const float half_h = screen_h * 0.5f;

    for (const auto& p : m_particles) {
        if (!p.alive) continue;

        // World to clip space
        const glm::vec4 clip = view_proj * glm::vec4(p.position, 1.0f);
        if (clip.w <= 0.001f) continue; // behind camera

        const float ndc_x = clip.x / clip.w;
        const float ndc_y = clip.y / clip.w;
        const float ndc_z = clip.z / clip.w;

        // Cull outside NDC
        if (ndc_x < -1.2f || ndc_x > 1.2f || ndc_y < -1.2f || ndc_y > 1.2f || ndc_z < 0.0f || ndc_z > 1.0f) {
            continue;
        }

        // NDC to screen (Vulkan: Y is flipped by projection, so ndc_y maps correctly)
        const float sx = (ndc_x * 0.5f + 0.5f) * screen_w;
        const float sy = (ndc_y * 0.5f + 0.5f) * screen_h;

        // Interpolate properties by age/lifetime
        const float t = p.age / p.lifetime;
        const glm::vec4 color = glm::mix(p.color_start, p.color_end, t);
        const float size = glm::mix(p.size_start, p.size_end, t);

        // Perspective size scaling (closer = bigger)
        const float depth_scale = std::clamp(2.0f / clip.w, 0.1f, 10.0f);
        const float final_size = size * depth_scale;

        const ImU32 col = ImGui::ColorConvertFloat4ToU32(
            ImVec4(color.r, color.g, color.b, color.a));

        draw_list->AddCircleFilled(ImVec2(sx, sy), final_size, col);
    }
}

void ParticleSystem::emit_burst(const glm::vec3& position, int count,
                                const glm::vec4& color_start,
                                const glm::vec4& color_end,
                                float speed, float lifetime)
{
    for (int i = 0; i < count; ++i) {
        Particle& p = get_free_particle();
        p.alive = true;
        p.age = 0.0f;
        p.lifetime = rand_range(lifetime * 0.5f, lifetime * 1.5f);
        p.position = position;

        // Random direction sphere
        const float theta = rand_range(0.0f, 6.2831853f);
        const float phi = rand_range(-1.0f, 1.0f);
        const float r = std::sqrt(1.0f - phi * phi);
        p.velocity = glm::vec3(r * std::cos(theta), std::abs(phi) + 0.5f, r * std::sin(theta))
                     * rand_range(speed * 0.5f, speed * 1.5f);

        p.color_start = color_start;
        p.color_end = color_end;
        p.size_start = 4.0f;
        p.size_end = 1.0f;
    }
}

} // namespace kenga
