#include "texgui.h"
#include "context.h"
#include "types.h"
#include "GLFW/glfw3.h"
#include <cassert>
#include <cstring>

using namespace TexGui;

// GLFW data
enum GlfwClientApi
{
    GlfwClientApi_Unknown,
    GlfwClientApi_OpenGL,
    GlfwClientApi_Vulkan,
};

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
#ifdef _WIN32
    WNDPROC                 PrevWndProc;
#endif

    ImplGlfw_Data()   { memset((void*)this, 0, sizeof(*this)); }
};

struct WindowState
{
    Math::fbox box;
    Math::fbox initial_box;
    Math::fvec2 last_cursorPos;

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

#include <queue>
struct InputData
{
    Math::fvec2 cursorPos;
    uint8_t     lmb;
    std::queue<unsigned int> charQueue;
};

struct TexGuiContext
{
    ImplGlfw_Data backendData;
    std::unordered_map<std::string, WindowState> windows;
    std::unordered_map<std::string, TextInputState> textInputs;
    GLContext renderCtx;
    InputData io;
};

TexGuiContext* GTexGui = nullptr;

RenderData renderData;

GLFWscrollfun TexGui_ImplGlfw_ScrollCallback;
GLFWmonitorfun TexGui_ImplGlfw_MonitorCallback; 

void TexGui_ImplGlfw_CursorPosCallback(GLFWwindow* window, double x, double y)
{
    auto& bd = GTexGui->backendData;
    auto& io = GTexGui->io;
    if (bd.PrevUserCallbackCursorPos)
        bd.PrevUserCallbackCursorPos(window, x, y);

    //#TODO: window scale
    io.cursorPos = Math::fvec2(x, y);
}

void TexGui_ImplGlfw_KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto& bd = GTexGui->backendData;
    auto& io = GTexGui->io;
    if (bd.PrevUserCallbackKey)
        bd.PrevUserCallbackKey(window, key, scancode, action, mods);
}

enum KeyState : uint8_t
{
    KEY_Off = 0,
    KEY_Press = 1,
    KEY_Held = 2,
    KEY_Release = 3,
};

void TexGui_ImplGlfw_MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    auto& bd = GTexGui->backendData;
    auto& io = GTexGui->io;

    if (bd.PrevUserCallbackMousebutton)
        bd.PrevUserCallbackMousebutton(window, button, action, mods);

    if (button == GLFW_MOUSE_BUTTON_1)
    {
        if (action == GLFW_RELEASE) io.lmb = KEY_Release;
        else if (io.lmb == KEY_Off) io.lmb = KEY_Press;
    }
};

void TexGui_ImplGlfw_CharCallback(GLFWwindow* window, unsigned int codepoint)
{
    auto& bd = GTexGui->backendData;
    auto& io = GTexGui->io;

    if (bd.PrevUserCallbackChar)
        bd.PrevUserCallbackChar(window, codepoint);

    io.charQueue.push(codepoint);
};

void TexGui_ImplGlfw_FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    auto& bd = GTexGui->backendData;
    if (bd.PrevUserCallbackFramebufferSize)
        bd.PrevUserCallbackFramebufferSize(window, width, height);

    GTexGui->renderCtx.setScreenSize(width, height);
    Base.bounds.size = Math::fvec2(width, height);
};


bool initGlfwCallbacks(GLFWwindow* window)
{
    auto& bd = GTexGui->backendData;
    if (bd.InstalledCallbacks) assert(false);
    //bd.PrevUserCallbackWindowFocus = glfwSetWindowFocusCallback(window, TexGui_ImplGlfw_WindowFocusCallback);
    //bd.PrevUserCallbackCursorEnter = glfwSetCursorEnterCallback(window, TexGui_ImplGlfw_CursorEnterCallback);
    //bd.PrevUserCallbackScroll = glfwSetScrollCallback(window, TexGui_ImplGlfw_ScrollCallback);
    //bd.PrevUserCallbackMonitor = glfwSetMonitorCallback(TexGui_ImplGlfw_MonitorCallback);
    bd.PrevUserCallbackCursorPos       = glfwSetCursorPosCallback(window, TexGui_ImplGlfw_CursorPosCallback);
    bd.PrevUserCallbackMousebutton     = glfwSetMouseButtonCallback(window, TexGui_ImplGlfw_MouseButtonCallback);
    bd.PrevUserCallbackKey             = glfwSetKeyCallback(window, TexGui_ImplGlfw_KeyCallback);
    bd.PrevUserCallbackChar            = glfwSetCharCallback(window, TexGui_ImplGlfw_CharCallback);
    bd.PrevUserCallbackFramebufferSize = glfwSetFramebufferSizeCallback(window, TexGui_ImplGlfw_FramebufferSizeCallback);
    bd.InstalledCallbacks = true;

    return true;
} 

bool TexGui::initGlfwOpenGL(GLFWwindow* window)
{
    GTexGui = new TexGuiContext();
    GTexGui->renderCtx.initFromGlfwWindow(window);
    initGlfwCallbacks(window);
    return true;
} 

void TexGui::render()
{
    GTexGui->renderCtx.renderFromRD(renderData);
}

void TexGui::loadFont(const char* font)
{
    GTexGui->renderCtx.loadFont(font);
}

void TexGui::loadTextures(const char* dir)
{
    GTexGui->renderCtx.loadTextures(dir);
}

void TexGui::clear()
{
    auto& io = GTexGui->io;
    if (io.lmb == KEY_Press) io.lmb = KEY_Held;
    if (io.lmb == KEY_Release) io.lmb = KEY_Off;
    renderData.clear();
    if (!io.charQueue.empty())
        io.charQueue.pop();
}

inline void setBit(unsigned int& dest, const unsigned int flag, bool on)
{
    dest = on ? dest | flag : dest & ~(flag);
}

inline bool getBit(const unsigned int dest, const unsigned int flag)
{
    return dest & flag;
}

using namespace Math;
uint32_t getBoxState(uint32_t& state, fbox box)
{
    auto& io = GTexGui->io;
    if (box.contains(io.cursorPos))
    {
        state |= STATE_HOVER;

        if (io.lmb == KEY_Press) {
            state |= STATE_PRESS;
            state |= STATE_ACTIVE;
        }

        if (io.lmb == KEY_Release)
            setBit(state, STATE_PRESS, 0);

        return state;
    }

    setBit(state, STATE_HOVER, 0);
    setBit(state, STATE_PRESS, 0);

    if (io.lmb == KEY_Press)
        setBit(state, STATE_ACTIVE, 0);

    return state;
}

#define _PX Defaults::PixelSize
extern std::unordered_map<std::string, TexEntry> m_tex_map;
Container Container::Window(const char* name, float xpos, float ypos, float width, float height, uint32_t flags)
{
    auto& io = GTexGui->io;
    if (!GTexGui->windows.contains(name))
    {
        GTexGui->windows.insert({name, {
            .box = {xpos, ypos, width, height},
            .initial_box = {xpos, ypos, width, height},
        }});
    }

    WindowState& wstate = GTexGui->windows[name];

    static TexEntry* wintex = &m_tex_map[Defaults::Window::Texture];

    getBoxState(wstate.state, wstate.box);
    
    if (io.lmb != KEY_Held)
    {
        wstate.moving = false;
        wstate.resizing = false;
    }

    if (fbox(wstate.box.x, wstate.box.y, wstate.box.width, wintex->top * _PX).contains(io.cursorPos) && io.lmb == KEY_Press)
    {
        wstate.last_cursorPos = io.cursorPos;
        wstate.moving = true;
    }

    if (fbox(wstate.box.x + wstate.box.width - wintex->right * _PX,
             wstate.box.y + wstate.box.height - wintex->bottom * _PX,
             wintex->right * _PX, wintex->bottom * _PX).contains(io.cursorPos) && io.lmb == KEY_Press)
    {
        wstate.last_cursorPos = io.cursorPos;
        wstate.resizing = true;
    }

    if (wstate.moving)
    {
        wstate.box.pos += io.cursorPos - wstate.last_cursorPos;
        wstate.last_cursorPos = io.cursorPos;
    }
    else if (wstate.resizing)
    {
        wstate.box.size += io.cursorPos - wstate.last_cursorPos;
        wstate.last_cursorPos = io.cursorPos;
    }

    fvec4 padding = Defaults::Window::Padding;
    padding.top = wintex->top * _PX;

    renderData.drawTexture(wstate.box, wintex, wstate.state, _PX, SLICE_9);
    renderData.drawText(name, {wstate.box.x + padding.left, wstate.box.y + wintex->top * _PX / 2},
                 Defaults::Font::Color, Defaults::Font::Scale, CENTER_Y);

    fbox internal = fbox::pad(wstate.box, padding);

    buttonStates = &wstate.buttonStates;
    return withBounds(internal);

}

bool Container::Button(const char* text)
{
    if (!buttonStates->contains(text))
    {
        buttonStates->insert({text, 0});
    }

    uint32_t& state = buttonStates->at(text);
    getBoxState(state, bounds);

    renderData.drawTexture(bounds, &m_tex_map[Defaults::Button::Texture], state, _PX, SLICE_9);

    renderData.drawText(text, 
        state & STATE_PRESS ? bounds.pos + bounds.size / 2 + Defaults::Button::POffset
                            : bounds.pos + bounds.size / 2,
        Defaults::Font::Color, Defaults::Font::Scale, CENTER_X | CENTER_Y);

    auto& io = GTexGui->io;
    return state & STATE_ACTIVE && io.lmb == KEY_Release && bounds.contains(io.cursorPos) ? true : false;
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
        renderData.drawTexture(boxBounds, boxtex, 0, 2, SLICE_9);
    }

    fbox internal = fbox::pad(boxBounds, Defaults::Box::Padding);
    return withBounds(internal);
}

void Container::TextInput(const char* name, std::string& buf)
{
    auto& io = GTexGui->io;
    static TexEntry* inputtex = &m_tex_map[Defaults::TextInput::Texture];
    if (!GTexGui->textInputs.contains(name))
    {
        GTexGui->textInputs.insert({name, {}});
    }
    
    auto& tstate = GTexGui->textInputs[name];

    getBoxState(tstate.state, bounds);
    
    if (!io.charQueue.empty() && tstate.state & STATE_ACTIVE)
    {
        buf.push_back(io.charQueue.front());
    }

    renderData.drawTexture(bounds, inputtex, tstate.state, _PX, SLICE_9);
    float offsetx = 0;
    fvec4 padding = Defaults::TextInput::Padding;
    fvec4 color = Defaults::Font::Color;
    renderData.drawText(
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
