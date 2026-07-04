/**
 * @file Time.h
 * @brief Управление временем и delta time
 *
 * PROJECT_RULES.md. Фаза 2.1: Core — Game loop.
 */

#pragma once

#include <GLFW/glfw3.h>

namespace kenga {

/**
 * @brief Статический менеджер времени (использует GLFW)
 */
class Time {
public:
    static double get_time();
    static double delta_time();
    static double fixed_delta_time();

    static void set_delta_time(double dt) { s_delta_time = dt; }
    static constexpr double fixed_dt() { return 1.0 / 60.0; }

private:
    static double s_delta_time;
};

} // namespace kenga
