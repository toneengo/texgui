#pragma once

#include <cstdint>
#include <texgui_math.hpp>
#include <texgui_flags.hpp>
#include <string>

namespace TexGui {

struct Texture;
struct Font;

// Im not a C++ cult member i swear  to god this just looks nice when its used
struct Style {
    Style();
    ~Style();

    int PixelSize  = 1;
    struct {
        float Padding;
    } Stack;

    struct {
        Math::fvec4 HoverPadding;
        int TextScale;
        Math::fvec2 MouseOffset;
        TexGui::Texture* Texture;
        Math::fvec4 Padding;
        float MaxWidth;
        Math::fvec2 UnderlineSize;
    } Tooltip;

    struct {
        Math::fvec4 Color;
        int Size;
        TexGui::Font* Font;
    } Text;

    struct {
        TexGui::Texture* Texture;
        Math::fvec4 Padding;
        uint32_t Flags;
    } Window;
    struct {
        TexGui::Texture* Texture;
        Math::fvec4 Padding;
        uint32_t    Flags  ;
        Math::fvec2 POffset;
    } Button;
    struct {
        TexGui::Texture* Texture;
        Math::fvec4 SelectColor;
        Math::fvec4 Padding;
    } TextInput;

    struct {
        TexGui::Texture* Texture;
        Math::fvec4 Padding;
    } ListItem;

    struct {
        TexGui::Texture* Texture = nullptr;
        Math::fvec4 Padding;
    } Box;

    struct {
        TexGui::Texture* BarTexture;
        TexGui::Texture* NodeTexture;
    } Slider;

    struct {
        TexGui::Texture* Texture;
    } CheckBox;

    struct RadioButton {
        TexGui::Texture* Texture;
    } RadioButton;

    struct {
        Math::fvec4 Padding;
        TexGui::Texture* PanelTexture = nullptr;
        TexGui::Texture* BarTexture;
    } ScrollPanel;

    struct {
        int Height;
        int Spacing;
    } Row;

    struct {
        int Height;
        int Spacing;
    } Column;
};

struct DefaultStyle : public Style
{
public:
    DefaultStyle();
};

Style* getDefaultStyle();

}
