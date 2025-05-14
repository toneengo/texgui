#ifndef __gl_h_
#include "glad/gl.h"
#endif
#include "GLFW/glfw3.h"

#include "texgui.h"
#include "texgui_opengl.hpp"
#include "util.h"

#include <glm/gtc/type_ptr.hpp>
#include "opengl_shaders.hpp"

#include "stb_image.h"

using namespace TexGui;
using namespace TexGui::Math;

constexpr int ATLAS_SIZE = 1024;

GLContext::GLContext()
{
    glEnable(GL_MULTISAMPLE);

    glCreateBuffers(1, &m_ssb.objects.buf);
    glNamedBufferStorage(m_ssb.objects.buf, (1 << 16) / 16 * sizeof(RenderData::Object), nullptr, GL_DYNAMIC_STORAGE_BIT);
    m_ssb.objects.bind = 0;

    glCreateBuffers(1, &m_ub.objIndex.buf);
    glNamedBufferStorage(m_ub.objIndex.buf, sizeof(int), nullptr, GL_DYNAMIC_STORAGE_BIT);
    m_ub.objIndex.bind = 1;

    glCreateBuffers(1, &m_ub.screen_size.buf);
    glNamedBufferStorage(m_ub.screen_size.buf, sizeof(int) * 2, &m_screen_size, GL_DYNAMIC_STORAGE_BIT);
    m_ub.screen_size.bind = 0;

    glCreateBuffers(1, &m_ub.bounds.buf);
    glNamedBufferStorage(m_ub.bounds.buf, sizeof(Math::fbox), nullptr, GL_DYNAMIC_STORAGE_BIT);
    m_ub.bounds.bind = 2;

    createShader(&m_shaders.text,
                 VERSION_HEADER + BUFFERS + TEXTVERT,
                 VERSION_HEADER + TEXTFRAG);

    createShader(&m_shaders.quad,
                 VERSION_HEADER + BUFFERS + QUADVERT,
                 VERSION_HEADER + QUADFRAG);

    glActiveTexture(GL_TEXTURE0);
}

extern Container Base;
void GLContext::initFromGlfwWindow(GLFWwindow* window)
{
    glfwGetWindowContentScale(window, &m_window_scale, nullptr);
    glfwGetWindowSize(window, &m_screen_size.x, &m_screen_size.y);
    glScissor(0,0,m_screen_size.x, m_screen_size.y);
    m_screen_size.x *= m_window_scale;
    m_screen_size.y *= m_window_scale; 
    Base.bounds.size = m_screen_size;
    glViewport(0, 0, m_screen_size.x, m_screen_size.y);
    glNamedBufferSubData(m_ub.screen_size.buf, 0, sizeof(int) * 2, &m_screen_size);
}

GLContext::~GLContext()
{
}

void GLContext::bindBuffers()
{
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, m_ssb.objects.bind, m_ssb.objects.buf);
    glBindBufferBase(GL_UNIFORM_BUFFER, m_ub.screen_size.bind, m_ub.screen_size.buf);
    glBindBufferBase(GL_UNIFORM_BUFFER, m_ub.objIndex.bind, m_ub.objIndex.buf);
    glBindBufferBase(GL_UNIFORM_BUFFER, m_ub.bounds.bind, m_ub.bounds.buf);
}

void GLContext::setScreenSize(int width, int height)
{
    m_screen_size.x = width;
    m_screen_size.y = height;
    glViewport(0, 0, width, height);
    glNamedBufferSubData(m_ub.screen_size.buf, 0, sizeof(int) * 2, &m_screen_size);
}

void GLContext::ogl_renderFromRD(const auto& objects, const auto& commands) {
    glNamedBufferSubData(m_ssb.objects.buf, 0, sizeof(RenderData::Object) * objects.size(), objects.data());

    int count = 0;
    glActiveTexture(GL_TEXTURE0);

    for (auto& c : commands)
    {
        glNamedBufferSubData(m_ub.objIndex.buf, 0, sizeof(int), &count);
        switch (c.type)
        {
            case RenderData::Command::QUAD:
            {
                fbox sb = c.scissorBox;
                sb.x -= m_screen_size.x / 2.f;
                sb.y = m_screen_size.y / 2.f - c.scissorBox.y - c.scissorBox.height;
                sb.pos /= vec2(m_screen_size.x/2.f, m_screen_size.y/2.f);
                sb.size = c.scissorBox.size / vec2(m_screen_size.x/2.f, m_screen_size.y/2.f);

                glNamedBufferSubData(m_ub.bounds.buf, 0, sizeof(Math::fbox), &sb);

                m_shaders.quad.use();

                int texID = c.texture->id;
                if (getBit(c.state, STATE_PRESS) && c.texture->press != -1)
                    texID = c.texture->press;
                if (getBit(c.state, STATE_HOVER) && c.texture->hover != -1)
                    texID = c.texture->hover;
                if (getBit(c.state, STATE_ACTIVE) && c.texture->active != -1)
                    texID = c.texture->active;

                glBindTexture(GL_TEXTURE_2D, texID);

                glUniform1i(0, c.texture->size.x);
                glUniform1i(1, c.texture->size.y);

                glDrawArraysInstanced(GL_TRIANGLES, 0, 6, c.number);
                break;
            }
            case RenderData::Command::CHARACTER:
            {
                m_shaders.text.use();
                glBindTexture(GL_TEXTURE_2D, fontAtlasID);
                glUniform1i(0, fontAtlasWidth);
                glUniform1i(1, fontAtlasHeight);
                glUniform1i(2, Defaults::Font::MsdfPxRange);
                glDrawArraysInstanced(GL_TRIANGLES, 0, 6, c.number);
                break;
            }
            default:
                break;
        }
        count += c.number;
    }

}
void GLContext::renderFromRD(const RenderData& data) {
    bindBuffers();

    // Later we can do better Z ordering or something. Idk what the best approach will be without having to sort everything :thinking:
    ogl_renderFromRD(data.objects, data.commands);
    ogl_renderFromRD(data.objects2, data.commands2);
}
    
int tc(int x, int y)
{
    return x + (y * ATLAS_SIZE);
}

uint32_t GLContext::createTexture(void* data, int width, int height)
{
    GLuint newTex;
    glGenTextures(1, &newTex);
        
    glBindTexture(GL_TEXTURE_2D, newTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
    return newTex;
}

Math::ivec2 GLContext::getTextureSize(uint32_t texID)
{
    ivec2 res;
    glBindTexture(GL_TEXTURE_2D, texID);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &res.x);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &res.y);
    return res;
}

void GLContext::clean()
{
}

void Shader::use()
{
    glUseProgram(id);
}

void TexGui::createShader(Shader* shader, const std::string& vertexShader, const std::string& fragmentShader)
{
    GLuint vsh = glCreateShader(GL_VERTEX_SHADER);
    GLuint fsh = glCreateShader(GL_FRAGMENT_SHADER);

    auto v_shaderSource = vertexShader.c_str();
    auto f_shaderSource = fragmentShader.c_str();

    glShaderSource(vsh, 1, (const GLchar**)&v_shaderSource, NULL);
    glShaderSource(fsh, 1, (const GLchar**)&f_shaderSource, NULL);

    int logLength;
    glCompileShader(vsh);
    glGetShaderiv(vsh, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0)
    {
        char* vsherr = new char[logLength + 1];
        glGetShaderInfoLog(vsh, logLength, NULL, vsherr);
        printf("\n\nIn %s:\n%s\n", v_shaderSource, vsherr);
        delete[] vsherr;
        exit(1);
    }

    logLength = 0;
    glCompileShader(fsh);
    glGetShaderiv(fsh, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0)
    {
        char* fsherr = new char[logLength + 1];
        glGetShaderInfoLog(fsh, logLength, NULL, fsherr);
        printf("\n\nIn %s:\n%s\n", f_shaderSource, fsherr);
        delete[] fsherr;
        exit(1);
    }

    glCompileShader(vsh);
    glCompileShader(fsh);
    shader->id = glCreateProgram();

    glAttachShader(shader->id, vsh);
    glAttachShader(shader->id, fsh);
    glLinkProgram(shader->id);

    int success = GL_FALSE;
    glGetProgramiv(shader->id, GL_LINK_STATUS, &success);
    if (!success)
    {
        int logLength = 0;
        glGetProgramiv(shader->id, GL_INFO_LOG_LENGTH, &logLength);
        char* err = new char[logLength + 1];
        glGetProgramInfoLog(shader->id, 512, NULL, err);
        printf("Error compiling program:\n%s\n", err);
        delete[] err;
        exit(1);
    }

    // Shaders have been linked into the program, we don't need them anymore
    glDeleteShader(vsh);
    glDeleteShader(fsh);
}

extern void _setRenderCtx(NoApiContext* ptr);
bool TexGui::initOpenGL()
{
    _setRenderCtx(new GLContext());
    return true;
} 
