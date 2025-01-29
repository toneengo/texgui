#pragma once

#include "../texgui.h"
#include "common.h"
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

struct TexEntry
{
    unsigned int glID;
    int layer;

    Math::ibox bounds;
    
    // Flags for if the texture has alternate images for states e.g. hovered/pressed etc
    unsigned int has_state = 0;

    // Used for 9-slice rendering
    float top;
    float right;
    float bottom;
    float left;

    // #TODO: low priority - this shouldn't be in the struct, and just in a temporary map when creating our atlas
    unsigned char* data;
    unsigned char* _hover;
    unsigned char* _press;
    unsigned char* _active;
};

inline int line_height = 0;
inline int font_height = 0;
inline float font_px = 0;
NAMESPACE_END(TexGui);
