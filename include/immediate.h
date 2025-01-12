#pragma once

#include "defaults.h"
#include "common.h"
#include "types.h"
#include <unordered_map>
#include <array>

NAMESPACE_BEGIN(TexGui);

class UIContext;

enum KeyState : uint8_t
{
    KEY_Off = 0,
    KEY_Press = 1,
    KEY_Held = 2,
    KEY_Release = 3,
};

struct InputState
{
    Math::ivec2 cursor_pos;
    uint8_t lmb;

};

class ImmCtx
{
public:
    RenderState* rs;
    Math::fbox bounds;

    inline ImmCtx withBounds(Math::fbox bounds) const
    {
        ImmCtx copy = *this;
        copy.bounds = bounds;
        return copy;
    }

    ImmCtx Window(const char* name, float xpos, float ypos, float width, float height, uint32_t flags = 0);
    bool Button(const char* text);
    ImmCtx Box(float xpos, float ypos, float width, float height, const char* texture = nullptr);
    template <uint32_t N>
    std::array<ImmCtx, N> Row(const float (&widths)[N], float height = 0)
    {
        std::array<ImmCtx, N> out;
        _Row_Internal(&out[0], widths, N, height);
        return out;
    }

    template <uint32_t N>
    std::array<ImmCtx, N> Column(const float (&heights)[N], float width = 0)
    {
        std::array<ImmCtx, N> out;
        _Column_Internal(&out[0], heights, N, width);
        return out;
    }
private:
    void _Row_Internal(ImmCtx* out, const float* widths, uint32_t n, float height);
    void _Column_Internal(ImmCtx* out, const float* widths, uint32_t n, float height);
};

struct WindowState
{
    Math::fbox box;
    Math::fbox initial_box;
    Math::fvec2 last_cursor_pos;

    bool active = false;
    bool moving = false;
    bool resizing = false;
};

inline InputState g_input_state;
inline RenderState g_immediate_state = {};
inline int g_window_count = 0;
inline ImmCtx ImmBase;
//inline std::vector<WindowState> g_windowStates;

inline std::unordered_map<std::string, WindowState> g_windowStates;

inline void clearImmediateUI()
{
    g_window_count = 0;
}

inline void clearImmediate()
{
    if (g_input_state.lmb == KEY_Press) g_input_state.lmb = KEY_Held;
    if (g_input_state.lmb == KEY_Release) g_input_state.lmb = KEY_Off;
    g_immediate_state.clear();
}

NAMESPACE_END(TexGui);
