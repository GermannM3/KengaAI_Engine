/**
 * @file VulkanContext.h
 * @brief Vulkan instance, device, queues, surface, debug messenger
 *
 * PROJECT_RULES.md. Фаза 3.1: Rendering — базовый Vulkan.
 */

#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace kenga {

class VulkanContext {
public:
    VulkanContext(GLFWwindow* window);
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    vk::Instance instance() const { return m_instance; }
    vk::PhysicalDevice physical_device() const { return m_physical_device; }
    vk::Device device() const { return m_device; }
    vk::Queue graphics_queue() const { return m_graphics_queue; }
    vk::Queue present_queue() const { return m_present_queue; }
    vk::SurfaceKHR surface() const { return m_surface; }
    uint32_t graphics_queue_family() const { return m_graphics_queue_family; }
    uint32_t present_queue_family() const { return m_present_queue_family; }

private:
    void create_instance(GLFWwindow* window);
    void setup_debug_messenger();
    void create_surface(GLFWwindow* window);
    void pick_physical_device();
    void create_logical_device();

    vk::Instance m_instance;
    vk::DebugUtilsMessengerEXT m_debug_messenger = VK_NULL_HANDLE;
    vk::SurfaceKHR m_surface;
    vk::PhysicalDevice m_physical_device;
    vk::Device m_device;
    vk::Queue m_graphics_queue;
    vk::Queue m_present_queue;
    uint32_t m_graphics_queue_family = 0;
    uint32_t m_present_queue_family = 0;
};

} // namespace kenga
