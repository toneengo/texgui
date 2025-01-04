#include "image.h"

using namespace TexGui;
using namespace Math;

Image::Image(const char* image)
    : Widget()
{
    assignTexture(image);
    setSize(fvec2(m_texentry->bounds.width * m_pixel_size, m_texentry->bounds.height * m_pixel_size));
}

void Image::draw(GLContext* ctx)
{
    Widget::draw(ctx);
    ctx->drawTexture(m_box, m_texentry, m_state, m_pixel_size, 0);
}
