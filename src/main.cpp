/**
 * @file main.cpp
 * @brief Kenga Engine — точка входа
 *
 * PROJECT_RULES.md. Проект: KengaEngine, AAA-движок на C++20/Vulkan/ECS.
 * В Release (NDEBUG) отключаются validation layers и подробный лог.
 */

#include "core/Application.h"
#include "core/LogManager.h"
#include "core/LoggerMacros.h"

#include <cstdlib>

int main()
{
    kenga::LogManager::init();
    KNG_INFO("Kenga Engine v0.1 — starting");

    kenga::Application app;
    app.run();

    KNG_INFO("Kenga Engine — exiting");
    return EXIT_SUCCESS;
}
