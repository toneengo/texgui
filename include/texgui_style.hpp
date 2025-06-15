#pragma once

#include <cstdint>
#include <texgui_math.hpp>
#include <texgui_flags.hpp>
#include <string>

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
    Math::fvec4 Color;
    int Size;
    TexGui::Font* Font = nullptr;
};

struct WindowStyle {
    TexGui::Texture* Texture;
    Math::fvec4 Padding;
    uint32_t Flags;
    Math::fvec4 TitleColor;
    int TitleFontSize = 20;
};

struct ButtonStyle {
    TexGui::Texture* Texture;
    Math::fvec4 Padding;
    Math::fvec2 POffset;
    TextStyle Text;
};
struct TextInputStyle {
    TexGui::Texture* Texture;
    Math::fvec4 SelectColor;
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
};

struct ColumnStyle {
    int Height;
    int Spacing;
};


// Im not a C++ cult member i swear  to god this just looks nice when its used
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
};

Style* BeginStyle();
void EndStyle();

Style* getDefaultStyle();

int setPixelSize(int px);
}

