#include "texgui.h"
#include "GLFW/glfw3.h"
#include "texgui_math.hpp"
#include "texgui_glfw.hpp"
#include "texgui_internal.hpp"
#include "cassert"

using namespace TexGui;;
// GLFW data
enum GlfwClientApi
{
    GlfwClientApi_Unknown,
    GlfwClientApi_OpenGL,
    GlfwClientApi_Vulkan,
};

//This is copied from Dear ImGui
TexGuiKey TexGui_ImplGlfw_KeyToTexGuiKey(int keycode, int scancode)
{
    switch (keycode)
    {
        case GLFW_KEY_TAB: return TexGuiKey_Tab;
        case GLFW_KEY_LEFT: return TexGuiKey_LeftArrow;
        case GLFW_KEY_RIGHT: return TexGuiKey_RightArrow;
        case GLFW_KEY_UP: return TexGuiKey_UpArrow;
        case GLFW_KEY_DOWN: return TexGuiKey_DownArrow;
        case GLFW_KEY_PAGE_UP: return TexGuiKey_PageUp;
        case GLFW_KEY_PAGE_DOWN: return TexGuiKey_PageDown;
        case GLFW_KEY_HOME: return TexGuiKey_Home;
        case GLFW_KEY_END: return TexGuiKey_End;
        case GLFW_KEY_INSERT: return TexGuiKey_Insert;
        case GLFW_KEY_DELETE: return TexGuiKey_Delete;
        case GLFW_KEY_BACKSPACE: return TexGuiKey_Backspace;
        case GLFW_KEY_SPACE: return TexGuiKey_Space;
        case GLFW_KEY_ENTER: return TexGuiKey_Enter;
        case GLFW_KEY_ESCAPE: return TexGuiKey_Escape;
        case GLFW_KEY_APOSTROPHE: return TexGuiKey_Apostrophe;
        case GLFW_KEY_COMMA: return TexGuiKey_Comma;
        case GLFW_KEY_MINUS: return TexGuiKey_Minus;
        case GLFW_KEY_PERIOD: return TexGuiKey_Period;
        case GLFW_KEY_SLASH: return TexGuiKey_Slash;
        case GLFW_KEY_SEMICOLON: return TexGuiKey_Semicolon;
        case GLFW_KEY_EQUAL: return TexGuiKey_Equal;
        case GLFW_KEY_LEFT_BRACKET: return TexGuiKey_LeftBracket;
        case GLFW_KEY_BACKSLASH: return TexGuiKey_Backslash;
        case GLFW_KEY_WORLD_1: return TexGuiKey_Oem102;
        case GLFW_KEY_WORLD_2: return TexGuiKey_Oem102;
        case GLFW_KEY_RIGHT_BRACKET: return TexGuiKey_RightBracket;
        case GLFW_KEY_GRAVE_ACCENT: return TexGuiKey_GraveAccent;
        case GLFW_KEY_CAPS_LOCK: return TexGuiKey_CapsLock;
        case GLFW_KEY_SCROLL_LOCK: return TexGuiKey_ScrollLock;
        case GLFW_KEY_NUM_LOCK: return TexGuiKey_NumLock;
        case GLFW_KEY_PRINT_SCREEN: return TexGuiKey_PrintScreen;
        case GLFW_KEY_PAUSE: return TexGuiKey_Pause;
        case GLFW_KEY_KP_0: return TexGuiKey_Keypad0;
        case GLFW_KEY_KP_1: return TexGuiKey_Keypad1;
        case GLFW_KEY_KP_2: return TexGuiKey_Keypad2;
        case GLFW_KEY_KP_3: return TexGuiKey_Keypad3;
        case GLFW_KEY_KP_4: return TexGuiKey_Keypad4;
        case GLFW_KEY_KP_5: return TexGuiKey_Keypad5;
        case GLFW_KEY_KP_6: return TexGuiKey_Keypad6;
        case GLFW_KEY_KP_7: return TexGuiKey_Keypad7;
        case GLFW_KEY_KP_8: return TexGuiKey_Keypad8;
        case GLFW_KEY_KP_9: return TexGuiKey_Keypad9;
        case GLFW_KEY_KP_DECIMAL: return TexGuiKey_KeypadDecimal;
        case GLFW_KEY_KP_DIVIDE: return TexGuiKey_KeypadDivide;
        case GLFW_KEY_KP_MULTIPLY: return TexGuiKey_KeypadMultiply;
        case GLFW_KEY_KP_SUBTRACT: return TexGuiKey_KeypadSubtract;
        case GLFW_KEY_KP_ADD: return TexGuiKey_KeypadAdd;
        case GLFW_KEY_KP_ENTER: return TexGuiKey_KeypadEnter;
        case GLFW_KEY_KP_EQUAL: return TexGuiKey_KeypadEqual;
        case GLFW_KEY_LEFT_SHIFT: return TexGuiKey_LeftShift;
        case GLFW_KEY_LEFT_CONTROL: return TexGuiKey_LeftCtrl;
        case GLFW_KEY_LEFT_ALT: return TexGuiKey_LeftAlt;
        case GLFW_KEY_LEFT_SUPER: return TexGuiKey_LeftSuper;
        case GLFW_KEY_RIGHT_SHIFT: return TexGuiKey_RightShift;
        case GLFW_KEY_RIGHT_CONTROL: return TexGuiKey_RightCtrl;
        case GLFW_KEY_RIGHT_ALT: return TexGuiKey_RightAlt;
        case GLFW_KEY_RIGHT_SUPER: return TexGuiKey_RightSuper;
        case GLFW_KEY_MENU: return TexGuiKey_Menu;
        case GLFW_KEY_0: return TexGuiKey_0;
        case GLFW_KEY_1: return TexGuiKey_1;
        case GLFW_KEY_2: return TexGuiKey_2;
        case GLFW_KEY_3: return TexGuiKey_3;
        case GLFW_KEY_4: return TexGuiKey_4;
        case GLFW_KEY_5: return TexGuiKey_5;
        case GLFW_KEY_6: return TexGuiKey_6;
        case GLFW_KEY_7: return TexGuiKey_7;
        case GLFW_KEY_8: return TexGuiKey_8;
        case GLFW_KEY_9: return TexGuiKey_9;
        case GLFW_KEY_A: return TexGuiKey_A;
        case GLFW_KEY_B: return TexGuiKey_B;
        case GLFW_KEY_C: return TexGuiKey_C;
        case GLFW_KEY_D: return TexGuiKey_D;
        case GLFW_KEY_E: return TexGuiKey_E;
        case GLFW_KEY_F: return TexGuiKey_F;
        case GLFW_KEY_G: return TexGuiKey_G;
        case GLFW_KEY_H: return TexGuiKey_H;
        case GLFW_KEY_I: return TexGuiKey_I;
        case GLFW_KEY_J: return TexGuiKey_J;
        case GLFW_KEY_K: return TexGuiKey_K;
        case GLFW_KEY_L: return TexGuiKey_L;
        case GLFW_KEY_M: return TexGuiKey_M;
        case GLFW_KEY_N: return TexGuiKey_N;
        case GLFW_KEY_O: return TexGuiKey_O;
        case GLFW_KEY_P: return TexGuiKey_P;
        case GLFW_KEY_Q: return TexGuiKey_Q;
        case GLFW_KEY_R: return TexGuiKey_R;
        case GLFW_KEY_S: return TexGuiKey_S;
        case GLFW_KEY_T: return TexGuiKey_T;
        case GLFW_KEY_U: return TexGuiKey_U;
        case GLFW_KEY_V: return TexGuiKey_V;
        case GLFW_KEY_W: return TexGuiKey_W;
        case GLFW_KEY_X: return TexGuiKey_X;
        case GLFW_KEY_Y: return TexGuiKey_Y;
        case GLFW_KEY_Z: return TexGuiKey_Z;
        case GLFW_KEY_F1: return TexGuiKey_F1;
        case GLFW_KEY_F2: return TexGuiKey_F2;
        case GLFW_KEY_F3: return TexGuiKey_F3;
        case GLFW_KEY_F4: return TexGuiKey_F4;
        case GLFW_KEY_F5: return TexGuiKey_F5;
        case GLFW_KEY_F6: return TexGuiKey_F6;
        case GLFW_KEY_F7: return TexGuiKey_F7;
        case GLFW_KEY_F8: return TexGuiKey_F8;
        case GLFW_KEY_F9: return TexGuiKey_F9;
        case GLFW_KEY_F10: return TexGuiKey_F10;
        case GLFW_KEY_F11: return TexGuiKey_F11;
        case GLFW_KEY_F12: return TexGuiKey_F12;
        case GLFW_KEY_F13: return TexGuiKey_F13;
        case GLFW_KEY_F14: return TexGuiKey_F14;
        case GLFW_KEY_F15: return TexGuiKey_F15;
        case GLFW_KEY_F16: return TexGuiKey_F16;
        case GLFW_KEY_F17: return TexGuiKey_F17;
        case GLFW_KEY_F18: return TexGuiKey_F18;
        case GLFW_KEY_F19: return TexGuiKey_F19;
        case GLFW_KEY_F20: return TexGuiKey_F20;
        case GLFW_KEY_F21: return TexGuiKey_F21;
        case GLFW_KEY_F22: return TexGuiKey_F22;
        case GLFW_KEY_F23: return TexGuiKey_F23;
        case GLFW_KEY_F24: return TexGuiKey_F24;
        default: return TexGuiKey_None;
    }
}

struct ImplGlfw_Data
{
    GLFWwindow*             Window;
    GlfwClientApi           ClientApi;
    double                  Time;
    GLFWwindow*             MouseWindow;
    //GLFWcursor*             MouseCursors[TexGuiMouseCursor_COUNT];
    Math::fvec2             LastValidMousePos;
    bool                    InstalledCallbacks;
    bool                    CallbacksChainForAllWindows;
#ifdef EMSCRIPTEN_USE_EMBEDDED_GLFW3
    const char*             CanvasSelector;
#endif

    // Chain GLFW callbacks: our callbacks will call the user's previously installed callbacks, if any.
    GLFWwindowfocusfun      PrevUserCallbackWindowFocus;
    GLFWcursorposfun        PrevUserCallbackCursorPos;
    GLFWcursorenterfun      PrevUserCallbackCursorEnter;
    GLFWmousebuttonfun      PrevUserCallbackMousebutton;
    GLFWscrollfun           PrevUserCallbackScroll;
    GLFWkeyfun              PrevUserCallbackKey;
    GLFWcharfun             PrevUserCallbackChar;
    GLFWmonitorfun          PrevUserCallbackMonitor;
    GLFWframebuffersizefun  PrevUserCallbackFramebufferSize;

    ImplGlfw_Data()   { memset((void*)this, 0, sizeof(*this)); }
};

GLFWmonitorfun TexGui_ImplGlfw_MonitorCallback; 

void TexGui_ImplGlfw_ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    auto& bd = *(ImplGlfw_Data*)GTexGui->backendData;
    auto& io = GTexGui->io;
    if (bd.PrevUserCallbackScroll)
        bd.PrevUserCallbackScroll(window, xoffset, yoffset);

    std::lock_guard<std::mutex> lock(TGInputLock);
    io.scroll += Math::fvec2(xoffset, yoffset);
}

void TexGui_ImplGlfw_CursorPosCallback(GLFWwindow* window, double x, double y)
{
    auto& bd = *(ImplGlfw_Data*)GTexGui->backendData;
    auto& io = GTexGui->io;
    if (bd.PrevUserCallbackCursorPos)
        bd.PrevUserCallbackCursorPos(window, x, y);

    x *= GTexGui->contentScale;
    y *= GTexGui->contentScale;

    std::lock_guard<std::mutex> lock(TGInputLock);
    if (!io.firstMouse)
        io.firstMouse = true;
    else
        io.mouseRelativeMotion += Math::fvec2(x - io.cursorPos.x, y - io.cursorPos.y);

    io.cursorPos = Math::fvec2(x, y);
}

void TexGui_ImplGlfw_KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto& bd = *(ImplGlfw_Data*)GTexGui->backendData;
    auto& io = GTexGui->io;
    if (bd.PrevUserCallbackKey)
        bd.PrevUserCallbackKey(window, key, scancode, action, mods);

    std::lock_guard<std::mutex> lock(TGInputLock);
    TexGuiKey tgkey = TexGui_ImplGlfw_KeyToTexGuiKey(key, scancode);
    //#TODO: probly dont need to check if keystate is off
    if (action == GLFW_RELEASE) io.submitKey(tgkey, KEY_Release);
    else if (io.getKeyState(tgkey) == KEY_Off) io.submitKey(tgkey, KEY_Press);
    else if (action == GLFW_REPEAT) io.submitKey(tgkey, KEY_Repeat);
}

void TexGui_ImplGlfw_MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    auto& bd = *(ImplGlfw_Data*)GTexGui->backendData;
    auto& io = GTexGui->io;

    if (bd.PrevUserCallbackMousebutton)
        bd.PrevUserCallbackMousebutton(window, button, action, mods);

    std::lock_guard<std::mutex> lock(TGInputLock);
    if (action == GLFW_RELEASE) io.submitMouseButton(button + 1, KEY_Release);
    else if (io.getMouseButtonState(button + 1) == KEY_Off) io.submitMouseButton(button + 1, KEY_Press);
};

void TexGui_ImplGlfw_CharCallback(GLFWwindow* window, unsigned int codepoint)
{
    auto& bd = *(ImplGlfw_Data*)GTexGui->backendData;
    auto& io = GTexGui->io;

    if (bd.PrevUserCallbackChar)
        bd.PrevUserCallbackChar(window, codepoint);

    std::lock_guard<std::mutex> lock(TGInputLock);
    io.charQueue.push(codepoint);
};

void TexGui_ImplGlfw_FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    auto& bd = *(ImplGlfw_Data*)GTexGui->backendData;
    if (bd.PrevUserCallbackFramebufferSize)
        bd.PrevUserCallbackFramebufferSize(window, width, height);

    GTexGui->renderCtx->setScreenSize(width, height);
    GTexGui->framebufferSize = {width, height};

    glfwGetWindowContentScale(window, &GTexGui->contentScale, nullptr);

    std::lock_guard<std::mutex> lock(TGInputLock);
    Base.bounds.size = Math::fvec2(width, height);
};

bool initGlfwCallbacks(GLFWwindow* window)
{
    auto& bd = *(ImplGlfw_Data*)GTexGui->backendData;
    assert(GTexGui && !bd.InstalledCallbacks);
    //bd.PrevUserCallbackWindowFocus = glfwSetWindowFocusCallback(window, TexGui_ImplGlfw_WindowFocusCallback);
    //bd.PrevUserCallbackCursorEnter = glfwSetCursorEnterCallback(window, TexGui_ImplGlfw_CursorEnterCallback);
    bd.PrevUserCallbackScroll = glfwSetScrollCallback(window, TexGui_ImplGlfw_ScrollCallback);
    //bd.PrevUserCallbackMonitor = glfwSetMonitorCallback(TexGui_ImplGlfw_MonitorCallback);
    bd.PrevUserCallbackCursorPos       = glfwSetCursorPosCallback(window, TexGui_ImplGlfw_CursorPosCallback);
    bd.PrevUserCallbackMousebutton     = glfwSetMouseButtonCallback(window, TexGui_ImplGlfw_MouseButtonCallback);
    bd.PrevUserCallbackKey             = glfwSetKeyCallback(window, TexGui_ImplGlfw_KeyCallback);
    bd.PrevUserCallbackChar            = glfwSetCharCallback(window, TexGui_ImplGlfw_CharCallback);
    bd.PrevUserCallbackFramebufferSize = glfwSetFramebufferSizeCallback(window, TexGui_ImplGlfw_FramebufferSizeCallback);
    bd.InstalledCallbacks = true;

    return true;
} 

bool TexGui::initGlfw(GLFWwindow* window)
{
    assert(GTexGui && !GTexGui->initialised);
    GTexGui->backendData = new ImplGlfw_Data(); 
    initGlfwCallbacks(window);
    glfwGetWindowContentScale(window, &GTexGui->contentScale, nullptr);
    glfwGetWindowSize(window, &GTexGui->framebufferSize.x, &GTexGui->framebufferSize.y);

    if (GTexGui->renderCtx != nullptr) GTexGui->renderCtx->setScreenSize(GTexGui->framebufferSize.x, GTexGui->framebufferSize.y);
    Base.bounds.size = GTexGui->framebufferSize;
    Base.rs = &TGRenderData;
    GTexGui->initialised = true;
    return true;
}
