// this header contains the global texgui context and states for stateful widgets (windows, text inputs, scroll panels, etc)

#pragma once

#include "texgui.h"
#include "context.hpp"
#include "types.h"
#include "SDL3/SDL.h"
#include <queue>
#include <mutex>
#include <unordered_map>

NAMESPACE_BEGIN(TexGui);

inline std::mutex TGInputLock;

inline RenderData TGRenderData;
inline RenderData TGSyncedRenderData;
struct WindowState
{
    Math::fbox box;
    Math::fbox initial_box;

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

struct ScrollPanelState
{
    Math::fvec2 contentPos;
    Math::fbox bounds;
};

enum KeyState : int
{
    KEY_Off = 0,
    KEY_Press = 1,
    KEY_Held = 2,
    KEY_Release = 3,
};

#define TEXGUI_MOUSE_BUTTON_COUNT 64
struct InputData {
    int keyStates[TexGuiKey_NamedKey_COUNT];
    int mouseStates[TEXGUI_MOUSE_BUTTON_COUNT];
    int mods = 0;

    int& lmb = mouseStates[0];

    Math::fvec2 cursorPos;
    Math::fvec2 mouseRelativeMotion;
    Math::fvec2 scroll;
    Math::ivec2 scrollDiscrete;
    std::queue<unsigned int> charQueue;

    bool firstMouse = false;

    bool backspace = false;

    inline void submitKey(TexGuiKey key, KeyState state)
    {
        keyStates[key - TexGuiKey_NamedKey_BEGIN] = state;
    }

    inline int getKeyState(TexGuiKey key)
    {
        return keyStates[key - TexGuiKey_NamedKey_BEGIN];
    }

    inline void submitMouseButton(int button, KeyState state)
    {
        mouseStates[button - 1] = state;
    }

    inline int getMouseButtonState(int button)
    {
        return mouseStates[button];
    }

    InputData() { memset(keyStates, KEY_Off, sizeof(int)*(TexGuiKey_NamedKey_COUNT));
                  memset(mouseStates, KEY_Off, sizeof(int)*(TEXGUI_MOUSE_BUTTON_COUNT)); }
};

struct TexGuiContext
{
    Math::ivec2 framebufferSize;
    float contentScale = 1.f;
    void* backendData = nullptr;
    std::unordered_map<std::string, WindowState> windows;
    std::unordered_map<std::string, TextInputState> textInputs;
    std::unordered_map<std::string, ScrollPanelState> scrollPanels;
    NoApiContext* renderCtx = nullptr;
    InputData io;
    bool initialised = false;
};

//#TODO: testing without dynamic alloc
inline TexGuiContext* GTexGui = nullptr;
NAMESPACE_END(TexGui);
