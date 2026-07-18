#include <stddef.h>
#include <string.h>

#include "brassert.h"
#include "drv.h"

static struct br_buffer_stored_dispatch bufferStoredDispatch;

#define F(f) offsetof(struct br_buffer_stored, f)

static struct br_tv_template_entry bufferStoredTemplateEntries[] = {
    { BRT(IDENTIFIER_CSTR), F(identifier), BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY },
};
#undef F

br_error BufferStoredVKUpdate(struct br_buffer_stored* self, struct br_device_pixelmap* pm, br_token_value* tv);

struct br_buffer_stored* BufferStoredVKAllocate(br_renderer* renderer, br_token use, struct br_device_pixelmap* pm,
    br_token_value* tv) {
    struct br_buffer_stored* self;
    char* ident;

    switch (use) {

    case BRT_TEXTURE_O:
    case BRT_COLOUR_MAP_O:
        ident = "Colour-Map";
        break;

    default:
        return NULL;
    }

    self = BrResAllocate(renderer, sizeof(*self), BR_MEMORY_OBJECT);
    if (self == NULL)
        return NULL;

    self->dispatch = &bufferStoredDispatch;
    self->identifier = ident;
    self->device = ObjectDevice(renderer);
    self->renderer = renderer;
    self->image = VK_NULL_HANDLE;
    self->memory = VK_NULL_HANDLE;
    self->view = VK_NULL_HANDLE;
    self->sampler = VK_NULL_HANDLE;
    self->imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    self->width = 0;
    self->height = 0;
    self->pixel_type = 0;
    self->templates = BrTVTemplateAllocate(self, (br_tv_template_entry*)bufferStoredTemplateEntries,
        BR_ASIZE(bufferStoredTemplateEntries));

    self->source = (br_pixelmap*)pm;

    if (BufferStoredVKUpdate(self, pm, tv) != BRE_OK) {
        BrResFreeNoCallback(self);
        return NULL;
    }

    ObjectContainerAddFront(renderer, (br_object*)self);

    return self;
}

static void expandIndex8ToRGBA(const br_uint_8* src, int width, int height, int srcStride,
    uint32_t* dst, const br_uint_32* palette) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = src[y * srcStride + x];
            br_uint_32 entry = palette[idx];
            uint8_t* d = (uint8_t*)&dst[y * width + x];
            d[0] = BR_BLU(entry);
            d[1] = BR_GRN(entry);
            d[2] = BR_RED(entry);
            d[3] = 0xFF;
        }
    }
}

br_error BufferStoredVKUpdate(struct br_buffer_stored* self, struct br_device_pixelmap* pm, br_token_value* tv) {
    (void)tv;

    if (!pm)
        return BRE_FAIL;

    br_device_pixelmap* screen = (br_device_pixelmap*)self->renderer->pixelmap->screen;
    HVIDEO hVideo = &screen->asFront.video;
    VkDevice dev = hVideo->device;

    if ((pm->pm_flags & BR_PMF_NO_ACCESS) && pm->use_type == BRT_OFFSCREEN &&
        pm->asBack.vkImage != VK_NULL_HANDLE) {

        if (self->view) { vkDestroyImageView(dev, self->view, NULL); self->view = VK_NULL_HANDLE; }
        if (self->sampler) { vkDestroySampler(dev, self->sampler, NULL); self->sampler = VK_NULL_HANDLE; }
        self->image = VK_NULL_HANDLE;
        self->memory = VK_NULL_HANDLE;

        {
            VkImageMemoryBarrier2 b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
            b.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image = pm->asBack.vkImage;
            b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            b.subresourceRange.levelCount = 1;
            b.subresourceRange.layerCount = 1;
            b.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
            b.srcAccessMask = VK_ACCESS_2_NONE;
            b.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            b.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
            VkCommandBufferAllocateInfo cbAi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            cbAi.commandPool = hVideo->commandPool;
            cbAi.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cbAi.commandBufferCount = 1;
            VkCommandBuffer cb;
            if (vkAllocateCommandBuffers(dev, &cbAi, &cb) == VK_SUCCESS) {
                VkCommandBufferBeginInfo cbBegin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
                cbBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                vkBeginCommandBuffer(cb, &cbBegin);
                VkDependencyInfo depInfo = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
                depInfo.imageMemoryBarrierCount = 1;
                depInfo.pImageMemoryBarriers = &b;
                vkCmdPipelineBarrier2(cb, &depInfo);
                vkEndCommandBuffer(cb);
                VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
                si.commandBufferCount = 1;
                si.pCommandBuffers = &cb;
                vkQueueSubmit(hVideo->graphicsQueue, 1, &si, VK_NULL_HANDLE);
                vkQueueWaitIdle(hVideo->graphicsQueue);
                vkFreeCommandBuffers(dev, hVideo->commandPool, 1, &cb);
            }
        }

        VkImageViewCreateInfo ivi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ivi.image = pm->asBack.vkImage;
        ivi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivi.format = hVideo->swapchainImageFormat;
        ivi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ivi.subresourceRange.baseMipLevel = 0;
        ivi.subresourceRange.levelCount = 1;
        ivi.subresourceRange.baseArrayLayer = 0;
        ivi.subresourceRange.layerCount = 1;
        VkResult res = vkCreateImageView(dev, &ivi, NULL, &self->view);
        if (res != VK_SUCCESS) return BRE_FAIL;

        VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sci.magFilter = VK_FILTER_LINEAR;
        sci.minFilter = VK_FILTER_LINEAR;
        sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.maxAnisotropy = 1.0f;
        sci.minLod = 0.0f;
        sci.maxLod = 0.0f;
        res = vkCreateSampler(dev, &sci, NULL, &self->sampler);
        if (res != VK_SUCCESS) return BRE_FAIL;

        self->source = (br_pixelmap*)pm;
        self->width = pm->pm_width;
        self->height = pm->pm_height;
        self->pixel_type = pm->pm_type;
        self->imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        self->paletted_source_dirty = BR_FALSE;
        return BRE_OK;
    }

    if (!pm->pm_pixels)
        return BRE_FAIL;

    VkFormat vkFormat;
    VkImageTiling tiling;
    VkImageUsageFlags usage;
    VkMemoryPropertyFlags memProps;

    br_uint_8 effectiveType = pm->pm_type;
    if (effectiveType == BR_PMT_INDEX_8)
        effectiveType = BR_PMT_RGBA_8888;

    if (VK_BrPixelmapGetTypeDetails(effectiveType, &vkFormat, &tiling, &usage, &memProps) != BRE_OK)
        return BRE_FAIL;

    int w = pm->pm_width;
    int h = pm->pm_height;

    void* pixelData = NULL;
    VkDeviceSize imageSize;

    if (pm->pm_type == BR_PMT_INDEX_8) {
        br_device* dev = ObjectDevice(self);
        if (dev && dev->clut) {
            self->palette_pointer = dev->clut;
            self->palette_revision = dev->clut->revision;
        } else {
            self->palette_pointer = NULL;
            self->palette_revision = 0;
        }

        br_uint_32* palette = NULL;
        if (dev && dev->clut && dev->clut->entries) {
            palette = dev->clut->entries;
        } else if (pm->pm_map && pm->pm_map->pixels) {
            palette = (br_uint_32*)pm->pm_map->pixels;
        }
        if (!palette)
            return BRE_FAIL;

        pixelData = BrScratchAllocate((size_t)w * h * 4);
        expandIndex8ToRGBA((const br_uint_8*)pm->pm_pixels, w, h, pm->pm_row_bytes,
            (uint32_t*)pixelData, palette);
        imageSize = (VkDeviceSize)w * h * 4;
    } else {
        int bpp = 4;
        if (pm->pm_type == BR_PMT_RGB_565 || pm->pm_type == BR_PMT_RGB_555)
            bpp = 2;
        else if (pm->pm_type == BR_PMT_RGB_888)
            bpp = 3;
        imageSize = (VkDeviceSize)w * h * bpp;
        pixelData = BrScratchAllocate((size_t)imageSize);
        for (int y = 0; y < h; y++) {
            memcpy((char*)pixelData + y * w * bpp,
                (const char*)pm->pm_pixels + y * pm->pm_row_bytes,
                (size_t)w * bpp);
        }
    }

    if (self->image != VK_NULL_HANDLE) {
        if (self->view) vkDestroyImageView(dev, self->view, NULL);
        if (self->sampler) vkDestroySampler(dev, self->sampler, NULL);
        vkDestroyImage(dev, self->image, NULL);
        if (self->memory) vkFreeMemory(dev, self->memory, NULL);
        self->image = VK_NULL_HANDLE;
        self->view = VK_NULL_HANDLE;
        self->sampler = VK_NULL_HANDLE;
        self->memory = VK_NULL_HANDLE;
    }

    VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = vkFormat;
    ii.extent.width = w;
    ii.extent.height = h;
    ii.extent.depth = 1;
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult res = vkCreateImage(dev, &ii, NULL, &self->image);
    if (res != VK_SUCCESS) goto cleanup;

    VkMemoryRequirements imgMemReq;
    vkGetImageMemoryRequirements(dev, self->image, &imgMemReq);

    VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = imgMemReq.size;
    ai.memoryTypeIndex = VK_FindMemoryType(hVideo->physicalDevice, imgMemReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (ai.memoryTypeIndex == UINT32_MAX) goto cleanup;

    res = vkAllocateMemory(dev, &ai, NULL, &self->memory);
    if (res != VK_SUCCESS) goto cleanup;

    vkBindImageMemory(dev, self->image, self->memory, 0);

    {
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;
        if (VK_CreateStagingBuffer(hVideo, imageSize, &stagingBuffer, &stagingMemory) != VK_SUCCESS)
            goto cleanup;

        void* mapped;
        vkMapMemory(dev, stagingMemory, 0, imageSize, 0, &mapped);
        memcpy(mapped, pixelData, (size_t)imageSize);
        vkUnmapMemory(dev, stagingMemory);

        VkCommandBufferAllocateInfo cbAi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cbAi.commandPool = hVideo->commandPool;
        cbAi.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbAi.commandBufferCount = 1;
        VkCommandBuffer cb;
        if (vkAllocateCommandBuffers(dev, &cbAi, &cb) != VK_SUCCESS) {
            vkDestroyBuffer(dev, stagingBuffer, NULL);
            vkFreeMemory(dev, stagingMemory, NULL);
            goto cleanup;
        }

        VkCommandBufferBeginInfo cbBegin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        cbBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cb, &cbBegin);

        VkImageMemoryBarrier2 b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = self->image;
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.layerCount = 1;
        b.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
        b.srcAccessMask = VK_ACCESS_2_NONE;
        b.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
        b.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;

        VkDependencyInfo depInfo = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &b;
        vkCmdPipelineBarrier2(cb, &depInfo);

        VkBufferImageCopy region = {0};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent.width = w;
        region.imageExtent.height = h;
        region.imageExtent.depth = 1;
        vkCmdCopyBufferToImage(cb, stagingBuffer, self->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
        b.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        b.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        b.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        vkCmdPipelineBarrier2(cb, &depInfo);

        vkEndCommandBuffer(cb);

        VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cb;
        vkQueueSubmit(hVideo->graphicsQueue, 1, &si, VK_NULL_HANDLE);
        vkQueueWaitIdle(hVideo->graphicsQueue);
        vkFreeCommandBuffers(dev, hVideo->commandPool, 1, &cb);
        vkDestroyBuffer(dev, stagingBuffer, NULL);
        vkFreeMemory(dev, stagingMemory, NULL);
    }

    VkImageViewCreateInfo ivi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    ivi.image = self->image;
    ivi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivi.format = vkFormat;
    ivi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ivi.subresourceRange.baseMipLevel = 0;
    ivi.subresourceRange.levelCount = 1;
    ivi.subresourceRange.baseArrayLayer = 0;
    ivi.subresourceRange.layerCount = 1;
    res = vkCreateImageView(dev, &ivi, NULL, &self->view);
    if (res != VK_SUCCESS) goto cleanup;

    VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.maxAnisotropy = 1.0f;
    sci.minLod = 0.0f;
    sci.maxLod = 0.0f;
    res = vkCreateSampler(dev, &sci, NULL, &self->sampler);
    if (res != VK_SUCCESS) goto cleanup;

    self->width = w;
    self->height = h;
    self->pixel_type = pm->pm_type;
    self->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    self->paletted_source_dirty = BR_FALSE;
    if (pm->pm_type == BR_PMT_INDEX_8) {
        self->paletted_source_dirty = BR_TRUE;
    }

    if (pixelData) BrScratchFree(pixelData);

    return BRE_OK;

cleanup:
    if (self->image) { vkDestroyImage(dev, self->image, NULL); self->image = VK_NULL_HANDLE; }
    if (self->memory) { vkFreeMemory(dev, self->memory, NULL); self->memory = VK_NULL_HANDLE; }
    if (self->view) { vkDestroyImageView(dev, self->view, NULL); self->view = VK_NULL_HANDLE; }
    if (self->sampler) { vkDestroySampler(dev, self->sampler, NULL); self->sampler = VK_NULL_HANDLE; }
    if (pixelData) BrScratchFree(pixelData);
    return BRE_FAIL;
}

br_boolean BufferStoredVKReupload(struct br_buffer_stored* self) {
    if (!self || !self->source)
        return BR_FALSE;

    br_device_pixelmap* pm = (br_device_pixelmap*)self->source;
    if ((pm->pm_flags & BR_PMF_NO_ACCESS) && pm->use_type == BRT_OFFSCREEN)
        return BR_TRUE;
    if (pm->pm_type != BR_PMT_INDEX_8)
        return BR_FALSE;

    br_device* dev_obj = ObjectDevice(self);

    if (self->palette_pointer && dev_obj && dev_obj->clut &&
        self->palette_revision != dev_obj->clut->revision) {
        self->paletted_source_dirty = BR_TRUE;
    }

    if (self->paletted_source_dirty != BR_TRUE)
        return BR_TRUE;

    br_device_pixelmap* screen = (br_device_pixelmap*)self->renderer->pixelmap->screen;
    HVIDEO hVideo = &screen->asFront.video;
    VkDevice dev = hVideo->device;

    br_uint_32* palette = NULL;
    if (dev_obj && dev_obj->clut && dev_obj->clut->entries) {
        palette = dev_obj->clut->entries;
    } else if (pm->pm_map && pm->pm_map->pixels) {
        palette = (br_uint_32*)pm->pm_map->pixels;
    }
    if (!palette)
        return BR_FALSE;

    int w = self->width;
    int h = self->height;

    br_uint_32* rgba = BrScratchAllocate((size_t)w * h * 4);
    expandIndex8ToRGBA((const br_uint_8*)pm->pm_pixels, w, h, pm->pm_row_bytes, rgba, palette);

    {
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;
        VkDeviceSize uploadSize = (VkDeviceSize)w * h * 4;
        if (VK_CreateStagingBuffer(hVideo, uploadSize, &stagingBuffer, &stagingMemory) != VK_SUCCESS) {
            BrScratchFree(rgba);
            return BR_FALSE;
        }

        void* mapped;
        vkMapMemory(dev, stagingMemory, 0, uploadSize, 0, &mapped);
        memcpy(mapped, rgba, (size_t)uploadSize);
        vkUnmapMemory(dev, stagingMemory);

        VkCommandBufferAllocateInfo cbAi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cbAi.commandPool = hVideo->commandPool;
        cbAi.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbAi.commandBufferCount = 1;
        VkCommandBuffer cb;
        if (vkAllocateCommandBuffers(dev, &cbAi, &cb) != VK_SUCCESS) {
            vkDestroyBuffer(dev, stagingBuffer, NULL);
            vkFreeMemory(dev, stagingMemory, NULL);
            BrScratchFree(rgba);
            return BR_FALSE;
        }

        VkCommandBufferBeginInfo cbBegin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        cbBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cb, &cbBegin);

        VkImageMemoryBarrier2 b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        b.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = self->image;
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.layerCount = 1;
        b.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        b.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        b.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
        b.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;

        VkDependencyInfo depInfo = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &b;
        vkCmdPipelineBarrier2(cb, &depInfo);

        VkBufferImageCopy region = {0};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent.width = w;
        region.imageExtent.height = h;
        region.imageExtent.depth = 1;
        vkCmdCopyBufferToImage(cb, stagingBuffer, self->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
        b.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        b.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        b.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        vkCmdPipelineBarrier2(cb, &depInfo);

        vkEndCommandBuffer(cb);

        VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cb;
        vkQueueSubmit(hVideo->graphicsQueue, 1, &si, VK_NULL_HANDLE);
        vkQueueWaitIdle(hVideo->graphicsQueue);
        vkFreeCommandBuffers(dev, hVideo->commandPool, 1, &cb);
        vkDestroyBuffer(dev, stagingBuffer, NULL);
        vkFreeMemory(dev, stagingMemory, NULL);
    }

    BrScratchFree(rgba);

    self->paletted_source_dirty = BR_FALSE;
    if (dev_obj && dev_obj->clut) {
        self->palette_revision = dev_obj->clut->revision;
    }

    return BR_TRUE;
}

static br_error BR_CMETHOD_DECL(br_buffer_stored_vk, update)(struct br_buffer_stored* self,
    struct br_device_pixelmap* pm, br_token_value* tv) {
    return BufferStoredVKUpdate(self, pm, tv);
}

static void BR_CMETHOD_DECL(br_buffer_stored_vk, free)(br_object* _self) {
    br_buffer_stored* self = (br_buffer_stored*)_self;

    ObjectContainerRemove(self->renderer, (br_object*)self);

    VkDevice dev = ((br_device_pixelmap*)self->renderer->pixelmap->screen)->asFront.video.device;
    HVIDEO hVideo = &((br_device_pixelmap*)self->renderer->pixelmap->screen)->asFront.video;
    if (dev != VK_NULL_HANDLE) {
        VK_DeferFreeImage(hVideo, self->image, self->view, self->sampler, self->memory);
    }

    BrResFreeNoCallback(self);
}

static const char* BR_CMETHOD_DECL(br_buffer_stored_vk, identifier)(br_object* self) {
    return ((br_buffer_stored*)self)->identifier;
}

static br_token BR_CMETHOD_DECL(br_buffer_stored_vk, type)(br_object* self) {
    (void)self;
    return BRT_BUFFER_STORED;
}

static br_boolean BR_CMETHOD_DECL(br_buffer_stored_vk, isType)(br_object* self, br_token t) {
    (void)self;
    return (t == BRT_BUFFER_STORED) || (t == BRT_OBJECT);
}

static br_device* BR_CMETHOD_DECL(br_buffer_stored_vk, device)(br_object* self) {
    return ((br_buffer_stored*)self)->device;
}

static br_size_t BR_CMETHOD_DECL(br_buffer_stored_vk, space)(br_object* self) {
    return BrResSizeTotal(self);
}

static struct br_tv_template* BR_CMETHOD_DECL(br_buffer_stored_vk, templateQuery)(br_object* _self) {
    return ((br_buffer_stored*)_self)->templates;
}

static struct br_buffer_stored_dispatch bufferStoredDispatch = {
    .__reserved0 = NULL,
    .__reserved1 = NULL,
    .__reserved2 = NULL,
    .__reserved3 = NULL,
    ._free = BR_CMETHOD_REF(br_buffer_stored_vk, free),
    ._identifier = BR_CMETHOD_REF(br_buffer_stored_vk, identifier),
    ._type = BR_CMETHOD_REF(br_buffer_stored_vk, type),
    ._isType = BR_CMETHOD_REF(br_buffer_stored_vk, isType),
    ._device = BR_CMETHOD_REF(br_buffer_stored_vk, device),
    ._space = BR_CMETHOD_REF(br_buffer_stored_vk, space),

    ._templateQuery = BR_CMETHOD_REF(br_buffer_stored_vk, templateQuery),
    ._query = BR_CMETHOD_REF(br_object, query),
    ._queryBuffer = BR_CMETHOD_REF(br_object, queryBuffer),
    ._queryMany = BR_CMETHOD_REF(br_object, queryMany),
    ._queryManySize = BR_CMETHOD_REF(br_object, queryManySize),
    ._queryAll = BR_CMETHOD_REF(br_object, queryAll),
    ._queryAllSize = BR_CMETHOD_REF(br_object, queryAllSize),

    ._update = BR_CMETHOD_REF(br_buffer_stored_vk, update),
};
