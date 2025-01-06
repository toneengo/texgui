#pragma once
#include <string>
#include "common.h"
#include "types.h"

NAMESPACE_BEGIN(TexGui);
NAMESPACE_BEGIN(Defaults);

inline int PixelSize  = 1;
inline uint32_t Flags = SLICE_9;

//#TODO: use font px instead of sacle, uising msdf

namespace Font {
    inline Math::fvec4 Color = {1, 1, 1, 1};
    inline float Scale = 0.4;
}

namespace Window {
    inline std::string Texture = "window";
    inline Math::fvec4 Padding(12);
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
    inline Math::fvec4 Padding(12);
}

namespace Row {
    inline int Height = INHERIT;
    inline int Spacing = 4;
}

namespace Column {
    inline int Height = INHERIT;
    inline int Spacing = 4;
}

NAMESPACE_END(Defaults);
NAMESPACE_END(TexGui);
