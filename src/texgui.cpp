#include "texgui.h"
#include "context.h"
#include "types.h"
#include "GLFW/glfw3.h"
#include <cassert>
#include <cstring>
#include <cmath>
#include <mutex>
#include <list>

using namespace TexGui;

std::mutex TGInputLock;

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

RenderData TGRenderData;

GLFWscrollfun TexGui_ImplGlfw_ScrollCallback;
GLFWmonitorfun TexGui_ImplGlfw_MonitorCallback; 

void TexGui_ImplGlfw_CursorPosCallback(GLFWwindow* window, double x, double y)
{
    auto& bd = GTexGui->backendData;
    auto& io = GTexGui->io;
    if (bd.PrevUserCallbackCursorPos)
        bd.PrevUserCallbackCursorPos(window, x, y);

    //#TODO: window scale
    std::lock_guard<std::mutex> lock(TGInputLock);
    io.cursorPos = Math::fvec2(x, y);
}

void TexGui_ImplGlfw_KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto& bd = GTexGui->backendData;
    auto& io = GTexGui->io;
    if (bd.PrevUserCallbackKey)
        bd.PrevUserCallbackKey(window, key, scancode, action, mods);

    std::lock_guard<std::mutex> lock(TGInputLock);
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

    std::lock_guard<std::mutex> lock(TGInputLock);
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

    std::lock_guard<std::mutex> lock(TGInputLock);
    io.charQueue.push(codepoint);
};

void TexGui_ImplGlfw_FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    auto& bd = GTexGui->backendData;
    if (bd.PrevUserCallbackFramebufferSize)
        bd.PrevUserCallbackFramebufferSize(window, width, height);

    GTexGui->renderCtx.setScreenSize(width, height);

    std::lock_guard<std::mutex> lock(TGInputLock);
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
    GTexGui->renderCtx.renderFromRD(TGRenderData);
}

void TexGui::loadFont(const char* font)
{
    GTexGui->renderCtx.loadFont(font);
}

void TexGui::loadTextures(const char* dir)
{
    GTexGui->renderCtx.loadTextures(dir);
}

extern std::unordered_map<std::string, TexGui::TexEntry> m_tex_map;

// We just need pointer stability, we aren't gonna be iterating it so using list :P
// This is a heap alloc per entry which is a bit of a pain so should change since we barely use heap at all otherwise #TODO
static std::list<TexEntry> m_custom_texs;

TexEntry* TexGui::texByName(const char* name)
{
    assert(m_tex_map.contains(name));
    return &m_tex_map[name];
}

TexEntry* TexGui::customTexture(unsigned int glTexID, unsigned int layer, Math::ibox pixelBounds)
{
    float xth = pixelBounds.w / 3.f;
    float yth = pixelBounds.h / 3.f;
    return &m_custom_texs.emplace_back(glTexID, layer, pixelBounds, 0, yth, xth, yth, xth);
}

struct InputFrame
{
    Math::fvec2 cursorPos;
    uint32_t lmb = 0;
    unsigned int character = 0;
};

InputFrame inputFrame;

void TexGui::clear()
{
    TGRenderData.clear();

    std::lock_guard<std::mutex> lock(TGInputLock);
    auto& io = GTexGui->io;

    inputFrame.cursorPos = io.cursorPos;
    inputFrame.lmb = io.lmb;

    if (io.lmb == KEY_Press) io.lmb = KEY_Held;
    if (io.lmb == KEY_Release) io.lmb = KEY_Off;
    if (!io.charQueue.empty())
    {
        inputFrame.character = io.charQueue.front();
        io.charQueue.pop();
    }
    else
    {
        inputFrame.character = 0;
    }
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
    auto& io = inputFrame;
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
Container Container::Window(const char* name, float xpos, float ypos, float width, float height, uint32_t flags, TexEntry* texture)
{
    auto& io = inputFrame;
    if (!GTexGui->windows.contains(name))
    {
        GTexGui->windows.insert({name, {
            .box = {xpos, ypos, width, height},
            .initial_box = {xpos, ypos, width, height},
        }});
    }

    WindowState& wstate = GTexGui->windows[name];

    static TexEntry* wintex = &m_tex_map[Defaults::Window::Texture];
    if (!texture) texture = wintex;

    getBoxState(wstate.state, wstate.box);
    
    if (io.lmb != KEY_Held)
    {
        wstate.moving = false;
        wstate.resizing = false;
    }

    if (fbox(wstate.box.x, wstate.box.y, wstate.box.width, texture->top * _PX).contains(io.cursorPos) && io.lmb == KEY_Press)
    {
        wstate.last_cursorPos = io.cursorPos;
        wstate.moving = true;
    }

    if (fbox(wstate.box.x + wstate.box.width - texture->right * _PX,
             wstate.box.y + wstate.box.height - texture->bottom * _PX,
             texture->right * _PX, texture->bottom * _PX).contains(io.cursorPos) && io.lmb == KEY_Press)
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
    padding.top = texture->top * _PX;

    TGRenderData.drawTexture(wstate.box, texture, wstate.state, _PX, SLICE_9);
    TGRenderData.drawText(name, {wstate.box.x + padding.left, wstate.box.y + texture->top * _PX / 2},
                 Defaults::Font::Color, Defaults::Font::Size, CENTER_Y);

    fbox internal = fbox::pad(wstate.box, padding);

    buttonStates = &wstate.buttonStates;
    return withBounds(internal);

}

bool Container::Button(const char* text, TexEntry* texture)
{
    if (!buttonStates->contains(text))
    {
        buttonStates->insert({text, 0});
    }

    uint32_t& state = buttonStates->at(text);
    getBoxState(state, bounds);

    static TexEntry* defaultTex = &m_tex_map[Defaults::Button::Texture];
    auto* tex = texture ? texture : defaultTex;

    TGRenderData.drawTexture(bounds, tex, state, _PX, SLICE_9);

    TGRenderData.drawText(text, 
        state & STATE_PRESS ? bounds.pos + bounds.size / 2 + Defaults::Button::POffset
                            : bounds.pos + bounds.size / 2,
        Defaults::Font::Color, Defaults::Font::Size, CENTER_X | CENTER_Y);

    auto& io = inputFrame;
    return state & STATE_ACTIVE && io.lmb == KEY_Release && bounds.contains(io.cursorPos) ? true : false;
}

Container Container::Box(float xpos, float ypos, float width, float height, TexEntry* texture)
{
    if (width <= 1)
        width = width == 0 ? bounds.width : bounds.width * width;
    if (height <= 1)
        height = height == 0 ? bounds.height : bounds.height * height;

    fbox boxBounds(bounds.x + xpos, bounds.y + ypos, width, height);
    if (texture != nullptr)
    {
        TGRenderData.drawTexture(boxBounds, texture, 0, 2, SLICE_9);
    }

    fbox internal = fbox::pad(boxBounds, Defaults::Box::Padding);
    return withBounds(internal);
}

void Container::Image(TexEntry* texture)
{
    ivec2 tsize = ivec2(texture->bounds.width, texture->bounds.height) * _PX;

    fbox sized = fbox(bounds.pos, fvec2(tsize));
    fbox arranged = Arrange(this, sized);

    TGRenderData.drawTexture(arranged, texture, STATE_NONE, _PX, 0);
}

fbox Container::Arrange(Container* o, fbox child)
{
    // This container doesn't arrange anything.
    if (!o->arrange) return child;

    return o->arrange(o, child);
}

// Arranges the list item based on the thing that is put inside it.
Container Container::ListItem(uint32_t* selected, uint32_t id)
{
    static auto arrange = [](Container* listItem, fbox child)
    {
        // A bit scuffed since we add margins then unpad them but mehh
        fbox bounds = fbox::margin(child, Defaults::ListItem::Padding);
        // Get the parent to arrange the list item
        bounds = Arrange(listItem->parent, bounds);
        
        static TexEntry* tex = &m_tex_map[Defaults::ListItem::Texture];

        auto& io = inputFrame;

        bool hovered = bounds.contains(io.cursorPos);
        auto state = hovered ? STATE_HOVER : STATE_NONE;
        if (io.lmb == KEY_Release && hovered)
            *(listItem->listItem.selected) = listItem->listItem.id;

        if (*(listItem->listItem.selected) == listItem->listItem.id)
            state = STATE_ACTIVE;

        TGRenderData.drawTexture(bounds, tex, state, _PX, SLICE_9);

        // The child is positioned by the list item's parent
        return fbox::pad(bounds, Defaults::ListItem::Padding);
    };
    // Add padding to the contents of the list item
    Container listItem = withBounds(fbox::pad(bounds, Defaults::ListItem::Padding), arrange);
    listItem.listItem = { selected, id };
    return listItem;
}

// Arranges the cells of a grid by adding a new child box to it.
Container Container::Grid()
{
    static auto arrange = [](Container* grid, fbox child)
    {
        // Don't nest grids in other arrangers
        assert(!grid->parent->arrange);

        auto& gs = grid->grid;

        gs.rowHeight = fmax(gs.rowHeight, child.height);


        float spacing = Defaults::Row::Spacing;
        if (gs.x + child.width > grid->bounds.width)
        {
            gs.x = 0;
            gs.y += gs.rowHeight + spacing;
            gs.rowHeight = 0;
            child = fbox(grid->bounds.pos + fvec2(gs.x, gs.y), child.size);
        }
        else
        {
            child = fbox(grid->bounds.pos + fvec2(gs.x, gs.y), child.size);
            gs.x += child.width + spacing;
        }
        
        return child;
    };
    Container grid = withBounds(bounds, arrange);
    grid.grid = { 0, 0, 0 };
    return grid;
}

void Container::TextInput(const char* name, std::string& buf)
{
    auto& io = inputFrame;
    static TexEntry* inputtex = &m_tex_map[Defaults::TextInput::Texture];
    if (!GTexGui->textInputs.contains(name))
    {
        GTexGui->textInputs.insert({name, {}});
    }
    
    auto& tstate = GTexGui->textInputs[name];

    getBoxState(tstate.state, bounds);
    
    if (io.character != 0 && tstate.state & STATE_ACTIVE)
    {
        buf.push_back(io.character);
    }

    TGRenderData.drawTexture(bounds, inputtex, tstate.state, _PX, SLICE_9);
    float offsetx = 0;
    fvec4 padding = Defaults::TextInput::Padding;
    fvec4 color = Defaults::Font::Color;
    TGRenderData.drawText(
        !getBit(tstate.state, STATE_ACTIVE) && buf.size() == 0
        ? name : buf.c_str(),
        {bounds.x + offsetx + padding.left, bounds.y + bounds.height / 2},
        Defaults::Font::Color,
        Defaults::Font::Size,
        CENTER_Y
    );
}

void Container::Row_Internal(Container* out, const float* widths, uint32_t n, float height, uint32_t flags)
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
            width = (bounds.width - absoluteWidth - (spacing * (n - 1))) * mult / inherit;
        }
        else
            width = widths[i];

        if (flags & WRAPPED && x + width > bounds.width)
        {
            x = 0;
            y += height + spacing;
        }

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
    int inherit = 0;
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
            height = (bounds.height - absoluteHeight - (spacing * (n - 1))) * mult / inherit;
        }
        else
            height = heights[i];
        
        out[i] = *this;
        out[i].bounds = Math::fbox(bounds.x + x, bounds.y + y, width, height);

        y += height + spacing;
    }
}
