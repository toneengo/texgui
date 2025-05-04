#pragma once
#include "texgui.h"

NAMESPACE_BEGIN(TexGui)
struct VulkanInitInfo;
class NoApiContext
{
    public:
        virtual void initFromGlfwWindow(GLFWwindow* window) = 0;
        virtual void loadFont(const char* pathToFont) = 0;
        virtual void loadTextures(const char* dir) = 0;
        virtual IconSheet loadIcons(const char* path, int32_t iconWidth, int32_t iconHeight) = 0;
        virtual void setScreenSize(int width, int height) = 0;
        virtual void renderFromRD(RenderData& data) = 0;
};

NAMESPACE_END(TexGui)
