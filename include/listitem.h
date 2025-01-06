#pragma once

#include "common.h"
#include "widget.h"

NAMESPACE_BEGIN(TexGui);

//image
class ListItem : public Widget
{
    friend class Widget;
public:
    ListItem(Widget* widget, int* v, int v_button);
    virtual void draw(RenderState& state);
    virtual void onMouseDownEvent(int button, int action) override;
protected:
    TexEntry* m_image_texentry;
    int m_data;
    Math::fvec2 m_col_pos;
    bool m_wrap;

    int* m_binding;
    int  m_value;
};

NAMESPACE_END(TexGui);
