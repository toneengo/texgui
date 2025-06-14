#include "texgui_settings.hpp"
#include "texgui_internal.hpp"
#include "texgui.h"

using namespace TexGui;

Style::Style()
{
    if (!GTexGui->styleStack.empty())
    {
        *this = *GTexGui->styleStack.back();
    };
    GTexGui->styleStack.push_back(this);
}

Style::~Style()
{
    GTexGui->styleStack.pop_back();
}
DefaultStyle::DefaultStyle() : Style()
{
    Tooltip.HoverPadding = {6, 12, 6, 12};
    Tooltip.TextScale = 20;
    Tooltip.MouseOffset = {16, 16};
    Tooltip.Texture = &GTexGui->textures["tooltip"];
    Tooltip.Padding = {8, 12, 8, 12};
    Tooltip.MaxWidth = 400;
    Tooltip.UnderlineSize = {0, 2};

    Stack.Padding = 8;

    Text.Color = {1, 1, 1, 1};
    Text.Size = 20;
    Text.Font;

    Window.Texture = &GTexGui->textures["window"];
    Window.Padding = {12, 12, 12, 12};
    Window.Flags = 0;

    Button.Texture = &GTexGui->textures["button"];
    Button.Padding = Math::fvec4(12);
    Button.Flags   = SLICE_9 | CENTER_X | CENTER_Y;
    Button.POffset = 0;

    TextInput.Texture = &GTexGui->textures["textinput"];
    TextInput.SelectColor = {0.0, 0.8, 0.8, 1.0};
    TextInput.Padding = Math::fvec4(8);

    ListItem.Texture = &GTexGui->textures["listitem"];
    ListItem.Padding = Math::fvec4(4);

    Box.Padding = Math::fvec4(8);

    Slider.BarTexture = &GTexGui->textures["sliderbar"];
    Slider.NodeTexture = &GTexGui->textures["slidernode"];

    CheckBox.Texture = &GTexGui->textures["checkbox"];

    RadioButton.Texture = &GTexGui->textures["radiobutton"];

    ScrollPanel.Padding = Math::fvec4(8);
    //ScrollPanel.PanelTexture = &GTexGui->textures["scroll_bar"];
    ScrollPanel.BarTexture = &GTexGui->textures["scroll_bar"];

    Row.Height = INHERIT;
    Row.Spacing = 4;

    Column.Height = INHERIT;
    Column.Spacing = 4;
}

Style* TexGui::getDefaultStyle()
{
    return GTexGui->styleStack[0];
}
