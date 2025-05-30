// this header contains the global texgui context and states for stateful widgets (windows, text inputs, scroll panels, etc)

#pragma once

#include "texgui.h"
#include "texgui_types.hpp"
#include <queue>
#include <mutex>
#include <unordered_map>
#include <atomic>

NAMESPACE_BEGIN(TexGui);

inline std::mutex TGInputLock;

struct WindowState
{
    Math::fbox box;

    int32_t order = 0;

    uint32_t state = 0;
    bool moving = false;
    bool resizing = false;

    bool visible = false;
    bool prevVisible = false;

    std::unordered_map<std::string, uint32_t> buttonStates;
};

struct TextInputState 
{
    //Math::fvec2 select_pos;
    int cursor_pos = 0;
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
    KEY_Press = 0x0001,
    KEY_Held = 0x0002,
    KEY_Release = 0x0004,
    KEY_Repeat = 0x0008,
};

#define TEXGUI_MOUSE_BUTTON_COUNT 64
#define TEXGUI_TEXT_BUF_SIZE 1000000
struct InputData {
    int keyStates[TexGuiKey_NamedKey_COUNT];
    int mouseStates[TEXGUI_MOUSE_BUTTON_COUNT];
    int mods = 0;

    int& lmb = mouseStates[0];

    Math::fvec2 cursorPos;
    Math::fvec2 mouseRelativeMotion;
    Math::fvec2 scroll;
    Math::ivec2 scrollDiscrete;

    char text[TEXGUI_TEXT_BUF_SIZE];

    bool firstMouse = false;

    inline void submitKey(TexGuiKey key, KeyState state)
    {
        keyStates[key] = state;
    }

    inline int getKeyState(TexGuiKey key)
    {
        return keyStates[key];
    }

    inline void submitMouseButton(int button, KeyState state)
    {
        mouseStates[button - 1] = state;
    }

    inline int getMouseButtonState(int button)
    {
        return mouseStates[button];
    }

    InputData()
    {
        memset(keyStates, KEY_Off, sizeof(int)*(TexGuiKey_NamedKey_COUNT));
        memset(mouseStates, KEY_Off, sizeof(int)*(TEXGUI_MOUSE_BUTTON_COUNT));
        memset(text, 0, sizeof(char) * TEXGUI_TEXT_BUF_SIZE);
    }
};

struct TexGuiContext
{
    bool capturingMouse = false;
    bool lastCapturingMouse = false;
    Math::ivec2 framebufferSize;
    std::atomic<bool> editingText = false;
    float contentScale = 1.f;
    void* backendData = nullptr;
    struct {
        uint32_t (*createTexture)(void* data, int width, int height);
        void (*framebufferSizeCallback)(int width, int height);
        void (*renderClean)();
        void (*newFrame)();
    } rendererFns;
    void* rendererData = nullptr;

    std::unordered_map<std::string, WindowState> windows;
    std::unordered_map<std::string, TextInputState> textInputs;
    std::unordered_map<std::string, ScrollPanelState> scrollPanels;

    //#TODO: separate rasterized and msdf font atlases 
    std::unordered_map<std::string, TexGui::Font> fonts;
    std::unordered_map<std::string, TexGui::Texture> textures;

    InputData io;
    bool initialised = false;
};

//#TODO: testing without dynamic alloc
inline std::atomic<bool> TexGui_editingText = false;
inline TexGuiContext* GTexGui = nullptr;
NAMESPACE_END(TexGui);
