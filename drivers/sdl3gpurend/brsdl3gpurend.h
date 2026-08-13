#ifndef _BRSDL3GPUREND_H_
#define _BRSDL3GPUREND_H_

#ifndef _BRENDER_H_
#error Please include brender.h first
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * SDL3 GPU device info. Pointers refer to SDL3 objects:
 *   gpu_device               -> SDL_GPUDevice*
 *   window                   -> SDL_Window*
 *   swapchain_texture_format -> SDL_GPUTextureFormat (swapchain format)
 */
typedef struct SDL3GPUREND_DeviceInfo {
    void* gpu_device;
    void* window;
    uint32_t swapchain_texture_format;
} SDL3GPUREND_DeviceInfo;

void SDL3GPUREND_GetDeviceInfo(SDL3GPUREND_DeviceInfo* info);

/*
 * External render callback, invoked inside the driver's frame while a
 * command buffer is active. The first argument is the active
 * SDL_GPUCommandBuffer*; the second is the opaque user pointer.
 */
void SDL3GPUREND_SetExternalRenderCallback(void (*cb)(void* cmd, void* ud), void* ud);

#ifdef __cplusplus
}
#endif

/*
 * Main entry point for device.
 */
#ifndef _NO_PROTOTYPES
struct br_device *BR_EXPORT BrDrv1SDL3GPURENDBegin(const char *arguments);
#endif /* _NO_PROTOTYPES */

#endif /* _BRSDL3GPUREND_H_ */
