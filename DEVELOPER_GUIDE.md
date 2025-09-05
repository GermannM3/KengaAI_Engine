# KengaAI Engine - Документация для разработчиков

## Введение

KengaAI Engine - это современный 3D игровой движок, разработанный на языке Rust с использованием графической библиотеки wgpu. Движок предоставляет широкий спектр возможностей для создания качественных 3D игр и интерактивных приложений.

## Начало работы

### Установка

1. Установите Rust toolchain с https://rustup.rs/
2. Установите Visual Studio Build Tools или Visual Studio Community
3. Клонируйте репозиторий:
   ```bash
   git clone https://github.com/your-repo/kengaai-engine.git
   cd kengaai-engine
   ```

### Структура проекта

```
kengaai-engine/
├── crates/
│   ├── scene_fps/          # Загрузка и управление сценами
│   ├── fps/                # Основной движок рендеринга
│   └── ...
├── demos/
│   ├── fps/                # Демо FPS
│   └── kengaquest/         # Демонстрационная игра
├── assets/
│   ├── levels/             # JSON файлы уровней
│   ├── textures/           # Текстуры
│   └── sounds/             # Звуковые файлы
├── studio/                 # Редактор уровней (Tauri/React)
└── ...
```

## Создание игры на KengaAI Engine

### 1. Создание нового проекта

Создайте новый бинарный крейт в директории `demos/`:

```bash
cd demos
cargo new my-game
```

Добавьте зависимости в `Cargo.toml`:

```toml
[package]
name = "my-game"
version = "0.1.0"
edition = "2021"

[dependencies]
kengaai-fps = { path = "../../crates/fps" }
kengaai-scene-fps = { path = "../../crates/scene_fps" }
winit = "0.29"
anyhow = "1"
env_logger = "0.11"
log = "0.4"
```

### 2. Базовая структура игры

```rust
use anyhow::Result;
use kengaai_fps::{FpsController, FpsRenderer};
use kengaai_scene_fps::load_scene;
use log::{error, info};
use std::env;
use std::time::Instant;
use winit::{
    event::{DeviceEvent, ElementState, Event, MouseButton, WindowEvent},
    event_loop::EventLoop,
    keyboard::{KeyCode, PhysicalKey},
};

fn main() -> Result<()> {
    env_logger::init();
    
    let args: Vec<String> = env::args().collect();
    let level_path = args.get(1).cloned().unwrap_or_else(|| "assets/levels/example.json".to_string());
    
    info!("Loading level: {}", level_path);
    let scene = load_scene(&level_path)?;
    
    let event_loop = EventLoop::new().expect("event loop");
    let window = winit::window::WindowBuilder::new()
        .with_title("My Game")
        .with_inner_size(winit::dpi::LogicalSize::new(1280.0, 720.0))
        .build(&event_loop)
        .expect("window");
    
    window.set_cursor_visible(false);
    window.set_cursor_grab(winit::window::CursorGrabMode::Confined).ok();
    
    let window: &'static winit::window::Window = Box::leak(Box::new(window));
    let mut renderer = FpsRenderer::new(window, &scene)?;
    
    let mut ctrl = FpsController::new(scene.player.r#move.speed, scene.player.r#move.run);
    renderer.set_clear(scene.render.clear_color);
    
    let start = Instant::now();
    let mut last = start;
    
    let win_id = window.id();
    event_loop.run(move |event, target| {
        match event {
            Event::WindowEvent { event, window_id } if window_id == win_id => {
                match event {
                    WindowEvent::CloseRequested => target.exit(),
                    WindowEvent::Resized(ns) => renderer.resize(ns),
                    WindowEvent::MouseInput { state, button, .. } => {
                        if button == MouseButton::Left && state.is_pressed() {
                            info!("Player shot!");
                        }
                    }
                    WindowEvent::KeyboardInput { event, .. } => {
                        let pressed = event.state == ElementState::Pressed;
                        match event.physical_key {
                            PhysicalKey::Code(KeyCode::Escape) if pressed => target.exit(),
                            PhysicalKey::Code(KeyCode::KeyW) => { ctrl.forward = pressed },
                            PhysicalKey::Code(KeyCode::KeyS) => { ctrl.back = pressed },
                            PhysicalKey::Code(KeyCode::KeyA) => { ctrl.left = pressed },
                            PhysicalKey::Code(KeyCode::KeyD) => { ctrl.right = pressed },
                            PhysicalKey::Code(KeyCode::ShiftLeft) => { ctrl.running = pressed },
                            _ => {}
                        }
                    }
                    _ => {}
                }
            }
            Event::DeviceEvent { event, .. } => {
                if let DeviceEvent::MouseMotion { delta } = event {
                    ctrl.mouse_delta.x += delta.0 as f32;
                    ctrl.mouse_delta.y += delta.1 as f32;
                }
            }
            Event::AboutToWait => {
                window.request_redraw();
            }
            Event::WindowEvent { event: WindowEvent::RedrawRequested, window_id } if window_id == win_id => {
                let now = Instant::now();
                let dt = (now - last).as_secs_f32();
                last = now;
                
                ctrl.step(&mut renderer.camera, dt);
                renderer.update_camera();
                
                if let Err(e) = renderer.render() {
                    error!("render: {e:?}");
                }
            }
            _ => {}
        }
    })?;
    Ok(())
}
```

### 3. Создание уровней

Уровни создаются в формате JSON. Пример структуры уровня:

```json
{
  "meta": {
    "schema": "KengaFPSSceneV0",
    "version": "0.1.0",
    "name": "my_level"
  },
  "render": {
    "clearColor": [0.05, 0.07, 0.09, 1.0]
  },
  "player": {
    "spawn": [0.0, 1.5, 4.0],
    "yaw": 3.1415,
    "pitch": 0.0,
    "move": {
      "speed": 4.5,
      "run": 7.5
    }
  },
  "weapons": [
    {
      "id": "rifle",
      "kind": "hitscan",
      "damage": 12.0,
      "rate": 6.0,
      "spread": 1.5
    }
  ],
  "level": {
    "boxes": [
      {
        "pos": [0.0, 0.0, 0.0],
        "size": [10.0, 0.5, 10.0],
        "rotY": 0.0,
        "color": [0.25, 0.25, 0.28],
        "texture": "floor.png"
      }
    ]
  },
  "lights": [
    {
      "kind": "point",
      "position": [0.0, 3.0, -2.0],
      "color": [1.0, 1.0, 1.0],
      "intensity": 1.0
    }
  ],
  "enemies": [
    {
      "kind": "grunt",
      "spawn": [2.0, 0.5, -3.0],
      "patrol": [[2.0, 0.5, -3.0], [-2.0, 0.5, -3.0]]
    }
  ],
  "triggers": [
    {
      "pos": [0.0, 0.5, -8.0],
      "size": [1.0, 1.0, 1.0],
      "onEnter": "spawn_wave:grunt:3"
    }
  ],
  "goals": {
    "type": "extract",
    "point": [0.0, 0.5, -8.0]
  }
}
```

## Основные возможности движка

### Рендеринг

KengaAI Engine предоставляет продвинутый рендеринг с поддержкой:

- Динамическое освещение (точечные и направленные источники)
- Тени
- Пост-обработка (bloom, HDR)
- Система частиц
- PBR (Physically Based Rendering) материалы
- Поддержка текстур

### Физика

Интеграция с физическим движком rapier3d:

- Реалистичная физика тел
- Система коллайдеров
- Физические свойства (масса, трение, упругость)

### Звук

Звуковая система на основе rodio:

- Пространственный звук
- Система аудио-источников
- Поддержка различных форматов (WAV, MP3, OGG)

### ИИ

Система поведения NPC:

- Патрулирование
- Преследование
- Избегание препятствий
- Навигационные сетки

## Использование редактора

KengaAI Studio - визуальный редактор уровней:

1. Запустите редактор:
   ```bash
   cd studio
   pnpm dev
   ```

2. Создавайте уровни визуально
3. Экспортируйте в JSON формат
4. Используйте в своих играх

## Расширение движка

### Добавление новых компонентов

Чтобы добавить новый компонент в сцену:

1. Обновите схему в `crates/scene_fps/src/lib.rs`
2. Добавьте поддержку рендеринга в `crates/fps/src/lib.rs`
3. Обновите шейдеры при необходимости

### Создание пользовательских систем

Для создания пользовательской системы:

1. Создайте новый крейт в `crates/`
2. Реализуйте необходимую логику
3. Интегрируйте с основным движком

## Лучшие практики

### Оптимизация производительности

- Используйте уровни детализации (LOD)
- Ограничивайте количество источников света
- Используйте occlusion culling
- Оптимизируйте текстуры

### Структура проекта

- Разделяйте логику игры и движка
- Используйте систему модулей
- Документируйте код
- Пишите тесты

## Поддержка и сообщество

- Документация: https://kengaai.github.io/docs
- GitHub: https://github.com/your-repo/kengaai-engine
- Discord: https://discord.gg/kengaai
- Форум: https://community.kengaai.com

## Лицензирование

KengaAI Engine распространяется под лицензией MIT. См. файл LICENSE для подробностей.

## Совместимость

### Поддерживаемые платформы

- Windows 10/11
- macOS 10.15+
- Linux (Ubuntu 20.04+, Fedora 32+)

### Системные требования

**Минимальные:**
- Процессор: Intel Core i5 или AMD Ryzen 5
- ОЗУ: 8 ГБ
- Видеокарта: DirectX 11 совместимая
- Место на диске: 2 ГБ

**Рекомендуемые:**
- Процессор: Intel Core i7 или AMD Ryzen 7
- ОЗУ: 16 ГБ
- Видеокарта: DirectX 12 совместимая
- Место на диске: 5 ГБ