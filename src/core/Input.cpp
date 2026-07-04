/**
 * @file Input.cpp
 * @brief Реализация Input
 */

#include "core/Input.h"
#include "core/LoggerMacros.h"

namespace kenga {

GLFWwindow* Input::s_window = nullptr;
double Input::s_last_x = 0.0;
double Input::s_last_y = 0.0;
double Input::s_mouse_delta_x = 0.0;
double Input::s_mouse_delta_y = 0.0;
bool Input::s_first_mouse = true;
double Input::s_cursor_x = 0.0;
double Input::s_cursor_y = 0.0;

bool Input::is_key_pressed(int key)
{
    if (s_window == nullptr) {
        return false;
    }
    return glfwGetKey(s_window, key) == GLFW_PRESS;
}

bool Input::is_mouse_button_pressed(int button)
{
    if (s_window == nullptr) {
        return false;
    }
    return glfwGetMouseButton(s_window, button) == GLFW_PRESS;
}

void Input::update_mouse_delta(bool fly_mode)
{
    if (s_window == nullptr) {
        return;
    }

    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(s_window, &x, &y);
    s_cursor_x = x;
    s_cursor_y = y;

    if (!fly_mode) {
        // UI / editor mode: no recentering, no delta for camera
        s_mouse_delta_x = 0.0;
        s_mouse_delta_y = 0.0;
        s_first_mouse = true; // reset so next fly_mode entry is smooth
        return;
    }

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(s_window, &width, &height);
    if (width <= 0 || height <= 0) {
        return;
    }
    const double center_x = width / 2.0;
    const double center_y = height / 2.0;

    if (s_first_mouse) {
        s_last_x = center_x;
        s_last_y = center_y;
        s_first_mouse = false;
        glfwSetCursorPos(s_window, center_x, center_y);
        s_mouse_delta_x = 0.0;
        s_mouse_delta_y = 0.0;
        return;
    }

    s_mouse_delta_x = x - s_last_x;
    s_mouse_delta_y = s_last_y - y;

    glfwSetCursorPos(s_window, center_x, center_y);
    s_last_x = center_x;
    s_last_y = center_y;
}

void Input::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    (void)scancode;
    (void)mods;
    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

void Input::mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    (void)window;
    (void)xpos;
    (void)ypos;
}

} // namespace kenga
