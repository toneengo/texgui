#include "types.h"
#include "GLFW/glfw3.h"

#include "texgui.h"
#include "texgui_vulkan.hpp"
#include "util.h"

#include <filesystem>

#include <glm/gtc/type_ptr.hpp>
#include "shaders/texgui_shaders.hpp"
#include "msdf-atlas-gen/msdf-atlas-gen.h"

#include "stb_image.h"

#include <iostream>

using namespace TexGui;
using namespace TexGui::Math;

VulkanContext::VulkanContext(const VulkanInitInfo& info)
{
}

VulkanContext::~VulkanContext()
{
}

void VulkanContext::initFromGlfwWindow(GLFWwindow* window)
{
}

void VulkanContext::setScreenSize(int width, int height)
{
}

void VulkanContext::renderFromRD(RenderData& data)
{
}
