#ifndef _DEVICE_P_H_
#define _DEVICE_P_H_

/*
    This file was added by dethrace.
    Nasty hack to provide device-specific types to external consumers of BRender
*/

#include "compiler.h"
#include "pixelmap.h"

// For use with `virtualframebuffer` device.
typedef void br_device_virtualfb_swapbuffers_cbfn(br_pixelmap* pm);
typedef void br_device_virtualfb_palette_changed_cbfn(br_uint_32* palette_entries);

typedef struct br_device_virtualfb_callback_procs {
    br_device_virtualfb_palette_changed_cbfn      *palette_changed;
    br_device_virtualfb_swapbuffers_cbfn          *swap_buffers;
} br_device_virtualfb_callback_procs;

// For use with `glrend` device.
typedef void BR_CALLBACK br_device_gl_swapbuffers_cbfn(br_pixelmap* pm);
typedef void* BR_CALLBACK br_device_gl_getprocaddress_cbfn(const char* name);
typedef void BR_CALLBACK br_device_gl_free_cbfn(br_pixelmap* pm, void* user);
typedef void BR_CALLBACK br_device_gl_getviewport_cbfn(int *x, int *y, float *width_multiplier, float *height_multiplier);

typedef struct br_device_gl_callback_procs {
    br_device_gl_getprocaddress_cbfn      *get_proc_address;
    br_device_gl_getviewport_cbfn         *get_viewport;
    br_device_gl_swapbuffers_cbfn         *swap_buffers;
    br_device_gl_free_cbfn                *free;
} br_device_gl_callback_procs;

// For use with `sdl3rend` device.
typedef void BR_CALLBACK br_device_sdl3_swapbuffers_cbfn(br_pixelmap* pm);
typedef void* BR_CALLBACK br_device_sdl3_getprocaddress_cbfn(const char* name);
typedef void BR_CALLBACK br_device_sdl3_free_cbfn(br_pixelmap* pm, void* user);
typedef void BR_CALLBACK br_device_sdl3_getviewport_cbfn(int *x, int *y, float *width_multiplier, float *height_multiplier);
typedef int BR_CALLBACK br_device_sdl3_get_map_mode_cbfn(void);
typedef void BR_CALLBACK br_device_sdl3_get_window_size_cbfn(int *width, int *height);
// For use with `sdl3gpu` device: returns the SDL_Window* the driver should
// claim for the GPU device.
typedef void* BR_CALLBACK br_device_sdl3_get_window_cbfn(void);

typedef struct br_device_sdl3_callback_procs {
    br_device_sdl3_getprocaddress_cbfn             *get_proc_address;
    br_device_sdl3_getviewport_cbfn                *get_viewport;
    br_device_sdl3_swapbuffers_cbfn                *swap_buffers;
    br_device_sdl3_free_cbfn                       *free;
    br_device_sdl3_get_map_mode_cbfn               *get_map_mode;
    br_device_sdl3_get_window_size_cbfn            *get_window_size;
    br_device_sdl3_get_window_cbfn                 *get_window;
} br_device_sdl3_callback_procs;

#endif
