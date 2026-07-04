/**
 * @file Input.h
 * @brief Обработка ввода (клавиатура, мышь)
 *
 * PROJECT_RULES.md. Фаза 2.1, 3.4: Input + camera.
 */

#pragma once

#include <GLFW/glfw3.h>

namespace kenga {

/**
 * @brief Статический менеджер ввода
 */
class Input {
public:
    static void set_window(GLFWwindow* window) { s_window = window; }
    static GLFWwindow* get_window() { return s_window; }

    static bool is_key_pressed(int key);
    static bool is_mouse_button_pressed(int button);

    /// Current cursor position in window coordinates
    static double mouse_x() { return s_cursor_x; }
    static double mouse_y() { return s_cursor_y; }

    /**
     * @brief Обновить delta мыши. Вызывать каждый кадр.
     * @param fly_mode If true, center cursor and compute delta (FPS camera).
     *                 If false, just read cursor position without recentering (UI/editor).
     */
    static void update_mouse_delta(bool fly_mode = true);

    static double mouse_delta_x() { return s_mouse_delta_x; }
    static double mouse_delta_y() { return s_mouse_delta_y; }

    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouse_callback(GLFWwindow* window, double xpos, double ypos);

private:
    static GLFWwindow* s_window;
    static double s_last_x;
    static double s_last_y;
    static double s_mouse_delta_x;
    static double s_mouse_delta_y;
    static bool s_first_mouse;
    static double s_cursor_x;
    static double s_cursor_y;
};

} // namespace kenga
