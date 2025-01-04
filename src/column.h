#pragma once

#include "defaults.h"
#include "common.h"
#include "widget.h"

NAMESPACE_BEGIN(TexGui);

class Column : public Widget
{
    friend class Widget;
public:
    Widget* addRow(Widget* widget, float size = 0, uint32_t flags = 0);
    bool contains(const Math::fvec2& pos) override;
    void clear() override;

    template <typename... Rows>
    Column(Rows... rows)
        : Widget(), m_spacing(Defaults::Column::Spacing)
    {
        (addRow(nullptr, rows), ...);
    }
    virtual void draw(GLContext* ctx) override;
    virtual void update() override;

protected:
    std::vector<double> m_heights;
    std::vector<double> m_inherit_heights;
    float m_width;
    float m_spacing;

private:
    float m_absolute_height = 0;
    float m_inherit_rows = 0;
};

NAMESPACE_END(TexGui);
