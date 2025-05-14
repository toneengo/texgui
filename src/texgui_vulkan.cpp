#include "types.h"
#include "GLFW/glfw3.h"

#include "texgui.h"
#include "texgui_vulkan.hpp"
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
#include "vk_mem_alloc.h"

using namespace TexGui;
using namespace TexGui::Math;

std::unordered_map<int, Math::ivec2> textureSizes;

VulkanContext* vulkanCtx;

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


VkShaderModule VulkanContext::createShaderModule(int stage, const std::string& shaderSource) {
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
} pushConstants;

extern Math::ivec2 _getScreenSize();
constexpr int MAX_SAMPLERS = 65536;
VulkanContext::VulkanContext(const VulkanInitInfo& init_info)
{
    windowSize = _getScreenSize();
    device = init_info.Device;
    physicalDevice = init_info.PhysicalDevice;

    instance = init_info.Instance;
    //debugMessenger = info.DebugMessenger;
    graphicsQueue = init_info.Queue;
    graphicsQueueFamily = init_info.QueueFamily;

    allocator = init_info.Allocator;

    imageCount = init_info.ImageCount;

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
        pool_info.maxSets                    = 0;

        for (VkDescriptorPoolSize& pool_size : pool_sizes)
            pool_info.maxSets += pool_size.descriptorCount;

        pool_info.poolSizeCount = sizeof(pool_sizes) / sizeof(pool_sizes[0]);
        pool_info.pPoolSizes    = pool_sizes;

        vkCreateDescriptorPool(device, &pool_info, nullptr, &globalDescriptorPool);
    }

    frameDescriptorPools.resize(imageCount);
    for (int i = 0; i < imageCount; i++)
    {
        VkDescriptorPoolSize pool_sizes[] = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags                      = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        pool_info.maxSets                    = 0;

        for (VkDescriptorPoolSize& pool_size : pool_sizes)
            pool_info.maxSets += pool_size.descriptorCount;

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

    set_info.pNext                           = VK_NULL_HANDLE;
    set_info.bindingCount                    = 1;

    set_info.pBindings                       = &bindings[0];
    set_info.flags                           = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    vkCreateDescriptorSetLayout(device, &set_info, nullptr, &storageDescriptorSetLayout);

    set_info.pBindings                       = &bindings[1];
    set_info.flags                           = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    vkCreateDescriptorSetLayout(device, &set_info, nullptr, &windowSizeDescriptorSetLayout);

    set_info.pBindings                       = &bindings[2];
    set_info.flags                           = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    vkCreateDescriptorSetLayout(device, &set_info, nullptr, &pxRangeDescriptorSetLayout);

    set_info.pBindings                       = &bindings[3];
    set_info.flags                           = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    vkCreateDescriptorSetLayout(device, &set_info, nullptr, &imageDescriptorSetLayout);

    {
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.pNext                       = VK_NULL_HANDLE;
        allocInfo.sType                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool              = globalDescriptorPool;
        allocInfo.descriptorSetCount          = 1;
        allocInfo.pSetLayouts                 = &imageDescriptorSetLayout;
        VkResult        result = vkAllocateDescriptorSets(device, &allocInfo, &imageDescriptorSet);

        allocInfo.pSetLayouts = &pxRangeDescriptorSetLayout;
        result = vkAllocateDescriptorSets(device, &allocInfo, &pxRangeDescriptorSet);
    }

    VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(PushConstants)
    };

    VkDescriptorSetLayout dsl[] = {storageDescriptorSetLayout, windowSizeDescriptorSetLayout, pxRangeDescriptorSetLayout, imageDescriptorSetLayout};
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pNext                  = nullptr;
    layoutInfo.pSetLayouts            = dsl;
    layoutInfo.setLayoutCount         = sizeof(dsl)/sizeof(dsl[0]);
    layoutInfo.pPushConstantRanges    = &pushConstantRange;
    layoutInfo.pushConstantRangeCount = 1;
    vkCreatePipelineLayout(device, &layoutInfo, nullptr, &quadPipelineLayout);
    assert(quadPipelineLayout != VK_NULL_HANDLE);

    {
        VkGraphicsPipelineCreateInfo info = {};
        info.sType                        = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        std::vector<VkFormat> colorAttachmentFormats;
        VkPipelineRenderingCreateInfo renderingCreateInfo = init_info.PipelineRenderingCreateInfo;

        std::vector<VkPipelineShaderStageCreateInfo> quadstages;
        std::vector<VkPipelineShaderStageCreateInfo> textstages;
        VkShaderModule quadvert = createShaderModule(EShLangVertex, VK_QUADVERT);
        VkShaderModule quadfrag = createShaderModule(EShLangFragment, VK_QUADFRAG);
        VkShaderModule textvert = createShaderModule(EShLangVertex, VK_TEXTVERT);
        VkShaderModule textfrag = createShaderModule(EShLangFragment, VK_TEXTFRAG);
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
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
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
        info.layout                         = quadPipelineLayout;
        //the rest are unused parameters

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &quadPipeline) != VK_SUCCESS) {
            printf("Failed to create pipeline\n");
            assert(false);
        }

        info.pStages = textstages.data();

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &textPipeline) != VK_SUCCESS) {
            printf("Failed to create pipeline\n");
            assert(false);
        }

        vkDestroyShaderModule(device, quadvert, nullptr);
        vkDestroyShaderModule(device, quadfrag, nullptr);
        vkDestroyShaderModule(device, textvert, nullptr);
        vkDestroyShaderModule(device, textfrag, nullptr);
    }
    // allocate buffer
    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.pNext              = nullptr;
    bufferCreateInfo.size               = sizeof(Math::ivec2);
    bufferCreateInfo.usage              = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage                   = VMA_MEMORY_USAGE_CPU_TO_GPU;
    vmaallocInfo.flags                   = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo allocationInfo;
    vmaCreateBuffer(allocator, &bufferCreateInfo, &vmaallocInfo, &windowSizeBuffer, &windowSizeBufferAllocation, &allocationInfo);

    bufferCreateInfo.size               = sizeof(float);
    vmaCreateBuffer(allocator, &bufferCreateInfo, &vmaallocInfo, &pxRangeBuffer, &pxRangeBufferAllocation, &allocationInfo);

    bufferCreateInfo.size               = sizeof(RenderData::Object) * (1 << 16);
    bufferCreateInfo.usage              = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    vmaCreateBuffer(allocator, &bufferCreateInfo, &vmaallocInfo, &storageBuffer, &storageBufferAllocation, &allocationInfo);

    //need to delete more stuff
}

VulkanContext::~VulkanContext()
{
}

void VulkanContext::setPxRange(float _pxRange)
{
    pxRange = _pxRange;
    vmaCopyMemoryToAllocation(allocator, &_pxRange, pxRangeBufferAllocation, 0, sizeof(float));
    VkDescriptorBufferInfo bufferInfo = {
        .buffer = pxRangeBuffer,
        .offset = 0,
        .range = sizeof(float)
    };

    VkWriteDescriptorSet bufferWrite = VkWriteDescriptorSet {
        .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext            = nullptr,
        .dstSet           = pxRangeDescriptorSet,
        .dstBinding       = 0,
        .dstArrayElement  = 0,
        .descriptorCount  = 1,
        .descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImageInfo       = nullptr,
        .pBufferInfo      = &bufferInfo,
        .pTexelBufferView = nullptr //unused for now
    };

    vkUpdateDescriptorSets(device, 1, &bufferWrite, 0, nullptr);
}

void VulkanContext::initFromGlfwWindow(GLFWwindow* window)
{
    glfwGetWindowContentScale(window, &windowScale, nullptr);
    glfwGetWindowSize(window, &windowSize.x, &windowSize.y);
    windowSize.x *= windowScale;
    windowSize.y *= windowScale; 
    Base.bounds.size = windowSize;
}

void VulkanContext::setScreenSize(int width, int height)
{
    windowSize.x = width;
    windowSize.y = height;
}

uint32_t VulkanContext::createTexture(void* data, int width, int height)
{
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

    vmaCreateBuffer(allocator, &bufferInfo, &vmaallocInfo, &uploadBuffer, &allocation, &allocationInfo);
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
    vmaCreateImage(allocator, &img_info, &allocinfo, &image, &imageAllocation, nullptr);
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
    vkCreateImageView(device, &info, nullptr, &iv);

    vkResetFences(device, 1, &immCommandFence);
    vkResetCommandBuffer(immCommandBuffer, 0);
    VkCommandBufferBeginInfo cmdBeginInfo = {};
    cmdBeginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBeginInfo.pNext                    = nullptr;
    cmdBeginInfo.pInheritanceInfo         = nullptr;
    cmdBeginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(immCommandBuffer, &cmdBeginInfo);

    image_barrier(immCommandBuffer, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

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
    vkCmdCopyBufferToImage(immCommandBuffer, uploadBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    image_barrier(immCommandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkEndCommandBuffer(immCommandBuffer);
    VkCommandBufferSubmitInfo cmdinfo{};
    cmdinfo.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdinfo.pNext         = nullptr;
    cmdinfo.commandBuffer = immCommandBuffer;
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
    vkQueueSubmit2(graphicsQueue, 1, &submit, immCommandFence);
    vkWaitForFences(device, 1, &immCommandFence, true, 9999999999);

    VkDescriptorImageInfo imgInfo{
        .sampler = nearestSampler,
        .imageView = iv,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    auto imageWrite = VkWriteDescriptorSet{
        .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext            = nullptr,
        .dstSet           = imageDescriptorSet,
        .dstBinding       = 0,
        .dstArrayElement  = textureCount,
        .descriptorCount  = 1,
        .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo       = &imgInfo,
        .pBufferInfo      = nullptr,
        .pTexelBufferView = nullptr //unused for now
    };
    vkUpdateDescriptorSets(device, 1, &imageWrite, 0, nullptr);

    textureSizes[textureCount] = {width, height};
    vmaDestroyBuffer(allocator, uploadBuffer, allocation);

    return textureCount++;
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
        .imageMemoryBarrierCount = 0,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &bufferBarrier
    };

    vkCmdPipelineBarrier2(cmd, &depInfo);
}

void VulkanContext::renderFromRD(const RenderData& data)
{
    // set dynamic viewport and scissor
    VkViewport viewport = {};
    viewport.x          = 0;
    viewport.y          = 0;
    viewport.width      = windowSize.x;
    viewport.height     = windowSize.y;
    viewport.minDepth   = 0;
    viewport.maxDepth   = 1;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor      = {};
    scissor.offset.x      = 0;
    scissor.offset.y      = 0;
    scissor.extent.width  = windowSize.x;
    scissor.extent.height = windowSize.y;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    //update window size uniform
    {
        vmaCopyMemoryToAllocation(allocator, &windowSize, windowSizeBufferAllocation, 0, sizeof(Math::ivec2));
        VkDescriptorBufferInfo bufferInfo = {
            .buffer = windowSizeBuffer,
            .offset = 0,
            .range = sizeof(Math::ivec2)
        };

        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.pNext                       = VK_NULL_HANDLE;
        allocInfo.sType                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool              = frameDescriptorPools[currentFrame];
        allocInfo.descriptorSetCount          = 1;
        allocInfo.pSetLayouts                 = &windowSizeDescriptorSetLayout;
        VkResult        result = vkAllocateDescriptorSets(device, &allocInfo, &windowSizeDescriptorSet);

        VkWriteDescriptorSet bufferWrite = VkWriteDescriptorSet {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = windowSizeDescriptorSet,
            .dstBinding       = 0,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pImageInfo       = nullptr,
            .pBufferInfo      = &bufferInfo,
            .pTexelBufferView = nullptr //unused for now
        };

        vkUpdateDescriptorSets(device, 1, &bufferWrite, 0, nullptr);
    }
    // allocate object buffer
    {
        vmaCopyMemoryToAllocation(allocator, data.objects.data(), storageBufferAllocation, 0, data.objects.size() * sizeof(RenderData::Object));
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.pNext                       = VK_NULL_HANDLE;
        allocInfo.sType                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool              = frameDescriptorPools[currentFrame];
        allocInfo.descriptorSetCount          = 1;
        allocInfo.pSetLayouts                 = &storageDescriptorSetLayout;
        VkResult        result = vkAllocateDescriptorSets(device, &allocInfo, &storageDescriptorSet);

        VkDescriptorBufferInfo bufferInfo = {
            .buffer = storageBuffer,
            .offset = 0,
            .range = data.objects.size() * sizeof(RenderData::Object)
        };

        VkWriteDescriptorSet bufferWrite = VkWriteDescriptorSet {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = storageDescriptorSet,
            .dstBinding       = 0,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo       = nullptr,
            .pBufferInfo      = &bufferInfo,
            .pTexelBufferView = nullptr //unused for now
        };

        vkUpdateDescriptorSets(device, 1, &bufferWrite, 0, nullptr);
        buffer_barrier(commandBuffer, storageBuffer, 0, data.objects.size() * sizeof(RenderData::Object));
    }

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, quadPipelineLayout, 0, 1, &storageDescriptorSet, 0, nullptr);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, quadPipelineLayout, 1, 1, &windowSizeDescriptorSet, 0, nullptr);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, quadPipelineLayout, 2, 1, &pxRangeDescriptorSet, 0, nullptr);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, quadPipelineLayout, 3, 1, &imageDescriptorSet, 0, nullptr);

    int count = 0;
    for (auto& c : data.commands)
    {
        switch (c.type)
        {
            case RenderData::Command::QUAD:
            {
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, quadPipeline);
                fbox sb = c.scissorBox;
                sb.x -= windowSize.x / 2.f;
                sb.y = windowSize.y / 2.f - c.scissorBox.y - c.scissorBox.height;
                sb.pos /= vec2(windowSize.x/2.f, windowSize.y/2.f);
                sb.size = c.scissorBox.size / vec2(windowSize.x/2.f, windowSize.y/2.f);
                sb.y *= -1;

                pushConstants.bounds = sb;
                pushConstants.index = count;
                int texID = c.texture->id;
                if (getBit(c.state, STATE_PRESS) && c.texture->press != -1)
                    texID = c.texture->press;
                if (getBit(c.state, STATE_HOVER) && c.texture->hover != -1)
                    texID = c.texture->hover;
                if (getBit(c.state, STATE_ACTIVE) && c.texture->active != -1)
                    texID = c.texture->active;
                pushConstants.texID = texID;
                pushConstants.atlasWidth = c.texture->size.x;
                pushConstants.atlasHeight = c.texture->size.y;
                vkCmdPushConstants(commandBuffer, quadPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);
                vkCmdDraw(commandBuffer, 6, c.number, 0, 0);
                break;
            }
            case RenderData::Command::CHARACTER:
            {
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, textPipeline);
                pushConstants.index = count;
                pushConstants.texID = fontAtlasID;
                pushConstants.atlasWidth = fontAtlasWidth;
                pushConstants.atlasHeight = fontAtlasHeight;
                vkCmdPushConstants(commandBuffer, quadPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);
                vkCmdDraw(commandBuffer, 6, c.number, 0, 0);
                break;
            }
            default:
                break;
        }

        count += c.number;
    }
}

Math::ivec2 VulkanContext::getTextureSize(uint32_t texID)
{
    return textureSizes[texID];
}

void VulkanContext::newFrame()
{
    currentFrame++;
    if (currentFrame == imageCount) currentFrame = 0;
    vkResetDescriptorPool(device, frameDescriptorPools[currentFrame], 0);
}

void VulkanContext::clean()
{
    vkDestroyDescriptorPool(device, globalDescriptorPool, 0);
    for (auto& dp : frameDescriptorPools)
        vkDestroyDescriptorPool(device, dp, 0);

    vmaDestroyBuffer(allocator, windowSizeBuffer, windowSizeBufferAllocation);
    vmaDestroyBuffer(allocator, storageBuffer, storageBufferAllocation);
    //#TODO: need to destroy all textures here
}

extern void _setRenderCtx(NoApiContext* ptr);
bool TexGui::initVulkan(VulkanInitInfo& info)
{
    vulkanCtx = new VulkanContext(info);
    _setRenderCtx(vulkanCtx);
    return true;
} 

void TexGui::renderVulkan(const TexGui::RenderData& data, VkCommandBuffer cmd)
{
    vulkanCtx->setCommandBuffer(cmd);
    vulkanCtx->renderFromRD(data);
}
