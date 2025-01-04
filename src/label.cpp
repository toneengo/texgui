#include "label.h"

using namespace TexGui;
using namespace Math;

Label::Label(const char* label, int xpos, int ypos, int width, int height)
    : Widget(xpos, ypos, width, height), m_label(label)
{
}

void draw(GLContext* ctx)
{
    
}
