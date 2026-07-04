/**
 * @file Entity.h
 * @brief Entity — идентификатор сущности (generational indexing)
 *
 * PROJECT_RULES.md. Фаза 2.2: ECS — sparse-set.
 */

#pragma once

#include <cstdint>

namespace kenga {

using Entity = std::uint32_t;

constexpr Entity INVALID_ENTITY = 0xFFFFFFFFu;

constexpr std::uint32_t ENTITY_INDEX_BITS = 20u;
constexpr std::uint32_t ENTITY_INDEX_MASK = (1u << ENTITY_INDEX_BITS) - 1u;
constexpr std::uint32_t ENTITY_VERSION_SHIFT = ENTITY_INDEX_BITS;

constexpr std::uint32_t entity_index(Entity e) { return e & ENTITY_INDEX_MASK; }
constexpr std::uint32_t entity_version(Entity e) { return e >> ENTITY_VERSION_SHIFT; }

} // namespace kenga
