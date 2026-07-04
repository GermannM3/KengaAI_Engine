/**
 * @file Registry.cpp
 * @brief Реализация Registry
 */

#include "ecs/Registry.h"
#include "core/LogManager.h"
#include "core/LoggerMacros.h"

#include <cassert>

namespace kenga {

Entity Registry::create_entity()
{
    std::uint32_t index;
    std::uint32_t version;

    if (!m_free_list.empty()) {
        index = m_free_list.back();
        m_free_list.pop_back();
        version = m_next_version++;
    } else {
        index = static_cast<std::uint32_t>(m_versions.size());
        m_versions.push_back(0);
        version = m_next_version++;
    }

    const Entity e = (version << ENTITY_VERSION_SHIFT) | index;
    m_versions[index] = version + 1;

    KNG_DEBUG("Created entity {}", static_cast<unsigned>(e));
    return e;
}

void Registry::destroy_entity(Entity e)
{
    if (e == INVALID_ENTITY || !is_valid(e)) {
        return;
    }

    const std::uint32_t index = entity_index(e);
    const std::uint32_t version = entity_version(e);

    if (m_versions[index] != version + 1) {
        KNG_WARN("destroy_entity: entity {} already destroyed or invalid", static_cast<unsigned>(e));
        return;
    }

    for (auto& [_, pool] : m_pools) {
        pool->remove_entity(e);
    }

    KNG_DEBUG("Destroyed entity {}", static_cast<unsigned>(e));
    m_versions[index] = 0; // mark slot free; is_valid(e) is false
    m_free_list.push_back(index);
}

bool Registry::is_valid(Entity e) const
{
    if (e == INVALID_ENTITY) {
        return false;
    }
    const std::uint32_t index = entity_index(e);
    const std::uint32_t version = entity_version(e);
    if (index >= m_versions.size()) {
        return false;
    }
    return m_versions[index] == version + 1;
}

std::vector<Entity> Registry::all_entities() const
{
    std::vector<Entity> result;
    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(m_versions.size()); ++i) {
        // Version stored is (creation_version + 1). Reconstruct entity with creation_version.
        const std::uint32_t stored = m_versions[i];
        if (stored == 0) continue; // never created
        const std::uint32_t version = stored - 1;
        const Entity e = (version << ENTITY_VERSION_SHIFT) | i;
        if (is_valid(e)) {
            result.push_back(e);
        }
    }
    return result;
}

} // namespace kenga
