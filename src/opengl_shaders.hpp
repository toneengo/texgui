#pragma once

#include <string>

namespace TexGui
{

inline const std::string VERSION_HEADER = R"#(
#version 460 core
)#";

inline const std::string BUFFERS = R"#(
struct Character {
    vec4 rect;
    ivec4 texBounds;
    vec4 colour;
    int size;
};

struct Quad {
    vec4 rect;
    ivec4 texBounds;
    vec4 colour;
    int pixelSize;
};

struct ColQuad {
    vec4 rect;
    vec4 col;
    int padding;
};

layout (std140, binding = 0) uniform screenSzBuf {
    ivec2 screenSz;
};

layout (std140, binding = 1) uniform indexBuf {
    int index;
};

)#";


inline const std::string TEXTVERT = R"#(
layout (std430, binding = 0) buffer objectBuf {
    Character text[];
};

out vec2 pos;
out vec2 uv;
flat out vec4 colour;

flat out float screenPxRange;

layout (location = 0) uniform int atlasWidth;
layout (location = 1) uniform int atlasHeight;
layout (location = 2) uniform int pxRange;

void main() {
    ivec2 quad[6] = ivec2[6](
        ivec2(0, 0),
        ivec2(1, 0),
        ivec2(0, 1),
        ivec2(1, 1),
        ivec2(0, 1),
        ivec2(1, 0)
    ); 

    const int FONT_PX = 32;
    vec4 rect = text[index + gl_InstanceID].rect;
    vec4 texBounds = text[index + gl_InstanceID].texBounds;
    rect.x -= screenSz.x / 2;
    rect.y = screenSz.y / 2 - rect.y;

    uv = quad[gl_VertexID];

    uv = vec2((texBounds.x+1)/atlasWidth, (texBounds.y+1)/atlasHeight) +
         uv * (vec2((texBounds.z-1)/atlasWidth, ((texBounds.w-1)/atlasHeight)));

    colour = text[index + gl_InstanceID].colour;

    screenPxRange = max(text[index + gl_InstanceID].size / FONT_PX * pxRange, 1);

    vec2 size = rect.zw / round(vec2(screenSz.x/2.0, screenSz.y/2.0)) / FONT_PX;
    pos = quad[gl_VertexID] * size + rect.xy / round(vec2(screenSz.x/2.0, screenSz.y/2.0));
    gl_Position = vec4(pos, 0.0, 1.0);
}
)#";

inline const std::string TEXTFRAG = R"#(
in vec2 uv;
in vec2 pos;
flat in vec4 colour;
flat in float screenPxRange;
out vec4 frag;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

layout (std140, binding = 2) uniform boundsBuf {
    vec4 bounds;
};

uniform sampler2D font;

bool contains(vec4 box, vec2 p)
{
    return p.x <= box.x + box.z && p.x >= box.x &&
           p.y <= box.y + box.w && p.y >= box.y;
}

void main() {
    if (!contains(bounds, pos))
        discard;

    vec3 msd = texture(font, uv).rgb;
    float sd = median(msd.r, msd.g, msd.b);
    float screenPxDistance = screenPxRange*(sd - 0.5);
    float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0) * colour.a;

    frag = vec4(colour.rgb, opacity);
}
)#";

inline const std::string QUADVERT = R"#(
layout (std430, binding = 0) buffer objectBuf {
    Quad quads[];
};

out vec2 pos;
out vec2 uv;

layout (location = 0) uniform int atlasWidth;
layout (location = 1) uniform int atlasHeight;

const ivec2 quad[6] = ivec2[6](
    ivec2(0, 0),
    ivec2(1, 0),
    ivec2(0, 1),
    ivec2(1, 1),
    ivec2(0, 1),
    ivec2(1, 0)
); 

void main() {
    vec4 rect = quads[index + gl_InstanceID].rect;
    vec4 texBounds = quads[index + gl_InstanceID].texBounds;

    uv = quad[gl_VertexID];

    uv = vec2(texBounds.x / atlasWidth, texBounds.y / atlasHeight) +
         uv * (vec2(texBounds.z / atlasWidth, -texBounds.w / atlasHeight));

    vec2 size = rect.zw / round(vec2(screenSz.x/2.0, screenSz.y/2.0));
    pos = quad[gl_VertexID] * size + rect.xy / round(vec2(screenSz.x/2.0, screenSz.y/2.0));

    gl_Position = vec4(pos, 0.0, 1.0);
}
)#";

inline const std::string QUADFRAG = R"#(
in vec2 uv;
out vec4 frag;
in vec2 pos;

uniform sampler2D sampler;

layout (std140, binding = 2) uniform boundsBuf {
    vec4 bounds;
};

bool contains(vec4 box, vec2 p)
{
    return p.x <= box.x + box.z && p.x >= box.x &&
           p.y <= box.y + box.w && p.y >= box.y;
}

void main() {
    frag = texture(sampler, uv);
    if (!contains(bounds, pos))
        frag.a = 0;
}
)#";

inline const std::string BLUR1 = R"#(
//kawase downscale

out vec4 frag;

in vec2 uv;

uniform vec2 halfpixel;
uniform float radius;

uniform sampler2D image;

void main() {
    vec2 uv2 = uv * 2.0;
    frag = texture(image, uv2) * 4.0;
    frag += texture(image, uv2 - halfpixel.xy * radius);
    frag += texture(image, uv2 + halfpixel.xy * radius);
    frag += texture(image, uv2 + vec2(halfpixel.x, -halfpixel.y) * radius);
    frag += texture(image, uv2 - vec2(halfpixel.x, -halfpixel.y) * radius);

    frag = frag / 8.0;
}
)#";

inline const std::string BLUR2 = R"#(
//kawase upscale

out vec4 frag;

in vec2 uv;

uniform vec2 halfpixel;
uniform float radius;

uniform sampler2D image;

void main() {
    vec2 uv2 = uv / 2.0;
    frag = texture(image, uv2 + vec2(-halfpixel.x * 2.0, 0.0) * radius);
    frag += texture(image, uv2 + vec2(-halfpixel.x, halfpixel.y) * radius) * 2.0;
    frag += texture(image, uv2 + vec2(0.0, halfpixel.y * 2.0) * radius);
    frag += texture(image, uv2 + vec2(halfpixel.x, halfpixel.y) * radius) * 2.0;
    frag += texture(image, uv2 + vec2(halfpixel.x * 2.0, 0.0) * radius);
    frag += texture(image, uv2 + vec2(halfpixel.x, -halfpixel.y) * radius) * 2.0;
    frag += texture(image, uv2 + vec2(0.0, -halfpixel.y * 2.0) * radius);
    frag += texture(image, uv2 + vec2(-halfpixel.x, -halfpixel.y) * radius) * 2.0;

    frag = frag / 12.0;
}
)#";

}
