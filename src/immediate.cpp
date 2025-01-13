#include "immediate.h"
#include "defaults.h"

#include <unordered_map>

using namespace TexGui;
using namespace Math;

extern std::unordered_map<std::string, TexEntry> m_tex_map;

NAMESPACE_BEGIN(TexGui);

#define _PX Defaults::PixelSize
ImmCtx ImmCtx::Window(const char* name, float xpos, float ypos, float width, float height, uint32_t flags)
{
    if (!g_windowStates.contains(name))
    {
        WindowState _ws = {
            .box = {xpos, ypos, width, height},
            .initial_box = {xpos, ypos, width, height},
        };
        g_windowStates.insert({name, _ws});
    }

    WindowState& wstate = g_windowStates[name];

    static TexEntry* wintex = &m_tex_map[Defaults::Window::Texture];
    uint32_t state = 0;

    if (wstate.box.contains(g_input_state.cursor_pos) && g_input_state.lmb == KEY_Press)
        wstate.active = true;
    else if (g_input_state.lmb == KEY_Press)
        wstate.active = false;

    if (g_input_state.lmb != KEY_Held)
    {
        wstate.moving = false;
        wstate.resizing = false;
    }

    if (fbox(wstate.box.x, wstate.box.y, wstate.box.width, wintex->top * _PX).contains(g_input_state.cursor_pos) && g_input_state.lmb == KEY_Press)
    {
        wstate.last_cursor_pos = g_input_state.cursor_pos;
        wstate.moving = true;
    }

    if (fbox(wstate.box.x + wstate.box.width - wintex->right * _PX,
             wstate.box.y + wstate.box.height - wintex->bottom * _PX,
             wintex->right * _PX, wintex->bottom * _PX).contains(g_input_state.cursor_pos) && g_input_state.lmb == KEY_Press)
    {
        wstate.last_cursor_pos = g_input_state.cursor_pos;
        wstate.resizing = true;
    }

    if (wstate.moving)
    {
        wstate.box.pos += g_input_state.cursor_pos - wstate.last_cursor_pos;
        wstate.last_cursor_pos = g_input_state.cursor_pos;
    }
    else if (wstate.resizing)
    {
        wstate.box.size += g_input_state.cursor_pos - wstate.last_cursor_pos;
        wstate.last_cursor_pos = g_input_state.cursor_pos;
    }

    if (wstate.active) state |= STATE_ACTIVE;

    fvec4 padding = Defaults::Window::Padding;
    padding.top = wintex->top * _PX;

    g_immediate_state.drawTexture(wstate.box, wintex, state, _PX, SLICE_9);
    g_immediate_state.drawText(name, {wstate.box.x + padding.left, wstate.box.y + wintex->top * _PX / 2},
                 Defaults::Font::Color, Defaults::Font::Scale, CENTER_Y);

    fbox internal = fbox::pad(wstate.box, padding);
    return withBounds(internal);

}

bool ImmCtx::Button(const char* text)
{
    uint32_t state = 0;

    bool click = false;

    if (bounds.contains(g_input_state.cursor_pos))
    {
        state |= STATE_HOVER;

        if (g_input_state.lmb) state |= STATE_PRESS;
        if (g_input_state.lmb == KEY_Press) click = true;
    }

    g_immediate_state.drawTexture(bounds, &m_tex_map[Defaults::Button::Texture], state, _PX, SLICE_9);

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
