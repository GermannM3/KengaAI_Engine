/**
 * @file AnimationSystem.h
 * @brief Skeletal animation system — samples keyframes, computes joint matrices
 *
 * PROJECT_RULES.md. Phase 9: Animation system.
 */

#pragma once

#include "ecs/ISystem.h"

namespace kenga {

class Registry;

/// @brief ECS system that updates SkinnedMesh components each frame
class AnimationSystem : public ISystem {
public:
    void fixed_update(Registry& registry, double dt) override;
    void variable_update(Registry& registry, double dt) override;
};

} // namespace kenga
