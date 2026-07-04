/**
 * @file Registry.h
 * @brief Центральный менеджер ECS — сущности и компоненты
 *
 * PROJECT_RULES.md. Фаза 2.2: ECS — sparse-set.
 */

#pragma once

#include "ecs/ComponentPool.h"
#include "ecs/Entity.h"
#include "ecs/IComponentPool.h"

#include <iterator>
#include <memory>
#include <tuple>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace kenga {

/**
 * @brief Реестр сущностей и компонентов
 */
class Registry {
    template <typename... Us>
    friend class View;

public:
    Entity create_entity();
    void destroy_entity(Entity e);
    bool is_valid(Entity e) const;

    template <typename T>
    void add_component(Entity e, T&& component);

    template <typename T>
    void remove_component(Entity e);

    template <typename T>
    T& get_component(Entity e);

    template <typename T>
    const T& get_component(Entity e) const;

    template <typename T>
    bool has_component(Entity e) const;

    template <typename... Ts>
    View<Ts...> view();

    /// Return all currently valid entities
    std::vector<Entity> all_entities() const;

    /// Total number of entity slots (including freed)
    std::uint32_t entity_capacity() const { return static_cast<std::uint32_t>(m_versions.size()); }

private:
    template <typename T>
    ComponentPool<T>* get_or_create_pool();

    template <typename T>
    ComponentPool<T>* get_pool() const;

    std::vector<std::uint32_t> m_versions;
    std::vector<std::uint32_t> m_free_list;
    std::uint32_t m_next_version = 0; ///< next version for new/reused entities
    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> m_pools;
};

template <typename T>
void Registry::add_component(Entity e, T&& component)
{
    get_or_create_pool<T>()->add(e, std::forward<T>(component));
}

template <typename T>
void Registry::remove_component(Entity e)
{
    auto* pool = get_pool<T>();
    if (pool != nullptr) {
        pool->remove(e);
    }
}

template <typename T>
T& Registry::get_component(Entity e)
{
    return get_or_create_pool<T>()->get(e);
}

template <typename T>
const T& Registry::get_component(Entity e) const
{
    auto* pool = const_cast<Registry*>(this)->get_pool<T>();
    assert(pool != nullptr && "Component pool does not exist");
    return pool->get(e);
}

template <typename T>
bool Registry::has_component(Entity e) const
{
    const auto* pool = get_pool<T>();
    return pool != nullptr && pool->has(e);
}

template <typename T>
ComponentPool<T>* Registry::get_or_create_pool()
{
    const std::type_index key{typeid(T)};
    auto it = m_pools.find(key);
    if (it == m_pools.end()) {
        it = m_pools.emplace(key, std::make_unique<ComponentPool<T>>()).first;
    }
    return static_cast<ComponentPool<T>*>(it->second.get());
}

template <typename T>
ComponentPool<T>* Registry::get_pool() const
{
    const std::type_index key{typeid(T)};
    const auto it = m_pools.find(key);
    if (it == m_pools.end()) {
        return nullptr;
    }
    return static_cast<ComponentPool<T>*>(it->second.get());
}

// --- View (inline to avoid circular include) ---
template <typename RegistryRef, typename... Vs>
class ViewIterator {
public:
    using value_type = Entity;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::forward_iterator_tag;

    ViewIterator(RegistryRef registry, const std::vector<Entity>* entities, size_t index)
        : m_registry(registry)
        , m_entities(entities)
        , m_index(index)
    {
    }

    Entity operator*() const { return (*m_entities)[m_index]; }
    ViewIterator& operator++()
    {
        ++m_index;
        return *this;
    }
    ViewIterator operator++(int)
    {
        ViewIterator tmp = *this;
        ++m_index;
        return tmp;
    }
    bool operator!=(const ViewIterator& other) const { return m_index != other.m_index; }
    bool operator==(const ViewIterator& other) const { return m_index == other.m_index; }

private:
    RegistryRef m_registry;
    const std::vector<Entity>* m_entities;
    size_t m_index;
};

template <typename... Ts>
class View {
public:
    explicit View(Registry& registry)
        : m_registry(registry)
    {
        build_entities();
    }

    auto begin() const { return ViewIterator<Registry&, Ts...>(m_registry, &m_entities, 0); }
    auto end() const
    {
        return ViewIterator<Registry&, Ts...>(m_registry, &m_entities, m_entities.size());
    }
    bool empty() const { return m_entities.empty(); }
    size_t size() const { return m_entities.size(); }

private:
    void build_entities();

    Registry& m_registry;
    std::vector<Entity> m_entities;
};

template <typename... Ts>
void View<Ts...>::build_entities()
{
    if constexpr (sizeof...(Ts) == 0) {
        return;
    }
    using FirstT = std::tuple_element_t<0, std::tuple<Ts...>>;
    auto* pool = m_registry.template get_pool<FirstT>();
    if (pool == nullptr) {
        return;
    }
    for (const Entity e : pool->entities()) {
        if (!m_registry.is_valid(e)) {
            continue;
        }
        if ((m_registry.template has_component<Ts>(e) && ...)) {
            m_entities.push_back(e);
        }
    }
}

template <typename... Ts>
View<Ts...> Registry::view()
{
    return View<Ts...>(*this);
}

} // namespace kenga
