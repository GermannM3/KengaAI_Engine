/**
 * @file LoggerMacros.h
 * @brief Макросы логирования Kenga Engine
 *
 * Использование: #include "core/LoggerMacros.h" после LogManager::init()
 */

#pragma once

#include "core/LogManager.h"

#define KNG_TRACE(...)    kenga::LogManager::get_logger().trace(__VA_ARGS__)
#define KNG_DEBUG(...)    kenga::LogManager::get_logger().debug(__VA_ARGS__)
#define KNG_INFO(...)     kenga::LogManager::get_logger().info(__VA_ARGS__)
#define KNG_WARN(...)     kenga::LogManager::get_logger().warn(__VA_ARGS__)
#define KNG_ERROR(...)    kenga::LogManager::get_logger().error(__VA_ARGS__)
#define KNG_CRITICAL(...) kenga::LogManager::get_logger().critical(__VA_ARGS__)
