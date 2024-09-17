#ifndef _BRGLREND_H_
#define _BRGLREND_H_

#ifndef _BRENDER_H_
#error Please include brender.h first
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern br_token BRT_OPENGL_TEXTURE_U32;

#ifdef __cplusplus
}
#endif

typedef void BR_CALLBACK br_device_pixelmap_gl_swapbuffers_cbfn(br_pixelmap *pm, void *user);

typedef void *BR_CALLBACK br_device_pixelmap_gl_getprocaddress_cbfn(const char *name);

typedef void BR_CALLBACK br_device_pixelmap_gl_free_cbfn(br_pixelmap *pm, void *user);

typedef struct br_device_gl_procs {
    br_device_pixelmap_gl_getprocaddress_cbfn      *get_proc_address;
    br_device_pixelmap_gl_swapbuffers_cbfn         *swap_buffers;
    br_device_pixelmap_gl_free_cbfn                *free;
    void                                           *user;

} br_device_gl_ext_procs;

/*
 * Function prototypes
 */
#ifndef _NO_PROTOTYPES

#ifndef _BRGLREND_P_H
#include "brglrend_p.h"
#endif

#endif /* _NO_PROTOTYPES */

#endif /* _BRGLREND_H_ */
