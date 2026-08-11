#ifndef _DEVPIXMP_H_
#define _DEVPIXMP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

typedef struct {
    float x, y, z;
    float r, g, b;
    float u, v;
} br_device_pixelmap_sdl3_tri;

typedef struct br_device_pixelmap_sdl3_quad {
    br_device_pixelmap_sdl3_tri tris[4];
    SDL_GPUGraphicsPipeline* pipeline;
    SDL_GPUBuffer* buffer;
} br_device_pixelmap_sdl3_quad;

#ifdef BR_DEVICE_PIXELMAP_PRIVATE

typedef struct br_device_pixelmap {
    const struct br_device_pixelmap_dispatch* dispatch;
    const char* pm_identifier;
    BR_PIXELMAP_MEMBERS
    struct br_device* device;
    struct br_output_facility* output_facility;
    br_token use_type;
    br_int_32 msaa_samples;
    struct br_renderer* renderer;
    struct br_device_pixelmap* screen;
    br_uint_16 parent_height;
    br_boolean sub_pixelmap;

    union {
        struct {
            br_device_sdl3_callback_procs callbacks;
            VIDEO video;
            void* sdl3_context;
            const char* sdl3_version;
            const char* sdl3_vendor;
            const char* sdl3_renderer;
            SDL_GPUDevice* sdl3_device;
            SDL_Window* sdl3_window;
            br_int_32 num_refs;
        } asFront;
        struct {
            struct br_device_pixelmap* depthbuffer;
            void* lockedPixels;
            int possiblyDirty;
            int locked;
        } asBack;
        struct {
            struct br_device_pixelmap* backbuffer;
        } asDepth;
    };
    struct br_device_clut* clut;
} br_device_pixelmap;

/* Forward declarations for br_device_pixelmap_sdl3rend methods defined in
 * commonrend/devpixmp.c (shared with the front dispatch in devpmgpu.c). */
extern void BR_CMETHOD_DECL(br_device_pixelmap_sdl3rend, free)(br_object* _self);
extern const char* BR_CMETHOD_DECL(br_device_pixelmap_sdl3rend, identifier)(br_object* self);
extern br_token BR_CMETHOD_DECL(br_device_pixelmap_sdl3rend, type)(br_object* self);
extern br_boolean BR_CMETHOD_DECL(br_device_pixelmap_sdl3rend, isType)(br_object* self, br_token t);
extern br_device* BR_CMETHOD_DECL(br_device_pixelmap_sdl3rend, device)(br_object* self);
extern br_size_t BR_CMETHOD_DECL(br_device_pixelmap_sdl3rend, space)(br_object* self);
extern struct br_tv_template* BR_CMETHOD_DECL(br_device_pixelmap_sdl3rend, templateQuery)(br_object* _self);
extern br_error BR_CMETHOD_DECL(br_device_pixelmap_sdl3rend, resize)(br_device_pixelmap* self, br_int_32 width, br_int_32 height);
extern br_error BR_CMETHOD_DECL(br_device_pixelmap_sdl3rend, match)(br_device_pixelmap* self, br_device_pixelmap** newpm, br_token_value* tv);
extern br_error BR_CMETHOD_DECL(br_device_pixelmap_sdl3rend, rectangleStretchCopy)(br_device_pixelmap* self, br_rectangle* d, br_device_pixelmap* src, br_rectangle* s);
extern br_error BR_CMETHOD_DECL(br_device_pixelmap_sdl3rend, rectangleCopy)(br_device_pixelmap* self, br_point* p, br_device_pixelmap* src, br_rectangle* sr);
extern br_error BR_CMETHOD_DECL(br_device_pixelmap_sdl3rend, rectangleCopyTo)(br_device_pixelmap* self, br_point* p, br_device_pixelmap* src, br_rectangle* sr);
extern br_error BR_CMETHOD_DECL(br_device_pixelmap_sdl3rend, rectangleFill)(br_device_pixelmap* self, br_rectangle* rect, br_uint_32 colour);
extern br_error BR_CMETHOD_DECL(br_device_pixelmap_sdl3rend, fill)(br_device_pixelmap* self, br_uint_32 colour);
extern br_error BR_CMETHOD_DECL(br_device_pixelmap_sdl3rend, allocateSub)(br_device_pixelmap* self, br_device_pixelmap** newpm, br_rectangle* rect);
extern br_error BR_CMETHOD_DECL(br_device_pixelmap_sdl3rend, flush)(br_device_pixelmap* self);
extern br_error BR_CMETHOD_DECL(br_device_pixelmap_sdl3rend, directLock)(br_device_pixelmap* self, br_boolean block);
extern br_error BR_CMETHOD_DECL(br_device_pixelmap_sdl3rend, directUnlock)(br_device_pixelmap* self);

#endif

#ifdef __cplusplus
};
#endif
#endif
