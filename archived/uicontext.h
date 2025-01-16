#pragma once

#include "common.h"
#include "screen.h"
#include <GLFW/glfw3.h>

NAMESPACE_BEGIN(TexGui);

// state/texture layer enum
class UIContext
{
public:
    UIContext(GLFWwindow* window);
    ~UIContext();

    // Appends the draw commands to the GL context
    RenderState draw();

    // Clears render data.
    void clear();

    Widget* addWidget(Widget* widget);

    void loadFont(const char* font);
    Screen* screenPtr() { return m_screen; };
    void preloadTextures(const char* dir);
    GLContext* m_gl_context;
private:
    Screen* m_screen;
};

NAMESPACE_END(TexGui);
