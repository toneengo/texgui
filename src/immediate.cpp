#include "immediate.h"
#include "defaults.h"

#include <unordered_map>

using namespace TexGui;
using namespace Math;

extern std::unordered_map<std::string, TexEntry> m_tex_map;

NAMESPACE_BEGIN(TexGui);

ImmCtx ImmCtx::Window(const char* name, float xpos, float ypos, float width, float height, uint32_t flags)
{
    static TexEntry* wintex = &m_tex_map[Defaults::Window::Texture];
    fvec4 padding = Defaults::Window::Padding;
    padding.top = wintex->top * Defaults::PixelSize;
    fbox winBounds(xpos, ypos, width, height);

    g_immediate_state.drawTexture(winBounds, wintex, 0, Defaults::PixelSize, SLICE_9);
    g_immediate_state.drawText(name, {winBounds.x + padding.left, winBounds.y + wintex->top * Defaults::PixelSize / 2},
                 Defaults::Font::Color, Defaults::Font::Scale, CENTER_Y);

    fbox internal = fbox::pad(winBounds, padding);
    return withBounds(internal);
}

bool ImmCtx::Button(const char* text)
{
    uint32_t state = 0;

    bool click = false;

    if (bounds.contains(g_input_state.cursor_pos))
    {
        state |= STATE_HOVER;

        if (g_input_state.lmb == KEY_Press)
        {
            state |= STATE_PRESS;
            click = true;
        }
    }

    g_immediate_state.drawTexture(bounds, &m_tex_map[Defaults::Button::Texture], state, Defaults::PixelSize, SLICE_9);

    g_immediate_state.drawText(text, 
        state & STATE_PRESS ? bounds.pos + bounds.size / 2 + Defaults::Button::POffset
                            : bounds.pos + bounds.size / 2,
        Defaults::Font::Color, Defaults::Font::Scale, CENTER_X | CENTER_Y);

    return click;
}

ImmCtx ImmCtx::Box(float xpos, float ypos, float width, float height, const char* texture)
{
    if (width <= 1)
        width = width == 0 ? bounds.width : bounds.width * width;
    if (height <= 1)
        height = height == 0 ? bounds.height : bounds.height * height;

    fbox boxBounds(bounds.x + xpos, bounds.y + ypos, width, height);
    if (texture != nullptr)
    {
        static TexEntry* boxtex = &m_tex_map[texture];
        g_immediate_state.drawTexture(boxBounds, boxtex, 0, 2, SLICE_9);
    }

    fbox internal = fbox::pad(boxBounds, Defaults::Box::Padding);
    return withBounds(internal);
}

void ImmCtx::_Row_Internal(ImmCtx* out, const float* widths, uint32_t n, float height)
{
    if (height < 1) {
        height = height == 0 ? bounds.height : bounds.height * height;
    }
    float absoluteWidth = 0;
    float inherit = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        if (widths[i] >= 1)
            absoluteWidth += widths[i];
        else
            inherit++;
    }

    float x = 0, y = 0;
    float spacing = Defaults::Row::Spacing;
    for (uint32_t i = 0; i < n; i++)
    {
        float width;
        if (widths[i] <= 1)
        {
            float mult = widths[i] == 0 ? 1 : widths[i];
            width = bounds.width - absoluteWidth - (spacing * (n - 1)) / inherit;
        }
        else
            width = widths[i];

        out[i] = *this;
        out[i].bounds = Math::fbox(bounds.x + x, bounds.y + y, width, height);

        x += width + spacing;
    }
}

void ImmCtx::_Column_Internal(ImmCtx* out, const float* heights, uint32_t n, float width)
{
    if (width < 1) {
        width = width == 0 ? bounds.width : bounds.width * width;
    }
    float absoluteHeight = 0;
    float inherit = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        if (heights[i] >= 1)
            absoluteHeight += heights[i];
        else
            inherit++;
    }

    float x = 0, y = 0;
    float spacing = Defaults::Column::Spacing;
    for (uint32_t i = 0; i < n; i++)
    {
        float height;
        if (heights[i] <= 1)
        {
            float mult = heights[i] == 0 ? 1 : heights[i];
            height = bounds.height - absoluteHeight - (spacing * (n - 1)) / inherit;
        }
        else
            height = heights[i];
        
        out[i] = *this;
        out[i].bounds = Math::fbox(bounds.x + x, bounds.y + y, width, height);

        y += height + spacing;
    }
}

NAMESPACE_END(TexGui);
