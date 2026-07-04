/**
 * @file Profiler.cpp
 * @brief Frame profiler implementation
 */

#include "core/Profiler.h"

namespace kenga {

static Profiler s_profiler;

Profiler& get_profiler()
{
    return s_profiler;
}

void Profiler::begin(Section s)
{
    m_starts[static_cast<int>(s)] = Clock::now();
}

void Profiler::end(Section s)
{
    const auto elapsed = Clock::now() - m_starts[static_cast<int>(s)];
    m_times_ms[static_cast<int>(s)] =
        std::chrono::duration<float, std::milli>(elapsed).count();
}

float Profiler::get_ms(Section s) const
{
    return m_times_ms[static_cast<int>(s)];
}

const char* Profiler::section_name(Section s)
{
    switch (s) {
    case Section::physics:       return "Physics";
    case Section::animation:     return "Animation";
    case Section::particles:     return "Particles";
    case Section::lua_scripts:   return "Lua Scripts";
    case Section::render_record: return "Render Record";
    case Section::render_submit: return "Render Submit";
    case Section::imgui:         return "ImGui";
    case Section::total_frame:   return "Total Frame";
    default:                     return "???";
    }
}

void Profiler::update_fps(double dt)
{
    if (dt > 0.0) {
        const float instant_fps = static_cast<float>(1.0 / dt);
        // Exponential moving average (alpha = 0.05 for smooth display)
        m_fps = m_fps * 0.95f + instant_fps * 0.05f;
    }
}

} // namespace kenga
