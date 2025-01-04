#pragma once

#include "glad/gl.h"
#include "GLFW/glfw3.h"
#include "common.h"
#include "shader.h"
#include "types.h"
#include <unordered_map>
#include <vector>

NAMESPACE_BEGIN(TexGui);

class GLContext
{
friend class Screen;
public:
    GLContext(GLFWwindow* window);
    ~GLContext();
    void bindBuffers();
    void setWidgetPos(Math::fvec2 pos);
    Math::fvec2 getWidgetPos() { return m_widget_pos; };

    void setScreenSize(int width, int height);
    Math::ivec2 getScreenSize() { return m_screen_size; };

    void loadFont(const char* font);
    void preloadTextures(const char* dir);

    int drawText(const char* text, Math::fvec2 pos, const Math::fvec4& col, float scale, uint32_t flags, float width = 0);
    void drawTexture(const Math::fbox& rect, TexEntry* e, int state, int pixel_size, uint32_t flags);
    void drawQuad(const Math::fbox& rect, const Math::fvec4& col);

    void draw();
    Math::fvec2 m_widget_pos;
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

    float fontPx;
    Math::ivec2 m_screen_size;

    float m_window_scale;

    int m_pixel_size = 2;

    int m_font_height;
    int m_line_height;

};

NAMESPACE_END(TexGui);
