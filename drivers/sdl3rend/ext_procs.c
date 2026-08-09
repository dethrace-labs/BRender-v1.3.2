#include "drv.h"
#include <brassert.h>

void* DevicePixelmapVKGetGetProcAddress(br_device_pixelmap* self) {
    UASSERT(self->use_type == BRT_NONE);
    return NULL;
}
