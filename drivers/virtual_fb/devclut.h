/*
 * Private device CLUT state
 */
#ifndef _DEVCLUT_H_
#define _DEVCLUT_H_

#include "drv.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void BR_CALLBACK br_device_virtualfb_palette_changed_cbfn(br_uint_32* palette_entries);

/*
 * Private state of device CLUT
 */
typedef struct br_device_clut {
    /*
     * Dispatch table
     */
    struct br_device_clut_dispatch* dispatch;

    /*
     * Standard handle identifier
     */
    char* identifier;

    /*
     * Device pointer
     */
    br_device* device;

    br_uint_32 entries[256];

    br_device_virtualfb_palette_changed_cbfn* palette_changed_cbfn;

} br_device_clut;

#ifdef __cplusplus
};
#endif
#endif
