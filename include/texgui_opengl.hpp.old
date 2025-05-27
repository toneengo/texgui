#pragma once

#ifndef __gl_h_
#include "glad/gl.h"
#endif
#include "texgui.h"
#include "context.hpp"
#include "types.h"

NAMESPACE_BEGIN(TexGui);

struct Shader
{
    uint32_t id;

    void use();
};

void createShader(Shader* shader, const std::string& vertexShader, const std::string& fragmentShader);

class GLContext : public NoApiContext
{
public:
    GLContext();
    ~GLContext();

    void bindBuffers();
    void setScreenSize(int width, int height);
    void renderFromRD(const RenderData& data);
    uint32_t createTexture(void* data, int width, int height);
    Math::ivec2 getTextureSize(uint32_t texID);
    void clean();
    void newFrame() {};

protected:
    void ogl_renderFromRD(const auto& objects, const auto& commands);
    // Buffer name/Buffer binding pair
    struct nameIdx
    {
        uint32_t buf;
        uint32_t bind;
    };

    // Uniform buffers
    struct
    {
        nameIdx screen_size;
        nameIdx objIndex;
        nameIdx bounds;
    } m_ub;

    // Shader storage buffers
    struct
    {
        nameIdx text;
        nameIdx quad;
        nameIdx colquad;
        nameIdx objects;
    } m_ssb;

    struct
    {
        Shader quad;
        Shader colquad;
        Shader quad9slice;
        Shader text;
    } m_shaders;

    Math::ivec2 m_screen_size;
    float m_window_scale;
    int m_pixel_size = 2;
};

bool initOpenGL();
void renderOpenGL(const RenderData& data);
NAMESPACE_END(TexGui);
