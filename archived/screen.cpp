#include "GLFW/glfw3.h"
#include "screen.h"
#include "immediate.h"

using namespace TexGui;
using namespace Math;

Screen::Screen(GLContext* gl_ctx)
    : Widget(), m_gl_context(gl_ctx)
{
    m_box.pos = {0.0, 0.0};
    m_box.size = gl_ctx->m_screen_size;
    m_window_scale = gl_ctx->m_window_scale;
}

void Screen::cursorPosCallback(double x, double y)
{
    x *= m_window_scale;
    y *= m_window_scale;
    g_input_state.cursor_pos = ivec2(x, y);
    if (m_drag_active && m_active_widget)
    {
        m_active_widget->onCursorPosEvent(x - m_active_widget_pos.x, y - m_active_widget_pos.y);
        return;
    }
    m_cursor_pos = ivec2(x, y);
    ivec2 temp_pos = m_cursor_pos;
    Widget* w = findWidget(temp_pos);
    m_active_widget_pos = m_cursor_pos - temp_pos;

    if (w != m_hovered_widget && m_hovered_widget)
    {
        m_hovered_widget->onCursorExitEvent();
        if (w) w->onCursorEnterEvent();
    }

    if (w)
        w->onCursorPosEvent(temp_pos.x, temp_pos.y);
    
    if (m_active_widget != nullptr && m_active_widget != m_hovered_widget)
        m_active_widget->onCursorPosEvent(temp_pos.x, temp_pos.y);

    m_hovered_widget = w;
}

void Screen::mouseButtonCallback(int button, int action)
{
    if (button == GLFW_MOUSE_BUTTON_1)
    {
        if (action == GLFW_RELEASE) g_input_state.lmb = KEY_Release;
        else if (g_input_state.lmb == KEY_Off) g_input_state.lmb = KEY_Press;
    }

    //Deactivates current active widget if it is not hovered
    if (m_active_widget)
        m_active_widget->onMouseDownEvent(button, action);

    if (action == GLFW_PRESS)
    {
        if (m_active_widget != m_hovered_widget && m_hovered_widget)
        {
            m_hovered_widget->onMouseDownEvent(button, action);
            m_active_widget = m_hovered_widget;
        }
        m_drag_active = true;
    }
    else
    {
        m_drag_active = false;
    }
}

void Screen::keyCallback(int key, int scancode, int action, int mods)
{
    if (m_active_widget)
        m_active_widget->onKeyEvent(key, scancode, action, mods);
}

void Screen::charCallback(unsigned int codepoint)
{
    g_input_state.chars.push(codepoint);
    if (m_active_widget)
    {
        m_active_widget->onCharEvent(codepoint);
    }
}

void Screen::framebufferSizeCallback(int width, int height)
{
    m_gl_context->setScreenSize(width, height);
    setSize({float(width), float(height)});
    ImmBase.bounds = m_box;
}

void Screen::draw(RenderState& state)
{
    Widget::draw(state);
    state.m_widget_pos += m_box.pos + fvec2(m_padding.left, m_padding.top);
    for (auto& w : m_immediates)
    {
        if (w == nullptr) continue;

        if (w->m_visible)
            w->draw(state);
    }
    state.m_widget_pos -= m_box.pos + fvec2(m_padding.left, m_padding.top);

}
