#pragma once

#if !defined(NAMESPACE_BEGIN) || defined(DOXYGEN_DOCUMENTATION_BUILD)
    #define NAMESPACE_BEGIN(name) namespace name {
#endif

#if !defined(NAMESPACE_END) || defined(DOXYGEN_DOCUMENTATION_BUILD)
    #define NAMESPACE_END(name) }
#endif

#include <array>
#include <cstring>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "texgui_math.hpp"
#include "texgui_settings.hpp"

#include <vulkan/vulkan_core.h>

NAMESPACE_BEGIN(TexGui);

void init();

inline bool CapturingMouse = false;

class RenderData;

struct Texture;
struct TextSegment;
struct Paragraph
{
    TextSegment* data;
    uint32_t count;

    Paragraph() = default;

    Paragraph(std::vector<TextSegment>& s);

    Paragraph(TextSegment* ptr, uint32_t n);
};

using LazyData = int64_t;

using LazyIconFunc = Texture*(*)(LazyData);

// Text chunks are used to describe the layout of a text block. They get cached into Paragraphs.
struct TextChunk
{
    enum Type : uint8_t {
        TEXT,
        ICON,        // An icon to render inline with the text
        TOOLTIP,     // Marks the beginning/end of a tooltip's boundary
        INDIRECT,    // include a dynamic part while still processing the rest at compile time
        PLACEHOLDER, // Gets substituted for another chunk at draw time
        LAZY_ICON,   // Lazily fetch an icon
    } type;

    union
    {
        const char* text;
        Texture* icon;

        struct {
            Paragraph base;
            Paragraph tooltip;
        } tooltip;

        TextChunk* indirect;

        struct {
            LazyData data;
            LazyIconFunc func;
        } lazyIcon;
    };

    TextChunk() = default;

    constexpr TextChunk(const char* text) :
        type(TEXT), text(text) {}

    constexpr TextChunk(Texture* icon) :
        type(ICON), icon(icon) {}

    constexpr TextChunk(Paragraph baseText, Paragraph tooltip) :
        type(TOOLTIP), tooltip({ baseText, tooltip }) {}

    constexpr TextChunk(TextChunk* chunk) :
        type(INDIRECT), indirect(chunk) {}

    constexpr TextChunk(LazyData d, LazyIconFunc f) :
        type(LAZY_ICON), lazyIcon({d, f}) {}
};
inline TextChunk Placeholder() { TextChunk c; c.type = TextChunk::PLACEHOLDER; return c; };

struct TextSegment
{
    union
    {
        struct {
            const char* data;
            float tw; // Width of the text (not including whitespace)
            int16_t len;
        } word;

        Texture* icon;

        struct {
            Paragraph base;
            Paragraph tooltip;
        } tooltip;

        struct {
            LazyData data;
            LazyIconFunc func;
        } lazyIcon;
    };

    float width; // Width of the segment pre-scaling
        // Pt for text, pixels for icons. Includes whitespace for text

    enum Type : uint8_t {
        WORD,        // Denotes text followed by whitespace
        ICON,        // An icon to render inline with the text
        NEWLINE,     // Line break
        TOOLTIP,     // Marks the beginning/end of a tooltip's boundary
        PLACEHOLDER, // Gets substituted for another chunk at draw time
        LAZY_ICON,
    } type;

    // Doesn't break a chunk into words so you don't get text wrapping.
    static TextSegment FromChunkFast(TextChunk chunk);
};



// initialiser lists can't be implicitly converted to spans until c++26 so just make our own span type.
struct TextDecl
{
    const TextChunk* data;
    uint32_t count;

    TextDecl() = default;

    TextDecl(TextChunk* d, uint32_t n);

    TextDecl(std::initializer_list<TextChunk> s);

    const TextChunk* begin() const { return data; }
    const TextChunk* end() const { return data + count; }
};
    
using CharacterFilter = bool(*)(unsigned int c);

class Container
{
    friend struct Arrangers;
    friend class RenderData;

public:
    using ArrangeFunc = Math::fbox(*)(Container* parent, Math::fbox in);
        
    RenderData* rs;
    Math::fbox bounds;
    Math::fbox scissorBox = {-1, -1, -1, -1};

    Container Window(const char* name, float xpos, float ypos, float width, float height, uint32_t flags = 0,  Texture* texture = nullptr);
    bool      Button(const char* text, Texture* texture = nullptr, Container* out = nullptr);
    Container Box(float xpos, float ypos, float width, float height, Texture* texture = nullptr);
    void      CheckBox(bool* val);
    void      RadioButton(uint32_t* selected, uint32_t id);
    Container ScrollPanel(const char* name, Texture* texture = nullptr, Texture* bartex = nullptr);
    Container Slider(const char* name, Texture* texture = nullptr, Texture* bartex = nullptr);
    void      Image(Texture* texture, int scale = Defaults::PixelSize);

    void      TextInput(const char* name, std::string& buf, CharacterFilter filter = nullptr);
    void      TextInput(const char* name, char* buf, uint32_t bufsize, CharacterFilter filter = nullptr);
    void      Text(Paragraph text, int32_t scale = 0, TextDecl parameters = {});
    void      Text(const char* text, int32_t scale = 0, TextDecl parameters = {});

    Container Align(uint32_t flags = 0, Math::fvec4 padding = Math::fvec4(0,0,0,0));

    void      Divider(float padding = 0);

    // Similar to radio buttons - the id of the selected one is stored in the *selected pointer.
    // If you don't want them to be clickable - set selected to nullptr, and 0 or 1 for whether it is active in id
    Container ListItem(uint32_t* selected, uint32_t id);

    // Arranges children in a bento-grid layout.
    Container Grid();
    // Arranges children in a vertical stack.
    Container Stack(float padding = -1);
        
    void Row(Container* out, const float* widths, uint32_t n, float height, uint32_t flags = 0);
    template <uint32_t N>
    std::array<Container, N> Row(const float (&widths)[N], float height = 0, uint32_t flags = 0)
    {
        std::array<Container, N> out;
        Row(&out[0], widths, N, height, flags);
        return out;
    }

    void Column(Container* out, const float* widths, uint32_t n, float height);
    template <uint32_t N>
    std::array<Container, N> Column(const float (&heights)[N], float width = 0)
    {
        std::array<Container, N> out;
        Column(&out[0], heights, N, width);
        return out;
    }

private:
    std::unordered_map<std::string, uint32_t>* buttonStates;

    Container* parent;
    ArrangeFunc arrange;

    union
    {
        struct
        {
            float x, y, rowHeight;
            int n;
        } grid;
        struct
        {
            uint32_t* selected;
            uint32_t id;
        } listItem;

        struct 
        {
            uint32_t flags;
            float top, right, bottom, left;
        } align;

        struct
        {
            float y;
            float padding;
        } stack;
    };

    void* scrollPanelState = nullptr;


    inline Container withBounds(Math::fbox bounds, ArrangeFunc arrange = nullptr)
    {
        Container copy = *this;
        copy.bounds = bounds;
        copy.parent = this;
        copy.arrange = arrange;
        return copy;
    }

    static Math::fbox Arrange(Container* o, Math::fbox child);
};

struct IconSheet
{
    uint32_t glID;
    int32_t iw, ih; // Size of a single icon
    int32_t w, h;
    Texture* getIcon(uint32_t x, uint32_t y);
};

inline Container Base;

void newFrame();
void render();
void render(const RenderData& rs);
RenderData* newRenderData();

//"official" one
const RenderData& getRenderData();
void loadFont(const char* font);
void loadTextures(const char* dir);
IconSheet loadIcons(const char* dir, int32_t iconWidth, int32_t iconHeight);
void clear();
void clear(RenderData& rs);

struct Texture;

// Get a specific texture that was read by loadTextures
Texture* texByName(const char* name);
// Prepare a reference to a texture already present in an OpenGL 2D texture object.
Texture* customTexture(unsigned int glTexID, Math::ibox pixelBounds);
// Prepare a reference to a texture already present in a Vulkan image.
Texture* customTexture(VkImageView imageView, VkSampler sampler, Math::ibox pixelBounds);

Math::ivec2 getTexSize(Texture* tex);

std::vector<TextSegment> cacheText(TextDecl text);
// Caches text into a buffer. Returns -1 on failure (if there's not enough buffer space), or the amount otherwise
int32_t cacheText(TextDecl text, TextSegment* buffer, uint32_t capacity);

float computeTextWidth(const char* text, size_t numchars);

class RenderData
{
    friend class GLContext;
    friend class VulkanContext;
public:
    RenderData()
    {
        Base = TexGui::Base;
        Base.rs = this;
        Base.bounds = Math::fbox(0, 0, 8192, 8192);
        Base.scissorBox = Math::fbox(0, 0, 8192, 8192);
    }
    Container Base;

    Container drawTooltip(Math::fvec2 size);

    void drawQuad(const Math::fbox& rect, const Math::fvec4& col, int32_t zLayer = 0);
    void drawTexture(Math::fbox rect, Texture* e, int state, int pixel_size, uint32_t flags, const Math::fbox& bounds, int32_t zLayer = 0);
    int drawText(const char* text, Math::fvec2 pos, const Math::fvec4& col, int size, uint32_t flags, int32_t len = -1, int32_t zLayer = 0);
    //void scissor(int x, int y, int width, int height);
    void scissor(Math::fbox bounds);
    void descissor();

    void clear() {
        objects.clear();
        commands.clear();

        objects2.clear();
        commands2.clear();
    }

    void swap(RenderData& other) {
        objects.swap(other.objects);
        commands.swap(other.commands);

        objects2.swap(other.objects2);
        commands2.swap(other.commands2);
    }

    void copy(const RenderData& other)
    {
        static auto copy = [](auto& a, auto& other, size_t sz)
        {
            size_t n = other.size();
            a.resize(n);
            memcpy(a.data(), other.data(), sz * n);
        };

        copy(objects, other.objects, sizeof(Object));
        copy(objects2, other.objects2, sizeof(Object));
        copy(commands, other.commands, sizeof(Command));
        copy(commands2, other.commands2, sizeof(Command));
    }

private:

    int prevObjCount = 0;
    int prevComCount = 0;
    // Renderable objects
    struct alignas(16) Character
    {
        Math::fbox rect; //xpos, ypos, width, height
        Math::fbox texBounds;
        Math::fvec4 colour = Math::fvec4(1.0);
        int size;
    };

    struct alignas(16) Quad
    {
        Math::fbox rect; //xpos, ypos, width, height
        Math::fbox texBounds; //xpos, ypos, width, height
        Math::fvec4 colour = Math::fvec4(1.0);
        int pixelSize;
    };

    struct alignas(16) Object
    {
        Object()
        {
        }

        Object(const Object& o)
        {
            *this = o;
        }

        Object(Character c)
        {
            ch = c;
        }
        Object(Quad q)
        {
            quad = q;
        }

        union {
            Character ch;
            Quad quad;
        };
    };

    struct Command
    {
        enum {
            QUAD,
            CHARACTER,
        } type;

        uint32_t number;
        uint32_t flags = 0;
        Texture * texture;
        Math::fbox scissorBox;
        int state = 0;
    };

    std::vector<Object> objects;
    std::vector<Command> commands;

    // This is a lazy approach until we have some proper draw ordering/z filtering etc
    std::vector<Object> objects2;
    std::vector<Command> commands2;
};

Math::fvec2 calculateTextBounds(Paragraph text, int32_t scale, float maxWidth);
Math::fvec2 calculateTextBounds(const char* text, float maxWidth, int32_t scale = Defaults::Font::Size);

//This is copied from Dear ImGui. Thank you Ocornut
enum TexGuiKey : int
{
    // Keyboard
    TexGuiKey_None = 0,
    TexGuiKey_NamedKey_BEGIN = 512,  // First valid key value (other than 0)

    TexGuiKey_Tab = 512,             // == TexGuiKey_NamedKey_BEGIN
    TexGuiKey_LeftArrow,
    TexGuiKey_RightArrow,
    TexGuiKey_UpArrow,
    TexGuiKey_DownArrow,
    TexGuiKey_PageUp,
    TexGuiKey_PageDown,
    TexGuiKey_Home,
    TexGuiKey_End,
    TexGuiKey_Insert,
    TexGuiKey_Delete,
    TexGuiKey_Backspace,
    TexGuiKey_Space,
    TexGuiKey_Enter,
    TexGuiKey_Escape,
    TexGuiKey_LeftCtrl, TexGuiKey_LeftShift, TexGuiKey_LeftAlt, TexGuiKey_LeftSuper,
    TexGuiKey_RightCtrl, TexGuiKey_RightShift, TexGuiKey_RightAlt, TexGuiKey_RightSuper,
    TexGuiKey_Menu,
    TexGuiKey_0, TexGuiKey_1, TexGuiKey_2, TexGuiKey_3, TexGuiKey_4, TexGuiKey_5, TexGuiKey_6, TexGuiKey_7, TexGuiKey_8, TexGuiKey_9,
    TexGuiKey_A, TexGuiKey_B, TexGuiKey_C, TexGuiKey_D, TexGuiKey_E, TexGuiKey_F, TexGuiKey_G, TexGuiKey_H, TexGuiKey_I, TexGuiKey_J,
    TexGuiKey_K, TexGuiKey_L, TexGuiKey_M, TexGuiKey_N, TexGuiKey_O, TexGuiKey_P, TexGuiKey_Q, TexGuiKey_R, TexGuiKey_S, TexGuiKey_T,
    TexGuiKey_U, TexGuiKey_V, TexGuiKey_W, TexGuiKey_X, TexGuiKey_Y, TexGuiKey_Z,
    TexGuiKey_F1, TexGuiKey_F2, TexGuiKey_F3, TexGuiKey_F4, TexGuiKey_F5, TexGuiKey_F6,
    TexGuiKey_F7, TexGuiKey_F8, TexGuiKey_F9, TexGuiKey_F10, TexGuiKey_F11, TexGuiKey_F12,
    TexGuiKey_F13, TexGuiKey_F14, TexGuiKey_F15, TexGuiKey_F16, TexGuiKey_F17, TexGuiKey_F18,
    TexGuiKey_F19, TexGuiKey_F20, TexGuiKey_F21, TexGuiKey_F22, TexGuiKey_F23, TexGuiKey_F24,
    TexGuiKey_Apostrophe,        // '
    TexGuiKey_Comma,             // ,
    TexGuiKey_Minus,             // -
    TexGuiKey_Period,            // .
    TexGuiKey_Slash,             // /
    TexGuiKey_Semicolon,         // ;
    TexGuiKey_Equal,             // =
    TexGuiKey_LeftBracket,       // [
    TexGuiKey_Backslash,         // \ (this text inhibit multiline comment caused by backslash)
    TexGuiKey_RightBracket,      // ]
    TexGuiKey_GraveAccent,       // `
    TexGuiKey_CapsLock,
    TexGuiKey_ScrollLock,
    TexGuiKey_NumLock,
    TexGuiKey_PrintScreen,
    TexGuiKey_Pause,
    TexGuiKey_Keypad0, TexGuiKey_Keypad1, TexGuiKey_Keypad2, TexGuiKey_Keypad3, TexGuiKey_Keypad4,
    TexGuiKey_Keypad5, TexGuiKey_Keypad6, TexGuiKey_Keypad7, TexGuiKey_Keypad8, TexGuiKey_Keypad9,
    TexGuiKey_KeypadDecimal,
    TexGuiKey_KeypadDivide,
    TexGuiKey_KeypadMultiply,
    TexGuiKey_KeypadSubtract,
    TexGuiKey_KeypadAdd,
    TexGuiKey_KeypadEnter,
    TexGuiKey_KeypadEqual,
    TexGuiKey_AppBack,               // Available on some keyboard/mouses. Often referred as "Browser Back"
    TexGuiKey_AppForward,
    TexGuiKey_Oem102,                // Non-US backslash.

    // Gamepad (some of those are analog values, 0.0f to 1.0f)                          // NAVIGATION ACTION
    // (download controller mapping PNG/PSD at http://dearimgui.com/controls_sheets)
    TexGuiKey_GamepadStart,          // Menu (Xbox)      + (Switch)   Start/Options (PS)
    TexGuiKey_GamepadBack,           // View (Xbox)      - (Switch)   Share (PS)
    TexGuiKey_GamepadFaceLeft,       // X (Xbox)         Y (Switch)   Square (PS)        // Tap: Toggle Menu. Hold: Windowing mode (Focus/Move/Resize windows)
    TexGuiKey_GamepadFaceRight,      // B (Xbox)         A (Switch)   Circle (PS)        // Cancel / Close / Exit
    TexGuiKey_GamepadFaceUp,         // Y (Xbox)         X (Switch)   Triangle (PS)      // Text Input / On-screen Keyboard
    TexGuiKey_GamepadFaceDown,       // A (Xbox)         B (Switch)   Cross (PS)         // Activate / Open / Toggle / Tweak
    TexGuiKey_GamepadDpadLeft,       // D-pad Left                                       // Move / Tweak / Resize Window (in Windowing mode)
    TexGuiKey_GamepadDpadRight,      // D-pad Right                                      // Move / Tweak / Resize Window (in Windowing mode)
    TexGuiKey_GamepadDpadUp,         // D-pad Up                                         // Move / Tweak / Resize Window (in Windowing mode)
    TexGuiKey_GamepadDpadDown,       // D-pad Down                                       // Move / Tweak / Resize Window (in Windowing mode)
    TexGuiKey_GamepadL1,             // L Bumper (Xbox)  L (Switch)   L1 (PS)            // Tweak Slower / Focus Previous (in Windowing mode)
    TexGuiKey_GamepadR1,             // R Bumper (Xbox)  R (Switch)   R1 (PS)            // Tweak Faster / Focus Next (in Windowing mode)
    TexGuiKey_GamepadL2,             // L Trig. (Xbox)   ZL (Switch)  L2 (PS) [Analog]
    TexGuiKey_GamepadR2,             // R Trig. (Xbox)   ZR (Switch)  R2 (PS) [Analog]
    TexGuiKey_GamepadL3,             // L Stick (Xbox)   L3 (Switch)  L3 (PS)
    TexGuiKey_GamepadR3,             // R Stick (Xbox)   R3 (Switch)  R3 (PS)
    TexGuiKey_GamepadLStickLeft,     // [Analog]                                         // Move Window (in Windowing mode)
    TexGuiKey_GamepadLStickRight,    // [Analog]                                         // Move Window (in Windowing mode)
    TexGuiKey_GamepadLStickUp,       // [Analog]                                         // Move Window (in Windowing mode)
    TexGuiKey_GamepadLStickDown,     // [Analog]                                         // Move Window (in Windowing mode)
    TexGuiKey_GamepadRStickLeft,     // [Analog]
    TexGuiKey_GamepadRStickRight,    // [Analog]
    TexGuiKey_GamepadRStickUp,       // [Analog]
    TexGuiKey_GamepadRStickDown,     // [Analog]

    // Aliases: Mouse Buttons (auto-submitted from AddMouseButtonEvent() calls)
    // - This is mirroring the data also written to io.MouseDown[], io.MouseWheel, in a format allowing them to be accessed via standard key API.
    TexGuiKey_MouseLeft, TexGuiKey_MouseRight, TexGuiKey_MouseMiddle, TexGuiKey_MouseX1, TexGuiKey_MouseX2, TexGuiKey_MouseWheelX, TexGuiKey_MouseWheelY,

    // [Internal] Reserved for mod storage
    TexGuiKey_ReservedForModCtrl, TexGuiKey_ReservedForModShift, TexGuiKey_ReservedForModAlt, TexGuiKey_ReservedForModSuper,
    TexGuiKey_NamedKey_END,

    // Keyboard Modifiers (explicitly submitted by backend via AddKeyEvent() calls)
    // - This is mirroring the data also written to io.KeyCtrl, io.KeyShift, io.KeyAlt, io.KeySuper, in a format allowing
    //   them to be accessed via standard key API, allowing calls such as IsKeyPressed(), IsKeyReleased(), querying duration etc.
    // - Code polling every key (e.g. an interface to detect a key press for input mapping) might want to ignore those
    //   and prefer using the real keys (e.g. TexGuiKey_LeftCtrl, TexGuiKey_RightCtrl instead of TexGuiMod_Ctrl).
    // - In theory the value of keyboard modifiers should be roughly equivalent to a logical or of the equivalent left/right keys.
    //   In practice: it's complicated; mods are often provided from different sources. Keyboard layout, IME, sticky keys and
    //   backends tend to interfere and break that equivalence. The safer decision is to relay that ambiguity down to the end-user...
    // - On macOS, we swap Cmd(Super) and Ctrl keys at the time of the io.AddKeyEvent() call.
    TexGuiMod_None                   = 0,
    TexGuiMod_Ctrl                   = 1 << 12, // Ctrl (non-macOS), Cmd (macOS)
    TexGuiMod_Shift                  = 1 << 13, // Shift
    TexGuiMod_Alt                    = 1 << 14, // Option/Menu
    TexGuiMod_Super                  = 1 << 15, // Windows/Super (non-macOS), Ctrl (macOS)
    TexGuiMod_Mask_                  = 0xF000,  // 4-bits

    // [Internal] If you need to iterate all keys (for e.g. an input mapper) you may use TexGuiKey_NamedKey_BEGIN..TexGuiKey_NamedKey_END.
    TexGuiKey_NamedKey_COUNT         = TexGuiKey_NamedKey_END - TexGuiKey_NamedKey_BEGIN,
    //TexGuiKey_KeysData_SIZE        = TexGuiKey_NamedKey_COUNT,  // Size of KeysData[]: only hold named keys
    //TexGuiKey_KeysData_OFFSET      = TexGuiKey_NamedKey_BEGIN,  // Accesses to io.KeysData[] must use (key - TexGuiKey_NamedKey_BEGIN) index.

};

NAMESPACE_END(TexGui);
