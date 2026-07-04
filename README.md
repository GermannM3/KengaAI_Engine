# Kenga Engine

Игровой движок на C++20 / Vulkan / ECS с играбельной демо-игрой — мини-FPS: анимированный игрок, враги на Lua, стрельба, здоровье, победа/поражение и рестарт. См. PROJECT_RULES.md, ROADMAP.md и **RELEASE_PLAN.md** (план до коммерческого релиза и первой игры Arena One).

## Что внутри

- **Графика:** Vulkan, PBR + IBL + skybox, bloom, tone mapping, тени, мульти-лайт, скелетная анимация и скининг, объёмный туман, god rays, SSR
- **Физика:** Bullet3, rigid bodies, гравитация, коллизии, синхронизация с ECS
- **Аудио:** miniaudio — шаги, удары
- **Частицы:** GPU-частицы, выбросы при коллизии/raycast
- **Редактор:** ImGui — Hierarchy, Properties, Asset Browser, drag&drop, Save/Load сцен, префабы, режимы Play/Edit
- **Скриптинг:** Lua (sol2), hot-reload, sandboxed API для position/rotation/light/animation/damage
- **Демо-игра:** мини-FPS (CesiumMan, враги на Lua, стрельба, HUD, победа/поражение, restart)
- **Конфигурация:** Runtime settings.json для разрешения, fullscreen, vsync, графики, аудио
- **Безопасность:** Graceful shutdown (OS signals), bounds-checked JSON, sandboxed Lua (без I/O)

## Требования

- **ОС:** Windows 10/11 (x64)
- **Vulkan SDK** (драйверы и [LunarG SDK](https://vulkan.lunarg.com/))
- **CMake** 3.20+
- **Компилятор:** MSVC 2022 (или GCC 11+ / Clang 14+)
- **vcpkg** — зависимости (Bullet, GLFW, glm, Vulkan, ImGui, Lua, sol2, miniaudio, tinygltf, nlohmann-json, shaderc, spdlog, stb, **gtest** для тестов)

## Сборка

### 1. vcpkg

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg integrate install
```

### 2. Конфигурация и сборка

```powershell
cd c:\Kenga_X
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

**Debug (с validation layers и отладкой):**

```powershell
cmake --build build --config Debug
.\build\Debug\KengaEngine.exe
```

**Release (оптимизация, без validation, для релиза):**

```powershell
cmake --build build --config Release
.\build\Release\KengaEngine.exe
```

Если vcpkg установлен не в `C:\vcpkg`, укажи свой путь в `-DCMAKE_TOOLCHAIN_FILE=...`.

### 3. Тесты

```powershell
cmake --build build --config Release --target KengaEngineTests
ctest --test-dir build -C Release
```

Или запуск вручную: `.\build\Release\KengaEngineTests.exe`.

Текущий набор: **28 тестов** (ECS Registry, Physics, JSON helpers, SceneSerializer, PrefabManager).

### 4. Конфигурация

Движок загружает `config/settings.json` при старте. Если файл отсутствует — используются дефолты:

```json
{
    "window": { "width": 1280, "height": 720, "fullscreen": false, "vsync": true },
    "rendering": { "shadow_resolution": 2048, "bloom": true, "ssr": true, "fog": true, "god_rays": true },
    "audio": { "master": 1.0, "music": 0.5, "sfx": 1.0 },
    "physics": { "gravity": -9.81 }
}
```

### 5. Запуск демо

Exe ищет папки `assets/` и `shaders/` рядом с собой. При сборке CMake копирует их в `build/Debug` или `build/Release`. Запускай из корня проекта или из папки `build/<Config>/` после копирования туда `assets` и `shaders` (см. раздел «Упаковка релиза»).

## Управление

| Действие | Клавиша/мышь |
|----------|----------------|
| Движение | **W A S D** — вперёд/влево/назад/вправо |
| Высота | **Q** — вниз, **E** — вверх |
| Взгляд | **Мышь** |
| Курсор / камера | **Tab** — переключение (UI mode / захват мыши) |
| Wireframe | **F** |
| Стрельба (демо) | **ЛКМ** |
| Рестарт (демо) | **R** |
| Пауза (в Play mode) | **Esc** — показывает Resume / Back to Editor |
| Выход (в редакторе) | **Esc** |

В режиме редактора: Hierarchy и Properties в ImGui, перетаскивание префабов, Save/Load сцены, Play/Edit mode.

## Скриншоты

*(Добавь 3–5 скриншотов: редактор, сцена в игре, HUD, победа/поражение.)*

| Редактор | Игровой вид | HUD / победа |
|----------|-------------|--------------|
| *(screenshot_editor.png)* | *(screenshot_game.png)* | *(screenshot_hud.png)* |

## Видео демо

*(Ссылка на 30–60 сек ролик: запуск, редактор, play mode, стрельба, победа/поражение. Запись: OBS Studio или Windows Game Bar → mp4 → YouTube / Google Drive.)*

## Упаковка релиза

После сборки Release выполни из корня репозитория:

```powershell
.\scripts\pack_release.ps1
```

Скрипт создаёт папку `release/` и архив `KengaEngine-Demo.zip` с:

- `KengaEngine.exe`
- `assets/`, `shaders/` (модели, текстуры, звуки, скрипты, prefabs, sky.hdr, шейдеры)
- DLL: `fmt.dll`, `glfw3.dll`, `lua.dll`, `spdlog.dll`, `vulkan-1.dll`
- `README.md`, `README_PACKAGE.txt`

## Лицензия и контакты

См. репозиторий и PROJECT_RULES.md. Текущая фаза: финальный релиз демо-игры (v0.1).
