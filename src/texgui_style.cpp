#include "texgui_style.hpp"
#include "texgui_internal.hpp"
#include "texgui.h"

using namespace TexGui;

Style* TexGui::BeginStyle()
{
    Style* style = new Style;
    if (!GTexGui->styleStack.empty())
    {
        *style = *GTexGui->styleStack.back();
    };
    GTexGui->styleStack.push_back(style);
    return style;
}

void TexGui::EndStyle()
{
    delete GTexGui->styleStack.back();
    GTexGui->styleStack.pop_back();
}

Style* initDefaultStyle()
{
    auto style = BeginStyle();
    style->Tooltip.HoverPadding = {6, 12, 6, 12};
    style->Tooltip.TextScale = 20;
    style->Tooltip.MouseOffset = {16, 16};
    style->Tooltip.Texture = &GTexGui->textures["tooltip"];
    style->Tooltip.Padding = {8, 12, 8, 12};
    style->Tooltip.MaxWidth = 400;
    style->Tooltip.UnderlineSize = {0, 2};

    style->Stack.Padding = 8;

    style->Text.Color = {1, 1, 1, 1};
    style->Text.Size = 20;

    style->Window.Texture = &GTexGui->textures["window"];
    style->Window.Padding = {12, 12, 12, 12};
    style->Window.Flags = 0;

    style->Button.Texture = &GTexGui->textures["button"];
    style->Button.Padding = Math::fvec4(12);
    style->Button.POffset = 0;
    style->Button.Text = style->Text;

    style->TextInput.Texture = &GTexGui->textures["textinput"];
    style->TextInput.SelectColor = {0.0, 0.8, 0.8, 1.0};
    style->TextInput.Padding = Math::fvec4(8);
    style->TextInput.Text = style->Text;

    style->ListItem.Texture = &GTexGui->textures["listitem"];
    style->ListItem.Padding = Math::fvec4(4);

    style->Box.Padding = Math::fvec4(8);

    style->Slider.BarTexture = &GTexGui->textures["sliderbar"];
    style->Slider.NodeTexture = &GTexGui->textures["slidernode"];

    style->CheckBox.Texture = &GTexGui->textures["checkbox"];

    style->RadioButton.Texture = &GTexGui->textures["radiobutton"];

    style->ScrollPanel.Padding = Math::fvec4(8);
    //style->ScrollPanel.PanelTexture = &GTexGui->textures["scroll_bar"];
    style->ScrollPanel.BarTexture = &GTexGui->textures["scroll_bar"];

    style->Row.Height = INHERIT;
    style->Row.Spacing = 4;

    style->Column.Height = INHERIT;
    style->Column.Spacing = 4;
    return style;
}

Style* TexGui::getDefaultStyle()
{
    return GTexGui->styleStack[0];
}

int TexGui::setPixelSize(int px)
{
    int old = GTexGui->pixelSize;
    GTexGui->pixelSize = px;
    return old;
}
