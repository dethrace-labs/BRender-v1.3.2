#ifndef _DRV_IP_H_
#define _DRV_IP_H_

#ifndef NO_PROTOTYPES

#ifdef __cplusplus
extern "C" {
#endif

/*
 * device.c
 */
br_device* DeviceVKAllocate(const char* identifier, const char* arguments);

/*
 * rendfcty.c
 */
br_renderer_facility* RendererFacilityVKInit(br_device* dev);

/*
 * outfcty.c
 */
br_output_facility* OutputFacilityVKInit(br_device* dev, br_renderer_facility* rendfcty);

/*
 * devpmvkf.c
 */
br_device_pixelmap* DevicePixelmapVKAllocateFront(br_device* dev, br_output_facility* outfcty, br_token_value* tv);

/*
 * renderer.c
 */
br_renderer* RendererVKAllocate(br_device* device, br_renderer_facility* facility, br_device_pixelmap* dest);

/*
 * sstate.c
 */
br_renderer_state_stored* RendererStateStoredVKAllocate(br_renderer* renderer, state_stack* base_state, br_uint_32 m,
    br_token_value* tv);

/*
 * state.c and friends
 */
void StateVKInit(state_all* state, void* res);
void StateVKInitMatrix(state_all* state);
void StateVKInitCull(state_all* state);
void StateVKInitClip(state_all* state);
void StateVKInitSurface(state_all* state);
void StateVKInitPrimitive(state_all* state);
void StateVKInitOutput(state_all* state);
void StateVKInitHidden(state_all* state);
void StateVKInitLight(state_all* state);

struct br_tv_template* StateVKGetStateTemplate(state_all* state, br_token part, br_int_32 index);

void StateVKReset(state_cache* cache);

br_boolean StateVKPush(state_all* state, uint32_t mask);
br_boolean StateVKPop(state_all* state, uint32_t mask);
void StateVKDefault(state_all* state, uint32_t mask);

void StateVKUpdateScene(state_cache* cache, state_stack* state);
void StateVKUpdateModel(state_cache* cache, state_matrix* matrix);
void StateVKCopy(state_stack* dst, state_stack* src, uint32_t mask);

/*
 * gstored.c
 */
br_geometry_stored* GeometryStoredVKAllocate(struct br_geometry_v1_model* gv1model, const char* id, struct br_renderer* r, struct v11model* model);
void StoredVKRenderGroup(struct br_geometry_stored* self, struct br_renderer* renderer, struct vk_groupinfo* groupinfo);

/*
 * cache.c
 */

/*
 * sbuffer.c
 */
struct br_buffer_stored* BufferStoredVKAllocate(struct br_renderer* renderer, br_token use, struct br_device_pixelmap* pm, br_token_value* tv);

/*
 * onscreen.c
 */
br_token VKOnScreenCheck(br_renderer* self, const br_matrix4* model_to_screen, const br_bounds3_f* bounds);

/*
 * ext_procs.c
 */
void* DevicePixelmapVKGetGetProcAddress(br_device_pixelmap* self);

void DevicePixelmapVKGetViewport(br_device_pixelmap* self, int *x, int *y, float *width_multiplier, float *height_multiplier);

void DevicePixelmapVKSwapBuffers(br_device_pixelmap* self);

void DevicePixelmapVKFree(br_device_pixelmap* self);

/*
 * devclut.c
 */
struct br_device_clut* DeviceClutVKAllocate(br_device* dev, char* identifier);

/*
 * Hijack nulldev's no-op implementations.
 */
br_geometry_lighting* GeometryLightingNullAllocate(br_renderer_facility* type, const char* id);
br_geometry_primitives* GeometryPrimitivesNullAllocate(br_renderer_facility* type, const char* id);
br_geometry_v1_model* GeometryV1ModelVKAllocate(br_renderer_facility* type, const char* id);

#ifdef __cplusplus
};
#endif

#endif
#endif /* _DRV_IP_H_ */
