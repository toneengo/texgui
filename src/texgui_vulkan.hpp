#pragma once

#include "texgui.h"
#include "common.h"
#include "context.hpp"
#include "types.h"
#include <vulkan/vulkan_core.h>

NAMESPACE_BEGIN(TexGui);

//ONLY DYNAMIC RENDERING IS SUPPORTED
//This is copied from Dear ImGui
struct VulkanInitInfo
{
    uint32_t                        ApiVersion;                 // Fill with API version of Instance, e.g. VK_API_VERSION_1_3 or your value of VkApplicationInfo::apiVersion. May be lower than header version (VK_HEADER_VERSION_COMPLETE)
    VkInstance                      Instance;
    VkPhysicalDevice                PhysicalDevice;
    VkDevice                        Device;
    uint32_t                        QueueFamily;
    VkQueue                         Queue;
    VkDescriptorPool                DescriptorPool;             // See requirements in note above; ignored if using DescriptorPoolSize > 0
    //VkRenderPass                    RenderPass;                 // Ignored if using dynamic rendering
    uint32_t                        MinImageCount;              // >= 2
    uint32_t                        ImageCount;                 // >= MinImageCount
    VkSampleCountFlagBits           MSAASamples;                // 0 defaults to VK_SAMPLE_COUNT_1_BIT

    // (Optional)
    //VkPipelineCache                 PipelineCache;
    //uint32_t                        Subpass;

    // (Optional) Set to create internal descriptor pool instead of using DescriptorPool
    uint32_t                        DescriptorPoolSize;

    // (Optional) Dynamic Rendering
    // Need to explicitly enable VK_KHR_dynamic_rendering extension to use this, even for Vulkan 1.3.
    bool                            UseDynamicRendering;
#ifdef IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
    VkPipelineRenderingCreateInfoKHR PipelineRenderingCreateInfo;
#endif

    // (Optional) Allocation, Debugging
    const VkAllocationCallbacks*    Allocator;
    void                            (*CheckVkResultFn)(VkResult err);
    VkDeviceSize                    MinAllocationSize;          // Minimum allocation size. Set to 1024*1024 to satisfy zealous best practices validation layer and waste a little memory.
};

class VulkanContext : public NoApiContext
{
public:
    VulkanContext(const VulkanInitInfo& info);
    ~VulkanContext();

    void initFromGlfwWindow(GLFWwindow* window);
    void setScreenSize(int width, int height);
    void loadFont(const char* pathToFont);
    void loadTextures(const char* dir);
    void renderFromRD(RenderData& data);
    IconSheet loadIcons(const char* path, int32_t iconWidth, int32_t iconHeight);

protected:
    VkDevice                    device;
    VkPhysicalDevice            physicalDevice;

    VkInstance                  instance;
    VkDebugUtilsMessengerEXT    debugMessenger;
    VkQueue                     graphicsQueue;
    uint32_t                    graphicsQueueFamily;

};

NAMESPACE_END(TexGui);
