#pragma once

#include "common.h"
#include "context.h"
#include "defaults.h"
#include <array>

#include <cstring>
NAMESPACE_BEGIN(TexGui);

bool initGlfwOpenGL(GLFWwindow* window);

class Container
{
public:
    RenderData* rs;
    Math::fbox bounds;

    inline Container withBounds(Math::fbox bounds) const
    {
        Container copy = *this;
        copy.bounds = bounds;
        return copy;
    }

    Container Window(const char* name, float xpos, float ypos, float width, float height, uint32_t flags = 0);
    bool      Button(const char* text);
    Container Box(float xpos, float ypos, float width, float height, const char* texture = nullptr);
    void      TextInput(const char* name, std::string& buf);

    template <uint32_t N>
    std::array<Container, N> Row(const float (&widths)[N], float height = 0)
    {
        std::array<Container, N> out;
        Row_Internal(&out[0], widths, N, height);
        return out;
    }

    template <uint32_t N>
    std::array<Container, N> Column(const float (&heights)[N], float width = 0)
    {
        std::array<Container, N> out;
        Column_Internal(&out[0], heights, N, width);
        return out;
    }
private:
    void Row_Internal(Container* out, const float* widths, uint32_t n, float height);
    void Column_Internal(Container* out, const float* widths, uint32_t n, float height);
    std::unordered_map<std::string, uint32_t>* buttonStates;
};

inline Container Base;

void render();
void loadFont(const char* font);
void loadTextures(const char* dir);
void clear();

NAMESPACE_END(TexGui);
