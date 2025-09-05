use anyhow::Result;
use kengaai_fps::{FpsController, FpsRenderer};
use kengaai_scene_fps::load_scene;
use log::{error, info, debug, trace};
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
    let level_path = args.get(1).cloned().unwrap_or_else(|| "assets/levels/minimal.json".to_string());
    
    info!("=== НАЧАЛО ДИАГНОСТИКИ ===");
    info!("Загрузка уровня: {}", level_path);
    
    // Попытка загрузить уровень
    let scene = match load_scene(&level_path) {
        Ok(s) => {
            info!("✓ Уровень успешно загружен");
            s
        },
        Err(e) => {
            error!("✗ Ошибка загрузки уровня: {:?}", e);
            return Err(e);
        }
    };
    
    info!("Создание event loop...");
    let event_loop = EventLoop::new().expect("event loop");
    
    info!("Создание окна...");
    let window = winit::window::WindowBuilder::new()
        .with_title("KengaAI Engine - DIAGNOSTICS")
        .with_inner_size(winit::dpi::LogicalSize::new(1280.0, 720.0))
        .build(&event_loop)
        .expect("window");
    
    info!("Настройка курсора...");
    window.set_cursor_visible(false);
    window.set_cursor_grab(winit::window::CursorGrabMode::Confined).ok();
    
    info!("Создание рендерера...");
    let window: &'static winit::window::Window = Box::leak(Box::new(window));
    
    let mut renderer = match FpsRenderer::new(window, &scene) {
        Ok(r) => {
            info!("✓ Рендерер успешно создан");
            r
        },
        Err(e) => {
            error!("✗ Ошибка создания ренDERERA: {:?}", e);
            return Err(e.into());
        }
    };
    
    info!("Инициализация контроллера...");
    let mut ctrl = FpsController::new(scene.player.r#move.speed, scene.player.r#move.run);
    renderer.set_clear(scene.render.clear_color);
    
    let start = Instant::now();
    let mut last = start;
    
    info!("=== ЗАПУСК ИГРОВОГО ЦИКЛА ===");
    
    let win_id = window.id();
    event_loop.run(move |event, target| {
        match event {
            Event::WindowEvent { event, window_id } if window_id == win_id => {
                match event {
                    WindowEvent::CloseRequested => {
                        info!("Получен запрос на закрытие");
                        target.exit();
                    },
                    WindowEvent::Resized(ns) => {
                        debug!("Изменение размера окна: {:?}x{:?}", ns.width, ns.height);
                        renderer.resize(ns);
                    },
                    WindowEvent::ScaleFactorChanged { .. } => { 
                        debug!("Изменение масштаба");
                    },
                    WindowEvent::MouseInput { state, button, .. } => {
                        if button == MouseButton::Left && state.is_pressed() {
                            info!("Выстрел!");
                        }
                    }
                    WindowEvent::KeyboardInput { event, .. } => {
                        let pressed = event.state == ElementState::Pressed;
                        match event.physical_key {
                            PhysicalKey::Code(KeyCode::Escape) if pressed => {
                                info!("Нажат ESC - выход");
                                target.exit();
                            },
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
                trace!("AboutToWait - запрос перерисовки");
                window.request_redraw();
            }
            Event::WindowEvent { event: WindowEvent::RedrawRequested, window_id } if window_id == win_id => {
                trace!("=== НАЧАЛО ОТРИСОВКИ КАДРА ===");
                
                let now = Instant::now();
                let dt = (now - last).as_secs_f32();
                last = now;
                
                debug!("Delta time: {:.4}ms", dt * 1000.0);
                
                // Обновление контроллера
                trace!("Обновление контроллера...");
                ctrl.step(&mut renderer.camera, dt);
                
                // Обновление камеры
                trace!("Обновление камеры...");
                renderer.update_camera();
                
                // Попытка рендеринга
                trace!("Попытка рендеринга...");
                match renderer.render() {
                    Ok(()) => {
                        trace!("✓ Кадр успешно отрисован");
                    },
                    Err(e) => {
                        error!("✗ Ошибка рендеринга: {:?}", e);
                    }
                }
                
                trace!("=== КОНЕЦ ОТРИСОВКИ КАДРА ===");
            }
            _ => {}
        }
    })?;
    
    info!("=== ЗАВЕРШЕНИЕ ДИАГНОСТИКИ ===");
    Ok(())
}