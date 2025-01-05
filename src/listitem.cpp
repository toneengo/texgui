#include "defaults.h"
#include "listitem.h"

using namespace TexGui;
using namespace Math;

extern std::unordered_map<std::string, TexEntry> m_tex_map;
//ListItem::ListItem(const char* image, const char* label, bool* binding)
ListItem::ListItem(Widget* widget, int* v, int v_button)
    : Widget(), m_binding(v), m_value(v_button)
{
    assignTexture(Defaults::ListItem::Texture);
    m_padding = Defaults::ListItem::Padding;
    addChild(widget);
    if (widget)
    {
        setSize({widget->m_box.width, widget->m_box.height});
    }
}

void ListItem::onMouseDownEvent(int button, int action)
{
    Widget::onMouseDownEvent(button, action);
    if (m_state & STATE_ACTIVE)
        *m_binding = m_value;
}

void ListItem::draw(GLContext* ctx)
{
    int state = m_state & STATE_ACTIVE ? STATE_ACTIVE : m_state;
    ctx->drawTexture(m_box, m_texentry, state, m_pixel_size, SLICE_9);
    Widget::draw(ctx);
}
