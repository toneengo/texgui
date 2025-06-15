#include "texgui.h"
#include "texgui_flags.hpp"
#include "texgui_style.hpp"
#include "texgui_types.hpp"
#include <cassert>
#include <cstring>
#include <cmath>
#include <mutex>
#include <list>
#include <filesystem>
#include <vulkan/vulkan_core.h>
#include "stb_image.h"
#include "msdf-atlas-gen/msdf-atlas-gen.h"
#include "texgui_internal.hpp"
#include "msdf-atlas-gen/GlyphGeometry.h"

using namespace TexGui;

#define HOVERED(rect) rect.contains(inputFrame.cursorPos) && parentState & STATE_HOVER && scissor.contains(inputFrame.cursorPos)
#define CLICKED(rect) rect.contains(inputFrame.cursorPos) && inputFrame.lmb == KEY_Press && parentState & STATE_ACTIVE && scissor.contains(inputFrame.cursorPos)

Style* initDefaultStyle();
void TexGui::init()
{
    GTexGui = new TexGuiContext();
    GTexGui->defaultStyle = initDefaultStyle();
}

TexGui::Math::ivec2 _getScreenSize()
{
    return GTexGui->framebufferSize;
}

void TexGui::newFrame()
{
    GTexGui->rendererFns.newFrame();
}

RenderData* TexGui::newRenderData()
{
    return new RenderData();
}

bool TexGui::isCapturingMouse()
{
    return GTexGui->lastCapturingMouse;
}


#ifndef M_PI
#define M_PI  3.14159265358979323846264  // from CRC
#endif

using namespace msdf_atlas;

uint32_t TexGui::loadFont(const char* font_path, int baseFontSize, float msdfPxRange)
{
    std::string fontName = font_path;
    fontName.erase(0, fontName.find_last_of('/') + 1);
    fontName.erase(fontName.begin() + fontName.find_last_of('.'), fontName.end());

    Texture output;
    int width = 0, height = 0;
    bool success = false;
    // Initialize instance of FreeType library

    std::vector<GlyphGeometry>& glyphs = GTexGui->fonts[fontName].glyphs;

    msdfgen::FreetypeHandle *ft = msdfgen::initializeFreetype();
    assert(ft);

    msdfgen::FontHandle *font = msdfgen::loadFont(ft, font_path);
    assert(font);

    // Copied from msdf-gen-atlas example

    // Storage for glyph geometry and their coordinates in the atlas
    // FontGeometry is a helper class that loads a set of glyphs from a single font.
    // It can also be used to get additional font metrics, kerning information, etc.
    FontGeometry fontGeometry(&glyphs);
    // Load a set of character glyphs:
    // The second argument can be ignored unless you mix different font sizes in one atlas.
    // In the last argument, you can specify a charset other than ASCII.
    // To load specific glyph indices, use loadGlyphs instead.
    fontGeometry.loadCharset(font, 1.0, Charset::ASCII);

    // Apply MSDF edge coloring. See edge-coloring.h for other coloring strategies.
    const double maxCornerAngle = M_PI / 4.0;
    for (GlyphGeometry &glyph : glyphs)
        glyph.edgeColoring(&msdfgen::edgeColoringSimple, maxCornerAngle, 0);
    // TightAtlasPacker class computes the layout of the atlas.
    TightAtlasPacker packer;
    // Set atlas parameters:
    // setDimensions or setDimensionsConstraint to find the best value
    packer.setDimensionsConstraint(DimensionsConstraint::SQUARE);
    // setScale for a fixed size or setMinimumScale to use the largest that fits
    packer.setScale(baseFontSize);
    // setPixelRange or setUnitRange
    packer.setPixelRange(msdfPxRange);
    packer.setMiterLimit(4.0);
    // Compute atlas layout - pack glyphs
    packer.pack(glyphs.data(), glyphs.size());
    // Get final atlas dimensions
    packer.getDimensions(width, height);
    // The ImmediateAtlasGenerator class facilitates the generation of the atlas bitmap.
    ImmediateAtlasGenerator<
        float, // pixel type of buffer for individual glyphs depends on generator function
        3, // number of atlas color channels
        msdfGenerator, // function to generate bitmaps for individual glyphs
        BitmapAtlasStorage<byte, 3> // class that stores the atlas bitmap
        // For example, a custom atlas storage class that stores it in VRAM can be used.
    > generator(width, height);
    // GeneratorAttributes can be modified to change the generator's default settings.
    GeneratorAttributes attributes;
    generator.setAttributes(attributes);
    generator.setThreadCount(4);
    // Generate atlas bitmap
    generator.generate(glyphs.data(), glyphs.size());
    // The atlas bitmap can now be retrieved via atlasStorage as a BitmapConstRef.
    // The glyphs array (or fontGeometry) contains positioning data for typesetting text.
    msdfgen::BitmapConstRef<byte, 3> ref = generator.atlasStorage();

    // Transform into RGBA
    unsigned char* rgba = new unsigned char[width * height * 4];
    for (int r = 0; r < height; r++)
    {
        for (int c = 0; c < width; c++)
        {
            int index = (r * width) + c;
            rgba[index * 4] = *((unsigned char*)(ref.pixels) + (index * 3));
            rgba[index * 4 + 1] = *((unsigned char*)(ref.pixels) + (index * 3 + 1));
            rgba[index * 4 + 2] = *((unsigned char*)(ref.pixels) + (index * 3 + 2));
            rgba[index * 4 + 3] = 255;
        }
    }

    GTexGui->fonts[fontName].textureIndex = GTexGui->rendererFns.createFontAtlas((void*)rgba, width, height);
    GTexGui->fonts[fontName].msdfPxRange = msdfPxRange;
    GTexGui->fonts[fontName].baseFontSize = baseFontSize;
    GTexGui->fonts[fontName].atlasWidth = width;
    GTexGui->fonts[fontName].atlasHeight = height;

    delete [] rgba;

    // Cleanup
    msdfgen::destroyFont(font);
    msdfgen::deinitializeFreetype(ft);

    for (const auto& glyph : glyphs)
    {
        GTexGui->fonts[fontName].codepointToGlyph[glyph.getCodepoint()] = &glyph;
    }

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
            printf("Unexpected file in sprites folder: %s\n", pstr.c_str());
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
    auto& g = *GTexGui;
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

ContainerState getState(TexGuiID id, const fbox& bounds, uint32_t parentState, const fbox& scissor)
{
    ContainerState state = 0;
    if (HOVERED(bounds))
    {
        GTexGui->hoveredWidget = id;
        state |= STATE_HOVER;
    }
    if (CLICKED(bounds))
    {
        GTexGui->activeWidget = id;
        state |= STATE_PRESS;
    }
    if (GTexGui->activeWidget == id) state |= STATE_ACTIVE;
    if (GTexGui->activeWidget == id && inputFrame.lmb == KEY_Held) state |= STATE_PRESS;
    return state;
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

    if (f & ALIGN_CENTER_X) child.x = bounds.x + (bounds.width - child.width) / 2;
    if (f & ALIGN_CENTER_Y) child.y = bounds.y + (bounds.height - child.height) / 2;
    return child;
}

#define _PX GTexGui->pixelSize
Container Container::Window(const char* name, float xpos, float ypos, float width, float height, uint32_t flags, WindowStyle* style)
{
    auto& io = inputFrame;
    if (!GTexGui->windows.contains(name))
    {
        for (auto& entry : GTexGui->windows)
        {
            if (entry.second.order != INT_MAX) entry.second.order++;
        }

        GTexGui->windows.insert({name, TexGuiWindow{
            .id = int(GTexGui->windows.size()),
            .box = {xpos, ypos, width, height},
        }});
    }

    if (style == nullptr)
        style = &GTexGui->styleStack.back()->Window;

    Texture* wintex = style->Texture;
    assert(wintex && wintex->id != -1);
    TexGuiWindow& wstate = GTexGui->windows[name];

    wstate.visible = true;

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

    if (flags & LOCKED || !wstate.prevVisible)
    {
        wstate.box = {xpos, ypos, width, height};
        wstate.box = AlignBox(Math::fbox(0, 0, GTexGui->framebufferSize.x, GTexGui->framebufferSize.y), wstate.box, flags);
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
    padding.top = wintex->top * _PX;

        fbox internal = fbox::pad(wstate.box, padding);

    renderData->ordered = true;
    parentState = wstate.state;

    Container child = withBounds(internal);
    child.window = &wstate;
    child.renderData = &renderData->children.emplace_back();

    //lowest order will be rendered last.
    child.renderData->priority = -wstate.order;
    child.renderData->addTexture(wstate.box, wintex, wstate.state, _PX, SLICE_9, scissor);

    if (!(flags & HIDE_TITLE)) 
        child.renderData->addText(name, {wstate.box.x + padding.left, wstate.box.y + wintex->top * _PX / 2},
                 style->TitleColor, style->TitleFontSize, CENTER_Y, scissor);


    return child;
}

bool Container::Button(const char* text, Container* out, ButtonStyle* style)
{
    auto& g = *GTexGui;
    auto& io = inputFrame;
    TexGuiID id = window->getID(text);

    bounds = Arrange(this, bounds);

    if (style == nullptr)
        style = &GTexGui->styleStack.back()->Button;

    uint32_t state = getState(id, bounds, parentState, scissor);

    Texture* tex = style->Texture;
    assert(tex && tex->id != -1);

    renderData->addTexture(bounds, tex, state, _PX, SLICE_9, scissor);

    TexGui::Math::vec2 pos = state & STATE_PRESS 
                       ? bounds.pos + style->POffset
                       : bounds.pos;
    fbox innerBounds = {pos, bounds.size};
    if (out)
        *out = withBounds(innerBounds);
    else
    {
        innerBounds.pos += bounds.size / 2;
        renderData->addText(text, innerBounds, style->Text.Color, style->Text.Size, CENTER_X | CENTER_Y, scissor);
    }

    bool hovered = scissor.contains(io.cursorPos) 
                && bounds.contains(io.cursorPos);

    return state & STATE_ACTIVE && io.lmb == KEY_Release && hovered ? true : false;
}

Container Container::Box(float xpos, float ypos, float width, float height, BoxStyle* style)
{
    if (width <= 1)
        width = width == 0 ? bounds.width : bounds.width * width;
    if (height <= 1)
        height = height == 0 ? bounds.height : bounds.height * height;

    if (style == nullptr)
        style = &GTexGui->styleStack.back()->Box;
    Texture* texture = style->Texture;

    fbox boxBounds(bounds.x + xpos, bounds.y + ypos, width, height);

    fbox internal = fbox::pad(boxBounds, style->Padding);
    Container child = withBounds(internal);
    child.renderData = &renderData->children.emplace_back();

    if (texture)
    {
        child.renderData->addTexture(boxBounds, texture, 0, 2, SLICE_9, scissor);
    }

    return child; 
}

void Container::CheckBox(bool* val, CheckBoxStyle* style)
{
    if (style == nullptr)
        style = &GTexGui->styleStack.back()->CheckBox;
    Texture* texture = style->Texture;

    auto& io = inputFrame;
    if (io.lmb == KEY_Release && bounds.contains(io.cursorPos) && parentState & STATE_HOVER) *val = !*val;

    if (texture == nullptr) return;
    renderData->addTexture(bounds, texture, *val ? STATE_ACTIVE : 0, 2, SLICE_9, scissor);
}

void Container::RadioButton(uint32_t* selected, uint32_t id, RadioButtonStyle* style)
{
    if (style == nullptr)
        style = &GTexGui->styleStack.back()->RadioButton;
    Texture* texture = style->Texture;

    auto& io = inputFrame;
    if (io.lmb == KEY_Release && bounds.contains(io.cursorPos) && parentState & STATE_HOVER) *selected = id;

    if (texture == nullptr) return;
    renderData->addTexture(bounds, texture, *selected == id ? STATE_ACTIVE : 0, 2, SLICE_9, scissor);

}

void Container::Line(float x1, float y1, float x2, float y2, Math::fvec4 color, float lineWidth)
{
    renderData->addLine(bounds.x + x1, bounds.y + y1, bounds.x + x2, bounds.y + y2, color, lineWidth);
}

Container Container::ScrollPanel(const char* name, ScrollPanelStyle* style)
{
    if (style == nullptr)
        style = &GTexGui->styleStack.back()->ScrollPanel;
    Texture* texture = style->PanelTexture;
    Texture* bartex = style->BarTexture;
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

    fvec4 padding = style->Padding;
    
    if (!bartex)
    {
        static auto defaultbt = bartex;
        bartex = defaultbt;
    }

    padding.right += bartex->bounds.width;

    static auto arrange = [](Container* scroll, fbox child)
    {
        auto& bounds = ((ScrollPanelState*)(scroll->scrollPanelState))->bounds;
        bounds = child;
        return bounds;
    };

    Container sp = withBounds(Math::fbox::pad(bounds, padding), arrange);
    sp.scrollPanelState = &spstate;
    sp.scissor = sp.bounds;

    if (spstate.scrolling && io.cursorPos.y >= sp.bounds.y && io.cursorPos.y < sp.bounds.y + sp.bounds.height)
        spstate.contentPos.y -= inputFrame.mouseRelativeMotion.y * spstate.bounds.height / sp.bounds.height;

    if (sp.scissor.contains(io.cursorPos))
        spstate.contentPos.y += inputFrame.scroll.y;

    float height = sp.bounds.height - padding.top - padding.bottom;
    if (spstate.bounds.height + spstate.contentPos.y < height)
        spstate.contentPos.y = -(spstate.bounds.height - height);
    spstate.contentPos.y = fmin(spstate.contentPos.y, 0);

    float barh = fmin(bounds.height, bounds.height * height / spstate.bounds.height);
    fbox bar = {bounds.x + bounds.width - padding.right,
                bounds.y + (bounds.height - barh) * -spstate.contentPos.y / (spstate.bounds.height - height),
                padding.right,
                barh};

    renderData->addTexture(bounds, texture, 0, _PX, 0, bounds);
    renderData->addTexture(bar, bartex, 0, 2, SLICE_9, bounds);

    if (bar.contains(io.cursorPos) && io.lmb == KEY_Press)
        spstate.scrolling = true;
    else if (io.lmb != KEY_Held)
        spstate.scrolling = false;

    sp.bounds.y += spstate.contentPos.y;

    return sp;
}

void renderSlider(RenderData* renderData, TexGuiID id, const fbox& bounds, float percent, Texture* bar, Texture* node)
{
    //just uses the height of the bar texture

}

int Container::SliderInt(int* val, int minVal, int maxVal, SliderStyle* style)
{
    if (style == nullptr)
        style = &GTexGui->styleStack.back()->Slider;
    auto& g = *GTexGui;
    auto& io = inputFrame;
    auto bar = style->BarTexture;
    auto node = style->NodeTexture;

    TexGuiID id = window->getID(&val);

    float percent = float(*val - minVal) / float(maxVal - minVal);
    //renderSlider(renderData, id, bounds, percent, bar, node);
    
    assert(bar);
    fbox barArea = {bounds.x, bounds.y, bounds.width, bar->bounds.height * _PX};
    barArea = Arrange(this, barArea);

    uint32_t state = getState(id, barArea, parentState, scissor);
    renderData->addTexture(barArea, bar, state, _PX, SLICE_3_HORIZONTAL, scissor);

    if (g.activeWidget == id && io.lmb == KEY_Held)
    {
        float p = clamp(float(io.cursorPos.x - barArea.x) / barArea.width, 0.f, 1.f);
        *val = minVal + p * (maxVal - minVal);
    }

    if (node)
    {
        Math::fvec2 nodePos = {bounds.x - node->bounds.width * _PX * 0.5f + barArea.width * percent, barArea.y + barArea.height / 2.f - node->bounds.height * _PX / 2.f};
        fbox nodeArea = {bounds.x - node->bounds.width * _PX * 0.5f + barArea.width * percent, barArea.y + barArea.height / 2.f - node->bounds.height * _PX / 2.f,
                         node->bounds.width * _PX, node->bounds.height * _PX};
        uint32_t nodeState;
        getBoxState(nodeState, nodeArea, state);
        renderData->addTexture(nodeArea, node, nodeState, _PX, 0, scissor);
    }

    return *val;
}

void Container::Image(Texture* texture, int scale)
{
    if (!texture) return;
    Style& style = *GTexGui->styleStack.back();
    if (scale == -1)
        scale = _PX;

    Math::ivec2 tsize = Math::ivec2(texture->bounds.width, texture->bounds.height) * scale;

    fbox sized = fbox(bounds.pos, fvec2(tsize));
    fbox arranged = Arrange(this, sized);

    renderData->addTexture(arranged, texture, STATE_NONE, scale, 0, scissor);
}

bool Container::DropdownInt(int* val, std::initializer_list<std::pair<const char*, int>> names)
{

}

Container Container::Tooltip(Math::fvec2 size, TooltipStyle* style)
{
    if (style == nullptr)
        style = &GTexGui->styleStack.back()->Tooltip;
    auto& io = inputFrame;
    Texture* tex = style->Texture;
    
    fbox rect = {
        io.cursorPos + style->MouseOffset, 
        size,
    };
    rect = fbox::pad(rect, style->Padding);

    auto box = Box(rect.x - bounds.x, rect.y - bounds.y, rect.w, rect.h);

    return box;
}

fbox Container::Arrange(Container* o, fbox child)
{
    // This container doesn't arrange anything.
    if (!o->arrange) return child;

    return o->arrange(o, child);
}

// Arranges the list item based on the thing that is put inside it.
Container Container::ListItem(uint32_t* selected, uint32_t id, ListItemStyle* style)
{
    static auto arrange = [](Container* listItem, fbox child)
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
            getBoxState(state, bounds, listItem->parentState);
            if (io.lmb == KEY_Press && state & STATE_HOVER)
                *(listItem->listItem.selected) = listItem->listItem.id;

            if (*(listItem->listItem.selected) == listItem->listItem.id)
                state = STATE_ACTIVE;
        }
        else
        {
            state = listItem->listItem.id ? STATE_ACTIVE : STATE_NONE;
        }

        listItem->renderData->addTexture(bounds, listItem->texture, state, _PX, SLICE_9, listItem->scissor);

        // The child is positioned by the list item's parent
        return fbox::pad(bounds, style.ListItem.Padding);
    };
    if (style == nullptr)
        style = &GTexGui->styleStack.back()->ListItem;
    // Add padding to the contents of the list item
    static Texture* tex = style->Texture;
    Container listItem = withBounds(fbox::pad(bounds, style->Padding), arrange);
    listItem.texture = !texture ? tex : texture;
    listItem.listItem = { selected, id };
    return listItem;
}

Container Container::Align(uint32_t flags, const Math::fvec4 padding)
{
    static auto arrange = [](Container* align, fbox child)
    {
        Math::fvec4 pad = {align->align.top, align->align.right, align->align.bottom, align->align.left};
        fbox bounds = fbox::margin(child, pad);
        bounds = Arrange(align->parent, bounds);
        return fbox::pad(AlignBox(align->bounds, bounds, align->align.flags), pad);
    };
    // Add padding to the contents
    Style& style = *GTexGui->styleStack.back();
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
        //#TODO: grid styling
        Style& style = *GTexGui->styleStack.back();
        auto& gs = grid->grid;

        gs.rowHeight = fmax(gs.rowHeight, child.height);

        float spacing = style.Row.Spacing;
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
    renderData->addQuad(line, fvec4(1,1,1,0.5));
}

Container Container::Stack(float padding, StackStyle* style)
{
    static auto arrange = [](Container* stack, fbox child)
    {
        auto& s = stack->stack;

        child = fbox(stack->bounds.pos + fvec2(0, s.y), child.size);
        s.y += child.size.y + s.padding;
    
        Arrange(stack->parent, {stack->bounds.x, stack->bounds.y, stack->bounds.width, s.y});

        return child;
    };

    if (style == nullptr)
        style = &GTexGui->styleStack.back()->Stack;
    Container stack = withBounds(bounds, arrange);
    stack.stack = {0, padding < 0 ? style->Padding : padding};
    return stack;
}

Container Container::Node(float x, float y)
{
    static auto arrange = [](Container* align, fbox child)
    {
        child.x -= child.width / 2.f;
        child.y -= child.height / 2.f;
        fbox bounds = Arrange(align->parent, child);
        return bounds;
    };

    //this is weird maybe there is btter way. bounds is different now 
    //the bounds box itself is moved and not resized,
    //the child will be slightly out of bounds sincei t is centred on top left corner of bounds
    Container node = withBounds(bounds, arrange);
    node.bounds.x += x;
    node.bounds.y += y;
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

    renderData->addTexture(bounds, inputtex, tstate.state, _PX, SLICE_9, scissor);
    float offsetx = 0;
    fvec4 padding = style->Padding;
    fvec4 color = style->Text.Color;

    float startx = bounds.x + offsetx + padding.left;
    float starty = bounds.y + bounds.height / 2;

    //#TODO: size
    int size = style->Text.Size;

    renderData->addTextWithCursor(
        !getBit(tstate.state, STATE_ACTIVE) && len == 0
        ? name : text,
        {startx, starty},
        style->Text.Color,
        size,
        CENTER_Y,
        bounds,
        tstate
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
void Container::TextInput(const char* name, char* buf, uint32_t bufsize, TextInputStyle* style)
{
    auto& g = *GTexGui;
    auto& io = inputFrame;
    
    uint32_t id = window->getID(&buf);

    auto& ti = g.textInputs[id];

    bounds = Arrange(this, bounds);

    ti.state = getState(id, bounds, parentState, scissor);

    uint32_t len = strlen(buf);

    if (g.activeWidget == id)
    {
        g.editingText = true;
        TextInputBehaviour(ti, buf, bufsize, len);
    }
    
    renderTextInput(renderData, name, bounds, scissor, ti, buf, len, style);
}

static void renderTooltip(Paragraph text, RenderData* renderData);

// Bump the line of text
static void bumpline(float& x, float& y, float w, TexGui::Math::fbox& bounds, int32_t scale)
{
    if (x + w > bounds.x + bounds.width)
    {
        y += scale * 1.1f;
        x = bounds.x;
    }
}

fvec2 TexGui::calculateTextBounds(const char* text, float maxWidth, int32_t scale)
{
    if (scale == -1)
    {
        Style& style = *GTexGui->styleStack.back();
        scale = style.Text.Size;
    }

    auto ts = TextSegment::FromChunkFast(TextChunk(text));
    auto p = Paragraph(&ts, 1);

    return calculateTextBounds(p, scale, maxWidth);
}

fvec2 TexGui::calculateTextBounds(Paragraph text, int32_t scale, float maxWidth)
{
    float x = 0, y = 0;

    Style& style = *GTexGui->styleStack.back();
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

#define TTUL style.Tooltip.UnderlineSize

static bool writeText(Paragraph text, int32_t scale, TexGui::Math::fbox bounds, float& x, float& y, RenderData* renderData, bool checkHover = false, uint32_t flags = 0, TextDecl parameters = {});

inline static void drawChunk(TextSegment s, RenderData* renderData, TexGui::Math::fbox bounds, float& x, float& y, int32_t scale, uint32_t flags, bool& hovered, bool checkHover, uint32_t& placeholderIdx, TextDecl parameters)
    {
        static const auto underline = [](auto* renderData, float x, float y, float w, int32_t scale, uint32_t flags)
        {
            Style& style = *GTexGui->styleStack.back();
            if (flags & UNDERLINE)
            {
                fbox ul = fbox(fvec2(x - TTUL.x, y + scale / 2.0 - 2), fvec2(w + TTUL.x * 2, TTUL.y));
                renderData->addQuad(ul, fvec4(1,1,1,0.3));
            }
        };

        auto& io = inputFrame;

        switch (s.type)
        {
        case TextSegment::WORD:
            {
                float segwidth = s.width * scale;
                bumpline(x, y, segwidth, bounds, scale);

                fbox textbounds = fbox(x, y - scale / 2.0, segwidth, scale);

                underline(renderData, x, y, segwidth, scale, flags);

                renderData->addText(s.word.data, {x, y}, {1,1,1,1}, scale, CENTER_Y, bounds, s.word.len);

                // if (checkHover) renderData->addQuad(bounds, fvec4(1, 0, 0, 1));
                hovered |= checkHover && !hovered && textbounds.contains(io.cursorPos);
                x += segwidth;
            }
            break;

        case TextSegment::ICON: 
        case TextSegment::LAZY_ICON:
            {
                Style& style = *GTexGui->styleStack.back();
                Texture* icon = s.type == TextSegment::LAZY_ICON ? s.lazyIcon.func(s.lazyIcon.data) : s.icon;

                fvec2 tsize = fvec2(icon->bounds.width, icon->bounds.height) * _PX;
                bumpline(x, y, tsize.x, bounds, scale);

                fbox bounds = { x, y - tsize.y / 2, tsize.x, tsize.y };

                underline(renderData, x, y, tsize.x, scale, flags);

                //#TODO: pass in container here
                renderData->addTexture(bounds, icon, 0, _PX, 0, {0,0,8192,8192});

                // if (checkHover) renderData->addQuad(bounds, fvec4(1, 0, 0, 1));
                hovered |= checkHover && !hovered && fbox::margin(bounds, style.Tooltip.HoverPadding).contains(io.cursorPos);
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
                bool hoverTT = writeText(s.tooltip.base, scale, bounds, x, y, renderData, check, UNDERLINE);
                if (hoverTT) renderTooltip(s.tooltip.tooltip, renderData);
            }
            break;
        case TextSegment::PLACEHOLDER:
            {
                assert(placeholderIdx < parameters.count);
                bool blah = false;
                drawChunk(TextSegment::FromChunkFast(parameters.data[placeholderIdx]), renderData, bounds, x, y, scale, flags, blah, false, placeholderIdx, {});
                placeholderIdx++;
            }
            break;
        }
        
    }

static bool writeText(Paragraph text, int32_t scale, TexGui::Math::fbox bounds, float& x, float& y, RenderData* renderData, bool checkHover, uint32_t flags, TextDecl parameters)
{
    bool hovered = false;
    // Index into the amount of placeholders we've substituted
    uint32_t placeholderIdx = 0;

    for (int32_t i = 0; i < text.count; i++) 
    {
        drawChunk(text.data[i], renderData, bounds, x, y, scale, flags, hovered, checkHover, placeholderIdx, parameters);
    }
    return hovered;
}

Container RenderData::drawTooltip(fvec2 size)
{
    auto& io = inputFrame;
    Style& style = *GTexGui->styleStack.back();
    Texture* tex = style.Tooltip.Texture;
    
    fbox bounds = {
        io.cursorPos + style.Tooltip.MouseOffset, 
        size,
    };
    bounds = fbox::margin(bounds, style.Tooltip.Padding);

    return this->Base.Box(bounds.x, bounds.y, bounds.w, bounds.h);
}

static void renderTooltip(Paragraph text, RenderData* renderData)
{
    Style& style = *GTexGui->styleStack.back();
    int scale = style.Tooltip.TextScale;

    fvec2 textBounds = calculateTextBounds(text, scale, style.Tooltip.MaxWidth);

    Container s = renderData->drawTooltip(textBounds);

    float x = s.bounds.x, y = s.bounds.y + style.Tooltip.Padding.top;
    writeText(text, style.Tooltip.TextScale, s.bounds, x, y, s.renderData, false, 1);
}

void Container::Text(Paragraph text, int32_t scale, TextDecl parameters, TextStyle* style)
{
    if (style == nullptr)
        style = &GTexGui->styleStack.back()->Text;
    if (scale == 0) scale = style->Size;

    fbox textBounds = {
        { bounds.x, bounds.y },
        calculateTextBounds(text, scale, bounds.width)
    };

    textBounds = Arrange(this, textBounds);
    textBounds.y += + scale / 2.0f;

    writeText(text, scale, bounds, textBounds.x, textBounds.y, renderData, false, 0, parameters);
}

void Container::Text(const char* text, int32_t scale, TextDecl parameters, TextStyle* style)
{
    if (style == nullptr)
        style = &GTexGui->styleStack.back()->Text;
    if (scale == 0) scale = style->Size;
    auto ts = TextSegment::FromChunkFast(TextChunk(text));
    auto p = Paragraph(&ts, 1);

    fbox textBounds = {
        { bounds.x, bounds.y },
        calculateTextBounds(p, scale, bounds.width)
    };

    if (scale == 0) scale = style->Size;
    textBounds = Arrange(this, textBounds);
    textBounds.y += + scale / 2.0f;

    writeText(p, scale, bounds, textBounds.x, textBounds.y, renderData, false, 0, parameters);
}

/*
Container::ContainerArray Container::Row(std::initializer_list<float> widths, float height, uint32_t flags)
{
    return {};
}
*/

void Container::Row(Container* out, const float* widths, uint32_t n, float height, uint32_t flags)
{
    Style& style = *GTexGui->styleStack.back();
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
    float spacing = style.Row.Spacing;
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
    Style& style = *GTexGui->styleStack.back();
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
    float spacing = style.Column.Spacing;
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
        
        out[i] = withBounds({bounds.pos + TexGui::Math::vec2{x, y}, {width, height}});

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

Texture* IconSheet::getIcon(uint32_t x, uint32_t y)
{
    ibox bounds = { Math::ivec2(x, y + 1) * Math::ivec2(iw, ih), Math::ivec2(iw, ih) };
    return &m_custom_texs.emplace_back(glID, bounds, Math::ivec2{w, h}, 0, 0, 0, 0);
}

float TexGui::computeTextWidth(const char* text, size_t numchars)
{
    float total = 0;
    Style& style = *GTexGui->styleStack.back();
    for (size_t i = 0; i < numchars; i++)
    {
        total += style.Text.Font->codepointToGlyph[text[i]]->getAdvance();
    }
    return total;
}

void RenderData::addText(const char* text, Math::fvec2 pos, const Math::fvec4& col, int size, uint32_t flags, const Math::fbox& scissor,  int32_t len)
{
    size_t numchars = len == -1 ? strlen(text) : len;
    size_t numcopy = numchars;

    Style& style = *GTexGui->styleStack.back();
    Font* font = style.Text.Font;
    //#TODO: this sucks .what if a is a differnet size in another font
    const auto& a = font->codepointToGlyph['a'];

    if (flags & CENTER_Y)
    {
        double l, b, r, t;
        a->getQuadPlaneBounds(l, b, r, t);
        pos.y += (size - (t * size)) / 2;
    }

    if (flags & CENTER_X)
    {
        pos.x -= computeTextWidth(text, numchars) * size / 2.0;
    }

    float currx = pos.x;

    //const CharInfo& hyphen = m_char_map['-'];
    //#TODO: unicode
    for (int i = 0; i < numchars; i++)
    {
        const auto& info = font->codepointToGlyph[text[i]];
        int x, y, w, h;
        info->getBoxRect(x, y, w, h);

        double l, b, r, t;
        info->getQuadPlaneBounds(l, b, r, t);

        if (info->isWhitespace())
            w = info->getAdvance() * font->baseFontSize;

        float xpos = currx + l * size;
        float ypos = pos.y - t * size;
        float width = w * size / float(font->baseFontSize);
        float height = h * size / float(font->baseFontSize);

        uint32_t idx = vertices.size();
        vertices.insert(vertices.end(), {
                Vertex{
                    .pos = {xpos, ypos + height}, .uv = Math::fvec2(x, y),
                },
                Vertex{
                    .pos = {xpos + width, ypos + height}, .uv = {float(x + w), float(y)},
                },
                Vertex{
                    .pos = {xpos, ypos}, .uv = {float(x), float(y + h)},
                },
                Vertex{
                    .pos = {xpos + width, ypos}, .uv = {float(x + w), float(y + h)},
                },
            }
        );
        indices.insert(indices.end(), {idx, idx+1, idx+2, idx+1, idx+2, idx+3});
        currx += info->getAdvance() * size;
    }

    Math::ivec2 framebufferSize = GTexGui->framebufferSize;
    commands.emplace_back(Command{
        .indexCount = uint32_t(6 * numchars), 
        .textureIndex = font->textureIndex,
        .scale = {2.f / float(framebufferSize.x), 2.f / float(framebufferSize.y)},
        .translate = {-1.f, -1.f},
        .pxRange = fmax(float(size) / float(font->baseFontSize) * font->msdfPxRange, 1.f),
        .uvScale = {1.f / float(font->atlasWidth), 1.f /float(font->atlasHeight)},
        .scissor = scissor,
    });
}

int RenderData::addTextWithCursor(const char* text, Math::fvec2 pos, const Math::fvec4& col, int size, uint32_t flags, const Math::fbox& scissor, TextInputState& textInput)
{
    auto& io = inputFrame;
    size_t numchars = strlen(text);
    size_t numcopy = numchars;

    Style& style = *GTexGui->styleStack.back();
    Font* font = style.Text.Font;
    //#TODO: this sucks .what if a is a differnet size in another font
    const auto& a = font->codepointToGlyph['a'];

    if (flags & CENTER_Y)
    {
        double l, b, r, t;
        a->getQuadPlaneBounds(l, b, r, t);
        pos.y += (size - (t * size)) / 2;
    }

    if (flags & CENTER_X)
    {
        pos.x -= computeTextWidth(text, numchars) * size / 2.0;
    }

    float currx = pos.x;

    Math::ivec2 framebufferSize = GTexGui->framebufferSize;
    //const CharInfo& hyphen = m_char_map['-'];
    //#TODO: unicode
    float cursorPosLocation = -1;
    int mouseIndex = -1;
    int& textCursorPos = textInput.textCursorPos;
    for (int i = 0; i < numchars; i++)
    {
        const auto& info = font->codepointToGlyph[text[i]];

        fvec4 color = col;

        int x, y, w, h;
        info->getBoxRect(x, y, w, h);

        double l, b, r, t;
        info->getQuadPlaneBounds(l, b, r, t);

        if (info->isWhitespace())
            w = info->getAdvance() * font->baseFontSize;

        float xpos = currx + l * size;
        float ypos = pos.y - t * size;
        float width = w * size / float(font->baseFontSize);
        float height = h * size / float(font->baseFontSize);

        float advance = info->getAdvance() * size;

        if (textInput.state & STATE_ACTIVE && io.lmb == KEY_Press)
        {
            textInput.selection[0] = -1;
            textInput.selection[1] = -1;
            if (io.cursorPos.x >= currx && io.cursorPos.x <= currx + advance * 0.5)
                textCursorPos = i;
            else if (io.cursorPos.x > currx + advance * 0.5)
                textCursorPos = i + 1;
        }

        if (textInput.state & STATE_ACTIVE && io.lmb == KEY_Held)
        {
            if (io.cursorPos.x >= currx && io.cursorPos.x <= currx + advance * 0.5)
            {
                textInput.selection[i >= textCursorPos ? 0 : 1] = textCursorPos;
                textInput.selection[i >= textCursorPos ? 1 : 0] = i;
            }
            else if (io.cursorPos.x > currx + advance * 0.5)
            {
                textInput.selection[i >= textCursorPos ? 0 : 1] = textCursorPos;
                textInput.selection[i >= textCursorPos ? 1 : 0] = i + 1;
            }
        }

        if (i >= textInput.selection[0] && i < textInput.selection[1])
        {
            addQuad({currx, pos.y + size / 4.f - size, advance, float(size)}, style.TextInput.SelectColor);
            color = {1.f - col.r, 1.f - col.g, 1.f - col.b, col.a};
        }

        uint32_t idx = vertices.size();
        vertices.insert(vertices.end(), {
                Vertex{
                    .pos = {xpos, ypos + height}, .uv = Math::fvec2(x, y), .col = color
                },
                Vertex{
                    .pos = {xpos + width, ypos + height}, .uv = {float(x + w), float(y)}, .col = color
                },
                Vertex{
                    .pos = {xpos, ypos}, .uv = {float(x), float(y + h)}, .col = color
                },
                Vertex{
                    .pos = {xpos + width, ypos}, .uv = {float(x + w), float(y + h)}, .col = color
                },
            }
        );
        indices.insert(indices.end(), {idx, idx+1, idx+2, idx+1, idx+2, idx+3});
        if (textCursorPos == i)
            cursorPosLocation = currx;
        currx += advance;

        commands.emplace_back(Command{
            .indexCount = uint32_t(6), 
            .textureIndex = font->textureIndex,
            .scale = {2.f / float(framebufferSize.x), 2.f / float(framebufferSize.y)},
            .translate = {-1.f, -1.f},
            .pxRange = fmax(float(size) / float(font->baseFontSize) * font->msdfPxRange, 1.f),
            .uvScale = {1.f / float(font->atlasWidth), 1.f /float(font->atlasHeight)},
            .scissor = scissor,
        });
    }
    
    if (textCursorPos == numchars)
        cursorPosLocation = currx;

    if (textInput.state & STATE_ACTIVE && cursorPosLocation != -1)
    {
        pos.y += size / 4.f;
        uint32_t idx = vertices.size();
        vertices.insert(vertices.end(), {
            Vertex{
                .pos = {cursorPosLocation, pos.y - size},
            },
            Vertex{
                .pos = {cursorPosLocation + 2, pos.y - size},
            },
            Vertex{
                .pos = {cursorPosLocation, pos.y},
            },
            Vertex{
                .pos = {cursorPosLocation + 2, pos.y},
            },
        });
        indices.insert(indices.end(), {idx, idx+1, idx+2, idx+1, idx+2, idx+3});
        commands.emplace_back(Command{
            .indexCount = 6, 
            .textureIndex = 0,
            .scale = {2.f / float(framebufferSize.x), 2.f / float(framebufferSize.y)},
            .translate = {-1.f, -1.f},
            .scissor = scissor,
        });

    }

    return -1;
}

static inline uint32_t getTextureIndexFromState(Texture* e, int state)
{
    return state & STATE_PRESS && e->press != -1 ? e->press :
        state & STATE_HOVER && e->hover != -1 ? e->hover :
        state & STATE_ACTIVE && e->active != -1 ? e->active : e->id;
}

void RenderData::addTexture(fbox rect, Texture* e, int state, int pixel_size, uint32_t flags, const fbox& scissor)
{
    if (!e || e->id == -1 || !scissor.isValid()) return;

    uint32_t tex = getTextureIndexFromState(e, state);

    Math::ivec2 framebufferSize = GTexGui->framebufferSize;

    if (!(flags & SLICE_9))
    {
        Math::fbox texBounds = e->bounds;

        uint32_t idx = vertices.size();

        vertices.insert(vertices.end(), {
                Vertex{
                    .pos = {rect.x, rect.y}, .uv = {texBounds.x, texBounds.y},
                },
                Vertex{
                    .pos = {rect.x + rect.w, rect.y}, .uv = {texBounds.x + texBounds.w, texBounds.y},
                },
                Vertex{
                    .pos = {rect.x, rect.y + rect.h}, .uv = {texBounds.x, texBounds.y + texBounds.h},
                },
                Vertex{
                    .pos = {rect.x + rect.w, rect.y + rect.h}, .uv = {texBounds.x + texBounds.w, texBounds.y + texBounds.h},
                },
            }
        );
        indices.insert(indices.end(), {idx, idx+1, idx+2, idx+1, idx+2, idx+3});

        commands.emplace_back(Command{
            .indexCount = 6,
            .textureIndex = tex,
            .scale = {2.f / float(framebufferSize.x), 2.f / float(framebufferSize.y)},
            .translate = {-1.f, -1.f},
            .uvScale = {1.f / float(e->size.x), 1.f / float(e->size.y)},
            .scissor = scissor,
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

            uint32_t idx = vertices.size();
            vertices.insert(vertices.end(), {
                    Vertex{
                        .pos = {slice.x, slice.y}, .uv = {texBounds.x, texBounds.y},
                    },
                    Vertex{
                        .pos = {slice.x + slice.w, slice.y}, .uv = {texBounds.x + texBounds.w, texBounds.y},
                    },
                    Vertex{
                        .pos = {slice.x, slice.y + slice.h}, .uv = {texBounds.x, texBounds.y + texBounds.h},
                    },
                    Vertex{
                        .pos = {slice.x + slice.w, slice.y + slice.h}, .uv = {texBounds.x + texBounds.w, texBounds.y + texBounds.h},
                    },
                }
            );

            indices.insert(indices.end(), {idx, idx+1, idx+2, idx+1, idx+2, idx+3});
        }
    }

    commands.emplace_back(Command{
        .indexCount = uint32_t(6 * (flags & SLICE_3_HORIZONTAL ? 3 : 1) * (flags & SLICE_3_VERTICAL ? 3 : 1)),
        .textureIndex = tex,
        .scale = {2.f / float(framebufferSize.x), 2.f / float(framebufferSize.y)},
        .translate = {-1.f, -1.f},
        .uvScale = {1.f / float(e->size.x), 1.f / float(e->size.y)},
        .scissor = scissor,
    });
}

void RenderData::addQuad(Math::fbox rect, const Math::fvec4& col)
{
    Math::ivec2 framebufferSize = GTexGui->framebufferSize;

    uint32_t idx = vertices.size();
    vertices.insert(vertices.end(), {
            Vertex{
                .pos = {rect.x, rect.y}, .uv = {0,0}, .col = col,
            },
            Vertex{
                .pos = {rect.x + rect.w, rect.y}, .uv = {0, 0}, .col = col,
            },
            Vertex{
                .pos = {rect.x, rect.y + rect.h}, .uv = {0, 0}, .col = col,
            },
            Vertex{
                .pos = {rect.x + rect.w, rect.y + rect.h}, .uv = {0, 0}, .col = col,
            },
        }
    );

    indices.insert(indices.end(), {idx, idx+1, idx+2, idx+1, idx+2, idx+3});
    commands.emplace_back(Command{
        .indexCount = 6,
        .textureIndex = 0,
        .scale = {2.f / float(framebufferSize.x), 2.f / float(framebufferSize.y)},
        .translate = {-1.f, -1.f},
    });
}

// from imgui
#define IM_NORMALIZE2F_OVER_ZERO(VX,VY)     { float d2 = VX*VX + VY*VY; if (d2 > 0.0f) { float inv_len = 1.0/sqrt(d2); VX *= inv_len; VY *= inv_len; } } (void)0 

void RenderData::addLine(float x1, float y1, float x2, float y2, const Math::fvec4& col, float lineWidth)
{
    Math::ivec2 framebufferSize = GTexGui->framebufferSize;

    float dx = x2 - x1;
    float dy = y2 - y1;

    IM_NORMALIZE2F_OVER_ZERO(dx, dy);

    dx *= (lineWidth * 0.5f);
    dy *= (lineWidth * 0.5f);

    uint32_t idx = vertices.size();
    vertices.insert(vertices.end(), {
            Vertex{
                .pos = {x1 + dy, y1 - dx}, .uv = {0,0}, .col = col,
            },
            Vertex{
                .pos = {x2 + dy, y2 - dx}, .uv = {0,0}, .col = col, 
            },
            Vertex{
                .pos = {x2 - dy, y2 + dx}, .uv = {0,0}, .col = col, 
            },
            Vertex{
                .pos = {x1 - dy, y1 + dx}, .uv = {0,0}, .col = col, 
            },
        }
    );

    indices.insert(indices.end(), {idx, idx+1, idx+2, idx, idx+2, idx+3});

    commands.emplace_back(Command{
        .indexCount = 6,
        .textureIndex = 0,
        .scale = {2.f / float(framebufferSize.x), 2.f / float(framebufferSize.y)},
        .translate = {-1.f, -1.f},
    });
}
