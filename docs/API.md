# Kenga Engine — Обзор API

Краткий перечень модулей и основных классов для разработки игр и инструментов.

## Core

| Класс/модуль | Назначение |
|--------------|------------|
| `Application` | Главный цикл (fixed timestep), инициализация окна (GLFW), систем, рендера |
| `Time` | Время, delta_time, fixed_delta_time |
| `Input` | Состояние клавиш/мыши, привязка к окну |
| `InputSystem` | ECS-интеграция: камера, raycast, спавн префабов по клику |
| `LogManager` | Централизованный логгер (spdlog), макросы KNG_INFO, KNG_WARN и т.д. |
| `Profiler` | Замер времени по секциям (physics, render, update) |

## ECS

| Класс/тип | Назначение |
|-----------|------------|
| `Registry` | Создание/удаление сущностей, добавление/удаление/получение компонентов, `view<T...>` |
| `Entity` | Идентификатор сущности (generational index) |
| `View<Ts...>` | Итерация по сущностям с заданным набором компонентов |
| `ISystem` | Интерфейс системы: `fixed_update(Registry&, double dt)`, `variable_update(Registry&, double dt)` |
| `SystemManager` | Регистрация систем, вызов fixed/variable update по порядку |

Компоненты: `Position`, `Velocity`, `Rotation`, `Scale`, `Orientation`, `Camera`, `RigidBody`, `Light`, `ParticleEmitter`, `SkinnedMesh`, `Script`, `Player`, `Enemy` и др. — см. `ecs/Components.h`.

## Rendering

| Класс/модуль | Назначение |
|--------------|------------|
| `Renderer` / Vulkan-рендерер | Создание через фабрику, `init(window)`, `set_scene(registry, camera_entity, ...)`, основной цикл отрисовки |
| `VulkanContext` | Instance, device, queues, surface, validation (debug) |
| `Swapchain` | Окно, форматы, пересоздание при resize |
| PBR pipeline | Вершинный/фрагментный шейдеры, UBO (model, view, proj, lights), тени |
| `GltfMesh` | Загрузка glTF (tinygltf), меши и скелет/анимации |
| `PostProcess` | HDR, bloom, tone mapping |
| `GpuParticleSystem` | GPU-частицы (спарклы и т.д.) |

## Physics

| Класс | Назначение |
|-------|------------|
| `PhysicsSystem` | Bullet3: `init()`/`shutdown()`, `fixed_update()` (step + sync Position/Orientation), `world()` |
| | `create_rigid_body(registry, entity, mass, is_static, position)`, `remove_rigid_body`, `clear_all_bodies` |
| | `create_hinge(registry, entity_a, entity_b, pivot_a, pivot_b, axis_a, axis_b)` |

## Audio

| Класс | Назначение |
|-------|------------|
| `AudioSystem` | miniaudio: `init()`/`shutdown()`, `play_step()`, `play_impact()` (fire-and-forget) |

## Editor

| Класс | Назначение |
|------|------------|
| `EditorSystem` | Флаг видимости редактора, Play mode, выбранная сущность, `draw_ui(registry)` |
| `AssetBrowser` | Просмотр и перетаскивание ассетов в сцену |
| `PrefabManager` | Создание префабов из сущностей, спавн по имени и позиции |
| Сериализация сцены | `SceneSerializer::save_scene` / `load_scene` (JSON) |

## Scripting

| Класс | Назначение |
|------|------------|
| `LuaSystem` | Подключение Lua (sol2), скрипты по компоненту `Script`, вызовы `fixed_update`/`variable_update` из движка |

## Сборка и запуск

- Требования: CMake 3.20+, C++20, vcpkg с зависимостями из `vcpkg.json`.
- Сборка: `cmake -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake`, `cmake --build build --config Release`.
- Тесты: `ctest --test-dir build -C Release` или `build/Release/KengaEngineTests.exe`.
- Запуск движка: `build/Release/KengaEngine.exe` (шейдеры и ассеты копируются рядом с exe).

Подробнее: PROJECT_RULES.md, ARCHITECTURE.md, RELEASE_PLAN.md.
