#pragma once

#include "texgui.h"
#ifdef __APPLE__
#include <_strings.h>
#endif
#include <stdint.h>
#include <vector>
#include "msdf-atlas-gen/msdf-atlas-gen.h"

NAMESPACE_BEGIN(TexGui);

struct FontGlyph
{
    unsigned int colored : 1;
    unsigned int visible : 1;
    unsigned int codepoint : 30;
    float advanceX;
    float X0, Y0, X1, Y1;
    float U0, V0, U1, V1;
};

struct Texture
{
    uint32_t id = -1;

    //Texture sub-region
    Math::ibox bounds;

    //Total size of texture
    Math::ivec2 size;

    // Used for 9-slice rendering
    float top;
    float right;
    float bottom;
    float left;

    //texture IDs for hover press and active textures
    uint32_t hover = -1;
    uint32_t press = -1;
    uint32_t active = -1;
};

// font information
struct Font
{
    std::vector<uint16_t> indexLookup;
    std::vector<FontGlyph> glyphs;

    float pixelSize;

    Texture* atlasTexture;

    float ascent;
    float descent;
    float lineGap;

    FontGlyph getGlyph(uint32_t codepoint)
    {
        if (indexLookup.size() < codepoint) return {};
        return glyphs[indexLookup[codepoint]];
    }

    void addGlyph(FontGlyph glyph)
    {
        if (indexLookup.size() < glyph.codepoint + 1)
        {
            indexLookup.resize(glyph.codepoint + 1);
        }

        if (glyph.X0 == glyph.X1 || glyph.Y0 == glyph.Y1) glyph.visible = false;

        indexLookup[glyph.codepoint] = glyphs.size();
        glyphs.push_back(glyph);
    };

    float getXHeight()
    {
        // requires it be initialised ffirst
        static auto glyph = glyphs[indexLookup['x']];
        return glyph.Y1 - glyph.Y0;
    }

    float getDescent(float pixelSize)
    {
        return fabs(descent) * (pixelSize / (ascent + fabs(descent)));
    }
    float getAscent(float pixelSize)
    {
        return ascent * (pixelSize / (ascent + fabs(descent)));
    }
    float getLineGap(float pixelSize)
    {
        return lineGap * (pixelSize / (ascent + fabs(descent)));
    }
};

NAMESPACE_END(TexGui);
