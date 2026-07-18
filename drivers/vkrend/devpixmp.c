#include "brassert.h"
#include "drv.h"
#include <string.h>

extern int gMap_mode;

static const struct br_device_pixelmap_dispatch devicePixelmapDispatch;

static br_error custom_query(br_value* pvalue, void** extra, br_size_t* pextra_size, void* block, struct br_tv_template_entry* tep) {
    const br_device_pixelmap* self = block;

    if (tep->token == BRT_VULKAN_CALLBACKS_P) {
        if (self->use_type == BRT_OFFSCREEN)
            pvalue->p = (void*)&self->asBack;
        else if (self->use_type == BRT_DEPTH)
            pvalue->p = (void*)&self->asDepth;
        else
            pvalue->p = NULL;

        return BRE_OK;
    }

    return BRE_UNKNOWN;
}

static const br_tv_custom custom = {
    .query = custom_query,
    .set = NULL,
    .extra_size = NULL,
};

#define F(f) offsetof(struct br_device_pixelmap, f)
static struct br_tv_template_entry devicePixelmapTemplateEntries[] = {
    { BRT(WIDTH_I32), F(pm_width), BRTV_QUERY | BRTV_ALL, BRTV_CONV_I32_U16, 0 },
    { BRT(HEIGHT_I32), F(pm_height), BRTV_QUERY | BRTV_ALL, BRTV_CONV_I32_U16, 0 },
    { BRT(PIXEL_TYPE_U8), F(pm_type), BRTV_QUERY | BRTV_ALL, BRTV_CONV_I32_U8, 0 },
    { BRT(OUTPUT_FACILITY_O), F(output_facility), BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY, 0 },
    { BRT(FACILITY_O), F(output_facility), BRTV_QUERY, BRTV_CONV_COPY, 0 },
    { BRT(IDENTIFIER_CSTR), F(pm_identifier), BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY, 0 },
    { BRT(MSAA_SAMPLES_I32), F(msaa_samples), BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY, 0 },
    { BRT(VULKAN_CALLBACKS_P), 0, BRTV_QUERY | BRTV_ALL, BRTV_CONV_CUSTOM, (br_uintptr_t)&custom },
};
#undef F

static br_error recreate_renderbuffers(br_device_pixelmap* self) {
    return BRE_OK;
}

static void delete_vk_resources(br_device_pixelmap* self) {
    HVIDEO hVideo = &self->screen->asFront.video;
    if (hVideo->device == VK_NULL_HANDLE)
        return;

    if (self->use_type == BRT_DEPTH) {
        if (self->asDepth.vkDepthView) vkDestroyImageView(hVideo->device, self->asDepth.vkDepthView, NULL);
        if (self->asDepth.vkDepthMemory) vkFreeMemory(hVideo->device, self->asDepth.vkDepthMemory, NULL);
        if (self->asDepth.vkDepth) vkDestroyImage(hVideo->device, self->asDepth.vkDepth, NULL);
    } else if (self->use_type == BRT_OFFSCREEN) {
        if (self->asBack.vkFramebuffer) vkDestroyFramebuffer(hVideo->device, self->asBack.vkFramebuffer, NULL);
        if (self->asBack.vkImageView) vkDestroyImageView(hVideo->device, self->asBack.vkImageView, NULL);
        if (self->asBack.vkMemory) vkFreeMemory(hVideo->device, self->asBack.vkMemory, NULL);
        if (self->asBack.vkImage) vkDestroyImage(hVideo->device, self->asBack.vkImage, NULL);
        if (hVideo->overlayImageView) { vkDestroyImageView(hVideo->device, hVideo->overlayImageView, NULL); hVideo->overlayImageView = VK_NULL_HANDLE; }
        if (hVideo->overlayMemory) { vkFreeMemory(hVideo->device, hVideo->overlayMemory, NULL); hVideo->overlayMemory = VK_NULL_HANDLE; }
        if (hVideo->overlayImage) { vkDestroyImage(hVideo->device, hVideo->overlayImage, NULL); hVideo->overlayImage = VK_NULL_HANDLE; }
        if (hVideo->lockedPixels) { BrMemFree(hVideo->lockedPixels); hVideo->lockedPixels = NULL; }
    }
}

void BR_CMETHOD_DECL(br_device_pixelmap_vk, free)(br_object* _self) {
    br_device_pixelmap* self = (br_device_pixelmap*)_self;

    if (self->sub_pixelmap) {
        return;
    }

    delete_vk_resources(self);
}

const char* BR_CMETHOD_DECL(br_device_pixelmap_vk, identifier)(br_object* self) {
    return ((br_device_pixelmap*)self)->pm_identifier;
}

br_token BR_CMETHOD_DECL(br_device_pixelmap_vk, type)(br_object* self) {
    (void)self;
    return BRT_DEVICE_PIXELMAP;
}

br_boolean BR_CMETHOD_DECL(br_device_pixelmap_vk, isType)(br_object* self, br_token t) {
    (void)self;
    return (t == BRT_DEVICE_PIXELMAP) || (t == BRT_OBJECT);
}

br_device* BR_CMETHOD_DECL(br_device_pixelmap_vk, device)(br_object* self) {
    (void)self;
    return ((br_device_pixelmap*)self)->device;
}

br_size_t BR_CMETHOD_DECL(br_device_pixelmap_vk, space)(br_object* self) {
    (void)self;
    return sizeof(br_device_pixelmap);
}

struct br_tv_template* BR_CMETHOD_DECL(br_device_pixelmap_vk, templateQuery)(br_object* _self) {
    br_device_pixelmap* self = (br_device_pixelmap*)_self;

    if (self->device->templates.devicePixelmapTemplate == NULL)
        self->device->templates.devicePixelmapTemplate = BrTVTemplateAllocate(self->device, devicePixelmapTemplateEntries,
            BR_ASIZE(devicePixelmapTemplateEntries));

    return self->device->templates.devicePixelmapTemplate;
}

br_error BR_CMETHOD_DECL(br_device_pixelmap_vk, resize)(br_device_pixelmap* self, br_int_32 width, br_int_32 height) {
    self->pm_width = width;
    self->pm_height = height;
    return recreate_renderbuffers(self);
}

struct pixelmapMatchTokens {
    br_int_32 width;
    br_int_32 height;
    br_int_32 pixel_bits;
    br_uint_8 type;
    br_token use_type;
    br_int_32 msaa_samples;
};

#define F(f) offsetof(struct pixelmapMatchTokens, f)
static struct br_tv_template_entry pixelmapMatchTemplateEntries[] = {
    { BRT(WIDTH_I32), F(width), BRTV_SET, BRTV_CONV_COPY },
    { BRT(HEIGHT_I32), F(height), BRTV_SET, BRTV_CONV_COPY },
    { BRT(PIXEL_BITS_I32), F(pixel_bits), BRTV_SET, BRTV_CONV_COPY },
    { BRT(PIXEL_TYPE_U8), F(type), BRTV_SET, BRTV_CONV_COPY },
    { BRT(USE_T), F(use_type), BRTV_SET, BRTV_CONV_COPY },
    { BRT(MSAA_SAMPLES_I32), F(msaa_samples), BRTV_SET, BRTV_CONV_COPY },
};
#undef F

br_error BR_CMETHOD_DECL(br_device_pixelmap_vk, match)(br_device_pixelmap* self, br_device_pixelmap** newpm, br_token_value* tv) {
    br_int_32 count;
    br_error err;
    br_device_pixelmap* pm;
    const char* typestring;
    VkFormat vkFormat;
    VkImageTiling tiling;
    VkImageUsageFlags usage;
    VkMemoryPropertyFlags memProps;
    HVIDEO hVideo;
    struct pixelmapMatchTokens mt = {
        .width = self->pm_width,
        .height = self->pm_height,
        .pixel_bits = -1,
        .type = BR_PMT_MAX,
        .use_type = BRT_NONE,
        .msaa_samples = 0,
    };
    char tmp[80];

    hVideo = &self->screen->asFront.video;

    if (self->device->templates.pixelmapMatchTemplate == NULL) {
        self->device->templates.pixelmapMatchTemplate = BrTVTemplateAllocate(self->device, pixelmapMatchTemplateEntries,
            BR_ASIZE(pixelmapMatchTemplateEntries));
    }

    err = BrTokenValueSetMany(&mt, &count, NULL, tv, self->device->templates.pixelmapMatchTemplate);
    if (err != BRE_OK)
        return err;

    if (mt.use_type == BRT_NO_RENDER)
        mt.use_type = BRT_OFFSCREEN;

    switch (mt.use_type) {
    case BRT_OFFSCREEN:
        typestring = "Backbuffer";
        break;
    case BRT_DEPTH:
        typestring = "Depth";

        if (self->use_type != BRT_OFFSCREEN)
            return BRE_UNSUPPORTED;

        if (self->asBack.depthbuffer != NULL)
            return BRE_FAIL;

        if (mt.pixel_bits != 16)
            return BRE_UNSUPPORTED;

        mt.type = BR_PMT_DEPTH_16;
        break;
    default:
        return BRE_UNSUPPORTED;
    }

    if (self->use_type == BRT_NONE && mt.use_type != BRT_OFFSCREEN)
        return BRE_UNSUPPORTED;

    if (mt.type == BR_PMT_MAX)
        mt.type = self->pm_type;

    err = VK_BrPixelmapGetTypeDetails(mt.type, &vkFormat, &tiling, &usage, &memProps);
    if (err != BRE_OK)
        return err;

    if (mt.msaa_samples < 0)
        mt.msaa_samples = 0;

    pm = BrResAllocate(self->device, sizeof(br_device_pixelmap), BR_MEMORY_OBJECT);
    pm->dispatch = &devicePixelmapDispatch;
    BrSprintfN(tmp, sizeof(tmp) - 1, "Vulkan:%s:%dx%d", typestring, mt.width, mt.height);
    pm->pm_identifier = BrResStrDup(self, tmp);
    pm->device = self->device;
    pm->output_facility = self->output_facility;
    pm->use_type = mt.use_type;
    pm->msaa_samples = mt.msaa_samples;
    pm->screen = self->screen;
    ++self->screen->asFront.num_refs;

    pm->pm_type = mt.type;
    pm->pm_width = mt.width;
    pm->pm_height = mt.height;
    switch (mt.type) {
        case BR_PMT_RGB_555:
        case BR_PMT_RGB_565:
            pm->pm_row_bytes = 2 * mt.width;
            break;
        case BR_PMT_RGB_888:
            pm->pm_row_bytes = 3 * mt.width;
            break;
        case BR_PMT_RGBA_8888:
        case BR_PMT_RGBX_888:
            pm->pm_row_bytes = 4 * mt.width;
            break;
        case BR_PMT_INDEX_8:
            pm->pm_row_bytes = 1 * mt.width;
            break;
        case BR_PMT_DEPTH_16:
            pm->pm_row_bytes = 2 * mt.width;
            break;
        default:
            pm->pm_row_bytes = 4 * mt.width;
            break;
    }
    pm->pm_flags = BR_PMF_NO_ACCESS;
    pm->pm_origin_x = 0;
    pm->pm_origin_y = 0;
    pm->pm_base_x = 0;
    pm->pm_base_y = 0;
    pm->sub_pixelmap = 0;
    if (mt.use_type == BRT_OFFSCREEN) {
        VkImageCreateInfo ici = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.format = vkFormat;
        ici.extent.width = mt.width;
        ici.extent.height = mt.height;
        ici.extent.depth = 1;
        ici.mipLevels = 1;
        ici.arrayLayers = 1;
        ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.tiling = tiling;
        ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (vkCreateImage(hVideo->device, &ici, NULL, &pm->asBack.vkImage) != VK_SUCCESS)
            return BRE_FAIL;

        VkMemoryRequirements mr;
        vkGetImageMemoryRequirements(hVideo->device, pm->asBack.vkImage, &mr);

        VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mai.allocationSize = mr.size;
        mai.memoryTypeIndex = VK_FindMemoryType(hVideo->physicalDevice, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (mai.memoryTypeIndex == UINT32_MAX ||
            vkAllocateMemory(hVideo->device, &mai, NULL, &pm->asBack.vkMemory) != VK_SUCCESS) {
            vkDestroyImage(hVideo->device, pm->asBack.vkImage, NULL);
            return BRE_FAIL;
        }
        vkBindImageMemory(hVideo->device, pm->asBack.vkImage, pm->asBack.vkMemory, 0);

        VkImageViewCreateInfo ivci = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ivci.image = pm->asBack.vkImage;
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = vkFormat;
        ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.levelCount = 1;
        ivci.subresourceRange.layerCount = 1;
        if (vkCreateImageView(hVideo->device, &ivci, NULL, &pm->asBack.vkImageView) != VK_SUCCESS) {
            vkDestroyImage(hVideo->device, pm->asBack.vkImage, NULL);
            vkFreeMemory(hVideo->device, pm->asBack.vkMemory, NULL);
            return BRE_FAIL;
        }
    } else {
        ASSERT(mt.use_type == BRT_DEPTH);
        self->asBack.depthbuffer = pm;
        pm->asDepth.backbuffer = self;
    }

    if (recreate_renderbuffers(pm) != BRE_OK) {
        --self->screen->asFront.num_refs;
        delete_vk_resources(pm);
        BrResFreeNoCallback(pm);
        return BRE_FAIL;
    }

    pm->pm_origin_x = self->pm_origin_x;
    pm->pm_origin_y = self->pm_origin_y;

    *newpm = pm;
    ObjectContainerAddFront(self->output_facility, (br_object*)pm);
    return BRE_OK;
}

br_error BR_CMETHOD_DECL(br_device_pixelmap_vk, rectangleStretchCopy)(br_device_pixelmap* self, br_rectangle* d,
    br_device_pixelmap* src, br_rectangle* s) {
    return BRE_FAIL;
}

br_error BR_CMETHOD_DECL(br_device_pixelmap_vk, rectangleCopy)(br_device_pixelmap* self, br_point* p,
    br_device_pixelmap* src, br_rectangle* sr) {
    return BRE_FAIL;
}

br_error BR_CMETHOD_DECL(br_device_pixelmap_vk, rectangleFill)(br_device_pixelmap* self, br_rectangle* rect, br_uint_32 colour) {
    br_uint_8* px8;
    br_uint_16* px16;

    if (self->use_type == BRT_OFFSCREEN) {
        if (self->pm_pixels != NULL) {
            switch (self->pm_type) {
            case BR_PMT_INDEX_8: {
                int stride = self->pm_row_bytes;
                int base_y = self->pm_base_y;
                int base_x = self->pm_base_x;
                px8 = self->pm_pixels;
                br_uint_8 index = colour & 0xFF;
                for (int y = rect->y; y < rect->y + rect->h; y++) {
                    for (int x = rect->x; x < rect->x + rect->w; x++) {
                        px8[(y + base_y) * stride + (x + base_x)] = index;
                    }
                }
                break;
            }
            case BR_PMT_RGB_565: {
                int stride = self->pm_row_bytes / 2;
                int base_y = self->pm_base_y;
                int base_x = self->pm_base_x;
                px16 = self->pm_pixels;
                br_uint_16 rgb565 = (br_uint_16)(((BR_RED(colour) >> 3) << 11) | ((BR_GRN(colour) >> 2) << 5) | (BR_BLU(colour) >> 3));
                for (int y = rect->y; y < rect->y + rect->h; y++) {
                    for (int x = rect->x; x < rect->x + rect->w; x++) {
                        px16[(y + base_y) * stride + (x + base_x)] = rgb565;
                    }
                }
                break;
            }
            default:
                return BRE_UNSUPPORTED;
            }
        }
    } else if (self->use_type == BRT_DEPTH) {
        if (self->screen != NULL) {
            HVIDEO hVideo = &self->screen->asFront.video;
            if (hVideo->renderPassActive && hVideo->isRecording) {
                VkClearAttachment ca = {0};
                ca.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                ca.clearValue.depthStencil.depth = 1.0f;
                VkClearRect cr = {{{0, 0}, {hVideo->swapchainExtent.width, hVideo->swapchainExtent.height}}, 0, 1};
                VkCommandBuffer cmd = hVideo->drawCommandBuffers[hVideo->currentImageIndex];
                vkCmdClearAttachments(cmd, 1, &ca, 1, &cr);
            }
        }
        return BRE_OK;
    } else {
        return BRE_UNSUPPORTED;
    }

    return BRE_OK;
}

br_error BR_CMETHOD_DECL(br_device_pixelmap_vk, fill)(br_device_pixelmap* self, br_uint_32 colour) {
    br_rectangle r;
    r.x = 0;
    r.y = 0;
    r.w = self->pm_width;
    r.h = self->pm_height;
    return DevicePixelmapRectangleFill(self, &r, colour);
}

br_error BR_CMETHOD_DECL(br_device_pixelmap_vk, rectangleCopyTo)(br_device_pixelmap* self, br_point* p,
    br_device_pixelmap* src, br_rectangle* sr) {
    p->x = -self->pm_origin_x;
    p->y = -self->pm_origin_y;
    sr->x = -src->pm_origin_x;
    sr->y = -src->pm_origin_y;

    if (src->pm_pixels == NULL && src->pm_type == BR_PMT_INDEX_8)
        return BRE_FAIL;

    HVIDEO hVideo = &self->screen->asFront.video;
    VkImage dstImage = self->asBack.vkImage;
    if (dstImage == VK_NULL_HANDLE)
        return BRE_FAIL;

    uint32_t bytesPerPixel;
    switch (src->pm_type) {
    case BR_PMT_RGB_565:
        bytesPerPixel = 2;
        break;
    case BR_PMT_INDEX_8:
    case BR_PMT_RGBA_8888:
    case BR_PMT_RGBX_888:
        bytesPerPixel = 4;
        break;
    default:
        return BRE_UNSUPPORTED;
    }

    VkDeviceSize pixelDataSize = (VkDeviceSize)sr->w * sr->h * bytesPerPixel;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    if (VK_CreateStagingBuffer(hVideo, pixelDataSize, &stagingBuffer, &stagingMemory) != VK_SUCCESS)
        return BRE_FAIL;

    void* mapped;
    vkMapMemory(hVideo->device, stagingMemory, 0, pixelDataSize, 0, &mapped);

    if (src->pm_pixels) {
        if (src->pm_type == BR_PMT_INDEX_8) {
            br_uint_8* src8 = src->pm_pixels;
            uint8_t* dst8 = mapped;
            int srcStride = src->pm_row_bytes;
            for (int y = 0; y < sr->h; y++) {
                for (int x = 0; x < sr->w; x++) {
                    br_uint_8 idx = src8[(y + sr->y) * srcStride + (x + sr->x)];
                    br_colour c = self->screen->clut->entries[idx];
                    uint8_t* d = &dst8[(y * sr->w + x) * 4];
                    d[0] = BR_BLU(c);
                    d[1] = BR_GRN(c);
                    d[2] = BR_RED(c);
                    d[3] = BR_ALPHA(c);
                }
            }
        } else {
            int srcStride = src->pm_row_bytes;
            int srcBpp = bytesPerPixel;
            for (int y = 0; y < sr->h; y++)
                memcpy((char*)mapped + y * sr->w * srcBpp,
                       src->pm_pixels + (y + sr->y) * srcStride + sr->x * srcBpp,
                       sr->w * srcBpp);
        }
    }
    vkUnmapMemory(hVideo->device, stagingMemory);

    VkCommandBufferAllocateInfo cbAi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbAi.commandPool = hVideo->commandPool;
    cbAi.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbAi.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(hVideo->device, &cbAi, &cmd);

    VkCommandBufferBeginInfo cbBegin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    cbBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &cbBegin);

    VkImageMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    barrier.image = dstImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
    barrier.srcAccessMask = VK_ACCESS_2_NONE;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;

    VkDependencyInfo depInfo = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(cmd, &depInfo);

    VkBufferImageCopy copyRegion = {0};
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageOffset.x = self->pm_base_x + p->x;
    copyRegion.imageOffset.y = self->pm_base_y + p->y;
    copyRegion.imageExtent.width = sr->w;
    copyRegion.imageExtent.height = sr->h;
    copyRegion.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(cmd, stagingBuffer, dstImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    vkCmdPipelineBarrier2(cmd, &depInfo);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    vkQueueSubmit(hVideo->graphicsQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(hVideo->graphicsQueue);

    vkFreeCommandBuffers(hVideo->device, hVideo->commandPool, 1, &cmd);
    vkDestroyBuffer(hVideo->device, stagingBuffer, NULL);
    vkFreeMemory(hVideo->device, stagingMemory, NULL);

    return BRE_OK;
}

br_error BR_CMETHOD_DECL(br_device_pixelmap_vk, allocateSub)(br_device_pixelmap* self, br_device_pixelmap** newpm, br_rectangle* rect) {
    br_device_pixelmap* pm;
    br_rectangle out;

    pm = BrResAllocate(self->device, sizeof(*pm), BR_MEMORY_PIXELMAP);

    *pm = *self;

    if (PixelmapRectangleClip(&out, rect, (br_pixelmap*)self) == BR_CLIP_REJECT)
        return BRE_FAIL;

    pm->sub_pixelmap = BR_TRUE;
    pm->parent_height = self->pm_height;

    if (out.w != self->pm_width)
        pm->pm_flags &= ~BR_PMF_LINEAR;

    pm->pm_base_x += out.x;
    pm->pm_base_y += out.y;

    pm->pm_width = out.w;
    pm->pm_height = out.h;

    pm->pm_origin_x = 0;
    pm->pm_origin_y = 0;

    *newpm = (br_device_pixelmap*)pm;

    return BRE_OK;
}

br_error BR_CMETHOD_DECL(br_device_pixelmap_vk, flush)(br_device_pixelmap* self) {
    HVIDEO hVideo = &self->screen->asFront.video;

    if (self->sub_pixelmap) {
        return BRE_OK;
    }

    if (!self->asBack.possiblyDirty && !hVideo->overlayDirty) {
        return BRE_OK;
    }

    if (hVideo->lockedPixels != NULL) {
        size_t pixelSize = (self->pm_type == BR_PMT_RGB_565) ? 2 : 4;
        int useRgbaOverlay = (self->pm_type == BR_PMT_RGB_565 || self->pm_type == BR_PMT_RGB_555);
        VkDeviceSize imageSize;
        VkFormat overlayVkFormat;
        if (useRgbaOverlay) {
            overlayVkFormat = VK_FORMAT_B8G8R8A8_UNORM;
            imageSize = self->pm_width * self->pm_height * 4;
        } else {
            overlayVkFormat = VK_FORMAT_UNDEFINED;
            imageSize = self->pm_width * self->pm_height * pixelSize;
        }

        if (hVideo->overlayImage == VK_NULL_HANDLE) {
            VkFormat vkFormat;
            VkImageTiling tiling;
            VkImageUsageFlags usage;
            VkMemoryPropertyFlags memProps;

            if (useRgbaOverlay) {
                vkFormat = overlayVkFormat;
                tiling = VK_IMAGE_TILING_LINEAR;
                usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                memProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            } else {
                VK_BrPixelmapGetTypeDetails(self->pm_type, &vkFormat, &tiling, &usage, &memProps);
            }

            VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            ii.imageType = VK_IMAGE_TYPE_2D;
            ii.format = vkFormat;
            ii.extent.width = self->pm_width;
            ii.extent.height = self->pm_height;
            ii.extent.depth = 1;
            ii.mipLevels = 1;
            ii.arrayLayers = 1;
            ii.samples = VK_SAMPLE_COUNT_1_BIT;
            ii.tiling = VK_IMAGE_TILING_LINEAR;
            ii.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            if (vkCreateImage(hVideo->device, &ii, NULL, &hVideo->overlayImage) != VK_SUCCESS)
                return BRE_FAIL;

            VkMemoryRequirements memReq;
            vkGetImageMemoryRequirements(hVideo->device, hVideo->overlayImage, &memReq);

            VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            ai.allocationSize = memReq.size;
            ai.memoryTypeIndex = VK_FindMemoryType(hVideo->physicalDevice, memReq.memoryTypeBits,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            if (ai.memoryTypeIndex == UINT32_MAX) {
                vkDestroyImage(hVideo->device, hVideo->overlayImage, NULL);
                hVideo->overlayImage = VK_NULL_HANDLE;
                return BRE_FAIL;
            }

            if (vkAllocateMemory(hVideo->device, &ai, NULL, &hVideo->overlayMemory) != VK_SUCCESS) {
                vkDestroyImage(hVideo->device, hVideo->overlayImage, NULL);
                hVideo->overlayImage = VK_NULL_HANDLE;
                return BRE_FAIL;
            }
            vkBindImageMemory(hVideo->device, hVideo->overlayImage, hVideo->overlayMemory, 0);

            VkImageViewCreateInfo ivi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            ivi.image = hVideo->overlayImage;
            ivi.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ivi.format = vkFormat;
            ivi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            ivi.subresourceRange.baseMipLevel = 0;
            ivi.subresourceRange.levelCount = 1;
            ivi.subresourceRange.baseArrayLayer = 0;
            ivi.subresourceRange.layerCount = 1;
            if (vkCreateImageView(hVideo->device, &ivi, NULL, &hVideo->overlayImageView) != VK_SUCCESS) {
                vkFreeMemory(hVideo->device, hVideo->overlayMemory, NULL);
                vkDestroyImage(hVideo->device, hVideo->overlayImage, NULL);
                hVideo->overlayImage = VK_NULL_HANDLE;
                hVideo->overlayMemory = VK_NULL_HANDLE;
                hVideo->overlayImageView = VK_NULL_HANDLE;
                return BRE_FAIL;
            }
        }

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;
        if (VK_CreateStagingBuffer(hVideo, imageSize, &stagingBuffer, &stagingMemory) != VK_SUCCESS)
            return BRE_FAIL;

        void* data;
        vkMapMemory(hVideo->device, stagingMemory, 0, imageSize, 0, &data);
        size_t srcOffset = self->pm_base_y * self->pm_row_bytes + self->pm_base_x * pixelSize;
        if (useRgbaOverlay) {
            if (gMap_mode) {
                for (int i = 0; i < hVideo->dimAreaCount; i++) {
                    int ax = hVideo->dimAreas[i].x, ay = hVideo->dimAreas[i].y;
                    int aw = hVideo->dimAreas[i].w, ah = hVideo->dimAreas[i].h;
                    int row_w = self->pm_row_bytes / 2;
                    for (int dy = 0; dy < ah; dy++) {
                        int py = ay + dy;
                        if (py < 0 || py >= self->pm_height) continue;
                        for (int dx = 0; dx < aw; dx++) {
                            int px = ax + dx;
                            if (px < 0 || px >= self->pm_width) continue;
                            int off = py * row_w + px;
                            br_uint_16 p = ((br_uint_16*)hVideo->lockedPixels)[off];
                            if (p == BR_COLOUR_565(31, 0, 31)) continue;
                            int r5 = (p >> 11) & 0x1F, g6 = (p >> 5) & 0x3F, b5 = p & 0x1F;
                            r5 = r5 >> 1; g6 = g6 >> 1; b5 = b5 >> 1;
                            ((br_uint_16*)hVideo->lockedPixels)[off] = (br_uint_16)((r5 << 11) | (g6 << 5) | b5);
                        }
                    }
                }
                hVideo->dimAreaCount = 0;
            } else {
                hVideo->dimAreaCount = 0;
            }
            for (int i = 0; i < hVideo->clearAreaCount; i++) {
                int row_w = self->pm_row_bytes / 2;
                for (int cy = 0; cy < hVideo->clearAreas[i].h; cy++) {
                    int off = (cy + hVideo->clearAreas[i].y) * row_w + hVideo->clearAreas[i].x;
                    for (int cx = 0; cx < hVideo->clearAreas[i].w; cx++) {
                        ((br_uint_16*)hVideo->lockedPixels)[off + cx] = BR_COLOUR_565(31, 0, 31);
                    }
                }
            }
            hVideo->clearAreaCount = 0;
            if (hVideo->pratcamAreaCount) {
                int row_w = self->pm_row_bytes / 2;
                int px = hVideo->pratcamArea.x, py = hVideo->pratcamArea.y;
                int pw = hVideo->pratcamArea.w, ph = hVideo->pratcamArea.h;
                for (int dy = 0; dy < ph; dy++) {
                    int sy = py + dy;
                    if (sy < 0 || sy >= self->pm_height) continue;
                    int off = sy * row_w + px;
                    int cw = pw;
                    if (px < 0) { off -= px; cw += px; }
                    if (px + cw > self->pm_width) cw = self->pm_width - px;
                    if (off < 0) continue;
                    for (int dx = 0; dx < cw; dx++) {
                        ((br_uint_16*)hVideo->lockedPixels)[off + dx] = BR_COLOUR_565(31, 0, 31);
                    }
                }
                hVideo->pratcamAreaCount = 0;
            }
            if (!hVideo->renderingStarted && hVideo->mainViewportW > 0 && hVideo->mainViewportH > 0 && !gMap_mode) {
                int row_w = self->pm_row_bytes / 2;
                for (int vy = hVideo->mainViewportY; vy < hVideo->mainViewportY + hVideo->mainViewportH; vy++) {
                    if (vy < 0 || vy >= self->pm_height) continue;
                    int off = vy * row_w + hVideo->mainViewportX;
                    int vw = hVideo->mainViewportW;
                    if (hVideo->mainViewportX + vw > self->pm_width) vw = self->pm_width - hVideo->mainViewportX;
                    if (hVideo->mainViewportX < 0) { off -= hVideo->mainViewportX; vw += hVideo->mainViewportX; }
                    for (int vx = 0; vx < vw; vx++) {
                        ((br_uint_16*)hVideo->lockedPixels)[off + vx] = BR_COLOUR_565(31, 0, 31);
                    }
                }
            }
            br_uint_16* src = (br_uint_16*)((char*)hVideo->lockedPixels + srcOffset);
            br_uint_32* dst = (br_uint_32*)data;
            for (int y = 0; y < self->pm_height; y++) {
                for (int x = 0; x < self->pm_width; x++) {
                    br_uint_16 p = src[y * (self->pm_row_bytes / 2) + x];
                    if (p == BR_COLOUR_565(31, 0, 31)) {
                        dst[y * self->pm_width + x] = 0;  // transparent
                    } else {
                        int r5 = (p >> 11) & 0x1F;
                        int g6 = (p >> 5) & 0x3F;
                        int b5 = p & 0x1F;
                        dst[y * self->pm_width + x] = (b5 * 255 / 31)
                            | ((g6 * 255 / 63) << 8)
                            | ((r5 * 255 / 31) << 16)
                            | (0xFF << 24);
                    }
                }
            }
        } else {
            int bpp = (self->pm_type == BR_PMT_RGB_565) ? 2 : 4;
            for (int i = 0; i < hVideo->clearAreaCount; i++) {
                br_uint_32 magenta = (bpp == 2) ? BR_COLOUR_565(31, 0, 31) : BR_COLOUR_RGB(255, 0, 255);
                int row_w = self->pm_row_bytes / bpp;
                for (int cy = 0; cy < hVideo->clearAreas[i].h; cy++) {
                    int off = (cy + hVideo->clearAreas[i].y) * row_w + hVideo->clearAreas[i].x;
                    for (int cx = 0; cx < hVideo->clearAreas[i].w; cx++) {
                        if (bpp == 2)
                            ((br_uint_16*)hVideo->lockedPixels)[off + cx] = (br_uint_16)magenta;
                        else
                            ((br_uint_32*)hVideo->lockedPixels)[off + cx] = magenta;
                    }
                }
            }
            hVideo->clearAreaCount = 0;
            if (!hVideo->renderingStarted && hVideo->mainViewportW > 0 && hVideo->mainViewportH > 0 && !gMap_mode) {
                int bpp = (self->pm_type == BR_PMT_RGB_565) ? 2 : 4;
                br_uint_32 magenta = (bpp == 2) ? BR_COLOUR_565(31, 0, 31) : BR_COLOUR_RGB(255, 0, 255);
                int row_w = self->pm_row_bytes / bpp;
                for (int vy = hVideo->mainViewportY; vy < hVideo->mainViewportY + hVideo->mainViewportH; vy++) {
                    if (vy < 0 || vy >= self->pm_height) continue;
                    int off = vy * row_w + hVideo->mainViewportX;
                    int vw = hVideo->mainViewportW;
                    if (hVideo->mainViewportX + vw > self->pm_width) vw = self->pm_width - hVideo->mainViewportX;
                    if (hVideo->mainViewportX < 0) { off -= hVideo->mainViewportX; vw += hVideo->mainViewportX; }
                    for (int vx = 0; vx < vw; vx++) {
                        if (bpp == 2)
                            ((br_uint_16*)hVideo->lockedPixels)[off + vx] = (br_uint_16)magenta;
                        else
                            ((br_uint_32*)hVideo->lockedPixels)[off + vx] = magenta;
                    }
                }
            }
            memcpy(data, (char*)hVideo->lockedPixels + srcOffset, (size_t)imageSize);
        }
        vkUnmapMemory(hVideo->device, stagingMemory);

        VkCommandBufferAllocateInfo cbAi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cbAi.commandPool = hVideo->commandPool;
        cbAi.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbAi.commandBufferCount = 1;
        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(hVideo->device, &cbAi, &cmd);

        VkCommandBufferBeginInfo cbBegin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        cbBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &cbBegin);

        VkImageMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = hVideo->overlayImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
        barrier.srcAccessMask = VK_ACCESS_2_NONE;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;

        VkDependencyInfo depInfo = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(cmd, &depInfo);

        VkBufferImageCopy copyRegion = {0};
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent.width = self->pm_width;
        copyRegion.imageExtent.height = self->pm_height;
        copyRegion.imageExtent.depth = 1;

        vkCmdCopyBufferToImage(cmd, stagingBuffer, hVideo->overlayImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        vkCmdPipelineBarrier2(cmd, &depInfo);

        vkEndCommandBuffer(cmd);

        VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd;
        vkQueueSubmit(hVideo->graphicsQueue, 1, &si, VK_NULL_HANDLE);
        vkQueueWaitIdle(hVideo->graphicsQueue);

        vkFreeCommandBuffers(hVideo->device, hVideo->commandPool, 1, &cmd);

        vkDestroyBuffer(hVideo->device, stagingBuffer, NULL);
        vkFreeMemory(hVideo->device, stagingMemory, NULL);

        hVideo->overlayDirty = 1;
    }

    self->asBack.possiblyDirty = 0;

    return BRE_OK;
}

br_error BR_CMETHOD_DECL(br_device_pixelmap_vk, directLock)(br_device_pixelmap* self, br_boolean block) {
    ASSERT(self->pm_pixels == NULL);
    ASSERT(self->use_type == BRT_OFFSCREEN);

    HVIDEO hVideo = &self->screen->asFront.video;

    if (hVideo->lockedPixels == NULL) {
        size_t pixelSize = (self->pm_type == BR_PMT_RGB_565) ? 2 : 4;
        hVideo->lockedPixels = BrMemAllocate(self->pm_height * self->pm_row_bytes, BR_MEMORY_PIXELS);
    }
    hVideo->pm_type = self->pm_type;
    hVideo->pm_width = self->pm_width;
    hVideo->pm_height = self->pm_height;
    hVideo->pm_row_bytes = self->pm_row_bytes;
    if (!hVideo->frameFlushed) {
        int bpp = (self->pm_type == BR_PMT_RGB_565 || self->pm_type == BR_PMT_RGB_555) ? 2 : 4;
        br_uint_32 magenta = (bpp == 2) ? BR_COLOUR_565(31, 0, 31) : BR_COLOUR_RGB(255, 0, 255);
        _MemFill_A(hVideo->lockedPixels, 0, self->pm_height * self->pm_row_bytes / bpp, bpp, magenta);
        hVideo->frameFlushed = 1;
    }

    self->pm_pixels = hVideo->lockedPixels;
    self->asBack.locked = 1;
    self->asBack.possiblyDirty = 1;

    return BRE_OK;
}

br_error BR_CMETHOD_DECL(br_device_pixelmap_vk, directUnlock)(br_device_pixelmap* self) {
    ASSERT(self->pm_pixels != NULL);
    ASSERT(self->use_type == BRT_OFFSCREEN);

    self->pm_pixels = NULL;
    self->asBack.possiblyDirty = 1;
    self->asBack.locked = 0;

    return BRE_OK;
}

static const struct br_device_pixelmap_dispatch devicePixelmapDispatch = {
    .__reserved0 = NULL,
    .__reserved1 = NULL,
    .__reserved2 = NULL,
    .__reserved3 = NULL,
    ._free = BR_CMETHOD_REF(br_device_pixelmap_vk, free),
    ._identifier = BR_CMETHOD_REF(br_device_pixelmap_vk, identifier),
    ._type = BR_CMETHOD_REF(br_device_pixelmap_vk, type),
    ._isType = BR_CMETHOD_REF(br_device_pixelmap_vk, isType),
    ._device = BR_CMETHOD_REF(br_device_pixelmap_vk, device),
    ._space = BR_CMETHOD_REF(br_device_pixelmap_vk, space),

    ._templateQuery = BR_CMETHOD_REF(br_device_pixelmap_vk, templateQuery),
    ._query = BR_CMETHOD_REF(br_object, query),
    ._queryBuffer = BR_CMETHOD_REF(br_object, queryBuffer),
    ._queryMany = BR_CMETHOD_REF(br_object, queryMany),
    ._queryManySize = BR_CMETHOD_REF(br_object, queryManySize),
    ._queryAll = BR_CMETHOD_REF(br_object, queryAll),
    ._queryAllSize = BR_CMETHOD_REF(br_object, queryAllSize),

    ._validSource = BR_CMETHOD_REF(br_device_pixelmap_mem, validSource),
    ._resize = BR_CMETHOD_REF(br_device_pixelmap_vk, resize),
    ._match = BR_CMETHOD_REF(br_device_pixelmap_vk, match),
    ._allocateSub = BR_CMETHOD_REF(br_device_pixelmap_vk, allocateSub),

    ._copy = BR_CMETHOD_REF(br_device_pixelmap_gen, copy),
    ._copyTo = BR_CMETHOD_REF(br_device_pixelmap_gen, copyTo),
    ._copyFrom = BR_CMETHOD_REF(br_device_pixelmap_gen, copyFrom),
    ._fill = BR_CMETHOD_REF(br_device_pixelmap_vk, fill),
    ._doubleBuffer = BR_CMETHOD_REF(br_device_pixelmap_fail, doubleBuffer),

    ._copyDirty = BR_CMETHOD_REF(br_device_pixelmap_gen, copyDirty),
    ._copyToDirty = BR_CMETHOD_REF(br_device_pixelmap_gen, copyToDirty),
    ._copyFromDirty = BR_CMETHOD_REF(br_device_pixelmap_gen, copyFromDirty),
    ._fillDirty = BR_CMETHOD_REF(br_device_pixelmap_gen, fillDirty),
    ._doubleBufferDirty = BR_CMETHOD_REF(br_device_pixelmap_gen, doubleBufferDirty),

    ._rectangle = BR_CMETHOD_REF(br_device_pixelmap_gen, rectangle),
    ._rectangle2 = BR_CMETHOD_REF(br_device_pixelmap_gen, rectangle2),
    ._rectangleCopy = BR_CMETHOD_REF(br_device_pixelmap_vk, rectangleCopy),
    ._rectangleCopyTo = BR_CMETHOD_REF(br_device_pixelmap_vk, rectangleCopyTo),
    ._rectangleCopyFrom = BR_CMETHOD_REF(br_device_pixelmap_fail, rectangleCopyFrom),
    ._rectangleStretchCopy = BR_CMETHOD_REF(br_device_pixelmap_vk, rectangleStretchCopy),
    ._rectangleStretchCopyTo = BR_CMETHOD_REF(br_device_pixelmap_fail, rectangleStretchCopyTo),
    ._rectangleStretchCopyFrom = BR_CMETHOD_REF(br_device_pixelmap_fail, rectangleStretchCopyFrom),
    ._rectangleFill = BR_CMETHOD_REF(br_device_pixelmap_vk, rectangleFill),
    ._pixelSet = BR_CMETHOD_REF(br_device_pixelmap_mem, pixelSet),
    ._line = BR_CMETHOD_REF(br_device_pixelmap_mem, line),
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

    ._flush = BR_CMETHOD_REF(br_device_pixelmap_vk, flush),
    ._synchronise = BR_CMETHOD_REF(br_device_pixelmap_fail, synchronise),
    ._directLock = BR_CMETHOD_REF(br_device_pixelmap_vk, directLock),
    ._directUnlock = BR_CMETHOD_REF(br_device_pixelmap_vk, directUnlock),
    ._getControls = BR_CMETHOD_REF(br_device_pixelmap_fail, getControls),
    ._setControls = BR_CMETHOD_REF(br_device_pixelmap_fail, setControls)
};
