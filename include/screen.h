#pragma once

#include "context.h"
#include "common.h"
#include "widget.h"
#include "shader.h"

NAMESPACE_BEGIN(TexGui);

class Screen : public Widget
{
    friend class Widget;
    friend class GLContext;
public:
    Screen(GLContext* gl_ctx);

    void draw(RenderState& state);
    void cursorPosCallback(double x, double y);
    void mouseButtonCallback(int button, int action);
    void keyCallback(int key, int scancode, int action, int mods);
    void charCallback(unsigned int codepoint);
    void framebufferSizeCallback(int width, int height);
private:
    GLContext* m_gl_context;
    Math::ivec2 m_cursor_pos;
    float m_window_scale;
    Widget* m_hovered_widget = nullptr;
    Widget* m_active_widget = nullptr;
    Math::ivec2 m_active_widget_pos;

    bool m_drag_active = false;
};

NAMESPACE_END(TexGui);
