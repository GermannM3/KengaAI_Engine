/**
 * @file FrustumCuller.h
 * @brief Frustum culling — extract planes from VP matrix, test AABB/sphere
 *
 * PROJECT_RULES.md. Phase 10: Optimization.
 */

#pragma once

#include <glm/glm.hpp>

#include <array>

namespace kenga {

/// @brief Six frustum planes extracted from a view-projection matrix
class FrustumCuller {
public:
    /// Extract frustum planes from a combined view*projection matrix
    void update(const glm::mat4& vp);

    /// Test if an axis-aligned bounding box is inside or intersects the frustum
    /// @param center  AABB center in world space
    /// @param extents AABB half-extents (positive)
    /// @return true if the AABB is at least partially inside the frustum
    bool is_aabb_visible(const glm::vec3& center, const glm::vec3& extents) const;

    /// Test if a sphere is inside or intersects the frustum
    bool is_sphere_visible(const glm::vec3& center, float radius) const;

private:
    // Plane: (nx, ny, nz, d) where nx*x + ny*y + nz*z + d >= 0 means inside
    std::array<glm::vec4, 6> m_planes;
};

} // namespace kenga
