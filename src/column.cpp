#include "column.h"

using namespace TexGui;
using namespace Math;

Widget* Column::addRow(Widget* widget, float size, uint32_t flags)
{
    if (size == -1 && widget && widget->m_inherit.height == 0)
        size = widget->m_box.height;

    if (widget) {
        widget->m_render_flags = flags;
    }
    m_children.push_back(widget);
    m_heights.push_back(size);
    if (size < 1) 
    {
        m_inherit_rows++;
        m_inherit_heights.push_back(size == 0 ? 1 : size);
    }
    else
    {
        m_inherit_heights.push_back(0);
        m_absolute_height += size;
    }

    m_needs_update = true;
    return widget;
}

bool Column::contains(const Math::fvec2& pos)
{
    Math::fvec2 d = pos - m_box.pos;
    return d.x <= m_width && d.x >= 0.f &&
           d.y <= m_box.h && d.y >= 0.f;
}

void Column::clear()
{
    Widget::clear();
    m_heights.clear();
    m_inherit_heights.clear();
    m_absolute_height = 0;
    m_inherit_rows = 0;
}

void Column::update()
{
    Widget::update();

    float currWidth = 0;
    float currHeight = 0;
    for (int i = 0; i < m_children.size(); i++)
    {
        if (m_inherit_heights[i] > 0)
            m_heights[i] = (m_box.height - m_absolute_height - m_spacing * (m_children.size() - 1)) * m_inherit_heights[i] / m_inherit_rows;

        if (m_render_flags & WRAPPED && currWidth + m_heights[i] > m_box.width)
        {
            currWidth += m_box.width + m_spacing;
            currHeight = 0;
        }

        if (m_children[i] != nullptr)
        {
            m_children[i]->m_inherit_bounds = false;
            m_children[i]->m_bounds = fbox(currWidth, currHeight, m_box.width, float(m_heights[i]));
        }
        currHeight += m_heights[i] + m_spacing;
    }
    m_width = m_render_flags & WRAPPED ? currWidth + m_box.width : m_box.width;
}

void Column::draw(RenderState& state)
{
    Widget::draw(state);
}
