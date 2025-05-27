#pragma once

#include "texgui.h"
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

NAMESPACE_BEGIN(TexGui);

struct Texture;
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
    VkPipelineCache                 PipelineCache;
    //uint32_t                        Subpass;
    // Render passes are not supported.

    // (Optional) Set to create internal descriptor pool instead of using DescriptorPool
    uint32_t                        DescriptorPoolSize;

    // (Optional) Dynamic Rendering
    // Need to explicitly enable VK_KHR_dynamic_rendering extension to use this, even for Vulkan 1.3.
    bool                            UseDynamicRendering;
    VkPipelineRenderingCreateInfo PipelineRenderingCreateInfo;

    // (Optional) Allocation, Debugging
    VkAllocationCallbacks*    allocationCallbacks = nullptr;
    void                            (*CheckVkResultFn)(VkResult err) = nullptr;
    VkDeviceSize                    MinAllocationSize = 1024*1024;          // Minimum allocation size. Set to 1024*1024 to satisfy zealous best practices validation layer and waste a little memory.

    //#TODO: GET rid of this
    VmaAllocator Allocator;
};

bool initVulkan(TexGui::VulkanInitInfo& info);
void renderFromRenderData_Vulkan(VkCommandBuffer cmd, const RenderData& data);
Texture* customTexture(VkImageView imageView, Math::ibox bounds, Math::ivec2 atlasSize = {0, 0});
NAMESPACE_END(TexGui);
