#version 450 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

layout( push_constant ) uniform constants
{	
    vec2 scale;
    vec2 translate;
    uint texID;
    float pxRange;
    vec2 uvScale;
    vec4 textBorderColor;
} pushConstants;

out gl_PerVertex { vec4 gl_Position; };
layout(location = 0) out struct { vec4 Color; vec2 UV; } Out;
layout(location = 2) flat out uint texID;
layout(location = 3) flat out float pxRange;
layout(location = 4) flat out vec4 textBorderColor;

void main()
{
    Out.Color = aColor;
    Out.UV = aUV * pushConstants.uvScale;
    texID = pushConstants.texID;
    pxRange = pushConstants.pxRange;
    textBorderColor = pushConstants.textBorderColor;
    gl_Position = vec4(aPos * pushConstants.scale + pushConstants.translate, 0, 1);
}
