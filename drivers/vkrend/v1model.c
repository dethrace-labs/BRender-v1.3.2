#include "brassert.h"
#include "drv.h"
#include "formats.h"
#include "state.h"
#include "video.h"
#include "sbuffer.h"
#include <string.h>

static br_boolean VK_ComputeScreenAABB(const br_matrix4* mvp, struct v11group* gp,
    HVIDEO hVideo, br_device_pixelmap* colour_target, br_rectangle* out) {
    float dvp_x = 0, dvp_y = 0, dvp_w, dvp_h;
    if (colour_target != NULL) {
        dvp_x = (float)colour_target->pm_base_x;
        dvp_y = (float)colour_target->pm_base_y;
        dvp_w = (float)colour_target->pm_width;
        dvp_h = (float)colour_target->pm_height;
    } else {
        dvp_w = (float)hVideo->swapchainExtent.width;
        dvp_h = (float)hVideo->swapchainExtent.height;
    }
    float min_x = 1e30f, max_x = -1e30f, min_y = 1e30f, max_y = -1e30f;
    for (br_uint_16 v = 0; v < gp->nvertices; v++) {
        br_vector3_f* pos = (br_vector3_f*)(gp->position + v);
        float cx = mvp->m[0][0]*pos->v[0] + mvp->m[1][0]*pos->v[1] + mvp->m[2][0]*pos->v[2] + mvp->m[3][0];
        float cy = mvp->m[0][1]*pos->v[0] + mvp->m[1][1]*pos->v[1] + mvp->m[2][1]*pos->v[2] + mvp->m[3][1];
        float cw = mvp->m[0][3]*pos->v[0] + mvp->m[1][3]*pos->v[1] + mvp->m[2][3]*pos->v[2] + mvp->m[3][3];
        float nx = cx / cw, ny = cy / cw;
        float sx = (nx + 1.0f) * 0.5f * dvp_w + dvp_x;
        float sy = (ny + 1.0f) * 0.5f * dvp_h + dvp_y;
        if (sx < min_x) min_x = sx;
        if (sx > max_x) max_x = sx;
        if (sy < min_y) min_y = sy;
        if (sy > max_y) max_y = sy;
    }
    out->x = (int)min_x;
    out->y = (int)min_y;
    out->w = (int)(max_x - min_x + 0.5f);
    out->h = (int)(max_y - min_y + 0.5f);
    return BR_TRUE;
}

static void apply_stored_properties(HVIDEO hVideo, state_stack* state, uint32_t states,
    shader_data_model* model, br_buffer_stored* default_texture,
    VkWriteDescriptorSet* writes, int* writeCount, VkDescriptorImageInfo* imageInfo) {
    (void)states;
    (void)default_texture;

    model->ka = BrScalarToFloat(state->surface.ka);
    model->kd = BrScalarToFloat(state->surface.kd);
    model->ks = BrScalarToFloat(state->surface.ks);
    model->power = BrScalarToFloat(state->surface.power);
    model->surface_colour.v[0] = BR_RED(state->surface.colour) / 255.0f;
    model->surface_colour.v[1] = BR_GRN(state->surface.colour) / 255.0f;
    model->surface_colour.v[2] = BR_BLU(state->surface.colour) / 255.0f;
    model->surface_colour.v[3] = state->surface.opacity / 255.0f;
    model->lighting = state->surface.lighting ? 1 : 0;
    model->prelit = state->surface.prelighting ? 1 : 0;

    switch (state->surface.mapping_source) {
    case BRT_ENVIRONMENT_LOCAL:
        model->uv_source = 1;
        break;
    case BRT_ENVIRONMENT_INFINITE:
        model->uv_source = 2;
        break;
    default:
        model->uv_source = 0;
        break;
    }

    BrMatrix4Copy23(&model->map_transform, &state->surface.map_transform);

    model->alpha = state->prim.alpha_val / 255.0f;
    model->fog_enabled = state->prim.fog_enabled ? 1 : 0;
    model->fog_colour.v[0] = BR_RED(state->prim.fog_colour) / 255.0f;
    model->fog_colour.v[1] = BR_GRN(state->prim.fog_colour) / 255.0f;
    model->fog_colour.v[2] = BR_BLU(state->prim.fog_colour) / 255.0f;
    model->fog_min = BrScalarToFloat(state->prim.fog_min);
    model->fog_max = BrScalarToFloat(state->prim.fog_max);

    br_buffer_stored* colour_map = state->prim.colour_map;
    if (colour_map) {
        BufferStoredVKReupload(colour_map);
        if (colour_map->view != VK_NULL_HANDLE && colour_map->sampler != VK_NULL_HANDLE) {
            model->disable_texture = 0;
            model->disable_colour_key = !(state->prim.flags & PRIMF_COLOUR_KEY);

            imageInfo->imageLayout = colour_map->imageLayout;
            imageInfo->imageView = colour_map->view;
            imageInfo->sampler = (state->prim.filter == BRT_NONE) ? hVideo->samplerNearest : hVideo->samplerLinear;
        } else {
            model->disable_texture = 1;
            model->disable_colour_key = 1;

            imageInfo->imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageInfo->imageView = hVideo->defaultTextureView;
            imageInfo->sampler = hVideo->defaultSampler;
        }
    } else {
        model->disable_texture = 1;
        model->disable_colour_key = 1;

        imageInfo->imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo->imageView = hVideo->defaultTextureView;
        imageInfo->sampler = hVideo->defaultSampler;
    }

    writes[*writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[*writeCount].dstBinding = 0;
    writes[*writeCount].descriptorCount = 1;
    writes[*writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[*writeCount].pImageInfo = imageInfo;
    (*writeCount)++;
}

void StoredVKRenderGroup(br_geometry_stored* self, br_renderer* renderer, vk_groupinfo* groupinfo) {
    br_device_pixelmap* screen = renderer->pixelmap->screen;
    HVIDEO hVideo = &screen->asFront.video;
    VkCommandBuffer cmd = hVideo->drawCommandBuffers[hVideo->currentImageIndex];

    state_cache* cache = &renderer->state.cache;
    StateVKUpdateModel(cache, &renderer->state.current->matrix);

    shader_data_model model = {0};
    model.projection_brender = cache->model.p_br;
    model.projection = cache->model.p;
    model.model_view = cache->model.mv;
    model.mvp = cache->model.mvp;
    model.normal_matrix = cache->model.normal;
    model.environment_matrix = cache->model.environment;
    model.eye_m = cache->model.eye_m;

    VkWriteDescriptorSet writes[3] = {0};
    VkDescriptorImageInfo imageInfo = {0};
    int writeCount = 0;

    apply_stored_properties(hVideo, renderer->state.current, MASK_STATE_PRIMITIVE | MASK_STATE_SURFACE,
        &model, NULL, writes, &writeCount, &imageInfo);

    VkDeviceSize modelOffset = hVideo->currentModelOffset;
    VkDeviceSize sceneDataSize = sizeof(renderer->state.cache.scene);
    VkDeviceSize modelDataSize = sizeof(model);
    VK_UpdateModelUBOAtOffset(hVideo, &renderer->state.cache.scene, sceneDataSize, modelOffset);
    VK_UpdateModelUBOAtOffset(hVideo, &model, modelDataSize, modelOffset + hVideo->sceneSlotSize);
    hVideo->currentModelOffset += hVideo->modelSlotSize;
    if (hVideo->currentModelOffset >= hVideo->modelBufferCapacity)
        hVideo->currentModelOffset = 0;

    br_boolean blending_on = (renderer->state.current->prim.flags & PRIMF_BLEND) ||
        (renderer->state.current->prim.colour_map != NULL && renderer->state.current->prim.colour_map->blended);
    br_boolean depth_off = renderer->state.current->surface.force_front || renderer->state.current->surface.force_back;

    VkPipeline pipeline = blending_on
        ? (depth_off ? hVideo->brenderBlendPipelineNoDepth : hVideo->brenderBlendPipeline)
        : (depth_off ? hVideo->brenderPipelineNoDepth : hVideo->brenderPipeline);
    if (pipeline != hVideo->lastPipeline) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        hVideo->lastPipeline = pipeline;
    }

    /* Small ring models (sub-allocated from the per-frame dynamic rings in
     * build_vbo/build_ibo) are only valid within the frame they were rebuilt:
     * the ring cursors reset in VK_EnsureRecording, so a ring model that
     * persists across frames references clobbered data. Re-upload the geometry
     * from the v11model into the current frame's ring slot when stale. Models
     * rebuilt this frame already stamped ringEpoch == frameEpoch and skip. */
    if ((self->vboMemory == VK_NULL_HANDLE || self->iboMemory == VK_NULL_HANDLE) &&
        self->ringEpoch != hVideo->frameEpoch) {
        VK_RefreshRingStored(hVideo, self);
    }

    if (self->vbo != hVideo->lastVbo || self->vboOffset != hVideo->lastVboOffset ||
        self->ibo != hVideo->lastIbo || self->iboOffset != hVideo->lastIboOffset) {
        VkDeviceSize vboOffset = self->vboOffset;
        vkCmdBindVertexBuffers(cmd, 0, 1, &self->vbo, &vboOffset);
        vkCmdBindIndexBuffer(cmd, self->ibo, self->iboOffset, VK_INDEX_TYPE_UINT16);
        hVideo->lastVbo = self->vbo;
        hVideo->lastVboOffset = self->vboOffset;
        hVideo->lastIbo = self->ibo;
        hVideo->lastIboOffset = self->iboOffset;
    }

    int descriptorStart = 0;
    VkWriteDescriptorSet pushWrites[3] = {0};
    int pushCount = 0;
    if (imageInfo.imageView == hVideo->lastTextureView && imageInfo.sampler == hVideo->lastTextureSampler) {
        descriptorStart = 1;
    } else {
        hVideo->lastTextureView = imageInfo.imageView;
        hVideo->lastTextureSampler = imageInfo.sampler;
    }

    VkDescriptorBufferInfo sceneBufferInfo = {hVideo->brenderDescriptors.modelBuffer, modelOffset, sceneDataSize};
    VkDescriptorBufferInfo modelBufferInfo = {hVideo->brenderDescriptors.modelBuffer, modelOffset + hVideo->sceneSlotSize, modelDataSize};

    if (descriptorStart == 0) {
    pushWrites[pushCount++] = writes[0];
    }

    pushWrites[pushCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    pushWrites[pushCount].dstBinding = 1;
    pushWrites[pushCount].descriptorCount = 1;
    pushWrites[pushCount].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pushWrites[pushCount].pBufferInfo = &sceneBufferInfo;
    pushCount++;

    pushWrites[pushCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    pushWrites[pushCount].dstBinding = 2;
    pushWrites[pushCount].descriptorCount = 1;
    pushWrites[pushCount].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pushWrites[pushCount].pBufferInfo = &modelBufferInfo;
    pushCount++;

    hVideo->pfnPushDescriptorSet(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, hVideo->brenderPipelineLayout,
        0, pushCount, pushWrites);

    vkCmdDrawIndexed(cmd, groupinfo->count, 1, groupinfo->offset / sizeof(br_uint_16), 0, 0);

    // Dim quad detection and screen-space AABB tracking for overlay compositing.
    br_boolean is_dim = (renderer->state.current->surface.colour == 0 &&
        blending_on && depth_off &&
        renderer->state.current->prim.colour_map == NULL);
    if (is_dim && hVideo->dimAreaCount < 8 && screen != NULL &&
        screen->pm_type == BR_PMT_RGB_565) {
        br_matrix4 combined;
        BrMatrix4Mul(&combined, &cache->model.p, &cache->model.mv);
        br_rectangle aabb;
        if (VK_ComputeScreenAABB(&combined, groupinfo->group, hVideo, renderer->state.current->output.colour, &aabb)) {
            int di = hVideo->dimAreaCount++;
            hVideo->dimAreas[di] = aabb;

            // The dim quad is rendered GPU-side. Purge its screen rect from the
            // CPU locked buffer so the swap-time overlay composite doesn't re-draw
            // the pre-dim 2D content (cockpit dashboard) ON TOP of the dim quad.
            // Content written into the buffer AFTER this dim (headup text,
            // instruments) lands on the magenta and still reaches the swap
            // composite. Skipped in map mode: the flush-time dimArea dimming
            // (devpixmp.c) dims the map image instead.
            extern int gMap_mode;
            if (!gMap_mode && hVideo->lockedPixels != NULL &&
                hVideo->pm_type == BR_PMT_RGB_565) {
                VK_PurgeRect(2, BR_COLOUR_565(31, 0, 31), hVideo->lockedPixels,
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
        if (VK_ComputeScreenAABB(&combined, groupinfo->group, hVideo, renderer->state.current->output.colour, &aabb)) {
            hVideo->pratcamAreaCount = 1;
            hVideo->pratcamArea = aabb;
        }
    }

    renderer->scene_stats.face_group_count++;
    renderer->scene_stats.triangles_rendered_count += groupinfo->group->nfaces;
    renderer->scene_stats.triangles_drawn_count += groupinfo->group->nfaces;
    renderer->scene_stats.vertices_rendered_count += groupinfo->group->nfaces * 3;
}
