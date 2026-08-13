/*
 * Device pixelmap implementation, offscreen/back edition — SDL3 GPU.
 *
 * The SDL3-GPU-specific halves of the (former) shared commonrend/devpixmp.c:
 * the SDL3GPU_CALLBACKS_P custom token, the offscreen/depth template, match,
 * the depth-clear path in rectangleFill, the locked-buffer rectangleCopyTo,
 * fill, flush (CPU locked buffer -> overlay texture, purging transparent
 * magenta regions), directLock and the offscreen dispatch table. The
 * driver-agnostic object methods live in commonrend/devpixmp.c.
 */

#include "drv.h"
#include <brassert.h>
#include <string.h>

/*
 * Default dispatch table for device (defined at end of file)
 */
static const struct br_device_pixelmap_dispatch devicePixelmapDispatch;

static br_error custom_query(br_value* pvalue, void** extra, br_size_t* pextra_size, void* block, struct br_tv_template_entry* tep) {
    const br_device_pixelmap* self = block;

    if (tep->token == BRT_SDL3GPU_CALLBACKS_P) {
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

/*
 * Device pixelmap info. template
 */
#define F(f) offsetof(struct br_device_pixelmap, f)
static struct br_tv_template_entry devicePixelmapTemplateEntries[] = {
    { BRT(WIDTH_I32), F(pm_width), BRTV_QUERY | BRTV_ALL, BRTV_CONV_I32_U16, 0 },
    { BRT(HEIGHT_I32), F(pm_height), BRTV_QUERY | BRTV_ALL, BRTV_CONV_I32_U16, 0 },
    { BRT(PIXEL_TYPE_U8), F(pm_type), BRTV_QUERY | BRTV_ALL, BRTV_CONV_I32_U8, 0 },
    { BRT(OUTPUT_FACILITY_O), F(output_facility), BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY, 0 },
    { BRT(FACILITY_O), F(output_facility), BRTV_QUERY, BRTV_CONV_COPY, 0 },
    { BRT(IDENTIFIER_CSTR), F(pm_identifier), BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY, 0 },
    { BRT(MSAA_SAMPLES_I32), F(msaa_samples), BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY, 0 },
    { BRT(SDL3GPU_CALLBACKS_P), 0, BRTV_QUERY | BRTV_ALL, BRTV_CONV_CUSTOM, (br_uintptr_t)&custom },
};
#undef F

/*
 * Backend resource teardown, called from the shared free().
 */
void BREND_FN(DevicePixelmap, DeleteResources)(br_device_pixelmap* self) {
    HVIDEO hVideo = &self->screen->asFront.video;
    if (hVideo->device == NULL)
        return;

    if (self->use_type == BRT_DEPTH) {
        /* Depth pixelmaps share the single frame depthTexture; nothing to free. */
    } else if (self->use_type == BRT_OFFSCREEN) {
        /* Offscreen pixelmaps are CPU locked buffers only (no per-pixelmap GPU
         * texture); their content reaches the GPU through the shared overlay
         * texture at flush time. */
        if (hVideo->overlayTexture) { SDL3_ReleaseGPUTexture(hVideo->device, hVideo->overlayTexture); hVideo->overlayTexture = NULL; }
        if (hVideo->lockedPixels) { BrMemFree(hVideo->lockedPixels); hVideo->lockedPixels = NULL; }
    }
}

struct br_tv_template* BREND_CMETHOD_DECL(BREND_CLASS(br_device_pixelmap_), templateQuery)(br_object* _self) {
    br_device_pixelmap* self = (br_device_pixelmap*)_self;

    if (self->device->templates.devicePixelmapTemplate == NULL)
        self->device->templates.devicePixelmapTemplate = BrTVTemplateAllocate(self->device, devicePixelmapTemplateEntries,
            BR_ASIZE(devicePixelmapTemplateEntries));

    return self->device->templates.devicePixelmapTemplate;
}

/*
 * Structure used to unpack the 'match' tokens/values
 */
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
    { BRT_WIDTH_I32, NULL, F(width), BRTV_SET, BRTV_CONV_COPY },
    { BRT_HEIGHT_I32, NULL, F(height), BRTV_SET, BRTV_CONV_COPY },
    { BRT_PIXEL_BITS_I32, NULL, F(pixel_bits), BRTV_SET, BRTV_CONV_COPY },
    { BRT_PIXEL_TYPE_U8, NULL, F(type), BRTV_SET, BRTV_CONV_COPY },
    { BRT_USE_T, NULL, F(use_type), BRTV_SET, BRTV_CONV_COPY },
    { BRT_MSAA_SAMPLES_I32, NULL, F(msaa_samples), BRTV_SET, BRTV_CONV_COPY },
};
#undef F

br_error BREND_CMETHOD_DECL(BREND_CLASS(br_device_pixelmap_), match)(br_device_pixelmap* self, br_device_pixelmap** newpm, br_token_value* tv) {
    br_int_32 count;
    br_error err;
    br_device_pixelmap* pm;
    const char* typestring;
    struct pixelmapMatchTokens mt = {
        .width = self->pm_width,
        .height = self->pm_height,
        .pixel_bits = -1,
        .type = BR_PMT_MAX,
        .use_type = BRT_NONE,
        .msaa_samples = 0,
    };
    char tmp[80];

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

        /*
         * Depth buffers must be matched with the backbuffer.
         */
        if (self->use_type != BRT_OFFSCREEN)
            return BRE_UNSUPPORTED;

        /*
         * Can't have >1 depth buffer.
         */
        if (self->asBack.depthbuffer != NULL)
            return BRE_FAIL;

        /*
         * Not supporting non-16bpp depth buffers.
         */
        if (mt.pixel_bits != 16)
            return BRE_UNSUPPORTED;

        mt.type = BR_PMT_DEPTH_16;
        break;
    default:
        return BRE_UNSUPPORTED;
    }

    /*
     * Only allow backbuffers to be instantiated from the frontbuffer.
     */
    if (self->use_type == BRT_NONE && mt.use_type != BRT_OFFSCREEN)
        return BRE_UNSUPPORTED;

    if (mt.type == BR_PMT_MAX)
        mt.type = self->pm_type;

    /* Offscreen pixelmaps are CPU locked buffers in the SDL3 GPU driver: they
     * are 2D scene targets (sub-areas of the front screen) whose content is
     * composited through the shared overlay texture at flush. No GPU resource
     * is created here. */
    if (mt.msaa_samples < 0)
        mt.msaa_samples = 0;

    pm = BrResAllocate(self->device, sizeof(br_device_pixelmap), BR_MEMORY_OBJECT);
    memset(pm, 0, sizeof(br_device_pixelmap));
    pm->dispatch = &devicePixelmapDispatch;
    BrSprintfN(tmp, sizeof(tmp) - 1, "SDL3 GPU:%s:%dx%d", typestring, mt.width, mt.height);
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
        /* No GPU resource — the offscreen buffer's pixels live in the shared
         * CPU locked buffer (hVideo->lockedPixels) once directLocked. */
    } else {
        ASSERT(mt.use_type == BRT_DEPTH);
        self->asBack.depthbuffer = pm;
        pm->asDepth.backbuffer = self;
    }

    if (BREND_FN(DevicePixelmap, RecreateRenderBuffers)(pm) != BRE_OK) {
        --self->screen->asFront.num_refs;
        BREND_FN(DevicePixelmap, DeleteResources)(pm);
        BrResFreeNoCallback(pm);
        return BRE_FAIL;
    }

    /*
     * Copy origin over.
     */
    pm->pm_origin_x = self->pm_origin_x;
    pm->pm_origin_y = self->pm_origin_y;

    *newpm = pm;
    ObjectContainerAddFront(self->output_facility, (br_object*)pm);
    return BRE_OK;
}

br_error BREND_CMETHOD_DECL(BREND_CLASS(br_device_pixelmap_), rectangleFill)(br_device_pixelmap* self, br_rectangle* rect, br_uint_32 colour) {
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
        /* SDL3 GPU has no in-pass clear, and the whole frame shares one depth
         * attachment (cleared only at the frame's first render pass). The game
         * clears a depth buffer before every z-buffered scene (rear-view
         * mirror, wreck summary), so restart the pass with depth LOADOP_CLEAR
         * (color preserved via LOADOP_LOAD) to avoid those scenes testing
         * against the main view's stale depth. */
        if (self->screen != NULL) {
            HVIDEO hVideo = &self->screen->asFront.video;
            if (hVideo->renderPassActive && hVideo->commandBuffer != NULL)
                SDL3GPUREND_ClearDepthAttachment(hVideo);
        }
    } else {
        return BRE_UNSUPPORTED;
    }

    return BRE_OK;
}

br_error BREND_CMETHOD_DECL(BREND_CLASS(br_device_pixelmap_), fill)(br_device_pixelmap* self, br_uint_32 colour) {
    br_rectangle r;
    r.x = 0;
    r.y = 0;
    r.w = self->pm_width;
    r.h = self->pm_height;
    return DevicePixelmapRectangleFill(self, &r, colour);
}

br_error BREND_CMETHOD_DECL(BREND_CLASS(br_device_pixelmap_), rectangleCopyTo)(br_device_pixelmap* self, br_point* p,
    br_device_pixelmap* src, br_rectangle* sr) {
    p->x = -self->pm_origin_x;
    p->y = -self->pm_origin_y;
    sr->x = -src->pm_origin_x;
    sr->y = -src->pm_origin_y;

    if (src->pm_pixels == NULL)
        return BRE_FAIL;

    /* SDL3 GPU offscreen pixelmaps have no GPU texture of their own: the copy
     * goes into the shared CPU locked buffer, which reaches the screen through
     * the overlay composite at flush time. */
    if (self->use_type != BRT_OFFSCREEN)
        return BRE_UNSUPPORTED;

    HVIDEO hVideo = &self->screen->asFront.video;
    if (hVideo->lockedPixels == NULL)
        return BRE_FAIL;

    int bpp;
    switch (self->pm_type) {
    case BR_PMT_RGB_565:
    case BR_PMT_RGB_555:
        bpp = 2;
        break;
    case BR_PMT_RGBA_8888:
    case BR_PMT_RGBX_888:
    case BR_PMT_INDEX_8:
        bpp = 4;
        break;
    default:
        return BRE_UNSUPPORTED;
    }

    int dx = self->pm_base_x + p->x;
    int dy = self->pm_base_y + p->y;
    int srcStride = src->pm_row_bytes;
    int dstStride = self->pm_row_bytes;

    if (src->pm_type == BR_PMT_INDEX_8 && bpp == 4) {
        for (int y = 0; y < sr->h; y++) {
            for (int x = 0; x < sr->w; x++) {
                int sx = sr->x + x, sy = sr->y + y;
                if (sx < 0 || sy < 0 || sx >= src->pm_width || sy >= src->pm_height) continue;
                br_uint_8 idx = ((const br_uint_8*)src->pm_pixels)[sy * srcStride + sx];
                br_colour c = self->screen->clut->entries[idx];
                uint8_t* d = (uint8_t*)hVideo->lockedPixels + (dy + y) * dstStride + (dx + x) * bpp;
                d[0] = (uint8_t)BR_RED(c);
                d[1] = (uint8_t)BR_GRN(c);
                d[2] = (uint8_t)BR_BLU(c);
                d[3] = 0xFF;
            }
        }
    } else if (src->pm_type == self->pm_type) {
        for (int y = 0; y < sr->h; y++) {
            int sy = sr->y + y, yy = dy + y;
            if (sy < 0 || yy < 0 || sy >= src->pm_height || yy >= self->pm_height) continue;
            int sx = sr->x;
            if (sx < 0) sx = 0;
            int cw = sr->w;
            if (sx + cw > src->pm_width) cw = src->pm_width - sx;
            if (cw <= 0) continue;
            memcpy((char*)hVideo->lockedPixels + yy * dstStride + dx * bpp,
                (const char*)src->pm_pixels + sy * srcStride + sx * bpp,
                (size_t)cw * bpp);
        }
    } else {
        return BRE_UNSUPPORTED;
    }

    self->asBack.possiblyDirty = 1;
    return BRE_OK;
}

/* Single entry point for all CPU locked-buffer region processing at flush time:
 * the clearArea/pratcam purges to transparent magenta and the counter resets.
 * (The map-screen dim is applied at dim-draw time in modelrender.c, not here,
 * so the map text drawn after the dim scene stays bright.) Runs BEFORE the
 * upload so the overlay image is uploaded with the purged regions. */
static void SDL3GPUREND_PurgeLockedRegions(HVIDEO hVideo, br_device_pixelmap* self) {
    int bpp = (self->pm_type == BR_PMT_RGB_565 || self->pm_type == BR_PMT_RGB_555) ? 2 : 4;
    br_uint_32 magenta = (bpp == 2) ? BR_COLOUR_565(31, 0, 31) : BR_COLOUR_RGB(255, 0, 255);

    for (int i = 0; i < hVideo->clearAreaCount; i++) {
        SDL3GPUREND_PurgeRect(bpp, magenta, hVideo->lockedPixels,
            self->pm_width, self->pm_height, self->pm_row_bytes,
            hVideo->clearAreas[i].x, hVideo->clearAreas[i].y,
            hVideo->clearAreas[i].w, hVideo->clearAreas[i].h);
    }
    hVideo->clearAreaCount = 0;

    if (bpp == 2 && hVideo->pratcamAreaCount) {
        SDL3GPUREND_PurgeRect(bpp, magenta, hVideo->lockedPixels,
            self->pm_width, self->pm_height, self->pm_row_bytes,
            hVideo->pratcamArea.x, hVideo->pratcamArea.y,
            hVideo->pratcamArea.w, hVideo->pratcamArea.h);
        hVideo->pratcamAreaCount = 0;
    }

    /* The main scene rect is purged at sceneBegin (where it is freshly computed
     * for the current frame), not here. A flush-time purge using the stale
     * mainViewport from the previous frame would fire on 2D-only frames (e.g.
     * the ESC pause menu, which never runs a scene) and wipe the entire menu
     * overlay, yielding a black screen. */
}

br_error BREND_CMETHOD_DECL(BREND_CLASS(br_device_pixelmap_), flush)(br_device_pixelmap* self) {
    HVIDEO hVideo = &self->screen->asFront.video;

    if (self->sub_pixelmap) {
        return BRE_OK;
    }

    if (!self->asBack.possiblyDirty && !hVideo->overlayDirty) {
        return BRE_OK;
    }

    if (hVideo->lockedPixels != NULL) {
        /* Ensure the overlay texture exists (BGRA8888 — universally supported).
         * The overlay is uploaded from the CPU locked buffer each frame and
         * composited on top of the 3D content in SDL3GPUREND_Present. */
        if (hVideo->overlayTexture == NULL) {
            SDL_GPUTextureCreateInfo ti = {0};
            ti.type = SDL_GPU_TEXTURETYPE_2D;
            ti.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
            ti.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
            ti.width = self->pm_width;
            ti.height = self->pm_height;
            ti.layer_count_or_depth = 1;
            ti.num_levels = 1;
            ti.sample_count = SDL_GPU_SAMPLECOUNT_1;
            hVideo->overlayTexture = SDL3_CreateGPUTexture(hVideo->device, &ti);
            if (!hVideo->overlayTexture)
                return BRE_FAIL;
        }

        size_t srcOffset = self->pm_base_y * self->pm_row_bytes + self->pm_base_x * 2;

        SDL3GPUREND_PurgeLockedRegions(hVideo, self);

        if (self->pm_type == BR_PMT_RGB_565 || self->pm_type == BR_PMT_RGB_555) {
            /* 565/555 -> BGRA8888. The transparent magenta sentinel (0xF81F)
             * becomes fully transparent so the blended overlay composite hides
             * it; overlay.frag also discards rgb==(1,0,1). */
            int rShift = (self->pm_type == BR_PMT_RGB_565) ? 11 : 10;
            int gShift = (self->pm_type == BR_PMT_RGB_565) ? 5 : 5;
            int gMask = (self->pm_type == BR_PMT_RGB_565) ? 0x3F : 0x1F;
            int gDiv = (self->pm_type == BR_PMT_RGB_565) ? 63 : 31;
            br_uint_32* rgba = BrScratchAllocate((size_t)self->pm_width * self->pm_height * 4);
            if (rgba == NULL)
                return BRE_FAIL;
            const br_uint_16* src = (const br_uint_16*)((char*)hVideo->lockedPixels + srcOffset);
            for (int y = 0; y < self->pm_height; y++) {
                for (int x = 0; x < self->pm_width; x++) {
                    br_uint_16 p = src[y * (self->pm_row_bytes / 2) + x];
                    if (p == BR_COLOUR_565(31, 0, 31)) {
                        rgba[y * self->pm_width + x] = 0;  // transparent
                    } else {
                        int r5 = (p >> rShift) & 0x1F;
                        int g = (p >> gShift) & gMask;
                        int b5 = p & 0x1F;
                        rgba[y * self->pm_width + x] = (b5 * 255 / 31)
                            | ((g * 255 / gDiv) << 8)
                            | ((r5 * 255 / 31) << 16)
                            | (0xFF << 24);
                    }
                }
            }
            if (SDL3GPUREND_UploadBufferToImage(hVideo, hVideo->overlayTexture,
                    self->pm_width, self->pm_height, 0, 0,
                    rgba, (size_t)self->pm_width * self->pm_height * 4) != 0) {
                BrScratchFree(rgba);
                return BRE_FAIL;
            }
            BrScratchFree(rgba);
        } else {
            /* 4 bytes/pixel raw copy (RGBA_8888 / RGBX_888). */
            if (SDL3GPUREND_UploadBufferToImage(hVideo, hVideo->overlayTexture,
                    self->pm_width, self->pm_height, 0, 0,
                    (const char*)hVideo->lockedPixels + srcOffset,
                    (size_t)self->pm_width * self->pm_height * 4) != 0)
                return BRE_FAIL;
        }

        hVideo->overlayDirty = 1;
    }

    self->asBack.possiblyDirty = 0;

    return BRE_OK;
}

br_error BREND_CMETHOD_DECL(BREND_CLASS(br_device_pixelmap_), directLock)(br_device_pixelmap* self, br_boolean block) {
    HVIDEO hVideo = &self->screen->asFront.video;

    ASSERT(self->pm_pixels == NULL);
    ASSERT(self->use_type == BRT_OFFSCREEN);

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

/*
 * Default dispatch table for device pixelmap
 */
static const struct br_device_pixelmap_dispatch devicePixelmapDispatch = {
    .__reserved0 = NULL,
    .__reserved1 = NULL,
    .__reserved2 = NULL,
    .__reserved3 = NULL,
    ._free = BREND_CMETHOD_REF(BREND_CLASS(br_device_pixelmap_), free),
    ._identifier = BREND_CMETHOD_REF(BREND_CLASS(br_device_pixelmap_), identifier),
    ._type = BREND_CMETHOD_REF(BREND_CLASS(br_device_pixelmap_), type),
    ._isType = BREND_CMETHOD_REF(BREND_CLASS(br_device_pixelmap_), isType),
    ._device = BREND_CMETHOD_REF(BREND_CLASS(br_device_pixelmap_), device),
    ._space = BREND_CMETHOD_REF(BREND_CLASS(br_device_pixelmap_), space),

    ._templateQuery = BREND_CMETHOD_REF(BREND_CLASS(br_device_pixelmap_), templateQuery),
    ._query = BR_CMETHOD_REF(br_object, query),
    ._queryBuffer = BR_CMETHOD_REF(br_object, queryBuffer),
    ._queryMany = BR_CMETHOD_REF(br_object, queryMany),
    ._queryManySize = BR_CMETHOD_REF(br_object, queryManySize),
    ._queryAll = BR_CMETHOD_REF(br_object, queryAll),
    ._queryAllSize = BR_CMETHOD_REF(br_object, queryAllSize),

    ._validSource = BR_CMETHOD_REF(br_device_pixelmap_mem, validSource),
    ._resize = BREND_CMETHOD_REF(BREND_CLASS(br_device_pixelmap_), resize),
    ._match = BREND_CMETHOD_REF(BREND_CLASS(br_device_pixelmap_), match),
    ._allocateSub = BREND_CMETHOD_REF(BREND_CLASS(br_device_pixelmap_), allocateSub),

    ._copy = BR_CMETHOD_REF(br_device_pixelmap_gen, copy),
    ._copyTo = BR_CMETHOD_REF(br_device_pixelmap_gen, copyTo),
    ._copyFrom = BR_CMETHOD_REF(br_device_pixelmap_gen, copyFrom),
    ._fill = BREND_CMETHOD_REF(BREND_CLASS(br_device_pixelmap_), fill),
    ._doubleBuffer = BR_CMETHOD_REF(br_device_pixelmap_fail, doubleBuffer),

    ._copyDirty = BR_CMETHOD_REF(br_device_pixelmap_gen, copyDirty),
    ._copyToDirty = BR_CMETHOD_REF(br_device_pixelmap_gen, copyToDirty),
    ._copyFromDirty = BR_CMETHOD_REF(br_device_pixelmap_gen, copyFromDirty),
    ._fillDirty = BR_CMETHOD_REF(br_device_pixelmap_gen, fillDirty),
    ._doubleBufferDirty = BR_CMETHOD_REF(br_device_pixelmap_gen, doubleBufferDirty),

    ._rectangle = BR_CMETHOD_REF(br_device_pixelmap_gen, rectangle),
    ._rectangle2 = BR_CMETHOD_REF(br_device_pixelmap_gen, rectangle2),
    ._rectangleCopy = BREND_CMETHOD_REF(BREND_CLASS(br_device_pixelmap_), rectangleCopy),
    ._rectangleCopyTo = BREND_CMETHOD_REF(BREND_CLASS(br_device_pixelmap_), rectangleCopyTo),
    ._rectangleCopyFrom = BR_CMETHOD_REF(br_device_pixelmap_fail, rectangleCopyFrom),
    ._rectangleStretchCopy = BREND_CMETHOD_REF(BREND_CLASS(br_device_pixelmap_), rectangleStretchCopy),
    ._rectangleStretchCopyTo = BR_CMETHOD_REF(br_device_pixelmap_fail, rectangleStretchCopyTo),
    ._rectangleStretchCopyFrom = BR_CMETHOD_REF(br_device_pixelmap_fail, rectangleStretchCopyFrom),
    ._rectangleFill = BREND_CMETHOD_REF(BREND_CLASS(br_device_pixelmap_), rectangleFill),
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

    ._flush = BREND_CMETHOD_REF(BREND_CLASS(br_device_pixelmap_), flush),
    ._synchronise = BR_CMETHOD_REF(br_device_pixelmap_fail, synchronise),
    ._directLock = BREND_CMETHOD_REF(BREND_CLASS(br_device_pixelmap_), directLock),
    ._directUnlock = BREND_CMETHOD_REF(BREND_CLASS(br_device_pixelmap_), directUnlock),
    ._getControls = BR_CMETHOD_REF(br_device_pixelmap_fail, getControls),
    ._setControls = BR_CMETHOD_REF(br_device_pixelmap_fail, setControls)
};
