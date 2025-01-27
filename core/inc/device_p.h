#ifndef _DEVICE_P_H_
#define _DEVICE_P_H_

#include "compiler.h"
#include "pixelmap.h"

typedef void br_device_virtualfb_swapbuffers_cbfn(br_pixelmap* pm);
typedef void br_device_virtualfb_palette_changed_cbfn(br_uint_32* palette_entries);

typedef struct br_device_virtualfb_callback_procs {
    br_device_virtualfb_palette_changed_cbfn      *palette_changed;
    br_device_virtualfb_swapbuffers_cbfn          *swap_buffers;
} br_device_virtualfb_callback_procs;

typedef void BR_CALLBACK br_device_pixelmap_gl_swapbuffers_cbfn(br_pixelmap* pm);
typedef void* BR_CALLBACK br_device_pixelmap_gl_getprocaddress_cbfn(const char* name);
typedef void BR_CALLBACK br_device_pixelmap_gl_free_cbfn(br_pixelmap* pm, void* user);
typedef void BR_CALLBACK br_device_pixelmap_gl_getviewport_cbfn(int *x, int *y, int *width, int *height);

typedef struct br_device_gl_callback_procs {
    br_device_pixelmap_gl_getprocaddress_cbfn      *get_proc_address;
    br_device_pixelmap_gl_getviewport_cbfn         *get_viewport;
    br_device_pixelmap_gl_swapbuffers_cbfn         *swap_buffers;
    br_device_pixelmap_gl_free_cbfn                *free;
} br_device_gl_callback_procs;

#endif
