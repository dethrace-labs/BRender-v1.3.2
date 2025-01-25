/*
 * Private device driver structure
 */
#ifndef _DRV_H_
#define _DRV_H_

#define BR_OBJECT_PRIVATE
#define BR_DEVICE_PRIVATE
#define BR_OUTPUT_FACILITY_PRIVATE
#define BR_DEVICE_PIXELMAP_PRIVATE
#define BR_DEVICE_CLUT_PRIVATE



#ifndef _BRDDI_H_
#include "brddi.h"
#endif

typedef void br_device_virtualfb_swapbuffers_cbfn(br_pixelmap* pm);
typedef void br_device_virtualfb_palette_changed_cbfn(br_uint_32* palette_entries);

typedef struct br_device_virtualfb_callback_procs {
    br_device_virtualfb_palette_changed_cbfn      *palette_changed;
    br_device_virtualfb_swapbuffers_cbfn          *swap_buffers;
} br_device_virtualfb_callback_procs;

#ifndef _TEMPLATE_H_
#include "template.h"
#endif

#ifndef _DEVICE_H_
#include "device.h"
#endif

#ifndef _OUTFCTY_H_
#include "outfcty.h"
#endif

#ifndef _DEVPIXMP_H_
#include "devpixmp.h"
#endif

#ifndef _DEVCLUT_H_
#include "devclut.h"
#endif

#ifndef _OBJECT_H_
#include "object.h"
#endif

/*
 * Macros that exapnd to the first two arguments of a template entry
 * Builtin or device token
 */
#define BRT(t) BRT_##t, 0
#define DEV(t) 0, #t

/*
 * Pull in private prototypes
 */
#ifndef _NO_PROTOTYPES

#ifndef _DRV_IP_H_
#include "drv_ip.h"
#endif

#endif


#ifdef __cplusplus
}
;
#endif
#endif
