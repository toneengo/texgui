#include "texgui.h"
#include "defaults.h"
#include "util.h"

#include <unordered_map>

using namespace TexGui;
using namespace Math;

extern std::unordered_map<std::string, TexEntry> m_tex_map;

NAMESPACE_BEGIN(TexGui);

uint32_t getBoxState(uint32_t& state, fbox box)
{
    if (box.contains(g_input_state.cursor_pos))
    {
        state |= STATE_HOVER;

        if (g_input_state.lmb == KEY_Press) {
            state |= STATE_PRESS;
            state |= STATE_ACTIVE;
        }

        if (g_input_state.lmb == KEY_Release)
            setBit(state, STATE_PRESS, 0);

        return state;
    }

    setBit(state, STATE_HOVER, 0);
    setBit(state, STATE_PRESS, 0);

    if (g_input_state.lmb == KEY_Press)
        setBit(state, STATE_ACTIVE, 0);

    return state;
}

#define _PX Defaults::PixelSize
Container Container::Window(const char* name, float xpos, float ypos, float width, float height, uint32_t flags)
{
    if (!g_windowStates.contains(name))
    {
        g_windowStates.insert({name, {
            .box = {xpos, ypos, width, height},
            .initial_box = {xpos, ypos, width, height},
        }});
    }

    WindowState& wstate = g_windowStates[name];

    static TexEntry* wintex = &m_tex_map[Defaults::Window::Texture];

    getBoxState(wstate.state, wstate.box);
    
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

    fvec4 padding = Defaults::Window::Padding;
    padding.top = wintex->top * _PX;

    g_immediate_state.drawTexture(wstate.box, wintex, wstate.state, _PX, SLICE_9);
    g_immediate_state.drawText(name, {wstate.box.x + padding.left, wstate.box.y + wintex->top * _PX / 2},
                 Defaults::Font::Color, Defaults::Font::Scale, CENTER_Y);

    fbox internal = fbox::pad(wstate.box, padding);

    buttonStates = &wstate.buttonStates;
    return withBounds(internal);

}

bool Container::Button(const char* text)
{
    bool click = false;
    if (!buttonStates->contains(text))
    {
        buttonStates->insert({text, 0});
    }

    uint32_t& state = buttonStates->at(text);
    getBoxState(state, bounds);

    g_immediate_state.drawTexture(bounds, &m_tex_map[Defaults::Button::Texture], state, _PX, SLICE_9);

    g_immediate_state.drawText(text, 
        state & STATE_PRESS ? bounds.pos + bounds.size / 2 + Defaults::Button::POffset
                            : bounds.pos + bounds.size / 2,
        Defaults::Font::Color, Defaults::Font::Scale, CENTER_X | CENTER_Y);

    return click;
}

Container Container::Box(float xpos, float ypos, float width, float height, const char* texture)
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

void Container::TextInput(const char* name, std::string& buf)
{
    static TexEntry* inputtex = &m_tex_map[Defaults::TextInput::Texture];
    if (!g_textInputStates.contains(name))
    {
        g_textInputStates.insert({name, {}});
    }
    
    auto& tstate = g_textInputStates[name];

    getBoxState(tstate.state, bounds);
    
    if (!g_input_state.chars.empty() && tstate.state & STATE_ACTIVE)
    {
        buf.push_back(g_input_state.chars.front());
    }

    g_immediate_state.drawTexture(bounds, inputtex, tstate.state, _PX, SLICE_9);
    float offsetx = 0;
    fvec4 padding = Defaults::TextInput::Padding;
    fvec4 color = Defaults::Font::Color;
    g_immediate_state.drawText(
        !getBit(tstate.state, STATE_ACTIVE) && buf.size() == 0
        ? name : buf.c_str(),
        {bounds.x + offsetx + padding.left, bounds.y + bounds.height / 2},
        Defaults::Font::Color,
        Defaults::Font::Scale,
        CENTER_Y
    );
}

void Container::Row_Internal(Container* out, const float* widths, uint32_t n, float height)
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

void Container::Column_Internal(Container* out, const float* heights, uint32_t n, float width)
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
