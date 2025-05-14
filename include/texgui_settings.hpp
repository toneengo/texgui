#pragma once

#include <cstdint>
#include <texgui_math.hpp>
#include <texgui_flags.hpp>
#include <string>
namespace TexGui {
namespace Defaults {

inline int PixelSize  = 1;
inline uint32_t Flags = SLICE_9;

namespace Stack {
    inline float Padding = 8;
}

//#TODO: use font px instead of sacle, uising msdf
namespace Tooltip {
    inline Math::fvec4 HoverPadding(6, 12, 6, 12);
    inline int TextScale = 20;
    inline Math::fvec2 MouseOffset(16, 16);
    inline std::string Texture = "tooltip";
    inline Math::fvec4 Padding(18, 12, 18, 12);
    inline float MaxWidth = 400;

    inline Math::fvec2 UnderlineSize(0, 2);
}

namespace Settings {
    inline bool Async = false;
}
namespace Font {
    inline Math::fvec4 Color = {1, 1, 1, 1};
    inline int Size = 20;
    inline float MsdfPxRange = 2.0;
}

namespace Window {
    inline std::string Texture = "window";
    inline Math::fvec4 Padding(12);
    inline uint32_t flags;
}

namespace Button {
    inline std::string Texture = "button";
    inline Math::fvec4 Padding(12);
    inline uint32_t    Flags   = SLICE_9 | CENTER_X | CENTER_Y;
    inline Math::fvec2 POffset = 0;
}

namespace TextInput {
    inline std::string Texture = "textinput";
    inline Math::fvec4 Padding(8);
}

namespace ListItem {
    inline std::string Texture = "listitem";
    inline Math::fvec4 Padding(4);
}

namespace Box {
    inline Math::fvec4 Padding(8);
}

namespace CheckBox {
    inline std::string Texture = "checkbox";
}
namespace RadioButton {
    inline std::string Texture = "radiobutton";
}

namespace ScrollPanel {
    inline Math::fvec4 Padding(8);
}

namespace Row {
    inline int Height = INHERIT;
    inline int Spacing = 4;
}

namespace Column {
    inline int Height = INHERIT;
    inline int Spacing = 4;
}

}
}
