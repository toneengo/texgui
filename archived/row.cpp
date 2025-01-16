#include "row.h"

using namespace TexGui;
using namespace Math;

Widget* Row::addCol(Widget* widget, float size)
{
    if (size == -1 && widget && widget->m_inherit.width == 0)
        size = widget->m_box.width;

    m_children.push_back(widget);
    m_widths.push_back(size);
    if (size < 1) 
    {
        m_inherit_cols++;
        m_inherit_widths.push_back(size == 0 ? 1 : size);
    }
    else
    {
        m_inherit_widths.push_back(0);
        m_absolute_width += size;
    }

    m_needs_update = true;
    return widget;
}

bool Row::contains(const Math::fvec2& pos)
{
    Math::fvec2 d = pos - m_box.pos;
    return d.x <= m_box.w  && d.x >= 0.f &&
           d.y <= m_height && d.y >= 0.f;
}

void Row::clear()
{
    Widget::clear();
    m_widths.clear();
    m_inherit_widths.clear();
    m_absolute_width = 0;
    m_inherit_cols = 0;
}

void Row::update()
{
    Widget::update();

    float currWidth = 0;
    float currHeight = 0;
    for (int i = 0; i < m_children.size(); i++)
    {
        if (m_inherit_widths[i] > 0)
            m_widths[i] = (m_box.width - m_absolute_width - m_spacing * (m_children.size() - 1)) * m_inherit_widths[i] / m_inherit_cols;

        if (m_render_flags & WRAPPED && currWidth + m_widths[i] > m_box.width)
        {
            currWidth = 0;
            currHeight += m_box.height + m_spacing;
        }

        if (m_children[i] != nullptr)
        {
            m_children[i]->m_inherit_bounds = false;
            m_children[i]->m_bounds = fbox(currWidth, currHeight, float(m_widths[i]), m_box.height);
        }

        currWidth += m_widths[i] + m_spacing;
    }

    m_height = m_render_flags & WRAPPED ? currHeight + m_box.height : m_box.height;
}

void Row::draw(RenderState& state)
{
    Widget::draw(state);
}
