#pragma once

#include "texgui.h"
#include "context.hpp"
#include "types.h"
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

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
    VkPipelineCache                 PipelineCache;
    uint32_t                        Subpass;

    // (Optional) Set to create internal descriptor pool instead of using DescriptorPool
    uint32_t                        DescriptorPoolSize;

    // (Optional) Dynamic Rendering
    // Need to explicitly enable VK_KHR_dynamic_rendering extension to use this, even for Vulkan 1.3.
    bool                            UseDynamicRendering;
    VkPipelineRenderingCreateInfo PipelineRenderingCreateInfo;

    // (Optional) Allocation, Debugging
    /*
    const VkAllocationCallbacks*    Allocator;
    void                            (*CheckVkResultFn)(VkResult err);
    VkDeviceSize                    MinAllocationSize;          // Minimum allocation size. Set to 1024*1024 to satisfy zealous best practices validation layer and waste a little memory.
                                                                // */

    //#TODO: GET rid of this
    VmaAllocator Allocator;
};

class VulkanContext : public NoApiContext
{
public:
    VulkanContext(const VulkanInitInfo& info);
    ~VulkanContext();

    void setScreenSize(int width, int height);
    void renderFromRD(const RenderData& data);
    uint32_t createTexture(void* data, int width, int height);
    Math::ivec2 getTextureSize(uint32_t texID);
    void setPxRange(float _pxRange);
    void clean();
    void newFrame();

    uint32_t createTexture(VkImageView imageView, Math::fbox bounds);
protected:

    struct Buf {
        VkBuffer buffer;
        VmaAllocation allocation;
        VmaAllocationInfo info;
    } buffer;
    uint32_t textureCount = 0;
    VkShaderModule createShaderModule(int stage, const std::string& filePath);
    VkDevice                    device;
    VkPhysicalDevice            physicalDevice;

    VkInstance                  instance;
    VkDebugUtilsMessengerEXT    debugMessenger;
    VkQueue                     graphicsQueue;
    uint32_t                    graphicsQueueFamily;

    //command buffer for immediate commands
    VkCommandPool   immCommandPool;
    VkCommandBuffer immCommandBuffer;
    VkFence         immCommandFence;

    VkSampler linearSampler;
    VkSampler nearestSampler;

    VmaAllocator allocator;
    bool needResize = true;
    float windowScale;
    Math::ivec2 windowSize;

    VkDescriptorPool globalDescriptorPool;

    uint32_t imageCount;
    std::vector<VkDescriptorPool> frameDescriptorPools;
    uint32_t currentFrame = 0;

    VkDescriptorSetLayout windowSizeDescriptorSetLayout;

    VkDescriptorSet pxRangeDescriptorSet;
    VkDescriptorSetLayout pxRangeDescriptorSetLayout;
    VkBuffer pxRangeBuffer = 0;
    VmaAllocation pxRangeBufferAllocation = 0;

    VkDescriptorSet imageDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSetLayout imageDescriptorSetLayout;
    VkBuffer imageBuffer = 0;
    VmaAllocation imageBufferAllocation = 0;

    VkDescriptorSetLayout storageDescriptorSetLayout;

    VkPipeline quadPipeline;
    VkPipeline textPipeline;
    VkPipelineLayout quadPipelineLayout = VK_NULL_HANDLE;

};

bool initVulkan(TexGui::VulkanInitInfo& info);
void renderVulkan(const TexGui::RenderData& data, VkCommandBuffer cmd);

Texture* customTexture(VkImageView imageView, Math::ibox bounds, Math::ivec2 atlasSize = {0, 0});
NAMESPACE_END(TexGui);
