#pragma once

#include <cstdint>
#include <texgui_math.hpp>
#include <texgui_flags.hpp>
#include <string>
#include <chrono>

namespace TexGui {

struct Texture;
struct Font;

struct StackStyle {
    float Padding;
};

struct TooltipStyle {
    Math::fvec4 HoverPadding;
    int TextScale;
    Math::fvec2 MouseOffset;
    TexGui::Texture* Texture;
    Math::fvec4 Padding;
    float MaxWidth;
    Math::fvec2 UnderlineSize;
};

struct TextStyle {
    uint32_t Color;
    int Size;
    TexGui::Font* Font = nullptr;
};

struct ProgressBarStyle {
    TexGui::Texture* BackTexture = nullptr;
    TexGui::Texture* FrameTexture = nullptr;
    TexGui::Texture* BarTexture = nullptr;
    Math::fvec4 Padding;
    bool Flipped = false;
};

struct GridStyle {

    float Spacing;
};
struct Animation
{
    bool enabled = false;
    double bezier[4] = {0, 0, 1, 1};

    // in milliseconds
    int time = 0;
    int duration = 1000;

    // The properties of the object at the start of the animation
    // These default options mean the object will fade in, and slide in from the bottom
    Math::fvec2 offset = {0, -80};
    uint32_t alphaModifier = 0x00000000;
};

struct WindowStyle {
    TexGui::Texture* Texture;
    Math::fvec4 Padding;
    uint32_t Flags; //unused
    Animation InAnimation;
    TextStyle Text;
};

struct ButtonStyle {
    TexGui::Texture* Texture;
    Math::fvec4 Padding;
    Math::fvec2 POffset;
    TextStyle Text;
};
struct TextInputStyle {
    TexGui::Texture* Texture;
    uint32_t SelectColor;
    Math::fvec4 Padding;
    TextStyle Text;
};

struct ListItemStyle {
    TexGui::Texture* Texture;
    Math::fvec4 Padding;
};

struct BoxStyle {
    TexGui::Texture* Texture = nullptr;
    Math::fvec4 Padding;
};

struct SliderStyle {
    TexGui::Texture* BarTexture;
    TexGui::Texture* NodeTexture;
};

struct CheckBoxStyle {
    TexGui::Texture* Texture;
};

struct RadioButtonStyle {
    TexGui::Texture* Texture;
};

struct ScrollPanelStyle {
    Math::fvec4 Padding;
    TexGui::Texture* PanelTexture = nullptr;
    TexGui::Texture* BarTexture;
};

struct RowStyle {
    int Height;
    int Spacing;
    bool Wrapped;
};

struct ColumnStyle {
    int Height;
    int Spacing;
};

struct Style {
    StackStyle Stack;

    TooltipStyle Tooltip;

    TextStyle Text;

    WindowStyle Window;
    ButtonStyle Button;
    TextInputStyle TextInput;

    ListItemStyle ListItem;

    BoxStyle Box;

    SliderStyle Slider;

    CheckBoxStyle CheckBox;

    RadioButtonStyle RadioButton;

    ScrollPanelStyle ScrollPanel;

    RowStyle Row;

    ColumnStyle Column;

    ProgressBarStyle ProgressBar;
    GridStyle Grid;
};

Style* BeginStyle();
void EndStyle();

Style* getDefaultStyle();

int setPixelSize(int px);
}
