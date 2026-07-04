/**
 * @file stub_physics_deps.cpp
 * @brief Stub implementations for PhysicsSystem tests — avoid linking Audio/Particles/Vulkan.
 * RELEASE_PLAN.md M1: тесты PhysicsSystem без полного рантайма движка.
 */

#include "audio/AudioSystem.h"
#include "particles/GpuParticleSystem.h"
#include "particles/ParticleSystem.h"

namespace kenga {

void AudioSystem::play_impact() {}

void ParticleSystem::emit_burst(const glm::vec3& /*position*/, int /*count*/,
                                const glm::vec4& /*color_start*/,
                                const glm::vec4& /*color_end*/,
                                float /*speed*/, float /*lifetime*/)
{
}

void GpuParticleSystem::emit_burst_typed(const glm::vec3& /*position*/, int /*count*/,
                                         int /*particle_type*/)
{
}

} // namespace kenga
