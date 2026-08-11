#ifndef sdl3_shaders_h
#define sdl3_shaders_h

#include <stddef.h>

/*
 * One shader stage in every format the driver can feed SDL3 GPU. Exactly one
 * field is used at runtime, selected by the device's shader format (see
 * video.c SDL3REND_CreateShader). Fields for formats this build did not
 * produce are NULL/0 (see sdl3rend_shader_formats.h and the CMake toolchain).
 */
typedef struct SDL3REND_ShaderSource {
    const char* spirv;
    size_t spirv_size;
    const char* msl;
    size_t msl_size;
    const char* dxil;
    size_t dxil_size;
} SDL3REND_ShaderSource;

/* Index of a stage within a shader pair. */
#define SDL3REND_STAGE_VERTEX 0
#define SDL3REND_STAGE_FRAGMENT 1

/* Embedded shader pairs: { vertex, fragment }. default_shaders aliases the
 * brender pair (the driver's 2D content uses the same vertex/fragment). */
extern const SDL3REND_ShaderSource brender_shaders[2];
extern const SDL3REND_ShaderSource overlay_shaders[2];
extern const SDL3REND_ShaderSource default_shaders[2];

#endif
