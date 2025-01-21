#pragma once

#include "../texgui.h"
#include "common.h"
#include <stdint.h>
#include <vector>

NAMESPACE_BEGIN(TexGui);
// Renderable objects
struct alignas(16) Character
{
    Math::fbox rect; //xpos, ypos, width, height
    Math::ibox texBounds;
    int layer;
    int size;
};

struct alignas(16) Quad
{
    Math::fbox rect; //xpos, ypos, width, height
    Math::ibox texBounds; //xpos, ypos, width, height
    int layer;
    int pixelSize;
};

struct alignas(16) ColQuad
{
    Math::fbox rect; //xpos, ypos, width, height
    Math::fvec4 col;
    int padding;
};

struct alignas(16) Object
{
    Object()
    {
    }

    Object(const Object& o)
    {
        *this = o;
    }

    Object(Character c)
    {
        ch = c;
    }
    Object(Quad q)
    {
        quad = q;
    }

    Object(ColQuad q)
    {
        cq = q;
    }
    union {
        Character ch;
        Quad quad;
        ColQuad cq;
    };
};

struct TexEntry;
struct Command
{
    enum {
        QUAD,
        CHARACTER,
        COLQUAD
    } type;

    uint32_t number;
    uint32_t flags = 0;
    TexEntry * texentry;
};

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
    int layer;

    unsigned int has_state = 0;

    float top;
    float right;
    float bottom;
    float left;

    Math::ibox bounds;

    unsigned char* data;
    unsigned char* _hover;
    unsigned char* _press;
    unsigned char* _active;
};

inline int line_height = 0;
inline int font_height = 0;
inline float font_px = 0;

struct RenderData
{
    std::vector<Object> objects;
    std::vector<Command> commands;

    Math::fvec2 m_widget_pos = Math::fvec2(0);

    void drawQuad(const Math::fbox& rect, const Math::fvec4& col);
    void drawTexture(const Math::fbox& rect, TexEntry* e, int state, int pixel_size, uint32_t flags);
    int drawText(const char* text, Math::fvec2 pos, const Math::fvec4& col, int size, uint32_t flags, float width = 0);

    void clear() {
        objects.clear();
        commands.clear();
    }
};

NAMESPACE_END(TexGui);
