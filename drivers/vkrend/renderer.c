#include "drv.h"
#include "brassert.h"
#include <string.h>
#include <math.h>

static const struct br_renderer_dispatch rendererDispatch;

#define F(f) offsetof(struct br_renderer, f)

static struct br_tv_template_entry rendererTemplateEntries[] = {
    { BRT(IDENTIFIER_CSTR), F(identifier), BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY },
    { BRT(FACE_GROUP_COUNT_U32), F(scene_stats.face_group_count), BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY },
    { BRT(TRIANGLES_DRAWN_COUNT_U32), F(scene_stats.triangles_drawn_count), BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY },
    { BRT(TRIANGLES_RENDERED_COUNT_U32), F(scene_stats.triangles_rendered_count), BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY },
    { BRT(VERTICES_RENDERED_COUNT_U32), F(scene_stats.vertices_rendered_count), BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY },
};
#undef F

br_renderer* RendererVKAllocate(br_device* device, br_renderer_facility* facility, br_device_pixelmap* dest) {
    br_renderer* self;

    if (dest == NULL || ObjectDevice(dest) != device)
        return NULL;

    self = BrResAllocate(facility, sizeof(*self), BR_MEMORY_OBJECT);
    self->dispatch = &rendererDispatch;
    self->identifier = facility->identifier;
    self->device = device;
    self->object_list = BrObjectListAllocate(self);
    self->pixelmap = dest;
    self->renderer_facility = facility;
    self->state_pool = BrPoolAllocate(sizeof(state_stack), 1024, BR_MEMORY_OBJECT_DATA);

    ObjectContainerAddFront(facility, (br_object*)self);

    StateVKInit(&self->state, self->device);

    RendererStateDefault(self, (br_uint_32)BR_STATE_ALL);

    self->has_begun = 0;
    dest->renderer = self;
    return (br_renderer*)self;
}

static void BR_CMETHOD_DECL(br_renderer_vk, sceneBegin)(br_renderer* self) {
    br_device_pixelmap* screen = self->pixelmap->screen;
    HVIDEO hVideo = &screen->asFront.video;

    self->scene_stats.face_group_count = 0;
    self->scene_stats.triangles_drawn_count = 0;
    self->scene_stats.triangles_rendered_count = 0;
    self->scene_stats.vertices_rendered_count = 0;

    br_device_pixelmap *colour_target = NULL;
    if (self->state.current->valid & BR_STATE_OUTPUT) {
        colour_target = self->state.current->output.colour;
        br_device_pixelmap *depth_target = self->state.current->output.depth;

        if (colour_target != NULL && ObjectDevice(colour_target) != self->device) {
            BR_ERROR0("Can't render to a non-device colour pixelmap");
        }

        if (depth_target != NULL && ObjectDevice(depth_target) != self->device) {
            BR_ERROR0("Can't render to a non-device depth pixelmap");
        }

        if (colour_target == NULL) {
            BR_ERROR("Can't render without a destination");
        }
    }

    StateVKReset(&self->state.cache);
    StateVKUpdateScene(&self->state.cache, self->state.current);

    hVideo->currentSceneOffset = hVideo->sceneSlotIndex * hVideo->sceneSlotSize;
    hVideo->sceneSlotIndex++;
    if (hVideo->sceneSlotIndex * hVideo->sceneSlotSize >= hVideo->sceneBufferCapacity)
        hVideo->sceneSlotIndex = 0;
    VK_UpdateSceneUBO(hVideo, &self->state.cache.scene, sizeof(self->state.cache.scene), hVideo->currentSceneOffset);

    if (hVideo->sceneCount == 0) {
        if (!hVideo->isRecording) {
            hVideo->currentModelOffset = 0;
            hVideo->sceneSlotIndex = 0;
            hVideo->dimAreaCount = 0;
            hVideo->clearAreaCount = 0;
            hVideo->pratcamAreaCount = 0;
            hVideo->lastVbo = VK_NULL_HANDLE;
            hVideo->lastIbo = VK_NULL_HANDLE;
            hVideo->lastTextureView = VK_NULL_HANDLE;
            hVideo->lastTextureSampler = VK_NULL_HANDLE;

            uint32_t f = hVideo->currentFrame;
            vkWaitForFences(hVideo->device, 1, &hVideo->inFlightFences[f], VK_TRUE, UINT64_MAX);
            vkResetFences(hVideo->device, 1, &hVideo->inFlightFences[f]);

            if (hVideo->deferredBufferFreeCount[f] > 0) {
                for (uint32_t i = 0; i < hVideo->deferredBufferFreeCount[f]; i++) {
                    if (hVideo->deferredBufferFrees[f][i].buffer != VK_NULL_HANDLE)
                        vkDestroyBuffer(hVideo->device, hVideo->deferredBufferFrees[f][i].buffer, NULL);
                    if (hVideo->deferredBufferFrees[f][i].memory != VK_NULL_HANDLE)
                        vkFreeMemory(hVideo->device, hVideo->deferredBufferFrees[f][i].memory, NULL);
                }
                hVideo->deferredBufferFreeCount[f] = 0;
            }

            if (hVideo->deferredImageFreeCount[f] > 0) {
                for (uint32_t i = 0; i < hVideo->deferredImageFreeCount[f]; i++) {
                    if (hVideo->deferredImageFrees[f][i].sampler != VK_NULL_HANDLE)
                        vkDestroySampler(hVideo->device, hVideo->deferredImageFrees[f][i].sampler, NULL);
                    if (hVideo->deferredImageFrees[f][i].view != VK_NULL_HANDLE)
                        vkDestroyImageView(hVideo->device, hVideo->deferredImageFrees[f][i].view, NULL);
                    if (hVideo->deferredImageFrees[f][i].image != VK_NULL_HANDLE)
                        vkDestroyImage(hVideo->device, hVideo->deferredImageFrees[f][i].image, NULL);
                    if (hVideo->deferredImageFrees[f][i].memory != VK_NULL_HANDLE)
                        vkFreeMemory(hVideo->device, hVideo->deferredImageFrees[f][i].memory, NULL);
                }
                hVideo->deferredImageFreeCount[f] = 0;
            }

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
                hVideo->imageAvailableSemaphores[hVideo->currentFrame], VK_NULL_HANDLE, &hVideo->currentImageIndex);
            if (res == VK_ERROR_OUT_OF_DATE_KHR) {
                VK_VideoRecreateSwapchain(hVideo);
                hVideo->mainViewportW = 0;
                res = vkAcquireNextImageKHR(hVideo->device, hVideo->swapchain, UINT64_MAX,
                    hVideo->imageAvailableSemaphores[hVideo->currentFrame], VK_NULL_HANDLE, &hVideo->currentImageIndex);
            }

            VkCommandBuffer cmd = hVideo->drawCommandBuffers[hVideo->currentImageIndex];
            vkResetCommandBuffer(cmd, 0);

            VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            begin.flags = 0;
            vkBeginCommandBuffer(cmd, &begin);

            hVideo->isRecording = 1;
            hVideo->renderingStarted = 1;
            if (colour_target != NULL &&
                colour_target->pm_width >= screen->pm_width &&
                colour_target->pm_height >= screen->pm_height)
                hVideo->primaryColourTarget = colour_target;
        }

        VkCommandBuffer cmd = hVideo->drawCommandBuffers[hVideo->currentImageIndex];
        if (!hVideo->renderPassActive) {
            VK_BeginRenderPass(hVideo, cmd);
            hVideo->renderPassActive = 1;
        }
    }
    {
        VkCommandBuffer cmd = hVideo->drawCommandBuffers[hVideo->currentImageIndex];
        if (hVideo->overlayDirty) {
            br_device_pixelmap* screen = self->pixelmap->screen;
            int vp_x, vp_y;
            float rx, ry;
            DevicePixelmapVKGetViewport(screen, &vp_x, &vp_y, &rx, &ry);
            float ov_vp_x = (float)screen->pm_base_x * rx + (float)vp_x;
            float ov_vp_y = (float)screen->pm_base_y * ry + (float)vp_y;
            float ov_vp_w = (float)screen->pm_width * rx;
            float ov_vp_h = (float)screen->pm_height * ry;
            VkViewport ov_vp = {ov_vp_x, ov_vp_y, ov_vp_w, ov_vp_h, 0.0f, 1.0f};
            vkCmdSetViewport(cmd, 0, 1, &ov_vp);
            VkRect2D ov_sc = {{(int32_t)ov_vp_x, (int32_t)ov_vp_y}, {(uint32_t)ov_vp_w, (uint32_t)ov_vp_h}};
            vkCmdSetScissor(cmd, 0, 1, &ov_sc);
            VK_OverlayDraw(hVideo, cmd);
            hVideo->overlayDirty = 0;
        }
    }

    hVideo->sceneCount++;

    {
        VkCommandBuffer cmd = hVideo->drawCommandBuffers[hVideo->currentImageIndex];
        int x = 0, y = 0;
        float rx = 1.0f, ry = 1.0f;
        float vp_x, vp_y, vp_w, vp_h;

        if (colour_target != NULL) {
            DevicePixelmapVKGetViewport(colour_target->screen, &x, &y, &rx, &ry);
            vp_x = (float)colour_target->pm_base_x * rx + (float)x;
            vp_y = (float)colour_target->pm_base_y * ry + (float)y;
            vp_w = (float)colour_target->pm_width * rx;
            vp_h = (float)colour_target->pm_height * ry;
        } else {
            vp_x = 0;
            vp_y = 0;
            vp_w = (float)hVideo->swapchainExtent.width;
            vp_h = (float)hVideo->swapchainExtent.height;
        }

        VkViewport viewport = {vp_x, vp_y, vp_w, vp_h, 0.0f, 1.0f};
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        int32_t sc_x = (int32_t)floorf(vp_x);
        int32_t sc_y = (int32_t)floorf(vp_y);
        VkRect2D scissor = {{sc_x, sc_y},
            {(uint32_t)((int32_t)ceilf(vp_x + vp_w) - sc_x),
             (uint32_t)((int32_t)ceilf(vp_y + vp_h) - sc_y)}};
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        hVideo->viewportX = (int)vp_x;
        hVideo->viewportY = (int)vp_y;
        hVideo->viewportW = (int)vp_w;
        hVideo->viewportH = (int)vp_h;

        extern int gMap_mode;
        if (colour_target != NULL &&
            colour_target->pm_width >= screen->pm_width &&
            colour_target->pm_height >= screen->pm_height &&
            !gMap_mode) {
            hVideo->mainViewportX = (int)((vp_x - (float)x) / rx);
            hVideo->mainViewportY = (int)((vp_y - (float)y) / ry);
            hVideo->mainViewportW = (int)(vp_w / rx);
            hVideo->mainViewportH = (int)(vp_h / ry);
        }
    }

    self->has_begun = 1;
}

static void BR_CMETHOD_DECL(br_renderer_vk, sceneEnd)(br_renderer* self) {
    br_device_pixelmap* screen = self->pixelmap->screen;
    HVIDEO hVideo = &screen->asFront.video;

    if (hVideo->sceneCount > 0) {
        hVideo->sceneCount--;
    }

    if (hVideo->clearAreaCount < 4 && (self->state.current->valid & BR_STATE_OUTPUT)) {
        br_device_pixelmap* colour_target = self->state.current->output.colour;
        if (colour_target != NULL) {
            int is_sub_area = (colour_target->pm_width < screen->pm_width ||
                               colour_target->pm_height < screen->pm_height);
            int is_primary = (colour_target == hVideo->primaryColourTarget);
            if (is_sub_area && !is_primary) {
                int idx = hVideo->clearAreaCount++;
                hVideo->clearAreas[idx].x = colour_target->pm_base_x;
                hVideo->clearAreas[idx].y = colour_target->pm_base_y;
                hVideo->clearAreas[idx].w = colour_target->pm_width;
                hVideo->clearAreas[idx].h = colour_target->pm_height;
                hVideo->overlayDirty = 1;
            }
        }
    }

    self->has_begun = 0;
}

static void BR_CMETHOD_DECL(br_renderer_vk, free)(br_object* _self) {
    br_renderer* self = (br_renderer*)_self;

    BrPoolFree(self->state_pool);

    ObjectContainerRemove(self->renderer_facility, (br_object*)self);

    BrObjectContainerFree((br_object_container*)self, BR_NULL_TOKEN, NULL, NULL);

    BrResFreeNoCallback(self);
}

static char* BR_CMETHOD_DECL(br_renderer_vk, identifier)(br_object* self) {
    return (char*)((br_renderer*)self)->identifier;
}

static br_token BR_CMETHOD_DECL(br_renderer_vk, type)(br_object* self) {
    return BRT_RENDERER;
}

static br_boolean BR_CMETHOD_DECL(br_renderer_vk, isType)(br_object* self, br_token t) {
    return (t == BRT_RENDERER) || (t == BRT_OBJECT);
}

static struct br_device* BR_CMETHOD_DECL(br_renderer_vk, device)(br_object* self) {
    return ((br_renderer*)self)->device;
}

static int BR_CMETHOD_DECL(br_renderer_vk, space)(br_object* self) {
    return sizeof(br_renderer);
}

static struct br_tv_template* BR_CMETHOD_DECL(br_renderer_vk, templateQuery)(br_object* _self) {
    br_renderer* self = (br_renderer*)_self;

    if (self->device->templates.rendererTemplate == NULL) {
        self->device->templates.rendererTemplate = BrTVTemplateAllocate(self->device, rendererTemplateEntries,
            BR_ASIZE(rendererTemplateEntries));
    }

    return self->device->templates.rendererTemplate;
}

static void* BR_CMETHOD_DECL(br_renderer_vk, listQuery)(br_object_container* self) {
    return ((br_renderer*)self)->object_list;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, validDestination)(br_renderer* self, br_boolean* bp, br_object* h) {
    return BRE_OK;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, stateStoredNew)(br_renderer* self, br_renderer_state_stored** pss,
    br_uint_32 mask, br_token_value* tv) {
    br_renderer_state_stored* ss;

    if ((ss = RendererStateStoredVKAllocate(self, self->state.current, mask, tv)) == NULL)
        return BRE_FAIL;

    *pss = ss;
    return BRE_OK;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, stateStoredAvail)(br_renderer* self, br_int_32* psize, br_uint_32 mask,
    br_token_value* tv) {
    return BRE_FAIL;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, bufferStoredNew)(br_renderer* self, br_buffer_stored** psm, br_token use,
    br_device_pixelmap* pm, br_token_value* tv) {
    (void)tv;

    if ((*psm = BufferStoredVKAllocate(self, use, pm, tv)) == NULL)
        return BRE_FAIL;

    return BRE_OK;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, bufferStoredAvail)(br_renderer* self, br_int_32* space, br_token use,
    br_token_value* tv) {
    return BRE_FAIL;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, partSet)(br_renderer* self, br_token part, br_int_32 index, br_token t, br_value value) {
    br_error r;
    br_uint_32 m;
    struct br_tv_template* tp;

    if ((tp = StateVKGetStateTemplate(&self->state, part, index)) == NULL)
        return BRE_FAIL;

    m = 0;
    r = BrTokenValueSet(self->state.current, &m, t, value, tp);

    return r;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, partSetMany)(br_renderer* self, br_token part, br_int_32 index,
    br_token_value* tv, br_int_32* pcount) {
    br_error r;
    br_uint_32 m;
    struct br_tv_template* tp;

    if ((tp = StateVKGetStateTemplate(&self->state, part, index)) == NULL)
        return BRE_FAIL;

    m = 0;
    r = BrTokenValueSetMany(self->state.current, pcount, &m, tv, tp);

    return r;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, partQuery)(br_renderer* self, br_token part, br_int_32 index,
    void* pvalue, br_token t) {
    struct br_tv_template* tp;

    if ((tp = StateVKGetStateTemplate(&self->state, part, index)) == NULL)
        return BRE_FAIL;

    return BrTokenValueQuery(pvalue, NULL, 0, t, self->state.current, tp);
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, partQueryBuffer)(br_renderer* self, br_token part, br_int_32 index,
    void* pvalue, void* buffer, br_size_t buffer_size, br_token t) {
    struct br_tv_template* tp;

    if ((tp = StateVKGetStateTemplate(&self->state, part, index)) == NULL)
        return BRE_FAIL;

    return BrTokenValueQuery(pvalue, buffer, buffer_size, t, self->state.current, tp);
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, partQueryMany)(br_renderer* self, br_token part, br_int_32 index,
    br_token_value* tv, void* extra, br_size_t extra_size,
    br_int_32* pcount) {
    struct br_tv_template* tp;

    if ((tp = StateVKGetStateTemplate(&self->state, part, index)) == NULL)
        return BRE_FAIL;

    return BrTokenValueQueryMany(tv, extra, extra_size, pcount, self->state.current, tp);
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, partQueryManySize)(br_renderer* self, br_token part, br_int_32 index,
    br_size_t* pextra_size, br_token_value* tv) {
    struct br_tv_template* tp;

    if ((tp = StateVKGetStateTemplate(&self->state, part, index)) == NULL)
        return BRE_FAIL;

    return BrTokenValueQueryManySize(pextra_size, tv, self->state.current, tp);
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, partQueryAll)(br_renderer* self, br_token part, br_int_32 index,
    br_token_value* buffer, br_size_t buffer_size) {
    struct br_tv_template* tp;

    if ((tp = StateVKGetStateTemplate(&self->state, part, index)) == NULL)
        return BRE_FAIL;

    return BrTokenValueQueryAll(buffer, buffer_size, self->state.current, tp);
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, partQueryAllSize)(br_renderer* self, br_token part, br_int_32 index,
    br_size_t* psize) {
    struct br_tv_template* tp;

    if ((tp = StateVKGetStateTemplate(&self->state, part, index)) == NULL)
        return BRE_FAIL;

    return BrTokenValueQueryAllSize(psize, self->state.current, tp);
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, partIndexQuery)(br_renderer* self, br_token part, br_int_32* pnindex) {
    (void)self;

    if (pnindex == NULL)
        return BRE_FAIL;

    switch (part) {
    case BRT_CULL:
    case BRT_SURFACE:
    case BRT_MATRIX:
    case BRT_ENABLE:
    case BRT_BOUNDS:
    case BRT_HIDDEN_SURFACE:
        *pnindex = 1;
        return BRE_OK;

    case BRT_LIGHT:
        *pnindex = MAX_STATE_LIGHTS;
        return BRE_OK;

    case BRT_CLIP:
        *pnindex = MAX_STATE_CLIP_PLANES;
        return BRE_OK;

    case BRT_OUTPUT:
    case BRT_PRIMITIVE:
        *pnindex = 1;
        return BRE_OK;

    default:
        break;
    }

    return BRE_FAIL;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, commandModeSet)(br_renderer* self, br_token mode) {
    return BRE_FAIL;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, commandModeQuery)(br_renderer* self, br_token* mode) {
    return BRE_FAIL;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, commandModeDefault)(br_renderer* self) {
    return BRE_FAIL;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, commandModePush)(br_renderer* self) {
    return BRE_FAIL;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, commandModePop)(br_renderer* self) {
    return BRE_FAIL;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, modelMul)(br_renderer* self, br_matrix34_f* m) {
    br_matrix34 om = self->state.current->matrix.model_to_view;

    BrMatrix34Mul(&self->state.current->matrix.model_to_view, (br_matrix34*)m, &om);

    self->state.current->matrix.model_to_view_hint = BRT_NONE;

    return BRE_OK;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, modelPopPushMul)(br_renderer* self, br_matrix34_f* m) {
    if (self->state.top == 0)
        return BRE_UNDERFLOW;

    BrMatrix34Mul(&self->state.current->matrix.model_to_view, (br_matrix34*)m, &self->state.stack[0].matrix.model_to_view);

    self->state.current->matrix.model_to_view_hint = BRT_NONE;

    return BRE_OK;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, modelInvert)(br_renderer* self) {
    br_matrix34 old;

    BrMatrix34Copy(&old, &self->state.current->matrix.model_to_view);

    if (self->state.current->matrix.model_to_view_hint == BRT_LENGTH_PRESERVING)
        BrMatrix34LPInverse(&self->state.current->matrix.model_to_view, &old);
    else
        BrMatrix34Inverse(&self->state.current->matrix.model_to_view, &old);

    return BRE_OK;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, statePush)(br_renderer* self, br_uint_32 mask) {
    return StateVKPush(&self->state, mask) ? BRE_OK : BRE_OVERFLOW;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, statePop)(br_renderer* self, br_uint_32 mask) {
    return StateVKPop(&self->state, mask) ? BRE_OK : BRE_OVERFLOW;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, stateSave)(br_renderer* self, br_renderer_state_stored* save, br_uint_32 mask) {
    StateVKCopy(&save->state, self->state.current, mask);
    return BRE_OK;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, stateRestore)(br_renderer* self, br_renderer_state_stored* save, br_uint_32 mask) {
    StateVKCopy(self->state.current, &save->state, mask);
    return BRE_OK;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, stateDefault)(br_renderer* self, br_uint_32 mask) {
    StateVKDefault(&self->state, mask);
    return BRE_OK;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, stateMask)(br_renderer* self, br_uint_32* mask, br_token* parts, int n_parts) {
    br_uint_32 m;

    (void)self;

    if (mask == NULL)
        return BRE_FAIL;

    m = 0;
    for (int i = 0; i < n_parts; i++) {
        switch (parts[i]) {
        case BRT_SURFACE:
            m |= MASK_STATE_SURFACE;
            break;
        case BRT_MATRIX:
            m |= MASK_STATE_MATRIX;
            break;
        case BRT_ENABLE:
            m |= MASK_STATE_ENABLE;
            break;
        case BRT_LIGHT:
            m |= MASK_STATE_LIGHT;
            break;
        case BRT_CLIP:
            m |= MASK_STATE_CLIP;
            break;
        case BRT_BOUNDS:
            m |= MASK_STATE_BOUNDS;
            break;
        case BRT_CULL:
            m |= MASK_STATE_CULL;
            break;
        case BRT_OUTPUT:
            m |= MASK_STATE_OUTPUT;
            break;
        case BRT_PRIMITIVE:
            m |= MASK_STATE_PRIMITIVE;
            break;
        default:
            break;
        }
    }

    *mask = m;

    return BRE_OK;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, boundsTest)(br_renderer* self, br_token* r, br_bounds3_f* bounds) {
    br_matrix4 m2s;
    BrMatrix4Mul34(&m2s, &self->state.current->matrix.model_to_view, &self->state.current->matrix.view_to_screen);
    *r = VKOnScreenCheck(self, &m2s, bounds);
    return BRE_OK;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, coverageTest)(br_renderer* self, br_float* r, br_bounds3_f* bounds) {
    return BRE_FAIL;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, viewDistance)(br_renderer* self, br_float* r) {
    return BRE_FAIL;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, flush)(br_renderer* self, br_boolean wait) {
    (void)self;
    (void)wait;
    return BRE_OK;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, synchronise)(br_renderer* self, br_token sync_type, br_boolean block) {
    return BRE_UNSUPPORTED;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, partQueryCapability)(br_renderer* self, br_token part, br_int_32 index,
    br_token_value* buffer, br_size_t buffer_size) {
    return BRE_FAIL;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, stateQueryPerformance)(br_renderer* self, br_fixed_lu* speed) {
    return BRE_FAIL;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, frameBegin)(br_renderer* self) {
    self->frame_stats.model_count = 0;
    return BRE_OK;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, frameEnd)(br_renderer* self) {
    return BRE_OK;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, focusLossBegin)(br_renderer* self) {
    return BRE_OK;
}

static br_error BR_CMETHOD_DECL(br_renderer_vk, focusLossEnd)(br_renderer* self) {
    return BRE_OK;
}

static const struct br_renderer_dispatch rendererDispatch = {
    .__reserved0 = NULL,
    .__reserved1 = NULL,
    .__reserved2 = NULL,
    .__reserved3 = NULL,
    ._free = BR_CMETHOD(br_renderer_vk, free),
    ._identifier = BR_CMETHOD(br_renderer_vk, identifier),
    ._type = BR_CMETHOD(br_renderer_vk, type),
    ._isType = BR_CMETHOD(br_renderer_vk, isType),
    ._device = BR_CMETHOD(br_renderer_vk, device),
    ._space = BR_CMETHOD(br_renderer_vk, space),
    ._templateQuery = BR_CMETHOD(br_renderer_vk, templateQuery),
    ._query = BR_CMETHOD(br_object, query),
    ._queryBuffer = BR_CMETHOD(br_object, queryBuffer),
    ._queryMany = BR_CMETHOD(br_object, queryMany),
    ._queryManySize = BR_CMETHOD(br_object, queryManySize),
    ._queryAll = BR_CMETHOD(br_object, queryAll),
    ._queryAllSize = BR_CMETHOD(br_object, queryAllSize),
    ._listQuery = BR_CMETHOD(br_renderer_vk, listQuery),
    ._tokensMatchBegin = BR_CMETHOD(br_object_container, tokensMatchBegin),
    ._tokensMatch = BR_CMETHOD(br_object_container, tokensMatch),
    ._tokensMatchEnd = BR_CMETHOD(br_object_container, tokensMatchEnd),
    ._addFront = BR_CMETHOD(br_object_container, addFront),
    ._removeFront = BR_CMETHOD(br_object_container, removeFront),
    ._remove = BR_CMETHOD(br_object_container, remove),
    ._find = BR_CMETHOD(br_object_container, find),
    ._findMany = BR_CMETHOD(br_object_container, findMany),
    ._count = BR_CMETHOD(br_object_container, count),

    ._validDestination = BR_CMETHOD(br_renderer_vk, validDestination),
    ._stateStoredNew = BR_CMETHOD(br_renderer_vk, stateStoredNew),
    ._stateStoredAvail = BR_CMETHOD(br_renderer_vk, stateStoredAvail),
    ._bufferStoredNew = BR_CMETHOD(br_renderer_vk, bufferStoredNew),
    ._bufferStoredAvail = BR_CMETHOD(br_renderer_vk, bufferStoredAvail),
    ._partSet = BR_CMETHOD(br_renderer_vk, partSet),
    ._partSetMany = BR_CMETHOD(br_renderer_vk, partSetMany),
    ._partQuery = BR_CMETHOD(br_renderer_vk, partQuery),
    ._partQueryBuffer = BR_CMETHOD(br_renderer_vk, partQueryBuffer),
    ._partQueryMany = BR_CMETHOD(br_renderer_vk, partQueryMany),
    ._partQueryManySize = BR_CMETHOD(br_renderer_vk, partQueryManySize),
    ._partQueryAll = BR_CMETHOD(br_renderer_vk, partQueryAll),
    ._partQueryAllSize = BR_CMETHOD(br_renderer_vk, partQueryAllSize),
    ._partIndexQuery = BR_CMETHOD(br_renderer_vk, partIndexQuery),
    ._modelMulF = BR_CMETHOD(br_renderer_vk, modelMul),
    ._modelPopPushMulF = BR_CMETHOD(br_renderer_vk, modelPopPushMul),
    ._modelInvert = BR_CMETHOD(br_renderer_vk, modelInvert),
    ._statePush = BR_CMETHOD(br_renderer_vk, statePush),
    ._statePop = BR_CMETHOD(br_renderer_vk, statePop),
    ._stateSave = BR_CMETHOD(br_renderer_vk, stateSave),
    ._stateRestore = BR_CMETHOD(br_renderer_vk, stateRestore),
    ._stateMask = BR_CMETHOD(br_renderer_vk, stateMask),
    ._stateDefault = BR_CMETHOD(br_renderer_vk, stateDefault),
    ._boundsTestF = BR_CMETHOD(br_renderer_vk, boundsTest),
    ._coverageTestF = BR_CMETHOD(br_renderer_vk, coverageTest),
    ._viewDistanceF = BR_CMETHOD(br_renderer_vk, viewDistance),
    ._commandModeSet = BR_CMETHOD(br_renderer_vk, commandModeSet),
    ._commandModeQuery = BR_CMETHOD(br_renderer_vk, commandModeQuery),
    ._commandModeDefault = BR_CMETHOD(br_renderer_vk, commandModeDefault),
    ._commandModePush = BR_CMETHOD(br_renderer_vk, commandModePush),
    ._commandModePop = BR_CMETHOD(br_renderer_vk, commandModePop),
    ._flush = BR_CMETHOD(br_renderer_vk, flush),
    ._synchronise = BR_CMETHOD(br_renderer_vk, synchronise),
    ._partQueryCapability = BR_CMETHOD(br_renderer_vk, partQueryCapability),
    ._stateQueryPerformance = BR_CMETHOD(br_renderer_vk, stateQueryPerformance),
    ._frameBegin = BR_CMETHOD(br_renderer_vk, frameBegin),
    ._frameEnd = BR_CMETHOD(br_renderer_vk, frameEnd),
    ._focusLossBegin = BR_CMETHOD(br_renderer_vk, focusLossBegin),
    ._focusLossEnd = BR_CMETHOD(br_renderer_vk, focusLossEnd),
    ._sceneBegin = BR_CMETHOD(br_renderer_vk, sceneBegin),
    ._sceneEnd = BR_CMETHOD(br_renderer_vk, sceneEnd),
};
