# ECS — Entity Component System

Фаза 2.2: sparse-set стиль (вдохновлён EnTT).

## Структура

- **Entity** — uint32_t, generational indexing
- **Registry** — create_entity, destroy_entity, add/remove/get/has, view
- **ComponentPool&lt;T&gt;**
- **IComponentPool** — type-erased интерфейс для remove_entity

## Использование

```cpp
Registry registry;
Entity e = registry.create_entity();
registry.add_component<Position>(e, Position{1, 2, 3});
registry.add_component<Velocity>(e, Velocity{0.5f, 0, 0});

for (Entity ent : registry.view<Position, Velocity>()) {
    auto& pos = registry.get_component<Position>(ent);
    auto& vel = registry.get_component<Velocity>(ent);
    pos.x += vel.x * dt;
}
```
