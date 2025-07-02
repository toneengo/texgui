#version 450 core
layout(location = 0) out vec4 fColor;
layout(location = 0) in struct { vec4 Color; vec2 UV; } In;
layout(location = 2) flat in uint texID;
layout(location = 3) flat in float pxRange;

layout (set = 0, binding = 0) uniform sampler2D tex;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main()
{
    fColor = texture(tex, In.UV.st);

    if (pxRange > 0.9)
    {
        float sd = median(fColor.r, fColor.g, fColor.b);
        float screenPxDistance = pxRange * (sd - 0.5);
        fColor.a = clamp(screenPxDistance + 0.5, 0.0, 1.0) * fColor.a;
        fColor.rgb = vec3(1.0);
    }

    fColor *= In.Color;
}
