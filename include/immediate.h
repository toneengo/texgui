#pragma once

#include "common.h"
#include "types.h"
#include <array>

NAMESPACE_BEGIN(TexGui);

class UIContext;

enum KeyState : uint8_t
{
    KEY_Off = 0,
    KEY_Press = 1,
    KEY_On = 2,
    KEY_Release = 3,
};

struct InputState
{
    Math::ivec2 cursor_pos;
    uint8_t lmb;
};

struct ImmCtx
{
    InputState* input;
    RenderState* rs;

    Math::fbox bounds;

    inline ImmCtx withBounds(Math::fbox bounds) const
    {
        ImmCtx copy = *this;
        copy.bounds = bounds;
        return copy;
    }
};

ImmCtx imm_window(ImmCtx& ctx, const char* name, uint32_t flags, float w, float h);

void imm_row(ImmCtx& ctx, ImmCtx* out, const float* widths, uint32_t n);

template <uint32_t N>
std::array<ImmCtx, N> imm_row(ImmCtx& ctx, const float (&widths)[N])
{
    std::array<ImmCtx, N> out;
    imm_row(ctx, &out[0], widths, N);
    return out;
}

bool imm_button(ImmCtx& ctx, const char* text);

NAMESPACE_END(TexGui);


