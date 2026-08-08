#include "brassert.h"
#include "drv.h"
#include "formats.h"
#include <string.h>

static const struct br_geometry_stored_dispatch geometryStoredDispatch;

#define F(f) offsetof(br_geometry_stored, f)

static struct br_tv_template_entry templateEntries[] = {
    { BRT(IDENTIFIER_CSTR), F(identifier), BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY },
    { BRT(GEOMETRY_V1_MODEL), F(gv1model), BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY },
    { BRT(SHARED_B), F(shared), BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY },
};
#undef F

#define VK_DYN_SMALL_MAX_VERTEX_BYTES (16u * 1024u)
#define VK_DYN_SMALL_MAX_INDEX_BYTES (16u * 1024u)

static vk_vertex_f* GenerateVBOData(const struct v11model* model, size_t total_vertices) {
    vk_vertex_f* vtx = BrScratchAllocate(total_vertices * sizeof(vk_vertex_f));
    vk_vertex_f* next = vtx;

    for (br_uint_16 i = 0; i < model->ngroups; ++i) {
        const struct v11group* gp = model->groups + i;
        for (br_uint_16 v = 0; v < gp->nvertices; ++v, ++next) {
            next->p = *(br_vector3_f*)(gp->position + v);
            next->map = *(br_vector2_f*)(gp->map + v);
            next->n = *(br_vector3_f*)(gp->normal + v);
            next->c.v[0] = BR_RED(gp->vertex_colours[v]) / 255.0f;
            next->c.v[1] = BR_GRN(gp->vertex_colours[v]) / 255.0f;
            next->c.v[2] = BR_BLU(gp->vertex_colours[v]) / 255.0f;
            next->c.v[3] = BR_ALPHA(gp->vertex_colours[v]) / 255.0f;
        }
    }
    return vtx;
}

static br_uint_16* GenerateIBOData(const struct v11model* model, size_t total_faces, br_size_t* out_size, vk_groupinfo* vk_groups) {
    br_uint_16* idx = BrScratchAllocate(total_faces * 3 * sizeof(br_uint_16));
    br_uint_16* next = idx;
    br_uint_16 offset = 0;
    br_size_t face_offset = 0;

    for (br_uint_16 i = 0; i < model->ngroups; ++i) {
        const struct v11group* gp = model->groups + i;
        if (vk_groups != NULL) {
            vk_groups[i].count = gp->nfaces * 3;
            vk_groups[i].offset = (uint32_t)(face_offset);
            vk_groups[i].group = model->groups + i;
            vk_groups[i].stored = NULL;
        }

        for (br_uint_16 f = 0; f < gp->nfaces; ++f) {
            const br_vector3_u16* fp = gp->vertex_numbers + f;
            *next++ = fp->v[0] + offset;
            *next++ = fp->v[1] + offset;
            *next++ = fp->v[2] + offset;
        }

        face_offset += (br_size_t)gp->nfaces * 3 * sizeof(br_uint_16);
        offset += model->groups[i].nvertices;
    }

    *out_size = face_offset;
    return idx;
}

static void UploadVBOToDedicated(HVIDEO hVideo, br_geometry_stored* self, const vk_vertex_f* vtx, VkDeviceSize size) {
    self->inDynamicRing = 0;
    self->vboOffset = 0;

    VkBufferCreateInfo bi = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(hVideo->device, &bi, NULL, &self->vbo);

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(hVideo->device, self->vbo, &memReq);

    VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = memReq.size;
    ai.memoryTypeIndex = hVideo->hostMemType;

    vkAllocateMemory(hVideo->device, &ai, NULL, &self->vboMemory);
    vkBindBufferMemory(hVideo->device, self->vbo, self->vboMemory, 0);

    void* data;
    vkMapMemory(hVideo->device, self->vboMemory, 0, size, 0, &data);
    memcpy(data, vtx, size);
    vkUnmapMemory(hVideo->device, self->vboMemory);
}

static void UploadIBOToDedicated(HVIDEO hVideo, br_geometry_stored* self, const br_uint_16* idx, VkDeviceSize size) {
    self->iboOffset = 0;

    VkBufferCreateInfo bi = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(hVideo->device, &bi, NULL, &self->ibo);

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(hVideo->device, self->ibo, &memReq);

    VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = memReq.size;
    ai.memoryTypeIndex = hVideo->hostMemType;

    vkAllocateMemory(hVideo->device, &ai, NULL, &self->iboMemory);
    vkBindBufferMemory(hVideo->device, self->ibo, self->iboMemory, 0);

    void* data;
    vkMapMemory(hVideo->device, self->iboMemory, 0, size, 0, &data);
    memcpy(data, idx, size);
    vkUnmapMemory(hVideo->device, self->iboMemory);
}

static void build_vbo(HVIDEO hVideo, br_geometry_stored* self, const struct v11model* model, size_t total_vertices) {
    vk_vertex_f* vtx = GenerateVBOData(model, total_vertices);

    VkDeviceSize size = total_vertices * sizeof(vk_vertex_f);
    int f = hVideo->currentFrame;

    /* Small models rebuilt during frame recording (electro-ray segments, sparks,
     * dim quads, pratcam quad) are sub-allocated from the persistent ring: a plain
     * memcpy, no vkCreateBuffer/vkAllocateMemory/vkBindBufferMemory per rebuild.
     * The ring cursor only advances within a frame (no wrap), so every build gets
     * its own slot; it resets next frame after the fence wait. Models built
     * outside recording (load time) and oversized models keep dedicated buffers.
     * Ring data is only valid until the next frame's reset, so ringEpoch stamps
     * the frame and a stale model is re-uploaded at render time. */
    if (hVideo->isRecording && hVideo->dynVboMapped[f] != NULL &&
        size <= VK_DYN_SMALL_MAX_VERTEX_BYTES &&
        hVideo->dynVboOffset[f] + size <= hVideo->dynVboCapacity) {
        memcpy((char*)hVideo->dynVboMapped[f] + hVideo->dynVboOffset[f], vtx, size);
        self->vbo = hVideo->dynVbo[f];
        self->vboMemory = VK_NULL_HANDLE;
        self->vboOffset = hVideo->dynVboOffset[f];
        self->inDynamicRing = 1;
        self->ringEpoch = hVideo->frameEpoch;
        hVideo->dynVboOffset[f] += size;
    } else {
        UploadVBOToDedicated(hVideo, self, vtx, size);
    }

    BrScratchFree(vtx);
}

static void build_ibo(HVIDEO hVideo, br_geometry_stored* self, const struct v11model* model, size_t total_faces) {
    br_size_t size = 0;
    br_uint_16* idx = GenerateIBOData(model, total_faces, &size, self->vk_groups);
    int f = hVideo->currentFrame;

    if (hVideo->isRecording && hVideo->dynIboMapped[f] != NULL &&
        size <= VK_DYN_SMALL_MAX_INDEX_BYTES &&
        hVideo->dynIboOffset[f] + size <= hVideo->dynIboCapacity) {
        memcpy((char*)hVideo->dynIboMapped[f] + hVideo->dynIboOffset[f], idx, size);
        self->ibo = hVideo->dynIbo[f];
        self->iboMemory = VK_NULL_HANDLE;
        self->iboOffset = hVideo->dynIboOffset[f];
        self->inDynamicRing = 1;
        self->ringEpoch = hVideo->frameEpoch;
        hVideo->dynIboOffset[f] += size;
    } else {
        UploadIBOToDedicated(hVideo, self, idx, size);
    }

    BrScratchFree(idx);
}

/* Re-uploads a stale ring sub-allocation into the current frame's ring slot.
 * The ring cursors are reset every frame in VK_EnsureRecording, so any ring
 * model that is not rebuilt this frame (built during a previous frame's
 * recording and persisted) references clobbered data. This regenerates the
 * vertex/index data from the v11model and copies it into the current slot,
 * updating the bind offsets. Falls back to a dedicated buffer if the ring is
 * full, permanently graduating the model out of the ring. Only the components
 * actually in the ring (vboMemory/iboMemory == NULL) are refreshed; a mixed
 * vbo-in-ring/ibo-dedicated model keeps its dedicated component. */
void VK_RefreshRingStored(HVIDEO hVideo, br_geometry_stored* self) {
    int f = hVideo->currentFrame;

    size_t total_vertices = 0, total_faces = 0;
    for (br_uint_16 i = 0; i < self->model->ngroups; ++i) {
        total_vertices += self->model->groups[i].nvertices;
        total_faces += self->model->groups[i].nfaces;
    }

    if (self->vboMemory == VK_NULL_HANDLE) {
        vk_vertex_f* vtx = GenerateVBOData(self->model, total_vertices);
        VkDeviceSize size = total_vertices * sizeof(vk_vertex_f);
        if (hVideo->dynVboMapped[f] != NULL &&
            hVideo->dynVboOffset[f] + size <= hVideo->dynVboCapacity) {
            memcpy((char*)hVideo->dynVboMapped[f] + hVideo->dynVboOffset[f], vtx, size);
            self->vbo = hVideo->dynVbo[f];
            self->vboOffset = hVideo->dynVboOffset[f];
            hVideo->dynVboOffset[f] += size;
        } else {
            UploadVBOToDedicated(hVideo, self, vtx, size);
        }
        BrScratchFree(vtx);
    }

    if (self->iboMemory == VK_NULL_HANDLE) {
        br_size_t size = 0;
        br_uint_16* idx = GenerateIBOData(self->model, total_faces, &size, NULL);
        if (hVideo->dynIboMapped[f] != NULL &&
            hVideo->dynIboOffset[f] + size <= hVideo->dynIboCapacity) {
            memcpy((char*)hVideo->dynIboMapped[f] + hVideo->dynIboOffset[f], idx, size);
            self->ibo = hVideo->dynIbo[f];
            self->iboOffset = hVideo->dynIboOffset[f];
            hVideo->dynIboOffset[f] += size;
        } else {
            UploadIBOToDedicated(hVideo, self, idx, size);
        }
        BrScratchFree(idx);
    }

    self->ringEpoch = hVideo->frameEpoch;
}

br_geometry_stored* GeometryStoredVKAllocate(br_geometry_v1_model* gv1model, const char* id, br_renderer* r, struct v11model* model) {
    br_geometry_stored* self;
    br_device_pixelmap* screen = r->pixelmap->screen;
    HVIDEO hVideo = &screen->asFront.video;

    self = BrResAllocate(gv1model->renderer_facility->object_list, sizeof(*self), BR_MEMORY_OBJECT);
    self->dispatch = &geometryStoredDispatch;
    self->identifier = id;
    self->device = gv1model->device;
    self->gv1model = gv1model;

    ObjectContainerAddFront(gv1model->renderer_facility, (br_object*)self);

    self->model = model;
    self->shared = BR_TRUE;
    self->deviceHandle = hVideo->device;
    self->hVideo = hVideo;

    size_t total_vertices = 0, total_faces = 0;
    for (br_uint_16 i = 0; i < model->ngroups; ++i) {
        total_vertices += model->groups[i].nvertices;
        total_faces += model->groups[i].nfaces;
    }

    self->num_groups = model->ngroups;
    self->vk_groups = BrResAllocate(gv1model, sizeof(vk_groupinfo) * model->ngroups, BR_MEMORY_OBJECT_DATA);

    build_vbo(hVideo, self, model, total_vertices);
    build_ibo(hVideo, self, model, total_faces);

    return (br_geometry_stored*)self;
}

static void BR_CMETHOD(br_geometry_stored_vk, free)(br_object* _self) {
    br_geometry_stored* self = (br_geometry_stored*)_self;

    ObjectContainerRemove(self->gv1model->renderer_facility, (br_object*)self);

    // Defer buffer destruction to next frame's sceneBegin (after fence wait).
    // BrModelRemove can be called while command buffers still reference these buffers
    // (e.g. shadow rendering: BrModelAdd → render → BrModelRemove within same frame).
    // VK's vkDestroyBuffer is immediate (unlike GL's glDeleteBuffers which is deferred),
    // so we must wait until the GPU is done with the command buffer before destroying.
    // Ring sub-allocations are detected by their NULL memory handle: they share the
    // video context's ring buffers (freed with the frame-slot reset) and are not
    // owned here, so they are skipped. Each buffer is handled independently because
    // build_vbo/build_ibo decide ring membership separately.
    HVIDEO hVideo = self->hVideo;
    if (hVideo != NULL) {
        uint32_t f = hVideo->currentFrame;
        for (int i = 0; i < 2; i++) {
            VkBuffer buffer = i == 0 ? self->vbo : self->ibo;
            VkDeviceMemory memory = i == 0 ? self->vboMemory : self->iboMemory;
            if (memory == VK_NULL_HANDLE)
                continue;
            if (hVideo->deferredBufferFreeCount[f] + 1 > hVideo->deferredBufferFreeCapacity[f]) {
                uint32_t newCap = hVideo->deferredBufferFreeCapacity[f] ? hVideo->deferredBufferFreeCapacity[f] * 2 : 64;
                hVideo->deferredBufferFrees[f] = BrResAllocate(hVideo->res, newCap * sizeof(VK_DeferredBufferFree), BR_MEMORY_OBJECT_DATA);
                hVideo->deferredBufferFreeCapacity[f] = newCap;
            }
            hVideo->deferredBufferFrees[f][hVideo->deferredBufferFreeCount[f]].buffer = buffer;
            hVideo->deferredBufferFrees[f][hVideo->deferredBufferFreeCount[f]].memory = memory;
            hVideo->deferredBufferFreeCount[f]++;
        }
        self->vbo = VK_NULL_HANDLE;
        self->vboMemory = VK_NULL_HANDLE;
        self->ibo = VK_NULL_HANDLE;
        self->iboMemory = VK_NULL_HANDLE;
    } else if (self->deviceHandle != VK_NULL_HANDLE) {
        if (self->vboMemory != VK_NULL_HANDLE) vkDestroyBuffer(self->deviceHandle, self->vbo, NULL);
        if (self->vboMemory != VK_NULL_HANDLE) vkFreeMemory(self->deviceHandle, self->vboMemory, NULL);
        if (self->iboMemory != VK_NULL_HANDLE) vkDestroyBuffer(self->deviceHandle, self->ibo, NULL);
        if (self->iboMemory != VK_NULL_HANDLE) vkFreeMemory(self->deviceHandle, self->iboMemory, NULL);
    }

    BrResFreeNoCallback(self);
}

static char* BR_CMETHOD(br_geometry_stored_vk, identifier)(br_object* self) {
    return (char*)((br_geometry_stored*)self)->identifier;
}

static br_device* BR_CMETHOD(br_geometry_stored_vk, device)(br_object* self) {
    return ((br_geometry_stored*)self)->device;
}

static br_token BR_CMETHOD(br_geometry_stored_vk, type)(br_object* self) {
    (void)self;
    return BRT_GEOMETRY_STORED;
}

static br_boolean BR_CMETHOD(br_geometry_stored_vk, isType)(br_object* self, br_token t) {
    (void)self;
    return (t == BRT_GEOMETRY_STORED) || (t == BRT_GEOMETRY) || (t == BRT_OBJECT);
}

static br_int_32 BR_CMETHOD(br_geometry_stored_vk, space)(br_object* self) {
    (void)self;
    return (br_int_32)sizeof(br_geometry_stored);
}

static struct br_tv_template* BR_CMETHOD(br_geometry_stored_vk, templateQuery)(br_object* _self) {
    br_geometry_stored* self = (br_geometry_stored*)_self;

    if (self->device->templates.geometryStoredTemplate == NULL) {
        self->device->templates.geometryStoredTemplate = BrTVTemplateAllocate(self->device, templateEntries,
            BR_ASIZE(templateEntries));
    }

    return self->device->templates.geometryStoredTemplate;
}

static br_error VKModel_RenderStored(br_geometry_stored* self, br_renderer* renderer, struct br_renderer_state_stored* default_state) {
    for (int i = 0; i < self->model->ngroups; ++i) {
        vk_groupinfo* groupinfo = self->vk_groups + i;
        struct v11group* group = self->model->groups + i;
        br_renderer_state_stored* stored = (br_renderer_state_stored*)group->stored;

        groupinfo->stored = stored;
        groupinfo->default_state = default_state;

        if (stored) {
            StateVKCopy(renderer->state.current, &stored->state, MASK_STATE_SURFACE | MASK_STATE_PRIMITIVE | MASK_STATE_CULL);
        } else {
            renderer->state.current->surface = default_state->state.surface;
            renderer->state.current->prim = default_state->state.prim;
            renderer->state.current->cull = default_state->state.cull;
        }

        StoredVKRenderGroup(self, renderer, groupinfo);
    }
    renderer->frame_stats.model_count++;
    return BRE_OK;
}

static br_error BR_CMETHOD(br_geometry_stored_vk, render)(br_geometry_stored* self, br_renderer* renderer, struct br_renderer_state_stored* default_state) {
    return VKModel_RenderStored(self, renderer, default_state);
}

static const struct br_geometry_stored_dispatch geometryStoredDispatch = {
    .__reserved0 = NULL,
    .__reserved1 = NULL,
    .__reserved2 = NULL,
    .__reserved3 = NULL,
    ._free = BR_CMETHOD(br_geometry_stored_vk, free),
    ._identifier = BR_CMETHOD(br_geometry_stored_vk, identifier),
    ._type = BR_CMETHOD(br_geometry_stored_vk, type),
    ._isType = BR_CMETHOD(br_geometry_stored_vk, isType),
    ._device = BR_CMETHOD(br_geometry_stored_vk, device),
    ._space = BR_CMETHOD(br_geometry_stored_vk, space),

    ._templateQuery = BR_CMETHOD(br_geometry_stored_vk, templateQuery),
    ._query = BR_CMETHOD(br_object, query),
    ._queryBuffer = BR_CMETHOD(br_object, queryBuffer),
    ._queryMany = BR_CMETHOD(br_object, queryMany),
    ._queryManySize = BR_CMETHOD(br_object, queryManySize),
    ._queryAll = BR_CMETHOD(br_object, queryAll),
    ._queryAllSize = BR_CMETHOD(br_object, queryAllSize),

    ._render = BR_CMETHOD(br_geometry_stored_vk, render),
    ._renderOnScreen = BR_CMETHOD(br_geometry_stored_vk, render),
};
