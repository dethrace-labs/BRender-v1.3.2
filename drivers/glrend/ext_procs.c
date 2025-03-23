#include "drv.h"
#include <brassert.h>

br_device_gl_getprocaddress_cbfn* DevicePixelmapGLGetGetProcAddress(br_device_pixelmap* self) {
    UASSERT(self->use_type == BRT_NONE);
    return self->asFront.callbacks.get_proc_address;
}

void DevicePixelmapGLGetViewport(br_device_pixelmap* self, int *x, int *y, float *width_multiplier, float *height_multiplier) {
    UASSERT(self->use_type == BRT_NONE);
    self->asFront.callbacks.get_viewport(x, y, width_multiplier, height_multiplier);
}

void DevicePixelmapGLSwapBuffers(br_device_pixelmap* self) {
    UASSERT(self->use_type == BRT_NONE);
    self->asFront.callbacks.swap_buffers((br_pixelmap*)self);
}

void DevicePixelmapGLFree(br_device_pixelmap* self) {
    UASSERT(self->use_type == BRT_NONE);
}
