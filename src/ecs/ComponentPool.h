/**
 * @file ComponentPool.h
 * @brief Sparse-set хранилище компонентов типа T
 *
 * PROJECT_RULES.md. Фаза 2.2: ECS — вдохновлён EnTT.
 */

#pragma once

#include "ecs/Entity.h"
#include "ecs/IComponentPool.h"

#include <cassert>
#include <cstdint>
#include <utility>
#include <vector>

namespace kenga {

constexpr std::uint32_t DENSE_INVALID = 0xFFFFFFFFu;

/**
 * @brief Пул компонентов с sparse-set маппингом
 */
template <typename T>
class ComponentPool : public IComponentPool {
public:
    void add(Entity e, T&& component);
    void remove(Entity e);
    T& get(Entity e);
    const T& get(Entity e) const;
    bool has(Entity e) const;

    void remove_entity(Entity e) override { remove(e); }

    std::vector<Entity> entities() const;

private:
    std::vector<std::uint32_t> m_sparse;
    std::vector<T> m_dense;
    std::vector<Entity> m_entity_dense;
};

template <typename T>
void ComponentPool<T>::add(Entity e, T&& component)
{
    const std::uint32_t idx = entity_index(e);
    if (idx >= m_sparse.size()) {
        m_sparse.resize(idx + 1, DENSE_INVALID);
    }
    if (m_sparse[idx] != DENSE_INVALID) {
        const std::uint32_t dense_i = m_sparse[idx];
        if (m_entity_dense[dense_i] == e) {
            m_dense[dense_i] = std::move(component);
            return;
        }
    }
    m_dense.push_back(std::forward<T>(component));
    m_entity_dense.push_back(e);
    m_sparse[idx] = static_cast<std::uint32_t>(m_dense.size() - 1);
}

template <typename T>
void ComponentPool<T>::remove(Entity e)
{
    const std::uint32_t idx = entity_index(e);
    if (idx >= m_sparse.size() || m_sparse[idx] == DENSE_INVALID) {
        return;
    }
    const std::uint32_t dense_i = m_sparse[idx];
    if (m_entity_dense[dense_i] != e) {
        return;
    }
    const std::uint32_t last = static_cast<std::uint32_t>(m_dense.size() - 1);
    if (dense_i != last) {
        m_dense[dense_i] = std::move(m_dense[last]);
        m_entity_dense[dense_i] = m_entity_dense[last];
        const std::uint32_t moved_idx = entity_index(m_entity_dense[dense_i]);
        m_sparse[moved_idx] = dense_i;
    }
    m_dense.pop_back();
    m_entity_dense.pop_back();
    m_sparse[idx] = DENSE_INVALID;
}

template <typename T>
T& ComponentPool<T>::get(Entity e)
{
    const std::uint32_t idx = entity_index(e);
    assert(idx < m_sparse.size() && m_sparse[idx] != DENSE_INVALID && "Invalid entity or missing component");
    const std::uint32_t dense_i = m_sparse[idx];
    assert(m_entity_dense[dense_i] == e && "Entity version mismatch");
    return m_dense[dense_i];
}

template <typename T>
const T& ComponentPool<T>::get(Entity e) const
{
    const std::uint32_t idx = entity_index(e);
    assert(idx < m_sparse.size() && m_sparse[idx] != DENSE_INVALID && "Invalid entity or missing component");
    const std::uint32_t dense_i = m_sparse[idx];
    assert(m_entity_dense[dense_i] == e && "Entity version mismatch");
    return m_dense[dense_i];
}

template <typename T>
bool ComponentPool<T>::has(Entity e) const
{
    const std::uint32_t idx = entity_index(e);
    if (idx >= m_sparse.size() || m_sparse[idx] == DENSE_INVALID) {
        return false;
    }
    return m_entity_dense[m_sparse[idx]] == e;
}

template <typename T>
std::vector<Entity> ComponentPool<T>::entities() const
{
    return m_entity_dense;
}

} // namespace kenga
