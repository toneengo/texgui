#include "GLFW/glfw3.h"
#include "defaults.h"
#include "button.h"
#include "types.h"
#include "util.h"

using namespace TexGui;
using namespace Math;

Button::Button(const char* label, _TexGui_FN_PTR fn, int xpos, int ypos, int width, int height)
    : Widget(xpos, ypos, width, height),
      m_label(label), m_pressed_offset(Defaults::Button::POffset), m_function(fn)
{
    assignTexture(Defaults::Button::Texture);
    m_padding = Defaults::Button::Padding;
}

/*
void Button::onMouseEnterEvent(bool enter)
{
}
*/

void Button::onCursorPosEvent(int x, int y)
{
    Widget::onCursorPosEvent(x, y);
    // I don't want it to appear pressed when the mouse is dragged outside
    //if (!getBit(m_state, STATE_HOVER)) setBit(m_state, STATE_PRESS, 0);
}

void Button::onMouseDownEvent(int button, int action)
{
    Widget::onMouseDownEvent(button, action);
    if (m_state & STATE_HOVER && action == GLFW_RELEASE)
        m_function();
}

void Button::draw(RenderState& state)
{
    Widget::draw(state);
    
    if (contains(g_cursor_pos)) m_state |= STATE_HOVER;

    state.drawTexture(m_box, m_texentry, m_state, m_pixel_size, SLICE_9);

    state.drawText(m_label.c_str(),
        m_state & STATE_PRESS ? m_box.pos + m_box.size / 2 + m_pressed_offset
                              : m_box.pos + m_box.size / 2,

        m_text_color, m_text_scale,
        CENTER_X | CENTER_Y);
}
