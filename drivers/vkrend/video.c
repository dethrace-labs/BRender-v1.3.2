#include "brassert.h"
#include "drv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vk_shaders.h"

#define VK_CHECK_RESULT(result) VK_AssertOrWarnIfError(result, __FILE__, __LINE__, #result)

static VkBool32 debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* cbData, void* userData) {
    (void)type;
    (void)userData;
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        BrWarning("VULKAN: %s\n", cbData->pMessage);
    }
    return VK_FALSE;
}

static int ratePhysicalDevice(VkPhysicalDevice device, VkSurfaceKHR surface,
    uint32_t* graphicsFamily, uint32_t* presentFamily) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);
    VkQueueFamilyProperties* families = malloc(sizeof(VkQueueFamilyProperties) * queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, families);

    int foundGraphics = 0, foundPresent = 0;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            *graphicsFamily = i;
            foundGraphics = 1;
        }
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) {
            *presentFamily = i;
            foundPresent = 1;
        }
    }
    free(families);

    if (!foundGraphics || !foundPresent)
        return 0;

    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extCount, NULL);
    VkExtensionProperties* exts = malloc(sizeof(VkExtensionProperties) * extCount);
    vkEnumerateDeviceExtensionProperties(device, NULL, &extCount, exts);

    int hasSwapchain = 0;
    for (uint32_t i = 0; i < extCount; i++) {
        if (strcmp(exts[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            hasSwapchain = 1;
            break;
        }
    }
    free(exts);

    if (!hasSwapchain)
        return 0;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);

    int score = 1;
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        score += 1000;

    return score;
}

static VkSurfaceFormatKHR chooseSurfaceFormat(VkPhysicalDevice device, VkSurfaceKHR surface) {
    uint32_t count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, NULL);
    VkSurfaceFormatKHR* formats = malloc(sizeof(VkSurfaceFormatKHR) * count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, formats);

    VkSurfaceFormatKHR result = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    for (uint32_t i = 0; i < count; i++) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            result = formats[i];
            break;
        }
    }
    free(formats);
    return result;
}

static VkPresentModeKHR choosePresentMode(VkPhysicalDevice device, VkSurfaceKHR surface) {
    uint32_t count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, NULL);
    VkPresentModeKHR* modes = malloc(sizeof(VkPresentModeKHR) * count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, modes);

    VkPresentModeKHR result = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32_t i = 0; i < count; i++) {
        if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            result = modes[i];
            break;
        }
        if (modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            result = modes[i];
        }
    }
    free(modes);
    return result;
}

static VkExtent2D chooseExtent(VkPhysicalDevice device, VkSurfaceKHR surface, int width, int height) {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &caps);

    if (caps.currentExtent.width != UINT32_MAX) {
        return caps.currentExtent;
    }
    VkExtent2D extent = {width, height};
    extent.width = caps.minImageExtent.width > extent.width ? caps.minImageExtent.width : extent.width;
    extent.width = caps.maxImageExtent.width < extent.width ? caps.maxImageExtent.width : extent.width;
    extent.height = caps.minImageExtent.height > extent.height ? caps.minImageExtent.height : extent.height;
    extent.height = caps.maxImageExtent.height < extent.height ? caps.maxImageExtent.height : extent.height;
    return extent;
}

static VkResult CreateSwapchain(HVIDEO hVideo, int width, int height) {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(hVideo->physicalDevice, hVideo->surface, &caps);

    VkSurfaceFormatKHR format = chooseSurfaceFormat(hVideo->physicalDevice, hVideo->surface);
    VkPresentModeKHR presentMode = choosePresentMode(hVideo->physicalDevice, hVideo->surface);
    VkExtent2D extent = chooseExtent(hVideo->physicalDevice, hVideo->surface, width, height);

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
        imageCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR ci = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    ci.surface = hVideo->surface;
    ci.minImageCount = imageCount;
    ci.imageFormat = format.format;
    ci.imageColorSpace = format.colorSpace;
    ci.imageExtent = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = {hVideo->graphicsFamilyIndex, hVideo->presentFamilyIndex};
    if (hVideo->graphicsFamilyIndex != hVideo->presentFamilyIndex) {
        ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ci.queueFamilyIndexCount = 0;
        ci.pQueueFamilyIndices = NULL;
    }

    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = presentMode;
    ci.clipped = VK_TRUE;
    ci.oldSwapchain = hVideo->swapchain;

    VkResult res = vkCreateSwapchainKHR(hVideo->device, &ci, NULL, &hVideo->swapchain);
    if (res != VK_SUCCESS) {
        BR_FATAL1("Vulkan: Failed to create swapchain (0x%x)", (int)res);
        return res;
    }

    hVideo->swapchainImageFormat = format.format;
    hVideo->swapchainExtent = extent;

    vkGetSwapchainImagesKHR(hVideo->device, hVideo->swapchain, &hVideo->swapchainImageCount, NULL);
    if (hVideo->swapchainImages)
        free(hVideo->swapchainImages);
    hVideo->swapchainImages = malloc(sizeof(VkImage) * hVideo->swapchainImageCount);
    vkGetSwapchainImagesKHR(hVideo->device, hVideo->swapchain, &hVideo->swapchainImageCount, hVideo->swapchainImages);

    return VK_SUCCESS;
}

static VkResult CreateImageViews(HVIDEO hVideo) {
    if (hVideo->swapchainImageViews)
        BrResFree(hVideo->swapchainImageViews);

    hVideo->swapchainImageViews = BrResAllocate(hVideo->res, sizeof(VkImageView) * hVideo->swapchainImageCount, BR_MEMORY_DRIVER);

    for (uint32_t i = 0; i < hVideo->swapchainImageCount; i++) {
        VkImageViewCreateInfo ci = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ci.image = hVideo->swapchainImages[i];
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format = hVideo->swapchainImageFormat;
        ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ci.subresourceRange.baseMipLevel = 0;
        ci.subresourceRange.levelCount = 1;
        ci.subresourceRange.baseArrayLayer = 0;
        ci.subresourceRange.layerCount = 1;

        VkResult res = vkCreateImageView(hVideo->device, &ci, NULL, &hVideo->swapchainImageViews[i]);
        if (res != VK_SUCCESS) {
            BR_FATAL1("Vulkan: Failed to create image view (0x%x)", (int)res);
            return res;
        }
    }
    return VK_SUCCESS;
}

static VkFormat FindDepthFormat(HVIDEO hVideo) {
    VkFormat candidates[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM};
    for (int i = 0; i < 3; i++) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(hVideo->physicalDevice, candidates[i], &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            return candidates[i];
    }
    return VK_FORMAT_UNDEFINED;
}

static VkResult CreateDepthResources(HVIDEO hVideo) {
    hVideo->depthFormat = FindDepthFormat(hVideo);
    if (hVideo->depthFormat == VK_FORMAT_UNDEFINED) {
        BR_FATAL0("Vulkan: No supported depth format found.");
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = hVideo->depthFormat;
    ii.extent.width = hVideo->swapchainExtent.width;
    ii.extent.height = hVideo->swapchainExtent.height;
    ii.extent.depth = 1;
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult res = vkCreateImage(hVideo->device, &ii, NULL, &hVideo->depthImage);
    if (res != VK_SUCCESS) return res;

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(hVideo->device, hVideo->depthImage, &memReq);

    VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = memReq.size;
    ai.memoryTypeIndex = VK_FindMemoryType(hVideo->physicalDevice, memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (ai.memoryTypeIndex == UINT32_MAX) BR_FATAL0("Vulkan: No suitable memory type for depth image");

    res = vkAllocateMemory(hVideo->device, &ai, NULL, &hVideo->depthMemory);
    if (res != VK_SUCCESS) {
        vkDestroyImage(hVideo->device, hVideo->depthImage, NULL);
        hVideo->depthImage = VK_NULL_HANDLE;
        return res;
    }
    vkBindImageMemory(hVideo->device, hVideo->depthImage, hVideo->depthMemory, 0);

    VkImageViewCreateInfo ivi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    ivi.image = hVideo->depthImage;
    ivi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivi.format = hVideo->depthFormat;
    ivi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    ivi.subresourceRange.baseMipLevel = 0;
    ivi.subresourceRange.levelCount = 1;
    ivi.subresourceRange.baseArrayLayer = 0;
    ivi.subresourceRange.layerCount = 1;

    res = vkCreateImageView(hVideo->device, &ivi, NULL, &hVideo->depthImageView);
    if (res != VK_SUCCESS) {
        vkFreeMemory(hVideo->device, hVideo->depthMemory, NULL);
        vkDestroyImage(hVideo->device, hVideo->depthImage, NULL);
        hVideo->depthImageView = VK_NULL_HANDLE;
        hVideo->depthMemory = VK_NULL_HANDLE;
        hVideo->depthImage = VK_NULL_HANDLE;
        return res;
    }

    return VK_SUCCESS;
}

static VkResult CreateImGuiCompatRenderPass(HVIDEO hVideo) {
    VkAttachmentDescription colorAttachment = {0};
    colorAttachment.format = hVideo->swapchainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass = {0};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkRenderPassCreateInfo ci = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    ci.attachmentCount = 1;
    ci.pAttachments = &colorAttachment;
    ci.subpassCount = 1;
    ci.pSubpasses = &subpass;

    return vkCreateRenderPass(hVideo->device, &ci, NULL, &hVideo->imguiCompatRenderPass);
}

static VkResult CreateCommandPool(HVIDEO hVideo) {
    VkCommandPoolCreateInfo ci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    ci.queueFamilyIndex = hVideo->graphicsFamilyIndex;
    return vkCreateCommandPool(hVideo->device, &ci, NULL, &hVideo->commandPool);
}

static VkResult CreateCommandBuffers(HVIDEO hVideo) {
    hVideo->drawCommandBuffers = BrResAllocate(hVideo->res, sizeof(VkCommandBuffer) * hVideo->swapchainImageCount, BR_MEMORY_DRIVER);

    VkCommandBufferAllocateInfo ai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = hVideo->commandPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = hVideo->swapchainImageCount;

    return vkAllocateCommandBuffers(hVideo->device, &ai, hVideo->drawCommandBuffers);
}

static VkResult CreateSyncObjects(HVIDEO hVideo) {
    VkSemaphoreCreateInfo si = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fi = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(hVideo->device, &si, NULL, &hVideo->imageAvailableSemaphores[i]) != VK_SUCCESS)
            return VK_ERROR_INITIALIZATION_FAILED;
        if (vkCreateSemaphore(hVideo->device, &si, NULL, &hVideo->renderFinishedSemaphores[i]) != VK_SUCCESS)
            return VK_ERROR_INITIALIZATION_FAILED;
        if (vkCreateFence(hVideo->device, &fi, NULL, &hVideo->inFlightFences[i]) != VK_SUCCESS)
            return VK_ERROR_INITIALIZATION_FAILED;
    }

    return VK_SUCCESS;
}

VkShaderModule VK_CreateShaderModule(HVIDEO hVideo, const char* code, size_t code_size, VkShaderStageFlagBits stage) {
    (void)hVideo;
    (void)stage;
    VkShaderModuleCreateInfo ci = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = code_size;
    ci.pCode = (const uint32_t*)code;

    VkShaderModule module;
    VkResult res = vkCreateShaderModule(hVideo->device, &ci, NULL, &module);
    if (res != VK_SUCCESS) {
        BR_FATAL1("Vulkan: Failed to create shader module (0x%x)", (int)res);
        return VK_NULL_HANDLE;
    }
    return module;
}

VkPipelineLayout VK_CreatePipelineLayout(HVIDEO hVideo, VkDescriptorSetLayout* descLayout, uint32_t descLayoutCount) {
    VkPipelineLayoutCreateInfo ci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    ci.setLayoutCount = descLayoutCount;
    ci.pSetLayouts = descLayout;

    VkPipelineLayout layout;
    VkResult res = vkCreatePipelineLayout(hVideo->device, &ci, NULL, &layout);
    if (res != VK_SUCCESS) {
        BR_FATAL1("Vulkan: Failed to create pipeline layout (0x%x)", (int)res);
        return VK_NULL_HANDLE;
    }
    return layout;
}

VkPipeline VK_CreateGraphicsPipeline(HVIDEO hVideo, VkShaderModule vertModule, VkShaderModule fragModule,
    VkPipelineLayout layout,
    VkVertexInputBindingDescription* bindingDesc,
    VkVertexInputAttributeDescription* attrDescs, uint32_t attrCount,
    uint32_t width, uint32_t height, VkBool32 blendEnable, VkBool32 depthTestEnable, VkBool32 depthWriteEnable) {

    VkPipelineShaderStageCreateInfo stages[2] = {0};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertModule;
    stages[0].pName = "main";

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragModule;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vertexInput = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInput.vertexBindingDescriptionCount = bindingDesc ? 1 : 0;
    vertexInput.pVertexBindingDescriptions = bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = attrCount;
    vertexInput.pVertexAttributeDescriptions = attrDescs;

    VkPipelineInputAssemblyStateCreateInfo assembly = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkDynamicState dynStates[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynState = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynState.dynamicStateCount = 2;
    dynState.pDynamicStates = dynStates;

    VkViewport viewport = {0, 0, (float)width, (float)height, 0.0f, 1.0f};
    VkRect2D scissor = {{0, 0}, {width, height}};

    VkPipelineViewportStateCreateInfo viewportState = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthClampEnable = VK_TRUE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blend = {0};
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend.blendEnable = blendEnable;
    if (blendEnable) {
        blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend.colorBlendOp = VK_BLEND_OP_ADD;
        blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blend.alphaBlendOp = VK_BLEND_OP_ADD;
    }

    VkPipelineColorBlendStateCreateInfo blending = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blending.attachmentCount = 1;
    blending.pAttachments = &blend;

    VkPipelineDepthStencilStateCreateInfo depthStencil = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencil.depthTestEnable = depthTestEnable;
    depthStencil.depthWriteEnable = depthWriteEnable;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineRenderingCreateInfo renderingInfo = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &hVideo->swapchainImageFormat;
    renderingInfo.depthAttachmentFormat = hVideo->depthFormat;

    VkGraphicsPipelineCreateInfo ci = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    ci.pNext = &renderingInfo;
    ci.stageCount = 2;
    ci.pStages = stages;
    ci.pVertexInputState = &vertexInput;
    ci.pInputAssemblyState = &assembly;
    ci.pViewportState = &viewportState;
    ci.pRasterizationState = &rasterizer;
    ci.pMultisampleState = &multisampling;
    ci.pDepthStencilState = &depthStencil;
    ci.pColorBlendState = &blending;
    ci.pDynamicState = &dynState;
    ci.layout = layout;
    ci.renderPass = VK_NULL_HANDLE;

    VkPipeline pipeline;
    VkResult res = vkCreateGraphicsPipelines(hVideo->device, VK_NULL_HANDLE, 1, &ci, NULL, &pipeline);
    if (res != VK_SUCCESS) {
        BR_FATAL1("Vulkan: Failed to create graphics pipeline (0x%x)", (int)res);
        return VK_NULL_HANDLE;
    }
    return pipeline;
}

HVIDEO VK_VideoOpen(HVIDEO hVideo, void* parent, const char* vert_spv_data, size_t vert_spv_size,
    const char* frag_spv_data, size_t frag_spv_size,
    br_device_vk_callback_procs* callbacks, int width, int height) {
    if (hVideo == NULL) {
        BR_FATAL("VIDEO: Invalid handle.");
        return NULL;
    }

    memset(hVideo, 0, sizeof(VIDEO));
    hVideo->res = parent;

    VkApplicationInfo appInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "dethrace";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "BRender Vulkan";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    uint32_t instanceExtCount = 0;
    const char* instanceExts[8];

    if (callbacks->get_instance_extensions) {
        const char** exts = callbacks->get_instance_extensions(&instanceExtCount);
        if (exts && instanceExtCount > 0) {
            uint32_t copyCount = instanceExtCount < 8 ? instanceExtCount : 8;
            for (uint32_t i = 0; i < copyCount; i++)
                instanceExts[i] = exts[i];
        } else {
            instanceExtCount = 0;
        }
    }

    VkInstanceCreateInfo instCi = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instCi.pApplicationInfo = &appInfo;
    instCi.enabledExtensionCount = instanceExtCount;
    instCi.ppEnabledExtensionNames = instanceExtCount > 0 ? instanceExts : NULL;

    VkResult res = vkCreateInstance(&instCi, NULL, &hVideo->instance);
    if (res != VK_SUCCESS) {
        BR_FATAL1("Vulkan: Failed to create instance (0x%x).", (int)res);
        return NULL;
    }

    if (callbacks->create_surface) {
        hVideo->surface = callbacks->create_surface(hVideo->instance);
        if (hVideo->surface == VK_NULL_HANDLE) {
            BR_FATAL("Vulkan: Failed to create surface.");
            goto cleanup_instance;
        }
    } else {
        BR_FATAL("Vulkan: No create_surface callback provided.");
        goto cleanup_instance;
    }

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(hVideo->instance, &deviceCount, NULL);
    if (deviceCount == 0) {
        BR_FATAL("Vulkan: No physical devices found.");
        goto cleanup_instance;
    }

    VkPhysicalDevice* devices = malloc(sizeof(VkPhysicalDevice) * deviceCount);
    vkEnumeratePhysicalDevices(hVideo->instance, &deviceCount, devices);

    int bestScore = 0;
    int selected = -1;
    for (uint32_t i = 0; i < deviceCount; i++) {
        uint32_t gf, pf;
        int score = ratePhysicalDevice(devices[i], hVideo->surface, &gf, &pf);
        if (score > bestScore) {
            bestScore = score;
            selected = i;
            hVideo->physicalDevice = devices[i];
            hVideo->graphicsFamilyIndex = gf;
            hVideo->presentFamilyIndex = pf;
        }
    }
    free(devices);

    if (selected < 0) {
        BR_FATAL("Vulkan: No suitable physical device.");
        goto cleanup_instance;
    }

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(hVideo->physicalDevice, &props);
    BrLogPrintf("VKREND: Vulkan Device = %s\n", props.deviceName);
    hVideo->maxUniformBufferRange = props.limits.maxUniformBufferRange;
    hVideo->minUniformBufferOffsetAlignment = props.limits.minUniformBufferOffsetAlignment;
    hVideo->maxVertexInputBindings = props.limits.maxVertexInputBindings;
    hVideo->maxVertexInputAttributes = props.limits.maxVertexInputAttributes;

    // Cache the host-visible+coherent memory type once. VBO/IBO and staging
    // uploads are created repeatedly (model crush rebuilds, per-flush staging),
    // and vkGetPhysicalDeviceMemoryProperties is a heavyweight driver query.
    hVideo->hostMemType = VK_FindMemoryType(hVideo->physicalDevice, ~0u,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    float queuePriority = 1.0f;
    uint32_t uniqueFamilies[2] = {hVideo->graphicsFamilyIndex, hVideo->presentFamilyIndex};
    uint32_t uniqueCount = (hVideo->graphicsFamilyIndex == hVideo->presentFamilyIndex) ? 1 : 2;

    VkDeviceQueueCreateInfo queueCis[2];
    for (uint32_t i = 0; i < uniqueCount; i++) {
        queueCis[i] = (VkDeviceQueueCreateInfo){VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queueCis[i].queueFamilyIndex = uniqueFamilies[i];
        queueCis[i].queueCount = 1;
        queueCis[i].pQueuePriorities = &queuePriority;
    }

    const char* deviceExts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME};

    VkPhysicalDeviceDynamicRenderingFeatures dynFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES};
    dynFeatures.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceMaintenance4Features maintenance4Features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES};
    maintenance4Features.maintenance4 = VK_TRUE;

    VkPhysicalDeviceMaintenance5Features maintenance5Features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES};
    maintenance5Features.maintenance5 = VK_TRUE;

    dynFeatures.pNext = &maintenance4Features;
    maintenance4Features.pNext = &maintenance5Features;

    VkPhysicalDeviceFeatures devFeatures = {0};

    VkDeviceCreateInfo devCi = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    devCi.pNext = &dynFeatures;
    devCi.queueCreateInfoCount = uniqueCount;
    devCi.pQueueCreateInfos = queueCis;
    devCi.enabledExtensionCount = 2;
    devCi.ppEnabledExtensionNames = deviceExts;
    devCi.pEnabledFeatures = &devFeatures;

    res = vkCreateDevice(hVideo->physicalDevice, &devCi, NULL, &hVideo->device);
    if (res != VK_SUCCESS) {
        BR_FATAL("Vulkan: Failed to create logical device.");
        goto cleanup_instance;
    }

    vkGetDeviceQueue(hVideo->device, hVideo->graphicsFamilyIndex, 0, &hVideo->graphicsQueue);
    vkGetDeviceQueue(hVideo->device, hVideo->presentFamilyIndex, 0, &hVideo->presentQueue);

    hVideo->pfnPushDescriptorSet = (PFN_vkCmdPushDescriptorSetKHR)vkGetDeviceProcAddr(hVideo->device, "vkCmdPushDescriptorSetKHR");
    if (!hVideo->pfnPushDescriptorSet) {
        hVideo->pfnPushDescriptorSet = (PFN_vkCmdPushDescriptorSetKHR)vkGetDeviceProcAddr(hVideo->device, "vkCmdPushDescriptorSet");
    }

    if (CreateSwapchain(hVideo, width, height) != VK_SUCCESS)
        goto cleanup_device;

    if (CreateImageViews(hVideo) != VK_SUCCESS)
        goto cleanup_device;

    if (CreateDepthResources(hVideo) != VK_SUCCESS)
        goto cleanup_device;

    if (CreateImGuiCompatRenderPass(hVideo) != VK_SUCCESS)
        goto cleanup_device;

    if (CreateCommandPool(hVideo) != VK_SUCCESS)
        goto cleanup_device;

    VkShaderModule vertModule = VK_CreateShaderModule(hVideo, vert_spv_data, vert_spv_size, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderModule fragModule = VK_CreateShaderModule(hVideo, frag_spv_data, frag_spv_size, VK_SHADER_STAGE_FRAGMENT_BIT);
    if (!vertModule || !fragModule)
        goto cleanup_device;

    if (VK_CreateBrenderDescriptors(hVideo, width, height) != VK_SUCCESS)
        goto cleanup_shaders;

    if (VK_CreateDynamicRings(hVideo) != VK_SUCCESS)
        goto cleanup_shaders;

    hVideo->brenderPipelineLayout = VK_CreatePipelineLayout(hVideo, &hVideo->brenderDescriptors.layout, 1);
    if (!hVideo->brenderPipelineLayout)
        goto cleanup_shaders;

    VkVertexInputBindingDescription bindingDesc = {0};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(vk_vertex_f);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrDescs[4] = {0};
    attrDescs[0].location = 0;
    attrDescs[0].binding = 0;
    attrDescs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDescs[0].offset = offsetof(vk_vertex_f, p);
    attrDescs[1].location = 1;
    attrDescs[1].binding = 0;
    attrDescs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrDescs[1].offset = offsetof(vk_vertex_f, map);
    attrDescs[2].location = 2;
    attrDescs[2].binding = 0;
    attrDescs[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDescs[2].offset = offsetof(vk_vertex_f, n);
    attrDescs[3].location = 3;
    attrDescs[3].binding = 0;
    attrDescs[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrDescs[3].offset = offsetof(vk_vertex_f, c);

    hVideo->brenderPipeline = VK_CreateGraphicsPipeline(hVideo, vertModule, fragModule,
        hVideo->brenderPipelineLayout,
        &bindingDesc, attrDescs, 4,
        hVideo->swapchainExtent.width, hVideo->swapchainExtent.height, VK_FALSE, VK_TRUE, VK_TRUE);
    if (!hVideo->brenderPipeline)
        goto cleanup_shaders;

    hVideo->brenderPipelineNoWrite = VK_CreateGraphicsPipeline(hVideo, vertModule, fragModule,
        hVideo->brenderPipelineLayout,
        &bindingDesc, attrDescs, 4,
        hVideo->swapchainExtent.width, hVideo->swapchainExtent.height, VK_FALSE, VK_TRUE, VK_FALSE);
    if (!hVideo->brenderPipelineNoWrite)
        goto cleanup_shaders;

    hVideo->brenderBlendPipeline = VK_CreateGraphicsPipeline(hVideo, vertModule, fragModule,
        hVideo->brenderPipelineLayout,
        &bindingDesc, attrDescs, 4,
        hVideo->swapchainExtent.width, hVideo->swapchainExtent.height, VK_TRUE, VK_TRUE, VK_FALSE);
    if (!hVideo->brenderBlendPipeline)
        goto cleanup_shaders;

    hVideo->brenderPipelineNoDepth = VK_CreateGraphicsPipeline(hVideo, vertModule, fragModule,
        hVideo->brenderPipelineLayout,
        &bindingDesc, attrDescs, 4,
        hVideo->swapchainExtent.width, hVideo->swapchainExtent.height, VK_FALSE, VK_FALSE, VK_FALSE);
    if (!hVideo->brenderPipelineNoDepth)
        goto cleanup_shaders;

    hVideo->brenderBlendPipelineNoDepth = VK_CreateGraphicsPipeline(hVideo, vertModule, fragModule,
        hVideo->brenderPipelineLayout,
        &bindingDesc, attrDescs, 4,
        hVideo->swapchainExtent.width, hVideo->swapchainExtent.height, VK_TRUE, VK_FALSE, VK_FALSE);
    if (!hVideo->brenderBlendPipelineNoDepth)
        goto cleanup_shaders;

    hVideo->defaultPipeline = hVideo->brenderPipeline;
    hVideo->defaultPipelineLayout = hVideo->brenderPipelineLayout;

    {
        VkDescriptorSetLayoutBinding overlayBindings[1] = {0};
        overlayBindings[0].binding = 0;
        overlayBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        overlayBindings[0].descriptorCount = 1;
        overlayBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo dslCi = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        dslCi.bindingCount = 1;
        dslCi.pBindings = overlayBindings;
        VkResult res = vkCreateDescriptorSetLayout(hVideo->device, &dslCi, NULL, &hVideo->overlayDescLayout);
        if (res != VK_SUCCESS) goto cleanup_shaders;

        VkDescriptorPoolSize poolSize = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
        VkDescriptorPoolCreateInfo dpCi = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        dpCi.maxSets = 1;
        dpCi.poolSizeCount = 1;
        dpCi.pPoolSizes = &poolSize;
        res = vkCreateDescriptorPool(hVideo->device, &dpCi, NULL, &hVideo->overlayDescPool);
        if (res != VK_SUCCESS) goto cleanup_shaders;

        VkDescriptorSetAllocateInfo dsAi = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        dsAi.descriptorPool = hVideo->overlayDescPool;
        dsAi.descriptorSetCount = 1;
        dsAi.pSetLayouts = &hVideo->overlayDescLayout;
        res = vkAllocateDescriptorSets(hVideo->device, &dsAi, &hVideo->overlayDescSet);
        if (res != VK_SUCCESS) goto cleanup_shaders;

        hVideo->overlayPipelineLayout = VK_CreatePipelineLayout(hVideo, &hVideo->overlayDescLayout, 1);
        if (!hVideo->overlayPipelineLayout) goto cleanup_shaders;

        VkShaderModule overlayVert = VK_CreateShaderModule(hVideo, overlay_vert_spv, overlay_vert_spv_size, VK_SHADER_STAGE_VERTEX_BIT);
        VkShaderModule overlayFrag = VK_CreateShaderModule(hVideo, overlay_frag_spv, overlay_frag_spv_size, VK_SHADER_STAGE_FRAGMENT_BIT);
        if (!overlayVert || !overlayFrag) {
            if (overlayVert) vkDestroyShaderModule(hVideo->device, overlayVert, NULL);
            if (overlayFrag) vkDestroyShaderModule(hVideo->device, overlayFrag, NULL);
            goto cleanup_shaders;
        }

        VkVertexInputBindingDescription bindingDesc = {0};
        bindingDesc.binding = 0;
        bindingDesc.stride = 4 * sizeof(float);
        bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attrDescs[2] = {0};
        attrDescs[0].location = 0;
        attrDescs[0].binding = 0;
        attrDescs[0].format = VK_FORMAT_R32G32_SFLOAT;
        attrDescs[0].offset = 0;

        attrDescs[1].location = 1;
        attrDescs[1].binding = 0;
        attrDescs[1].format = VK_FORMAT_R32G32_SFLOAT;
        attrDescs[1].offset = 2 * sizeof(float);

        hVideo->overlayPipeline = VK_CreateGraphicsPipeline(hVideo, overlayVert, overlayFrag,
            hVideo->overlayPipelineLayout,
            &bindingDesc, attrDescs, 2,
            hVideo->swapchainExtent.width, hVideo->swapchainExtent.height, VK_TRUE, VK_FALSE, VK_FALSE);

        vkDestroyShaderModule(hVideo->device, overlayFrag, NULL);
        vkDestroyShaderModule(hVideo->device, overlayVert, NULL);

        if (!hVideo->overlayPipeline) goto cleanup_shaders;
    }

    {
        float quad[] = {
             1.0f, -1.0f,   1.0f, 1.0f,
             1.0f,  1.0f,   1.0f, 0.0f,
            -1.0f,  1.0f,   0.0f, 0.0f,
            -1.0f, -1.0f,   0.0f, 1.0f,
        };
        uint16_t quadIdx[] = {0, 1, 3, 1, 2, 3};

        VkBufferCreateInfo bi = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.size = sizeof(quad);
        bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkResult res = vkCreateBuffer(hVideo->device, &bi, NULL, &hVideo->overlayQuadVbo);
        if (res != VK_SUCCESS) goto cleanup_shaders;

        VkMemoryRequirements memReq;
        vkGetBufferMemoryRequirements(hVideo->device, hVideo->overlayQuadVbo, &memReq);

        VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = memReq.size;

        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(hVideo->physicalDevice, &memProps);
        for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
            if ((memReq.memoryTypeBits & (1 << i)) &&
                (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
                (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                ai.memoryTypeIndex = i;
                break;
            }
        }

        res = vkAllocateMemory(hVideo->device, &ai, NULL, &hVideo->overlayQuadVboMemory);
        if (res != VK_SUCCESS) goto cleanup_shaders;
        vkBindBufferMemory(hVideo->device, hVideo->overlayQuadVbo, hVideo->overlayQuadVboMemory, 0);

        void* data;
        vkMapMemory(hVideo->device, hVideo->overlayQuadVboMemory, 0, sizeof(quad), 0, &data);
        memcpy(data, quad, sizeof(quad));
        vkUnmapMemory(hVideo->device, hVideo->overlayQuadVboMemory);

        bi.size = sizeof(quadIdx);
        bi.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        res = vkCreateBuffer(hVideo->device, &bi, NULL, &hVideo->overlayQuadIbo);
        if (res != VK_SUCCESS) goto cleanup_shaders;

        vkGetBufferMemoryRequirements(hVideo->device, hVideo->overlayQuadIbo, &memReq);
        ai.allocationSize = memReq.size;

        for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
            if ((memReq.memoryTypeBits & (1 << i)) &&
                (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
                (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                ai.memoryTypeIndex = i;
                break;
            }
        }

        res = vkAllocateMemory(hVideo->device, &ai, NULL, &hVideo->overlayQuadIboMemory);
        if (res != VK_SUCCESS) goto cleanup_shaders;
        vkBindBufferMemory(hVideo->device, hVideo->overlayQuadIbo, hVideo->overlayQuadIboMemory, 0);

        vkMapMemory(hVideo->device, hVideo->overlayQuadIboMemory, 0, sizeof(quadIdx), 0, &data);
        memcpy(data, quadIdx, sizeof(quadIdx));
        vkUnmapMemory(hVideo->device, hVideo->overlayQuadIboMemory);
    }

    {
        VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sci.magFilter = VK_FILTER_NEAREST;
        sci.minFilter = VK_FILTER_NEAREST;
        sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.maxAnisotropy = 1.0f;
        if (vkCreateSampler(hVideo->device, &sci, NULL, &hVideo->overlaySampler) != VK_SUCCESS)
            goto cleanup_shaders;
    }

    {
        VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ii.imageType = VK_IMAGE_TYPE_2D;
        ii.format = VK_FORMAT_B8G8R8A8_UNORM;
        ii.extent.width = 1;
        ii.extent.height = 1;
        ii.extent.depth = 1;
        ii.mipLevels = 1;
        ii.arrayLayers = 1;
        ii.samples = VK_SAMPLE_COUNT_1_BIT;
        ii.tiling = VK_IMAGE_TILING_LINEAR;
        ii.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vkCreateImage(hVideo->device, &ii, NULL, &hVideo->defaultTextureImage) != VK_SUCCESS)
            goto cleanup_shaders;

        VkMemoryRequirements memReq;
        vkGetImageMemoryRequirements(hVideo->device, hVideo->defaultTextureImage, &memReq);

        VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = memReq.size;
        ai.memoryTypeIndex = VK_FindMemoryType(hVideo->physicalDevice, memReq.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (ai.memoryTypeIndex == UINT32_MAX || vkAllocateMemory(hVideo->device, &ai, NULL, &hVideo->defaultTextureMemory) != VK_SUCCESS) {
            vkDestroyImage(hVideo->device, hVideo->defaultTextureImage, NULL);
            hVideo->defaultTextureImage = VK_NULL_HANDLE;
            goto cleanup_shaders;
        }
        vkBindImageMemory(hVideo->device, hVideo->defaultTextureImage, hVideo->defaultTextureMemory, 0);

        void* data;
        vkMapMemory(hVideo->device, hVideo->defaultTextureMemory, 0, 4, 0, &data);
        *(uint32_t*)data = 0xFFFFFFFF;
        vkUnmapMemory(hVideo->device, hVideo->defaultTextureMemory);

        VkImageViewCreateInfo ivi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ivi.image = hVideo->defaultTextureImage;
        ivi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivi.format = VK_FORMAT_B8G8R8A8_UNORM;
        ivi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ivi.subresourceRange.baseMipLevel = 0;
        ivi.subresourceRange.levelCount = 1;
        ivi.subresourceRange.baseArrayLayer = 0;
        ivi.subresourceRange.layerCount = 1;
        if (vkCreateImageView(hVideo->device, &ivi, NULL, &hVideo->defaultTextureView) != VK_SUCCESS)
            goto cleanup_shaders;

        VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sci.magFilter = VK_FILTER_LINEAR;
        sci.minFilter = VK_FILTER_LINEAR;
        sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.maxAnisotropy = 1.0f;
        if (vkCreateSampler(hVideo->device, &sci, NULL, &hVideo->defaultSampler) != VK_SUCCESS)
            goto cleanup_shaders;
    }

    {
        VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sci.maxAnisotropy = 1.0f;
        sci.maxLod = 0.0f;

        sci.magFilter = VK_FILTER_LINEAR;
        sci.minFilter = VK_FILTER_LINEAR;
        sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        if (vkCreateSampler(hVideo->device, &sci, NULL, &hVideo->samplerLinear) != VK_SUCCESS)
            goto cleanup_shaders;

        sci.magFilter = VK_FILTER_NEAREST;
        sci.minFilter = VK_FILTER_NEAREST;
        sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        if (vkCreateSampler(hVideo->device, &sci, NULL, &hVideo->samplerNearest) != VK_SUCCESS)
            goto cleanup_shaders;
    }

    hVideo->overlayDirty = 0;

    vkDestroyShaderModule(hVideo->device, fragModule, NULL);
    vkDestroyShaderModule(hVideo->device, vertModule, NULL);

    CreateCommandBuffers(hVideo);
    CreateSyncObjects(hVideo);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        hVideo->frameStaging[i].buffer = VK_NULL_HANDLE;
        hVideo->frameStaging[i].memory = VK_NULL_HANDLE;
        hVideo->frameStaging[i].size = 0;
        hVideo->frameStaging[i].offset = 0;
        hVideo->frameStaging[i].mapped = NULL;

        VkCommandPoolCreateInfo cpCi = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        cpCi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cpCi.queueFamilyIndex = hVideo->graphicsFamilyIndex;
        if (vkCreateCommandPool(hVideo->device, &cpCi, NULL, &hVideo->uploadPool[i]) != VK_SUCCESS)
            return NULL;

        VkCommandBufferAllocateInfo cbAi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cbAi.commandPool = hVideo->uploadPool[i];
        cbAi.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbAi.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(hVideo->device, &cbAi, &hVideo->uploadBuffer[i]) != VK_SUCCESS)
            return NULL;

        VkFenceCreateInfo fCi = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fCi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        if (vkCreateFence(hVideo->device, &fCi, NULL, &hVideo->uploadFence[i]) != VK_SUCCESS)
            return NULL;
    }

    hVideo->isRecording = 0;

    VK_CHECK_RESULT(VK_SUCCESS);
    return hVideo;

cleanup_shaders:
    if (vertModule) vkDestroyShaderModule(hVideo->device, vertModule, NULL);
    if (fragModule) vkDestroyShaderModule(hVideo->device, fragModule, NULL);
cleanup_device:
    vkDestroyDevice(hVideo->device, NULL);
cleanup_instance:
    vkDestroyInstance(hVideo->instance, NULL);
    return NULL;
}

static void VK_DestroyDynamicRings(HVIDEO hVideo);

void VK_VideoClose(HVIDEO hVideo) {
    if (!hVideo)
        return;

    vkDeviceWaitIdle(hVideo->device);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VK_FrameStaging* st = &hVideo->frameStaging[i];
        if (st->mapped) vkUnmapMemory(hVideo->device, st->memory);
        if (st->buffer) vkDestroyBuffer(hVideo->device, st->buffer, NULL);
        if (st->memory) vkFreeMemory(hVideo->device, st->memory, NULL);
        st->buffer = VK_NULL_HANDLE;
        st->memory = VK_NULL_HANDLE;
        st->mapped = NULL;
        st->size = 0;
        st->offset = 0;

        if (hVideo->uploadBuffer[i]) {
            vkDestroyCommandPool(hVideo->device, hVideo->uploadPool[i], NULL);
            hVideo->uploadPool[i] = VK_NULL_HANDLE;
            hVideo->uploadBuffer[i] = VK_NULL_HANDLE;
        }
        if (hVideo->uploadFence[i]) {
            vkDestroyFence(hVideo->device, hVideo->uploadFence[i], NULL);
            hVideo->uploadFence[i] = VK_NULL_HANDLE;
        }
    }

    for (int f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {
        for (uint32_t i = 0; i < hVideo->deferredImageFreeCount[f]; i++) {
            if (hVideo->deferredImageFrees[f][i].sampler != VK_NULL_HANDLE)
                vkDestroySampler(hVideo->device, hVideo->deferredImageFrees[f][i].sampler, NULL);
            if (hVideo->deferredImageFrees[f][i].view != VK_NULL_HANDLE)
                vkDestroyImageView(hVideo->device, hVideo->deferredImageFrees[f][i].view, NULL);
            if (hVideo->deferredImageFrees[f][i].image != VK_NULL_HANDLE)
                vkDestroyImage(hVideo->device, hVideo->deferredImageFrees[f][i].image, NULL);
            if (hVideo->deferredImageFrees[f][i].memory != VK_NULL_HANDLE)
                vkFreeMemory(hVideo->device, hVideo->deferredImageFrees[f][i].memory, NULL);
        }
        hVideo->deferredImageFreeCount[f] = 0;
    }

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (hVideo->imageAvailableSemaphores[i]) vkDestroySemaphore(hVideo->device, hVideo->imageAvailableSemaphores[i], NULL);
        if (hVideo->renderFinishedSemaphores[i]) vkDestroySemaphore(hVideo->device, hVideo->renderFinishedSemaphores[i], NULL);
        if (hVideo->inFlightFences[i]) vkDestroyFence(hVideo->device, hVideo->inFlightFences[i], NULL);
    }

    if (hVideo->brenderDescriptors.layout) vkDestroyDescriptorSetLayout(hVideo->device, hVideo->brenderDescriptors.layout, NULL);
    if (hVideo->brenderDescriptors.sceneBuffer) vkDestroyBuffer(hVideo->device, hVideo->brenderDescriptors.sceneBuffer, NULL);
    if (hVideo->brenderDescriptors.sceneMapped) vkUnmapMemory(hVideo->device, hVideo->brenderDescriptors.sceneMemory);
    if (hVideo->brenderDescriptors.sceneMemory) vkFreeMemory(hVideo->device, hVideo->brenderDescriptors.sceneMemory, NULL);
    if (hVideo->brenderDescriptors.modelBuffer) vkDestroyBuffer(hVideo->device, hVideo->brenderDescriptors.modelBuffer, NULL);
    if (hVideo->brenderDescriptors.modelMapped) vkUnmapMemory(hVideo->device, hVideo->brenderDescriptors.modelMemory);
    if (hVideo->brenderDescriptors.modelMemory) vkFreeMemory(hVideo->device, hVideo->brenderDescriptors.modelMemory, NULL);

    VK_DestroyDynamicRings(hVideo);

    if (hVideo->commandPool) vkDestroyCommandPool(hVideo->device, hVideo->commandPool, NULL);

    if (hVideo->defaultPipeline && hVideo->defaultPipeline != hVideo->brenderPipeline)
        vkDestroyPipeline(hVideo->device, hVideo->defaultPipeline, NULL);
    if (hVideo->overlayPipeline) vkDestroyPipeline(hVideo->device, hVideo->overlayPipeline, NULL);
    if (hVideo->overlayPipelineLayout) vkDestroyPipelineLayout(hVideo->device, hVideo->overlayPipelineLayout, NULL);
    if (hVideo->overlaySampler) vkDestroySampler(hVideo->device, hVideo->overlaySampler, NULL);
    if (hVideo->defaultSampler) vkDestroySampler(hVideo->device, hVideo->defaultSampler, NULL);
    if (hVideo->samplerLinear) vkDestroySampler(hVideo->device, hVideo->samplerLinear, NULL);
    if (hVideo->samplerNearest) vkDestroySampler(hVideo->device, hVideo->samplerNearest, NULL);
    if (hVideo->defaultTextureView) vkDestroyImageView(hVideo->device, hVideo->defaultTextureView, NULL);
    if (hVideo->defaultTextureMemory) vkFreeMemory(hVideo->device, hVideo->defaultTextureMemory, NULL);
    if (hVideo->defaultTextureImage) vkDestroyImage(hVideo->device, hVideo->defaultTextureImage, NULL);
    if (hVideo->depthImageView) vkDestroyImageView(hVideo->device, hVideo->depthImageView, NULL);
    if (hVideo->depthMemory) vkFreeMemory(hVideo->device, hVideo->depthMemory, NULL);
    if (hVideo->depthImage) vkDestroyImage(hVideo->device, hVideo->depthImage, NULL);
    if (hVideo->overlayDescPool) vkDestroyDescriptorPool(hVideo->device, hVideo->overlayDescPool, NULL);
    if (hVideo->overlayDescLayout) vkDestroyDescriptorSetLayout(hVideo->device, hVideo->overlayDescLayout, NULL);
    if (hVideo->overlayQuadIboMemory) vkFreeMemory(hVideo->device, hVideo->overlayQuadIboMemory, NULL);
    if (hVideo->overlayQuadIbo) vkDestroyBuffer(hVideo->device, hVideo->overlayQuadIbo, NULL);
    if (hVideo->overlayQuadVboMemory) vkFreeMemory(hVideo->device, hVideo->overlayQuadVboMemory, NULL);
    if (hVideo->overlayQuadVbo) vkDestroyBuffer(hVideo->device, hVideo->overlayQuadVbo, NULL);

    if (hVideo->brenderPipeline) vkDestroyPipeline(hVideo->device, hVideo->brenderPipeline, NULL);
    if (hVideo->brenderPipelineNoWrite) vkDestroyPipeline(hVideo->device, hVideo->brenderPipelineNoWrite, NULL);
    if (hVideo->brenderBlendPipeline) vkDestroyPipeline(hVideo->device, hVideo->brenderBlendPipeline, NULL);
    if (hVideo->brenderPipelineNoDepth) vkDestroyPipeline(hVideo->device, hVideo->brenderPipelineNoDepth, NULL);
    if (hVideo->brenderBlendPipelineNoDepth) vkDestroyPipeline(hVideo->device, hVideo->brenderBlendPipelineNoDepth, NULL);

    if (hVideo->brenderPipelineLayout) vkDestroyPipelineLayout(hVideo->device, hVideo->brenderPipelineLayout, NULL);
    if (hVideo->defaultPipelineLayout && hVideo->defaultPipelineLayout != hVideo->brenderPipelineLayout)
        vkDestroyPipelineLayout(hVideo->device, hVideo->defaultPipelineLayout, NULL);

    if (hVideo->imguiCompatRenderPass) vkDestroyRenderPass(hVideo->device, hVideo->imguiCompatRenderPass, NULL);

    if (hVideo->swapchainImageViews) {
        for (uint32_t i = 0; i < hVideo->swapchainImageCount; i++)
            vkDestroyImageView(hVideo->device, hVideo->swapchainImageViews[i], NULL);
    }

    if (hVideo->swapchain) vkDestroySwapchainKHR(hVideo->device, hVideo->swapchain, NULL);
    if (hVideo->device) vkDestroyDevice(hVideo->device, NULL);
    if (hVideo->instance) vkDestroyInstance(hVideo->instance, NULL);

    memset(hVideo, 0, sizeof(VIDEO));
}

void VK_VideoRecreateSwapchain(HVIDEO hVideo) {
    vkDeviceWaitIdle(hVideo->device);

    for (uint32_t i = 0; i < hVideo->swapchainImageCount; i++) {
        vkDestroyImageView(hVideo->device, hVideo->swapchainImageViews[i], NULL);
    }

    if (hVideo->depthImageView) vkDestroyImageView(hVideo->device, hVideo->depthImageView, NULL);
    if (hVideo->depthMemory) vkFreeMemory(hVideo->device, hVideo->depthMemory, NULL);
    if (hVideo->depthImage) vkDestroyImage(hVideo->device, hVideo->depthImage, NULL);
    hVideo->depthImageView = VK_NULL_HANDLE;
    hVideo->depthMemory = VK_NULL_HANDLE;
    hVideo->depthImage = VK_NULL_HANDLE;

    VkSwapchainKHR oldSwapchain = hVideo->swapchain;

    // Compute actual window size from the viewport callback
    br_device_pixelmap* screen = (br_device_pixelmap*)hVideo->res;
    int vp_x, vp_y;
    float scale_x, scale_y;
    DevicePixelmapVKGetViewport(screen, &vp_x, &vp_y, &scale_x, &scale_y);
    int width = (int)(2 * vp_x + scale_x * screen->pm_width + 0.5f);
    int height = (int)(2 * vp_y + scale_y * screen->pm_height + 0.5f);

    CreateSwapchain(hVideo, width, height);

    if (oldSwapchain) {
        vkDestroySwapchainKHR(hVideo->device, oldSwapchain, NULL);
    }

    CreateImageViews(hVideo);
    CreateDepthResources(hVideo);
}

static VkResult createBuffer(HVIDEO hVideo, VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags props, VkBuffer* buffer, VkDeviceMemory* memory) {
    VkBufferCreateInfo bi = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult res = vkCreateBuffer(hVideo->device, &bi, NULL, buffer);
    if (res != VK_SUCCESS) return res;

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(hVideo->device, *buffer, &memReq);

    VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = memReq.size;
    ai.memoryTypeIndex = VK_FindMemoryType(hVideo->physicalDevice, memReq.memoryTypeBits, props);
    if (ai.memoryTypeIndex == UINT32_MAX) {
        vkDestroyBuffer(hVideo->device, *buffer, NULL);
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    res = vkAllocateMemory(hVideo->device, &ai, NULL, memory);
    if (res != VK_SUCCESS) {
        vkDestroyBuffer(hVideo->device, *buffer, NULL);
        *buffer = VK_NULL_HANDLE;
        return res;
    }

    vkBindBufferMemory(hVideo->device, *buffer, *memory, 0);
    return VK_SUCCESS;
}

#define VK_DYN_RING_CAPACITY (4u * 1024u * 1024u)

VkResult VK_CreateDynamicRings(HVIDEO hVideo) {
    for (int f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {
        VkResult res = createBuffer(hVideo, VK_DYN_RING_CAPACITY, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &hVideo->dynVbo[f], &hVideo->dynVboMemory[f]);
        if (res != VK_SUCCESS) return res;
        res = vkMapMemory(hVideo->device, hVideo->dynVboMemory[f], 0, VK_DYN_RING_CAPACITY, 0,
            &hVideo->dynVboMapped[f]);
        if (res != VK_SUCCESS) return res;

        res = createBuffer(hVideo, VK_DYN_RING_CAPACITY, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &hVideo->dynIbo[f], &hVideo->dynIboMemory[f]);
        if (res != VK_SUCCESS) return res;
        res = vkMapMemory(hVideo->device, hVideo->dynIboMemory[f], 0, VK_DYN_RING_CAPACITY, 0,
            &hVideo->dynIboMapped[f]);
        if (res != VK_SUCCESS) return res;

        hVideo->dynVboOffset[f] = 0;
        hVideo->dynIboOffset[f] = 0;
    }
    hVideo->dynVboCapacity = VK_DYN_RING_CAPACITY;
    hVideo->dynIboCapacity = VK_DYN_RING_CAPACITY;
    return VK_SUCCESS;
}

static void VK_DestroyDynamicRings(HVIDEO hVideo) {
    for (int f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {
        if (hVideo->dynVboMapped[f]) vkUnmapMemory(hVideo->device, hVideo->dynVboMemory[f]);
        if (hVideo->dynVbo[f]) vkDestroyBuffer(hVideo->device, hVideo->dynVbo[f], NULL);
        if (hVideo->dynVboMemory[f]) vkFreeMemory(hVideo->device, hVideo->dynVboMemory[f], NULL);
        if (hVideo->dynIboMapped[f]) vkUnmapMemory(hVideo->device, hVideo->dynIboMemory[f]);
        if (hVideo->dynIbo[f]) vkDestroyBuffer(hVideo->device, hVideo->dynIbo[f], NULL);
        if (hVideo->dynIboMemory[f]) vkFreeMemory(hVideo->device, hVideo->dynIboMemory[f], NULL);
    }
}

VkResult VK_CreateBrenderDescriptors(HVIDEO hVideo, uint32_t width, uint32_t height) {
    (void)width;
    (void)height;

    VkDescriptorSetLayoutBinding bindings[3] = {0};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo dslCi = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslCi.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    dslCi.bindingCount = 3;
    dslCi.pBindings = bindings;
    VkResult res = vkCreateDescriptorSetLayout(hVideo->device, &dslCi, NULL, &hVideo->brenderDescriptors.layout);
    if (res != VK_SUCCESS) return res;

    VkDeviceSize sceneSlotSize = sizeof(shader_data_scene);
    VkDeviceSize align = hVideo->minUniformBufferOffsetAlignment;
    if (align > 0)
        sceneSlotSize = (sceneSlotSize + align - 1) & ~(align - 1);
    VkDeviceSize sceneSize = sceneSlotSize * 64;
    VkDeviceSize modelSlotSize = sceneSlotSize + sizeof(shader_data_model);
    VkDeviceSize modelDrawCapacity = 1024;
    VkDeviceSize modelSize = modelSlotSize * modelDrawCapacity;

    hVideo->sceneSlotSize = sceneSlotSize;
    hVideo->modelSlotSize = modelSlotSize;
    hVideo->modelBufferCapacity = modelSize;
    hVideo->sceneBufferCapacity = sceneSize;

    res = createBuffer(hVideo, sceneSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &hVideo->brenderDescriptors.sceneBuffer, &hVideo->brenderDescriptors.sceneMemory);
    if (res != VK_SUCCESS) return res;

    res = createBuffer(hVideo, modelSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &hVideo->brenderDescriptors.modelBuffer, &hVideo->brenderDescriptors.modelMemory);
    if (res != VK_SUCCESS) return res;

    // Persistently map both UBO buffers: per-group uploads become plain memcpys.
    // vkMapMemory/vkUnmapMemory per group was a major CPU cost that grew linearly
    // with scene group count (crash debris, ped gibs, many actors).
    vkMapMemory(hVideo->device, hVideo->brenderDescriptors.sceneMemory, 0, sceneSize, 0,
        &hVideo->brenderDescriptors.sceneMapped);
    vkMapMemory(hVideo->device, hVideo->brenderDescriptors.modelMemory, 0, modelSize, 0,
        &hVideo->brenderDescriptors.modelMapped);

    return VK_SUCCESS;
}

void VK_UpdateSceneUBO(HVIDEO hVideo, void* data, size_t size, VkDeviceSize offset) {
    memcpy((char*)hVideo->brenderDescriptors.sceneMapped + offset, data, size);
}

void VK_UpdateModelUBOAtOffset(HVIDEO hVideo, void* data, size_t size, VkDeviceSize offset) {
    memcpy((char*)hVideo->brenderDescriptors.modelMapped + offset, data, size);
}

static void VK_BeginDynamicRenderingWithOps(HVIDEO hVideo, VkCommandBuffer cmd,
    VkAttachmentLoadOp colorLoadOp, VkAttachmentLoadOp depthLoadOp) {

    VkRenderingAttachmentInfo colorAttachment = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttachment.imageView = hVideo->swapchainImageViews[hVideo->currentImageIndex];
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = colorLoadOp;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color.float32[0] = 0.0f;
    colorAttachment.clearValue.color.float32[1] = 0.0f;
    colorAttachment.clearValue.color.float32[2] = 0.0f;
    colorAttachment.clearValue.color.float32[3] = 1.0f;

    VkRenderingAttachmentInfo depthAttachment = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    depthAttachment.imageView = hVideo->depthImageView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = depthLoadOp;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.clearValue.depthStencil.depth = 1.0f;
    depthAttachment.clearValue.depthStencil.stencil = 0;

    VkRenderingInfo renderInfo = {VK_STRUCTURE_TYPE_RENDERING_INFO};
    renderInfo.renderArea.offset.x = 0;
    renderInfo.renderArea.offset.y = 0;
    renderInfo.renderArea.extent = hVideo->swapchainExtent;
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttachment;
    renderInfo.pDepthAttachment = &depthAttachment;

    vkCmdBeginRendering(cmd, &renderInfo);
}

/* Starts recording the current frame's command buffer exactly once. Called by
 * the first sceneBegin AND the first flush of a frame; idempotent via
 * hVideo->isRecording. renderingStarted is NOT set here — it is scene-scoped
 * (set in sceneBegin) so the pre-scene flush can still run the mainViewport
 * purge. */
void VK_EnsureRecording(HVIDEO hVideo) {
    hVideo->currentModelOffset = 0;
    hVideo->sceneSlotIndex = 0;
    hVideo->dimAreaCount = 0;
    hVideo->clearAreaCount = 0;
    hVideo->pratcamAreaCount = 0;
    hVideo->lastVbo = VK_NULL_HANDLE;
    hVideo->lastIbo = VK_NULL_HANDLE;
    hVideo->lastVboOffset = 0;
    hVideo->lastIboOffset = 0;
    hVideo->lastTextureView = VK_NULL_HANDLE;
    hVideo->lastTextureSampler = VK_NULL_HANDLE;
    hVideo->lastPipeline = VK_NULL_HANDLE;

    uint32_t f = hVideo->currentFrame;
    vkWaitForFences(hVideo->device, 1, &hVideo->inFlightFences[f], VK_TRUE, UINT64_MAX);
    vkResetFences(hVideo->device, 1, &hVideo->inFlightFences[f]);

    /* Reset the dynamic-ring cursors. The fence wait above guarantees the GPU is
     * done with the previous command buffer submitted for this frame slot, so its
     * ring data is no longer referenced and may be overwritten. Within a frame the
     * cursor only advances (no wrap), so every small-model build gets its own slot
     * even when the same model is rebuilt hundreds of times (electro ray). */
    hVideo->dynVboOffset[f] = 0;
    hVideo->dynIboOffset[f] = 0;
    hVideo->frameEpoch++;

    if (hVideo->deferredBufferFreeCount[f] > 0) {
        for (uint32_t i = 0; i < hVideo->deferredBufferFreeCount[f]; i++) {
            if (hVideo->deferredBufferFrees[f][i].buffer != VK_NULL_HANDLE)
                vkDestroyBuffer(hVideo->device, hVideo->deferredBufferFrees[f][i].buffer, NULL);
            if (hVideo->deferredBufferFrees[f][i].memory != VK_NULL_HANDLE)
                vkFreeMemory(hVideo->device, hVideo->deferredBufferFrees[f][i].memory, NULL);
        }
        hVideo->deferredBufferFreeCount[f] = 0;
    }

    if (hVideo->deferredImageFreeCount[f] > 0) {
        for (uint32_t i = 0; i < hVideo->deferredImageFreeCount[f]; i++) {
            if (hVideo->deferredImageFrees[f][i].sampler != VK_NULL_HANDLE)
                vkDestroySampler(hVideo->device, hVideo->deferredImageFrees[f][i].sampler, NULL);
            if (hVideo->deferredImageFrees[f][i].view != VK_NULL_HANDLE)
                vkDestroyImageView(hVideo->device, hVideo->deferredImageFrees[f][i].view, NULL);
            if (hVideo->deferredImageFrees[f][i].image != VK_NULL_HANDLE)
                vkDestroyImage(hVideo->device, hVideo->deferredImageFrees[f][i].image, NULL);
            if (hVideo->deferredImageFrees[f][i].memory != VK_NULL_HANDLE)
                vkFreeMemory(hVideo->device, hVideo->deferredImageFrees[f][i].memory, NULL);
        }
        hVideo->deferredImageFreeCount[f] = 0;
    }

    {
        extern int gHarness_window_width, gHarness_window_height;
        if (gHarness_window_width > 0 && gHarness_window_height > 0 &&
            ((uint32_t)gHarness_window_width != hVideo->swapchainExtent.width ||
             (uint32_t)gHarness_window_height != hVideo->swapchainExtent.height)) {
            VK_VideoRecreateSwapchain(hVideo);
            hVideo->mainViewportW = 0;
        }
    }

    VkResult res = vkAcquireNextImageKHR(hVideo->device, hVideo->swapchain, UINT64_MAX,
        hVideo->imageAvailableSemaphores[hVideo->currentFrame], VK_NULL_HANDLE, &hVideo->currentImageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        VK_VideoRecreateSwapchain(hVideo);
        hVideo->mainViewportW = 0;
        res = vkAcquireNextImageKHR(hVideo->device, hVideo->swapchain, UINT64_MAX,
            hVideo->imageAvailableSemaphores[hVideo->currentFrame], VK_NULL_HANDLE, &hVideo->currentImageIndex);
    }

    VkCommandBuffer cmd = hVideo->drawCommandBuffers[hVideo->currentImageIndex];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = 0;
    vkBeginCommandBuffer(cmd, &begin);

    hVideo->isRecording = 1;
}

void VK_BeginRenderPass(HVIDEO hVideo, VkCommandBuffer cmd) {
    VkImageMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    barrier.image = hVideo->swapchainImages[hVideo->currentImageIndex];
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_NONE;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

    VkDependencyInfo depInfo = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(cmd, &depInfo);

    barrier.image = hVideo->depthImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    vkCmdPipelineBarrier2(cmd, &depInfo);

    VK_BeginDynamicRenderingWithOps(hVideo, cmd, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_LOAD_OP_CLEAR);
}

void VK_BeginOverlayRenderPass(HVIDEO hVideo, VkCommandBuffer cmd) {
    VK_BeginDynamicRenderingWithOps(hVideo, cmd, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_LOAD_OP_LOAD);
}

void VK_EndRenderPass(HVIDEO hVideo, VkCommandBuffer cmd) {
    (void)hVideo;
    vkCmdEndRendering(cmd);
}

VkResult VK_Present(HVIDEO hVideo) {
    uint32_t f = (hVideo->currentFrame + MAX_FRAMES_IN_FLIGHT - 1) % MAX_FRAMES_IN_FLIGHT;
    VkPresentInfoKHR pi = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &hVideo->renderFinishedSemaphores[f];
    pi.swapchainCount = 1;
    pi.pSwapchains = &hVideo->swapchain;
    pi.pImageIndices = &hVideo->currentImageIndex;

    return vkQueuePresentKHR(hVideo->presentQueue, &pi);
}

br_error VK_BrPixelmapGetTypeDetails(br_uint_8 pmType, VkFormat* format, VkImageTiling* tiling,
    VkImageUsageFlags* usage, VkMemoryPropertyFlags* memProps) {
    VkFormat fmt = VK_FORMAT_UNDEFINED;
    VkImageTiling til = VK_IMAGE_TILING_OPTIMAL;
    VkImageUsageFlags usg = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VkMemoryPropertyFlags mem = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    switch (pmType) {
    case BR_PMT_RGB_555:
        fmt = VK_FORMAT_B5G5R5A1_UNORM_PACK16;
        break;
    case BR_PMT_RGB_565:
        fmt = VK_FORMAT_R5G6B5_UNORM_PACK16;
        break;
    case BR_PMT_INDEX_8:
        fmt = VK_FORMAT_B8G8R8A8_UNORM;
        break;
    case BR_PMT_RGB_888:
        fmt = VK_FORMAT_R8G8B8_UNORM;
        break;
    case BR_PMT_RGBX_888:
    case BR_PMT_RGBA_8888:
        fmt = VK_FORMAT_B8G8R8A8_UNORM;
        break;
    case BR_PMT_DEPTH_16:
        fmt = VK_FORMAT_D16_UNORM;
        usg = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        break;
    default:
        return BRE_FAIL;
    }

    if (format) *format = fmt;
    if (tiling) *tiling = til;
    if (usage) *usage = usg;
    if (memProps) *memProps = mem;
    return BRE_OK;
}

void VK_DeferFreeImage(HVIDEO hVideo, VkImage image, VkImageView view, VkSampler sampler, VkDeviceMemory memory) {
    if (image == VK_NULL_HANDLE && view == VK_NULL_HANDLE && sampler == VK_NULL_HANDLE && memory == VK_NULL_HANDLE)
        return;

    uint32_t f = hVideo->currentFrame;
    if (hVideo->deferredImageFreeCount[f] + 1 > hVideo->deferredImageFreeCapacity[f]) {
        uint32_t newCap = hVideo->deferredImageFreeCapacity[f] ? hVideo->deferredImageFreeCapacity[f] * 2 : 64;
        size_t elemSize = sizeof(VK_DeferredImageFree);
        VK_DeferredImageFree *newArray = BrResAllocate(hVideo->res, newCap * elemSize, BR_MEMORY_OBJECT_DATA);
        if (hVideo->deferredImageFrees[f])
            BrMemCpy(newArray, hVideo->deferredImageFrees[f], hVideo->deferredImageFreeCount[f] * elemSize);
        hVideo->deferredImageFrees[f] = newArray;
        hVideo->deferredImageFreeCapacity[f] = newCap;
    }

    hVideo->deferredImageFrees[f][hVideo->deferredImageFreeCount[f]].image = image;
    hVideo->deferredImageFrees[f][hVideo->deferredImageFreeCount[f]].view = view;
    hVideo->deferredImageFrees[f][hVideo->deferredImageFreeCount[f]].sampler = sampler;
    hVideo->deferredImageFrees[f][hVideo->deferredImageFreeCount[f]].memory = memory;
    hVideo->deferredImageFreeCount[f]++;
}

/* Grows (or lazily creates) the current frame slot's staging buffer to at least
 * `aligned` bytes. The caller has already waited the slot's uploadFence, so no
 * uploads reference the old buffer and it may be destroyed immediately. Returns 1
 * on success, 0 on allocation failure. */
static int VK_GrowFrameStaging(HVIDEO hVideo, VK_FrameStaging* st, VkDeviceSize aligned) {
    VkDeviceSize newSize = st->size > 0 ? st->size : 16u * 1024u * 1024u;
    while (newSize < aligned)
        newSize *= 2;

    VkBufferCreateInfo bi = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = newSize;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer newBuffer;
    if (vkCreateBuffer(hVideo->device, &bi, NULL, &newBuffer) != VK_SUCCESS)
        return 0;

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(hVideo->device, newBuffer, &memReq);
    VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = memReq.size;
    ai.memoryTypeIndex = VK_FindMemoryType(hVideo->physicalDevice, memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkDeviceMemory newMemory;
    if (ai.memoryTypeIndex == UINT32_MAX || vkAllocateMemory(hVideo->device, &ai, NULL, &newMemory) != VK_SUCCESS) {
        vkDestroyBuffer(hVideo->device, newBuffer, NULL);
        return 0;
    }
    vkBindBufferMemory(hVideo->device, newBuffer, newMemory, 0);

    void* mapped;
    if (vkMapMemory(hVideo->device, newMemory, 0, VK_WHOLE_SIZE, 0, &mapped) != VK_SUCCESS) {
        vkDestroyBuffer(hVideo->device, newBuffer, NULL);
        vkFreeMemory(hVideo->device, newMemory, NULL);
        return 0;
    }

    if (st->buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(hVideo->device, st->buffer, NULL);
        vkFreeMemory(hVideo->device, st->memory, NULL);
    }

    st->buffer = newBuffer;
    st->memory = newMemory;
    st->mapped = mapped;
    st->size = newSize;
    st->offset = 0;
    return 1;
}

VkResult VK_UploadBufferToImage(HVIDEO hVideo, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
    uint32_t width, uint32_t height, uint32_t dstX, uint32_t dstY, VkImageAspectFlags aspectMask,
    const void* hostData, size_t hostDataSize) {

    uint32_t f = hVideo->currentFrame;
    VK_FrameStaging* st = &hVideo->frameStaging[f];

    /* Wait for this slot's previous upload batch before reusing its staging buffer
     * and command buffer. In the steady frame loop the slot is reused two frames
     * later and the upload submit (a tiny copy, submitted before that frame's main
     * command buffer) has long finished, so this is a cheap already-signaled check
     * — never a full GPU drain. Because the wait precedes every reuse, uploads may
     * be recorded from anywhere: the frame loop OR out-of-frame track/asset loading
     * (BufferStoredVKUpdate during LoadTrack). */
    vkWaitForFences(hVideo->device, 1, &hVideo->uploadFence[f], VK_TRUE, UINT64_MAX);
    vkResetFences(hVideo->device, 1, &hVideo->uploadFence[f]);

    st->offset = 0;
    vkResetCommandBuffer(hVideo->uploadBuffer[f], 0);

    VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(hVideo->uploadBuffer[f], &begin) != VK_SUCCESS)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    VkDeviceSize aligned = (hostDataSize + 255u) & ~(VkDeviceSize)255u;
    if (st->buffer == VK_NULL_HANDLE || st->offset + aligned > st->size) {
        if (!VK_GrowFrameStaging(hVideo, st, aligned))
            return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    memcpy((char*)st->mapped + st->offset, hostData, hostDataSize);
    VkDeviceSize srcOffset = st->offset;
    st->offset += aligned;

    VkCommandBuffer cmd = hVideo->uploadBuffer[f];

    VkImageMemoryBarrier2 b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    b.oldLayout = oldLayout;
    b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = image;
    b.subresourceRange.aspectMask = aspectMask;
    b.subresourceRange.baseMipLevel = 0;
    b.subresourceRange.levelCount = 1;
    b.subresourceRange.baseArrayLayer = 0;
    b.subresourceRange.layerCount = 1;
    b.srcStageMask = (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        ? VK_PIPELINE_STAGE_2_NONE : VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    b.srcAccessMask = (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        ? VK_ACCESS_2_NONE : VK_ACCESS_2_SHADER_READ_BIT;
    b.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
    b.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;

    VkDependencyInfo depInfo = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &b;
    vkCmdPipelineBarrier2(cmd, &depInfo);

    VkBufferImageCopy region = {0};
    region.bufferOffset = srcOffset;
    region.imageSubresource.aspectMask = aspectMask;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset.x = dstX;
    region.imageOffset.y = dstY;
    region.imageExtent.width = width;
    region.imageExtent.height = height;
    region.imageExtent.depth = 1;
    vkCmdCopyBufferToImage(cmd, st->buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b.newLayout = newLayout;
    b.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
    b.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    b.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    b.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    vkCmdPipelineBarrier2(cmd, &depInfo);

    vkEndCommandBuffer(cmd);

    /* Submit immediately with the slot's own fence: the next call on this slot waits
     * it before reusing the staging/command buffer, and the frame's main command
     * buffer (submitted later, same queue) executes in-order after this copy. */
    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    vkQueueSubmit(hVideo->graphicsQueue, 1, &si, hVideo->uploadFence[f]);

    return VK_SUCCESS;
}