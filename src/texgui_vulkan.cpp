#include "texgui_types.hpp"

#include "texgui.h"
#include "texgui_vulkan.hpp"
#include "texgui_internal.hpp"
#include "util.h"

#include <cassert>
#include "vulkan_shaders.hpp"
#include "msdf-atlas-gen/msdf-atlas-gen.h"

#include "stb_image.h"

#include <vulkan/vulkan_core.h>

#undef VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

using namespace TexGui;
using namespace TexGui::Math;

namespace TexGui
{
    struct TGVulkanBuffer
    {
        VkBuffer buffer;
        VmaAllocation allocation;
        VmaAllocationInfo info;
    };
}

struct TexGui_ImplVulkan_Data
{
    uint32_t whiteTextureID = 0;
    uint32_t textureCount = 0;
    VkDevice                    device;
    VkPhysicalDevice            physicalDevice;

    VkInstance                  instance;
    VkDebugUtilsMessengerEXT    debugMessenger;
    VkQueue                     graphicsQueue;
    uint32_t                    graphicsQueueFamily;

    VkPipelineRenderingCreateInfo renderingInfo;

    VkAllocationCallbacks*      allocationCallbacks = nullptr;

    //command buffer for immediate commands
    VkCommandPool   immCommandPool;
    VkCommandBuffer immCommandBuffer;
    VkFence         immCommandFence;

    VkSampler       textureSampler;
    VkSampler       linearSampler;

    int currentFrame = 0;
    int imageCount;
    std::vector<VkDescriptorPool> frameDescriptorPools;
    std::vector<std::vector<TGVulkanBuffer>> bufferDestroyQueue;

    VmaAllocator allocator;

    VkDescriptorPool globalDescriptorPool;

    VkDescriptorSet samplerDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSetLayout samplerDescriptorSetLayout;
    VkBuffer samplerBuffer = 0;
    VmaAllocation samplerBufferAllocation = 0;

    VkPipeline vertPipeline;
    VkPipelineLayout vertPipelineLayout = VK_NULL_HANDLE;

    TexGui_ImplVulkan_Data(const VulkanInitInfo& init_info);
};

static inline void image_barrier(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout,
                              VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask,
                              VkPipelineStageFlags2 dstStageMask,
                              VkAccessFlags2        dstAccessMask) {
    VkImageMemoryBarrier2 imageBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    imageBarrier.pNext = nullptr;

    imageBarrier.srcStageMask  = srcStageMask;
    imageBarrier.srcAccessMask = srcAccessMask;
    imageBarrier.dstStageMask  = dstStageMask;
    imageBarrier.dstAccessMask = dstAccessMask;

    imageBarrier.oldLayout = currentLayout;
    imageBarrier.newLayout = newLayout;

    VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    imageBarrier.subresourceRange = VkImageSubresourceRange{
        .aspectMask = aspectMask,
        .baseMipLevel = 0,
        .levelCount     = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount     = VK_REMAINING_ARRAY_LAYERS,
    };
    imageBarrier.image            = image;

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.pNext = nullptr;

    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers    = &imageBarrier;

    vkCmdPipelineBarrier2(cmd, &depInfo);
}

static VkShaderModule createShaderModule(VkDevice device, const uint32_t* shaderSource, size_t codeSize) {
    // create a new shader module, using the buffer we loaded
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext                    = nullptr;
    createInfo.codeSize                 = codeSize;
    createInfo.pCode                    = shaderSource;

    // check that the creation goes well.
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        printf("Error creating shader\n");
        assert(false);
    }

    return shaderModule;
}

struct VertexPushConstants
{
    Math::fvec2 scale;
    Math::fvec2 translate;
    uint32_t textureIndex;
    float pxRange;
    Math::fvec2 uvScale;
} vertPushConstants;

static uint32_t createTexture(VkImageView imageView, Math::fbox bounds)
{
    TexGui_ImplVulkan_Data* v = (TexGui_ImplVulkan_Data*)(GTexGui->rendererData);
    VkDescriptorImageInfo imgInfo{
        .sampler = v->textureSampler,
        .imageView = imageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    auto imageWrite = VkWriteDescriptorSet{
        .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext            = nullptr,
        .dstSet           = v->samplerDescriptorSet,
        .dstBinding       = 0,
        .dstArrayElement  = v->textureCount,
        .descriptorCount  = 1,
        .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo       = &imgInfo,
        .pBufferInfo      = nullptr,
        .pTexelBufferView = nullptr //unused for now
    };
    vkUpdateDescriptorSets(v->device, 1, &imageWrite, 0, nullptr);

    return v->textureCount++;
}

namespace TexGui {
struct Image {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
};
}

std::vector<TexGui::Image> images;

static uint32_t _createTexture_Vulkan(void* data, int width, int height, VkSampler sampler)
{
    TexGui_ImplVulkan_Data* v = (TexGui_ImplVulkan_Data*)(GTexGui->rendererData);
    VkExtent3D size = {
        .width = uint32_t(width),
        .height = uint32_t(height),
        .depth = 1
    };
    
    // allocate buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext              = nullptr;
    bufferInfo.size               = width * height * 4;
    bufferInfo.usage              = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.requiredFlags           = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    vmaallocInfo.flags                   = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer uploadBuffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;

    vmaCreateBuffer(v->allocator, &bufferInfo, &vmaallocInfo, &uploadBuffer, &allocation, &allocationInfo);
    memcpy(allocationInfo.pMappedData, data, bufferInfo.size);

    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

    VkImageCreateInfo img_info = {
        .sType             = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext             = nullptr,
        .imageType         = VK_IMAGE_TYPE_2D,
        .format            = format,
        .extent            = size,
        .mipLevels         = 1,
        .arrayLayers       = 1,
        .samples           = VK_SAMPLE_COUNT_1_BIT, // no msaa
        .tiling            = VK_IMAGE_TILING_OPTIMAL,
        .usage             = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    };

    // always allocate images on dedicated GPU memory
    VmaAllocationCreateInfo allocinfo = {};
    allocinfo.usage                   = VMA_MEMORY_USAGE_GPU_ONLY;
    allocinfo.requiredFlags           = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkImage image;
    VmaAllocation imageAllocation;
    vmaCreateImage(v->allocator, &img_info, &allocinfo, &image, &imageAllocation, nullptr);
    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageViewCreateInfo info           = {};
    info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.pNext                           = nullptr;
    info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    info.image                           = image;
    info.format                          = format;
    info.subresourceRange.baseMipLevel   = 0;
    info.subresourceRange.levelCount     = 1;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount     = 1;
    info.subresourceRange.aspectMask     = aspectFlag;

    VkImageView iv;
    vkCreateImageView(v->device, &info, nullptr, &iv);

    vkResetFences(v->device, 1, &v->immCommandFence);
    vkResetCommandBuffer(v->immCommandBuffer, 0);
    VkCommandBufferBeginInfo cmdBeginInfo = {};
    cmdBeginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBeginInfo.pNext                    = nullptr;
    cmdBeginInfo.pInheritanceInfo         = nullptr;
    cmdBeginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(v->immCommandBuffer, &cmdBeginInfo);

    image_barrier(v->immCommandBuffer, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR, VK_ACCESS_2_MEMORY_WRITE_BIT_KHR);

    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset      = 0;
    copyRegion.bufferRowLength   = 0;
    copyRegion.bufferImageHeight = 0;

    copyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel       = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount     = 1;
    copyRegion.imageExtent                     = size;

    // copy the buffer into the image
    vkCmdCopyBufferToImage(v->immCommandBuffer, uploadBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    image_barrier(v->immCommandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR | VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT_KHR,
            VK_ACCESS_2_SHADER_READ_BIT_KHR);

    vkEndCommandBuffer(v->immCommandBuffer);
    VkCommandBufferSubmitInfo cmdinfo{};
    cmdinfo.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdinfo.pNext         = nullptr;
    cmdinfo.commandBuffer = v->immCommandBuffer;
    cmdinfo.deviceMask    = 0;

    VkSubmitInfo2 submit            = {};
    submit.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit.pNext                    = nullptr;
    submit.waitSemaphoreInfoCount   = 0;
    submit.pWaitSemaphoreInfos      = nullptr;
    submit.signalSemaphoreInfoCount = 0;
    submit.pSignalSemaphoreInfos    = nullptr;
    submit.commandBufferInfoCount   = 1;
    submit.pCommandBufferInfos      = &cmdinfo;
    // submit command buffer to the queue and execute it.
    //  _renderFence will now block until the graphic commands finish execution
    vkQueueSubmit2(v->graphicsQueue, 1, &submit, v->immCommandFence);
    vkWaitForFences(v->device, 1, &v->immCommandFence, true, 9999999999);

    VkDescriptorImageInfo imgInfo{
        .sampler = sampler,
        .imageView = iv,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    auto imageWrite = VkWriteDescriptorSet{
        .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext            = nullptr,
        .dstSet           = v->samplerDescriptorSet,
        .dstBinding       = 0,
        .dstArrayElement  = v->textureCount,
        .descriptorCount  = 1,
        .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo       = &imgInfo,
        .pBufferInfo      = nullptr,
        .pTexelBufferView = nullptr //unused for now
    };
    vkUpdateDescriptorSets(v->device, 1, &imageWrite, 0, nullptr);

    vmaDestroyBuffer(v->allocator, uploadBuffer, allocation);

    images.push_back({image, iv, imageAllocation});

    return v->textureCount++;
}

static uint32_t createTexture_Vulkan(void* data, int width, int height)
{
    TexGui_ImplVulkan_Data* v = (TexGui_ImplVulkan_Data*)(GTexGui->rendererData);
    return _createTexture_Vulkan(data, width, height, v->textureSampler);
}

static uint32_t createFontAtlas_Vulkan(void* data, int width, int height)
{
    TexGui_ImplVulkan_Data* v = (TexGui_ImplVulkan_Data*)(GTexGui->rendererData);
    return _createTexture_Vulkan(data, width, height, v->linearSampler);
}

constexpr int MAX_SAMPLERS = 65536;
TexGui_ImplVulkan_Data::TexGui_ImplVulkan_Data(const VulkanInitInfo& init_info)
{
    device = init_info.Device;
    physicalDevice = init_info.PhysicalDevice;
    instance = init_info.Instance;
    //debugMessenger = info.DebugMessenger;
    graphicsQueue = init_info.Queue;
    graphicsQueueFamily = init_info.QueueFamily;
    allocator = init_info.Allocator;
    allocationCallbacks = init_info.allocationCallbacks;
    renderingInfo = init_info.PipelineRenderingCreateInfo;
    textureSampler = init_info.Sampler;
    imageCount = init_info.ImageCount;
}

static void createImmediateCommandBuffers_Vulkan()
{
    TexGui_ImplVulkan_Data* v = (TexGui_ImplVulkan_Data*)(GTexGui->rendererData);
    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.pNext                   = nullptr;
    commandPoolInfo.flags                   = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex        = v->graphicsQueueFamily;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext             = nullptr;
    fenceInfo.flags             = VK_FENCE_CREATE_SIGNALED_BIT;

    vkCreateFence(v->device, &fenceInfo, nullptr, &v->immCommandFence);

    vkCreateCommandPool(v->device, &commandPoolInfo, nullptr, &v->immCommandPool);
    VkCommandBufferAllocateInfo cmdAllocInfo = {};
    cmdAllocInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.pNext                       = nullptr;
    cmdAllocInfo.commandPool                 = v->immCommandPool;
    cmdAllocInfo.commandBufferCount          = 1;
    cmdAllocInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    vkAllocateCommandBuffers(v->device, &cmdAllocInfo, &v->immCommandBuffer);
}

static void initializeDescriptors_Vulkan()
{
    TexGui_ImplVulkan_Data* v = (TexGui_ImplVulkan_Data*)(GTexGui->rendererData);
    {
        VkDescriptorPoolSize pool_sizes[] = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 65536},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags                      = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        pool_info.maxSets                    = 2;

        pool_info.poolSizeCount = sizeof(pool_sizes) / sizeof(pool_sizes[0]);
        pool_info.pPoolSizes    = pool_sizes;

        vkCreateDescriptorPool(v->device, &pool_info, nullptr, &v->globalDescriptorPool);
    }

    v->frameDescriptorPools.resize(v->imageCount);
    for (int i = 0; i < v->imageCount; i++)
    {
        VkDescriptorPoolSize pool_sizes[] = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 64},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 64},
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags                      = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        pool_info.maxSets                    = 64;

        /*
        for (VkDescriptorPoolSize& pool_size : pool_sizes)
            pool_info.maxSets += pool_size.descriptorCount;
        */

        pool_info.poolSizeCount = sizeof(pool_sizes) / sizeof(pool_sizes[0]);
        pool_info.pPoolSizes    = pool_sizes;

        vkCreateDescriptorPool(v->device, &pool_info, nullptr, &v->frameDescriptorPools[i]);
    }

    // probably shouldnt be in descriptors section
    v->bufferDestroyQueue.resize(v->imageCount);
    VkDescriptorSetLayoutCreateInfo set_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    //all separate descriptor sets
    VkDescriptorSetLayoutBinding bindings[] =
    {
        VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = MAX_SAMPLERS,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
        }
    };

    VkDescriptorBindingFlags bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingInfo = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
    bindingInfo.pBindingFlags = &bindingFlags;
    bindingInfo.bindingCount = 1;
    set_info.pNext                           = &bindingInfo;
    set_info.flags                           = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    set_info.bindingCount                    = 1;
    set_info.pBindings                       = bindings;
    vkCreateDescriptorSetLayout(v->device, &set_info, nullptr, &v->samplerDescriptorSetLayout);

    {
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.pNext                       = VK_NULL_HANDLE;
        allocInfo.sType                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool              = v->globalDescriptorPool;
        allocInfo.descriptorSetCount          = 1;
        allocInfo.pSetLayouts                 = &v->samplerDescriptorSetLayout;
        VkResult        result = vkAllocateDescriptorSets(v->device, &allocInfo, &v->samplerDescriptorSet);
    }
}

// creates 1x1 white texture for coloured objects without a texture
static void createWhiteTexture_Vulkan()
{
    TexGui_ImplVulkan_Data* v = (TexGui_ImplVulkan_Data*)(GTexGui->rendererData);
    //make white texture
    unsigned char white[4] = {255, 255, 255, 255};
    v->whiteTextureID = createTexture_Vulkan(white, 1, 1);
}

static void createPipelines_Vulkan()
{
    TexGui_ImplVulkan_Data* v = (TexGui_ImplVulkan_Data*)(GTexGui->rendererData);
    //create pipeline layout
    VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(VertexPushConstants)
    };

    VkDescriptorSetLayout dsl[] = {v->samplerDescriptorSetLayout};
    VkPipelineLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = sizeof(dsl)/sizeof(dsl[0]),
        .pSetLayouts = dsl,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &pushConstantRange,
    };
    vkCreatePipelineLayout(v->device, &layoutInfo, nullptr, &v->vertPipelineLayout);
    assert(v->vertPipelineLayout != VK_NULL_HANDLE);

    std::vector<VkPipelineShaderStageCreateInfo> vertstages;
    VkShaderModule vertvert = createShaderModule(v->device, VK_VERT, sizeof(VK_VERT));
    VkShaderModule vertfrag = createShaderModule(v->device, VK_FRAG, sizeof(VK_FRAG));
    VkPipelineShaderStageCreateInfo shaderStage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    shaderStage.pName = "main";
    shaderStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStage.module = vertvert;
    vertstages.push_back(shaderStage);
    shaderStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStage.module = vertfrag;
    vertstages.push_back(shaderStage);

    VkPipelineInputAssemblyStateCreateInfo           inputAssemblyState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkPipelineTessellationStateCreateInfo            tessellationState  = {.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO};

    VkPipelineViewportStateCreateInfo                viewportState      = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext         = nullptr,
        .viewportCount = 1,
        .scissorCount  = 1,
    };

    VkPipelineRasterizationStateCreateInfo           rasterizationState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode    = VK_CULL_MODE_NONE,
        .frontFace   = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth   = 1.f,
    };

    VkPipelineMultisampleStateCreateInfo           multisampleState = {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable   = VK_FALSE,
        .pSampleMask           = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable      = VK_FALSE,
    };
    VkPipelineDepthStencilStateCreateInfo depthStencilState = {
        .sType                                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable                       = VK_FALSE,
        .depthWriteEnable                      = VK_FALSE,
    };

    VkPipelineColorBlendAttachmentState colorBlendState = {
        .blendEnable         = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp        = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp        = VK_BLEND_OP_ADD,
        .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo colorBlendStateInfo = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext           = VK_NULL_HANDLE,
        .flags           = 0,
        .logicOpEnable   = VK_FALSE,
        .logicOp         = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments    = &colorBlendState,
    };

    VkDynamicState dynamicStates[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicStateInfo = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext             = VK_NULL_HANDLE,
        .flags             = 0,
        .dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]),
        .pDynamicStates    = dynamicStates,
    };

    // create general purpose vertex pipeline
    VkVertexInputBindingDescription binding_desc[1] = {};
    binding_desc[0] = {
        .binding = 0,
        .stride = sizeof(RenderData::Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    VkVertexInputAttributeDescription attribute_desc[3] = {};
    attribute_desc[0] = {
        .location = 0,
        .binding = binding_desc[0].binding,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof(RenderData::Vertex, pos),
    };
    attribute_desc[1] = {
        .location = 1,
        .binding = binding_desc[0].binding,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof(RenderData::Vertex, uv),
    };
    //#TODO: use RGBA8 instead
    attribute_desc[2] = {
        .location = 2,
        .binding = binding_desc[0].binding,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = offsetof(RenderData::Vertex, col),
    };

    VkPipelineVertexInputStateCreateInfo             vertexInputState   = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = sizeof(binding_desc)/sizeof(binding_desc[0]),
        .pVertexBindingDescriptions = binding_desc,
        .vertexAttributeDescriptionCount = sizeof(attribute_desc)/sizeof(attribute_desc[0]),
        .pVertexAttributeDescriptions = attribute_desc,
    };

    VkGraphicsPipelineCreateInfo info = {};
    info.sType                        = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.pNext                             = &v->renderingInfo;
    info.flags                             = 0;
    info.stageCount                        = 2;
    info.pStages = vertstages.data();
    info.pVertexInputState                 = &vertexInputState;
    info.pInputAssemblyState               = &inputAssemblyState;
    info.pTessellationState                = &tessellationState;
    info.pViewportState                    = &viewportState;
    info.pRasterizationState               = &rasterizationState;
    info.pMultisampleState                 = &multisampleState;
    info.pDepthStencilState                = &depthStencilState;
    info.pColorBlendState               = &colorBlendStateInfo;
    info.pDynamicState                     = &dynamicStateInfo;
    info.layout                            = v->vertPipelineLayout;

    if (vkCreateGraphicsPipelines(v->device, VK_NULL_HANDLE, 1, &info, nullptr, &v->vertPipeline) != VK_SUCCESS) {
        printf("Failed to create pipeline\n");
        assert(false);
    }
        
    vkDestroyShaderModule(v->device, vertvert, nullptr);
    vkDestroyShaderModule(v->device, vertfrag, nullptr);
}

static void buffer_barrier(VkCommandBuffer cmd, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size,
                              VkPipelineStageFlags2 srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VkAccessFlags2 srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
                              VkPipelineStageFlags2 dstStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                              VkAccessFlags2        dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT) {
    VkBufferMemoryBarrier2 bufferBarrier{
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .pNext = nullptr,
        .srcStageMask = srcStageMask,
        .srcAccessMask = srcAccessMask,
        .dstStageMask = dstStageMask,
        .dstAccessMask = dstAccessMask,
        .buffer = buffer,
        .offset = offset,
        .size = size
    };

    VkDependencyInfo depInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &bufferBarrier,
        .imageMemoryBarrierCount = 0,
    };

    vkCmdPipelineBarrier2(cmd, &depInfo);
}

//alignment must be power of two
static VkDeviceSize alignDeviceSize(VkDeviceSize size, VkDeviceSize alignment = 16)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

/*
void VulkanContext::createOrResizeBuffer(VkBuffer& buffer, VkDeviceMemory& buffer_memory, VkDeviceSize& buffer_size, VkDeviceSize new_size, VkBufferUsageFlagBits usage)
{
    if (buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(device, buffer, allocationCallbacks);
    if (buffer_memory != VK_NULL_HANDLE)
        vkFreeMemory(device, buffer_memory, allocationCallbacks);
}
*/

static void framebufferSizeCallback_Vulkan(int width, int height)
{
}

void TexGui::renderFromRenderData_Vulkan(VkCommandBuffer cmd, const RenderData& data)
{
    TexGui_ImplVulkan_Data* v = (TexGui_ImplVulkan_Data*)(GTexGui->rendererData);
    // set dynamic scissor
    VkRect2D scissor      = {};
    scissor.offset.x      = data.scissor.x;
    scissor.offset.y      = data.scissor.y;
    scissor.extent.width  = data.scissor.width;
    scissor.extent.height = data.scissor.height;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // allocate vertices buffer
    if (data.vertices.size() > 0)
    {
        assert(data.indices.size() > 0);
        VkBufferCreateInfo bufferCreateInfo = {};
        bufferCreateInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.pNext              = nullptr;
        bufferCreateInfo.size               = sizeof(RenderData::Vertex) * data.vertices.size();
        bufferCreateInfo.usage              = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

        VmaAllocationCreateInfo vmaallocInfo = {};
        vmaallocInfo.requiredFlags           = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        vmaallocInfo.flags                   = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo allocationInfo;
        TGVulkanBuffer vertexBuffer;
        vmaCreateBuffer(v->allocator, &bufferCreateInfo, &vmaallocInfo, &vertexBuffer.buffer, &vertexBuffer.allocation, &allocationInfo);

        v->bufferDestroyQueue[v->currentFrame].push_back(vertexBuffer);

        vmaCopyMemoryToAllocation(v->allocator, data.vertices.data(), vertexBuffer.allocation, 0, data.vertices.size() * sizeof(RenderData::Vertex));

        VkDeviceSize vertexOffset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer.buffer, &vertexOffset);

        bufferCreateInfo.size = sizeof(uint32_t) * data.indices.size();
        bufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

        TGVulkanBuffer indexBuffer;
        vmaCreateBuffer(v->allocator, &bufferCreateInfo, &vmaallocInfo, &indexBuffer.buffer, &indexBuffer.allocation, &allocationInfo);
        vmaCopyMemoryToAllocation(v->allocator, data.indices.data(), indexBuffer.allocation, 0, data.indices.size() * sizeof(uint32_t));

        v->bufferDestroyQueue[v->currentFrame].push_back(indexBuffer);

        vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    }

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, v->vertPipelineLayout, 0, 1, &v->samplerDescriptorSet, 0, nullptr);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, v->vertPipeline);

    int count = 0;
    int currIndex = 0;
    for (auto& c : data.commands)
    {
        scissor.offset.x      = fmax(0, c.scissor.x);
        scissor.offset.y      = fmax(0, c.scissor.y);
        scissor.extent.width  = fmax(0, c.scissor.width);
        scissor.extent.height = fmax(0, c.scissor.height);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vertPushConstants.textureIndex = c.textureIndex;
        vertPushConstants.scale = c.scale;
        vertPushConstants.translate = c.translate;
        vertPushConstants.pxRange = c.pxRange;
        vertPushConstants.uvScale = c.uvScale;

        vkCmdPushConstants(cmd, v->vertPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VertexPushConstants), &vertPushConstants);

        vkCmdDrawIndexed(cmd, c.indexCount, 1, currIndex, 0, 0);

        //#TODO: have firstIndex in the command itself maybe
        currIndex += c.indexCount;
    }

    std::vector<RenderData> children(data.children);

    if (data.ordered)
    {
        std::sort(children.begin(), children.end(), [](const RenderData& lhs, const RenderData& rhs)
                {
                    return lhs.priority < rhs.priority;
                }
                );
    }

    for (const auto& child : children)
        renderFromRenderData_Vulkan(cmd, child);

    children.clear();
}

static void newFrame_Vulkan()
{
    //#TODO: wrong if more than one image count, can delete buffers while theyre being used by another frame
    TexGui_ImplVulkan_Data* v = (TexGui_ImplVulkan_Data*)(GTexGui->rendererData);
    v->currentFrame++;
    if (v->currentFrame == v->imageCount) v->currentFrame = 0;

    vkResetDescriptorPool(v->device, v->frameDescriptorPools[v->currentFrame], 0);

    auto& dq = v->bufferDestroyQueue[v->currentFrame];
    for (auto it = dq.rbegin(); it != dq.rend(); it++) {
        vmaDestroyBuffer(v->allocator, it->buffer, it->allocation);
    }

    dq.clear();
}

void renderClean_Vulkan()
{
    TexGui_ImplVulkan_Data* v = (TexGui_ImplVulkan_Data*)(GTexGui->rendererData);
    vkDestroyDescriptorPool(v->device, v->globalDescriptorPool, 0);
    for (auto& dp : v->frameDescriptorPools)
        vkDestroyDescriptorPool(v->device, dp, 0);

    vkDestroyCommandPool(v->device, v->immCommandPool, 0);
    vkDestroyFence(v->device, v->immCommandFence, 0);

    vkDestroySampler(v->device, v->linearSampler, nullptr);

    vkDestroyPipeline(v->device, v->vertPipeline, nullptr);
    vkDestroyPipelineLayout(v->device, v->vertPipelineLayout, nullptr);

    vkDestroyDescriptorSetLayout(v->device, v->samplerDescriptorSetLayout, nullptr);

    for (auto& dq : v->bufferDestroyQueue)
    {
        for (auto it = dq.rbegin(); it != dq.rend(); it++) {
            vmaDestroyBuffer(v->allocator, it->buffer, it->allocation);
        }
    }
    for (auto& im : images)
    {
        vmaDestroyImage(v->allocator, im.image, im.allocation);
        vkDestroyImageView(v->device, im.imageView, nullptr);
    }
    //#TODO: need to destroy all textures here
}

bool TexGui::initVulkan(VulkanInitInfo& info)
{
    assert(!GTexGui->rendererData);
    GTexGui->rendererData = new TexGui_ImplVulkan_Data(info);

    GTexGui->rendererFns.createTexture = createTexture_Vulkan;
    GTexGui->rendererFns.createFontAtlas = createFontAtlas_Vulkan;
    GTexGui->rendererFns.renderClean = renderClean_Vulkan;
    GTexGui->rendererFns.framebufferSizeCallback = framebufferSizeCallback_Vulkan;
    GTexGui->rendererFns.newFrame = newFrame_Vulkan;

    TexGui_ImplVulkan_Data* v = (TexGui_ImplVulkan_Data*)(GTexGui->rendererData);
    VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampl.magFilter           = VK_FILTER_LINEAR;
    sampl.minFilter           = VK_FILTER_LINEAR;
    vkCreateSampler(v->device, &sampl, nullptr, &v->linearSampler);

    createImmediateCommandBuffers_Vulkan();
    initializeDescriptors_Vulkan();
    createPipelines_Vulkan();
    createWhiteTexture_Vulkan();
    //#TODO: need to delete more stuff probably
    return true;
} 

#include <list>
static std::list<Texture> m_custom_texs;
Texture* TexGui::customTexture(VkImageView imageView, Math::ibox bounds, Math::ivec2 atlasSize)
{
    int index = createTexture(imageView, bounds);

    float xth = bounds.w / 3.f;
    float yth = bounds.h / 3.f;
    ivec2 size;
    if (atlasSize.x > 0 && atlasSize.y > 0) size = atlasSize;
    else size = bounds.size;
    return &m_custom_texs.emplace_back(index, bounds, size, yth, xth, yth, xth);
}
