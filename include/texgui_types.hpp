#pragma once

#include "texgui.h"
#include <stdint.h>
#include <vector>
#include "msdf-atlas-gen/msdf-atlas-gen.h"

NAMESPACE_BEGIN(TexGui);

// font information
struct Font
{
    std::unordered_map<uint32_t, const msdf_atlas::GlyphGeometry*> codepointToGlyph;
    std::vector<msdf_atlas::GlyphGeometry> glyphs;

    //data for the renderer
    uint32_t textureIndex;

    //#TODO: remove these. Shader shouldnt need to know atlas width/height
    uint32_t atlasWidth;
    uint32_t atlasHeight;
    float msdfPxRange;
    uint32_t baseFontSize;
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

NAMESPACE_END(TexGui);
