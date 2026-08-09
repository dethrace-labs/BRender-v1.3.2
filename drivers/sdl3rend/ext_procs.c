#include "drv.h"
#include <brassert.h>

void* DevicePixelmapSDL3RENDGetGetProcAddress(br_device_pixelmap* self) {
    UASSERT(self->use_type == BRT_NONE);
    return (void*)self->asFront.callbacks.get_proc_address;
}

void DevicePixelmapSDL3RENDGetViewport(br_device_pixelmap* self, int *x, int *y, float *width_multiplier, float *height_multiplier) {
    UASSERT(self->use_type == BRT_NONE);
    self->asFront.callbacks.get_viewport(x, y, width_multiplier, height_multiplier);
}

void DevicePixelmapSDL3RENDSwapBuffers(br_device_pixelmap* self) {
    UASSERT(self->use_type == BRT_NONE);
    self->asFront.callbacks.swap_buffers((br_pixelmap*)self);
}

void DevicePixelmapSDL3RENDFree(br_device_pixelmap* self) {
    UASSERT(self->use_type == BRT_NONE);
}
