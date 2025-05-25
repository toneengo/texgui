
#pragma once

#include <string>

namespace TexGui
{

inline const std::string VK_VERSION_HEADER = R"#(
#version 460
)#";

inline const std::string VK_BUFFERS = R"#(
struct Character {
    vec4 rect;
    vec4 texBounds;
    vec4 colour;
    int size;
    int padding[3];
};

struct Quad {
    vec4 rect;
    vec4 texBounds;
    vec4 colour;
    int pixelSize;
    int padding[3];
};

layout (set = 1, binding = 0) uniform screenSzBuf {
    ivec2 screenSz;
};
)#";

inline const std::string VK_QUADVERT = VK_VERSION_HEADER + VK_BUFFERS + R"#(
layout (std430, set = 0, binding = 0) readonly buffer objectBuf {
    Quad quads[];
};

layout (location = 0) out vec2 uv;
layout (location = 1) out vec2 pos;

const ivec2 quad[6] = ivec2[6](
    ivec2(0, 0),
    ivec2(1, 0),
    ivec2(0, 1),
    ivec2(1, 1),
    ivec2(0, 1),
    ivec2(1, 0)
); 

//push constants block
layout( push_constant ) uniform constants
{	
    vec4 bounds;
    int index;
    int texID;
    int atlasWidth;
    int atlasHeight;
} pushConstants;

void main() {
    vec4 rect = quads[pushConstants.index + gl_InstanceIndex].rect;
    vec4 texBounds = quads[pushConstants.index + gl_InstanceIndex].texBounds;

    uv = quad[gl_VertexIndex];

    uv = vec2(texBounds.x / pushConstants.atlasWidth, texBounds.y / pushConstants.atlasHeight) +
         uv * (vec2(texBounds.z / pushConstants.atlasWidth, -texBounds.w / pushConstants.atlasHeight));

    vec2 size = rect.zw / round(vec2(screenSz.x/2.0, screenSz.y/2.0));
    pos = quad[gl_VertexIndex] * size + rect.xy / round(vec2(screenSz.x/2.0, screenSz.y/2.0));
    pos.y *= -1;

    gl_Position = vec4(pos, 0.0, 1.0);
}
)#";

inline const std::string VK_QUADFRAG = R"#(
#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) out vec4 frag;

layout (location = 0) in vec2 uv;
layout (location = 1) in vec2 pos;

layout (set = 3, binding = 0) uniform sampler2D samplers[];

layout( push_constant ) uniform constants
{	
    vec4 bounds;
    int index;
    int texID;
    int atlasWidth;
    int atlasHeight;
} pushConstants;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

bool contains(vec4 box, vec2 p)
{
    return p.x <= box.x + box.z && p.x >= box.x &&
           p.y >= box.y - box.w && p.y <= box.y;
}

void main() {
    frag = texture(samplers[nonuniformEXT(pushConstants.texID)], uv);
    if (!contains(pushConstants.bounds, pos) || frag.a < 0.1)
        frag.a = 0.0;
}
)#";

inline const std::string VK_TEXTVERT = VK_VERSION_HEADER + VK_BUFFERS + R"#(
layout (std430, set = 0, binding = 0) readonly buffer objectBuf {
    Character text[];
};

layout (location = 0) out vec2 uv;
layout (location = 1) out vec2 pos;
layout (location = 2) flat out vec4 colour;
layout (location = 3) flat out float screenPxRange;

const ivec2 quad[6] = ivec2[6](
    ivec2(0, 0),
    ivec2(1, 0),
    ivec2(0, 1),
    ivec2(1, 1),
    ivec2(0, 1),
    ivec2(1, 0)
); 

layout (set = 2, binding = 0) uniform ScreenPxRange {
    float pxRange;
};

layout( push_constant ) uniform constants
{	
    vec4 bounds;
    int index;
    int texID;
    int atlasWidth;
    int atlasHeight;
} pushConstants;

void main() {
    const int FONT_PX = 100;
    vec4 rect = text[pushConstants.index + gl_InstanceIndex].rect;
    colour = text[pushConstants.index + gl_InstanceIndex].colour;
    vec4 texBounds = text[pushConstants.index + gl_InstanceIndex].texBounds;

    rect.x -= screenSz.x / 2;
    rect.y = screenSz.y / 2 - rect.y;

    uv = quad[gl_VertexIndex];

    uv = vec2(texBounds.x / pushConstants.atlasWidth, texBounds.y / pushConstants.atlasHeight) +
         uv * (vec2(texBounds.z / pushConstants.atlasWidth, texBounds.w / pushConstants.atlasHeight));

    vec2 size = rect.zw / round(vec2(screenSz.x/2.0, screenSz.y/2.0));
    pos = quad[gl_VertexIndex] * size + rect.xy / round(vec2(screenSz.x/2.0, screenSz.y/2.0));
    pos.y *= -1;

    screenPxRange = max(text[pushConstants.index + gl_InstanceIndex].size / FONT_PX * pxRange, 1);

    gl_Position = vec4(pos, 0.0, 1.0);
}
)#";

inline const std::string VK_TEXTFRAG = R"#(
#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) out vec4 frag;

layout (location = 0) in vec2 uv;
layout (location = 1) in vec2 pos;
layout (location = 2) flat in vec4 colour;
layout (location = 3) flat in float screenPxRange;

layout (set = 3, binding = 0) uniform sampler2D samplers[];

layout( push_constant ) uniform constants
{	
    vec4 bounds;
    int index;
    int texID;
    int atlasWidth;
    int atlasHeight;
} pushConstants;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

bool contains(vec4 box, vec2 p)
{
    return p.x <= box.x + box.z && p.x >= box.x &&
           p.y >= box.y - box.w && p.y <= box.y;
}

void main() {
    vec4 msd = texture(samplers[nonuniformEXT(pushConstants.texID)], uv);
    float sd = median(msd.r, msd.g, msd.b);
    float screenPxDistance = screenPxRange * (sd - 0.5);
    float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0) * colour.a;
    frag = vec4(colour.rgb, opacity);
    if (!contains(pushConstants.bounds, pos))
        frag.a = 0.0;
}
)#";

}
