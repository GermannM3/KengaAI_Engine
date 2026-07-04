/**
 * @file FrustumCuller.cpp
 * @brief Frustum plane extraction and AABB/sphere tests
 *
 * Uses Gribb-Hartmann method: extract planes directly from VP matrix rows.
 */

#include "rendering/FrustumCuller.h"

#include <cmath>

namespace kenga {

void FrustumCuller::update(const glm::mat4& vp)
{
    // Gribb-Hartmann: planes from rows of the transposed VP matrix
    // vp is column-major in glm, so vp[col][row]
    // Row i of the matrix = (vp[0][i], vp[1][i], vp[2][i], vp[3][i])

    auto row = [&](int i) -> glm::vec4 {
        return glm::vec4(vp[0][i], vp[1][i], vp[2][i], vp[3][i]);
    };

    // Left:   row3 + row0
    m_planes[0] = row(3) + row(0);
    // Right:  row3 - row0
    m_planes[1] = row(3) - row(0);
    // Bottom: row3 + row1
    m_planes[2] = row(3) + row(1);
    // Top:    row3 - row1
    m_planes[3] = row(3) - row(1);
    // Near:   row3 + row2
    m_planes[4] = row(3) + row(2);
    // Far:    row3 - row2
    m_planes[5] = row(3) - row(2);

    // Normalize each plane
    for (auto& p : m_planes) {
        const float len = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
        if (len > 0.0001f) {
            p /= len;
        }
    }
}

bool FrustumCuller::is_aabb_visible(const glm::vec3& center, const glm::vec3& extents) const
{
    for (const auto& plane : m_planes) {
        const glm::vec3 n(plane);
        // Compute the "positive vertex" distance — the AABB corner most aligned with the plane normal
        const float r = extents.x * std::abs(n.x)
                      + extents.y * std::abs(n.y)
                      + extents.z * std::abs(n.z);
        const float d = glm::dot(n, center) + plane.w;
        // If the farthest corner is behind the plane, the AABB is fully outside
        if (d + r < 0.0f) {
            return false;
        }
    }
    return true;
}

bool FrustumCuller::is_sphere_visible(const glm::vec3& center, float radius) const
{
    for (const auto& plane : m_planes) {
        const float d = glm::dot(glm::vec3(plane), center) + plane.w;
        if (d < -radius) {
            return false;
        }
    }
    return true;
}

} // namespace kenga
