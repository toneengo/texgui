#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#ifndef __gl_h_
#include "glad/gl.h"
#endif

#include "shader.h"

using namespace TexGui;

void Shader::use()
{
    glUseProgram(id);
}

uint32_t Shader::getLocation(const char * uniform_name)
{
    return shgets(uniforms, uniform_name).value.location;
}

void TexGui::createShader(Shader* shader, const std::string& vertexShader, const std::string& fragmentShader)
{
    sh_new_arena(shader->uniforms);

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

    GLint uniform_count;
    GLchar * uniform_name;
    glGetProgramiv(shader->id, GL_ACTIVE_UNIFORMS, &uniform_count);

    if (uniform_count > 0)
    {
        GLint   max_name_len;
        GLsizei length = 0;
        GLsizei count = 0;
        GLenum  type = GL_NONE;

        glGetProgramiv(shader->id, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_name_len);

        uniform_name = new char[max_name_len];

        for (GLint i = 0; i < uniform_count; i++)
        {
            glGetActiveUniform(shader->id, i, max_name_len, &length, &count, &type, uniform_name);
            uniformInfo::Value info{
                glGetUniformLocation(shader->id, uniform_name),
                uint32_t(count)
            };
            shput(shader->uniforms, uniform_name, info);
        }
        delete[] uniform_name;
    }
}
