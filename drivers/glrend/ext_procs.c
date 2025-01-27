#include "drv.h"
#include <brassert.h>

br_device_gl_getprocaddress_cbfn* DevicePixelmapGLGetGetProcAddress(br_device_pixelmap* self) {
    UASSERT(self->use_type == BRT_NONE);
    return self->asFront.callbacks.get_proc_address;
}

void DevicePixelmapGLGetViewport(br_device_pixelmap* self, int *x, int *y, int *width, int *height) {
    UASSERT(self->use_type == BRT_NONE);
    self->asFront.callbacks.get_viewport(x, y, width, height);
}

void DevicePixelmapGLSwapBuffers(br_device_pixelmap* self) {
    UASSERT(self->use_type == BRT_NONE);
    self->asFront.callbacks.swap_buffers((br_pixelmap*)self);
}

void DevicePixelmapGLFree(br_device_pixelmap* self) {
    UASSERT(self->use_type == BRT_NONE);
}
