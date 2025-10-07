#pragma once

#include "texgui_flags.hpp"
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
#include <cassert>
#include <unordered_map>
#include <vector>
#include <span>
#include "texgui_math.hpp"
#include "texgui_style.hpp"

NAMESPACE_BEGIN(TexGui);

Math::fvec2 getCursorPos();
bool isCapturingMouse();

Texture* loadTexture(const char* name, const unsigned char* pixels, int width, int height);
bool loadTexture(const char* path);
void init();

using TexGuiID = uint32_t;

class RenderData;

struct Texture;
struct Font;

using LazyData = int64_t;

using LazyIconFunc = Texture*(*)(LazyData);

struct TGStr {
    const uint8_t* utf8;
    size_t len;

    /*
    TGStr() = default;
    TGStr(const char* c_str) :
        utf8((const uint8_t*) c_str), len(strlen(c_str)) {}
    */
};

using CharacterFilter = bool(*)(unsigned int c);
struct TexGuiWindow;

struct IconSheet
{
    uint32_t glID;
    int32_t iw, ih; // Size of a single icon
    int32_t w, h;
    Texture* getIcon(uint32_t x, uint32_t y);
};

struct TGContainer;
struct TGContainerArray
{
    TGContainer* data;
    size_t size = 0;
    TGContainer* operator[](size_t index);
};
using ArrangeFunc = Math::fbox(*)(TGContainer* parent, Math::fbox in);
using RenderFunc = void(*)(TGContainer* container);

TGContainer* BeginTooltip(Math::fvec2 size, TooltipStyle* style = nullptr);
void EndTooltip(TGContainer* c);

TGContainer* Window(const char* id, TGStr name, float xpos, float ypos, float width, float height, uint32_t flags = 0, TexGui::WindowStyle* style = nullptr);
bool         Button(TGContainer* container, const char* id, TGStr label, TexGui::ButtonStyle* style = nullptr);
TGContainer* Box(TGContainer* container, float xpos, float ypos, float width, float height, uint32_t flags = 0, TexGui::BoxStyle* style = nullptr);
TGContainer* Box(TGContainer* container);
bool         CheckBox(TGContainer* container, bool* val, TexGui::CheckBoxStyle* style = nullptr);
void         RadioButton(TGContainer* container, uint32_t* selected, uint32_t id, TexGui::RadioButtonStyle* style = nullptr);
TGContainer* BeginScrollPanel(TGContainer* container, const char* name, TexGui::ScrollPanelStyle* style = nullptr);
void         EndScrollPanel(TGContainer* container);
int          SliderInt(TGContainer* container, int* val, int minVal, int maxVal, TexGui::SliderStyle* style = nullptr);
//void      Image(TGContainer* container, Texture* texture, int scale = -1);
bool         DropdownInt(TGContainer* container, int* val, std::initializer_list<std::pair<TGStr, int>> names);
void         TextInput(TGContainer* container, const char* name, char* buf, uint32_t bufsize, TexGui::TextInputStyle* style = nullptr);
void         Text(TGContainer* container, TGStr text, TexGui::TextStyle* style = nullptr);
void         Text(TGContainer* container, const char* text, TexGui::TextStyle* style = nullptr);
void         Text(TGContainer* container, TexGui::TextStyle* style, const char* text, ...);
TGContainer* Align(TGContainer* container, uint32_t flags = 0, const Math::fvec4 padding = {0,0,0,0});
void         Divider(TGContainer* container, float padding = 0);
void         Line(TGContainer* container, float x1, float y1, float x2, float y2, uint32_t color, float lineWidth = 1.f);

// Similar to radio buttons - the id of the selected one is stored in the *selected pointer.
// If you don't want them to be clickable - set selected to nullptr, and 0 or 1 for whether it is active in id
TGContainer* ListItem(TGContainer* container, uint32_t* selected, uint32_t id, TexGui::ListItemStyle* style = nullptr);

// Arranges children in a bento-grid layout.
TGContainer* Grid(TGContainer* container, GridStyle* style = nullptr);

// Arranges children in a vertical stack.
TGContainer* Stack(TGContainer* container, float padding = -1, TexGui::StackStyle* style = nullptr);

void         ProgressBar(TGContainer* container, float percentage, const TexGui::ProgressBarStyle* style = nullptr);
void         ProgressBarV(TGContainer* container, float percentage, const TexGui::ProgressBarStyle* style = nullptr);
TGContainer* Node(TGContainer* container, float x, float y);

TGContainerArray Row(TGContainer* container, std::initializer_list<float> widths, float height = 0, TexGui::RowStyle* style = nullptr);
TGContainerArray Column(TGContainer* container, std::initializer_list<float> heights, float width = 0, TexGui::ColumnStyle* style = nullptr);

TGContainerArray Row(TGContainer* container, uint32_t widthCount, const float* pWidths, float height = 0, TexGui::RowStyle* style = nullptr);
TGContainerArray Column(TGContainer* container, uint32_t heightCount, const float* pHeights, float width = 0, TexGui::ColumnStyle* style = nullptr);

Math::fbox getBounds(TGContainer* c);

// #TODO: Doesn't work for all widgets.
Math::fbox getSize(TGContainer* c);
void Image(TGContainer* c, Texture* texture, int scale = -1);
void Image(TGContainer* c, Texture* texture, uint32_t colorOverride, int scale);

void newFrame();
float getTextScale();
void setTextScale(float scale);
float getUiScale();
void setUiScale(float scale);
void render(const RenderData& rs);
RenderData* newRenderData();

//"official" one
const RenderData& getRenderData();
void loadTextures(const char* dir);
IconSheet loadIcons(const char* dir, int32_t iconWidth, int32_t iconHeight);
void clear();
void destroy();

// DBG shorthands
inline bool Button(TGContainer* container, const char* id, TexGui::ButtonStyle* style = nullptr) {
    return Button(container, id, {(const uint8_t*)id, strlen(id)}, style);    
}

struct Texture;

// Get a specific texture that was read by loadTextures
Texture* getTexture(const char* name);
// Prepare a reference to a texture already present in an OpenGL 2D texture object.
//Texture* customTexture(unsigned int glTexID, Math::ibox pixelBounds);

Font* getFont(const char* name);

Math::ivec2 getTextureSize(Texture* tex);

float computeTextWidth(const std::vector<uint32_t>& codepoints);

struct TextInputState;
class RenderData
{
public:
    bool ordered = false;
    int32_t priority = -1;
    std::vector<RenderData> children;
    uint32_t alphaModifier = 0x000000FF;

    RenderData()
    {
    }

    void operator=(const RenderData& other)
    {
        commands = other.commands;
        children = other.children;
        vertices = other.vertices;
        indices = other.indices;
        ordered = other.ordered;
        priority = other.priority;
    }

    void operator=(RenderData&& other)
    {
        commands.swap(other.commands);
        children.swap(other.children);
        vertices.swap(other.vertices);
        indices.swap(other.indices);
        std::swap(ordered, other.ordered);
        std::swap(priority, other.priority);
    }

    RenderData(const RenderData& other)
    {
        *this = other;
    }

    RenderData(RenderData&& other)
    {
        *this = std::move(other);
    }

    //just in case?
    ~RenderData()
    {
        commands.clear();
        children.clear();
    }

    void addLine(float x1, float y1, float x2, float y2, uint32_t col, float lineWidth);
    void addQuad(Math::fbox rect, uint32_t col);
    void addTexture(Math::fbox rect, Texture* e, int state, int pixel_size, uint32_t flags, uint32_t col = 0xFFFFFFFF);
    void addText(TGStr text, TexGui::Font* font, Math::fvec2 pos, uint32_t col, int pixelSize, float boundWidth, float boundHeight, TextInputState* textInput = nullptr);
    void addText(const uint16_t* codepointStart, const uint32_t len, TexGui::Font* font, Math::fvec2 pos, uint32_t col, int pixelSize, float boundWidth, float boundHeight, TextInputState* textInput = nullptr);
    bool drawTextSelection(const uint16_t* codepointStart, const uint32_t len, TextInputState* textInput, Math::fvec2 textPos, int size);
    void pushScissor(Math::fbox region);
    void popScissor();

    void clear() {
        commands.clear();
        children.clear();
        vertices.clear();
        indices.clear();
        ordered = false;
    }

    struct Command
    {
        RenderDataCommandType type;
        union {
            struct
            {
                uint32_t indexCount;
                uint32_t textureIndex;
                float scaleX;
                float scaleY;
                float translateX = -1.f;
                float translateY = -1.f;
                float uvScaleX;
                float uvScaleY;
            } draw;
            struct
            {
                bool push;
                int x;
                int y;
                int width;
                int height;
            } scissor;
        };
    };

    struct Vertex
    {
        Math::fvec2 pos;
        Math::fvec2 uv;
        uint32_t col = 0xFFFFFFFF;
    };

    // Renderable objects
    std::vector<Command> commands;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

void setRenderData(RenderData* renderData);
Math::fvec2 calculateTextBounds(const char* text, float maxWidth, int32_t scale = -1);

//This is copied from Dear ImGui. Thank you Ocornut
enum TexGuiKey : int
{
    // Keyboard
    TexGuiKey_None = 0,
    TexGuiKey_NamedKey_BEGIN = 1,  // First valid key value (other than 0)

    TexGuiKey_Tab = 1,             // == TexGuiKey_NamedKey_BEGIN
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

    TexGuiKey_NamedKey_END,

    //TexGuiKey_NamedKey_COUNT         = TexGuiKey_NamedKey_END - TexGuiKey_NamedKey_BEGIN,
    TexGuiKey_NamedKey_COUNT         = TexGuiKey_NamedKey_END,
    //TexGuiKey_KeysData_SIZE        = TexGuiKey_NamedKey_COUNT,  // Size of KeysData[]: only hold named keys
    //TexGuiKey_KeysData_OFFSET      = TexGuiKey_NamedKey_BEGIN,  // Accesses to io.KeysData[] must use (key - TexGuiKey_NamedKey_BEGIN) index.

};

enum TexGuiMod : uint16_t
{
    TexGuiMod_None = 0x0000u,
    TexGuiMod_LeftShift = 0x0001u,
    TexGuiMod_RightShift = 0x0002u,
    TexGuiMod_Level5 = 0x0004u,
    TexGuiMod_LeftCtrl = 0x0040u,
    TexGuiMod_RightCtrl = 0x0080u,
    TexGuiMod_LeftAlt = 0x0100u,
    TexGuiMod_RightAlt = 0x0200u,
    TexGuiMod_LeftSuper = 0x0400u,
    TexGuiMod_RightSuper = 0x0800u,
    TexGuiMod_Num = 0x1000u,
    TexGuiMod_Caps = 0x2000u,
    TexGuiMod_Mode = 0x4000u,
    TexGuiMod_Scroll = 0x8000u,
    TexGuiMod_Ctrl = (TexGuiMod_LeftCtrl | TexGuiMod_RightCtrl),
    TexGuiMod_Shift = (TexGuiMod_LeftShift | TexGuiMod_RightShift),
    TexGuiMod_Alt = (TexGuiMod_LeftAlt | TexGuiMod_RightAlt),
    TexGuiMod_Super = (TexGuiMod_LeftSuper | TexGuiMod_RightSuper),
};

NAMESPACE_END(TexGui);
