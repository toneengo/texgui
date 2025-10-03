#include "texgui.h"
#include "core/edge-coloring.h"
#include "texgui_flags.hpp"
#include "texgui_style.hpp"
#include "texgui_types.hpp"
#include <cassert>
#include <cstring>
#include <cmath>
#include <mutex>
#include <list>
#include <filesystem>
#include "stb_image.h"
#include "texgui_internal.hpp"
#include <chrono>

#define ALPHA_MASK 0x000000FF
using namespace TexGui;
using namespace std::chrono_literals;
namespace stc = std::chrono;

namespace TexGui
{
    static int deltaMs = 0;
    stc::time_point<stc::steady_clock> currentTime;
}


Style* initDefaultStyle();
void TexGui::init()
{
    GTexGui = new TexGuiContext();
    GTexGui->defaultStyle = initDefaultStyle();
}

void TexGui::newFrame()
{
    GTexGui->rendererFns.newFrame();
}

void TexGui::setTextScale(float scale)
{
    GTexGui->textScale = scale;
}

float TexGui::getUiScale()
{
    return GTexGui->scale;
}

void TexGui::setUiScale(float scale)
{
    GTexGui->scale = scale;
}
float TexGui::getTextScale()
{
    return GTexGui->textScale;
}
RenderData* TexGui::newRenderData()
{
    return new RenderData();
}

TGContainer* TGContainerArray::operator[](size_t index)
{
    assert(index < size);
    return &data[index];
}
bool TexGui::isCapturingMouse()
{
    return GTexGui->lastCapturingMouse;
}

#ifndef M_PI
#define M_PI  3.14159265358979323846264  // from CRC
#endif

// returns 0 on success
// #TODO: remove hover, press etc. and put them in styles instead
// assumes 4 bytes in stride
Texture* TexGui::loadTexture(const char* name, const unsigned char* pixels, int width, int height)
{
    if (GTexGui->textures.contains(name) && GTexGui->textures[name].id > -1)
    {
        if (GTexGui->textures[name].bounds.w != width || GTexGui->textures[name].bounds.h != height)
        {
            printf("Texture variant dimension mismatch: %s\n", name);
            return nullptr;
        }
    }
    else
    {
        Texture& t = GTexGui->textures[name];
        t.bounds.x = 0;
        t.bounds.y = 0;
        t.bounds.w = width;
        t.bounds.h = height;

        t.size.x = width;
        t.size.y = height;

        t.top = float(height)/3.f;
        t.right = float(width)/3.f;
        t.bottom = float(height)/3.f;
        t.left = float(width)/3.f;
    }

    Texture& t = GTexGui->textures[name];
    t.id = GTexGui->rendererFns.createTexture((void*)pixels, width, height);
    //t.name = name;

    return &t;

}
bool TexGui::loadTexture(const char* path)
{
    // im lazy so i just use stdsstring
    std::string pstr = path;

    // name of file, removed of extensions (including variants)
    std::string fstr = path;
    fstr.erase(fstr.begin() + fstr.find('.'), fstr.end());
    fstr.erase(fstr.begin(), fstr.begin() + fstr.find_last_of("/"));

    if (!pstr.ends_with(".png"))
    {
#ifdef DBG
        printf("Unexpected file in sprites folder: %s\n", pstr.c_str());
#endif
        return 1;
    }

    int width, height, channels;
    unsigned char* pixels = stbi_load(pstr.c_str(), &width, &height, &channels, 4);
    if (pixels == nullptr)
    {
        printf("Failed to load file: %s\n", pstr.c_str());
        return 1;
    }

    if (GTexGui->textures.contains(fstr) && GTexGui->textures[fstr].id > -1)
    {
        if (GTexGui->textures[fstr].bounds.w != width || GTexGui->textures[fstr].bounds.h != height)
        {
            printf("Texture variant dimension mismatch: %s\n", pstr.c_str());
            return 1;
        }
    }
    else
    {
        Texture& t = GTexGui->textures[fstr];
        t.bounds.x = 0;
        t.bounds.y = 0;
        t.bounds.w = width;
        t.bounds.h = height;

        t.size.x = width;
        t.size.y = height;

        t.top = float(height)/3.f;
        t.right = float(width)/3.f;
        t.bottom = float(height)/3.f;
        t.left = float(width)/3.f;
    }

    Texture& t = GTexGui->textures[fstr];

    if (pstr.ends_with(".hover.png"))
        t.hover = GTexGui->rendererFns.createTexture(pixels, width, height);
    else if (pstr.ends_with(".press.png"))
        t.press = GTexGui->rendererFns.createTexture(pixels, width, height);
    else if (pstr.ends_with(".active.png"))
        t.active = GTexGui->rendererFns.createTexture(pixels, width, height);
    else
        t.id = GTexGui->rendererFns.createTexture(pixels, width, height);

    //t.name = fstr;
    stbi_image_free(pixels);

    return 0;
}

void TexGui::loadTextures(const char* dir)
{
    std::vector<std::filesystem::directory_entry> files;
    for (const auto& f : std::filesystem::recursive_directory_iterator(dir))
        files.push_back(f);

    for (auto& f : files)
    {
        if (!f.is_regular_file())
            continue;

        std::string pstr = f.path().string();
        std::string fstr = f.path().filename().string();
        fstr.erase(fstr.begin() + fstr.find('.'), fstr.end());

        if (!pstr.ends_with(".png"))
        {
#ifdef DBG
            printf("Unexpected file in sprites folder: %s\n", pstr.c_str());
#endif
            continue;
        }

        int width, height, channels;
        unsigned char* pixels = stbi_load(pstr.c_str(), &width, &height, &channels, 4);
        if (pixels == nullptr)
        {
            printf("Failed to load file: %s\n", pstr.c_str());
            continue;
        }

        if (GTexGui->textures.contains(fstr) && GTexGui->textures[fstr].id > -1)
        {
            if (GTexGui->textures[fstr].bounds.w != width || GTexGui->textures[fstr].bounds.h != height)
            {
                printf("Texture variant dimension mismatch: %s\n", pstr.c_str());
                continue;
            }
        }
        else
        {
            Texture& t = GTexGui->textures[fstr];
            t.bounds.x = 0;
            t.bounds.y = 0;
            t.bounds.w = width;
            t.bounds.h = height;

            t.size.x = width;
            t.size.y = height;

            t.top = float(height)/3.f;
            t.right = float(width)/3.f;
            t.bottom = float(height)/3.f;
            t.left = float(width)/3.f;
        }

        Texture& t = GTexGui->textures[fstr];

        if (pstr.ends_with(".hover.png"))
            t.hover = GTexGui->rendererFns.createTexture(pixels, width, height);
        else if (pstr.ends_with(".press.png"))
            t.press = GTexGui->rendererFns.createTexture(pixels, width, height);
        else if (pstr.ends_with(".active.png"))
            t.active = GTexGui->rendererFns.createTexture(pixels, width, height);
        else
            t.id = GTexGui->rendererFns.createTexture(pixels, width, height);

        //t.name = fstr;

        stbi_image_free(pixels);
    }
}

IconSheet TexGui::loadIcons(const char* dir, int32_t iconWidth, int32_t iconHeight)
{
    //return GTexGui->renderCtx->loadIcons(dir, iconWidth, iconHeight);
    int width, height, channels;
    unsigned char* pixels = stbi_load(dir, &width, &height, &channels, 4);
    assert(pixels != nullptr);

    uint32_t texID = GTexGui->rendererFns.createTexture(pixels, width, height);
    stbi_image_free(pixels);

    return { texID, iconWidth, iconHeight, width, height };
}

// We just need pointer stability, we aren't gonna be iterating it so using list :P
// This is a heap alloc per entry which is a bit of a pain so should change since we barely use heap at all otherwise #TODO
static std::list<Texture> m_custom_texs;

Texture* TexGui::getTexture(const char* name)
{
    if (!GTexGui->textures.contains(name)) return nullptr;
    return &GTexGui->textures[name];
}

Font* TexGui::getFont(const char* name)
{
    if (!GTexGui->fonts.contains(name)) return nullptr;
    return &GTexGui->fonts[name];
}
/*
Texture* TexGui::customTexture(unsigned int texID, Math::ibox pixelBounds)
{
    float xth = pixelBounds.w / 3.f;
    float yth = pixelBounds.h / 3.f;
    auto size = GTexGui->rendererFns.getTextureSize(texID);
    return &m_custom_texs.emplace_back(texID, pixelBounds, size, yth, xth, yth, xth);
}
*/

TexGui::Math::ivec2 TexGui::getTextureSize(Texture* tex)
{
    return tex->bounds.wh;
}

Math::fbox Arrange(TGContainer* c, Math::fbox child)
{
    if (c && c->arrangeProc) return c->arrangeProc(c, child);
    return child;
}

struct InputFrame
{
    int keyStates[TexGuiKey_NamedKey_COUNT];
    int mouseStates[TEXGUI_MOUSE_BUTTON_COUNT];
    int mods;

    int& lmb = mouseStates[0];

    TexGui::Math::fvec2 cursorPos;
    TexGui::Math::fvec2 mouseRelativeMotion;
    TexGui::Math::fvec2 scroll;

    //unsigned int character;
    char text[TEXGUI_TEXT_BUF_SIZE];

    //int& backspace = keyStates[TexGuiKey_Backspace];
};
InputFrame inputFrame;

Math::fvec2 TexGui::getCursorPos()
{
    return inputFrame.cursorPos;
}

// There are two input states, the one in GTexGui and inputFrame.
// updateInput() is called in TexGui::clear().
// GTexGui->io is submittetd to inputFrame, then cleared.
inline static void updateInput()
{
    auto& io = GTexGui->io;

    std::lock_guard<std::mutex> lock(TGInputLock);

    //we don't clear this one
    inputFrame.mods = io.mods;

    inputFrame.cursorPos = io.cursorPos;
    inputFrame.scroll = io.scroll;
    inputFrame.mouseRelativeMotion = io.mouseRelativeMotion;

    io.mouseRelativeMotion = {0, 0};

    GTexGui->lastCapturingMouse = GTexGui->capturingMouse;
    GTexGui->capturingMouse = false;
    io.scroll = {0, 0};

    memcpy(inputFrame.mouseStates, io.mouseStates, sizeof(int) * TEXGUI_MOUSE_BUTTON_COUNT);
    memcpy(inputFrame.keyStates, io.keyStates, sizeof(int) * TexGuiKey_NamedKey_COUNT);
    //if press and release happen too fast (glfw thread polling faster than sim thread), the press event can be lost. but that never should happen so watever
    for (int i = 0; i < TEXGUI_MOUSE_BUTTON_COUNT; i++)
    {
        if (io.mouseStates[i] == KEY_Press) io.mouseStates[i] = KEY_Held;
        if (io.mouseStates[i] == KEY_Release) io.mouseStates[i] = KEY_Off;
    }

    for (int i = 0; i < TexGuiKey_NamedKey_COUNT; i++)
    {
        if (io.keyStates[i] == KEY_Press) io.keyStates[i] = KEY_Held;
        if (io.keyStates[i] == KEY_Repeat) io.keyStates[i] = KEY_Held;
        if (io.keyStates[i] == KEY_Release) io.keyStates[i] = KEY_Off;
    }

    strcpy(inputFrame.text, io.text);

    io.text[0] = '\0';
}

void TexGui::clear()
{
    stc::nanoseconds nanodelta = stc::steady_clock::now() - currentTime;
    deltaMs = stc::duration_cast<stc::milliseconds>(nanodelta).count();
    currentTime = stc::steady_clock::now();

    auto& g = *GTexGui;
    g.containers.clear();
    g.containers.reserve(2048);
    auto& c = g.baseContainer;

    //#TODO: waste to call "getscreensize" here again but who actually gaf
    c.bounds = Math::fbox({0,0}, g.getScreenSize());
    c.scissor = c.bounds;

    if (g.hoveredWidget == 0 && inputFrame.lmb == KEY_Press)
    {
        g.activeWidget = 0;
    }
    g.hoveredWidget = 0;

    updateInput();
    for (auto& win : GTexGui->windows)
    {
        win.second.prevVisible = win.second.visible;
        win.second.visible = false;
    }
}

void TexGui::destroy()
{
    GTexGui->rendererFns.renderClean();
    for (auto& style : GTexGui->styleStack)
    {
        delete style;
    }
    delete GTexGui;
}

inline void setBit(unsigned int& dest, const unsigned int flag, bool on)
{
    dest = on ? dest | flag : dest & ~(flag);
}

inline bool getBit(const unsigned int dest, const unsigned int flag)
{
    return dest & flag;
}

using namespace TexGui::Math;

ContainerState getState(TexGuiID id, TGContainer* c, const fbox& bounds, const fbox& scissor)
{
#define TG_STATE_HOVERED(rect) rect.contains(inputFrame.cursorPos) && c->window && c->window->state & STATE_HOVER && scissor.contains(inputFrame.cursorPos)
#define TG_STATE_CLICKED(rect) rect.contains(inputFrame.cursorPos) && inputFrame.lmb == KEY_Press && c->window && c->window->state & STATE_HOVER && scissor.contains(inputFrame.cursorPos)
    ContainerState state = 0;
    if (TG_STATE_HOVERED(bounds))
    {
        GTexGui->hoveredWidget = id;
        state |= STATE_HOVER;
    }
    if (TG_STATE_CLICKED(bounds))
    {
        GTexGui->activeWidget = id;
        state |= STATE_PRESS;
    }
    if (GTexGui->activeWidget == id) state |= STATE_ACTIVE;
    if (GTexGui->activeWidget == id && inputFrame.lmb == KEY_Held) state |= STATE_PRESS;
    return state;
#undef TG_STATE_HOVERED
#undef TG_STATE_CLICKED
}

uint32_t getBoxState(uint32_t& state, fbox box, uint32_t parentState)
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

        // if parent state is not active, it cant be active
        // if parent state is not hover, it cant be hover
        if (!(parentState & STATE_ACTIVE))
            setBit(state, STATE_ACTIVE, 0);
        if (!(parentState & STATE_HOVER))
            setBit(state, STATE_HOVER, 0);
        if (!(parentState & STATE_PRESS))
            setBit(state, STATE_PRESS, 0);

        return state;
    }

    setBit(state, STATE_HOVER, 0);
    setBit(state, STATE_PRESS, 0);

    if (io.lmb == KEY_Press || !(parentState & STATE_ACTIVE))
        setBit(state, STATE_ACTIVE, 0);

    return state;
}

fbox AlignBox(fbox bounds, fbox child, uint32_t f)
{
    if (f & ALIGN_LEFT) child.x = bounds.x;
    if (f & ALIGN_RIGHT) child.x = bounds.x + bounds.width - child.width;

    if (f & ALIGN_TOP) child.y = bounds.y;
    if (f & ALIGN_BOTTOM) child.y = bounds.y + bounds.height - child.height;

    if (f & ALIGN_CENTER_X) child.x = floor(bounds.x + (bounds.width - child.width) / 2);
    if (f & ALIGN_CENTER_Y) child.y = floor(bounds.y + (bounds.height - child.height) / 2);
    return child;
}

#define _PX GTexGui->pixelSize

void TexGui::setRenderData(RenderData* renderData)
{
    GTexGui->renderData = renderData;
    GTexGui->baseContainer.renderData = renderData;
}

bool animate(const Animation& animation, Animation& out, fbox& box, uint32_t& alpha, bool reset)
{
    if (!animation.enabled) return false;
    bool active = false;
    if (reset)
    {
        out.offset = animation.offset;
        out.time = -deltaMs;
    }

    if (out.time < animation.duration) {
        active = true;

        out.time += deltaMs;
        double duration = out.duration / 1000.0;
        double t = out.time / double(animation.duration);
        double* b = out.bezier;
        // this is a faked bezier curve cuz i dont know how to do it yet
        double val = pow((1 - t), 3) * b[0] +
                     3 * pow((1 - t), 2) * t * b[1] +
                     3 * (1 - t) * pow(t, 2) * b[2] +
                     pow(t, 3) * b[3];

        out.offset = animation.offset * -1 * (1 - float(val));
        box.pos += out.offset;

        out.alphaModifier = (alpha - animation.alphaModifier) * (1 - float(val));
        alpha -= out.alphaModifier;
    }

    return active;
}

TGContainer* TexGui::Window(const char* id, TGStr name, float xpos, float ypos, float width, float height, uint32_t flags, WindowStyle* style)
{
    auto& io = inputFrame;
    auto& g = *GTexGui;
    TexGuiID hash = ImHashStr(id, strlen(id), -1);
    if (!GTexGui->windows.contains(hash))
    {
        for (auto& entry : GTexGui->windows)
        {
            if (entry.second.order != INT_MAX) entry.second.order++;
        }

        GTexGui->windows.insert({hash, TexGuiWindow{
            .id = int(GTexGui->windows.size()),
            .box = {xpos, ypos, width, height},
        }});
    }

    if (style == nullptr)
        style = &GTexGui->styleStack.back()->Window;

    Texture* wintex = style->Texture;
    assert(wintex);
    TexGuiWindow& wstate = GTexGui->windows[hash];
    wstate.visible = true;

    fbox box = {xpos, ypos, width, height};
    box = AlignBox(Math::fbox(0, 0, GTexGui->getScreenSize().x, GTexGui->getScreenSize().y), box, flags);
    if (flags & (ALIGN_LEFT | ALIGN_CENTER_X | ALIGN_RIGHT))
        box.x += xpos;
    if (flags & (ALIGN_BOTTOM | ALIGN_CENTER_Y | ALIGN_TOP))
        box.y += ypos;

    uint32_t alpha = 0xFF;
    bool animationActive = animate(style->InAnimation, GTexGui->animations[hash], box, alpha, !wstate.prevVisible);

    if (flags & LOCKED || !wstate.prevVisible || animationActive)
        wstate.box = box;

    if (flags & BACK)
        wstate.order = INT_MAX;
    if (flags & FRONT)
        wstate.order = -1;

    if (flags & CAPTURE_INPUT && wstate.box.contains(io.cursorPos)) GTexGui->capturingMouse = true;

    //if there is a window over the window being clicked, set state to 0
    getBoxState(wstate.state, wstate.box, STATE_ALL);
    if (wstate.order > 0 && wstate.state != STATE_NONE && wstate.box.contains(io.cursorPos))
    {
        for (auto& entry : GTexGui->windows)
        {
            if (!entry.second.prevVisible || entry.second.order >= wstate.order) continue;
            if (entry.second.box.contains(io.cursorPos)) {
                wstate.state = STATE_NONE;
                break;
            }
        }
    }

    //bring focused window to the front (0), if it doesnt have a FRONT or BACK flag
    if (!(flags & FORCED_ORDER) && wstate.state & STATE_ACTIVE && wstate.order > 0)
    {
        int order = wstate.order;
        for (auto& entry : GTexGui->windows)
        {
            if (entry.second.order < order && entry.second.order > -1) entry.second.order++;
        }

        wstate.order = 0;
    }

    if (flags & LOCKED || !(io.lmb & (KEY_Held | KEY_Press)) || !(wstate.state & STATE_ACTIVE))
    {
        wstate.moving = false;
        wstate.resizing = false;
    }
    else if (fbox(wstate.box.x, wstate.box.y, wstate.box.width, wintex->top * _PX).contains(io.cursorPos) && io.lmb == KEY_Press)
        wstate.moving = true;
    else if (flags & RESIZABLE && fbox(wstate.box.x + wstate.box.width - wintex->right * _PX,
             wstate.box.y + wstate.box.height - wintex->bottom * _PX,
             wintex->right * _PX, wintex->bottom * _PX).contains(io.cursorPos) && io.lmb == KEY_Press)
        wstate.resizing = true;

    if (wstate.moving)
        wstate.box.pos += io.mouseRelativeMotion;
    else if (wstate.resizing)
        wstate.box.size += io.mouseRelativeMotion;

    fvec4 padding = style->Padding;

    fbox internal = fbox::pad(wstate.box, padding);

    g.renderData->ordered = true;

    TGContainer* child = &g.containers.emplace_back();

    child->bounds = internal;
    child->renderData = &g.renderData->children.emplace_back();
    child->window = &wstate;
    child->renderData->priority = -wstate.order;
    child->renderData->alphaModifier = alpha;
    child->renderData->addTexture(wstate.box, wintex, wstate.state, _PX, SLICE_9);
    child->scissor = {{0, 0}, g.getScreenSize()};

    if (!(flags & HIDE_TITLE))
        child->renderData->addText(name, {wstate.box.x + padding.left, wstate.box.y},
                 style->Text.Color, style->Text.Size * g.textScale, internal.width, wintex->top * _PX, nullptr, CENTER_Y);

    return child;
}

bool TexGui::Button(TGContainer* c, const char* id, TGStr text, TexGui::ButtonStyle* style)
{
    auto& g = *GTexGui;
    auto& io = inputFrame;
    TexGuiID bid = c->window->getID(id);

    fbox internal = Arrange(c, c->bounds);

    if (style == nullptr)
        style = &GTexGui->styleStack.back()->Button;

    uint32_t state = getState(bid, c, internal, c->scissor);

    Texture* tex = style->Texture;
    c->renderData->addTexture(internal, tex, state, _PX, SLICE_9);

    TexGui::Math::vec2 pos = state & STATE_PRESS
                       ? c->bounds.pos + style->POffset
                       : c->bounds.pos;

    internal = {pos, internal.size};

    c->renderData->addText(text, pos, style->Text.Color, style->Text.Size * g.textScale, internal.width, internal.height, nullptr, CENTER_X | CENTER_Y);

    bool hovered = c->scissor.contains(io.cursorPos)
                && c->bounds.contains(io.cursorPos);

    return state & STATE_ACTIVE && io.lmb == KEY_Release && hovered ? true : false;
}

TGContainer* TexGui::Box(TGContainer* c, float xpos, float ypos, float width, float height, uint32_t flags, TexGui::BoxStyle* style)
{
    if (width <= 1)
        width = width == 0 ? c->bounds.width : c->bounds.width * width;
    if (height <= 1)
        height = height == 0 ? c->bounds.height : c->bounds.height * height;

    if (style == nullptr)
        style = &GTexGui->styleStack.back()->Box;
    if (!c) c = &GTexGui->baseContainer;
    Texture* texture = style->Texture;

    fbox box(c->bounds.x + xpos, c->bounds.y + ypos, width, height);
    box = AlignBox(Math::fbox(0, 0, GTexGui->getScreenSize().x, GTexGui->getScreenSize().y), box, flags);
    if (flags & (ALIGN_LEFT | ALIGN_CENTER_X | ALIGN_RIGHT))
        box.x += xpos;
    if (flags & (ALIGN_BOTTOM | ALIGN_CENTER_Y | ALIGN_TOP))
        box.y += ypos;

    fbox internal = fbox::pad(box, style->Padding);
    TGContainer* child = &GTexGui->containers.emplace_back();

    child->size = box;
    child->bounds = internal;
    child->renderData = &c->renderData->children.emplace_back();
    child->window = c->window;
    //child->renderData->colorMultiplier = color;
    child->scissor = box;

    if (texture)
    {
        child->renderData->addTexture(box, texture, 0, 2, flags);
    }

    return child;

}

bool TexGui::CheckBox(TGContainer* c, bool* val, TexGui::CheckBoxStyle* style)
{
    if (style == nullptr)
        style = &GTexGui->styleStack.back()->CheckBox;
    Texture* texture = style->Texture;

    auto& io = inputFrame;
    bool pressed = false;
    if (io.lmb == KEY_Release && c->bounds.contains(io.cursorPos) && c->window->state & STATE_HOVER) 
    {
        *val = !*val;
        pressed = true;
    }

    if (texture == nullptr) return pressed;

    c->renderData->addTexture(c->bounds, texture, *val ? STATE_ACTIVE : 0, 2, SLICE_9);

    return pressed;
}

void TexGui::RadioButton(TGContainer* c, uint32_t* selected, uint32_t id, RadioButtonStyle* style)
{
    if (style == nullptr)
        style = &GTexGui->styleStack.back()->RadioButton;
    Texture* texture = style->Texture;

    auto& io = inputFrame;
    if (io.lmb == KEY_Release && c->bounds.contains(io.cursorPos) && c->window->state & STATE_HOVER) *selected = id;

    if (texture == nullptr) return;
    c->renderData->addTexture(c->bounds, texture, *selected == id ? STATE_ACTIVE : 0, 2, SLICE_9);
}

void TexGui::Line(TGContainer* c, float x1, float y1, float x2, float y2, uint32_t color, float lineWidth)
{
    if (!c) c = &GTexGui->baseContainer;
    c->renderData->addLine(c->bounds.x + x1, c->bounds.y + y1, c->bounds.x + x2, c->bounds.y + y2, color, lineWidth);
}

TGContainer* createChild(TGContainer* c, Math::fbox bounds, ArrangeFunc arrange = nullptr)
{
    TGContainer* child = &GTexGui->containers.emplace_back();
    child->bounds = bounds;
    child->scissor = !c ? Math::fbox({0,0}, GTexGui->getScreenSize()) : c->scissor;
    child->parent = c;
    child->arrangeProc = arrange;
    child->window = !c ? nullptr : c->window;
    child->renderData = !c ? GTexGui->renderData : c->renderData;
    return child;
}

TGContainer* TexGui::BeginScrollPanel(TGContainer* c, const char* name, ScrollPanelStyle* style)
{
    if (style == nullptr)
        style = &GTexGui->styleStack.back()->ScrollPanel;
    Texture* texture = style->PanelTexture;
    Texture* bartex = style->BarTexture;
    auto& io = inputFrame;
    TexGuiID id = c->window->getID(name);
    if (!GTexGui->scrollPanels.contains(id))
    {
        ScrollPanelState _s =
        {
            .contentPos = {0,0},
            .bounds = c->bounds
        };
        GTexGui->scrollPanels.insert({id, _s});
    }

    auto& spstate = GTexGui->scrollPanels[id];

    fvec4 padding = style->Padding;

    if (!bartex)
    {
        static auto defaultbt = bartex;
        bartex = defaultbt;
    }

    padding.right += bartex->bounds.width;

    static auto arrange = [](TGContainer* scroll, fbox child)
    {
        auto& bounds = ((ScrollPanelState*)(scroll->scrollPanelState))->bounds;
        bounds = child;
        return bounds;
    };

    TGContainer* sp = createChild(c, Math::fbox::pad(c->bounds, padding), arrange);
    sp->scrollPanelState = &spstate;

    if (spstate.scrolling && io.cursorPos.y >= sp->bounds.y && io.cursorPos.y < sp->bounds.y + sp->bounds.height)
        spstate.contentPos.y -= inputFrame.mouseRelativeMotion.y * spstate.bounds.height / sp->bounds.height;

    if (sp->bounds.contains(io.cursorPos))
        spstate.contentPos.y += inputFrame.scroll.y;

    float height = sp->bounds.height - padding.top - padding.bottom;
    if (spstate.bounds.height + spstate.contentPos.y < height)
        spstate.contentPos.y = -(spstate.bounds.height - height);
    spstate.contentPos.y = fmin(spstate.contentPos.y, 0);

    float barh = fmin(c->bounds.height, c->bounds.height * height / spstate.bounds.height);
    fbox bar = {c->bounds.x + c->bounds.width - padding.right,
                c->bounds.y + (c->bounds.height - barh) * -spstate.contentPos.y / (spstate.bounds.height - height),
                padding.right,
                barh};

    c->renderData->addTexture(c->bounds, texture, 0, _PX, 0);
    c->renderData->addTexture(bar, bartex, 0, 2, SLICE_9);

    sp->scissor = sp->bounds;
    c->renderData->pushScissor(sp->bounds);

    if (bar.contains(io.cursorPos) && io.lmb == KEY_Press)
        spstate.scrolling = true;
    else if (io.lmb != KEY_Held)
        spstate.scrolling = false;

    sp->bounds.y += spstate.contentPos.y;

    return sp;
}

void TexGui::EndScrollPanel(TGContainer* c)
{
    c->parent->renderData->popScissor();
}

void renderSlider(RenderData* renderData, TexGuiID id, const fbox& bounds, float percent, Texture* bar, Texture* node)
{
    //just uses the height of the bar texture

}

int TexGui::SliderInt(TGContainer* c, int* val, int minVal, int maxVal, SliderStyle* style)
{
    if (style == nullptr)
        style = &GTexGui->styleStack.back()->Slider;
    auto& g = *GTexGui;
    auto& io = inputFrame;
    auto bar = style->BarTexture;
    auto node = style->NodeTexture;

    TexGuiID id = c->window->getID(&val);

    float percent = float(*val - minVal) / float(maxVal - minVal);
    //renderSlider(renderData, id, bounds, percent, bar, node);

    assert(bar);
    fbox barArea = {c->bounds.x, c->bounds.y, c->bounds.width, bar->bounds.height * _PX};
    barArea = Arrange(c, barArea);

    uint32_t state = getState(id, c, barArea, c->scissor);
    c->renderData->addTexture(barArea, bar, state, _PX, SLICE_3_HORIZONTAL);

    if (g.activeWidget == id && io.lmb == KEY_Held)
    {
        float p = clamp(float(io.cursorPos.x - barArea.x) / barArea.width, 0.f, 1.f);
        *val = minVal + p * (maxVal - minVal);
    }

    if (node)
    {
        Math::fvec2 nodePos = {c->bounds.x - node->bounds.width * _PX * 0.5f + barArea.width * percent, barArea.y + barArea.height / 2.f - node->bounds.height * _PX / 2.f};
        fbox nodeArea = {c->bounds.x - node->bounds.width * _PX * 0.5f + barArea.width * percent, barArea.y + barArea.height / 2.f - node->bounds.height * _PX / 2.f,
                         node->bounds.width * _PX, node->bounds.height * _PX};
        uint32_t nodeState;
        getBoxState(nodeState, nodeArea, state);
        c->renderData->addTexture(nodeArea, node, nodeState, _PX, 0);
    }

    return *val;
}

void TexGui::Image(TGContainer* c, Texture* texture, int scale)
{
    if (!texture) return;
    Style& style = *GTexGui->styleStack.back();
    if (scale == -1)
        scale = _PX;

    Math::fvec2 tsize = Math::fvec2(texture->bounds.width, texture->bounds.height) * scale;

    fbox sized = fbox(c->bounds.pos, tsize);
    fbox arranged = Arrange(c, sized);

    c->renderData->addTexture(arranged, texture, STATE_NONE, scale, 0);
}

void TexGui::Image(TGContainer* c, Texture* texture, uint32_t colorOverride, int scale)
{
    if (!texture) return;
    Style& style = *GTexGui->styleStack.back();
    if (scale == -1)
        scale = _PX;

    Math::fvec2 tsize = Math::fvec2(texture->bounds.width, texture->bounds.height) * scale;

    fbox sized = fbox(c->bounds.pos, tsize);
    fbox arranged = Arrange(c, sized);

    c->renderData->addTexture(arranged, texture, STATE_NONE, scale, 0, colorOverride);
}
/*
bool Container::DropdownInt(int* val, std::initializer_list<std::pair<const char*, int>> names)
{

}
*/

TGContainer* TexGui::BeginTooltip(Math::fvec2 size, TooltipStyle* style)
{
    static auto arrange = [](TGContainer* tooltip, fbox child)
    {
        Style& style = *GTexGui->styleStack.back();
        // A bit scuffed since we add margins then unpad them but mehh
        fbox bounds = fbox::margin(child, style.Tooltip.Padding);
        // Get the parent to arrange the list item
        if (bounds.width > tooltip->box.width) tooltip->box.width = bounds.width;
        if (bounds.height > tooltip->box.height) tooltip->box.height = bounds.height;

        uint32_t state = STATE_NONE;

        auto& io = inputFrame;

        bounds.pos = io.cursorPos + style.Tooltip.MouseOffset;

        // The child is positioned by the tooptip parent
        return fbox::pad(bounds, style.Tooltip.Padding);
    };

    static auto render = [](TGContainer* a)
    {
        Style& style = *GTexGui->styleStack.back();
        auto& io = inputFrame;
        a->parentRenderData->addTexture(fbox(io.cursorPos + style.Tooltip.MouseOffset, fvec2(a->box.width, a->box.height)), a->texture, 0, _PX, SLICE_9);
    };

    if (style == nullptr)
        style = &GTexGui->styleStack.back()->Tooltip;
    auto& io = inputFrame;

    fbox rect = {
        io.cursorPos + style->MouseOffset,
        size,
    };

    auto& g = *GTexGui;
    fbox internal = fbox::pad(rect, style->Padding);
    TGContainer* child = &g.containers.emplace_back();
    child->box.width = size.x;
    child->box.height = size.y;
    child->bounds = internal;
    child->window = nullptr;
    child->scissor = {{0, 0}, g.getScreenSize()};

    child->texture = style->Texture;
    child->renderProc = render;
    child->arrangeProc = arrange;

    //this is scuffed
    child->parentRenderData = &g.renderData->children.emplace_back();
    child->parentRenderData->priority = INT_MAX;
    child->renderData = &child->parentRenderData->children.emplace_back();
    child->renderData->priority;
    //child->renderData->colorMultiplier = renderData->colorMultiplier;

    return child;
}

void TexGui::EndTooltip(TGContainer* c)
{
    c->renderProc(c);
}

// Arranges the list item based on the thing that is put inside it.
TGContainer* TexGui::ListItem(TGContainer* c, uint32_t* selected, uint32_t id, ListItemStyle* style)
{
    static auto arrange = [](TGContainer* listItem, fbox child)
    {
        Style& style = *GTexGui->styleStack.back();
        // A bit scuffed since we add margins then unpad them but mehh
        fbox bounds = fbox::margin(child, style.ListItem.Padding);
        // Get the parent to arrange the list item
        bounds = Arrange(listItem->parent, bounds);

        if (!listItem->scissor.contains(bounds))
        {
            return fbox(0,0,0,0);
        }

        uint32_t state = STATE_NONE;
        if (listItem->listItem.selected != nullptr)
        {
            auto& io = inputFrame;

            // can always be active
            getBoxState(state, bounds, listItem->window->state);
            if (io.lmb == KEY_Press && state & STATE_HOVER)
                *(listItem->listItem.selected) = listItem->listItem.id;

            if (*(listItem->listItem.selected) == listItem->listItem.id)
                state = STATE_ACTIVE;
        }
        else
        {
            state = listItem->listItem.id ? STATE_ACTIVE : STATE_NONE;
        }

        listItem->renderData->addTexture(bounds, listItem->texture, state, _PX, SLICE_9);

        // The child is positioned by the list item's parent
        return fbox::pad(bounds, style.ListItem.Padding);
    };
    if (style == nullptr)
        style = &GTexGui->styleStack.back()->ListItem;
    // Add padding to the contents of the list item
    Texture* tex = style->Texture;
    TGContainer* listItem = createChild(c, fbox::pad(c->bounds, style->Padding), arrange);
    listItem->texture = tex;
    listItem->listItem = { selected, id };
    return listItem;
}

TGContainer* TexGui::Align(TGContainer* c, uint32_t flags, const Math::fvec4 padding)
{
    static auto arrange = [](TGContainer* align, fbox child)
    {
        Math::fvec4 pad = {align->align.top, align->align.right, align->align.bottom, align->align.left};
        fbox bounds = fbox::margin(child, pad);
        bounds = Arrange(align->parent, bounds);
        return fbox::pad(AlignBox(align->bounds, bounds, align->align.flags), pad);
    };
    // Add padding to the contents
    if (!c) c = &GTexGui->baseContainer;
    TGContainer* aligner = createChild(c, fbox::pad(c->bounds, padding), arrange);
    aligner->align = {
        flags,
        padding.x, padding.y, padding.z, padding.w,
    };
    return aligner;
}

// Arranges the cells of a grid by adding a new child box to it.
TGContainer* TexGui::Grid(TGContainer* c, TexGui::GridStyle* style)
{
    static auto arrange = [](TGContainer* grid, fbox child)
    {
        //#TODO: grid styling
        Style& style = *GTexGui->styleStack.back();
        auto& gs = grid->grid;

        gs.rowHeight = fmax(gs.rowHeight, child.height);

        float spacing = gs.spacing == -1 ? style.Row.Spacing : gs.spacing;
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
    if (style == nullptr)
        style = &GTexGui->styleStack.back()->Grid;
    TGContainer* grid = createChild(c, c->bounds, arrange);
    grid->grid = { 0, 0, 0, 0, style->Spacing };
    return grid;
}

void TexGui::Divider(TGContainer* c, float padding)
{
    float width = 2;
    fbox ln = Arrange(c, {c->bounds.x,c->bounds.y, c->bounds.width, width + padding*2.f});
    fbox line = {
        ln.x, ln.y + padding, ln.width, width,
    };
    c->renderData->addQuad(line, 0xFFFFFF80);
}

TGContainer* TexGui::Stack(TGContainer* c, float padding, StackStyle* style)
{
    static auto arrange = [](TGContainer* stack, fbox child)
    {
        auto& s = stack->stack;

        child = fbox(stack->bounds.pos + fvec2(0, s.y), child.size);
        s.y += child.size.y + s.padding;

        //TexGui::Arrange(child);
        Arrange(stack->parent, {stack->bounds.x, stack->bounds.y, child.width, s.y});

        return child;
    };

    if (style == nullptr)
        style = &GTexGui->styleStack.back()->Stack;
    TGContainer* stack = createChild(c, c->bounds, arrange);
    stack->stack = {0, padding < 0 ? style->Padding : padding, 0};
    return stack;
}

void TexGui::ProgressBar(TGContainer* c, float percentage, const ProgressBarStyle* style)
{
    if (style == nullptr)
        style = &GTexGui->styleStack.back()->ProgressBar;

    auto& g = *GTexGui;
    auto& io = inputFrame;

    //Fixed height
    Texture* bartex = style->BarTexture;
    Texture* frametex = style->FrameTexture;
    assert(bartex && frametex);
    fbox frame = {c->bounds.x, c->bounds.y, c->bounds.width, frametex ? frametex->bounds.height * _PX : bartex->bounds.height * _PX};
    frame = Arrange(c, frame);

    fbox internal = fbox::pad(frame, style->Padding);
    internal.width = percentage * internal.width;
    if (style->Flipped)
    {
        internal.x += c->bounds.width - internal.width;
    }

    c->renderData->addTexture(frame, style->BackTexture, 0, _PX, SLICE_3_HORIZONTAL);
    c->renderData->pushScissor(internal);
    c->renderData->addTexture(frame, bartex, 0, _PX, SLICE_3_HORIZONTAL);
    c->renderData->popScissor();
    c->renderData->addTexture(frame, frametex, 0, _PX, SLICE_3_HORIZONTAL);
}

void TexGui::ProgressBarV(TGContainer* c, float percentage, const ProgressBarStyle* style)
{
    if (style == nullptr)
        style = &GTexGui->styleStack.back()->ProgressBar;

    auto& g = *GTexGui;
    auto& io = inputFrame;

    //Fixed height
    Texture* bartex = style->BarTexture;
    Texture* frametex = style->FrameTexture;
    assert(bartex);
    fbox frame = {c->bounds.x, c->bounds.y, frametex ? frametex->bounds.width * _PX : bartex->bounds.width * _PX, c->bounds.height};
    frame = Arrange(c, frame);

    fbox internal = fbox::pad(frame, style->Padding);
    internal.height = percentage * internal.height;
    if (style->Flipped)
    {
        internal.y += c->bounds.height - internal.height;
    }

    c->renderData->addTexture(frame, style->BackTexture, 0, _PX, SLICE_3_VERTICAL);
    c->renderData->pushScissor(internal);
    c->renderData->addTexture(frame, bartex, 0, _PX, SLICE_3_VERTICAL);
    c->renderData->popScissor();
    c->renderData->addTexture(frame, frametex, 0, _PX, SLICE_3_VERTICAL);
}


TGContainer* TexGui::Node(TGContainer* c, float x, float y)
{
    static auto arrange = [](TGContainer* align, fbox child)
    {
        child.x -= child.width / 2.f;
        child.y -= child.height / 2.f;
        fbox bounds = Arrange(align->parent, child);
        return bounds;
    };

    if (!c) c = &GTexGui->baseContainer;
    //this is weird maybe there is btter way. bounds is different now
    //the bounds box itself is moved and not resized,
    //the child will be slightly out of bounds sincei t is centred on top left corner of bounds
    TGContainer* node = createChild(c, c->bounds, arrange);
    node->bounds.x += x;
    node->bounds.y += y;
    return node;
}

//true = dont write text, false = write text
static bool textInputUpdate(TextInputState& tstate, char** c, CharacterFilter filter)
{
    auto& io = inputFrame;
    *c = io.text;
    if (!(tstate.state & STATE_ACTIVE)) return false;

    /*
    //#TODO: character filter. below code is using old text impl
    if (io.character != 0 && (filter == nullptr || filter(io.character)))
    {
        *c = io.character;
        return false;
    }
    */

    if (io.keyStates[TexGuiKey_Backspace] == KEY_Press || io.keyStates[TexGuiKey_Backspace] == KEY_Repeat)
    {
        return true;
    }
    return false;
}

void renderTextInput(RenderData* renderData, const char* name, fbox bounds, fbox scissor, TextInputState& tstate, const char* text, int32_t len, TextInputStyle* style)
{
    if (style == nullptr)
        style = &GTexGui->styleStack.back()->TextInput;
    Texture* inputtex = style->Texture;

    renderData->addTexture(bounds, inputtex, tstate.state, _PX, SLICE_9);
    float offsetx = 0;
    fvec4 padding = style->Padding;
    fvec4 color = style->Text.Color;

    float startx = bounds.x + offsetx + padding.left;
    float starty = bounds.y + bounds.height / 2;

    //#TODO: size
    int size = style->Text.Size;
    size *= GTexGui->textScale;

    const char* renderText = !getBit(tstate.state, STATE_ACTIVE) && len == 0
                             ? name : text;

    renderData->addText(
        {(const uint8_t*)renderText, strlen(renderText)},
        {startx, starty},
        style->Text.Color,
        size,
        bounds.width,
        bounds.height,
        nullptr,
        CENTER_Y,
        &tstate
    );
}

void TextInputBehaviour(TextInputState& ti, char* buf, uint32_t bufsize, int tlen)
{
    auto& g = *GTexGui;
    auto& io = inputFrame;
    //left takes priority
    bool left = io.keyStates[TexGuiKey_LeftArrow] & (KEY_Press | KEY_Repeat) || io.keyStates[TexGuiKey_Backspace] & (KEY_Press | KEY_Repeat);
    bool right = io.keyStates[TexGuiKey_RightArrow] & (KEY_Press | KEY_Repeat);

    int dir = left ? -1 : right ? 1 : 0;

    if (dir != 0 && ti.selection[0] == ti.selection[1])
    {
        if (io.keyStates[TexGuiKey_Backspace] & (KEY_Press | KEY_Repeat))
        {
            ti.selection[1] = ti.textCursorPos;
        }
        ti.textCursorPos = clamp(ti.textCursorPos + dir, 0, tlen);

        if (io.mods &
        #ifdef __APPLE__
            TexGuiMod_Alt
        #else
            TexGuiMod_Ctrl
        #endif
            )
        {
            while (ti.textCursorPos > 0 && ti.textCursorPos < tlen)
            {
                const char& curr = buf[ti.textCursorPos];
                const char& next = buf[ti.textCursorPos + dir];
                //#TODO: more delimiters
                if (curr != ' ' && curr != '.' && (next == ' ' || next == '.'))
                {
                    if (dir > 0) ti.textCursorPos++;
                    break;
                }

                ti.textCursorPos += dir;
            }
        }
        if (io.keyStates[TexGuiKey_Backspace] & (KEY_Press | KEY_Repeat))
        {
            ti.selection[0] = ti.textCursorPos;
        }
    }

    if (io.mods &
    #ifdef __APPLE__
        TexGuiMod_Super
    #else
        TexGuiMod_Ctrl
    #endif
        && io.keyStates[TexGuiKey_A]
        )
    {
        ti.selection[0] = 0;
        ti.selection[1] = tlen;
    }

    if (io.keyStates[TexGuiKey_Backspace] & (KEY_Press | KEY_Repeat) || io.text[0] != '\0')
    {
        if (ti.selection[0] != ti.selection[1])
        {
            int size = ti.selection[1] - ti.selection[0];
            for (int i = ti.selection[0]; i <= tlen; i++)
            {
                buf[i] = buf[i + size];
            }
            tlen -= size;
            ti.textCursorPos = ti.selection[0];
        }

        uint32_t newlen = strlen(io.text);
        if (tlen + newlen < bufsize - 1 && newlen > 0)
        {
            if (ti.textCursorPos != tlen)
                strcat(io.text, buf + ti.textCursorPos);

            strcpy(buf + ti.textCursorPos, io.text);
            ti.textCursorPos += newlen;
            tlen += newlen;
        }
    }

    //deselect
    if (dir != 0 || io.text[0] != '\0')
    {
        ti.selection[0] = -1;
        ti.selection[1] = -1;
    }
}
void TexGui::TextInput(TGContainer* c, const char* name, char* buf, uint32_t bufsize, TextInputStyle* style)
{
    auto& g = *GTexGui;
    auto& io = inputFrame;

    uint32_t id = c->window->getID(&buf);

    auto& ti = g.textInputs[id];

    fbox internal = Arrange(c, c->bounds);

    ti.state = getState(id, c, internal, c->scissor);

    uint32_t len = strlen(buf);

    if (g.activeWidget == id)
    {
        g.editingText = true;
        TextInputBehaviour(ti, buf, bufsize, len);
    }

    renderTextInput(c->renderData, name, c->bounds, c->scissor, ti, buf, len, style);
}

// Bump the line of text
static void bumpline(float& x, float& y, float w, TexGui::Math::fbox& bounds, int32_t scale)
{
    if (x + w > bounds.x + bounds.width)
    {
        y += scale * 1.1f;
        x = bounds.x;
    }
}

#define TTUL style.Tooltip.UnderlineSize

void TexGui::Text(TGContainer* container, const char* text, int32_t scale, uint32_t flags, TexGui::TextStyle* style)
{
    Text(container, TGStr{(uint8_t*)text, strlen(text)}, scale, flags, style);
}

void TexGui::Text(TGContainer* c, TGStr text, int32_t scale, uint32_t flags, TextStyle* style)
{
    if (style == nullptr)
        style = &GTexGui->styleStack.back()->Text;

    if (scale == 0) scale = style->Size;
    scale *= GTexGui->textScale;

    Math::fbox textBounds;
    c->renderData->addText(text, c->bounds.pos, style->Color, scale, c->bounds.width, c->bounds.height, &textBounds, flags);
}

/*
Container::ContainerArray Container::Row(std::initializer_list<float> widths, float height, uint32_t flags)
{
    return {};
}
*/

TGContainerArray TexGui::Row(TGContainer* c, uint32_t widthCount, const float* pWidths, float height, TexGui::RowStyle* style)
{
    if (style == nullptr)
        style = &GTexGui->styleStack.back()->Row;
    int n = widthCount;
    if (height < 1) {
        height = height == 0 ? c->bounds.height : c->bounds.height * height;
    }
    float absoluteWidth = 0;
    float inherit = 0;
    for (int i = 0; i < widthCount; i++)
    {
        if (pWidths[i] >= 1)
            absoluteWidth += pWidths[i];
        else
            inherit++;
    }

    float x = 0, y = 0;
    float spacing = style->Spacing;
    TGContainerArray out;
    out.size = n;
    for (uint32_t i = 0; i < n; i++)
    {
        if (i != 0) x += spacing;

        float width;
        if (pWidths[i] <= 1)
        {
            float mult = pWidths[i] == 0 ? 1 : pWidths[i];
            width = (c->bounds.width - absoluteWidth - (spacing * (n - 1))) * mult / inherit;
        }
        else
            width = pWidths[i];

        if (style->Wrapped && x + width > c->bounds.width)
        {
            x = 0;
            y += height + spacing;
        }

        if (i == 0)
            out.data = createChild(c, {x, y, width, height});
        else
            createChild(c, {x, y, width, height});

        x += width;
    }

    fbox arranged = Arrange(c, {c->bounds.x, c->bounds.y, x, y + height});

    for (uint32_t i = 0; i < n; i++)
    {
        out[i]->bounds.pos = out[i]->bounds.pos + arranged.pos;
    }

    return out;

}
TGContainerArray TexGui::Column(TGContainer* c, uint32_t heightCount, const float* pHeights, float width, TexGui::ColumnStyle* style)
{
    if (style == nullptr)
        style = &GTexGui->styleStack.back()->Column;
    if (width < 1) {
        width = width == 0 ? c->bounds.width : c->bounds.width * width;
    }
    float absoluteHeight = 0;
    int inherit = 0;
    int n = heightCount;
    for (int i = 0; i < n; i++)
    {
        if (pHeights[i] >= 1)
            absoluteHeight += pHeights[i];
        else
            inherit++;
    }

    float x = 0, y = 0;
    float spacing = style->Spacing;
    TGContainerArray out;
    out.size = n;
    for (uint32_t i = 0; i < n; i++)
    {
        float height;
        if (pHeights[i] <= 1)
        {
            float mult = pHeights[i] == 0 ? 1 : pHeights[i];
            height = (c->bounds.height - absoluteHeight - (spacing * (n - 1))) * mult / inherit;
        }
        else
            height = pHeights[i];

        if (i == 0)
            out.data = createChild(c, {x, y, width, height});
        else
            createChild(c, {x, y, width, height});

        y += height + spacing;
    }

    fbox arranged = Arrange(c, {c->bounds.x, c->bounds.y, x + width, y});
    for (uint32_t i = 0; i < n; i++)
    {
        out[i]->bounds.pos = out[i]->bounds.pos + arranged.pos;
    }
    return out;

}

TGContainerArray TexGui::Row(TGContainer* c, std::initializer_list<float> widths, float height, RowStyle* style)
{
    return TexGui::Row(c, widths.size(), widths.begin(), height, style);
}

TGContainerArray TexGui::Column(TGContainer* c, std::initializer_list<float> heights, float width, ColumnStyle* style)
{
    return TexGui::Column(c, heights.size(), heights.begin(), width, style);
}

Math::fbox TexGui::getBounds(TGContainer* c)
{
    return c->bounds;
}

Math::fbox TexGui::getSize(TGContainer* c)
{
    return c->size;
}
// Text processing

Texture* IconSheet::getIcon(uint32_t x, uint32_t y)
{
    ibox bounds = { Math::ivec2(x, y + 1) * Math::ivec2(iw, ih), Math::ivec2(iw, ih) };
    return &m_custom_texs.emplace_back(glID, bounds, Math::ivec2{w, h}, 0, 0, 0, 0);
}

float TexGui::computeTextWidth(const std::vector<uint32_t>& codepoints)
{
    float total = 0;
    Style& style = *GTexGui->styleStack.back();
    for (size_t i = 0; i < codepoints.size(); i++)
    {
        // #TODO: decode utf8
        total += style.Text.Font->getGlyph(codepoints[i]).advanceX;
    }
    return total;
}

bool RenderData::drawTextSelection(const std::vector<uint32_t>& codepoints, TextInputState* textInput, Math::fvec2 textPos, int size)
{

    auto& io = inputFrame;
    int& textCursorPos = textInput->textCursorPos;
    Style& style = *GTexGui->styleStack.back();
    Font* font = style.Text.Font;

    float currx = textPos.x;
    float curry = textPos.y;

    float cursorPosLocation = -1;
    for (int i = 0; i < codepoints.size(); i++)
    {
        FontGlyph glyph = font->getGlyph(codepoints[i]);
        float advance = glyph.advanceX * size;
        if (textInput->state & STATE_ACTIVE && io.lmb == KEY_Press)
        {
            textInput->selection[0] = -1;
            textInput->selection[1] = -1;
            if (io.cursorPos.x >= currx && io.cursorPos.x <= currx + advance * 0.5)
                textCursorPos = i;
            else if (io.cursorPos.x > currx + advance * 0.5)
                textCursorPos = i + 1;
        }

        if (textCursorPos == i)
            cursorPosLocation = currx;

        if (textInput->state & STATE_ACTIVE && io.lmb == KEY_Held)
        {
            if (io.cursorPos.x >= currx && io.cursorPos.x <= currx + advance * 0.5)
            {
                textInput->selection[i >= textCursorPos ? 0 : 1] = textCursorPos;
                textInput->selection[i >= textCursorPos ? 1 : 0] = i;
            }
            else if (io.cursorPos.x > currx + advance * 0.5)
            {
                textInput->selection[i >= textCursorPos ? 0 : 1] = textCursorPos;
                textInput->selection[i >= textCursorPos ? 1 : 0] = i + 1;
            }
        }
        // If currIdx is within range of the selection
        if (i >= textInput->selection[0] && i < textInput->selection[1])
        {
            addQuad({currx, curry + size / 4.f - size, advance, float(size)}, style.TextInput.SelectColor);
        }

        currx += advance;
    }

    if (textCursorPos == codepoints.size())
        cursorPosLocation = currx;

    const Math::ivec2& framebufferSize = GTexGui->framebufferSize;

    if (textInput->state & STATE_ACTIVE && cursorPosLocation != -1)
    {
        float cursorY = curry + size / 4.f;

        vertices.emplace_back(Vertex{.pos = {cursorPosLocation, cursorY - size}});
        vertices.emplace_back(Vertex{.pos = {cursorPosLocation + 2, cursorY - size}});
        vertices.emplace_back(Vertex{.pos = {cursorPosLocation, cursorY}});
        vertices.emplace_back(Vertex{.pos = {cursorPosLocation + 2, cursorY}});
        uint32_t idx = vertices.size() - 4;
        indices.emplace_back(idx);
        indices.emplace_back(idx+1);
        indices.emplace_back(idx+2);
        indices.emplace_back(idx+1);
        indices.emplace_back(idx+2);
        indices.emplace_back(idx+3);

        commands.emplace_back(Command{
            .type = RD_CMD_Draw,
            .draw = {
                .indexCount = 6,
                .textureIndex = 0,
                .scaleX = 2.f / float(framebufferSize.x),
                .scaleY = 2.f / float(framebufferSize.y),
            }
        });

    }

    return false;
}

bool decodeUTF8(const TGStr& text, std::vector<uint32_t>& out)
{
    uint32_t curr = 0;
    //#TODO: more invalid string checks
    while (curr < text.len)
    {
        uint32_t codepoint = 0;
        uint32_t start = curr;
        if (text.utf8[curr] & 0b10000000)
        {
            if (text.utf8[curr] & 0b01000000)
            {
                if (text.utf8[curr] & 0b00100000)
                {
                    if (text.utf8[curr] & 0b00010000)
                    {
                        codepoint |= text.utf8[curr] & 0b00000111;
                        codepoint <<= 6;
                        curr++;
                    }

                    codepoint |= text.utf8[curr] & (curr == start ? 0b00001111 : 0b00111111);
                    codepoint <<= 6;
                    curr++;
                }

                codepoint |= text.utf8[curr] & (curr == start ? 0b00011111 : 0b00111111);
                codepoint <<= 6;
                curr++;
            }
            else
            {
                //Invalid string
                assert(false);
                return 1;
            }
            codepoint |= text.utf8[curr] & 0b00111111;
            curr++;
        }
        else
        {
            codepoint = text.utf8[curr++];
        }

        out.push_back(codepoint);
    }
}

void RenderData::addText(TGStr text, Math::fvec2 pos, uint32_t col, int _size, float boundWidth, float boundHeight, Math::fbox* boundsOut, uint32_t flags, TextInputState* textInput)
{
    col &= ~(ALPHA_MASK);
    col |= alphaModifier;

    // #TODO: We don't actually know the number of chars!
    // This will be the number of utf8 points which doesn't even remotely map to the number of glyphs to render
    // We should change this to a multi-step thing:
    // 1. Text shaping + breaking:
    //    - Get correct x-advance of each character (with kerning based on prev character)
    //    - Based on bounds (needs to be added as param), choose any break points necessary
    //      (iterate line breaks using ICU (bleh) or kb_text_shape)
    // 2. For each line:
    //    - Choose pen start x,y based on line length and alignment (center_y, etc)
    //    - Make quad for each, advancing XY

    float size = _size * GTexGui->scale;
    pos *= GTexGui->scale;

    Style& style = *GTexGui->styleStack.back();
    Font* font = style.Text.Font;

    //i still have no idea how to do this properly i looked everywehre istg
    if (flags & CENTER_Y)
    {
        pos.y += ceil(boundHeight / 2.f);
    }

    auto glyph = font->getGlyph('x');
    pos.y += ceil((glyph.Y1 - glyph.Y0) * size / 2.f);

    std::vector<uint32_t> codepoints;
    //#TODO: do this in-place without fking everything else
    decodeUTF8(text, codepoints);

    // #TODO kill this
    if (flags & CENTER_X)
    {
        pos.x += boundWidth / 2.f;
        pos.x -= computeTextWidth(codepoints) * size / 2.0;
    }

    float currx = pos.x;
    float curry = pos.y;

    if (textInput)
        drawTextSelection(codepoints, textInput, pos, size);

    const Math::ivec2& framebufferSize = GTexGui->framebufferSize;

    for (auto codepoint : codepoints)
    {
        FontGlyph glyph = font->getGlyph(codepoint);
        float advance = glyph.advanceX * size;

        if ((currx - pos.x) + advance > boundWidth && boundWidth > 0)
        {
            curry += size;
            currx = pos.x;
        }

        if (glyph.visible)
        {
            float x0 = currx + glyph.X0 * size;
            float y0 = curry + glyph.Y0 * size;
            float x1 = currx + glyph.X1 * size;
            float y1 = curry + glyph.Y1 * size;

            vertices.emplace_back(Vertex{.pos = {x0, y0}, .uv = {glyph.U0, glyph.V0}, .col = col});
            vertices.emplace_back(Vertex{.pos = {x1, y0}, .uv = {glyph.U1, glyph.V0}, .col = col});
            vertices.emplace_back(Vertex{.pos = {x0, y1}, .uv = {glyph.U0, glyph.V1}, .col = col});
            vertices.emplace_back(Vertex{.pos = {x1, y1}, .uv = {glyph.U1, glyph.V1}, .col = col});
            uint32_t idx = vertices.size() - 4;

            indices.emplace_back(idx);
            indices.emplace_back(idx+1);
            indices.emplace_back(idx+2);
            indices.emplace_back(idx+1);
            indices.emplace_back(idx+2);
            indices.emplace_back(idx+3);

            commands.emplace_back(Command{
                .type = RD_CMD_Draw,
                .draw = {
                    .indexCount = 6,
                    .textureIndex = font->atlasTexture->id,
                    .scaleX = 2.f / float(framebufferSize.x),
                    .scaleY = 2.f / float(framebufferSize.y),
                    .uvScaleX = 1.f / float(font->atlasTexture->bounds.width),
                    .uvScaleY = 1.f / float(font->atlasTexture->bounds.height),
                }
            });
        }

        currx += advance;
    }
}

static inline uint32_t getTextureIndexFromState(Texture* e, int state)
{
    return state & STATE_PRESS && e->press != -1 ? e->press :
        state & STATE_HOVER && e->hover != -1 ? e->hover :
        state & STATE_ACTIVE && e->active != -1 ? e->active : e->id;
}

void RenderData::pushScissor(Math::fbox region)
{
    region *= GTexGui->scale;
    commands.emplace_back(Command{
        .type = RD_CMD_Scissor,
        .scissor = {
            .push = true,
            .x = int(region.x),
            .y = int(region.y),
            .width = int(region.width),
            .height = int(region.height),
        }
    });
}

void RenderData::popScissor()
{
    commands.emplace_back(Command{
        .type = RD_CMD_Scissor,
        .scissor = {
            .push = false,
        }
    });
}

void RenderData::addTexture(fbox rect, Texture* e, int state, int pixel_size, uint32_t flags, uint32_t col)
{
    if (!e || e->id == -1) return;
    col &= ~(ALPHA_MASK);
    col |= alphaModifier;

    uint32_t tex = getTextureIndexFromState(e, state);

    auto& g = *GTexGui;
    Math::ivec2 framebufferSize = g.framebufferSize;

    rect *= g.scale;
    if (!(flags & SLICE_9))
    {
        Math::fbox texBounds = e->bounds;

        vertices.emplace_back(Vertex{.pos = {rect.x, rect.y}, .uv = {texBounds.x, texBounds.y}, .col = col});
        vertices.emplace_back(Vertex{.pos = {rect.x + rect.w, rect.y}, .uv = {texBounds.x + texBounds.w, texBounds.y}, .col = col});
        vertices.emplace_back(Vertex{.pos = {rect.x, rect.y + rect.h}, .uv = {texBounds.x, texBounds.y + texBounds.h}, .col = col});
        vertices.emplace_back(Vertex{.pos = {rect.x + rect.w, rect.y + rect.h}, .uv = {texBounds.x + texBounds.w, texBounds.y + texBounds.h}, .col = col});
        uint32_t idx = vertices.size() - 4;
        indices.emplace_back(idx);
        indices.emplace_back(idx+1);
        indices.emplace_back(idx+2);
        indices.emplace_back(idx+1);
        indices.emplace_back(idx+2);
        indices.emplace_back(idx+3);

        commands.emplace_back(Command{
            .type = RD_CMD_Draw,
            .draw = {
                .indexCount = 6,
                .textureIndex = tex,
                .scaleX = 2.f / float(framebufferSize.x),
                .scaleY = 2.f / float(framebufferSize.y),
                .uvScaleX = 1.f / float(e->size.x),
                .uvScaleY = 1.f / float(e->size.y),
            }
        });
        return;
    }

    float rectSliceH[][2] = {
        {rect.x, e->left * pixel_size},
        {rect.x + e->left * pixel_size, rect.width - (e->left + e->right) * pixel_size},
        {rect.x + rect.width - e->right * pixel_size, e->right * pixel_size}
    };
    float texBoundsSliceH[][2] = {
        {e->bounds.x, e->left},
        {e->bounds.x + e->left, e->bounds.width - (e->left + e->right)},
        {e->bounds.x + e->bounds.width - e->right, e->right}
    };

    float rectSliceV[][2] = {
        {rect.y, e->top * pixel_size},
        {rect.y + e->top * pixel_size, rect.height - (e->top + e->bottom) * pixel_size},
        {rect.y + rect.height - e->bottom * pixel_size, e->bottom * pixel_size}
    };

    float texBoundsSliceV[][2] = {
        {e->bounds.y, e->top},
        {e->bounds.y + e->top, e->bounds.height - (e->top + e->bottom)},
        {e->bounds.y + e->bounds.height - e->bottom, e->bottom}
    };

    int minY = flags & SLICE_3_VERTICAL ? 0 : 1;
    int maxY = flags & SLICE_3_VERTICAL ? 3 : 2;
    int minX = flags & SLICE_3_HORIZONTAL ? 0 : 1;
    int maxX = flags & SLICE_3_HORIZONTAL ? 3 : 2;

    for (int y = minY; y < maxY; y++)
    {
        for (int x = minX; x < maxX; x++)
        {
            Math::fbox slice = rect;
            Math::fbox texBounds = e->bounds;

            if (flags & SLICE_3_HORIZONTAL)
            {
                slice.x = rectSliceH[x][0];
                slice.width = rectSliceH[x][1];
                texBounds.x = texBoundsSliceH[x][0];
                texBounds.width = texBoundsSliceH[x][1];
            }

            if (flags & SLICE_3_VERTICAL)
            {
                slice.y = rectSliceV[y][0];
                slice.height = rectSliceV[y][1];
                texBounds.y = texBoundsSliceV[y][0];
                texBounds.height = texBoundsSliceV[y][1];
            }

            vertices.emplace_back(Vertex{.pos = {slice.x, slice.y}, .uv = {texBounds.x, texBounds.y}, .col = col});
            vertices.emplace_back(Vertex{.pos = {slice.x + slice.w, slice.y}, .uv = {texBounds.x + texBounds.w, texBounds.y}, .col = col});
            vertices.emplace_back(Vertex{.pos = {slice.x, slice.y + slice.h}, .uv = {texBounds.x, texBounds.y + texBounds.h}, .col = col});
            vertices.emplace_back(Vertex{.pos = {slice.x + slice.w, slice.y + slice.h}, .uv = {texBounds.x + texBounds.w, texBounds.y + texBounds.h}, .col = col});
            uint32_t idx = vertices.size() - 4;
            indices.emplace_back(idx);
            indices.emplace_back(idx+1);
            indices.emplace_back(idx+2);
            indices.emplace_back(idx+1);
            indices.emplace_back(idx+2);
            indices.emplace_back(idx+3);
        }
    }

    commands.emplace_back(Command{
            .type = RD_CMD_Draw,
            .draw = {
                .indexCount = uint32_t(6 * (flags & SLICE_3_HORIZONTAL ? 3 : 1) * (flags & SLICE_3_VERTICAL ? 3 : 1)),
                .textureIndex = tex,
                .scaleX = 2.f / float(framebufferSize.x),
                .scaleY = 2.f / float(framebufferSize.y),
                .uvScaleX = 1.f / float(e->size.x),
                .uvScaleY = 1.f / float(e->size.y),
            }
    });
}

void RenderData::addQuad(Math::fbox rect, uint32_t col)
{
    col &= ~(ALPHA_MASK);
    col |= alphaModifier;
    Math::ivec2 framebufferSize = GTexGui->framebufferSize;
    rect *= GTexGui->scale;

    vertices.emplace_back(Vertex{.pos = {rect.x, rect.y}, .uv = {0,0}, .col = col,});
    vertices.emplace_back(Vertex{.pos = {rect.x + rect.w, rect.y}, .uv = {0, 0}, .col = col,});
    vertices.emplace_back(Vertex{.pos = {rect.x, rect.y + rect.h}, .uv = {0, 0}, .col = col,});
    vertices.emplace_back(Vertex{.pos = {rect.x + rect.w, rect.y + rect.h}, .uv = {0, 0}, .col = col,});
    uint32_t idx = vertices.size() - 4;
    indices.emplace_back(idx);
    indices.emplace_back(idx+1);
    indices.emplace_back(idx+2);
    indices.emplace_back(idx+1);
    indices.emplace_back(idx+2);
    indices.emplace_back(idx+3);

    commands.emplace_back(Command{
        .type = RD_CMD_Draw,
        .draw = {
            .indexCount = 6,
            .textureIndex = 0,
            .scaleX = 2.f / float(framebufferSize.x),
            .scaleY = 2.f / float(framebufferSize.y),
        }
    });
}

// from imgui
#define IM_NORMALIZE2F_OVER_ZERO(VX,VY)     { float d2 = VX*VX + VY*VY; if (d2 > 0.0f) { float inv_len = 1.0/sqrt(d2); VX *= inv_len; VY *= inv_len; } } (void)0

void RenderData::addLine(float x1, float y1, float x2, float y2, uint32_t col, float lineWidth)
{
    Math::ivec2 framebufferSize = GTexGui->framebufferSize;

    col &= ~(ALPHA_MASK);
    col |= alphaModifier;

    auto& g = *GTexGui;
    x1 *= g.scale;
    x2 *= g.scale;
    y1 *= g.scale;
    y2 *= g.scale;
    lineWidth *= g.scale;

    float dx = x2 - x1;
    float dy = y2 - y1;

    IM_NORMALIZE2F_OVER_ZERO(dx, dy);

    dx *= (lineWidth * 0.5f);
    dy *= (lineWidth * 0.5f);

    vertices.emplace_back(Vertex{.pos = {x1 + dy, y1 - dx}, .uv = {0,0}, .col = col});
    vertices.emplace_back(Vertex{.pos = {x2 + dy, y2 - dx}, .uv = {0,0}, .col = col,});
    vertices.emplace_back(Vertex{.pos = {x2 - dy, y2 + dx}, .uv = {0,0}, .col = col,});
    vertices.emplace_back(Vertex{.pos = {x1 - dy, y1 + dx}, .uv = {0,0}, .col = col,});

    uint32_t idx = vertices.size() - 4;
    indices.emplace_back(idx);
    indices.emplace_back(idx+1);
    indices.emplace_back(idx+2);
    indices.emplace_back(idx);
    indices.emplace_back(idx+2);
    indices.emplace_back(idx+3);

    commands.emplace_back(Command{
        .type = RD_CMD_Draw,
        .draw = {
            .indexCount = 6,
            .textureIndex = 0,
            .scaleX = 2.f / float(framebufferSize.x),
            .scaleY = 2.f / float(framebufferSize.y),
        }
    });
}
