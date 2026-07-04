/**
 * @file VulkanContext.cpp
 * @brief Реализация VulkanContext
 */

#include "rendering/VulkanContext.h"
#include "core/LogManager.h"
#include "core/LoggerMacros.h"

#include <cstdlib>
#include <set>
#include <stdexcept>

namespace kenga {

#ifdef NDEBUG
constexpr bool enable_validation_layers = false;
#else
constexpr bool enable_validation_layers = true;
#endif

constexpr std::array<const char*, 1> validation_layers = {"VK_LAYER_KHRONOS_validation"};

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* data, void*)
{
    const char* msg = data->pMessage;
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        KNG_ERROR("[Vulkan] {}", msg);
    } else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        KNG_WARN("[Vulkan] {}", msg);
    } else {
        KNG_DEBUG("[Vulkan] {}", msg);
    }
    return VK_FALSE;
}

static bool check_validation_layer_support()
{
    const auto available = vk::enumerateInstanceLayerProperties();
    for (const char* layer : validation_layers) {
        bool found = false;
        for (const auto& prop : available) {
            if (strcmp(layer, prop.layerName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

VulkanContext::VulkanContext(GLFWwindow* window)
{
    create_instance(window);
    if (enable_validation_layers && check_validation_layer_support()) {
        setup_debug_messenger();
    }
    create_surface(window);
    pick_physical_device();
    create_logical_device();
}

VulkanContext::~VulkanContext()
{
    if (m_device) {
        m_device.destroy();
    }
    if (m_surface && m_instance) {
        m_instance.destroySurfaceKHR(m_surface);
    }
    if (enable_validation_layers && m_debug_messenger && m_instance) {
        const auto destroy_fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(static_cast<VkInstance>(m_instance),
                                 "vkDestroyDebugUtilsMessengerEXT"));
        if (destroy_fn) {
            destroy_fn(static_cast<VkInstance>(m_instance), m_debug_messenger, nullptr);
        }
    }
    if (m_instance) {
        m_instance.destroy();
    }
}

void VulkanContext::create_instance(GLFWwindow* window)
{
    bool use_validation = enable_validation_layers && check_validation_layer_support();
    if (enable_validation_layers && !use_validation) {
        KNG_WARN("Validation layers requested but not available (install Vulkan SDK for debug)");
    }

    vk::ApplicationInfo app_info;
    app_info.pApplicationName = "Kenga Engine";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName = "KengaEngine";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;

    uint32_t glfw_ext_count = 0;
    const char** glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
    std::vector<const char*> extensions(glfw_exts, glfw_exts + glfw_ext_count);
    if (use_validation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    vk::InstanceCreateInfo create_info;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();
    if (use_validation) {
        create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
        create_info.ppEnabledLayerNames = validation_layers.data();
    }

    vk::Result result = vk::createInstance(&create_info, nullptr, &m_instance);
    if (result != vk::Result::eSuccess) {
        KNG_CRITICAL("createVulkanInstance: {} at {}", vk::to_string(result), __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }
}

void VulkanContext::setup_debug_messenger()
{
    const auto create_fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(static_cast<VkInstance>(m_instance),
                             "vkCreateDebugUtilsMessengerEXT"));
    if (!create_fn) {
        KNG_CRITICAL("vkCreateDebugUtilsMessengerEXT not found at {}", __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }

    VkDebugUtilsMessengerCreateInfoEXT create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = debug_callback;

    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
    if (create_fn(static_cast<VkInstance>(m_instance), &create_info, nullptr, &messenger) !=
        VK_SUCCESS) {
        KNG_CRITICAL("createDebugMessenger failed at {}", __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }
    m_debug_messenger = messenger;
}

void VulkanContext::create_surface(GLFWwindow* window)
{
    VkSurfaceKHR surface_raw = VK_NULL_HANDLE;
    const VkResult result =
        glfwCreateWindowSurface(static_cast<VkInstance>(m_instance), window, nullptr, &surface_raw);
    if (result != VK_SUCCESS) {
        KNG_CRITICAL("createWindowSurface failed at {}", __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }
    m_surface = surface_raw;
}

void VulkanContext::pick_physical_device()
{
    const auto devices = m_instance.enumeratePhysicalDevices();
    if (devices.empty()) {
        KNG_CRITICAL("No Vulkan physical devices found at {}", __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }

    for (const auto& device : devices) {
        const auto props = device.getProperties();
        if (props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
            m_physical_device = device;
            KNG_INFO("Selected GPU: {}", static_cast<const char*>(props.deviceName));
            return;
        }
    }
    m_physical_device = devices[0];
    KNG_INFO("Selected default GPU: {}",
             static_cast<const char*>(m_physical_device.getProperties().deviceName));
}

void VulkanContext::create_logical_device()
{
    const auto queue_families = m_physical_device.getQueueFamilyProperties();
    std::optional<uint32_t> graphics_idx;
    std::optional<uint32_t> present_idx;

    for (uint32_t i = 0; i < queue_families.size(); ++i) {
        if (queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics) {
            graphics_idx = i;
        }
        if (m_physical_device.getSurfaceSupportKHR(i, m_surface)) {
            present_idx = i;
        }
        if (graphics_idx && present_idx) {
            break;
        }
    }

    if (!graphics_idx || !present_idx) {
        KNG_CRITICAL("Queue families not found at {}", __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }

    m_graphics_queue_family = *graphics_idx;
    m_present_queue_family = *present_idx;

    std::set<uint32_t> unique_families = {m_graphics_queue_family, m_present_queue_family};
    std::vector<vk::DeviceQueueCreateInfo> queue_infos;
    const float queue_priority = 1.0f;
    for (uint32_t family : unique_families) {
        vk::DeviceQueueCreateInfo info;
        info.queueFamilyIndex = family;
        info.queueCount = 1;
        info.pQueuePriorities = &queue_priority;
        queue_infos.push_back(info);
    }

    vk::DeviceCreateInfo create_info;
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_infos.size());
    create_info.pQueueCreateInfos = queue_infos.data();
    create_info.enabledExtensionCount = 0;
    const char* device_ext = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    create_info.enabledExtensionCount = 1;
    create_info.ppEnabledExtensionNames = &device_ext;

    vk::Result result = m_physical_device.createDevice(&create_info, nullptr, &m_device);
    if (result != vk::Result::eSuccess) {
        KNG_CRITICAL("createLogicalDevice: {} at {}", vk::to_string(result), __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }

    m_graphics_queue = m_device.getQueue(m_graphics_queue_family, 0);
    m_present_queue = m_device.getQueue(m_present_queue_family, 0);
}

} // namespace kenga
