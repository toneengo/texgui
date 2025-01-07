#include "immediate.h"
#include "defaults.h"

#include <unordered_map>

using namespace TexGui;
using namespace Math;

extern std::unordered_map<std::string, TexEntry> m_tex_map;

NAMESPACE_BEGIN(TexGui);

ImmCtx imm_window(ImmCtx& ctx, const char* name, uint32_t flags, float w, float h)
{
    fbox winBounds(0,0, w, h);

    ctx.rs->drawTexture(winBounds, &m_tex_map[Defaults::Window::Texture], 0, 2, SLICE_9);

    fbox internal = fbox::pad(winBounds, Defaults::Window::Padding);

    return ctx.withBounds(internal);
}

void imm_row(ImmCtx& ctx, ImmCtx* out, const float* widths, uint32_t n)
{
    float x = 0;
    float y = 0;

    float spacing = Defaults::Row::Spacing;

    // Sum of all sized cells. The rest of the row is distributed evenly between unsized cells.
    float absoluteWidth = 0;
    for (uint32_t i = 0; i < n; i++) absoluteWidth += widths[i];


    for (uint32_t i = 0; i < n; i++)
    {
        float width = widths[i] > 0 
                      ? widths[i] 
                      : ctx.bounds.width - absoluteWidth - (spacing * (n - 1)); 

        out[i] = ctx;
        out[i].bounds = fbox(ctx.bounds.x + x, ctx.bounds.y + y, width, ctx.bounds.height);

        x += width + spacing;
    }
}

bool imm_button(ImmCtx& ctx, const char* text)
{
    uint32_t state = 0;

    bool click = false;

    if (ctx.bounds.contains(ctx.input->cursor_pos))
    {
        state |= STATE_HOVER;

        if (ctx.input->lmb) state |= STATE_PRESS;
        if (ctx.input->lmb == KEY_Press) click = true;
    }

    ctx.rs->drawTexture(ctx.bounds, &m_tex_map[Defaults::Button::Texture], state, 2, SLICE_9);

    ctx.rs->drawText(text, 
            state & STATE_PRESS ? ctx.bounds.pos + ctx.bounds.size / 2 + Defaults::Button::POffset
                                : ctx.bounds.pos + ctx.bounds.size / 2,

            {1,1,1,1}, 0.5, CENTER_X | CENTER_Y);

    return click;
}

NAMESPACE_END(TexGui);
