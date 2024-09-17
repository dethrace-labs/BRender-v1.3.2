#include "drv.h"
#include <brassert.h>

void DevicePixelmapGLExtSwapBuffers(br_device_pixelmap* self) {
    UASSERT(self->use_type == BRT_NONE);
    self->asFront.ext_procs.swap_buffers((br_pixelmap*)self, self->asFront.ext_procs.user);
}

br_device_pixelmap_gl_getprocaddress_cbfn* DevicePixelmapGLExtGetGetProcAddress(br_device_pixelmap* self) {
    UASSERT(self->use_type == BRT_NONE);
    return self->asFront.ext_procs.get_proc_address;
}

void DevicePixelmapGLExtFree(br_device_pixelmap* self) {
    UASSERT(self->use_type == BRT_NONE);
    if (self->asFront.ext_procs.free != NULL)
        self->asFront.ext_procs.free((br_pixelmap*)self, self->asFront.ext_procs.user);
}
