/**
 * @file Application.h
 * @brief Главный цикл приложения (fixed timestep)
 *
 * PROJECT_RULES.md. Фаза 2.1: Core — Game loop.
 */

#pragma once

#include "ecs/Entity.h"
#include "core/Settings.h"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <atomic>
#include <memory>

namespace kenga {

class PrefabManager;
class Registry;
class Renderer;
class SystemManager;

/**
 * @brief Основной класс приложения, управляет циклом и окном
 */
class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void run();

private:
    void init();
    void shutdown();

    void fixed_update(double dt);
    void update(double dt);
    void render(double alpha);
    void on_framebuffer_resize(int width, int height);

    /// Handle key input (Esc: pause in play mode, close in edit mode)
    void on_key(GLFWwindow* window, int key, int scancode, int action, int mods);

    /// Restart demo game — respawn enemies, reset player, wave 1
    void restart_demo();

    /// Spawn one wave of enemies (Arena One / M2)
    void spawn_wave(int wave_num);

    void init_imgui();
    void shutdown_imgui();
    VkDescriptorPool create_imgui_descriptor_pool();

    GLFWwindow* m_window = nullptr;
    int m_frame_count = 0;
    double m_fps_accumulator = 0.0;
    double m_fps_last_log = 0.0;
    int m_fixed_update_count = 0;

    Entity m_camera_entity = INVALID_ENTITY;
    VkDescriptorPool m_imgui_pool = VK_NULL_HANDLE;
    bool m_paused = false; ///< Pause overlay shown in play mode

    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<Registry> m_registry;
    std::unique_ptr<SystemManager> m_system_manager;
    std::unique_ptr<PrefabManager> m_prefab_manager;
    Settings m_settings; ///< Runtime configuration
    int m_last_spawned_wave = 0; ///< last wave we spawned (wave-based demo)
    std::atomic<bool> m_should_close{false}; ///< Set by signal handler for graceful shutdown

    static BOOL WINAPI signal_handler(DWORD ctrl_type);
};

} // namespace kenga
