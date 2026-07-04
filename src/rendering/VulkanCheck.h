/**
 * @file VulkanCheck.h
 * @brief Макрос проверки Vulkan result — бросает исключение при ошибке
 *
 * Включать после vulkan/vulkan.hpp и core/LoggerMacros.h
 */
#pragma once

#include "core/LoggerMacros.h"
#include <vulkan/vulkan.hpp>
#include <stdexcept>

#define VK_CHECK_RESULT(result)                                                       \
    do {                                                                              \
        if ((result) != vk::Result::eSuccess) {                                       \
            KNG_CRITICAL("Vulkan error: {} at {}", vk::to_string(result), __FUNCTION__); \
            throw std::runtime_error(std::string("Vulkan error: ") + vk::to_string(result)); \
        }                                                                             \
    } while (0)
