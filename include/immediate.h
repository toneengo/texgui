#pragma once

#include "defaults.h"
#include "common.h"
#include "types.h"
#include <unordered_map>
#include <array>
#include <queue>

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
    std::queue<unsigned int> chars;
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
    void TextInput(const char* name, std::string& buf);
    template <uint32_t N>
    std::array<ImmCtx, N> Row(const float (&widths)[N], float height = 0)
    {
        std::array<ImmCtx, N> out;
        Row_Internal(&out[0], widths, N, height);
        return out;
    }

    template <uint32_t N>
    std::array<ImmCtx, N> Column(const float (&heights)[N], float width = 0)
    {
        std::array<ImmCtx, N> out;
        Column_Internal(&out[0], heights, N, width);
        return out;
    }
private:
    void Row_Internal(ImmCtx* out, const float* widths, uint32_t n, float height);
    void Column_Internal(ImmCtx* out, const float* widths, uint32_t n, float height);
    uint32_t getBoxState(uint32_t& state, Math::fbox box);

    std::unordered_map<std::string, uint32_t>* buttonStates;
};

struct WindowState
{
    Math::fbox box;
    Math::fbox initial_box;
    Math::fvec2 last_cursor_pos;

    uint32_t state = 0;
    bool moving = false;
    bool resizing = false;

    std::unordered_map<std::string, uint32_t> buttonStates;
};

struct TextInputState 
{
    Math::fvec2 select_pos;
    bool selecting = false;
    uint32_t state = 0;
};

inline InputState g_input_state;
inline RenderState g_immediate_state = {};
inline int g_window_count = 0;
inline ImmCtx ImmBase;
//inline std::vector<WindowState> g_windowStates;

inline std::unordered_map<std::string, WindowState> g_windowStates;
inline std::unordered_map<std::string, TextInputState> g_textInputStates;

inline void clearImmediateUI()
{
    g_window_count = 0;
}

inline void clearImmediate()
{
    if (g_input_state.lmb == KEY_Press) g_input_state.lmb = KEY_Held;
    if (g_input_state.lmb == KEY_Release) g_input_state.lmb = KEY_Off;
    g_immediate_state.clear();
    if (!g_input_state.chars.empty())
        g_input_state.chars.pop();
}

NAMESPACE_END(TexGui);
