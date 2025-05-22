#include "texgui_sdl3.hpp"
#include "texgui_internal.hpp"
#include <cassert>

using namespace TexGui;

// SDL Data
// Copied from Dear ImGui. Thank you Ocornut
struct TexGui_ImplSDL3_Data
{
    SDL_Window*             window;
    SDL_WindowID            windowID;
    SDL_Renderer*           renderer;
    Uint64                  time;
    char*                   clipboardTextData;
    char                    backendPlatformName[48];

    // IME handling
    SDL_Window*             imeWindow;

    // Mouse handling
    Uint32                  mouseWindowID;
    int                     mouseButtonsDown;
    //SDL_Cursor*             mouseCursors[TexGuiMouseCursor_COUNT];
    SDL_Cursor*             mouseLastCursor;
    int                     mousePendingLeaveFrame;
    bool                    mouseCanUseGlobalState;
    bool                    mouseCanUseCapture;

    // #TODO: Gamepad handling
    //ImVector<SDL_Gamepad*>      gamepads;
    //TexGui_ImplSDL3_GamepadMode  gamepadMode;
    bool                        wantUpdateGamepadsList;

    TexGui_ImplSDL3_Data()   { memset((void*)this, 0, sizeof(*this)); }
};

TexGuiKey TexGui_ImplSDL3_KeyEventToTexGuiKey(SDL_Keycode keycode, SDL_Scancode scancode)
{
    // Keypad doesn't have individual key values in SDL3
    switch (scancode)
    {
        case SDL_SCANCODE_KP_0: return TexGuiKey_Keypad0;
        case SDL_SCANCODE_KP_1: return TexGuiKey_Keypad1;
        case SDL_SCANCODE_KP_2: return TexGuiKey_Keypad2;
        case SDL_SCANCODE_KP_3: return TexGuiKey_Keypad3;
        case SDL_SCANCODE_KP_4: return TexGuiKey_Keypad4;
        case SDL_SCANCODE_KP_5: return TexGuiKey_Keypad5;
        case SDL_SCANCODE_KP_6: return TexGuiKey_Keypad6;
        case SDL_SCANCODE_KP_7: return TexGuiKey_Keypad7;
        case SDL_SCANCODE_KP_8: return TexGuiKey_Keypad8;
        case SDL_SCANCODE_KP_9: return TexGuiKey_Keypad9;
        case SDL_SCANCODE_KP_PERIOD: return TexGuiKey_KeypadDecimal;
        case SDL_SCANCODE_KP_DIVIDE: return TexGuiKey_KeypadDivide;
        case SDL_SCANCODE_KP_MULTIPLY: return TexGuiKey_KeypadMultiply;
        case SDL_SCANCODE_KP_MINUS: return TexGuiKey_KeypadSubtract;
        case SDL_SCANCODE_KP_PLUS: return TexGuiKey_KeypadAdd;
        case SDL_SCANCODE_KP_ENTER: return TexGuiKey_KeypadEnter;
        case SDL_SCANCODE_KP_EQUALS: return TexGuiKey_KeypadEqual;
        default: break;
    }
    switch (keycode)
    {
        case SDLK_TAB: return TexGuiKey_Tab;
        case SDLK_LEFT: return TexGuiKey_LeftArrow;
        case SDLK_RIGHT: return TexGuiKey_RightArrow;
        case SDLK_UP: return TexGuiKey_UpArrow;
        case SDLK_DOWN: return TexGuiKey_DownArrow;
        case SDLK_PAGEUP: return TexGuiKey_PageUp;
        case SDLK_PAGEDOWN: return TexGuiKey_PageDown;
        case SDLK_HOME: return TexGuiKey_Home;
        case SDLK_END: return TexGuiKey_End;
        case SDLK_INSERT: return TexGuiKey_Insert;
        case SDLK_DELETE: return TexGuiKey_Delete;
        case SDLK_BACKSPACE: return TexGuiKey_Backspace;
        case SDLK_SPACE: return TexGuiKey_Space;
        case SDLK_RETURN: return TexGuiKey_Enter;
        case SDLK_ESCAPE: return TexGuiKey_Escape;
        //case SDLK_APOSTROPHE: return TexGuiKey_Apostrophe;
        case SDLK_COMMA: return TexGuiKey_Comma;
        //case SDLK_MINUS: return TexGuiKey_Minus;
        case SDLK_PERIOD: return TexGuiKey_Period;
        //case SDLK_SLASH: return TexGuiKey_Slash;
        case SDLK_SEMICOLON: return TexGuiKey_Semicolon;
        //case SDLK_EQUALS: return TexGuiKey_Equal;
        //case SDLK_LEFTBRACKET: return TexGuiKey_LeftBracket;
        //case SDLK_BACKSLASH: return TexGuiKey_Backslash;
        //case SDLK_RIGHTBRACKET: return TexGuiKey_RightBracket;
        //case SDLK_GRAVE: return TexGuiKey_GraveAccent;
        case SDLK_CAPSLOCK: return TexGuiKey_CapsLock;
        case SDLK_SCROLLLOCK: return TexGuiKey_ScrollLock;
        case SDLK_NUMLOCKCLEAR: return TexGuiKey_NumLock;
        case SDLK_PRINTSCREEN: return TexGuiKey_PrintScreen;
        case SDLK_PAUSE: return TexGuiKey_Pause;
        case SDLK_LCTRL: return TexGuiKey_LeftCtrl;
        case SDLK_LSHIFT: return TexGuiKey_LeftShift;
        case SDLK_LALT: return TexGuiKey_LeftAlt;
        case SDLK_LGUI: return TexGuiKey_LeftSuper;
        case SDLK_RCTRL: return TexGuiKey_RightCtrl;
        case SDLK_RSHIFT: return TexGuiKey_RightShift;
        case SDLK_RALT: return TexGuiKey_RightAlt;
        case SDLK_RGUI: return TexGuiKey_RightSuper;
        case SDLK_APPLICATION: return TexGuiKey_Menu;
        case SDLK_0: return TexGuiKey_0;
        case SDLK_1: return TexGuiKey_1;
        case SDLK_2: return TexGuiKey_2;
        case SDLK_3: return TexGuiKey_3;
        case SDLK_4: return TexGuiKey_4;
        case SDLK_5: return TexGuiKey_5;
        case SDLK_6: return TexGuiKey_6;
        case SDLK_7: return TexGuiKey_7;
        case SDLK_8: return TexGuiKey_8;
        case SDLK_9: return TexGuiKey_9;
        case SDLK_A: return TexGuiKey_A;
        case SDLK_B: return TexGuiKey_B;
        case SDLK_C: return TexGuiKey_C;
        case SDLK_D: return TexGuiKey_D;
        case SDLK_E: return TexGuiKey_E;
        case SDLK_F: return TexGuiKey_F;
        case SDLK_G: return TexGuiKey_G;
        case SDLK_H: return TexGuiKey_H;
        case SDLK_I: return TexGuiKey_I;
        case SDLK_J: return TexGuiKey_J;
        case SDLK_K: return TexGuiKey_K;
        case SDLK_L: return TexGuiKey_L;
        case SDLK_M: return TexGuiKey_M;
        case SDLK_N: return TexGuiKey_N;
        case SDLK_O: return TexGuiKey_O;
        case SDLK_P: return TexGuiKey_P;
        case SDLK_Q: return TexGuiKey_Q;
        case SDLK_R: return TexGuiKey_R;
        case SDLK_S: return TexGuiKey_S;
        case SDLK_T: return TexGuiKey_T;
        case SDLK_U: return TexGuiKey_U;
        case SDLK_V: return TexGuiKey_V;
        case SDLK_W: return TexGuiKey_W;
        case SDLK_X: return TexGuiKey_X;
        case SDLK_Y: return TexGuiKey_Y;
        case SDLK_Z: return TexGuiKey_Z;
        case SDLK_F1: return TexGuiKey_F1;
        case SDLK_F2: return TexGuiKey_F2;
        case SDLK_F3: return TexGuiKey_F3;
        case SDLK_F4: return TexGuiKey_F4;
        case SDLK_F5: return TexGuiKey_F5;
        case SDLK_F6: return TexGuiKey_F6;
        case SDLK_F7: return TexGuiKey_F7;
        case SDLK_F8: return TexGuiKey_F8;
        case SDLK_F9: return TexGuiKey_F9;
        case SDLK_F10: return TexGuiKey_F10;
        case SDLK_F11: return TexGuiKey_F11;
        case SDLK_F12: return TexGuiKey_F12;
        case SDLK_F13: return TexGuiKey_F13;
        case SDLK_F14: return TexGuiKey_F14;
        case SDLK_F15: return TexGuiKey_F15;
        case SDLK_F16: return TexGuiKey_F16;
        case SDLK_F17: return TexGuiKey_F17;
        case SDLK_F18: return TexGuiKey_F18;
        case SDLK_F19: return TexGuiKey_F19;
        case SDLK_F20: return TexGuiKey_F20;
        case SDLK_F21: return TexGuiKey_F21;
        case SDLK_F22: return TexGuiKey_F22;
        case SDLK_F23: return TexGuiKey_F23;
        case SDLK_F24: return TexGuiKey_F24;
        case SDLK_AC_BACK: return TexGuiKey_AppBack;
        case SDLK_AC_FORWARD: return TexGuiKey_AppForward;
        default: break;
    }

    // Fallback to scancode
    switch (scancode)
    {
    case SDL_SCANCODE_GRAVE: return TexGuiKey_GraveAccent;
    case SDL_SCANCODE_MINUS: return TexGuiKey_Minus;
    case SDL_SCANCODE_EQUALS: return TexGuiKey_Equal;
    case SDL_SCANCODE_LEFTBRACKET: return TexGuiKey_LeftBracket;
    case SDL_SCANCODE_RIGHTBRACKET: return TexGuiKey_RightBracket;
    case SDL_SCANCODE_NONUSBACKSLASH: return TexGuiKey_Oem102;
    case SDL_SCANCODE_BACKSLASH: return TexGuiKey_Backslash;
    case SDL_SCANCODE_SEMICOLON: return TexGuiKey_Semicolon;
    case SDL_SCANCODE_APOSTROPHE: return TexGuiKey_Apostrophe;
    case SDL_SCANCODE_COMMA: return TexGuiKey_Comma;
    case SDL_SCANCODE_PERIOD: return TexGuiKey_Period;
    case SDL_SCANCODE_SLASH: return TexGuiKey_Slash;
    default: break;
    }
    return TexGuiKey_None;
}

bool TexGui::initSDL3(SDL_Window* window)
{
    assert(GTexGui && !GTexGui->initialised);
    GTexGui->backendData = new TexGui_ImplSDL3_Data();
    auto& bd = *(TexGui_ImplSDL3_Data*)GTexGui->backendData;
    bd.window = window;
    SDL_GetWindowSize(window, &GTexGui->framebufferSize.x, &GTexGui->framebufferSize.y);
    GTexGui->contentScale = SDL_GetWindowDisplayScale(window);

    if (GTexGui->renderCtx != nullptr) GTexGui->renderCtx->setScreenSize(GTexGui->framebufferSize.x, GTexGui->framebufferSize.y);
    Base.bounds.size = GTexGui->framebufferSize;
    Base.rs = &TGRenderData;
    GTexGui->initialised = true;
    return true;
}

void TexGui::processEvent_SDL3(const SDL_Event& event)
{
    auto& bd = *(TexGui_ImplSDL3_Data*)GTexGui->backendData;

    if (!SDL_TextInputActive(bd.window) && GTexGui->editingText)
        SDL_StartTextInput(bd.window);
    else if (SDL_TextInputActive(bd.window) && !GTexGui->editingText)
        SDL_StopTextInput(bd.window);

    auto& io = GTexGui->io;
    TexGuiKey tgkey;

    //#TODO: text input
    TGInputLock.lock();
    switch (event.type)
    {
        case SDL_EVENT_KEY_DOWN:
            tgkey = TexGui_ImplSDL3_KeyEventToTexGuiKey(event.key.key, event.key.scancode);
            if (io.getKeyState(tgkey) == KEY_Off) io.submitKey(tgkey, KEY_Press);
            else if (event.key.repeat) io.submitKey(tgkey, KEY_Repeat);
            break;
        case SDL_EVENT_KEY_UP:
            tgkey = TexGui_ImplSDL3_KeyEventToTexGuiKey(event.key.key, event.key.scancode);
            io.submitKey(tgkey, KEY_Release);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            io.submitMouseButton(event.button.button, KEY_Press);
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            io.submitMouseButton(event.button.button, KEY_Release);
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            io.scroll += Math::fvec2(event.wheel.x, event.wheel.y);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            io.cursorPos = Math::fvec2(event.motion.x, event.motion.y);
            io.mouseRelativeMotion += Math::fvec2(event.motion.xrel, event.motion.yrel);
            break;
        case SDL_EVENT_TEXT_INPUT:
            for (const char* c = event.text.text; *c != '\0'; c++)
                io.charQueue.push(*c);
            break;
        default:
            break;
    }
    TGInputLock.unlock();

    switch (event.type)
    {
        case SDL_EVENT_WINDOW_RESIZED:
            GTexGui->renderCtx->setScreenSize(event.window.data1, event.window.data2);
            GTexGui->framebufferSize = {event.window.data1, event.window.data2};
            break;
        default:
            break;
    }
}
