/**
 * @file LogManager.cpp
 * @brief Реализация LogManager
 */

#include "core/LogManager.h"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace kenga {

std::shared_ptr<spdlog::logger> LogManager::s_logger = nullptr;

void LogManager::init()
{
    if (s_logger) {
        return; // already initialized (e.g. tests, multiple init)
    }
    s_logger = spdlog::stdout_color_mt("KengaEngine");
    s_logger->set_pattern("[%Y-%m-%d %T.%e] [%l] %v");

#ifdef NDEBUG
    s_logger->set_level(spdlog::level::info);
#else
    s_logger->set_level(spdlog::level::trace);
#endif

    spdlog::set_default_logger(s_logger);
}

} // namespace kenga
