/**
 * @file Time.cpp
 * @brief Реализация Time
 */

#include "core/Time.h"

namespace kenga {

double Time::s_delta_time = 0.0;

double Time::get_time()
{
    return glfwGetTime();
}

double Time::delta_time()
{
    return s_delta_time;
}

double Time::fixed_delta_time()
{
    return fixed_dt();
}

} // namespace kenga
