/**
 * @file LogManager.h
 * @brief Централизованный логгер Kenga Engine
 *
 * PROJECT_RULES.md. Фаза 2.1: Core — Logging refinement.
 */

#pragma once

#include <memory>
#include <spdlog/spdlog.h>

namespace kenga {

/**
 * @brief Singleton-обёртка над spdlog для движка
 */
class LogManager {
public:
    static void init();
    static spdlog::logger& get_logger() { return *s_logger; }

private:
    static std::shared_ptr<spdlog::logger> s_logger;
};

} // namespace kenga
