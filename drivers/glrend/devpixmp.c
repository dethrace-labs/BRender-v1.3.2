/*
 * Device pixelmap methods
 *
 * TODO:
 * - Cleanup dangling pointers when a back/depth buffer is destroyed.
 */

#include "brassert.h"
#include "drv.h"
#include <string.h>

/*
 * Default dispatch table for device (defined at end of file)
 */
static const struct br_device_pixelmap_dispatch devicePixelmapDispatch;

static br_error custom_query(br_value* pvalue, void** extra, br_size_t* pextra_size, void* block, struct br_tv_template_entry* tep) {
    const br_device_pixelmap* self = block;

    if (tep->token == BRT_OPENGL_TEXTURE_U32) {
        if (self->use_type == BRT_OFFSCREEN)
            pvalue->u32 = self->asBack.glTex;
        else if (self->use_type == BRT_DEPTH)
            pvalue->u32 = self->asDepth.glDepth;
        else
            pvalue->u32 = 0;

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
    { DEV(OPENGL_TEXTURE_U32), 0, BRTV_QUERY | BRTV_ALL, BRTV_CONV_CUSTOM, (br_uintptr_t)&custom },
};
#undef F

/*
 * (Re)create the renderbuffers and attach them to the framebuffer.
 */
static br_error recreate_renderbuffers(br_device_pixelmap* self) {
    GLenum binding_point = self->msaa_samples > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;

    UASSERT(self->use_type == BRT_OFFSCREEN || self->use_type == BRT_DEPTH);

    if (self->use_type == BRT_OFFSCREEN) {
        const GLenum draw_buffers[1] = { GL_COLOR_ATTACHMENT0 };

        UASSERT(self->asBack.glFbo != 0);

        /* Delete */
        glDeleteTextures(1, &self->asBack.glTex);

        /* Create */
        glGenTextures(1, &self->asBack.glTex);
        glBindTexture(binding_point, self->asBack.glTex);
        glTexParameteri(binding_point, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        if (self->msaa_samples) {
            glTexImage2DMultisample(binding_point, self->msaa_samples, GL_RGBA8, self->pm_width, self->pm_height, GL_TRUE);
        } else {
            glTexImage2D(binding_point, 0, GL_RGBA8, self->pm_width, self->pm_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        }

        glBindTexture(binding_point, 0);

        /* Attach */
        glBindFramebuffer(GL_FRAMEBUFFER, self->asBack.glFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, binding_point, self->asBack.glTex, 0);
        glDrawBuffers(1, draw_buffers);
    } else if (self->use_type == BRT_DEPTH) {
        UASSERT(self->asDepth.backbuffer->asBack.glFbo != 0);

        /* Delete */
        glGenTextures(1, &self->asDepth.glDepth);

        /* Create */
        glGenTextures(1, &self->asDepth.glDepth);
        glBindTexture(binding_point, self->asDepth.glDepth);

        if (self->msaa_samples) {
            glTexImage2DMultisample(binding_point, self->msaa_samples, GL_DEPTH_COMPONENT, self->pm_width,
                self->pm_height, GL_TRUE);
        } else {
            glTexImage2D(binding_point, 0, GL_DEPTH_COMPONENT, self->pm_width, self->pm_height, 0, GL_DEPTH_COMPONENT,
                GL_UNSIGNED_BYTE, NULL);
        }

        glBindTexture(binding_point, 0);

        /* Attach */
        glBindFramebuffer(GL_FRAMEBUFFER, self->asDepth.backbuffer->asBack.glFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, binding_point, self->asDepth.glDepth, 0);
    }

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return BRE_FAIL;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return BRE_OK;
}

static void delete_gl_resources(br_device_pixelmap* self) {
    if (self->use_type == BRT_DEPTH) {
        // FIXME: We should be destroyed before our parent.
        // FIXME: If we haven't, should I bind the parent and detach?
        glDeleteTextures(1, &self->asDepth.glDepth);
    } else if (self->use_type == BRT_OFFSCREEN) {
        glDeleteFramebuffers(1, &self->asBack.glFbo);
        glDeleteTextures(1, &self->asBack.glTex);

        /* Cleanup the quad */
        DeviceGLFiniQuad(&self->asBack.quad);
    }
}

void BR_CMETHOD_DECL(br_device_pixelmap_gl, free)(br_object* _self) {
    br_device_pixelmap* self = (br_device_pixelmap*)_self;

    if (self->sub_pixelmap) {
        return;
    }

    //BrLogPrintf("GLREND: Freeing %s", self->pm_identifier);

    delete_gl_resources(self);

    ObjectContainerRemove(self->output_facility, (br_object*)self);

    --self->screen->asFront.num_refs;

    BrResFreeNoCallback(self);
}

const char* BR_CMETHOD_DECL(br_device_pixelmap_gl, identifier)(br_object* self) {
    return ((br_device_pixelmap*)self)->pm_identifier;
}

br_token BR_CMETHOD_DECL(br_device_pixelmap_gl, type)(br_object* self) {
    (void)self;
    return BRT_DEVICE_PIXELMAP;
}

br_boolean BR_CMETHOD_DECL(br_device_pixelmap_gl, isType)(br_object* self, br_token t) {
    (void)self;
    return (t == BRT_DEVICE_PIXELMAP) || (t == BRT_OBJECT);
}

br_device* BR_CMETHOD_DECL(br_device_pixelmap_gl, device)(br_object* self) {
    (void)self;
    return ((br_device_pixelmap*)self)->device;
}

br_size_t BR_CMETHOD_DECL(br_device_pixelmap_gl, space)(br_object* self) {
    (void)self;
    return sizeof(br_device_pixelmap);
}

struct br_tv_template* BR_CMETHOD_DECL(br_device_pixelmap_gl, templateQuery)(br_object* _self) {
    br_device_pixelmap* self = (br_device_pixelmap*)_self;

    if (self->device->templates.devicePixelmapTemplate == NULL)
        self->device->templates.devicePixelmapTemplate = BrTVTemplateAllocate(self->device, devicePixelmapTemplateEntries,
            BR_ASIZE(devicePixelmapTemplateEntries));

    return self->device->templates.devicePixelmapTemplate;
}

br_error BR_CMETHOD_DECL(br_device_pixelmap_gl, resize)(br_device_pixelmap* self, br_int_32 width, br_int_32 height) {
    self->pm_width = width;
    self->pm_height = height;
    return recreate_renderbuffers(self);
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

br_error BR_CMETHOD_DECL(br_device_pixelmap_gl, match)(br_device_pixelmap* self, br_device_pixelmap** newpm, br_token_value* tv) {
    br_int_32 count;
    br_error err;
    br_device_pixelmap* pm;
    const char* typestring;
    GLint gl_internal_format;
    GLenum gl_format, gl_type;
    GLsizeiptr gl_elem_bytes;
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

    err = VIDEOI_BrPixelmapGetTypeDetails(mt.type, &gl_internal_format, &gl_format, &gl_type, &gl_elem_bytes, NULL);
    if (err != BRE_OK)
        return err;

    if (mt.msaa_samples < 0)
        mt.msaa_samples = 0;
    else if (mt.msaa_samples > hVideo->maxSamples)
        mt.msaa_samples = hVideo->maxSamples;

    pm = BrResAllocate(self->device, sizeof(br_device_pixelmap), BR_MEMORY_OBJECT);
    pm->dispatch = &devicePixelmapDispatch;
    BrSprintfN(tmp, sizeof(tmp) - 1, "OpenGL:%s:%dx%d", typestring, mt.width, mt.height);
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
    pm->pm_row_bytes = gl_elem_bytes * mt.width;
    pm->pm_flags = BR_PMF_NO_ACCESS;
    pm->pm_origin_x = 0;
    pm->pm_origin_y = 0;
    pm->pm_base_x = 0;
    pm->pm_base_y = 0;
    pm->sub_pixelmap = 0;

    if (mt.use_type == BRT_OFFSCREEN) {
        pm->asBack.depthbuffer = NULL;
        glGenFramebuffers(1, &pm->asBack.glFbo);
        DeviceGLInitQuad(&pm->asBack.quad, hVideo);
    } else {
        UASSERT(mt.use_type == BRT_DEPTH);
        self->asBack.depthbuffer = pm;
        pm->asDepth.backbuffer = self;
    }

    if (recreate_renderbuffers(pm) != BRE_OK) {
        --self->screen->asFront.num_refs;
        delete_gl_resources(pm);
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

br_error BR_CMETHOD_DECL(br_device_pixelmap_gl, rectangleStretchCopy)(br_device_pixelmap* self, br_rectangle* d,
    br_device_pixelmap* src, br_rectangle* s) {
    /*
     * Device->Device non-addressable stretch copy.
     */
    GLbitfield bits;
    GLuint srcFbo, dstFbo;

    /*
     * Refusing copy/blit to screen.
     */
    if (self->use_type == BRT_NONE)
        return BRE_FAIL;

    if (self->use_type == BRT_OFFSCREEN) {
        if (src->use_type != BRT_OFFSCREEN)
            return BRE_FAIL;
        dstFbo = self->asBack.glFbo;
        srcFbo = src->asBack.glFbo;
        bits = GL_COLOR_BUFFER_BIT;
    } else if (self->use_type == BRT_DEPTH) {
        if (src->use_type != BRT_DEPTH)
            return BRE_FAIL;

        dstFbo = self->asDepth.backbuffer->asBack.glFbo;
        srcFbo = src->asDepth.backbuffer->asBack.glFbo;
        bits = GL_DEPTH_BUFFER_BIT;
    } else {
        return BRE_FAIL;
    }

    /*
     * Ignore self-blit.
     */
    if (self == src)
        return BRE_OK;

    VIDEOI_BrRectToGL((br_pixelmap*)self, d);
    VIDEOI_BrRectToGL((br_pixelmap*)src, s);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFbo);

    glBlitFramebuffer(s->x, s->y, s->x + s->w, s->y + s->h, d->x, d->y, d->x + d->w, d->y + d->h, bits, GL_NEAREST);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    return BRE_OK;
}

br_error BR_CMETHOD_DECL(br_device_pixelmap_gl, rectangleCopy)(br_device_pixelmap* self, br_point* p,
    br_device_pixelmap* src, br_rectangle* sr) {

    return BRE_FAIL;
}

br_error BR_CMETHOD_DECL(br_device_pixelmap_gl, rectangleFill)(br_device_pixelmap* self, br_rectangle* rect, br_uint_32 colour) {
    GLuint fbo;
    GLbitfield mask;
    br_uint_8* px8;
    br_uint_16* px16;
    float a = (float)((colour & 0xFF000000) >> 24) / 255.0f;
    float r = (float)((colour & 0x00FF0000) >> 16) / 255.0f;
    float g = (float)((colour & 0x0000FF00) >> 8) / 255.0f;
    float b = (float)((colour & 0x000000FF) >> 0) / 255.0f;

    // TODO: handle the colour format correctly
    br_rectangle rr;
    // if(PixelmapRectangleClip(&rr, rect, (br_pixelmap *)self) == BR_CLIP_REJECT)
    // 	return BRE_OK;

    // rr.x = self->pm_base_x + rr.x;
    // rr.y = self->pm_base_y + rr.y;
    // rr.w = self->pm_base_x + rr.x + rr.w;
    // rr.y self->pm_base_y + rr.y + rr.h

    VIDEOI_BrRectToGL((br_pixelmap*)self, rect);

    if (self->use_type == BRT_DEPTH) {
        glBindFramebuffer(GL_FRAMEBUFFER, self->asBack.depthbuffer->asBack.glFbo);
        glClear(GL_DEPTH_BUFFER_BIT);
        return BRE_OK;
    }

    if (self->use_type == BRT_OFFSCREEN) {
        if (self->pm_pixels != NULL) {
            switch (self->pm_type) {
            case BR_PMT_INDEX_8:
                px8 = self->pm_pixels;
                for (int y = rect->y; y < rect->y + rect->h; y++) {
                    for (int x = rect->x; x < rect->x + rect->w; x++) {
                        px8[y * self->pm_width + x] = BR_ALPHA(colour);
                    }
                }
                break;

            case BR_PMT_RGB_565:
                px16 = self->pm_pixels;
                for (int y = rect->y; y < rect->y + rect->h; y++) {
                    for (int x = rect->x; x < rect->x + rect->w; x++) {
                        px16[y * self->pm_width + x] = colour & 0xffff;
                    }
                }
                break;

            default:
                return BRE_UNSUPPORTED;
            }
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, self->asBack.glFbo);
            glClearColor(colour & 0xff, colour & 0xff, colour & 0xff, 0xff);
            glClear(GL_COLOR_BUFFER_BIT);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

    } else {
        return BRE_UNSUPPORTED;
    }

    return BRE_OK;
}

br_error BR_CMETHOD(br_device_pixelmap_gl, rectangleStretchCopyTo)(br_device_pixelmap* self, br_rectangle* dr,
    br_device_pixelmap* _src, br_rectangle* sr) {
    return BRE_FAIL;
}

br_error BR_CMETHOD_DECL(br_device_pixelmap_gl, rectangleCopyTo)(br_device_pixelmap* self, br_point* p,
    br_device_pixelmap* src, br_rectangle* sr) {
    /* Pixelmap->Device, addressable same-size copy. */

    glBindTexture(GL_TEXTURE_2D, self->asBack.glTex);

    switch (src->pm_type) {

    case BR_PMT_RGB_565: {
        br_uint_16* buffer = BrScratchAllocate(sizeof(uint16_t) * sr->w * sr->h);
        br_uint_16* buffer_ptr = buffer;
        br_uint_16* src_px = src->pm_pixels;
        for (int y = sr->y; y < sr->y + sr->h; y++) {
            for (int x = sr->x; x < sr->x + sr->w; x++) {
                br_uint_16 c = src_px[y * src->pm_row_bytes / 2 + x];
                *buffer_ptr = c;
                buffer_ptr++;
            }
        }
        glTexSubImage2D(GL_TEXTURE_2D, 0, p->x, p->y, sr->w, sr->h, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, buffer);
        BrScratchFree(buffer);
        break;
    }
    case BR_PMT_INDEX_8: {
        uint32_t* buffer = BrScratchAllocate(sizeof(uint32_t) * sr->w * sr->h);
        uint32_t* buffer_ptr = buffer;
        char* src_px = src->pm_pixels;
        uint32_t* map;
        if (src->pm_map) {
            map = src->pm_map->pixels;
        } else {
            map = ObjectDevice(self)->clut->entries;
        }
        for (int y = sr->y; y < sr->y + sr->h; y++) {
            for (int x = sr->x; x < sr->x + sr->w; x++) {
                int index = src_px[y * src->pm_row_bytes + x];
                *buffer_ptr = map[index];
                buffer_ptr++;
            }
        }
        glTexSubImage2D(GL_TEXTURE_2D, 0, p->x, p->y, sr->w, sr->h, GL_RGB, GL_UNSIGNED_BYTE, buffer);
        BrScratchFree(buffer);
        break;
    }
    default:
        UASSERT(0);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    return BRE_OK;
}

/*
 * Device->Pixelmap, addressable same-size copy.
 */
br_error BR_CMETHOD_DECL(br_device_pixelmap_gl, rectangleCopyFrom)(br_device_pixelmap* self, br_point* p,
    br_device_pixelmap* dest, br_rectangle* r) {
    br_error err;
    GLint internalFormat;
    GLenum format, type;
    GLsizeiptr elemBytes;
    void* rowTemp;

    return BRE_FAIL;
}

br_error BR_CMETHOD(br_device_pixelmap_gl, text)(br_device_pixelmap* self, br_point* point, br_font* font,
    const char* text, br_uint_32 colour) {
    return BRE_FAIL;
}

br_error BR_CMETHOD_DECL(br_device_pixelmap_gl, allocateSub)(br_device_pixelmap* self, br_device_pixelmap** newpm, br_rectangle* rect) {
    br_device_pixelmap* pm;
    br_rectangle out;

    /*
     * Create the new structure and copy
     */
    pm = BrResAllocate(self->device, sizeof(*pm), BR_MEMORY_PIXELMAP);

    /*
     * Set all the fields to be the same as the parent pixelmap for now
     */
    *pm = *self;

    /*
     * Create sub-window (clipped against original)
     */
    if (PixelmapRectangleClip(&out, rect, (br_pixelmap*)self) == BR_CLIP_REJECT)
        return BRE_FAIL;

    pm->sub_pixelmap = BR_TRUE;
    pm->parent_height = self->pm_height;

    /*
     * Pixel rows are not contiguous
     */
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

br_error BR_CMETHOD_DECL(br_device_pixelmap_gl, directLock)(br_device_pixelmap* self, br_boolean block) {

    UASSERT(self->pm_pixels == NULL);
    UASSERT(self->use_type == BRT_OFFSCREEN);

    self->pm_pixels = BrMemAllocate(self->pm_height * self->pm_row_bytes, BR_MEMORY_PIXELS);
    br_uint_8* buffer = BrScratchAllocate(self->pm_row_bytes * self->pm_height);

    glBindTexture(GL_TEXTURE_2D, self->asBack.glTex);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, buffer);
    glBindTexture(GL_TEXTURE_2D, 0);

    // invert pixels to make (0,0) point to top-left
    int row = self->pm_height - 1;
    br_uint_8* src_ptr = buffer;                                                                     // top of buffer
    br_uint_8* dst_ptr = ((br_uint_8*)self->pm_pixels) + self->pm_row_bytes * (self->pm_height - 1); // bottom of buffer
    for (int y = 0; y < self->pm_height; y++) {
        BrMemCpy(dst_ptr, src_ptr, self->pm_row_bytes);
        src_ptr += self->pm_row_bytes;
        dst_ptr -= self->pm_row_bytes;
    }
    BrScratchFree(buffer);

    return BRE_OK;
}

br_error BR_CMETHOD_DECL(br_device_pixelmap_gl, directUnlock)(br_device_pixelmap* self) {
    int e;
    UASSERT(self->pm_pixels != NULL);
    UASSERT(self->use_type == BRT_OFFSCREEN);

    br_uint_8* buffer = BrScratchAllocate(self->pm_row_bytes * self->pm_height);
    // invert pixels to make (0,0) point to top-left
    int row = self->pm_height - 1;
    br_uint_8* src_ptr = self->pm_pixels;                                     // top of buffer
    br_uint_8* dst_ptr = buffer + self->pm_row_bytes * (self->pm_height - 1); // bottom of buffer
    for (int y = 0; y < self->pm_height; y++) {
        BrMemCpy(dst_ptr, src_ptr, self->pm_row_bytes);
        src_ptr += self->pm_row_bytes;
        dst_ptr -= self->pm_row_bytes;
    }

    glBindTexture(GL_TEXTURE_2D, self->asBack.glTex);
    e = glGetError();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, self->pm_width, self->pm_height, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, buffer);
    e = glGetError();
    BrScratchFree(buffer);
    BrMemFree(self->pm_pixels);
    self->pm_pixels = NULL;
    glBindTexture(GL_TEXTURE_2D, 0);
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
    ._free = BR_CMETHOD_REF(br_device_pixelmap_gl, free),
    ._identifier = BR_CMETHOD_REF(br_device_pixelmap_gl, identifier),
    ._type = BR_CMETHOD_REF(br_device_pixelmap_gl, type),
    ._isType = BR_CMETHOD_REF(br_device_pixelmap_gl, isType),
    ._device = BR_CMETHOD_REF(br_device_pixelmap_gl, device),
    ._space = BR_CMETHOD_REF(br_device_pixelmap_gl, space),

    ._templateQuery = BR_CMETHOD_REF(br_device_pixelmap_gl, templateQuery),
    ._query = BR_CMETHOD_REF(br_object, query),
    ._queryBuffer = BR_CMETHOD_REF(br_object, queryBuffer),
    ._queryMany = BR_CMETHOD_REF(br_object, queryMany),
    ._queryManySize = BR_CMETHOD_REF(br_object, queryManySize),
    ._queryAll = BR_CMETHOD_REF(br_object, queryAll),
    ._queryAllSize = BR_CMETHOD_REF(br_object, queryAllSize),

    ._validSource = BR_CMETHOD_REF(br_device_pixelmap_mem, validSource),
    ._resize = BR_CMETHOD_REF(br_device_pixelmap_gl, resize),
    ._match = BR_CMETHOD_REF(br_device_pixelmap_gl, match),
    ._allocateSub = BR_CMETHOD_REF(br_device_pixelmap_gl, allocateSub),

    ._copy = BR_CMETHOD_REF(br_device_pixelmap_gen, copy),
    ._copyTo = BR_CMETHOD_REF(br_device_pixelmap_gen, copyTo),
    ._copyFrom = BR_CMETHOD_REF(br_device_pixelmap_gen, copyFrom),
    ._fill = BR_CMETHOD_REF(br_device_pixelmap_gen, fill),
    ._doubleBuffer = BR_CMETHOD_REF(br_device_pixelmap_fail, doubleBuffer),

    ._copyDirty = BR_CMETHOD_REF(br_device_pixelmap_gen, copyDirty),
    ._copyToDirty = BR_CMETHOD_REF(br_device_pixelmap_gen, copyToDirty),
    ._copyFromDirty = BR_CMETHOD_REF(br_device_pixelmap_gen, copyFromDirty),
    ._fillDirty = BR_CMETHOD_REF(br_device_pixelmap_gen, fillDirty),
    ._doubleBufferDirty = BR_CMETHOD_REF(br_device_pixelmap_gen, doubleBufferDirty),

    ._rectangle = BR_CMETHOD_REF(br_device_pixelmap_gen, rectangle),
    ._rectangle2 = BR_CMETHOD_REF(br_device_pixelmap_gen, rectangle2),
    ._rectangleCopy = BR_CMETHOD_REF(br_device_pixelmap_gl, rectangleCopy),
    ._rectangleCopyTo = BR_CMETHOD_REF(br_device_pixelmap_gl, rectangleCopyTo),
    ._rectangleCopyFrom = BR_CMETHOD_REF(br_device_pixelmap_gl, rectangleCopyFrom),
    ._rectangleStretchCopy = BR_CMETHOD_REF(br_device_pixelmap_gl, rectangleStretchCopy),
    ._rectangleStretchCopyTo = BR_CMETHOD_REF(br_device_pixelmap_gl, rectangleStretchCopyTo),
    ._rectangleStretchCopyFrom = BR_CMETHOD_REF(br_device_pixelmap_fail, rectangleStretchCopyFrom),
    ._rectangleFill = BR_CMETHOD_REF(br_device_pixelmap_gl, rectangleFill),
    ._pixelSet = BR_CMETHOD_REF(br_device_pixelmap_mem, pixelSet),
    ._line = BR_CMETHOD_REF(br_device_pixelmap_mem, line),
    ._copyBits = BR_CMETHOD_REF(br_device_pixelmap_fail, copyBits),

    ._text = BR_CMETHOD_REF(br_device_pixelmap_gl, text),
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
    ._directLock = BR_CMETHOD_REF(br_device_pixelmap_gl, directLock),
    ._directUnlock = BR_CMETHOD_REF(br_device_pixelmap_gl, directUnlock),
    ._getControls = BR_CMETHOD_REF(br_device_pixelmap_fail, getControls),
    ._setControls = BR_CMETHOD_REF(br_device_pixelmap_fail, setControls)
};
