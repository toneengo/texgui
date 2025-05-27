#include "texgui_types.hpp"

#include "texgui.h"
#include "texgui_vulkan.hpp"
#include "texgui_internal.hpp"
#include "util.h"

#include <cassert>
#include "vulkan_shaders.hpp"
#include "msdf-atlas-gen/msdf-atlas-gen.h"

#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include "stb_image.h"

#include <vulkan/vulkan_core.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

using namespace TexGui;
using namespace TexGui::Math;

struct TexGui_ImplVulkan_Data
{
    struct Buf {
        VkBuffer buffer;
        VmaAllocation allocation;
        VmaAllocationInfo info;
    } buffer;
    uint32_t whiteTextureID = 0;
    uint32_t textureCount = 0;
    VkDevice                    device;
    VkPhysicalDevice            physicalDevice;

    VkInstance                  instance;
    VkDebugUtilsMessengerEXT    debugMessenger;
    VkQueue                     graphicsQueue;
    uint32_t                    graphicsQueueFamily;

    VkPipelineRenderingCreateInfo renderingInfo;

    bool resizeRequested = true;

    VkAllocationCallbacks*      allocationCallbacks = nullptr;
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

    VkDescriptorSet samplerDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSetLayout samplerDescriptorSetLayout;
    VkBuffer samplerBuffer = 0;
    VmaAllocation samplerBufferAllocation = 0;

    VkDescriptorSetLayout storageDescriptorSetLayout;

    VkPipeline quadPipeline;
    VkPipeline textPipeline;
    VkPipelineLayout quadPipelineLayout = VK_NULL_HANDLE;

    TexGui_ImplVulkan_Data(const VulkanInitInfo& init_info);
};

static inline void image_barrier(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout,
                              VkPipelineStageFlags2 srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VkAccessFlags2 srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
                              VkPipelineStageFlags2 dstStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                              VkAccessFlags2        dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT) {
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

static std::vector<uint32_t> compileShaderToSPIRV_Vulkan(const char* shaderSource, EShLanguage stage) {
    glslang::InitializeProcess();

    glslang::TShader shader(stage);
    shader.setDebugInfo(true);
    shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_6);

    shader.setStrings(&shaderSource, 1);
    if (!shader.parse(GetDefaultResources(), 100, false, EShMsgDefault)) {
        puts(shader.getInfoLog());
        puts(shader.getInfoDebugLog());
        return {};
    }

    const char* const* ptr;
    int                n = 0;
    shader.getStrings(ptr, n);

    glslang::TProgram program;
    program.addShader(&shader);
    program.link(EShMsgDefault);

    std::vector<uint32_t> spirv;
    glslang::GlslangToSpv(*program.getIntermediate(stage), spirv);

    glslang::FinalizeProcess();

    return spirv;
}

static VkShaderModule createShaderModule(VkDevice device, int stage, const std::string& shaderSource) {
    auto spirv = compileShaderToSPIRV_Vulkan(shaderSource.c_str(), EShLanguage(stage));
    if (spirv.size() == 0) {
        printf("Error compiling shader\n");
        assert(false);
    }

    // create a new shader module, using the buffer we loaded
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext                    = nullptr;
    createInfo.codeSize                 = spirv.size() * sizeof(uint32_t);
    createInfo.pCode                    = spirv.data();

    // check that the creation goes well.
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        printf("Error creating shader\n");
        assert(false);
    }

    return shaderModule;
}

struct PushConstants
{
    Math::fbox bounds;
    int index;
    int texID;
    int atlasWidth;
    int atlasHeight;
    float pxRange;
    int fontPx;
} pushConstants;

static uint32_t createTexture(VkImageView imageView, Math::fbox bounds)
{
    TexGui_ImplVulkan_Data* v = (TexGui_ImplVulkan_Data*)(GTexGui->rendererData);
    VkDescriptorImageInfo imgInfo{
        .sampler = v->nearestSampler,
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

static uint32_t createTexture_Vulkan(void* data, int width, int height)
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
    vmaallocInfo.usage                   = VMA_MEMORY_USAGE_CPU_TO_GPU;
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

    image_barrier(v->immCommandBuffer, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

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

    image_barrier(v->immCommandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

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
        .sampler = v->nearestSampler,
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

    return v->textureCount++;
}


constexpr int MAX_SAMPLERS = 65536;
TexGui_ImplVulkan_Data::TexGui_ImplVulkan_Data(const VulkanInitInfo& init_info)
{
    windowSize = GTexGui->framebufferSize;
    device = init_info.Device;
    physicalDevice = init_info.PhysicalDevice;

    instance = init_info.Instance;
    //debugMessenger = info.DebugMessenger;
    graphicsQueue = init_info.Queue;
    graphicsQueueFamily = init_info.QueueFamily;

    allocator = init_info.Allocator;
    allocationCallbacks = init_info.allocationCallbacks;

    imageCount = init_info.ImageCount;

    renderingInfo = init_info.PipelineRenderingCreateInfo;

    //create default samplers
    VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampl.magFilter           = VK_FILTER_NEAREST;
    sampl.minFilter           = VK_FILTER_NEAREST;
    vkCreateSampler(device, &sampl, nullptr, &nearestSampler);

    sampl.magFilter = VK_FILTER_LINEAR;
    sampl.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(device, &sampl, nullptr, &linearSampler);

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

        vkCreateDescriptorPool(device, &pool_info, nullptr, &globalDescriptorPool);
    }

    frameDescriptorPools.resize(imageCount);
    for (int i = 0; i < imageCount; i++)
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

        vkCreateDescriptorPool(device, &pool_info, nullptr, &frameDescriptorPools[i]);
    }

    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.pNext                   = nullptr;
    commandPoolInfo.flags                   = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex        = graphicsQueueFamily;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext             = nullptr;
    fenceInfo.flags             = VK_FENCE_CREATE_SIGNALED_BIT;

    vkCreateFence(device, &fenceInfo, nullptr, &immCommandFence);

    vkCreateCommandPool(device, &commandPoolInfo, nullptr, &immCommandPool);
    VkCommandBufferAllocateInfo cmdAllocInfo = {};
    cmdAllocInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.pNext                       = nullptr;
    cmdAllocInfo.commandPool                 = immCommandPool;
    cmdAllocInfo.commandBufferCount          = 1;
    cmdAllocInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    vkAllocateCommandBuffers(device, &cmdAllocInfo, &immCommandBuffer);

    VkDescriptorSetLayoutCreateInfo set_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    //all separate descriptor sets
    VkDescriptorSetLayoutBinding bindings[] =
    {
        VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
        },
        VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
        },
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
    set_info.bindingCount                    = 1;

    set_info.pBindings                       = &bindings[0];
    set_info.flags                           = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    vkCreateDescriptorSetLayout(device, &set_info, nullptr, &storageDescriptorSetLayout);

    set_info.pBindings                       = &bindings[1];
    vkCreateDescriptorSetLayout(device, &set_info, nullptr, &windowSizeDescriptorSetLayout);

    set_info.pBindings                       = &bindings[2];
    vkCreateDescriptorSetLayout(device, &set_info, nullptr, &samplerDescriptorSetLayout);

    {
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.pNext                       = VK_NULL_HANDLE;
        allocInfo.sType                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool              = globalDescriptorPool;
        allocInfo.descriptorSetCount          = 1;
        allocInfo.pSetLayouts                 = &samplerDescriptorSetLayout;
        VkResult        result = vkAllocateDescriptorSets(device, &allocInfo, &samplerDescriptorSet);
    }

    VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(PushConstants)
    };

    VkDescriptorSetLayout dsl[] = {storageDescriptorSetLayout, windowSizeDescriptorSetLayout, samplerDescriptorSetLayout};
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pNext                  = nullptr;
    layoutInfo.pSetLayouts            = dsl;
    layoutInfo.setLayoutCount         = sizeof(dsl)/sizeof(dsl[0]);
    layoutInfo.pPushConstantRanges    = &pushConstantRange;
    layoutInfo.pushConstantRangeCount = 1;
    vkCreatePipelineLayout(device, &layoutInfo, nullptr, &quadPipelineLayout);
    assert(quadPipelineLayout != VK_NULL_HANDLE);
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
    VkGraphicsPipelineCreateInfo info = {};
    info.sType                        = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    std::vector<VkFormat> colorAttachmentFormats;
    VkPipelineRenderingCreateInfo renderingCreateInfo = v->renderingInfo;

    std::vector<VkPipelineShaderStageCreateInfo> quadstages;
    std::vector<VkPipelineShaderStageCreateInfo> textstages;
    VkShaderModule quadvert = createShaderModule(v->device, EShLangVertex, VK_QUADVERT);
    VkShaderModule quadfrag = createShaderModule(v->device, EShLangFragment, VK_QUADFRAG);
    VkShaderModule textvert = createShaderModule(v->device, EShLangVertex, VK_TEXTVERT);
    VkShaderModule textfrag = createShaderModule(v->device, EShLangFragment, VK_TEXTFRAG);
    quadstages.push_back(VkPipelineShaderStageCreateInfo{
        .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext               = nullptr,
        .flags               = 0,
        .stage               = VK_SHADER_STAGE_VERTEX_BIT,
        .module              = quadvert,
        .pName               = "main",
        .pSpecializationInfo = VK_NULL_HANDLE,
    });
    quadstages.push_back(VkPipelineShaderStageCreateInfo{
        .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext               = nullptr,
        .flags               = 0,
        .stage               = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module              = quadfrag,
        .pName               = "main",
        .pSpecializationInfo = VK_NULL_HANDLE,
    });

    textstages.push_back(VkPipelineShaderStageCreateInfo{
        .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext               = nullptr,
        .flags               = 0,
        .stage               = VK_SHADER_STAGE_VERTEX_BIT,
        .module              = textvert,
        .pName               = "main",
        .pSpecializationInfo = VK_NULL_HANDLE,
    });
    textstages.push_back(VkPipelineShaderStageCreateInfo{
        .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext               = nullptr,
        .flags               = 0,
        .stage               = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module              = textfrag,
        .pName               = "main",
        .pSpecializationInfo = VK_NULL_HANDLE,
    });


    VkPipelineVertexInputStateCreateInfo             vertexInputState   = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

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

    info.pNext                             = &renderingCreateInfo;
    info.flags                             = 0;
    info.stageCount                        = 2;
    info.pStages                           = quadstages.data();
    info.pVertexInputState                 = &vertexInputState;
    info.pInputAssemblyState               = &inputAssemblyState;
    info.pTessellationState                = &tessellationState;
    info.pViewportState                    = &viewportState;
    info.pRasterizationState               = &rasterizationState;
    info.pMultisampleState                 = &multisampleState;
    info.pDepthStencilState                = &depthStencilState;

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

    VkPipelineColorBlendStateCreateInfo _c = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext           = VK_NULL_HANDLE,
        .flags           = 0,
        .logicOpEnable   = VK_FALSE,
        .logicOp         = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments    = &colorBlendState,
    };

    VkDynamicState dynamicStates[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    info.pColorBlendState               = &_c;
    VkPipelineDynamicStateCreateInfo _d = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext             = VK_NULL_HANDLE,
        .flags             = 0,
        .dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]),
        .pDynamicStates    = dynamicStates,
    };
    info.pDynamicState                  = &_d;
    info.layout                         = v->quadPipelineLayout;
    //the rest are unused parameters

    if (vkCreateGraphicsPipelines(v->device, VK_NULL_HANDLE, 1, &info, nullptr, &v->quadPipeline) != VK_SUCCESS) {
        printf("Failed to create pipeline\n");
        assert(false);
    }

    info.pStages = textstages.data();

    if (vkCreateGraphicsPipelines(v->device, VK_NULL_HANDLE, 1, &info, nullptr, &v->textPipeline) != VK_SUCCESS) {
        printf("Failed to create pipeline\n");
        assert(false);
    }

    vkDestroyShaderModule(v->device, quadvert, nullptr);
    vkDestroyShaderModule(v->device, quadfrag, nullptr);
    vkDestroyShaderModule(v->device, textvert, nullptr);
    vkDestroyShaderModule(v->device, textfrag, nullptr);
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

std::vector<VmaAllocation> allocationsToDestroy;
std::vector<VkBuffer> buffersToDestroy;

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
    TexGui_ImplVulkan_Data* v = (TexGui_ImplVulkan_Data*)(GTexGui->rendererData);
    v->windowSize = {width, height};
    v->resizeRequested = true;
}

void TexGui::renderFromRenderData_Vulkan(VkCommandBuffer cmd, const RenderData& data)
{
    TexGui_ImplVulkan_Data* v = (TexGui_ImplVulkan_Data*)(GTexGui->rendererData);
    std::vector<RenderData> children = std::move(data.children);
    if (data.ordered)
    {
        std::sort(children.begin(), children.end(), [](const RenderData& lhs, const RenderData& rhs)
                {
                    return lhs.priority < rhs.priority;
                }
                );
    }
    for (const auto& child : children)
    {
        renderFromRenderData_Vulkan(cmd, child);
    }
    children.clear();

    if (data.objects.size() < 1) return;

    // set dynamic viewport and scissor
    VkViewport viewport = {};
    viewport.x          = 0;
    viewport.y          = 0;
    viewport.width      = v->windowSize.x;
    viewport.height     = v->windowSize.y;
    viewport.minDepth   = 0;
    viewport.maxDepth   = 1;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor      = {};
    scissor.offset.x      = 0;
    scissor.offset.y      = 0;
    scissor.extent.width  = v->windowSize.x;
    scissor.extent.height = v->windowSize.y;
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    //#TODO: more efficeint way to do this is to usue one buffer, then keep track of the counts of each RenderData's arrays
    //update window size uniform
    {
        VkBufferCreateInfo bufferCreateInfo = {};
        bufferCreateInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.pNext              = nullptr;
        bufferCreateInfo.size               = sizeof(Math::ivec2);
        bufferCreateInfo.usage              = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        VmaAllocationCreateInfo vmaallocInfo = {};
        vmaallocInfo.usage                   = VMA_MEMORY_USAGE_CPU_TO_GPU;
        vmaallocInfo.flags                   = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo allocationInfo;

        VkBuffer windowSzBuf;
        VmaAllocation windowSzBufAlloc;
        vmaCreateBuffer(v->allocator, &bufferCreateInfo, &vmaallocInfo, &windowSzBuf, &windowSzBufAlloc, &allocationInfo);

        buffersToDestroy.push_back(windowSzBuf);
        allocationsToDestroy.push_back(windowSzBufAlloc);

        vmaCopyMemoryToAllocation(v->allocator, &v->windowSize, windowSzBufAlloc, 0, sizeof(Math::ivec2));
        VkDescriptorBufferInfo bufferInfo = {
            .buffer = windowSzBuf,
            .offset = 0,
            .range = sizeof(Math::ivec2)
        };

        VkDescriptorSet windowSzDescSet;

        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.pNext                       = VK_NULL_HANDLE;
        allocInfo.sType                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool              = v->frameDescriptorPools[v->currentFrame];
        allocInfo.descriptorSetCount          = 1;
        allocInfo.pSetLayouts                 = &v->windowSizeDescriptorSetLayout;
        VkResult        result = vkAllocateDescriptorSets(v->device, &allocInfo, &windowSzDescSet);

        VkWriteDescriptorSet bufferWrite = VkWriteDescriptorSet {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = windowSzDescSet,
            .dstBinding       = 0,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pImageInfo       = nullptr,
            .pBufferInfo      = &bufferInfo,
            .pTexelBufferView = nullptr //unused for now
        };

        vkUpdateDescriptorSets(v->device, 1, &bufferWrite, 0, nullptr);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, v->quadPipelineLayout, 1, 1, &windowSzDescSet, 0, nullptr);
    }
    // allocate object buffer
    {
        VkBufferCreateInfo bufferCreateInfo = {};
        bufferCreateInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.pNext              = nullptr;
        bufferCreateInfo.size               = sizeof(RenderData::Object) * data.objects.size();
        bufferCreateInfo.usage              = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

        VmaAllocationCreateInfo vmaallocInfo = {};
        vmaallocInfo.usage                   = VMA_MEMORY_USAGE_CPU_TO_GPU;
        vmaallocInfo.flags                   = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo allocationInfo;

        VkBuffer objBuf;
        VmaAllocation objBufAlloc;
        vmaCreateBuffer(v->allocator, &bufferCreateInfo, &vmaallocInfo, &objBuf, &objBufAlloc, &allocationInfo);

        buffersToDestroy.push_back(objBuf);
        allocationsToDestroy.push_back(objBufAlloc);

        vmaCopyMemoryToAllocation(v->allocator, data.objects.data(), objBufAlloc, 0, data.objects.size() * sizeof(RenderData::Object));

        VkDescriptorSet objDescSet;
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.pNext                       = VK_NULL_HANDLE;
        allocInfo.sType                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool              = v->frameDescriptorPools[v->currentFrame];
        allocInfo.descriptorSetCount          = 1;
        allocInfo.pSetLayouts                 = &v->storageDescriptorSetLayout;
        VkResult        result = vkAllocateDescriptorSets(v->device, &allocInfo, &objDescSet);

        VkDescriptorBufferInfo bufferInfo = {
            .buffer = objBuf,
            .offset = 0,
            .range = data.objects.size() * sizeof(RenderData::Object)
        };

        VkWriteDescriptorSet bufferWrite = VkWriteDescriptorSet {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = objDescSet,
            .dstBinding       = 0,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo       = nullptr,
            .pBufferInfo      = &bufferInfo,
            .pTexelBufferView = nullptr //unused for now
        };

        vkUpdateDescriptorSets(v->device, 1, &bufferWrite, 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, v->quadPipelineLayout, 0, 1, &objDescSet, 0, nullptr);
    }
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, v->quadPipelineLayout, 2, 1, &v->samplerDescriptorSet, 0, nullptr);

    int count = 0;
    for (auto& c : data.commands)
    {
        //#TODO: chaange scissor method (current method leads to gpu branching)
        fbox sb = c.scissor;
        sb.x -= v->windowSize.x / 2.f;
        sb.y = v->windowSize.y / 2.f - c.scissor.y - c.scissor.height;
        sb.pos /= vec2(v->windowSize.x/2.f, v->windowSize.y/2.f);
        sb.size = c.scissor.size / vec2(v->windowSize.x/2.f, v->windowSize.y/2.f);
        sb.y *= -1;
        pushConstants.bounds = sb;
        pushConstants.index = count;

        switch (c.type)
        {
            case RenderData::Command::QUAD:
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, v->quadPipeline);

                pushConstants.texID = v->whiteTextureID;
                pushConstants.atlasWidth = 1;
                pushConstants.atlasHeight = 1;
                vkCmdPushConstants(cmd, v->quadPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);
                vkCmdDraw(cmd, 6, c.number, 0, 0);
                break;
            case RenderData::Command::TEXTURE:
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, v->quadPipeline);
                pushConstants.texID = c.texture.texture->id;
                if (getBit(c.texture.state, STATE_PRESS) && c.texture.texture->press != -1)
                    pushConstants.texID = c.texture.texture->press;
                if (getBit(c.texture.state, STATE_HOVER) && c.texture.texture->hover != -1)
                    pushConstants.texID = c.texture.texture->hover;
                if (getBit(c.texture.state, STATE_ACTIVE) && c.texture.texture->active != -1)
                    pushConstants.texID = c.texture.texture->active;
                pushConstants.atlasWidth = c.texture.texture->size.x;
                pushConstants.atlasHeight = c.texture.texture->size.y;
                vkCmdPushConstants(cmd, v->quadPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);
                vkCmdDraw(cmd, 6, c.number, 0, 0);
                break;
            case RenderData::Command::CHARACTER:
            {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, v->textPipeline);
                pushConstants.texID = c.character.font->textureIndex;
                pushConstants.atlasWidth = c.character.font->atlasWidth;
                pushConstants.atlasHeight = c.character.font->atlasHeight;
                pushConstants.pxRange = c.character.font->msdfPxRange;
                pushConstants.fontPx = c.character.font->baseFontSize;
                vkCmdPushConstants(cmd, v->quadPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);
                vkCmdDraw(cmd, 6, c.number, 0, 0);
                break;
            }
            default:
                break;
        }

        count += c.number;
    }
}

static void newFrame_Vulkan()
{
    //#TODO: wrong if more than one image count, can delete buffers while theyre being used by another frame
    TexGui_ImplVulkan_Data* v = (TexGui_ImplVulkan_Data*)(GTexGui->rendererData);
    v->currentFrame++;
    if (v->currentFrame == v->imageCount) v->currentFrame = 0;
    vkResetDescriptorPool(v->device, v->frameDescriptorPools[v->currentFrame], 0);
    for (int i = buffersToDestroy.size() - 1; i >= 0; i--)
    {
        vmaDestroyBuffer(v->allocator, buffersToDestroy[i], allocationsToDestroy[i]);
    }

    buffersToDestroy.clear();
    allocationsToDestroy.clear();
}

void renderClean_Vulkan()
{
    TexGui_ImplVulkan_Data* v = (TexGui_ImplVulkan_Data*)(GTexGui->rendererData);
    vkDestroyDescriptorPool(v->device, v->globalDescriptorPool, 0);
    for (auto& dp : v->frameDescriptorPools)
        vkDestroyDescriptorPool(v->device, dp, 0);

    for (int i = buffersToDestroy.size() - 1; i >= 0; i--)
    {
        vmaDestroyBuffer(v->allocator, buffersToDestroy[i], allocationsToDestroy[i]);
    }
    buffersToDestroy.clear();
    allocationsToDestroy.clear();
    //#TODO: need to destroy all textures here
}


bool TexGui::initVulkan(VulkanInitInfo& info)
{
    assert(!GTexGui->rendererData);
    GTexGui->rendererData = new TexGui_ImplVulkan_Data(info);
    GTexGui->rendererFns.createTexture = createTexture_Vulkan;
    GTexGui->rendererFns.renderClean = renderClean_Vulkan;
    GTexGui->rendererFns.framebufferSizeCallback = framebufferSizeCallback_Vulkan;
    GTexGui->rendererFns.newFrame = newFrame_Vulkan;

    createPipelines_Vulkan();
    createWhiteTexture_Vulkan();
    //#TODO: need to delete more stuff probably
    return true;
} 

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
