# Kenga Engine — Полная дорожная карта

> Следуй PROJECT_RULES.md строго. Проект — KengaEngine, AAA-движок на C++20/Vulkan/ECS.

---

## Обзор фаз

| Фаза | Название | Срок | Зависимости |
|------|----------|------|-------------|
| 1 | Подготовка и исследование | 1–2 нед | — |
| 2 | Core системы | 2–3 нед | Фаза 1 |
| 3 | Rendering system | 3–4 нед | Core |
| 4 | Physics и simulation | 2–3 нед | Rendering |
| 5 | Audio system | 1–2 нед | Core |
| 6 | Networking и multiplayer | 2–3 нед | Core |
| 7 | Asset management | 1–2 нед | Rendering |
| 8 | AI и gameplay systems | 2–3 нед | Physics |
| 9 | Editor и tools | 2–3 нед | Все предыдущие |
| 10 | Optimization и performance | 2–3 нед | Все |
| 11 | Platform support | 1–2 нед | Все |
| 12 | Testing, QA и релиз | 2–3 нед | Все |

---

## Фаза 1: Подготовка и исследование (1–2 недели)

**Цель:** Фундамент знаний и окружения.

### Задачи
- [ ] Изучить C++ advanced: RAII, smart pointers, templates, multithreading
- [ ] Установить инструменты: Visual Studio, Vulkan SDK, DirectX 12, CMake, vcpkg
- [ ] Спроектировать архитектуру: Game class, Managers (Renderer, Input, Physics), ECS
- [ ] Исследовать математику: glm

### Тест
Пустой CMake-проект с "Hello Engine".

### Результат
- CMake-проект на C++20 с vcpkg
- glm для математики
- Базовый main с логгингом (spdlog)

---

## Фаза 2: Core системы (2–3 недели)

**Цель:** Ядро движка.

### Задачи
- [ ] **Game loop:** Fixed timestep, update/render separation
- [ ] **Input system:** GLFW/SDL
- [ ] **Logging и error handling:** spdlog
- [ ] **Memory allocator:** Custom pool
- [ ] **Базовая ECS:** Entities с components (Position, Velocity)

### Тест
Консольная "игра" — движущийся квадрат.

### Зависимости
Фаза 1.

---

## Фаза 3: Rendering system (3–4 недели) ✅ ЗАВЕРШЕНА

**Цель:** Полноценный рендерер.

### Задачи
- [x] **Базовый renderer:** Vulkan, swapchain, шейдеры (shaderc GLSL→SPIR-V)
- [x] **Mesh loading:** glTF (tinygltf), textures (stb_image), UV-координаты
- [x] **PBR pipeline:** Metallic-roughness, albedo textures, push constants
- [x] **Lighting:** Directional/point/spot (до 8 источников), shadow mapping (PCF 3x3)
- [x] **Post-effects:** HDR offscreen, Bloom (threshold + Gaussian blur), ACES tone mapping
- [x] **Camera system:** Free/fly camera (WASD + mouse look)
- [x] **Depth buffer:** D32_SFLOAT в main render pass
- [x] **ImGui debug UI:** FPS, camera, wireframe, light direction slider

### Тест
PBR-куб с освещением, демо-прогулка, 3 типа источников света, bloom + tone mapping.

### Зависимости
Core.

---

## Фаза 4: Physics и simulation (2–3 недели)

**Цель:** Физика и симуляция.

### Задачи
- [x] **Physics engine:** Bullet3 (btDiscreteDynamicsWorld, ECS sync)
- [x] **Rigid bodies:** Collisions (manifolds → audio + particles), sync Position/Orientation
- [x] **Joints:** Hinge (btHingeConstraint) для дверей/маятников
- [ ] **Ragdolls:** (опционально, позже)
- [x] **Particle system:** CPU (ParticleSystem) + GPU (GpuParticleSystem), collision sparks
- [ ] **Simulation:** Weather, destructible (опционально)

### Тест
Физическая демо — ящики, стены, земля, спавн кубов по клику, hinge-демо.

### Зависимости
Rendering.

---

## Фаза 5: Audio system (1–2 недели)

**Цель:** Звук.

### Задачи
- [ ] **FMOD/Wwise:** 3D sound, effects
- [ ] **Spatial audio:** Reverb
- [ ] **Music manager**

### Тест
Звуки в демо.

### Зависимости
Core.

---

## Фаза 6: Networking и multiplayer (2–3 недели)

**Цель:** Мультиплеер.

### Задачи
- [ ] **Client-server:** UDP/TCP, latency compensation
- [ ] **Replication:** State sync, RPC
- [ ] **Anti-cheat**

### Тест
Мультиплеер-демо.

### Зависимости
Core.

---

## Фаза 7: Asset management и pipelines (1–2 недели)

**Цель:** Пайплайн ассетов.

### Задачи
- [ ] **Asset importer:** Models, animations (Assimp)
- [ ] **Streaming:** Level loading

### Тест
Загрузка модели.

### Зависимости
Rendering.

---

## Фаза 8: AI и gameplay systems (2–3 недели)

**Цель:** AI и геймплей.

### Задачи
- [ ] **Pathfinding:** A*, navmesh
- [ ] **Behavior trees:** FSM
- [ ] **Scripting:** Lua

### Тест
AI-боты в демо.

### Зависимости
Physics.

---

## Фаза 9: Editor и tools (2–3 недели)

**Цель:** Инструменты разработки.

### Задачи
- [ ] **ImGui/UI** для editing
- [ ] **Visual scripting**
- [ ] **Profiler, debugger**

### Тест
Runtime-редактирование.

### Зависимости
Все предыдущие.

---

## Фаза 10: Optimization и performance (2–3 недели)

**Цель:** Производительность.

### Задачи
- [ ] **Multithreading:** Job system
- [ ] **Memory/GPU profiling**
- [ ] **LOD, culling**

### Тест
Stress-test — тысячи объектов.

### Зависимости
Все.

---

## Фаза 11: Platform support и portability (1–2 недели)

**Цель:** Кроссплатформенность.

### Задачи
- [ ] **Cross-compile:** Windows/Linux/consoles
- [ ] **Mobile/VR**

### Тест
Запуск на разных ОС.

### Зависимости
Все.

---

## Фаза 12: Testing, QA и релиз (2–3 недели)

**Цель:** Релиз.

### Задачи
- [ ] **Unit/integration tests:** Google Test
- [ ] **Beta-testing:** Демо-игра
- [ ] **Documentation**
- [ ] **Release:** GitHub/open-source

### Зависимости
Все.

---

## Текущая фаза

**Фаза 4: Physics и simulation** (в работе)

Фаза 3 (Rendering) завершена. Фаза 4: Bullet3 интегрирован (rigid bodies, collisions, particles, hinge joints). Опционально: ragdolls, destructible, weather.

**План до релиза и первой игры:** см. **RELEASE_PLAN.md** (милестоуны M1–M5: тесты, Arena One, asset pipeline, релиз).
