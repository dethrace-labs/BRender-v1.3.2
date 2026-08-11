#ifndef _DRV_IP_H_
#define _DRV_IP_H_

#include "commonrend.h"

#ifndef NO_PROTOTYPES

#ifdef __cplusplus
extern "C" {
#endif

/*
 * device.c
 */
br_device* DeviceSDL3RENDAllocate(const char* identifier, const char* arguments);

/*
 * rendfcty.c
 */
br_renderer_facility* RendererFacilitySDL3RENDInit(br_device* dev);

/*
 * outfcty.c
 */
br_output_facility* OutputFacilitySDL3RENDInit(br_device* dev, br_renderer_facility* rendfcty);

/*
 * devpmgpu.c
 */
br_device_pixelmap* DevicePixelmapSDL3RENDAllocateFront(br_device* dev, br_output_facility* outfcty, br_token_value* tv);

/*
 * renderer.c
 */
br_renderer* RendererSDL3RENDAllocate(br_device* device, br_renderer_facility* facility, br_device_pixelmap* dest);

/*
 * sstate.c
 */
br_renderer_state_stored* RendererStateStoredSDL3RENDAllocate(br_renderer* renderer, state_stack* base_state, br_uint_32 m,
    br_token_value* tv);

/*
 * state.c and friends
 */
void StateSDL3RENDInit(state_all* state, void* res);
void StateSDL3RENDInitMatrix(state_all* state);
void StateSDL3RENDInitCull(state_all* state);
void StateSDL3RENDInitClip(state_all* state);
void StateSDL3RENDInitSurface(state_all* state);
void StateSDL3RENDInitPrimitive(state_all* state);
void StateSDL3RENDInitOutput(state_all* state);
void StateSDL3RENDInitHidden(state_all* state);
void StateSDL3RENDInitLight(state_all* state);

struct br_tv_template* StateSDL3RENDGetStateTemplate(state_all* state, br_token part, br_int_32 index);

void StateSDL3RENDReset(state_cache* cache);

br_boolean StateSDL3RENDPush(state_all* state, uint32_t mask);
br_boolean StateSDL3RENDPop(state_all* state, uint32_t mask);
void StateSDL3RENDDefault(state_all* state, uint32_t mask);

void StateSDL3RENDUpdateScene(state_cache* cache, state_stack* state);
void StateSDL3RENDUpdateModel(state_cache* cache, state_matrix* matrix);
void StateSDL3RENDFillModel(state_stack* state, uint32_t states, shader_data_model* model);
void StateSDL3RENDCopy(state_stack* dst, state_stack* src, uint32_t mask);

/*
 * v1model.c
 *
 * Applies the current material state into `model` (the shader UBO payload)
 * and, when a texture is active, fills `texture`/`sampler` with the SDL3 GPU
 * texture/sampler to bind on the fragment sampler slot used by the draw.
 * The draw tail (modelrender.c) uploads the UBO payload and issues the draw.
 */
void StoredSDL3RENDApplyProperties(HVIDEO hVideo, state_stack* state, uint32_t states, shader_data_model* model, br_buffer_stored* default_texture, struct SDL_GPUTexture** texture, struct SDL_GPUSampler** sampler);
br_boolean SDL3REND_ComputeScreenAABB(const br_matrix4* mvp, struct v11group* gp, HVIDEO hVideo, br_device_pixelmap* colour_target, br_rectangle* out);

/*
 * gstored.c
 */
br_geometry_stored* GeometryStoredSDL3RENDAllocate(struct br_geometry_v1_model* gv1model, const char* id, struct br_renderer* r, struct v11model* model);
void StoredSDL3RENDRenderGroup(struct br_geometry_stored* self, struct br_renderer* renderer, struct sdl3_groupinfo* groupinfo);

/*
 * cache.c
 */

/*
 * sbuffer.c
 */
struct br_buffer_stored* BufferStoredSDL3RENDAllocate(struct br_renderer* renderer, br_token use, struct br_device_pixelmap* pm, br_token_value* tv);

/*
 * onscreen.c
 */
br_token SDL3RENDOnScreenCheck(br_renderer* self, const br_matrix4* model_to_screen, const br_bounds3_f* bounds);

/*
 * ext_procs.c
 */
void* BREND_FN(DevicePixelmap, GetGetProcAddress)(br_device_pixelmap* self);

void BREND_FN(DevicePixelmap, GetViewport)(br_device_pixelmap* self, int *x, int *y, float *width_multiplier, float *height_multiplier);

void BREND_FN(DevicePixelmap, SwapBuffers)(br_device_pixelmap* self);

void BREND_FN(DevicePixelmap, Free)(br_device_pixelmap* self);

/*
 * devclut.c
 */
struct br_device_clut* DeviceClutSDL3RENDAllocate(br_device* dev, char* identifier);

/*
 * Hijack nulldev's no-op implementations.
 */
br_geometry_lighting* GeometryLightingNullAllocate(br_renderer_facility* type, const char* id);
br_geometry_primitives* GeometryPrimitivesNullAllocate(br_renderer_facility* type, const char* id);
br_geometry_v1_model* BREND_FN(GeometryV1Model, Allocate)(br_renderer_facility* type, const char* id);

#ifdef __cplusplus
};
#endif

#endif
#endif /* _DRV_IP_H_ */
