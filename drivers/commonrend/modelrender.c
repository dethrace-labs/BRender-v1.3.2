#include "brassert.h"
#include "drv.h"
#include "formats.h"
#include "commonrend.h"
#include "state.h"
#include "video.h"
#include "sbuffer.h"
#include <string.h>

/*
 * Per-group model render, shared by the glrend/sdl3rend drivers.
 *
 * Both drivers follow the same shape:
 *   1. refresh the per-model matrix/light cache
 *   2. copy the matrices into the shader_data_model
 *   3. resolve the group's material state (stored, else default) into
 *      renderer->state.current
 *   4. clear_colour (the vertex shader adds it to every lit model)
 *   5. apply material state and record the draw (backend-specific)
 *   6. tally scene stats
 *
 * Steps 1-4 and 6 are identical; step 5 differs because GL is immediate mode
 * (glBufferData + glDrawElements) while VK uses ring buffers + descriptors.
 * Each driver's v1model.c provides Stored*ApplyProperties; the ring/dim
 * overlay logic stays VK-only.
 */
#if defined(BREND_DRIVER_GL)
void StoredGLRenderGroup(br_geometry_stored* self, br_renderer* renderer, const gl_groupinfo* groupinfo)
#else
void StoredSDL3RENDRenderGroup(br_geometry_stored* self, br_renderer* renderer, sdl3_groupinfo* groupinfo)
#endif
{
    state_cache* cache = &renderer->state.cache;
    br_device_pixelmap* screen = renderer->pixelmap->screen;
    HVIDEO hVideo = &screen->asFront.video;
    br_renderer_state_stored* stored = groupinfo->stored;
    br_renderer_state_stored* default_state = groupinfo->default_state;
    shader_data_model model = {0};

    /* 1. Update the per-model cache (matrices and lights). */
    BREND_FN(State, UpdateModel)(cache, &renderer->state.current->matrix);

    /* 2. Copy matrices into the model UBO. */
    model.projection_brender = cache->model.p_br;
    model.projection = cache->model.p;
    model.model_view = cache->model.mv;
    model.mvp = cache->model.mvp;
    model.normal_matrix = cache->model.normal;
    model.environment_matrix = cache->model.environment;
    model.eye_m = cache->model.eye_m;

    /* 3. Resolve the group's material state into state.current. The matrix in
     * state.current is left untouched so deferred (order-table) renders keep
     * the model's matrix that was snapshotted at insertion time. */
    if (stored) {
        BREND_FN(State, Copy)(renderer->state.current, &stored->state,
            MASK_STATE_SURFACE | MASK_STATE_PRIMITIVE | MASK_STATE_CULL);
    } else {
        renderer->state.current->surface = default_state->state.surface;
        renderer->state.current->prim = default_state->state.prim;
        renderer->state.current->cull = default_state->state.cull;
    }

    /* 4. The vertex shader adds clear_colour to every lit model when no direct
     * light exists; the game driver leaves it black. */
    BrVector4Set(&model.clear_colour, 0.0f, 0.0f, 0.0f, 0.0f);

#if defined(BREND_DRIVER_GL)
    {
#if DEBUG
        { /* Check that sceneBegin() actually did it's shit. */
            GLint p;
            glGetIntegerv(GL_CURRENT_PROGRAM, &p);
            ASSERT(p == hVideo->brenderProgram.program);

            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &p);
            ASSERT(p == renderer->state.current->output.colour->asBack.glFbo);

            glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &p);
            ASSERT(p == hVideo->brenderProgram.uboModel);
        }
#endif

        glBindVertexArray(self->gl_vao);

        GLuint default_tex;
        if (stored) {
            default_tex = screen->asFront.tex_white;
        } else if (groupinfo->default_state->state.prim.colour_map) {
            default_tex = groupinfo->default_state->state.prim.colour_map->gl_tex;
        } else {
            default_tex = renderer->pixelmap->asFront.tex_white;
        }

        StoredGLApplyProperties(hVideo, renderer->state.current,
            MASK_STATE_PRIMITIVE | MASK_STATE_SURFACE, &model, default_tex);

        glBufferData(GL_UNIFORM_BUFFER, sizeof(model), &model, GL_STATIC_DRAW);
        glDrawElements(GL_TRIANGLES, groupinfo->count, GL_UNSIGNED_SHORT, groupinfo->offset);
        GL_CHECK_ERROR();
    }
#else
    {
        SDL_GPUTexture* texture = NULL;
        SDL_GPUSampler* sampler = NULL;

        StoredSDL3RENDApplyProperties(hVideo, renderer->state.current,
            MASK_STATE_PRIMITIVE | MASK_STATE_SURFACE, &model, NULL, &texture, &sampler);

        br_boolean blending_on = (renderer->state.current->prim.flags & PRIMF_BLEND) ||
            (renderer->state.current->prim.colour_map != NULL && renderer->state.current->prim.colour_map->blended);
        br_boolean depth_off = renderer->state.current->surface.force_front || renderer->state.current->surface.force_back;

        SDL_GPUGraphicsPipeline* pipeline = blending_on
            ? (depth_off ? hVideo->brenderBlendPipelineNoDepth : hVideo->brenderBlendPipeline)
            : (depth_off ? hVideo->brenderPipelineNoDepth : hVideo->brenderPipeline);
        if (pipeline != hVideo->lastPipeline) {
            SDL_BindGPUGraphicsPipeline(hVideo->currentPass, pipeline);
            hVideo->lastPipeline = pipeline;
        }

        /* Small ring models (sub-allocated from the per-frame dynamic rings in
         * build_vbo/build_ibo) are only valid within the frame they were rebuilt:
         * the ring cursors reset in SDL3REND_EnsureRecording, so a ring model that
         * persists across frames references clobbered data. Re-upload the geometry
         * from the v11model into the current frame's ring slot when stale. Models
         * rebuilt this frame already stamped ringEpoch == frameEpoch and skip. */
        if (self->inDynamicRing && self->ringEpoch != hVideo->frameEpoch) {
            SDL3REND_RefreshRingStored(hVideo, self);
        }

        if (self->vbo != hVideo->lastVbo || self->vboOffset != hVideo->lastVboOffset ||
            self->ibo != hVideo->lastIbo || self->iboOffset != hVideo->lastIboOffset) {
            SDL_GPUBufferBinding vb = { self->vbo, (Uint32)self->vboOffset };
            SDL_BindGPUVertexBuffers(hVideo->currentPass, 0, &vb, 1);
            SDL_GPUBufferBinding ib = { self->ibo, (Uint32)self->iboOffset };
            SDL_BindGPUIndexBuffer(hVideo->currentPass, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);
            hVideo->lastVbo = self->vbo;
            hVideo->lastVboOffset = self->vboOffset;
            hVideo->lastIbo = self->ibo;
            hVideo->lastIboOffset = self->iboOffset;
        }

        if (texture != NULL) {
            if (texture != hVideo->lastTexture || sampler != hVideo->lastSampler) {
                SDL_GPUTextureSamplerBinding tsb = { texture, sampler };
                SDL_BindGPUFragmentSamplers(hVideo->currentPass, SDL3REND_FRAGMENT_SAMPLER_SLOT, &tsb, 1);
                hVideo->lastTexture = texture;
                hVideo->lastSampler = sampler;
            }
        }

        SDL3REND_PushModel(hVideo, &model, sizeof(model));

        SDL_DrawGPUIndexedPrimitives(hVideo->currentPass, groupinfo->count, 1,
            groupinfo->offset / sizeof(br_uint_16), 0, 0);

        /* Dim quad detection and screen-space AABB tracking for overlay compositing. */
        br_boolean is_dim = (renderer->state.current->surface.colour == 0 &&
            blending_on && depth_off &&
            renderer->state.current->prim.colour_map == NULL);
        if (is_dim && hVideo->dimAreaCount < 8 && screen != NULL &&
            screen->pm_type == BR_PMT_RGB_565) {
            br_matrix4 combined;
            BrMatrix4Mul(&combined, &cache->model.p, &cache->model.mv);
            br_rectangle aabb;
            if (SDL3REND_ComputeScreenAABB(&combined, groupinfo->group, hVideo, renderer->state.current->output.colour, &aabb)) {
                int di = hVideo->dimAreaCount++;
                hVideo->dimAreas[di] = aabb;

                /* The dim quad is rendered GPU-side. Purge its screen rect from the
                 * CPU locked buffer so the swap-time overlay composite doesn't re-draw
                 * the pre-dim 2D content (cockpit dashboard) ON TOP of the dim quad.
                 * Content written into the buffer AFTER this dim (headup text,
                 * instruments) lands on the magenta and still reaches the swap
                 * composite. Skipped in map mode: the flush-time dimArea dimming
                 * (devpixmp.c) dims the map image instead. */
                if (!SDL3REND_IsMapMode(hVideo) && hVideo->lockedPixels != NULL &&
                    hVideo->pm_type == BR_PMT_RGB_565) {
                    SDL3REND_PurgeRect(2, BR_COLOUR_565(31, 0, 31), hVideo->lockedPixels,
                        hVideo->pm_width, hVideo->pm_height, hVideo->pm_row_bytes,
                        aabb.x, aabb.y, aabb.w, aabb.h);
                }
            }
        }

        br_boolean is_pratcam = (depth_off &&
            renderer->state.current->prim.colour_map != NULL &&
            screen != NULL && screen->pm_type == BR_PMT_RGB_565);
        if (is_pratcam) {
            br_matrix4 combined;
            BrMatrix4Mul(&combined, &cache->model.p, &cache->model.mv);
            br_rectangle aabb;
            if (SDL3REND_ComputeScreenAABB(&combined, groupinfo->group, hVideo, renderer->state.current->output.colour, &aabb)) {
                hVideo->pratcamAreaCount = 1;
                hVideo->pratcamArea = aabb;
            }
        }
    }
#endif

    /* 6. Tally scene stats. */
    renderer->scene_stats.face_group_count++;
    renderer->scene_stats.triangles_rendered_count += groupinfo->group->nfaces;
    renderer->scene_stats.triangles_drawn_count += groupinfo->group->nfaces;
    renderer->scene_stats.vertices_rendered_count += groupinfo->group->nfaces * 3;
}
