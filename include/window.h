#pragma once

#include "common.h"
#include "widget.h"

NAMESPACE_BEGIN(TexGui);

class Window : public Widget
{
    friend class Widget;
public:
    Window(const char* title, float x = 0, float y = 0, float width = INHERIT, float height = INHERIT);
    virtual void draw(GLContext* ctx);
    void onCursorPosEvent(int x, int y);
    void onMouseDownEvent(int button, int action);
protected:
    std::string m_title;
    bool m_resizing;
    bool m_moving;
    Math::fvec2 m_cursor_offset;
    float m_bar_height;
};

NAMESPACE_END(TexGui);
