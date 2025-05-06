#pragma once

#include "texgui.h"
#include "context.hpp"
#include "types.h"
#include <vulkan/vulkan_core.h>

NAMESPACE_BEGIN(TexGui);

class VulkanContext : public NoApiContext
{
public:
    VulkanContext(const VulkanInitInfo& info);
    ~VulkanContext();

    void initFromGlfwWindow(GLFWwindow* window);
    void setScreenSize(int width, int height);
    void renderFromRD(const RenderData& data);
    uint32_t createTexture(void* data, int width, int height);
    Math::ivec2 getTextureSize(uint32_t texID);
    void clean();
    void newFrame();

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
    VkDescriptorPool descriptorPool;

    VkDescriptorSet windowSizeDescriptorSet;
    VkDescriptorSetLayout windowSizeDescriptorSetLayout;
    VkBuffer windowSizeBuffer = 0;
    VmaAllocation windowSizeBufferAllocation = 0;

    VkDescriptorSet imageDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSetLayout imageDescriptorSetLayout;
    VkBuffer imageBuffer = 0;
    VmaAllocation imageBufferAllocation = 0;

    VkDescriptorSet storageDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSetLayout storageDescriptorSetLayout;
    VkBuffer storageBuffer = 0;
    VmaAllocation storageBufferAllocation = 0;

    VkPipeline quadPipeline;
    VkPipelineLayout quadPipelineLayout = VK_NULL_HANDLE;

};

NAMESPACE_END(TexGui);
