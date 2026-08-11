#include "sdl3_shaders.h"

#include "sdl3rend_shader_formats.h"

#include "brender.vert.spv.h"
#include "brender.frag.spv.h"
#include "overlay.vert.spv.h"
#include "overlay.frag.spv.h"

#if SDL3REND_SHADERFORMAT_MSL_AVAILABLE
#include "brender.vert.metal.h"
#include "brender.frag.metal.h"
#include "overlay.vert.metal.h"
#include "overlay.frag.metal.h"
#endif

#if SDL3REND_SHADERFORMAT_DXIL_AVAILABLE
#include "brender.vert.dxil.h"
#include "brender.frag.dxil.h"
#include "overlay.vert.dxil.h"
#include "overlay.frag.dxil.h"
#endif

static const SDL3REND_ShaderSource brender_vert = {
    BRENDER_VERT_SPV, sizeof(BRENDER_VERT_SPV),
#if SDL3REND_SHADERFORMAT_MSL_AVAILABLE
    BRENDER_VERT_METAL, sizeof(BRENDER_VERT_METAL),
#else
    NULL, 0,
#endif
#if SDL3REND_SHADERFORMAT_DXIL_AVAILABLE
    BRENDER_VERT_DXIL, sizeof(BRENDER_VERT_DXIL),
#else
    NULL, 0,
#endif
};

static const SDL3REND_ShaderSource brender_frag = {
    BRENDER_FRAG_SPV, sizeof(BRENDER_FRAG_SPV),
#if SDL3REND_SHADERFORMAT_MSL_AVAILABLE
    BRENDER_FRAG_METAL, sizeof(BRENDER_FRAG_METAL),
#else
    NULL, 0,
#endif
#if SDL3REND_SHADERFORMAT_DXIL_AVAILABLE
    BRENDER_FRAG_DXIL, sizeof(BRENDER_FRAG_DXIL),
#else
    NULL, 0,
#endif
};

static const SDL3REND_ShaderSource overlay_vert = {
    OVERLAY_VERT_SPV, sizeof(OVERLAY_VERT_SPV),
#if SDL3REND_SHADERFORMAT_MSL_AVAILABLE
    OVERLAY_VERT_METAL, sizeof(OVERLAY_VERT_METAL),
#else
    NULL, 0,
#endif
#if SDL3REND_SHADERFORMAT_DXIL_AVAILABLE
    OVERLAY_VERT_DXIL, sizeof(OVERLAY_VERT_DXIL),
#else
    NULL, 0,
#endif
};

static const SDL3REND_ShaderSource overlay_frag = {
    OVERLAY_FRAG_SPV, sizeof(OVERLAY_FRAG_SPV),
#if SDL3REND_SHADERFORMAT_MSL_AVAILABLE
    OVERLAY_FRAG_METAL, sizeof(OVERLAY_FRAG_METAL),
#else
    NULL, 0,
#endif
#if SDL3REND_SHADERFORMAT_DXIL_AVAILABLE
    OVERLAY_FRAG_DXIL, sizeof(OVERLAY_FRAG_DXIL),
#else
    NULL, 0,
#endif
};

const SDL3REND_ShaderSource brender_shaders[2] = { brender_vert, brender_frag };
const SDL3REND_ShaderSource overlay_shaders[2] = { overlay_vert, overlay_frag };
const SDL3REND_ShaderSource default_shaders[2] = { brender_vert, brender_frag };
