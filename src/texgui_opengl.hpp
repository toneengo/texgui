#pragma once

#ifndef __gl_h_
#include "glad/gl.h"
#endif
#include "texgui.h"
#include "common.h"
#include "context.hpp"
#include "types.h"

NAMESPACE_BEGIN(TexGui);

struct uniformInfo
{ 
    char * key;

    struct Value
    {
	    int32_t location;
	    uint32_t count;
    };
    Value value;
};

struct Shader
{
    uint32_t id;
    uniformInfo * uniforms;

    void use();
    uint32_t getLocation(const char * uniform_name);

    uint32_t fontPx = -1;
    int slices;
};

void createShader(Shader* shader, const std::string& vertexShader, const std::string& fragmentShader);

class GLContext : public NoApiContext
{
public:
    GLContext();
    ~GLContext();

    void initFromGlfwWindow(GLFWwindow* window);
    void bindBuffers();
    void setScreenSize(int width, int height);
    void loadFont(const char* pathToFont);
    void loadTextures(const char* dir);
    void renderFromRD(RenderData& data);
    IconSheet loadIcons(const char* path, int32_t iconWidth, int32_t iconHeight);

protected:
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

    // Texture arrays
    struct
    {
        nameIdx texture;
        nameIdx font;
    } m_ta;

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

NAMESPACE_END(TexGui);
