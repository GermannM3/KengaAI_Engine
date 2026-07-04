/**
 * @file Renderer.h
 * @brief Абстрактный интерфейс рендерера
 *
 * PROJECT_RULES.md. Фаза 3.1: Rendering, 3.3: Camera.
 */

#pragma once

#include "ecs/Entity.h"
#include "ecs/Registry.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <memory>
#include <cstdint>

namespace kenga {

class GpuParticleSystem;
struct VfxSettings;

class Renderer {
public:
    virtual ~Renderer() = default;

    virtual void init(GLFWwindow* window) = 0;
    virtual void set_scene(Registry* registry, Entity camera_entity, Entity cube_entity,
                          Entity plane_entity) = 0;
    virtual void set_wireframe(bool wireframe) = 0;
    virtual void set_light_direction(const glm::vec3& dir) = 0;
    virtual void set_skybox_enabled(bool enabled) = 0;
    virtual void set_ibl_intensity(float intensity) = 0;
    virtual void render(double alpha) = 0;
    virtual void shutdown() = 0;
    virtual void on_resize(int width, int height) = 0;

    virtual VkInstance get_vk_instance() const = 0;
    virtual VkPhysicalDevice get_vk_physical_device() const = 0;
    virtual VkDevice get_vk_device() const = 0;
    virtual uint32_t get_graphics_queue_family() const = 0;
    virtual VkQueue get_vk_graphics_queue() const = 0;
    virtual VkRenderPass get_vk_render_pass() const = 0;
    virtual VkRenderPass get_vk_imgui_render_pass() const = 0;
    virtual uint32_t get_swapchain_image_count() const = 0;
    virtual VkCommandPool get_vk_command_pool() const = 0;

    /// Access the GPU particle system (may be null if not initialized)
    virtual GpuParticleSystem* get_gpu_particle_system() { return nullptr; }

    /// Access VFX settings (may be null if not initialized)
    virtual VfxSettings* get_vfx_settings() { return nullptr; }
};

std::unique_ptr<Renderer> create_vulkan_renderer();

} // namespace kenga
