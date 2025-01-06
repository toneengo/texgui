#pragma once

#include "common.h"
#include "widget.h"

using _TexGui_FN_PTR = void(*)();

NAMESPACE_BEGIN(TexGui);

class Label : public Widget
{
    friend class Widget;
public:
    Label(const char* label, int xpos = 0, int ypos = 0, int width = 0, int height = 0);
    virtual void draw(RenderState& state);
protected:
    std::string m_label;
};

NAMESPACE_END(TexGui);
