#include "drv.h"
#include <brassert.h>

void DevicePixelmapGLExtSwapBuffers(br_device_pixelmap* self) {
    UASSERT(self->use_type == BRT_NONE);
    self->asFront.swap_buffers((br_pixelmap*)self);
}

br_device_pixelmap_gl_getprocaddress_cbfn* DevicePixelmapGLExtGetGetProcAddress(br_device_pixelmap* self) {
    UASSERT(self->use_type == BRT_NONE);
    return self->asFront.get_proc_address;
}

void DevicePixelmapGLExtFree(br_device_pixelmap* self) {
    UASSERT(self->use_type == BRT_NONE);
}
