#pragma once

#include "common.h"
#include "widget.h"

using _TexGui_FN_PTR = void(*)();

NAMESPACE_BEGIN(TexGui);

class Button : public Widget
{
    friend class Widget;
public:
    Button(const char* label, _TexGui_FN_PTR fn, int xpos = 0, int ypos = 0, int width = 0, int height = 0);
    virtual void draw(RenderState& state);
    //void onMouseEnterEvent(bool enter);
    void onCursorPosEvent(int x, int y);
    void onMouseDownEvent(int button, int action);
protected:
    std::string m_label;
    _TexGui_FN_PTR m_function;

    Math::fvec2 m_pressed_offset;
};

NAMESPACE_END(TexGui);
