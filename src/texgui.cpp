#include "texgui.h"
#include "context.hpp"
#include "input.hpp"
#include "types.h"
#include <cassert>
#include <cstring>
#include <cmath>
#include <mutex>
#include <list>
#include <queue>
#include <filesystem>
#include <vulkan/vulkan_core.h>
#include "stb_image.h"
#include "msdf-atlas-gen/msdf-atlas-gen.h"
#include "texgui_internal.hpp"

using namespace TexGui;

void TexGui::init()
{
    GTexGui = new TexGuiContext();
}

void _setRenderCtx(NoApiContext* ptr)
{
    GTexGui->renderCtx = ptr;
}

TexGui::Math::ivec2 _getScreenSize()
{
    return GTexGui->framebufferSize;
}

void TexGui::newFrame()
{
    GTexGui->renderCtx->newFrame();
}

RenderData* TexGui::newRenderData()
{
    return new RenderData();
}

#ifndef M_PI
#define M_PI  3.14159265358979323846264  // from CRC
#endif

using namespace msdf_atlas;
std::vector<GlyphGeometry> glyphs;
std::unordered_map<uint32_t, uint32_t> m_char_map;
void TexGui::loadFont(const char* font_path)
{
    Texture output;
    int width = 0, height = 0;
    bool success = false;
    font_px = 100.f;
    // Initialize instance of FreeType library
    if (msdfgen::FreetypeHandle *ft = msdfgen::initializeFreetype()) {
        // Load font file
        if (msdfgen::FontHandle *font = msdfgen::loadFont(ft, font_path)) {
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
            packer.setScale(font_px);
            // setPixelRange or setUnitRange
            packer.setPixelRange(TexGui::Defaults::Font::MsdfPxRange);
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
            uint32_t texID = GTexGui->renderCtx->createTexture((void*)rgba, width, height);
            GTexGui->renderCtx->setPxRange(TexGui::Defaults::Font::MsdfPxRange);
            GTexGui->renderCtx->setFontAtlas(texID, width, height);

            delete []rgba;

            // Cleanup
            msdfgen::destroyFont(font);
        }
        msdfgen::deinitializeFreetype(ft);
    }

    int i = 0;
    for (auto& glyph : glyphs)
    {
        m_char_map[glyph.getCodepoint()] = i;
        i++;
    }
}

std::unordered_map<std::string, TexGui::Texture> m_tex_map;
void TexGui::loadTextures(const char* dir)
{
    auto ctx = GTexGui->renderCtx;
    std::vector<std::filesystem::directory_entry> files;
    for (const auto& f : std::filesystem::recursive_directory_iterator(dir))
    {
        files.push_back(f);
    }

    for (auto& f : files)
    {
        if (!f.is_regular_file())
        {
            continue;
        }

        std::string pstr = f.path().string();
        std::string fstr = f.path().filename().string();
        fstr.erase(fstr.begin() + fstr.find('.'), fstr.end());

        if (!pstr.ends_with(".png"))
        {
            printf("Unexpected file in sprites folder: %s", pstr.c_str());
            continue;
        }

        int width, height, channels;
        unsigned char* pixels = stbi_load(pstr.c_str(), &width, &height, &channels, 4);
        if (pixels == nullptr)
        {
            printf("Failed to load file: %s\n", pstr.c_str());
            continue;
        }
        
        if (m_tex_map.contains(fstr))
        {
            if (m_tex_map[fstr].bounds.w != width || m_tex_map[fstr].bounds.h != height)
            {
                printf("Texture variant dimension mismatch: %s", pstr.c_str());
                continue;
            }
        }
        else
        {
            Texture& t = m_tex_map[fstr];
            t.bounds.x = 0; 
            t.bounds.y = height; 
            t.bounds.w = width; 
            t.bounds.h = height; 
            
            t.size.x = width;
            t.size.y = height;

            t.top = float(height)/3.f;
            t.right = float(width)/3.f;
            t.bottom = float(height)/3.f;
            t.left = float(width)/3.f;

        }

        Texture& t = m_tex_map[fstr];

        if (pstr.ends_with(".hover.png"))
        {
            t.hover = ctx->createTexture(pixels, width, height);
        }
        else if (pstr.ends_with(".press.png"))
        {
            t.press = ctx->createTexture(pixels, width, height);
        }
        else if (pstr.ends_with(".active.png"))
        {
            t.active = ctx->createTexture(pixels, width, height);
        }
        else
            t.id = ctx->createTexture(pixels, width, height);

        stbi_image_free(pixels);
    }

}

IconSheet TexGui::loadIcons(const char* dir, int32_t iconWidth, int32_t iconHeight)
{
    //return GTexGui->renderCtx->loadIcons(dir, iconWidth, iconHeight);
    int width, height, channels;
    unsigned char* pixels = stbi_load(dir, &width, &height, &channels, 4);
    assert(pixels != nullptr);
    
    uint32_t texID = GTexGui->renderCtx->createTexture(pixels, width, height);
    stbi_image_free(pixels);

    return { texID, iconWidth, iconHeight, width, height };
}

extern std::unordered_map<std::string, TexGui::Texture> m_tex_map;

// We just need pointer stability, we aren't gonna be iterating it so using list :P
// This is a heap alloc per entry which is a bit of a pain so should change since we barely use heap at all otherwise #TODO
static std::list<Texture> m_custom_texs;

Texture* TexGui::texByName(const char* name)
{
    if (!m_tex_map.contains(name)) return nullptr;
    return &m_tex_map[name];
}

Texture* TexGui::customTexture(unsigned int texID, Math::ibox pixelBounds)
{
    float xth = pixelBounds.w / 3.f;
    float yth = pixelBounds.h / 3.f;
    auto size = GTexGui->renderCtx->getTextureSize(texID);
    return &m_custom_texs.emplace_back(texID, pixelBounds, size, yth, xth, yth, xth);
}

TexGui::Math::ivec2 TexGui::getTexSize(Texture* tex)
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

// There are two input states, the one in GTexGui and inputFrame.
// updateInput() is called in TexGui::clear().
// GTexGui->io is submittetd to inputFrame, then cleared.
inline static void updateInput()
{
    auto& io = GTexGui->io;
    GTexGui->editingText.store(TexGui_editingText);
    TexGui_editingText = false;

    std::lock_guard<std::mutex> lock(TGInputLock);

    //we don't clear this one
    inputFrame.mods = io.mods;

    inputFrame.cursorPos = io.cursorPos;
    inputFrame.scroll = io.scroll;
    inputFrame.mouseRelativeMotion = io.mouseRelativeMotion;

    io.mouseRelativeMotion = {0, 0};

    TexGui::CapturingMouse = false;
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
    updateInput();
    for (auto& win : GTexGui->windows)
    {
        win.second.prevVisible = win.second.visible;
        win.second.visible = false;
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

using namespace TexGui::Math;

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

#define _PX Defaults::PixelSize
extern std::unordered_map<std::string, Texture> m_tex_map;
Container Container::Window(const char* name, float xpos, float ypos, float width, float height, uint32_t flags, Texture* texture)
{
    auto& io = inputFrame;
    if (!GTexGui->windows.contains(name))
    {
        for (auto& entry : GTexGui->windows)
        {
            entry.second.order++;
        }

        GTexGui->windows.insert({name, WindowState{
            .box = {xpos, ypos, width, height},
        }});
    }

    static Texture* wintex = &m_tex_map[Defaults::Window::Texture];
    if (!texture) texture = wintex;
    WindowState& wstate = GTexGui->windows[name];

    wstate.visible = true;

    if (flags & BACK)
        wstate.order = INT_MAX;
    if (flags & FRONT)
        wstate.order = -1;

    if (flags & CAPTURE_INPUT && wstate.box.contains(io.cursorPos)) CapturingMouse = true;

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

    if (flags & LOCKED)
    {
        wstate.box = {xpos, ypos, width, height};
        wstate.box = AlignBox(Base.bounds, wstate.box, flags);
    }
    
    if (flags & LOCKED || io.lmb != KEY_Held || !(wstate.state & STATE_ACTIVE))
    {
        wstate.moving = false;
        wstate.resizing = false;
    }
    else if (fbox(wstate.box.x, wstate.box.y, wstate.box.width, texture->top * _PX).contains(io.cursorPos) && io.lmb == KEY_Held)
        wstate.moving = true;
    else if (flags & RESIZABLE && fbox(wstate.box.x + wstate.box.width - texture->right * _PX,
             wstate.box.y + wstate.box.height - texture->bottom * _PX,
             texture->right * _PX, texture->bottom * _PX).contains(io.cursorPos) && io.lmb == KEY_Held)
        wstate.resizing = true;

    if (wstate.moving)
        wstate.box.pos += io.mouseRelativeMotion;
    else if (wstate.resizing)
        wstate.box.size += io.mouseRelativeMotion;

    fvec4 padding = Defaults::Window::Padding;
    padding.top = texture->top * _PX;

        fbox internal = fbox::pad(wstate.box, padding);

    buttonStates = &wstate.buttonStates;

    renderData->ordered = true;
    parentState = wstate.state;

    Container child = withBounds(internal);
    child.renderData = &renderData->children.emplace_back();

    //lowest order will be rendered last.
    child.renderData->priority = -wstate.order;
    child.renderData->drawTexture(wstate.box, texture, wstate.state, _PX, SLICE_9, scissorBox);

    if (!(flags & HIDE_TITLE)) 
        child.renderData->drawText(name, {wstate.box.x + padding.left, wstate.box.y + texture->top * _PX / 2},
                 Defaults::Font::Color, Defaults::Font::Size, CENTER_Y);


    return child;
}

bool Container::Button(const char* text, Texture* texture, Container* out)
{
    if (!buttonStates->contains(text))
    {
        buttonStates->insert({text, 0});
    }

    uint32_t& state = buttonStates->at(text);
    getBoxState(state, bounds, parentState);

    if (!(parentState & STATE_ACTIVE))
        setBit(state, STATE_ACTIVE, 0);
    if (!(parentState & STATE_HOVER))
        setBit(state, STATE_HOVER, 0);
    static Texture* defaultTex = &m_tex_map[Defaults::Button::Texture];
    auto* tex = texture ? texture : defaultTex;

    renderData->drawTexture(bounds, tex, state, _PX, SLICE_9, scissorBox);

    TexGui::Math::vec2 pos = state & STATE_PRESS 
                       ? bounds.pos + Defaults::Button::POffset
                       : bounds.pos;
    fbox innerBounds = {pos, bounds.size};

    if (out)
        *out = withBounds(innerBounds);
    else
    {
        innerBounds.pos += bounds.size / 2;
        renderData->drawText(text, innerBounds, Defaults::Font::Color, Defaults::Font::Size, CENTER_X | CENTER_Y);
    }

    auto& io = inputFrame;

    bool hovered = scissorBox.contains(io.cursorPos) 
                && bounds.contains(io.cursorPos);

    return state & STATE_ACTIVE && io.lmb == KEY_Release && hovered ? true : false;
}

Container Container::Box(float xpos, float ypos, float width, float height, Texture* texture)
{
    if (width <= 1)
        width = width == 0 ? bounds.width : bounds.width * width;
    if (height <= 1)
        height = height == 0 ? bounds.height : bounds.height * height;

    fbox boxBounds(bounds.x + xpos, bounds.y + ypos, width, height);
    if (texture != nullptr)
    {
        renderData->drawTexture(boxBounds, texture, 0, 2, SLICE_9, scissorBox);
    }

    fbox internal = fbox::pad(boxBounds, Defaults::Box::Padding);
    return withBounds(internal);
}

void Container::CheckBox(bool* val)
{
    static Texture* texture = &m_tex_map[Defaults::CheckBox::Texture];

    auto& io = inputFrame;
    if (io.lmb == KEY_Release && bounds.contains(io.cursorPos)) *val = !*val;

    if (texture == nullptr) return;
    renderData->drawTexture(bounds, texture, *val ? STATE_ACTIVE : 0, 2, SLICE_9, scissorBox);
}

void Container::RadioButton(uint32_t* selected, uint32_t id)
{
    static Texture* texture = &m_tex_map[Defaults::RadioButton::Texture];

    auto& io = inputFrame;
    if (io.lmb == KEY_Release && bounds.contains(io.cursorPos)) *selected = id;

    if (texture == nullptr) return;
    renderData->drawTexture(bounds, texture, *selected == id ? STATE_ACTIVE : 0, 2, SLICE_9, scissorBox);

}

Container Container::ScrollPanel(const char* name, Texture* texture, Texture* bartex)
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

    renderData->drawTexture(bounds, texture, 0, _PX, 0, bounds);
    if (bartex)
        renderData->drawTexture(bar, bartex, 0, 2, SLICE_9, bounds);
    else
        renderData->drawQuad(bar, {1,1,1,1});

    sp.bounds.y += spstate.contentPos.y;

    return sp;
}

void Container::Image(Texture* texture, int scale)
{
    Math::ivec2 tsize = Math::ivec2(texture->bounds.width, texture->bounds.height) * scale;

    fbox sized = fbox(bounds.pos, fvec2(tsize));
    fbox arranged = Arrange(this, sized);

    renderData->drawTexture(arranged, texture, STATE_NONE, scale, 0, scissorBox);
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
        
        static Texture* tex = &m_tex_map[Defaults::ListItem::Texture];

        if (!listItem->scissorBox.contains(bounds))
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

        listItem->renderData->drawTexture(bounds, tex, state, _PX, SLICE_9, listItem->scissorBox);

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
    renderData->drawQuad(line, fvec4(1,1,1,0.5), 1);
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

void renderTextInput(RenderData* renderData, const char* name, fbox bounds, fbox scissor, TextInputState& tstate, const char* text, int32_t len)
{
    static Texture* inputtex = &m_tex_map[Defaults::TextInput::Texture];
    static Texture* textcursor = &m_tex_map[Defaults::TextInput::TextCursor];

    renderData->drawTexture(bounds, inputtex, tstate.state, _PX, SLICE_9, scissor);
    float offsetx = 0;
    fvec4 padding = Defaults::TextInput::Padding;
    fvec4 color = Defaults::Font::Color;

    float startx = bounds.x + offsetx + padding.left;
    float starty = bounds.y + bounds.height / 2;

    int cursor_pos = tstate.cursor_pos;

    //#TODO: size
    int size = Defaults::Font::Size;
    auto characters = renderData->drawText(
        !getBit(tstate.state, STATE_ACTIVE) && len == 0
        ? name : text,
        {startx, starty},
        Defaults::Font::Color,
        size,
        CENTER_Y
    );

    if (!(tstate.state & STATE_ACTIVE) || cursor_pos > len) return;

    TexGui::Math::fbox textCursorQuad;

    textCursorQuad.width = textcursor->bounds.width * _PX;
    textCursorQuad.height = float(size);
    if (cursor_pos == 0)
    {
        textCursorQuad.x = startx - textcursor->bounds.width;
        textCursorQuad.y = starty - Defaults::Font::Size / 2.f;
    }
    else
    {
        auto lastChar = characters[cursor_pos - 1].ch.rect;
        textCursorQuad.x = lastChar.x - textcursor->bounds.width + lastChar.width;
        //#TODO starty is wrong if we do multiline textinputs
        textCursorQuad.y = starty - Defaults::Font::Size / 2.f;
    }
    renderData->drawTexture(textCursorQuad, textcursor, tstate.state, _PX, SLICE_9, bounds);
}

void Container::TextInput(const char* name, std::string& buf, CharacterFilter filter)
{
    return;
    /*
    if (!GTexGui->textInputs.contains(name))
    {
        GTexGui->textInputs.insert({name, {}});
    }
    
    auto& tstate = GTexGui->textInputs[name];
    etBoxState(tstate.state, bounds);

    //#TODO: variable cursor pos
    tstate.cursor_pos = buf.size();

    if (tstate.state & STATE_ACTIVE)
        TexGui_editingText = true;
    
    char* textsrc = "\0";
    if (textInputUpdate(tstate, &textsrc, filter))
    {
        if (buf.size() > 0)
        {
            buf.pop_back();
            tstate.cursor_pos--;
        }
    }
    buf += textsrc;
    tstate.cursor_pos += strlen(textsrc);
    
    renderTextInput(renderData, name, bounds, scissorBox, tstate, buf.c_str(), buf.size(), tstate.cursor_pos);
    */
}

void Container::TextInput(const char* name, char* buf, uint32_t bufsize, CharacterFilter filter)
{
    auto& io = inputFrame;
    
    if (!GTexGui->textInputs.contains(name))
    {
        GTexGui->textInputs.insert({name, {}});
    }
    
    auto& tstate = GTexGui->textInputs[name];
    getBoxState(tstate.state, bounds, parentState);

    if (tstate.state & STATE_ACTIVE)
        TexGui_editingText = true;

    int32_t tlen = strlen(buf);
    
    //left takes priority
    bool left = io.keyStates[TexGuiKey_LeftArrow] & (KEY_Press | KEY_Repeat);
    bool right = io.keyStates[TexGuiKey_RightArrow] & (KEY_Press | KEY_Repeat);
    int dir = left ? -1 : right ? 1 : 0;
    char* c;

    if (dir != 0)
    {
        tstate.cursor_pos = clamp(tstate.cursor_pos + dir, 0, tlen);

        if (io.mods &
        #ifdef __APPLE__
            TexGuiMod_Alt
        #else
            TexGuiMod_Ctrl
        #endif
            )
        {
            while (tstate.cursor_pos > 0 && tstate.cursor_pos < tlen)
            {
                const char& curr = buf[tstate.cursor_pos];
                const char& next = buf[tstate.cursor_pos + dir];
                //#TODO: more delimiters
                if (curr != ' ' && curr != '.' && (next == ' ' || next == '.')) 
                {
                    if (dir > 0) tstate.cursor_pos++;
                    break;
                }

                tstate.cursor_pos += dir;
            }
        }
    }

    if (textInputUpdate(tstate, &c, filter))
    {
        //#TODO: this is just backspace, we want more deletion later
        if (tstate.cursor_pos > 0)
        {
            if (tstate.cursor_pos != tlen)
            {
                char tail[tlen - tstate.cursor_pos + 1]; // + 1 to include the null
                strcpy(tail, buf + tstate.cursor_pos);
                buf[tstate.cursor_pos - 1] = '\0';
                strcat(buf, tail);
            }
            else
                buf[tstate.cursor_pos - 1] = '\0';

            tstate.cursor_pos--;
            tlen--;
        }
    }
    else if (c && tlen < bufsize - 1)
    {
        if (tstate.cursor_pos != tlen)
        {
            char tail[tlen - tstate.cursor_pos + 1];
            strcpy(tail, buf + tstate.cursor_pos);
            strcpy(buf + tstate.cursor_pos, io.text);
            strcat(buf, tail);
        }
        else
            strcpy(buf + tstate.cursor_pos, io.text);

        tstate.cursor_pos += strlen(io.text);
        tlen += strlen(io.text);
    }
    renderTextInput(renderData, name, bounds, scissorBox, tstate, buf, tlen);
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

static bool writeText(Paragraph text, int32_t scale, TexGui::Math::fbox bounds, float& x, float& y, RenderData* renderData, bool checkHover = false, int32_t zLayer = 0, uint32_t flags = 0, TextDecl parameters = {});

inline static void drawChunk(TextSegment s, RenderData* renderData, TexGui::Math::fbox bounds, float& x, float& y, int32_t scale, uint32_t flags, int32_t zLayer, bool& hovered, bool checkHover, uint32_t& placeholderIdx, TextDecl parameters)
    {
        static const auto underline = [](auto* renderData, float x, float y, float w, int32_t scale, uint32_t flags, int32_t zLayer)
        {
            if (flags & UNDERLINE)
            {
                fbox ul = fbox(fvec2(x - TTUL.x, y + scale / 2.0 - 2), fvec2(w + TTUL.x * 2, TTUL.y));
                renderData->drawQuad(ul, fvec4(1,1,1,0.3), zLayer);
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

                underline(renderData, x, y, segwidth, scale, flags, zLayer);

                renderData->drawText(s.word.data, {x, y}, {1,1,1,1}, scale, CENTER_Y, s.word.len);

                // if (checkHover) renderData->drawQuad(bounds, fvec4(1, 0, 0, 1));
                hovered |= checkHover && !hovered && bounds.contains(io.cursorPos);
                x += segwidth;
            }
            break;

        case TextSegment::ICON: 
        case TextSegment::LAZY_ICON:
            {
                Texture* icon = s.type == TextSegment::LAZY_ICON ? s.lazyIcon.func(s.lazyIcon.data) : s.icon;

                fvec2 tsize = fvec2(icon->bounds.width, icon->bounds.height) * _PX;
                bumpline(x, y, tsize.x, bounds, scale);

                fbox bounds = { x, y - tsize.y / 2, tsize.x, tsize.y };

                underline(renderData, x, y, tsize.x, scale, flags, zLayer);

                //#TODO: pass in container here
                renderData->drawTexture(bounds, icon, 0, _PX, 0, {0,0,8192,8192}, zLayer);

                // if (checkHover) renderData->drawQuad(bounds, fvec4(1, 0, 0, 1));
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
                bool hoverTT = writeText(s.tooltip.base, scale, bounds, x, y, renderData, check, zLayer, UNDERLINE);
                if (hoverTT) renderTooltip(s.tooltip.tooltip, renderData);
            }
            break;
        case TextSegment::PLACEHOLDER:
            {
                assert(placeholderIdx < parameters.count);
                bool blah = false;
                drawChunk(TextSegment::FromChunkFast(parameters.data[placeholderIdx]), renderData, bounds, x, y, scale, flags, zLayer, blah, false, placeholderIdx, {});
                placeholderIdx++;
            }
            break;
        }
        
    }

static bool writeText(Paragraph text, int32_t scale, TexGui::Math::fbox bounds, float& x, float& y, RenderData* renderData, bool checkHover, int32_t zLayer, uint32_t flags, TextDecl parameters)
{
    bool hovered = false;
    // Index into the amount of placeholders we've substituted
    uint32_t placeholderIdx = 0;

    for (int32_t i = 0; i < text.count; i++) 
    {
        drawChunk(text.data[i], renderData, bounds, x, y, scale, flags, zLayer, hovered, checkHover, placeholderIdx, parameters);
    }
    return hovered;
}

Container RenderData::drawTooltip(fvec2 size)
{
    auto& io = inputFrame;
    static Texture* tex = &m_tex_map[Defaults::Tooltip::Texture];
    
    fbox bounds = {
        io.cursorPos + Defaults::Tooltip::MouseOffset, 
        size,
    };
    bounds = fbox::margin(bounds, Defaults::Tooltip::Padding);

    return this->Base.Box(bounds.x, bounds.y, bounds.w, bounds.h, tex);
}

static void renderTooltip(Paragraph text, RenderData* renderData)
{
    int scale = Defaults::Tooltip::TextScale;

    fvec2 textBounds = calculateTextBounds(text, scale, Defaults::Tooltip::MaxWidth);

    Container s = renderData->drawTooltip(textBounds);

    float x = s.bounds.x, y = s.bounds.y;
    writeText(text, Defaults::Tooltip::TextScale, s.bounds, x, y, renderData, false, 1);
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

    writeText(text, scale, bounds, textBounds.x, textBounds.y, renderData, false, 0, 0, parameters);
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

    writeText(p, scale, bounds, textBounds.x, textBounds.y, renderData, false, 0, 0, parameters);
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

#include "msdf-atlas-gen/msdf-atlas-gen.h"
float TexGui::computeTextWidth(const char* text, size_t numchars)
{
    float total = 0;
    for (size_t i = 0; i < numchars; i++)
    {
        total += glyphs[m_char_map[text[i]]].getAdvance();
    }
    return total;
}

std::span<RenderData::Object> RenderData::drawText(const char* text, Math::fvec2 pos, const Math::fvec4& col, int size, uint32_t flags, int32_t len)
{
    auto& objects =  this->objects;
    auto& commands = this->commands;

    size_t numchars = len == -1 ? strlen(text) : len;
    size_t numcopy = numchars;

    const auto& a = glyphs[m_char_map['a']];
    if (flags & CENTER_Y)
    {
        double l, b, r, t;
        a.getQuadPlaneBounds(l, b, r, t);
        pos.y += (size - (t * size)) / 2;
    }

    if (flags & CENTER_X)
    {
        pos.x -= computeTextWidth(text, numchars) * size / 2.0;
    }

    float currx = pos.x;

    //const CharInfo& hyphen = m_char_map['-'];
    //#TODO: unicode
    for (int idx = 0; idx < numchars; idx++)
    {
        const auto& info = glyphs[m_char_map[text[idx]]];
        int x, y, w, h;
        info.getBoxRect(x, y, w, h);
        double l, b, r, t;
        info.getQuadPlaneBounds(l, b, r, t);

        if (info.isWhitespace())
        {
            w = info.getAdvance() * font_px;
        }
        objects.emplace_back(
            Character{
                .rect = fbox(
                    currx + l * size,
                    pos.y - b * size,
                    w * size / font_px,
                    h * size / font_px
                ),
                .texBounds = fbox(x, y, w, h),
                .colour = col,
                .size = size
            }
        );
        currx += info.getAdvance() * size;
    }

    commands.push_back({Command::CHARACTER, uint32_t(numchars), flags});
    return std::span<RenderData::Object>(objects.end() - numchars, numchars);
}

void RenderData::drawTexture(fbox rect, Texture* e, int state, int pixel_size, uint32_t flags, const fbox& bounds, int32_t zLayer )
{
    if (bounds.width < 1 || bounds.height < 1) return;

    auto& objects = this->objects;
    auto& commands = this->commands;

    if (!e) return;

    Math::ivec2 framebufferSize = GTexGui->framebufferSize;
    rect.x -= framebufferSize.x / 2.f;
    rect.y = framebufferSize.y / 2.f - rect.y - rect.height;

    Quad quad;
    quad.pixelSize = pixel_size;
    if (flags & SLICE_9)
    {
        for (int y = 0; y < 3; y++)
        {
            for (int x = 0; x < 3; x++)
            {
                quad.rect = rect;
                quad.texBounds = e->bounds;
                if (x == 0)
                {
                    quad.rect.width = e->left * pixel_size;
                    quad.texBounds.width = e->left;
                }
                else if (x == 1)
                {
                    quad.rect.x += e->left * pixel_size;
                    quad.rect.width -= (e->left + e->right) * pixel_size;
                    quad.texBounds.x += e->left;
                    quad.texBounds.width -= e->left + e->right;
                }
                else if (x == 2)
                {
                    quad.rect.x += quad.rect.width - e->right * pixel_size;
                    quad.rect.width = e->right * pixel_size;
                    quad.texBounds.x += quad.texBounds.width - e->right; 
                    quad.texBounds.width = e->right;
                }

                if (y == 0)
                {
                    quad.rect.height = e->bottom * pixel_size;
                    quad.texBounds.height = e->bottom;
                }
                else if (y == 1)
                {
                    quad.rect.y += e->bottom * pixel_size;
                    quad.rect.height -= (e->top + e->bottom) * pixel_size;
                    quad.texBounds.y -= e->bottom;
                    quad.texBounds.height -= e->top + e->bottom;
                }
                else if (y == 2)
                {
                    quad.rect.y += quad.rect.height - e->top * pixel_size;
                    quad.rect.height = e->top * pixel_size;
                    quad.texBounds.y -= quad.texBounds.height - e->top;
                    quad.texBounds.height = e->top;
                }
                objects.push_back(quad);
            }
        }
        commands.push_back({Command::QUAD, 9, flags, e, bounds, state});
        return;
    }

    objects.push_back(Quad{
        .rect = rect,
        .texBounds = e->bounds,
        .pixelSize = pixel_size
    });
    commands.push_back({Command::QUAD, 1, flags, e, bounds, state});
}

void RenderData::drawQuad(const Math::fbox& rect, const Math::fvec4& col, int32_t zLayer)
{
}
