#include "GLFW/glfw3.h"
#include "defaults.h"
#include "textinput.h"
#include "util.h"
#include "math.h"
#include <chrono>

using namespace TexGui;
using namespace Math;

// ##### Timekeeping stuff ##### // 
using namespace std::chrono_literals;
namespace stc = std::chrono;

stc::nanoseconds flashTime = stc::milliseconds(1000) / 2;
auto currentTime = stc::steady_clock::now();

TextInput::TextInput(const char* placeholder, int x, int y, int width, int height)
    : Widget(x, y, width, height), m_placeholder(placeholder), m_width(0), m_offsetx(0),
      m_pos_buf(1, 0), m_text_cur(0), showTextCursor(true), accumulator(0),
      m_sel(-1, -1)
{
    m_padding = Defaults::TextInput::Padding;
    assignTexture(Defaults::TextInput::Texture);
}

void TextInput::onCursorPosEvent(int x, int y)
{
    Widget::onCursorPosEvent(x, y);
    if (getFlagBit(m_state, STATE_PRESS) && m_sel.x != -1)
        m_sel.y = binarySearch(m_pos_buf, x - m_offsetx);
}

void TextInput::onMouseDownEvent(int button, int action)
{
    Widget::onMouseDownEvent(button, action);
    if (action == GLFW_PRESS)
    {
        m_text_cur = binarySearch(m_pos_buf, m_cursor_pos.x - m_offsetx);
        m_sel = {m_text_cur, m_text_cur};
    }
    if (!getFlagBit(m_state, STATE_ACTIVE))
        m_sel = {-1, -1};
}

extern std::unordered_map<char, TexGui::CharInfo> m_char_map;
inline void TextInput::eraseSelection()
{
    float delWidth = 0;

    if (m_sel.y < m_sel.x) std::swap(m_sel.x, m_sel.y);
    delWidth = m_pos_buf[m_sel.y] - m_pos_buf[m_sel.x];
    m_text_cur = m_sel.x;
    m_text_buf.erase(m_text_buf.begin() + m_sel.x, m_text_buf.begin() + m_sel.y);
    m_pos_buf.erase(m_pos_buf.begin() + m_sel.x + 1, m_pos_buf.begin() + m_sel.y + 1);
    m_sel = {-1, -1};

    if (m_width > m_box.width)
        m_offsetx += fmin(delWidth, -m_offsetx);

    m_width -= delWidth;

    for (int i = m_text_cur + 1; i < m_pos_buf.size(); i++)
        m_pos_buf[i] -= delWidth;

}

void TextInput::onKeyEvent(int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_BACKSPACE && m_text_buf.size() > 0 && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        if (m_sel.x != m_sel.y) eraseSelection();
        else if (m_text_cur > 0)
        {
            m_text_cur--;
            float delWidth = m_char_map[m_text_buf[m_text_cur]].advance * m_text_scale;
            m_text_buf.erase(m_text_buf.begin() + m_text_cur);
            m_pos_buf.erase(m_pos_buf.begin() + m_text_cur + 1);

            if (m_width > m_box.width)
                m_offsetx += fmin(delWidth, -m_offsetx);

            m_width -= delWidth;

            for (int i = m_text_cur + 1; i < m_pos_buf.size(); i++)
                m_pos_buf[i] -= delWidth;
        }
    }

    if (key == GLFW_KEY_LEFT && m_text_buf.size() > 0 && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        if (m_text_cur > 0) m_text_cur--;
        showTextCursor = true;
    }
    if (key == GLFW_KEY_RIGHT && m_text_buf.size() > 0 && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        if (m_text_cur < m_pos_buf.size() - 1) m_text_cur++;
        showTextCursor = true;
    }
}

void TextInput::onCharEvent(unsigned int codepoint)
{
    if (m_sel.x != m_sel.y) {
        eraseSelection();
        return;
    }

    m_text_buf.insert(m_text_buf.begin() + m_text_cur, codepoint);
    float advance = m_char_map[codepoint].advance;

    m_width += advance * m_text_scale;
    m_text_cur++;

    m_pos_buf.insert(m_pos_buf.begin() + m_text_cur,
                             m_pos_buf[m_text_cur-1] + advance * m_text_scale);

    for (auto it = m_pos_buf.begin() + m_text_cur + 1; it != m_pos_buf.end(); ++it)
    {
        *it += advance * m_text_scale;
    }

    if (m_width > m_box.width)
        m_offsetx -= advance * m_text_scale;
}

void TextInput::draw(RenderState& state)
{
    // Calculate time elapsed since last loop
    auto startTime = stc::steady_clock::now();
    auto frameTime = startTime - currentTime;
    currentTime = startTime;
    accumulator += frameTime;

    if (accumulator >= flashTime)
    {
        showTextCursor = !showTextCursor;
        accumulator -= flashTime;
    }

    Widget::draw(state);

    state.drawTexture(m_box, m_texentry, m_state, m_pixel_size, SLICE_9);

    /*
    ibox ogSx;
    glGetIntegerv(GL_SCISSOR_BOX, (GLint*)&ogSx);

    fvec2 parentPos = state,getWidgetPos();
    glScissor(parentPos.x + m_box.x,
        ogSx.y + ogSx.height - parentPos.y - m_box.y - m_box.height,
        m_box.width, m_box.height);
    */

    if (m_sel.x != m_sel.y)
        //#TODO: dont use static colour
        state.drawQuad(
            fbox(
                fmin(m_pos_buf[m_sel.x], m_pos_buf[m_sel.y]) +
                    m_box.x + m_offsetx + m_padding.left,
                m_box.y + m_padding.top,
                abs(m_pos_buf[m_sel.x] - m_pos_buf[m_sel.y]),
                abs(m_box.height - m_padding.top - m_padding.bottom)
            ),
            {115/255.0, 164/255.0, 194/255.0, 1.0}
        );


    state.drawText(
        !getFlagBit(m_state, STATE_ACTIVE) && m_text_buf.size() == 0
        ? m_placeholder.c_str() : m_text_buf.c_str(),
        {m_box.x + m_offsetx + m_padding.left, m_box.y + m_box.height / 2},
        m_text_color,
        m_text_scale,
        CENTER_Y
    );

    if (showTextCursor && getFlagBit(m_state, STATE_ACTIVE))
    {
        state.drawText("|",
            {m_box.x + m_pos_buf[m_text_cur] - 5 + m_offsetx + m_padding.left, m_box.y + m_box.height / 2}, m_text_color, m_text_scale, CENTER_Y);
    }

    /*
    glScissor(ogSx.x, ogSx.y, ogSx.width, ogSx.height);
    */
}
