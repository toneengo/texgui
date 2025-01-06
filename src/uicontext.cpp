#include "context.h"
#include "screen.h"
#include "types.h"

#include "uicontext.h"

using namespace TexGui;
using namespace Math;

void UIContext::render()
{
    m_gl_context->draw();
}

void UIContext::draw()
{
    m_screen->draw(m_gl_context);
}

UIContext::UIContext(GLFWwindow* window)
{
    m_gl_context = new GLContext(window);
    m_screen = new Screen(m_gl_context);
    m_screen->setPos(fvec2(0));
}

Widget* UIContext::addWidget(Widget* widget)
{
    m_screen->addChild(widget);
    return widget;
}

void UIContext::loadFont(const char* font)
{
    m_gl_context->loadFont(font);
}

void UIContext::preloadTextures(const char* dir)
{
    m_gl_context->preloadTextures(dir);
}

UIContext::~UIContext()
{
    delete m_gl_context;
}
