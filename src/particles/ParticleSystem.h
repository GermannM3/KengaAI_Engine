/**
 * @file ParticleSystem.h
 * @brief CPU particle system with ImGui overlay rendering
 *
 * PROJECT_RULES.md. Фаза 7: Particles + VFX.
 */

#pragma once

#include "ecs/Entity.h"
#include "ecs/ISystem.h"

#include <glm/glm.hpp>
#include <vector>

namespace kenga {

class Registry;

/// @brief Single particle (CPU-side)
struct Particle {
    glm::vec3 position = {0, 0, 0};
    glm::vec3 velocity = {0, 0, 0};
    glm::vec4 color_start = {1, 1, 1, 1};
    glm::vec4 color_end = {1, 1, 1, 0};
    float size_start = 4.0f;
    float size_end = 1.0f;
    float lifetime = 1.0f;       ///< total lifetime
    float age = 0.0f;            ///< current age
    bool alive = false;
};

/**
 * @brief Manages particle emission, simulation, and rendering.
 *
 * Particles are updated on CPU. Rendering uses ImGui foreground draw list
 * with world-to-screen projection (no extra Vulkan pipeline needed).
 */
class ParticleSystem : public ISystem {
public:
    static constexpr int MAX_PARTICLES = 4096;

    ParticleSystem();

    void fixed_update(Registry& registry, double dt) override;
    void variable_update(Registry& registry, double dt) override;

    /// Draw particles as ImGui overlay circles. Call inside ImGui frame.
    void draw_particles(const glm::mat4& view_proj, float screen_w, float screen_h);

    /// Emit a burst of particles at a world position (for collision sparks etc.)
    void emit_burst(const glm::vec3& position, int count,
                    const glm::vec4& color_start = {1.0f, 0.7f, 0.1f, 1.0f},
                    const glm::vec4& color_end = {1.0f, 0.1f, 0.0f, 0.0f},
                    float speed = 5.0f, float lifetime = 0.6f);

    int alive_count() const { return m_alive_count; }

private:
    Particle& get_free_particle();

    std::vector<Particle> m_particles;
    float m_emit_accumulator = 0.0f;
    int m_alive_count = 0;
};

} // namespace kenga
