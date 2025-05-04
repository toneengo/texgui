#pragma once
#include "texgui.h"

NAMESPACE_BEGIN(TexGui)
struct VulkanInitInfo;
class NoApiContext
{
    public:
        virtual void initFromGlfwWindow(GLFWwindow* window) = 0;
        virtual void setScreenSize(int width, int height) = 0;
        virtual void renderFromRD(const RenderData& data) = 0;
        virtual uint32_t createTexture(void* data, int width, int height) = 0;
        virtual void setFontAtlas(uint32_t texID, int width, int height)
            {fontAtlasID=texID; fontAtlasWidth=width; fontAtlasHeight=height;}

        virtual Math::ivec2 getTextureSize(uint32_t texID) = 0;
    protected:
        uint32_t fontAtlasID;
        uint32_t fontAtlasWidth;
        uint32_t fontAtlasHeight;
};

NAMESPACE_END(TexGui)
