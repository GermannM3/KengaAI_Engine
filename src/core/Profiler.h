/**
 * @file Profiler.h
 * @brief Lightweight frame profiler — tracks CPU timings per subsystem
 *
 * PROJECT_RULES.md. Phase 10: Optimization.
 */

#pragma once

#include <chrono>
#include <string>
#include <array>

namespace kenga {

/// @brief Simple frame profiler that tracks CPU timings for named sections
class Profiler {
public:
    /// Named timing sections
    enum class Section : int {
        physics = 0,
        animation,
        particles,
        lua_scripts,
        render_record,  ///< command buffer recording
        render_submit,  ///< queue submit + present
        imgui,
        total_frame,
        count_           ///< sentinel — must be last
    };

    static constexpr int section_count = static_cast<int>(Section::count_);

    /// Call at the start of a section
    void begin(Section s);

    /// Call at the end of a section — records elapsed time
    void end(Section s);

    /// Get the last recorded time for a section (milliseconds)
    float get_ms(Section s) const;

    /// Get a human-readable name for a section
    static const char* section_name(Section s);

    /// Smoothed FPS (exponential moving average)
    float fps() const { return m_fps; }

    /// Call once per frame with frame delta to update FPS counter
    void update_fps(double dt);

    /// Total draw calls this frame (set by renderer)
    int draw_calls = 0;
    /// Total draw calls culled this frame
    int culled_count = 0;

private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;

    std::array<TimePoint, section_count> m_starts{};
    std::array<float, section_count> m_times_ms{};  ///< last recorded times
    float m_fps = 0.0f;
};

/// Global profiler instance
Profiler& get_profiler();

} // namespace kenga
