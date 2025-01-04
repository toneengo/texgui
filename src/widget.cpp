#include "defaults.h"
#include "widget.h"
#include "util.h"

using namespace TexGui;

using namespace Math;
Widget::Widget(float xpos, float ypos, float width, float height)
    : m_visible(true),
      m_box(xpos, ypos, width, height),
      m_text_color(Defaults::Font::Color),
      m_text_scale(Defaults::Font::Scale), m_render_flags(Defaults::Flags),
      m_pixel_size(Defaults::PixelSize),
      m_state(STATE_NONE), m_needs_update(true)
{
    if (xpos < 1)
        m_inherit.x = xpos;
    else
        m_inherit.x = -1;

    if (ypos < 1)
        m_inherit.y = ypos;
    else
        m_inherit.y = -1;

    if (width < 1)
        m_inherit.width = width == 0 ? 1 : width;

    if (height < 1)
        m_inherit.height = height == 0 ? 1 : height;
}

void Widget::onCursorPosEvent(int x, int y)
{
    m_cursor_pos = {x, y};
    if (!contains(fvec2(x, y) + m_box.pos))
        setFlagBit(m_state, STATE_HOVER, 0);
    /*
    if (!enter)
        setFlagBit(m_state, STATE_PRESS, 0);
    */
}

void Widget::onCursorEnterEvent()
{
    if (m_parent)
        m_parent->onCursorEnterEvent();

    setFlagBit(m_state, STATE_HOVER, 1);
}

void Widget::onCursorExitEvent()
{
    if (m_parent)
        m_parent->onCursorExitEvent();
    
    setFlagBit(m_state, STATE_HOVER, 0);
}

void Widget::onMouseDownEvent(int button, int action)
{
    if (m_parent)
        m_parent->onMouseDownEvent(button, action);

    if (action == GLFW_PRESS)
    {
        if (m_state & STATE_HOVER)
        {
            setFlagBit(m_state, STATE_PRESS, 1);
            setFlagBit(m_state, STATE_ACTIVE, 1);
            return;
        }

        setFlagBit(m_state, STATE_ACTIVE, 0);
    }

    if (action == GLFW_RELEASE) {
        setFlagBit(m_state, STATE_PRESS, 0);
    }
}

void Widget::update()
{
    if (m_parent && m_inherit_bounds) {
        m_bounds = fbox::pad(m_parent->m_box, m_parent->m_padding);
        m_bounds.pos = fvec2(0);
    }

    if (m_inherit.width  > 0) m_box.width  = m_bounds.width  * m_inherit.width;
    if (m_inherit.height > 0) m_box.height = m_bounds.height * m_inherit.height;

    if (m_inherit.x != -1) m_box.x = m_bounds.x + m_bounds.width * m_inherit.x;
    if (m_inherit.y != -1) m_box.y = m_bounds.y + m_bounds.width * m_inherit.y;

    if (m_render_flags & ALIGN_CENTER_X)
        m_box.x = m_bounds.width / 2 - m_box.width / 2;
    if (m_render_flags & ALIGN_CENTER_Y)
        m_box.y = m_bounds.height / 2 - m_box.height / 2;
    if (m_render_flags & ALIGN_LEFT)
        m_box.x = 0;
    if (m_render_flags & ALIGN_RIGHT)
        m_box.x = m_bounds.width - m_box.width;
    if (m_render_flags & ALIGN_TOP)
        m_box.y = 0;
    if (m_render_flags & ALIGN_BOTTOM)
        m_box.y = m_bounds.height - m_box.height;
}

void Widget::draw(GLContext* ctx)
{
    if (!m_visible) return;

    if (m_needs_update)
        update();

    ctx->setWidgetPos(ctx->m_widget_pos + m_box.pos + fvec2(m_padding.left, m_padding.top));
    for (auto& w : m_children)
    {
        if (w == nullptr) continue;

        if (m_needs_update)
            w->m_needs_update = true;

        if (w->m_visible)
            w->draw(ctx);
    }
    ctx->setWidgetPos(ctx->m_widget_pos - m_box.pos - fvec2(m_padding.left, m_padding.top));
    m_needs_update = false;
}

bool Widget::contains(const Math::fvec2& pos)
{
    Math::fvec2 d = pos - m_box.pos;
    return d.x <= m_box.w && d.x >= 0.f &&
           d.y <= m_box.h && d.y >= 0.f;
}

extern std::unordered_map<std::string, TexEntry> m_tex_map;
void Widget::assignTexture(std::string tex)
{
    if (!m_tex_map.contains(tex))
    {
        printf("%s not found/loaded", tex.c_str());
        exit(1);
    }

    m_texture_name = tex;
    TexEntry* e = &m_tex_map[tex];
    m_texentry = e;
}

Widget* Widget::addChild(Widget* widget)
{
    m_children.push_back(widget);
    widget->m_parent = this;
    widget->m_bounds = fbox::pad(m_box, m_padding);
    m_needs_update = true;
    return widget;
}

//changes pos
Widget* Widget::findWidget(ivec2& pos)
{
    pos -= m_box.pos;
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
    {
        Widget* child = *it;
        if (child->visible() && child->contains(pos - fvec2(m_padding.left, m_padding.top)))
        {
            pos -= ivec2(m_padding.left, m_padding.top);
            return child->findWidget(pos);
        }
    }
    return /*contains(pos) &&*/ m_visible ? this : nullptr;
}
