#include "vk_shaders.h"

#include "brender.vert.spv.h"
#include "brender.frag.spv.h"
#include "overlay.vert.spv.h"
#include "overlay.frag.spv.h"

const char* brender_vert_spv = BRENDER_VERT_SPV;
const size_t brender_vert_spv_size = sizeof(BRENDER_VERT_SPV);

const char* brender_frag_spv = BRENDER_FRAG_SPV;
const size_t brender_frag_spv_size = sizeof(BRENDER_FRAG_SPV);

const char* overlay_vert_spv  = OVERLAY_VERT_SPV;
const size_t overlay_vert_spv_size  = sizeof(OVERLAY_VERT_SPV);

const char* overlay_frag_spv  = OVERLAY_FRAG_SPV;
const size_t overlay_frag_spv_size  = sizeof(OVERLAY_FRAG_SPV);
