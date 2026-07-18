#ifndef _BRVKREND_H_
#define _BRVKREND_H_

#ifndef _BRENDER_H_
#error Please include brender.h first
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VKREND_DeviceInfo {
    void* instance;
    void* physical_device;
    void* device;
    void* graphics_queue;
    uint32_t graphics_queue_family;
    void* render_pass;
    uint32_t min_image_count;
    uint32_t image_count;
} VKREND_DeviceInfo;

void VKREND_GetDeviceInfo(VKREND_DeviceInfo* info);

void VKREND_SetExternalRenderCallback(void (*cb)(void* cmd, void* ud), void* ud);

#ifdef __cplusplus
}
#endif

#ifndef _NO_PROTOTYPES

#ifndef _BRVKREND_P_H
#include "brvkrend_p.h"
#endif

#endif /* _NO_PROTOTYPES */

#endif /* _BRVKREND_H_ */
