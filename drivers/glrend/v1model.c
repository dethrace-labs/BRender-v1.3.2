/*
 * Support routines for rendering models
 */
#include "brassert.h"
#include "drv.h"
#include "commonrend.h"
#include <string.h>

static void apply_blend_mode(state_stack* self) {
    /* C_result = (C_source * F_Source) + (C_dest * F_dest) */

    /* NB: srcAlpha and dstAlpha are all GL_ONE and GL_ZERO respectively. */
    switch (self->prim.blend_mode) {
    default:
        /* fallthrough */
    case BRT_BLEND_STANDARD:
        /* fallthrough */
    case BRT_BLEND_DIMMED:
        /*
         * 3dfx blending mode = 1
         * Colour = (alpha * src) + ((1 - alpha) * dest)
         * Alpha  = (1     * src) + (0           * dest)
         */
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
        break;

    case BRT_BLEND_SUMMED:
        /*
         * 3fdx blending mode = 4
         * Colour = (alpha * src) + (1 * dest)
         * Alpha  = (1     * src) + (0 * dest)
         */
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ZERO);
        break;

    case BRT_BLEND_PREMULTIPLIED:
        /*
         * 3dfx qblending mode = 2
         * Colour = (1 * src) + ((1 - alpha) * dest)
         * Alpha  = (1 * src) + (0           * dest)
         */
        glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
        break;
    }
    GL_CHECK_ERROR();
}

static void apply_depth_properties(state_stack* state, uint32_t states) {
    br_boolean depth_valid = BR_TRUE; /* Defaulting to BR_TRUE to keep existing behaviour. */
    GLenum depth_test = GL_NONE;

    /* Only use the states we want (if valid). */
    states = state->valid & states;

    if (states & MASK_STATE_OUTPUT) {
        depth_valid = state->output.depth != NULL;
    }

    if (states & MASK_STATE_SURFACE) {
        if (state->surface.force_front || state->surface.force_back)
            depth_test = GL_FALSE;
        else
            depth_test = GL_TRUE;
    }

    if (depth_valid == BR_TRUE) {
        if (depth_test == GL_TRUE)
            glEnable(GL_DEPTH_TEST);
        else if (depth_test == GL_FALSE)
            glDisable(GL_DEPTH_TEST);
    }

    if (states & MASK_STATE_PRIMITIVE) {
        if (state->prim.flags & PRIMF_DEPTH_WRITE)
            glDepthMask(GL_TRUE);
        else
            glDepthMask(GL_FALSE);

        GLenum depthFunc;
        switch (state->prim.depth_test) {
        case BRT_LESS:
            depthFunc = GL_LESS;
            break;
        case BRT_GREATER:
            depthFunc = GL_GREATER;
            break;
        case BRT_LESS_OR_EQUAL:
            depthFunc = GL_LEQUAL;
            break;
        case BRT_GREATER_OR_EQUAL:
            depthFunc = GL_GEQUAL;
            break;
        case BRT_EQUAL:
            depthFunc = GL_EQUAL;
            break;
        case BRT_NOT_EQUAL:
            depthFunc = GL_NOTEQUAL;
            break;
        case BRT_NEVER:
            depthFunc = GL_NEVER;
            break;
        case BRT_ALWAYS:
            depthFunc = GL_ALWAYS;
            break;
        default:
            depthFunc = GL_LESS;
        }
        glDepthFunc(depthFunc);
    }
    GL_CHECK_ERROR();
}

// take a pixelmap and palette and convert 8 bit to 32 bit just in time
static void update_paletted_texture(br_pixelmap *src, br_uint_32 *palette) {
    uint32_t* buffer = BrScratchAllocate(sizeof(uint32_t) * src->width * src->height);
    uint8_t* buffer_ptr = (uint8_t*)buffer;
    br_uint_8* src_px = src->pixels;

    for (int y = 0; y < src->height; y++) {
        for (int x = 0; x < src->width; x++) {
            int index = src_px[y * src->row_bytes + x];
            br_uint_32 entry = palette[index];
            buffer_ptr[0] = BR_RED(entry);
            buffer_ptr[1] = BR_GRN(entry);
            buffer_ptr[2] = BR_BLU(entry);
            buffer_ptr[3] = 0xff;
            buffer_ptr += 4;
        }
    }
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, src->width, src->height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
    glGenerateMipmap(GL_TEXTURE_2D);
    BrScratchFree(buffer);
    GL_CHECK_ERROR();
}

void StoredGLApplyProperties(HVIDEO hVideo, state_stack* state, uint32_t states, shader_data_model* model, GLuint tex_default) {
    br_boolean blending_on;

    /* Only use the states we want (if valid). */
    states = state->valid & states;

    BREND_FN(State, FillModel)(state, states, model);

    if (states & MASK_STATE_CULL) {
        /*
         * Apply culling states. These are a bit confusing:
         * BRT_ONE_SIDED - Simple, cull back faces. From BRT_ONE_SIDED.
         *
         * BRT_TWO_SIDED - This means the face is two-sided, not to cull
         *                 both sides. From BR_MATF_TWO_SIDED. In the .3ds file
         *                 format, the "two sided" flag means the material is
         *                 visible from the back, or "not culled". fmt/load3ds.c
         *                 sets BR_MATF_TWO_SIDED if this is set, so assume this is
         *                 the correct behaviour.
         *
         * BRT_NONE      - Confusing, this is set if the material has
         *                 BR_MATF_ALWAYS_VISIBLE, but is overridden if
         *                 BR_MATF_TWO_SIDED is set. Assume it means the same
         *                 as BR_MATF_TWO_SIDED.
         */
        switch (state->cull.type) {
        case BRT_ONE_SIDED:
        default: /* Default BRender policy, so default. */
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            break;

        case BRT_TWO_SIDED:
        case BRT_NONE:
            glDisable(GL_CULL_FACE);
            break;
        }
    }

    if (states & MASK_STATE_SURFACE) {
        glActiveTexture(GL_TEXTURE0);
    }

    if (states & MASK_STATE_PRIMITIVE) {

        if (state->prim.flags & PRIMF_COLOUR_WRITE)
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        else
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

        if (state->prim.colour_map) {
            model->disable_colour_key = !(state->prim.flags & PRIMF_COLOUR_KEY);

            glBindTexture(GL_TEXTURE_2D, BufferStoredGLGetTexture(state->prim.colour_map));

            // has the 8 bit color source changed?
            if (state->prim.colour_map->paletted_source_dirty == BR_TRUE) {
                update_paletted_texture(state->prim.colour_map->source, state->prim.colour_map->palette_pointer->entries);
                state->prim.colour_map->paletted_source_dirty = BR_FALSE;
                state->prim.colour_map->palette_revision = state->prim.colour_map->palette_pointer->revision;
            }
            // or has the palette changed?
            else if (state->prim.colour_map->palette_pointer != NULL && state->prim.colour_map->palette_revision != state->prim.colour_map->palette_pointer->revision) {
                update_paletted_texture(state->prim.colour_map->source, state->prim.colour_map->palette_pointer->entries);
                state->prim.colour_map->palette_revision = state->prim.colour_map->palette_pointer->revision;
            }

            model->disable_texture = 0;

        } else {

            // todo?
            model->disable_colour_key = 0;
            glBindTexture(GL_TEXTURE_2D, tex_default);
            model->disable_texture = 1;
        }

        GLenum minFilter, magFilter;
        GLfloat maxAnisotropy;
        if (state->prim.filter == BRT_LINEAR && state->prim.mip_filter == BRT_LINEAR) {
            minFilter = GL_LINEAR_MIPMAP_LINEAR;
            magFilter = GL_LINEAR;
            maxAnisotropy = hVideo->maxAnisotropy;
        } else if (state->prim.filter == BRT_LINEAR && state->prim.mip_filter == BRT_NONE) {
            minFilter = GL_LINEAR;
            magFilter = GL_LINEAR;
            maxAnisotropy = 1.0f;
        } else if (state->prim.filter == BRT_NONE && state->prim.mip_filter == BRT_LINEAR) {
            minFilter = GL_NEAREST_MIPMAP_NEAREST;
            magFilter = GL_NEAREST;
            maxAnisotropy = hVideo->maxAnisotropy;
        } else if (state->prim.filter == BRT_NONE && state->prim.mip_filter == BRT_NONE) {
            minFilter = GL_NEAREST;
            magFilter = GL_NEAREST;
            maxAnisotropy = 1.0f;
        } else {
            assert(0);
        }

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLint)minFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLint)magFilter);

        if (GLAD_GL_EXT_texture_filter_anisotropic)
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAnisotropy);

        blending_on = (state->prim.flags & PRIMF_BLEND) || (state->prim.colour_map != NULL && state->prim.colour_map->blended);
        if (blending_on) {
            glEnable(GL_BLEND);
            model->alpha = state->prim.alpha_val / 255.0f;
            apply_blend_mode(state);
        } else {
            glDisable(GL_BLEND);
        }
    }
    apply_depth_properties(state, states);
    GL_CHECK_ERROR();
}
