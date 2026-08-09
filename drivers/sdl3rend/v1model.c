#include "brassert.h"
#include "drv.h"
#include "formats.h"
#include "rend_common.h"
#include "state.h"
#include "video.h"
#include "sbuffer.h"
#include <string.h>

br_boolean VK_ComputeScreenAABB(const br_matrix4* mvp, struct v11group* gp,
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

void StoredVKApplyProperties(HVIDEO hVideo, state_stack* state, uint32_t states,
    shader_data_model* model, br_buffer_stored* default_texture,
    VkWriteDescriptorSet* writes, int* writeCount, VkDescriptorImageInfo* imageInfo) {
    (void)default_texture;

    /* Fill the BRender-agnostic parts of the model UBO (shared with GL). */
    BREND_FN(State, FillModel)(state, states, model);

    model->alpha = state->prim.alpha_val / 255.0f;

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
