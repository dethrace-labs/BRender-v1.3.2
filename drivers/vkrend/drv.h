#ifndef _DRV_H_
#define _DRV_H_

#if defined(_MSC_VER) && _MSC_VER <= 1929
#define alignas(X) _Alignas(X)
#else
#include <stdalign.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>

#include <vulkan/vulkan.h>

#define BR_DEVICE_PRIVATE
#define BR_OUTPUT_FACILITY_PRIVATE
#define BR_DEVICE_PIXELMAP_PRIVATE
#define BR_RENDERER_FACILITY_PRIVATE
#define BR_BUFFER_STORED_PRIVATE
#define BR_GEOMETRY_V1_MODEL_PRIVATE
#define BR_GEOMETRY_STORED_PRIVATE
#define BR_RENDERER_STATE_STORED_PRIVATE
#define BR_RENDERER_PRIVATE
#define BR_DEVICE_CLUT_PRIVATE

#include "brddi.h"
#include "brvkrend.h"

#include "formats.h"
#include "pm.h"
#include "video.h"
#include "state.h"

#include "template.h"
#include "device.h"
#include "outfcty.h"
#include "devpixmp.h"
#include "rendfcty.h"
#include "renderer.h"
#include "sstate.h"
#include "sbuffer.h"
#include "gstored.h"
#include "gv1model.h"
#include "devclut.h"
#include "vkassert.h"

#define BRT(t) BRT_##t, 0
#define DEV(t) 0, #t

#ifndef _NO_PROTOTYPES

#include "drv_ip.h"

#endif

#ifdef __cplusplus
};
#endif
#endif /* _DRV_H_ */
