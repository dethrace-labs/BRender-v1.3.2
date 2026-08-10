/*
 * Stored buffer methods (SDL3 GPU)
 *
 * Textures are always uploaded as RGBA8888: the SDL3 GPU backend only needs to
 * feed the fragment sampler, and the shared GLSL samples R8G8B8A8_UNORM, so
 * every source format (INDEX_8, RGB_565/555/888, RGBA/RGBX_8888) is converted
 * to RGBA8888 on the CPU before SDL3REND_UploadBufferToImage.
 *
 * Offscreen device pixelmaps have no GPU image of their own — their pixels live
 * in the shared CPU locked buffer (hVideo->lockedPixels, see devpixmp.c). The
 * texture is created lazily from that region on first use (BufferStoredSDL3REND
 * Reupload), which is what supports the pratcam quad sampling an offscreen
 * render target.
 */
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

static void expandIndex8ToRGBA(const br_uint_8* src, int width, int height, int srcStride,
    uint32_t* dst, const br_uint_32* palette) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = src[y * srcStride + x];
            br_uint_32 entry = palette[idx];
            uint8_t* d = (uint8_t*)&dst[y * width + x];
            d[0] = (uint8_t)BR_RED(entry);
            d[1] = (uint8_t)BR_GRN(entry);
            d[2] = (uint8_t)BR_BLU(entry);
            d[3] = 0xFF;
        }
    }
}

/* Expands the region of the shared CPU locked buffer that backs an offscreen
 * device pixelmap into RGBA8888. The write path (devpixmp.c rectangleCopyFrom
 * and the renderer flush) lays the pixels out at (pm_base_x, pm_base_y) using
 * the offscreen's own pm_row_bytes as the row stride, so the reads mirror that
 * exactly. */
static void lockedRegionToRGBA(br_device_pixelmap* pm, const void* lockedPixels, uint32_t* dst) {
    int w = pm->pm_width;
    int h = pm->pm_height;
    int bpp;
    switch (pm->pm_type) {
    case BR_PMT_RGB_565:
    case BR_PMT_RGB_555:
        bpp = 2;
        break;
    default:
        bpp = 4;
        break;
    }

    const char* base = (const char*)lockedPixels + pm->pm_base_y * pm->pm_row_bytes + pm->pm_base_x * bpp;

    for (int y = 0; y < h; y++) {
        const char* row = base + y * pm->pm_row_bytes;
        if (bpp == 2) {
            const br_uint_16* s = (const br_uint_16*)row;
            for (int x = 0; x < w; x++) {
                br_uint_16 p = s[x];
                uint8_t* d = (uint8_t*)&dst[y * w + x];
                if (pm->pm_type == BR_PMT_RGB_565) {
                    d[0] = (uint8_t)(((p >> 11) & 0x1F) << 3);
                    d[1] = (uint8_t)(((p >> 5) & 0x3F) << 2);
                    d[2] = (uint8_t)((p & 0x1F) << 3);
                } else {
                    d[0] = (uint8_t)(((p >> 10) & 0x1F) << 3);
                    d[1] = (uint8_t)(((p >> 5) & 0x1F) << 3);
                    d[2] = (uint8_t)((p & 0x1F) << 3);
                }
                d[3] = 0xFF;
            }
        } else {
            memcpy(dst + y * w, row, (size_t)w * 4);
        }
    }
}

static HVIDEO BufferStoredVideo(struct br_buffer_stored* self) {
    br_device_pixelmap* screen = (br_device_pixelmap*)self->renderer->pixelmap->screen;
    return &screen->asFront.video;
}

/* (Re)creates the GPU texture + sampler for `rgba` and uploads it. */
static br_error uploadRGBA(struct br_buffer_stored* self, HVIDEO hVideo, const void* rgba, int w, int h) {
    if (self->image) {
        SDL3REND_DeferFreeImage(hVideo, self->image, self->sampler);
        self->image = NULL;
        self->sampler = NULL;
    }

    SDL_GPUTextureCreateInfo ti = {0};
    ti.type = SDL_GPU_TEXTURETYPE_2D;
    ti.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ti.width = w;
    ti.height = h;
    ti.layer_count_or_depth = 1;
    ti.num_levels = 1;
    ti.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

    self->image = SDL_CreateGPUTexture(hVideo->device, &ti);
    if (!self->image)
        return BRE_FAIL;

    if (SDL3REND_UploadBufferToImage(hVideo, self->image, w, h, 0, 0, rgba, (size_t)w * h * 4) != 0)
        return BRE_FAIL;

    SDL_GPUSamplerCreateInfo si = {0};
    si.min_filter = SDL_GPU_FILTER_LINEAR;
    si.mag_filter = SDL_GPU_FILTER_LINEAR;
    si.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;

    self->sampler = SDL_CreateGPUSampler(hVideo->device, &si);
    if (!self->sampler)
        return BRE_FAIL;

    self->width = w;
    self->height = h;
    return BRE_OK;
}

static br_error updateMemory(struct br_buffer_stored* self, br_pixelmap* pm) {
    HVIDEO hVideo = BufferStoredVideo(self);
    int w = pm->width;
    int h = pm->height;
    size_t rgbaSize = (size_t)w * h * 4;

    if ((pm->flags & BR_PMF_NO_ACCESS) || pm->pixels == NULL)
        return BRE_FAIL;

    br_uint_32* rgba = BrScratchAllocate(rgbaSize);

    if (pm->type == BR_PMT_INDEX_8) {
        br_device* dev_obj = ObjectDevice(self);
        if (dev_obj && dev_obj->clut) {
            self->palette_pointer = dev_obj->clut;
            self->palette_revision = dev_obj->clut->revision;
        } else {
            self->palette_pointer = NULL;
            self->palette_revision = 0;
        }

        br_uint_32* palette = NULL;
        if (dev_obj && dev_obj->clut && dev_obj->clut->entries)
            palette = dev_obj->clut->entries;
        else if (pm->map && pm->map->pixels)
            palette = (br_uint_32*)pm->map->pixels;

        if (!palette) {
            BrScratchFree(rgba);
            return BRE_FAIL;
        }

        expandIndex8ToRGBA((const br_uint_8*)pm->pixels, w, h, pm->row_bytes, rgba, palette);
    } else if (pm->type == BR_PMT_RGB_565 || pm->type == BR_PMT_RGB_555) {
        const br_uint_16* src = (const br_uint_16*)pm->pixels;
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                br_uint_16 p = src[y * (pm->row_bytes / 2) + x];
                uint8_t* d = (uint8_t*)&rgba[y * w + x];
                if (pm->type == BR_PMT_RGB_565) {
                    d[0] = (uint8_t)(((p >> 11) & 0x1F) << 3);
                    d[1] = (uint8_t)(((p >> 5) & 0x3F) << 2);
                    d[2] = (uint8_t)((p & 0x1F) << 3);
                } else {
                    d[0] = (uint8_t)(((p >> 10) & 0x1F) << 3);
                    d[1] = (uint8_t)(((p >> 5) & 0x1F) << 3);
                    d[2] = (uint8_t)((p & 0x1F) << 3);
                }
                d[3] = 0xFF;
            }
        }
    } else {
        int sbpp = (pm->type == BR_PMT_RGBA_8888 || pm->type == BR_PMT_RGBX_888) ? 4 : 3;
        for (int y = 0; y < h; y++) {
            const char* s = (const char*)pm->pixels + y * pm->row_bytes;
            uint8_t* d = (uint8_t*)rgba + (size_t)y * w * 4;
            if (sbpp == 3) {
                for (int x = 0; x < w; x++) {
                    d[x * 4 + 0] = s[x * 3 + 0];
                    d[x * 4 + 1] = s[x * 3 + 1];
                    d[x * 4 + 2] = s[x * 3 + 2];
                    d[x * 4 + 3] = 0xFF;
                }
            } else {
                memcpy(d, s, (size_t)w * 4);
            }
        }
    }

    br_error r = uploadRGBA(self, hVideo, rgba, w, h);

    BrScratchFree(rgba);

    if (r != BRE_OK)
        return r;

    self->source = pm;
    self->source_flags = pm->flags;
    self->pixel_type = pm->type;
    self->paletted_source_dirty = BR_FALSE;
    if (pm->type == BR_PMT_INDEX_8)
        self->paletted_source_dirty = BR_TRUE;

    return BRE_OK;
}

br_error BufferStoredSDL3RENDUpdate(struct br_buffer_stored* self, struct br_device_pixelmap* pm, br_token_value* tv) {
    br_device* pm_device;
    (void)tv;

    if (!pm)
        return BRE_FAIL;

    if (pm->pm_type != BR_PMT_INDEX_8 && pm->pm_type != BR_PMT_RGB_565 && pm->pm_type != BR_PMT_RGB_555 &&
        pm->pm_type != BR_PMT_RGB_888 && pm->pm_type != BR_PMT_RGBA_8888 && pm->pm_type != BR_PMT_RGBX_888)
        return BRE_FAIL;

    /*
     * Find out where the pixelmap comes from
     */
    pm_device = ObjectDevice(pm);
    if (pm_device == NULL) {
        return updateMemory(self, (br_pixelmap*)pm);
    } else if (pm_device == self->device) {
        /*
         * Device pixelmap (offscreen render target): keep the source. The GPU
         * texture is created lazily from hVideo->lockedPixels on first use.
         */
        ASSERT(self->source == NULL || self->source == (br_pixelmap*)pm);
        self->source = (br_pixelmap*)pm;
        self->source_flags = pm->pm_flags;
        self->width = pm->pm_width;
        self->height = pm->pm_height;
        self->pixel_type = pm->pm_type;
        return BRE_OK;
    } else {
        /*
         * The pixelmap is from another device, we can't use it
         */
        return BRE_FAIL;
    }
}

br_boolean BufferStoredSDL3RENDReupload(struct br_buffer_stored* self) {
    if (!self || !self->source)
        return BR_FALSE;

    br_pixelmap* pm = self->source;
    br_device* dev_obj = ObjectDevice(self);

    /* Device pixelmap source: pixels live in the shared CPU locked buffer. */
    if (ObjectDevice(pm) == self->device) {
        br_device_pixelmap* dpm = (br_device_pixelmap*)pm;
        if (dpm->use_type == BRT_OFFSCREEN) {
            HVIDEO hVideo = &dpm->screen->asFront.video;
            if (!hVideo->lockedPixels)
                return BR_TRUE;
            if (self->width <= 0 || self->height <= 0)
                return BR_TRUE;

            br_uint_32* rgba = BrScratchAllocate((size_t)self->width * self->height * 4);
            lockedRegionToRGBA(dpm, hVideo->lockedPixels, rgba);
            uploadRGBA(self, hVideo, rgba, self->width, self->height);
            BrScratchFree(rgba);

            self->paletted_source_dirty = BR_FALSE;
            return BR_TRUE;
        }
        return BR_TRUE;
    }

    if (pm->type != BR_PMT_INDEX_8)
        return BR_FALSE;

    if (dev_obj && dev_obj->clut && self->palette_pointer && self->palette_revision != dev_obj->clut->revision)
        self->paletted_source_dirty = BR_TRUE;

    if (self->paletted_source_dirty != BR_TRUE)
        return BR_TRUE;

    HVIDEO hVideo = BufferStoredVideo(self);

    br_uint_32* palette = NULL;
    if (dev_obj && dev_obj->clut && dev_obj->clut->entries)
        palette = dev_obj->clut->entries;
    else if (pm->map && pm->map->pixels)
        palette = (br_uint_32*)pm->map->pixels;
    if (!palette)
        return BR_FALSE;

    int w = self->width;
    int h = self->height;

    br_uint_32* rgba = BrScratchAllocate((size_t)w * h * 4);
    expandIndex8ToRGBA((const br_uint_8*)pm->pixels, w, h, pm->row_bytes, rgba, palette);

    br_error r = uploadRGBA(self, hVideo, rgba, w, h);

    BrScratchFree(rgba);

    if (r != BRE_OK)
        return BR_FALSE;

    self->paletted_source_dirty = BR_FALSE;
    if (dev_obj && dev_obj->clut)
        self->palette_revision = dev_obj->clut->revision;

    return BR_TRUE;
}

static br_error BR_CMETHOD_DECL(br_buffer_stored_sdl3rend, update)(struct br_buffer_stored* self,
    struct br_device_pixelmap* pm, br_token_value* tv) {
    return BufferStoredSDL3RENDUpdate(self, pm, tv);
}

static void BR_CMETHOD_DECL(br_buffer_stored_sdl3rend, free)(br_object* _self) {
    br_buffer_stored* self = (br_buffer_stored*)_self;

    ObjectContainerRemove(self->renderer, (br_object*)self);

    if (self->image) {
        SDL3REND_DeferFreeImage(BufferStoredVideo(self), self->image, self->sampler);
        self->image = NULL;
        self->sampler = NULL;
    }

    BrResFreeNoCallback(self);
}

static const char* BR_CMETHOD_DECL(br_buffer_stored_sdl3rend, identifier)(br_object* self) {
    return ((br_buffer_stored*)self)->identifier;
}

static br_token BR_CMETHOD_DECL(br_buffer_stored_sdl3rend, type)(br_object* self) {
    (void)self;
    return BRT_BUFFER_STORED;
}

static br_boolean BR_CMETHOD_DECL(br_buffer_stored_sdl3rend, isType)(br_object* self, br_token t) {
    (void)self;
    return (t == BRT_BUFFER_STORED) || (t == BRT_OBJECT);
}

static br_device* BR_CMETHOD_DECL(br_buffer_stored_sdl3rend, device)(br_object* self) {
    return ((br_buffer_stored*)self)->device;
}

static br_size_t BR_CMETHOD_DECL(br_buffer_stored_sdl3rend, space)(br_object* self) {
    return BrResSizeTotal(self);
}

static struct br_tv_template* BR_CMETHOD_DECL(br_buffer_stored_sdl3rend, templateQuery)(br_object* _self) {
    return ((br_buffer_stored*)_self)->templates;
}

struct br_buffer_stored* BufferStoredSDL3RENDAllocate(br_renderer* renderer, br_token use, struct br_device_pixelmap* pm,
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
    self->image = NULL;
    self->sampler = NULL;
    self->width = 0;
    self->height = 0;
    self->pixel_type = 0;
    self->templates = BrTVTemplateAllocate(self, (br_tv_template_entry*)bufferStoredTemplateEntries,
        BR_ASIZE(bufferStoredTemplateEntries));

    if (BufferStoredSDL3RENDUpdate(self, pm, tv) != BRE_OK) {
        BrResFreeNoCallback(self);
        return NULL;
    }

    ObjectContainerAddFront(renderer, (br_object*)self);

    return self;
}

/*
 * Default dispatch table for device
 */
static struct br_buffer_stored_dispatch bufferStoredDispatch = {
    .__reserved0 = NULL,
    .__reserved1 = NULL,
    .__reserved2 = NULL,
    .__reserved3 = NULL,
    ._free = BR_CMETHOD_REF(br_buffer_stored_sdl3rend, free),
    ._identifier = BR_CMETHOD_REF(br_buffer_stored_sdl3rend, identifier),
    ._type = BR_CMETHOD_REF(br_buffer_stored_sdl3rend, type),
    ._isType = BR_CMETHOD_REF(br_buffer_stored_sdl3rend, isType),
    ._device = BR_CMETHOD_REF(br_buffer_stored_sdl3rend, device),
    ._space = BR_CMETHOD_REF(br_buffer_stored_sdl3rend, space),

    ._templateQuery = BR_CMETHOD_REF(br_buffer_stored_sdl3rend, templateQuery),
    ._query = BR_CMETHOD_REF(br_object, query),
    ._queryBuffer = BR_CMETHOD_REF(br_object, queryBuffer),
    ._queryMany = BR_CMETHOD_REF(br_object, queryMany),
    ._queryManySize = BR_CMETHOD_REF(br_object, queryManySize),
    ._queryAll = BR_CMETHOD_REF(br_object, queryAll),
    ._queryAllSize = BR_CMETHOD_REF(br_object, queryAllSize),

    ._update = BR_CMETHOD_REF(br_buffer_stored_sdl3rend, update),
};
