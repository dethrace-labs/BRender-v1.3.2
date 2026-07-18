#include "drv.h"
#include <brassert.h>
#include <string.h>
#include "vk_shaders.h"

/* ImGui / external render callback */
static void (*g_external_render_cb)(void* cmd, void* ud) = NULL;
static void* g_external_render_ud = NULL;
static VKREND_DeviceInfo g_vkrend_info = {NULL};

void VKREND_SetExternalRenderCallback(void (*cb)(void* cmd, void* ud), void* ud) {
    g_external_render_cb = cb;
    g_external_render_ud = ud;
}

void VKREND_GetDeviceInfo(VKREND_DeviceInfo* info) {
    if (info) *info = g_vkrend_info;
}

static const struct br_device_pixelmap_dispatch devicePixelmapFrontDispatch;

static br_uint_8 DeviceVKTypeOrBits(br_uint_8 pixel_type, br_int_32 pixel_bits)
{
    if(pixel_type != BR_PMT_MAX)
        return pixel_type;

    switch(pixel_bits) {
        case 16:
            return BR_PMT_RGB_565;
        case 24:
            return BR_PMT_RGB_888;
        case 32:
            return BR_PMT_RGBX_888;
        default:
            break;
    }

    return BR_PMT_MAX;
}

#define F(f) offsetof(br_device_pixelmap, f)
#define FF(f) offsetof(br_device_pixelmap, asFront.f)
static struct br_tv_template_entry devicePixelmapFrontTemplateEntries[] = {
    { BRT(WIDTH_I32), F(pm_width), BRTV_QUERY | BRTV_ALL, BRTV_CONV_I32_U16, 0 },
    { BRT(HEIGHT_I32), F(pm_height), BRTV_QUERY | BRTV_ALL, BRTV_CONV_I32_U16, 0 },
    { BRT(PIXEL_TYPE_U8), F(pm_type), BRTV_QUERY | BRTV_ALL, BRTV_CONV_I32_U8, 0 },
    { BRT(OUTPUT_FACILITY_O), F(output_facility), BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY, 0 },
    { BRT(FACILITY_O), F(output_facility), BRTV_QUERY, BRTV_CONV_COPY, 0 },
    { BRT(IDENTIFIER_CSTR), F(pm_identifier), BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY, 0 },
    { BRT(MSAA_SAMPLES_I32), F(msaa_samples), BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY, 0 },
    { BRT(VULKAN_CALLBACKS_P), 0, BRTV_QUERY | BRTV_ALL, BRTV_CONV_DIRECT },
    { DEV(VULKAN_VERSION_CSTR), FF(vk_version), BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY, 0 },
    { DEV(VULKAN_VENDOR_CSTR), FF(vk_vendor), BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY, 0 },
    { DEV(VULKAN_RENDERER_CSTR), FF(vk_renderer), BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY, 0 },
    { BRT_CLUT_O, 0, F(clut), BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY, 0 }
};
#undef FF
#undef F

struct pixelmapNewTokens {
    br_int_32 width;
    br_int_32 height;
    br_int_32 pixel_bits;
    br_uint_8 pixel_type;
    int msaa_samples;
    br_device_vk_callback_procs* callbacks;
    const char* vertex_spv_data;
    size_t vertex_spv_size;
    const char* fragment_spv_data;
    size_t fragment_spv_size;
};

#define F(f) offsetof(struct pixelmapNewTokens, f)
static struct br_tv_template_entry pixelmapNewTemplateEntries[] = {
    { BRT(WIDTH_I32), F(width), BRTV_SET, BRTV_CONV_COPY },
    { BRT(HEIGHT_I32), F(height), BRTV_SET, BRTV_CONV_COPY },
    { BRT(PIXEL_BITS_I32), F(pixel_bits), BRTV_SET, BRTV_CONV_COPY },
    { BRT(PIXEL_TYPE_U8), F(pixel_type), BRTV_SET, BRTV_CONV_COPY },
    { BRT(MSAA_SAMPLES_I32), F(msaa_samples), BRTV_SET, BRTV_CONV_COPY },
    { BRT(VULKAN_CALLBACKS_P), F(callbacks), BRTV_SET, BRTV_CONV_COPY },
    { DEV(VERTEX_SHADER_P), F(vertex_spv_data), BRTV_SET, BRTV_CONV_COPY },
    { DEV(FRAGMENT_SHADER_P), F(fragment_spv_data), BRTV_SET, BRTV_CONV_COPY },
};
#undef F

void DevicePixelmapVKGetViewport(br_device_pixelmap* self, int *x, int *y, float *wm, float *hm) {
    self->asFront.callbacks.get_viewport(x, y, wm, hm);
}

void VK_OverlayDraw(HVIDEO hVideo, VkCommandBuffer cmd) {
    if (!hVideo->overlayDirty || hVideo->overlayImageView == VK_NULL_HANDLE)
        return;

    VkDescriptorImageInfo imgInfo = {0};
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imgInfo.imageView = hVideo->overlayImageView;
    imgInfo.sampler = hVideo->overlaySampler;

    VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = hVideo->overlayDescSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imgInfo;
    vkUpdateDescriptorSets(hVideo->device, 1, &write, 0, NULL);

    VkDeviceSize offset = 0;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, hVideo->overlayPipeline);
    vkCmdBindVertexBuffers(cmd, 0, 1, &hVideo->overlayQuadVbo, &offset);
    vkCmdBindIndexBuffer(cmd, hVideo->overlayQuadIbo, 0, VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        hVideo->overlayPipelineLayout, 0, 1, &hVideo->overlayDescSet, 0, NULL);
    vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);

    hVideo->overlayDirty = 0;
}

void VK_SetPratcamArea(HVIDEO hVideo, int x, int y, int w, int h) {
    hVideo->pratcamAreaCount = 1;
    hVideo->pratcamArea.x = x;
    hVideo->pratcamArea.y = y;
    hVideo->pratcamArea.w = w;
    hVideo->pratcamArea.h = h;
}

static void submitAndPresent(br_device_pixelmap* self) {
    HVIDEO hVideo = &self->asFront.video;
    VkCommandBuffer cmd = hVideo->drawCommandBuffers[hVideo->currentImageIndex];
    uint32_t f = hVideo->currentFrame;

    VkImageMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = hVideo->swapchainImages[hVideo->currentImageIndex];
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
    barrier.dstAccessMask = VK_ACCESS_2_NONE;

    VkDependencyInfo depInfo = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(cmd, &depInfo);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &hVideo->imageAvailableSemaphores[f];
    si.pWaitDstStageMask = &waitStage;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &hVideo->renderFinishedSemaphores[f];
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;

    vkQueueSubmit(hVideo->graphicsQueue, 1, &si, hVideo->inFlightFences[f]);

    hVideo->currentFrame = (f + 1) % MAX_FRAMES_IN_FLIGHT;

    VkResult res = VK_Present(hVideo);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        VK_VideoRecreateSwapchain(hVideo);
    }

    hVideo->isRecording = 0;
}



static void drawExternalCallbacks(HVIDEO hVideo, VkCommandBuffer cmd) {
    if (g_external_render_cb) {
        VK_BeginOverlayRenderPass(hVideo, cmd);
        g_external_render_cb(cmd, g_external_render_ud);
        VK_EndRenderPass(hVideo, cmd);
    }
}

void DevicePixelmapVKSwapBuffers(br_device_pixelmap* self) {
    HVIDEO hVideo = &self->asFront.video;

    if (hVideo->isRecording) {
        VkCommandBuffer cmd = hVideo->drawCommandBuffers[hVideo->currentImageIndex];

        if (hVideo->renderPassActive) {
            if (hVideo->overlayDirty) {
                int vp_x, vp_y;
                float rx, ry;
                DevicePixelmapVKGetViewport(self, &vp_x, &vp_y, &rx, &ry);
                float ov_vp_x = (float)self->pm_base_x * rx + (float)vp_x;
                float ov_vp_y = (float)self->pm_base_y * ry + (float)vp_y;
                float ov_vp_w = (float)self->pm_width * rx;
                float ov_vp_h = (float)self->pm_height * ry;
                VkViewport vp = {ov_vp_x, ov_vp_y, ov_vp_w, ov_vp_h, 0.0f, 1.0f};
                vkCmdSetViewport(cmd, 0, 1, &vp);
                VkRect2D sc = {{(int32_t)ov_vp_x, (int32_t)ov_vp_y}, {(uint32_t)ov_vp_w, (uint32_t)ov_vp_h}};
                vkCmdSetScissor(cmd, 0, 1, &sc);
                VK_OverlayDraw(hVideo, cmd);
                hVideo->overlayDirty = 0;
            }
            VK_EndRenderPass(hVideo, cmd);

            hVideo->renderPassActive = 0;
        }

        drawExternalCallbacks(hVideo, cmd);

        hVideo->sceneCount = 0;

        submitAndPresent(self);
    } else {
        uint32_t f = hVideo->currentFrame;
        vkWaitForFences(hVideo->device, 1, &hVideo->inFlightFences[f], VK_TRUE, UINT64_MAX);
        vkResetFences(hVideo->device, 1, &hVideo->inFlightFences[f]);

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
            hVideo->imageAvailableSemaphores[f], VK_NULL_HANDLE, &hVideo->currentImageIndex);
        if (res == VK_ERROR_OUT_OF_DATE_KHR) {
            VK_VideoRecreateSwapchain(hVideo);
            hVideo->mainViewportW = 0;
            res = vkAcquireNextImageKHR(hVideo->device, hVideo->swapchain, UINT64_MAX,
                hVideo->imageAvailableSemaphores[f], VK_NULL_HANDLE, &hVideo->currentImageIndex);
        }

        VkCommandBuffer cmd = hVideo->drawCommandBuffers[hVideo->currentImageIndex];
        vkResetCommandBuffer(cmd, 0);

        VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = 0;
        vkBeginCommandBuffer(cmd, &begin);

        hVideo->isRecording = 1;

        VK_BeginRenderPass(hVideo, cmd);
        hVideo->renderPassActive = 1;

        if (hVideo->overlayDirty) {
            int vp_x, vp_y;
            float rx, ry;
            DevicePixelmapVKGetViewport(self, &vp_x, &vp_y, &rx, &ry);
            float ov_vp_x = (float)self->pm_base_x * rx + (float)vp_x;
            float ov_vp_y = (float)self->pm_base_y * ry + (float)vp_y;
            float ov_vp_w = (float)self->pm_width * rx;
            float ov_vp_h = (float)self->pm_height * ry;
            VkViewport vp = {ov_vp_x, ov_vp_y, ov_vp_w, ov_vp_h, 0.0f, 1.0f};
            vkCmdSetViewport(cmd, 0, 1, &vp);
            VkRect2D sc = {{(int32_t)ov_vp_x, (int32_t)ov_vp_y}, {(uint32_t)ov_vp_w, (uint32_t)ov_vp_h}};
            vkCmdSetScissor(cmd, 0, 1, &sc);
            VK_OverlayDraw(hVideo, cmd);
            hVideo->overlayDirty = 0;
        }

        VK_EndRenderPass(hVideo, cmd);
        hVideo->renderPassActive = 0;

        drawExternalCallbacks(hVideo, cmd);

        hVideo->sceneCount = 0;

        submitAndPresent(self);
    }
}

void DevicePixelmapVKFree(br_device_pixelmap* self) {
    self->asFront.callbacks.free((br_pixelmap*)self, NULL);
}

br_device_pixelmap* DevicePixelmapVKAllocateFront(br_device* dev, br_output_facility* outfcty, br_token_value* tv) {
    br_device_pixelmap* self;
    br_int_32 count;
    struct pixelmapNewTokens pt = {
        .width = -1,
        .height = -1,
        .pixel_bits = -1,
        .pixel_type = BR_PMT_MAX,
        .msaa_samples = 0,
        .callbacks = NULL,
        .vertex_spv_data = NULL,
        .vertex_spv_size = 0,
        .fragment_spv_data = NULL,
        .fragment_spv_size = 0,
    };
    char tmp[80];

    if (dev->templates.pixelmapNewTemplate == NULL) {
        dev->templates.pixelmapNewTemplate = BrTVTemplateAllocate(dev, pixelmapNewTemplateEntries,
            BR_ASIZE(pixelmapNewTemplateEntries));
    }

    BrTokenValueSetMany(&pt, &count, NULL, tv, dev->templates.pixelmapNewTemplate);

    if (pt.callbacks == NULL || pt.width <= 0 || pt.height <= 0)
        return NULL;

    if ((pt.pixel_type = DeviceVKTypeOrBits(pt.pixel_type, pt.pixel_bits)) == BR_PMT_MAX)
        return NULL;

    self = BrResAllocate(dev->res, sizeof(br_device_pixelmap), BR_MEMORY_OBJECT);

    BrSprintfN(tmp, sizeof(tmp) - 1, "Vulkan:Screen:%dx%d", pt.width, pt.height);
    self->pm_identifier = BrResStrDup(self, tmp);
    self->dispatch = &devicePixelmapFrontDispatch;
    self->device = dev;
    self->output_facility = outfcty;
    self->use_type = BRT_NONE;
    self->msaa_samples = pt.msaa_samples;
    self->screen = self;
    self->clut = dev->clut;

    self->pm_type = pt.pixel_type;
    self->pm_width = pt.width;
    self->pm_height = pt.height;
    self->pm_flags |= BR_PMF_NO_ACCESS;

    self->asFront.callbacks = *pt.callbacks;

    if (VK_VideoOpen(&self->asFront.video, self,
            pt.vertex_spv_data ? pt.vertex_spv_data : brender_vert_spv,
            pt.vertex_spv_size ? pt.vertex_spv_size : brender_vert_spv_size,
            pt.fragment_spv_data ? pt.fragment_spv_data : brender_frag_spv,
            pt.fragment_spv_size ? pt.fragment_spv_size : brender_frag_spv_size,
            &self->asFront.callbacks, pt.width, pt.height) == NULL) {
        BrResFree(self);
        return NULL;
    }

    g_vkrend_info.instance = (void*)self->asFront.video.instance;
    g_vkrend_info.physical_device = (void*)self->asFront.video.physicalDevice;
    g_vkrend_info.device = (void*)self->asFront.video.device;
    g_vkrend_info.graphics_queue = (void*)self->asFront.video.graphicsQueue;
    g_vkrend_info.graphics_queue_family = self->asFront.video.graphicsFamilyIndex;
    g_vkrend_info.render_pass = (void*)self->asFront.video.imguiCompatRenderPass;
    g_vkrend_info.min_image_count = self->asFront.video.swapchainImageCount;
    g_vkrend_info.image_count = self->asFront.video.swapchainImageCount;

    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(self->asFront.video.physicalDevice, &props);
        self->asFront.vk_version = BrResStrDup(self, (char*)&props.apiVersion);
        (void)props;
    }
    self->asFront.vk_vendor = BrResStrDup(self, "Vulkan");
    self->asFront.vk_renderer = BrResStrDup(self, "Vulkan Device");
    self->asFront.vk_device = self->asFront.video.device;
    self->asFront.vk_physical_device = self->asFront.video.physicalDevice;
    self->asFront.vk_context = NULL;

    BrLogPrintf("VKREND: Vulkan Initialized\n");

    self->asFront.num_refs = 0;

    ObjectContainerAddFront(self->output_facility, (br_object*)self);
    return self;

cleanup_context:
    DevicePixelmapVKFree(self);
    BrResFreeNoCallback(self);

    return NULL;
}

static void BR_CMETHOD_DECL(br_device_pixelmap_vkf, free)(br_object* _self) {
    br_device_pixelmap* self = (br_device_pixelmap*)_self;

    VK_VideoClose(&self->asFront.video);

    DevicePixelmapVKFree(self);

    BrResFreeNoCallback(self);
}

const char* BR_CMETHOD_DECL(br_device_pixelmap_vkf, identifier)(br_object* self) {
    return ((br_device_pixelmap*)self)->pm_identifier;
}

br_token BR_CMETHOD_DECL(br_device_pixelmap_vkf, type)(br_object* self) {
    (void)self;
    return BRT_DEVICE_PIXELMAP;
}

br_boolean BR_CMETHOD_DECL(br_device_pixelmap_vkf, isType)(br_object* self, br_token t) {
    (void)self;
    return (t == BRT_DEVICE_PIXELMAP) || (t == BRT_OBJECT);
}

br_device* BR_CMETHOD_DECL(br_device_pixelmap_vkf, device)(br_object* self) {
    (void)self;
    return ((br_device_pixelmap*)self)->device;
}

br_size_t BR_CMETHOD_DECL(br_device_pixelmap_vkf, space)(br_object* self) {
    (void)self;
    return sizeof(br_device_pixelmap);
}

struct br_tv_template* BR_CMETHOD_DECL(br_device_pixelmap_vkf, templateQuery)(br_object* _self) {
    br_device_pixelmap* self = (br_device_pixelmap*)_self;

    if (self->device->templates.devicePixelmapFrontTemplate == NULL)
        self->device->templates.devicePixelmapFrontTemplate = BrTVTemplateAllocate(
            self->device, devicePixelmapFrontTemplateEntries, BR_ASIZE(devicePixelmapFrontTemplateEntries));

    return self->device->templates.devicePixelmapFrontTemplate;
}

br_error BR_CMETHOD_DECL(br_device_pixelmap_vkf, resize)(br_device_pixelmap* self, br_int_32 width, br_int_32 height) {
    self->pm_width = width;
    self->pm_height = height;
    return BRE_OK;
}

br_error BR_CMETHOD_DECL(br_device_pixelmap_vkf, doubleBuffer)(br_device_pixelmap* self, br_device_pixelmap* src) {
    if (self == src)
        return BRE_OK;

    if (ObjectDevice(src) != self->device)
        return BRE_UNSUPPORTED;

    if (self->use_type != BRT_NONE || src->use_type != BRT_OFFSCREEN) {
        return BRE_UNSUPPORTED;
    }

    BrPixelmapFlush((br_pixelmap*)src);

    DevicePixelmapVKSwapBuffers(self);

    HVIDEO hVideo = &self->asFront.video;
    hVideo->frameFlushed = 0;
    hVideo->renderingStarted = 0;

    // Call the platform swap callback (which includes the FPS limiter).
    // The GL driver does the same in ext_procs.c:DevicePixelmapGLSwapBuffers.
    if (self->asFront.callbacks.swap_buffers)
        self->asFront.callbacks.swap_buffers((br_pixelmap*)self);

    BrRendererFrameBegin();

    return BRE_OK;
}

static const struct br_device_pixelmap_dispatch devicePixelmapFrontDispatch = {
    .__reserved0 = NULL,
    .__reserved1 = NULL,
    .__reserved2 = NULL,
    .__reserved3 = NULL,
    ._free = BR_CMETHOD_REF(br_device_pixelmap_vkf, free),
    ._identifier = BR_CMETHOD_REF(br_device_pixelmap_vkf, identifier),
    ._type = BR_CMETHOD_REF(br_device_pixelmap_vkf, type),
    ._isType = BR_CMETHOD_REF(br_device_pixelmap_vkf, isType),
    ._device = BR_CMETHOD_REF(br_device_pixelmap_vkf, device),
    ._space = BR_CMETHOD_REF(br_device_pixelmap_vkf, space),

    ._templateQuery = BR_CMETHOD_REF(br_device_pixelmap_vkf, templateQuery),
    ._query = BR_CMETHOD_REF(br_object, query),
    ._queryBuffer = BR_CMETHOD_REF(br_object, queryBuffer),
    ._queryMany = BR_CMETHOD_REF(br_object, queryMany),
    ._queryManySize = BR_CMETHOD_REF(br_object, queryManySize),
    ._queryAll = BR_CMETHOD_REF(br_object, queryAll),
    ._queryAllSize = BR_CMETHOD_REF(br_object, queryAllSize),

    ._validSource = BR_CMETHOD_REF(br_device_pixelmap_mem, validSource),
    ._resize = BR_CMETHOD_REF(br_device_pixelmap_vkf, resize),
    ._match = BR_CMETHOD_REF(br_device_pixelmap_vk, match),
    ._allocateSub = BR_CMETHOD_REF(br_device_pixelmap_fail, allocateSub),

    ._copy = BR_CMETHOD_REF(br_device_pixelmap_fail, copy),
    ._copyTo = BR_CMETHOD_REF(br_device_pixelmap_fail, copyTo),
    ._copyFrom = BR_CMETHOD_REF(br_device_pixelmap_fail, copyFrom),
    ._fill = BR_CMETHOD_REF(br_device_pixelmap_fail, fill),
    ._doubleBuffer = BR_CMETHOD_REF(br_device_pixelmap_vkf, doubleBuffer),

    ._copyDirty = BR_CMETHOD_REF(br_device_pixelmap_fail, copyDirty),
    ._copyToDirty = BR_CMETHOD_REF(br_device_pixelmap_fail, copyToDirty),
    ._copyFromDirty = BR_CMETHOD_REF(br_device_pixelmap_fail, copyFromDirty),
    ._fillDirty = BR_CMETHOD_REF(br_device_pixelmap_fail, fillDirty),
    ._doubleBufferDirty = BR_CMETHOD_REF(br_device_pixelmap_fail, doubleBufferDirty),

    ._rectangle = BR_CMETHOD_REF(br_device_pixelmap_fail, rectangle),
    ._rectangle2 = BR_CMETHOD_REF(br_device_pixelmap_fail, rectangle2),
    ._rectangleCopy = BR_CMETHOD_REF(br_device_pixelmap_vk, rectangleCopy),
    ._rectangleCopyTo = BR_CMETHOD_REF(br_device_pixelmap_vk, rectangleCopyTo),
    ._rectangleCopyFrom = BR_CMETHOD_REF(br_device_pixelmap_fail, rectangleCopyFrom),
    ._rectangleStretchCopy = BR_CMETHOD_REF(br_device_pixelmap_vk, rectangleStretchCopy),
    ._rectangleStretchCopyTo = BR_CMETHOD_REF(br_device_pixelmap_fail, rectangleStretchCopyTo),
    ._rectangleStretchCopyFrom = BR_CMETHOD_REF(br_device_pixelmap_fail, rectangleStretchCopyFrom),
    ._rectangleFill = BR_CMETHOD_REF(br_device_pixelmap_vk, rectangleFill),
    ._pixelSet = BR_CMETHOD_REF(br_device_pixelmap_fail, pixelSet),
    ._line = BR_CMETHOD_REF(br_device_pixelmap_fail, line),
    ._copyBits = BR_CMETHOD_REF(br_device_pixelmap_fail, copyBits),

    ._text = BR_CMETHOD_REF(br_device_pixelmap_fail, text),
    ._textBounds = BR_CMETHOD_REF(br_device_pixelmap_gen, textBounds),

    ._rowSize = BR_CMETHOD_REF(br_device_pixelmap_fail, rowSize),
    ._rowQuery = BR_CMETHOD_REF(br_device_pixelmap_fail, rowQuery),
    ._rowSet = BR_CMETHOD_REF(br_device_pixelmap_fail, rowSet),

    ._pixelQuery = BR_CMETHOD_REF(br_device_pixelmap_fail, pixelQuery),
    ._pixelAddressQuery = BR_CMETHOD_REF(br_device_pixelmap_fail, pixelAddressQuery),

    ._pixelAddressSet = BR_CMETHOD_REF(br_device_pixelmap_fail, pixelAddressSet),
    ._originSet = BR_CMETHOD_REF(br_device_pixelmap_mem, originSet),

    ._flush = BR_CMETHOD_REF(br_device_pixelmap_fail, flush),
    ._synchronise = BR_CMETHOD_REF(br_device_pixelmap_fail, synchronise),
    ._directLock = BR_CMETHOD_REF(br_device_pixelmap_fail, directLock),
    ._directUnlock = BR_CMETHOD_REF(br_device_pixelmap_fail, directUnlock),
    ._getControls = BR_CMETHOD_REF(br_device_pixelmap_fail, getControls),
    ._setControls = BR_CMETHOD_REF(br_device_pixelmap_fail, setControls)
};
