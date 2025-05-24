#pragma once

#include "texgui.h"
#include <stdint.h>
#include <vector>

NAMESPACE_BEGIN(TexGui);

// font information
struct CharInfo
{
    int layer;
    Math::uivec2 size;
    Math::ivec2 bearing;
    unsigned int advance;
};

struct Texture
{
    unsigned int id = -1;

    //Texture sub-region
    Math::fbox bounds;

    //Total size of texture
    Math::ivec2 size;

    // Used for 9-slice rendering
    float top;
    float right;
    float bottom;
    float left;

    //texture IDs for hover press and active textures
    unsigned int hover = -1;
    unsigned int press = -1;
    unsigned int active = -1;
};

inline int line_height = 0;
inline int font_height = 0;
//atlas size
inline float font_px = 0;
NAMESPACE_END(TexGui);
