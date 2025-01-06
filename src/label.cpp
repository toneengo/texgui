#include "label.h"

using namespace TexGui;
using namespace Math;

Label::Label(const char* label, int xpos, int ypos, int width, int height)
    : Widget(xpos, ypos, width, height), m_label(label)
{
}

extern std::unordered_map<char, TexGui::CharInfo> m_char_map;
void Label::draw(RenderState& state)
{
    Widget::draw(state);
    state.drawText(m_label.c_str(), m_box.pos, m_text_color, m_text_scale, WRAPPED, m_bounds.width);
}
