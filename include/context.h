#pragma once

#ifndef __gl_h_
#include "glad/gl.h"
#endif
//#include "GLFW/glfw3.h"
#include "common.h"
#include "shader.h"
#include "types.h"
#include <unordered_map>
#include <vector>


struct GLFWwindow;
NAMESPACE_BEGIN(TexGui);

class GLContext
{
friend class Screen;
public:
    GLContext(GLFWwindow* window);
    ~GLContext();
    void bindBuffers();

    void setScreenSize(int width, int height);
    Math::ivec2 getScreenSize() { return m_screen_size; };

    void loadFont(const char* font);
    void preloadTextures(const char* dir);

    void render(const RenderState& state);
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
        nameIdx widget_pos;
        nameIdx objIndex;
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

    std::vector<Command> m_vCommands;
    std::vector<Object> m_vObjects;
    Math::ivec2 m_screen_size;
    float m_window_scale;
    int m_pixel_size = 2;
};

NAMESPACE_END(TexGui);
