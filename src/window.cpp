#include "defaults.h"
#include "window.h"
#include "util.h"

using namespace TexGui;
using namespace Math;

Window::Window(const char* title, float xpos, float ypos, float width, float height)
    : Widget(xpos, ypos, width, height), m_title(title), m_moving(false), m_resizing(false)
{
    assignTexture(Defaults::Window::Texture);
    m_padding = Defaults::Window::Padding;
    m_padding.top = m_texentry->top * m_pixel_size;
}

void Window::onCursorPosEvent(int x, int y)
{
    Widget::onCursorPosEvent(x, y);
    if (m_moving)
    {
        setPos(fvec2(x - m_cursor_offset.x, y - m_cursor_offset.y));
        setFlagBit(m_state, STATE_HOVER, 1);
    }
    else if (m_resizing)
    {
        setSize(m_cursor_pos + m_cursor_offset);
        setFlagBit(m_state, STATE_HOVER, 1);
    }
}

void Window::onMouseDownEvent(int button, int action)
{
    Widget::onMouseDownEvent(button, action);

    if (!getFlagBit(m_state, STATE_ACTIVE)) return;
    if (action == GLFW_PRESS &&
        fbox(0, 0, m_box.width, m_texentry->top * m_pixel_size).contains(m_cursor_pos))
    {
        m_cursor_offset = m_cursor_pos - m_box.pos;
        m_moving = true;
        return;
    }
    
    if (action == GLFW_PRESS &&
        fbox(m_box.width  - m_texentry->right * m_pixel_size,
             m_box.height - m_texentry->bottom * m_pixel_size,
             m_texentry->right * m_pixel_size, m_texentry->bottom * m_pixel_size).contains(m_cursor_pos))
    {
        m_cursor_offset = fvec2(m_box.width, m_box.height) - m_cursor_pos;
        m_resizing = true;
        return;
    }

    m_moving = false;
    m_resizing = false;
}

void Window::draw(GLContext* ctx)
{
    ctx->drawTexture(m_box, m_texentry, m_state, m_pixel_size, SLICE_9);

    ctx->drawText(m_title.c_str(), {m_box.x + m_padding.left, m_box.y + m_texentry->top * m_pixel_size / 2}, m_text_color, m_text_scale, CENTER_Y);

    Widget::draw(ctx);
}
