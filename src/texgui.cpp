#include "texgui.h"
#include "context.h"
#include "types.h"
#include "GLFW/glfw3.h"
#include <cassert>
#include <cstring>
#include <cmath>
#include <mutex>
#include <list>
#include <queue>

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

enum KeyState : int
{
    KEY_Off = 0,
    KEY_Press = 1,
    KEY_Held = 2,
    KEY_Release = 3,
};

struct InputData {
    int keyStates[GLFW_KEY_LAST + 1];
    int mouseStates[GLFW_MOUSE_BUTTON_LAST + 1];
    int mods;

    int& lmb = mouseStates[GLFW_MOUSE_BUTTON_LEFT];

    Math::fvec2 cursorPos;
    Math::fvec2 mouseRelativeMotion;
    Math::fvec2 scroll;
    std::queue<unsigned int> charQueue;

    bool firstMouse = false;

    bool backspace = false;

    InputData() { memset(keyStates, KEY_Off, sizeof(int)*(GLFW_KEY_LAST+1));
                  memset(mouseStates, KEY_Off, sizeof(int)*(GLFW_MOUSE_BUTTON_LAST+1)); }
};

struct ScrollPanelState
{
    Math::fvec2 contentPos;
    Math::fbox bounds;
};

struct TexGuiContext
{
    ImplGlfw_Data backendData;
    std::unordered_map<std::string, WindowState> windows;
    std::unordered_map<std::string, TextInputState> textInputs;
    std::unordered_map<std::string, ScrollPanelState> scrollPanels;
    GLContext renderCtx;
    InputData io;
};

TexGuiContext* GTexGui = nullptr;

RenderData TGRenderData;
RenderData TGSyncedRenderData;

GLFWmonitorfun TexGui_ImplGlfw_MonitorCallback; 

void TexGui_ImplGlfw_ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    auto& bd = GTexGui->backendData;
    auto& io = GTexGui->io;
    if (bd.PrevUserCallbackScroll)
        bd.PrevUserCallbackScroll(window, xoffset, yoffset);

    //#TODO: window scale
    std::lock_guard<std::mutex> lock(TGInputLock);
    io.scroll += Math::fvec2(xoffset, yoffset);
}

void TexGui_ImplGlfw_CursorPosCallback(GLFWwindow* window, double x, double y)
{
    auto& bd = GTexGui->backendData;
    auto& io = GTexGui->io;
    if (bd.PrevUserCallbackCursorPos)
        bd.PrevUserCallbackCursorPos(window, x, y);

    //#TODO: window scale

    std::lock_guard<std::mutex> lock(TGInputLock);
    if (!io.firstMouse)
        io.firstMouse = true;
    else
        io.mouseRelativeMotion += Math::fvec2(x - io.cursorPos.x, y - io.cursorPos.y);

    io.cursorPos = Math::fvec2(x, y);
}

void TexGui_ImplGlfw_KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto& bd = GTexGui->backendData;
    auto& io = GTexGui->io;
    if (bd.PrevUserCallbackKey)
        bd.PrevUserCallbackKey(window, key, scancode, action, mods);

    if (key == GLFW_KEY_BACKSPACE
        && (action == GLFW_PRESS || action == GLFW_REPEAT)) 
        io.backspace = true;

    std::lock_guard<std::mutex> lock(TGInputLock);
}

void TexGui_ImplGlfw_MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    auto& bd = GTexGui->backendData;
    auto& io = GTexGui->io;

    if (bd.PrevUserCallbackMousebutton)
        bd.PrevUserCallbackMousebutton(window, button, action, mods);

    std::lock_guard<std::mutex> lock(TGInputLock);
    if (action == GLFW_RELEASE) io.mouseStates[button] = KEY_Release;
    else if (io.mouseStates[button] == KEY_Off) io.mouseStates[button] = KEY_Press;
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

bool TexGui::initGlfwOpenGL(GLFWwindow* window)
{
    GTexGui = new TexGuiContext();
    GTexGui->renderCtx.initFromGlfwWindow(window);
    initGlfwCallbacks(window);
    Base.rs = &TGRenderData;
    return true;
} 

void TexGui::render()
{
    if (Defaults::Settings::Async)
    {
        GTexGui->renderCtx.renderFromRD(TGRenderData);
        return;
    }

    GTexGui->renderCtx.renderFromRD(TGSyncedRenderData);
}

void TexGui::render(RenderData& data)
{
    GTexGui->renderCtx.renderFromRD(data);
}

RenderData* TexGui::newRenderData()
{
    return new RenderData();
}

void TexGui::loadFont(const char* font)
{
    GTexGui->renderCtx.loadFont(font);
}

void TexGui::loadTextures(const char* dir)
{
    GTexGui->renderCtx.loadTextures(dir);
}

IconSheet TexGui::loadIcons(const char* dir, int32_t iconWidth, int32_t iconHeight)
{
    return GTexGui->renderCtx.loadIcons(dir, iconWidth, iconHeight);
}

extern std::unordered_map<std::string, TexGui::TexEntry> m_tex_map;

// We just need pointer stability, we aren't gonna be iterating it so using list :P
// This is a heap alloc per entry which is a bit of a pain so should change since we barely use heap at all otherwise #TODO
static std::list<TexEntry> m_custom_texs;

TexEntry* TexGui::texByName(const char* name)
{
    if (!m_tex_map.contains(name)) return nullptr;
    return &m_tex_map[name];
}

TexEntry* TexGui::customTexture(unsigned int glTexID, unsigned int layer, Math::ibox pixelBounds)
{
    float xth = pixelBounds.w / 3.f;
    float yth = pixelBounds.h / 3.f;
    return &m_custom_texs.emplace_back(glTexID, layer, pixelBounds, 0, yth, xth, yth, xth);
}

Math::ivec2 TexGui::getTexSize(TexEntry* tex)
{
    return tex->bounds.wh;
}

struct InputFrame
{
    int keyStates[GLFW_KEY_LAST + 1];
    int mouseStates[GLFW_MOUSE_BUTTON_LAST + 1];
    int mods;

    int& lmb = mouseStates[GLFW_MOUSE_BUTTON_LEFT];

    Math::fvec2 cursorPos;
    Math::fvec2 mouseRelativeMotion;
    Math::fvec2 scroll;

    unsigned int character;

    bool backspace;
};

InputFrame inputFrame;

inline static void clearInput()
{
    auto& io = GTexGui->io;
    std::lock_guard<std::mutex> lock(TGInputLock);

    inputFrame.mods = io.mods;

    inputFrame.cursorPos = io.cursorPos;
    inputFrame.scroll = io.scroll;
    inputFrame.mouseRelativeMotion = io.mouseRelativeMotion;
    inputFrame.backspace = io.backspace;
    io.backspace = false;
    io.mouseRelativeMotion = {0, 0};

    TexGui::CapturingMouse = false;
    io.scroll = {0, 0};

    memcpy(inputFrame.mouseStates, io.mouseStates, sizeof(int) * GLFW_MOUSE_BUTTON_LAST + 1);
    memcpy(inputFrame.keyStates, io.keyStates, sizeof(int) * GLFW_KEY_LAST + 1);
    //if press and release happen too fast (glfw thread polling faster than sim thread), the press event can be lost. but that never should happen so watever
    for (int i = 0; i < GLFW_MOUSE_BUTTON_LAST + 1; i++)
    {
        if (io.mouseStates[i] == KEY_Press) io.mouseStates[i] = KEY_Held;
        if (io.mouseStates[i] == KEY_Release) io.mouseStates[i] = KEY_Off;
    }

    for (int i = 0; i < GLFW_KEY_LAST + 1; i++)
    {
        if (io.keyStates[i] == KEY_Press) io.keyStates[i] = KEY_Held;
        if (io.keyStates[i] == KEY_Release) io.keyStates[i] = KEY_Off;
    }

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

void TexGui::clear()
{
    if (!Defaults::Settings::Async)
    {
        TGSyncedRenderData.clear();
        TGSyncedRenderData.swap(TGRenderData);
    }

    TGRenderData.clear();
    clearInput();
}

void TexGui::clear(RenderData& rs)
{
    if (!Defaults::Settings::Async)
    {
        TGSyncedRenderData.clear();
        TGSyncedRenderData.swap(TGRenderData);
    }

    TGRenderData.clear();
    clearInput();
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

fbox AlignBox(fbox bounds, fbox child, uint32_t f)
{
    if (f & ALIGN_LEFT) child.x = bounds.x;
    if (f & ALIGN_RIGHT) child.x = bounds.x + bounds.width - child.width;

    if (f & ALIGN_TOP) child.y = bounds.y;
    if (f & ALIGN_BOTTOM) child.y = bounds.y + bounds.height - child.height;

    if (f & ALIGN_CENTER_X) child.x = bounds.x + (bounds.width - child.width) / 2;
    if (f & ALIGN_CENTER_Y) child.y = bounds.y + (bounds.height - child.height) / 2;
    return child;
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

    static TexEntry* wintex = &m_tex_map[Defaults::Window::Texture];
    if (!texture) texture = wintex;
    WindowState& wstate = GTexGui->windows[name];

    if (flags & CAPTURE_INPUT && wstate.box.contains(io.cursorPos)) CapturingMouse = true;

    if (flags & LOCKED)
    {
        wstate.box = AlignBox(Base.bounds, wstate.initial_box, flags);
    }
    else
    {
        getBoxState(wstate.state, wstate.box);
        
        if (io.lmb != KEY_Held)
        {
            wstate.moving = false;
            wstate.resizing = false;
        }

        if (fbox(wstate.box.x, wstate.box.y, wstate.box.width, texture->top * _PX).contains(io.cursorPos) && io.lmb == KEY_Held)
            wstate.moving = true;

        if (flags & RESIZABLE && fbox(wstate.box.x + wstate.box.width - texture->right * _PX,
                 wstate.box.y + wstate.box.height - texture->bottom * _PX,
                 texture->right * _PX, texture->bottom * _PX).contains(io.cursorPos) && io.lmb == KEY_Held)
            wstate.resizing = true;

        if (wstate.moving)
            wstate.box.pos += io.mouseRelativeMotion;
        else if (wstate.resizing)
            wstate.box.size += io.mouseRelativeMotion;
    }

    fvec4 padding = Defaults::Window::Padding;
    padding.top = texture->top * _PX;

    rs->drawTexture(wstate.box, texture, wstate.state, _PX, SLICE_9, scissorBox);

    if (!(flags & HIDE_TITLE)) 
        rs->drawText(name, {wstate.box.x + padding.left, wstate.box.y + texture->top * _PX / 2},
                 Defaults::Font::Color, Defaults::Font::Size, CENTER_Y);

    fbox internal = fbox::pad(wstate.box, padding);

    buttonStates = &wstate.buttonStates;
    return withBounds(internal);

}

bool Container::Button(const char* text, TexEntry* texture, Container* out)
{
    if (!buttonStates->contains(text))
    {
        buttonStates->insert({text, 0});
    }

    uint32_t& state = buttonStates->at(text);
    getBoxState(state, bounds);

    static TexEntry* defaultTex = &m_tex_map[Defaults::Button::Texture];
    auto* tex = texture ? texture : defaultTex;

    rs->drawTexture(bounds, tex, state, _PX, SLICE_9, scissorBox);

    vec2 pos = state & STATE_PRESS 
                       ? bounds.pos + Defaults::Button::POffset
                       : bounds.pos;
    fbox innerBounds = {pos, bounds.size};

    if (out)
        *out = withBounds(innerBounds);
    else
    {
        innerBounds.pos += bounds.size / 2;
        rs->drawText(text, innerBounds, Defaults::Font::Color, Defaults::Font::Size, CENTER_X | CENTER_Y);
    }

    auto& io = inputFrame;

    bool hovered = scissorBox.contains(io.cursorPos) 
                && bounds.contains(io.cursorPos);

    return state & STATE_ACTIVE && io.lmb == KEY_Release && hovered ? true : false;
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
        rs->drawTexture(boxBounds, texture, 0, 2, SLICE_9, scissorBox);
    }

    fbox internal = fbox::pad(boxBounds, Defaults::Box::Padding);
    return withBounds(internal);
}

void Container::CheckBox(bool* val)
{
    static TexEntry* texture = &m_tex_map[Defaults::CheckBox::Texture];

    auto& io = inputFrame;
    if (io.lmb == KEY_Release && bounds.contains(io.cursorPos)) *val = !*val;

    if (texture == nullptr) return;
    rs->drawTexture(bounds, texture, *val ? STATE_ACTIVE : 0, 2, SLICE_9, scissorBox);
}

void Container::RadioButton(uint32_t* selected, uint32_t id)
{
    static TexEntry* texture = &m_tex_map[Defaults::RadioButton::Texture];

    auto& io = inputFrame;
    if (io.lmb == KEY_Release && bounds.contains(io.cursorPos)) *selected = id;

    if (texture == nullptr) return;
    rs->drawTexture(bounds, texture, *selected == id ? STATE_ACTIVE : 0, 2, SLICE_9, scissorBox);

}

Container Container::ScrollPanel(const char* name, TexEntry* texture, TexEntry* bartex)
{
    auto& io = inputFrame;
    if (!GTexGui->scrollPanels.contains(name))
    {
        ScrollPanelState _s = 
        {
            .contentPos = {0,0},
            .bounds = bounds 
        };
        GTexGui->scrollPanels.insert({name, _s});
    }

    auto& spstate = GTexGui->scrollPanels[name];

    fvec4 padding = Defaults::ScrollPanel::Padding;
    if (bartex)
        padding.right += bartex->bounds.width;

    static auto arrange = [](Container* scroll, fbox child)
    {
        auto& bounds = ((ScrollPanelState*)(scroll->scrollPanelState))->bounds;
        bounds = child;
        return bounds;
    };

    Container sp = withBounds(Math::fbox::pad(bounds, padding), arrange);
    sp.scrollPanelState = &spstate;
    sp.scissorBox = sp.bounds;

    if (sp.scissorBox.contains(io.cursorPos))
        spstate.contentPos.y += inputFrame.scroll.y * 30;

    float height = sp.bounds.height - padding.top - padding.bottom;
    if (spstate.bounds.height + spstate.contentPos.y < height)
        spstate.contentPos.y = -(spstate.bounds.height - height);
    spstate.contentPos.y = fmin(spstate.contentPos.y, 0);

    float barh = fmin(bounds.height, bounds.height * height / spstate.bounds.height);
    fbox bar = {bounds.x + bounds.width - padding.right,
                bounds.y + (bounds.height - barh) * -spstate.contentPos.y / (spstate.bounds.height - height),
                padding.right,
                barh};

    rs->drawTexture(bounds, texture, 0, _PX, 0, bounds);
    if (bartex)
        rs->drawTexture(bar, bartex, 0, 2, SLICE_9, bounds);
    else
        rs->drawQuad(bar, {1,1,1,1});

    sp.bounds.y += spstate.contentPos.y;

    return sp;
}

void Container::Image(TexEntry* texture, int scale)
{
    ivec2 tsize = ivec2(texture->bounds.width, texture->bounds.height) * scale;

    fbox sized = fbox(bounds.pos, fvec2(tsize));
    fbox arranged = Arrange(this, sized);

    rs->drawTexture(arranged, texture, STATE_NONE, scale, 0, scissorBox);
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

        if (!listItem->scissorBox.contains(bounds))
        {
            return fbox(0,0,0,0);
        }

        auto state = STATE_NONE;
        if (listItem->listItem.selected != nullptr)
        {
            auto& io = inputFrame;

            bool hovered = listItem->scissorBox.contains(io.cursorPos) 
                        && bounds.contains(io.cursorPos);
            state = hovered ? STATE_HOVER : STATE_NONE;
            if (io.lmb == KEY_Release && hovered)
                *(listItem->listItem.selected) = listItem->listItem.id;

            if (*(listItem->listItem.selected) == listItem->listItem.id)
                state = STATE_ACTIVE;
        }
        else
        {
            state = listItem->listItem.id ? STATE_ACTIVE : STATE_NONE;
        }

        listItem->rs->drawTexture(bounds, tex, state, _PX, SLICE_9, listItem->scissorBox);

        // The child is positioned by the list item's parent
        return fbox::pad(bounds, Defaults::ListItem::Padding);
    };
    // Add padding to the contents of the list item
    Container listItem = withBounds(fbox::pad(bounds, Defaults::ListItem::Padding), arrange);
    listItem.listItem = { selected, id };
    return listItem;
}

Container Container::Align(uint32_t flags, Math::fvec4 padding)
{
    static auto arrange = [](Container* align, fbox child)
    {
        Math::fvec4 pad = {align->align.top, align->align.right, align->align.bottom, align->align.left};
        fbox bounds = fbox::margin(child, pad);
        bounds = Arrange(align->parent, bounds);
        return fbox::pad(AlignBox(align->bounds, bounds, align->align.flags), pad);
    };
    // Add padding to the contents
    Container aligner = withBounds(fbox::pad(bounds, padding), arrange);
    aligner.align = {
        flags,
        padding.x, padding.y, padding.z, padding.w,
    };
    return aligner;
}

// Arranges the cells of a grid by adding a new child box to it.
Container Container::Grid()
{
    static auto arrange = [](Container* grid, fbox child)
    {
        auto& gs = grid->grid;

        gs.rowHeight = fmax(gs.rowHeight, child.height);

        float spacing = Defaults::Row::Spacing;
        if (gs.x + child.width > grid->bounds.width && gs.n > 0)
        {
            gs.x = 0;
            gs.y += gs.rowHeight + spacing;
            child = fbox(grid->bounds.pos + fvec2(gs.x, gs.y), child.size);
            gs.rowHeight = 0;
        }
        else
            child = fbox(grid->bounds.pos + fvec2(gs.x, gs.y), child.size);

        gs.x += child.width + spacing;

        Arrange(grid->parent, {grid->bounds.x, grid->bounds.y, grid->bounds.width, gs.y + gs.rowHeight + spacing});

        gs.n++;
        
        return child;
    };
    Container grid = withBounds(bounds, arrange);
    grid.grid = { 0, 0, 0, 0 };
    return grid;
}
void Container::Divider(float padding)
{
    float width = 2;
    fbox ln = Arrange(this, {bounds.x,bounds.y, this->bounds.width, width + padding*2.f});
    fbox line = {
        ln.x, ln.y + padding, ln.width, width,
    };
    rs->drawQuad(line, fvec4(1,1,1,0.5), 1);
}

Container Container::Stack(float padding)
{
    static auto arrange = [](Container* stack, fbox child)
    {
        auto& s = stack->stack;

        child = fbox(stack->bounds.pos + fvec2(0, s.y), child.size);
        s.y += child.size.y + s.padding;
    
        Arrange(stack->parent, {stack->bounds.x, stack->bounds.y, stack->bounds.width, s.y});

        return child;
    };
    Container stack = withBounds(bounds, arrange);
    stack.stack = {0, padding < 0 ? Defaults::Stack::Padding : padding};
    return stack;
}

static bool textInputUpdate(TextInputState& tstate, unsigned int* c, CharacterFilter filter)
{
    auto& io = inputFrame;
    *c = 0;

    if (!(tstate.state & STATE_ACTIVE)) return false;

    if (io.character != 0 && (filter == nullptr || filter(io.character)))
    {
        *c = io.character;
        return false;
    }
    if (io.backspace)
    {
        return true;
    }
    return false;
}

void renderTextInput(RenderData* rs, const char* name, fbox bounds, fbox scissor, TextInputState& tstate, const char* text, int32_t len)
{
    static TexEntry* inputtex = &m_tex_map[Defaults::TextInput::Texture];

    rs->drawTexture(bounds, inputtex, tstate.state, _PX, SLICE_9, scissor);
    float offsetx = 0;
    fvec4 padding = Defaults::TextInput::Padding;
    fvec4 color = Defaults::Font::Color;
    rs->drawText(
        !getBit(tstate.state, STATE_ACTIVE) && len == 0
        ? name : text,
        {bounds.x + offsetx + padding.left, bounds.y + bounds.height / 2},
        Defaults::Font::Color,
        Defaults::Font::Size,
        CENTER_Y
    );
}

void Container::TextInput(const char* name, std::string& buf, CharacterFilter filter)
{
    if (!GTexGui->textInputs.contains(name))
    {
        GTexGui->textInputs.insert({name, {}});
    }
    
    auto& tstate = GTexGui->textInputs[name];
    getBoxState(tstate.state, bounds);
    
    unsigned int c;
    if (textInputUpdate(tstate, &c, filter))
    {
        if (buf.size() > 0) buf.pop_back();
    }
    else if (c)
        buf.push_back(c);
    
    renderTextInput(rs, name, bounds, scissorBox, tstate, buf.c_str(), buf.size());
}

void Container::TextInput(const char* name, char* buf, uint32_t bufsize, CharacterFilter filter)
{
    auto& io = inputFrame;
    
    if (!GTexGui->textInputs.contains(name))
    {
        GTexGui->textInputs.insert({name, {}});
    }
    
    auto& tstate = GTexGui->textInputs[name];

    getBoxState(tstate.state, bounds);
    int32_t tlen = strlen(buf);
    
    unsigned int c;
    if (textInputUpdate(tstate, &c, filter))
    {
        if (tlen > 0)
        {
            // Backspace
            tlen -= 1;
            buf[tlen] = '\0';
        }
    }
    else if (c && tlen < bufsize - 1)
    {
        buf[tlen] = io.character;
        buf[tlen + 1] = '\0';
        tlen += 1;
    }
    renderTextInput(rs, name, bounds, scissorBox, tstate, buf, tlen);
}

static void renderTooltip(Paragraph text, RenderData* rs);

// Bump the line of text
static void bumpline(float& x, float& y, float w, Math::fbox& bounds, int32_t scale)
{
    if (x + w > bounds.x + bounds.width)
    {
        y += scale * 1.1f;
        x = bounds.x;
    }
}

fvec2 TexGui::calculateTextBounds(const char* text, float maxWidth, int32_t scale)
{
    auto ts = TextSegment::FromChunkFast(TextChunk(text));
    auto p = Paragraph(&ts, 1);

    return calculateTextBounds(p, scale, maxWidth);
}

fvec2 TexGui::calculateTextBounds(Paragraph text, int32_t scale, float maxWidth)
{
    float x = 0, y = 0;

    fbox bounds = {x, y, maxWidth, 2000};

    for (int32_t i = 0; i < text.count; i++) 
    {
        auto& s = text.data[i];
        switch (s.type)
        {
        case TextSegment::WORD:
            {
                float segwidth = s.width * scale;
                bumpline(x, y, segwidth, bounds, scale);
                x += segwidth;
            }
            break;

        case TextSegment::ICON: 
            {
                fvec2 tsize = fvec2(s.icon->bounds.width, s.icon->bounds.height) * _PX;
                bumpline(x, y, tsize.x, bounds, scale);
                x += tsize.x;
            }
            break;

        case TextSegment::NEWLINE:
            y += scale;
            x = bounds.x;
            break;

        case TextSegment::PLACEHOLDER:
            // Assume small width of placeholder. this is fudgy af but generally gives us better layouts
            {
                bumpline(x, y, 10 * _PX, bounds, scale);
                x += 10 * _PX;
            }
            break;
        case TextSegment::TOOLTIP:
            break;

        case TextSegment::LAZY_ICON:
            break;
        }
    }

    return y > 0
        ? fvec2(maxWidth, y + scale)
        : fvec2(x, scale);
}

#define TTUL Defaults::Tooltip::UnderlineSize

static bool writeText(Paragraph text, int32_t scale, Math::fbox bounds, float& x, float& y, RenderData* rs, bool checkHover = false, int32_t zLayer = 0, uint32_t flags = 0, TextDecl parameters = {});

inline static void drawChunk(TextSegment s, RenderData* rs, Math::fbox bounds, float& x, float& y, int32_t scale, uint32_t flags, int32_t zLayer, bool& hovered, bool checkHover, uint32_t& placeholderIdx, TextDecl parameters)
    {
        static const auto underline = [](auto* rs, float x, float y, float w, int32_t scale, uint32_t flags, int32_t zLayer)
        {
            if (flags & UNDERLINE)
            {
                fbox ul = fbox(fvec2(x - TTUL.x, y + scale / 2.0 - 2), fvec2(w + TTUL.x * 2, TTUL.y));
                rs->drawQuad(ul, fvec4(1,1,1,0.3), zLayer);
            }
        };

        auto& io = inputFrame;

        switch (s.type)
        {
        case TextSegment::WORD:
            {
                float segwidth = s.width * scale;
                bumpline(x, y, segwidth, bounds, scale);

                fbox bounds = fbox(x, y - scale / 2.0, segwidth, scale);

                underline(rs, x, y, segwidth, scale, flags, zLayer);

                rs->drawText(s.word.data, {x, y}, {1,1,1,1}, scale, CENTER_Y, s.word.len, zLayer);

                // if (checkHover) rs->drawQuad(bounds, fvec4(1, 0, 0, 1));
                hovered |= checkHover && !hovered && bounds.contains(io.cursorPos);
                x += segwidth;
            }
            break;

        case TextSegment::ICON: 
        case TextSegment::LAZY_ICON:
            {
                TexEntry* icon = s.type == TextSegment::LAZY_ICON ? s.lazyIcon.func(s.lazyIcon.data) : s.icon;

                fvec2 tsize = fvec2(icon->bounds.width, icon->bounds.height) * _PX;
                bumpline(x, y, tsize.x, bounds, scale);

                fbox bounds = { x, y - tsize.y / 2, tsize.x, tsize.y };

                underline(rs, x, y, tsize.x, scale, flags, zLayer);

                //#TODO: pass in container here
                rs->drawTexture(bounds, icon, 0, _PX, 0, {0,0,8192,8192}, zLayer);

                // if (checkHover) rs->drawQuad(bounds, fvec4(1, 0, 0, 1));
                hovered |= checkHover && !hovered && fbox::margin(bounds, Defaults::Tooltip::HoverPadding).contains(io.cursorPos);
                x += tsize.x;
            }
            break;

        case TextSegment::NEWLINE:
            y += scale;
            x = bounds.x;
            break;

        case TextSegment::TOOLTIP:
            {
                // Render the base text of the tooltip. Do a coarse bounds check
                // so we don't check if each word is hovered if we don't need to
                bool check = bounds.contains(io.cursorPos);
                bool hoverTT = writeText(s.tooltip.base, scale, bounds, x, y, rs, check, zLayer, UNDERLINE);
                if (hoverTT) renderTooltip(s.tooltip.tooltip, rs);
            }
            break;
        case TextSegment::PLACEHOLDER:
            {
                assert(placeholderIdx < parameters.count);
                bool blah = false;
                drawChunk(TextSegment::FromChunkFast(parameters.data[placeholderIdx]), rs, bounds, x, y, scale, flags, zLayer, blah, false, placeholderIdx, {});
                placeholderIdx++;
            }
            break;
        }
        
    }

static bool writeText(Paragraph text, int32_t scale, Math::fbox bounds, float& x, float& y, RenderData* rs, bool checkHover, int32_t zLayer, uint32_t flags, TextDecl parameters)
{
    bool hovered = false;
    // Index into the amount of placeholders we've substituted
    uint32_t placeholderIdx = 0;

    for (int32_t i = 0; i < text.count; i++) 
    {
        drawChunk(text.data[i], rs, bounds, x, y, scale, flags, zLayer, hovered, checkHover, placeholderIdx, parameters);
    }
    return hovered;
}

Container RenderData::drawTooltip(fvec2 size)
{
    auto& io = inputFrame;
    static TexEntry* tex = &m_tex_map[Defaults::Tooltip::Texture];
    
    fbox bounds = {
        io.cursorPos + Defaults::Tooltip::MouseOffset, 
        size,
    };
    bounds = fbox::margin(bounds, Defaults::Tooltip::Padding);

    return this->Base.Box(bounds.x, bounds.y, bounds.w, bounds.h, tex);
}

static void renderTooltip(Paragraph text, RenderData* rs)
{
    int scale = Defaults::Tooltip::TextScale;

    fvec2 textBounds = calculateTextBounds(text, scale, Defaults::Tooltip::MaxWidth);

    Container s = rs->drawTooltip(textBounds);

    float x = s.bounds.x, y = s.bounds.y;
    writeText(text, Defaults::Tooltip::TextScale, s.bounds, x, y, rs, false, 1);
}

void Container::Text(Paragraph text, int32_t scale, TextDecl parameters)
{
    if (scale == 0) scale = Defaults::Font::Size;

    fbox textBounds = {
        { bounds.x, bounds.y },
        calculateTextBounds(text, scale, bounds.width)
    };

    textBounds = Arrange(this, textBounds);
    textBounds.y += + scale / 2.0f;

    writeText(text, scale, bounds, textBounds.x, textBounds.y, rs, false, 0, 0, parameters);
}

void Container::Text(const char* text, int32_t scale, TextDecl parameters)
{
    if (scale == 0) scale = Defaults::Font::Size;
    auto ts = TextSegment::FromChunkFast(TextChunk(text));
    auto p = Paragraph(&ts, 1);

    fbox textBounds = {
        { bounds.x, bounds.y },
        calculateTextBounds(p, scale, bounds.width)
    };

    if (scale == 0) scale = Defaults::Font::Size;
    textBounds = Arrange(this, textBounds);
    textBounds.y += + scale / 2.0f;

    writeText(p, scale, bounds, textBounds.x, textBounds.y, rs, false, 0, 0, parameters);
}

void Container::Row(Container* out, const float* widths, uint32_t n, float height, uint32_t flags)
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
        if (i != 0) x += spacing;

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

        out[i] = withBounds({x, y, width, height});

        x += width;
    }

    fbox arranged = Arrange(this, {bounds.x, bounds.y, x, y + height});

    for (uint32_t i = 0; i < n; i++)
    {
        out[i].bounds.pos = out[i].bounds.pos + arranged.pos;
    }
}

void Container::Column(Container* out, const float* heights, uint32_t n, float width)
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
        
        out[i] = withBounds({bounds.pos + vec2{x, y}, {width, height}});

        y += height + spacing;
    }
}

// Text processing

static int32_t parseText(const char* text, TextSegment* out)
{
    switch (text[0])
    {
    case '\n': 
        *out = { .type = TextSegment::NEWLINE };
        return 1;
    case '\0':
        return -1;
    }

    int32_t i = 0;
    int16_t len = 0;
    int16_t ws = 0;
    // Loop through a single word
    for (;; i++)
    {
        // terminal characters. This will be handled the next time we parse text.
        if (text[i] == '\n' || text[i] == '\0') break;

        else if (text[i] == ' ')
        {
            // Trailing whitespace continues
            ws++;
            continue;
        }

        // End of trailing whitespace. word complete
        if (ws) break;
        
        len++;
    }

    *out = {
        .word = { text, computeTextWidth(text, len), len },
        .width = computeTextWidth(text, i),
        .type = TextSegment::WORD,
    };
    return i;
}

void cacheChunk(const TextChunk& chunk, std::vector<TextSegment>& out)
{
    if (chunk.type == TextChunk::TEXT) 
    {
        const char* t = chunk.text;
        while (true)
        {
            // Break the chunk into words and whitespace
            TextSegment s;
            int32_t len = parseText(t, &s);
            if (len == -1) break;

            out.push_back(s);
            t += len;
        }
    }
    else if (chunk.type == TextChunk::ICON) 
    {
        out.push_back({
            .icon = chunk.icon,
            .width = float(chunk.icon->bounds.width),
            .type = TextSegment::ICON,
        });
    }
    else if (chunk.type == TextChunk::LAZY_ICON)
    {
        out.push_back({
            .lazyIcon = { chunk.lazyIcon.data, chunk.lazyIcon.func },
            .type = TextSegment::LAZY_ICON,
        });
    }
    else if (chunk.type == TextChunk::TOOLTIP)
    {
        out.push_back({
            .tooltip = { chunk.tooltip.base, chunk.tooltip.tooltip },
            .width = 0,
            .type = TextSegment::TOOLTIP,
        });
    }
    else if (chunk.type == TextChunk::INDIRECT)
    {
        cacheChunk(*chunk.indirect, out);
    }
    else if (chunk.type == TextChunk::PLACEHOLDER)
    {
        out.push_back({
            .type = TextSegment::TOOLTIP,
        });
    }
}
std::vector<TextSegment> TexGui::cacheText(TextDecl text)
{
    std::vector<TextSegment> out;
    for (auto& chunk : text)
    {
        cacheChunk(chunk, out);
    }

    return out;
}

TextSegment TextSegment::FromChunkFast(TextChunk chunk)
{
    switch (chunk.type)
    {
        // These should not be used as substitutes for a placeholder
        case TextChunk::TOOLTIP:
        case TextChunk::INDIRECT:
        case TextChunk::PLACEHOLDER:
            chunk.text = "ERROR";

        case TextChunk::TEXT: {
            int16_t len = strlen(chunk.text);
            float w = computeTextWidth(chunk.text, len);
            return {
                .word = { chunk.text, w, len },
                .width = w,
                .type = TextSegment::WORD,
            };
        }

        case TextChunk::ICON: {
            return {
                .icon = chunk.icon,
                .width = float(chunk.icon->bounds.width),
                .type = TextSegment::ICON,
            };
        }
        case TextChunk::LAZY_ICON: {
            return {
                .lazyIcon = { chunk.lazyIcon.data, chunk.lazyIcon.func },
                .width = 0,
                .type = TextSegment::LAZY_ICON,
            };
        }
    }
}


bool cacheChunk(uint32_t& i, const TextChunk& chunk, TextSegment* buffer, uint32_t capacity)
{
    switch (chunk.type)
    {
    case TextChunk::TEXT:
        {
            const char* t = chunk.text;
            while (true)
            {
                // Break the chunk into words and whitespace
                TextSegment s;
                int32_t len = parseText(t, &s);
                if (len == -1) break;

                if (i >= capacity) return false;
                buffer[i] = s;
                i++;
                t += len;
            }
        } break;
    case TextChunk::ICON:
        {
            if (i >= capacity) return false;
            buffer[i] = {
                .icon = chunk.icon,
                .width = float(chunk.icon->bounds.width),
                .type = TextSegment::ICON,
            };
            i++;
        } break;
    case TextChunk::LAZY_ICON:
        {
            if (i >= capacity) return false;
            buffer[i] = {
                .lazyIcon = { chunk.lazyIcon.data, chunk.lazyIcon.func },
                .width = 0,
                .type = TextSegment::LAZY_ICON,
            };
            i++;
        } break;
    case TextChunk::TOOLTIP:
        {
            if (i >= capacity) return false;
            buffer[i] = {
                .tooltip = { chunk.tooltip.base, chunk.tooltip.tooltip },
                .width = 0,
                .type = TextSegment::TOOLTIP,
            };
            i++;
        } break;
    case TextChunk::INDIRECT:
        cacheChunk(i, *chunk.indirect, buffer, capacity);
        break;
    case TextChunk::PLACEHOLDER:
        {
            if (i >= capacity) return false;
            buffer[i] = {
                .type = TextSegment::PLACEHOLDER,
            }; 
            i++;
        } break;
    }
    return true;
}

int32_t TexGui::cacheText(TextDecl text, TextSegment* buffer, uint32_t capacity)
{
    uint32_t i = 0;
    for (auto& chunk : text)
    {
        if (!cacheChunk(i, chunk, buffer, capacity)) return -1; 
    }
    return i;
}


TexGui::Paragraph::Paragraph(std::vector<TextSegment>& s) :
    data(s.data()), count(s.size()) {}

TexGui::Paragraph::Paragraph(TextSegment* ptr, uint32_t n) :
    data(ptr), count(n) {}

TexGui::TextDecl::TextDecl(TextChunk* d, uint32_t n) :
    data(d),
    count(n) {}

TexGui::TextDecl::TextDecl(std::initializer_list<TextChunk> s) :
    data(s.begin()),
    count(s.size()) {}

TexEntry* IconSheet::getIcon(uint32_t x, uint32_t y)
{
    // #TODO: the user's icon sheet has different size, but
    // the shader expects all tex size to be 512.
    ibox bounds = { ivec2(x, y + 1) * ivec2(iw, ih), ivec2(iw, ih) };
    return &m_custom_texs.emplace_back(glID, 0, bounds, 0, 0, 0, 0);
}
