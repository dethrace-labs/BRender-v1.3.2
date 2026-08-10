/*
 * SDL3 GPU video driver for BRender.
 *
 * Rendering model: every render pass targets an offscreen
 * (transferTexture + depthTexture) so the depth attachment and all pipeline
 * formats are fixed for the lifetime of the window; the frame is blitted to
 * the swapchain texture at present. SDL3 GPU inserts the necessary layout
 * transitions, so this driver needs no explicit barriers.
 */

#include <string.h>

#include "drv.h"
#include "drv_ip.h"
#include "brsdl3rend.h"
#include "gstored.h"
#include "sdl3_shaders.h"
#include "video.h"

#define SDL3REND_DEFAULT_RING_VBO_CAPACITY (512 * 1024)
#define SDL3REND_DEFAULT_RING_IBO_CAPACITY (256 * 1024)
#define SDL3REND_DEFAULT_STAGING_CAPACITY  (16 * 1024 * 1024)

#define SDL3REND_OVERLAY_QUAD_VERTS   4
#define SDL3REND_OVERLAY_QUAD_INDICES 6

static HVIDEO g_sdl3rend_video = NULL;
static void (*g_sdl3rend_external_cb)(void* cmd, void* ud) = NULL;
static void* g_sdl3rend_external_ud = NULL;

static void WaitFence(SDL_GPUDevice* device, SDL_GPUFence* fence) {
    if (!fence) return;
    SDL_GPUFence* fences[1] = { fence };
    SDL_WaitForGPUFences(device, true, fences, 1);
    SDL_ReleaseGPUFence(device, fence);
}

int SDL3REND_UploadBufferToBuffer(HVIDEO hVideo, SDL_GPUBuffer* buffer, const void* hostData, size_t size);

static int CreateOffscreenTargets(HVIDEO hVideo) {
    SDL_GPUTextureCreateInfo ti = {0};
    ti.type = SDL_GPU_TEXTURETYPE_2D;
    ti.format = hVideo->swapchainTextureFormat;
    ti.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ti.width = hVideo->windowWidth;
    ti.height = hVideo->windowHeight;
    ti.layer_count_or_depth = 1;
    ti.num_levels = 1;
    ti.sample_count = SDL_GPU_SAMPLECOUNT_1;

    hVideo->transferTexture = SDL_CreateGPUTexture(hVideo->device, &ti);
    if (!hVideo->transferTexture) {
        BR_FATAL("SDL3GPU: Failed to create transfer texture.");
        return 0;
    }

    ti.format = hVideo->depthFormat;
    ti.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    hVideo->depthTexture = SDL_CreateGPUTexture(hVideo->device, &ti);
    if (!hVideo->depthTexture) {
        BR_FATAL("SDL3GPU: Failed to create depth texture.");
        return 0;
    }
    return 1;
}

static int CreateSamplers(HVIDEO hVideo) {
    SDL_GPUSamplerCreateInfo si = {0};
    si.min_filter = SDL_GPU_FILTER_LINEAR;
    si.mag_filter = SDL_GPU_FILTER_LINEAR;
    si.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    si.compare_op = SDL_GPU_COMPAREOP_INVALID;
    si.max_anisotropy = 0.0f;
    si.enable_anisotropy = false;

    hVideo->samplerLinear = SDL_CreateGPUSampler(hVideo->device, &si);
    if (!hVideo->samplerLinear) {
        BR_FATAL("SDL3GPU: Failed to create linear sampler.");
        return 0;
    }

    si.min_filter = SDL_GPU_FILTER_NEAREST;
    si.mag_filter = SDL_GPU_FILTER_NEAREST;
    si.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    hVideo->samplerNearest = SDL_CreateGPUSampler(hVideo->device, &si);
    if (!hVideo->samplerNearest) {
        BR_FATAL("SDL3GPU: Failed to create nearest sampler.");
        return 0;
    }

    hVideo->overlaySampler = hVideo->samplerLinear;
    return 1;
}

static int CreateDefaultTexture(HVIDEO hVideo) {
    SDL_GPUTextureCreateInfo ti = {0};
    ti.type = SDL_GPU_TEXTURETYPE_2D;
    ti.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ti.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ti.width = 1;
    ti.height = 1;
    ti.layer_count_or_depth = 1;
    ti.num_levels = 1;
    ti.sample_count = SDL_GPU_SAMPLECOUNT_1;

    hVideo->defaultTexture = SDL_CreateGPUTexture(hVideo->device, &ti);
    if (!hVideo->defaultTexture) {
        BR_FATAL("SDL3GPU: Failed to create default texture.");
        return 0;
    }

    const uint32_t white = 0xFFFFFFFF;
    if (SDL3REND_UploadBufferToImage(hVideo, hVideo->defaultTexture, 1, 1, 0, 0,
            &white, sizeof(white)) != 0)
        return 0;

    return 1;
}

static int CreateOverlayQuad(HVIDEO hVideo) {
    float quad[] = {
         1.0f, -1.0f,   1.0f, 1.0f,
         1.0f,  1.0f,   1.0f, 0.0f,
        -1.0f,  1.0f,   0.0f, 0.0f,
        -1.0f, -1.0f,   0.0f, 1.0f,
    };
    uint16_t quadIdx[] = {0, 1, 3, 1, 2, 3};

    SDL_GPUBufferCreateInfo bi = {0};
    bi.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bi.size = sizeof(quad);
    hVideo->overlayQuadVbo = SDL_CreateGPUBuffer(hVideo->device, &bi);
    if (!hVideo->overlayQuadVbo) {
        BR_FATAL("SDL3GPU: Failed to create overlay quad VBO.");
        return 0;
    }

    bi.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    bi.size = sizeof(quadIdx);
    hVideo->overlayQuadIbo = SDL_CreateGPUBuffer(hVideo->device, &bi);
    if (!hVideo->overlayQuadIbo) {
        BR_FATAL("SDL3GPU: Failed to create overlay quad IBO.");
        return 0;
    }

    /* Static resources upload through the staging path (fills slot 0; its
     * fence is waited in the first SDL3REND_EnsureRecording). */
    if (SDL3REND_UploadBufferToBuffer(hVideo, hVideo->overlayQuadVbo, quad, sizeof(quad)) != 0)
        return 0;
    if (SDL3REND_UploadBufferToBuffer(hVideo, hVideo->overlayQuadIbo, quadIdx, sizeof(quadIdx)) != 0)
        return 0;

    return 1;
}

/*
 * Ensures the current frame slot has a mapped staging transfer buffer with at
 * least `need` free bytes. Grows (waits the slot's pending upload first) when
 * the current staging buffer is exhausted.
 */
static int EnsureStagingCapacity(HVIDEO hVideo, size_t need) {
    uint32_t f = hVideo->currentFrame;

    if (hVideo->stagingTransfer[f] &&
        hVideo->stagingMapped[f] &&
        hVideo->stagingOffset[f] + need <= hVideo->stagingSize) {
        return 1;
    }

    if (hVideo->uploadFence[f]) {
        WaitFence(hVideo->device, hVideo->uploadFence[f]);
        hVideo->uploadFence[f] = NULL;
    }
    if (hVideo->stagingMapped[f]) {
        SDL_UnmapGPUTransferBuffer(hVideo->device, hVideo->stagingTransfer[f]);
        hVideo->stagingMapped[f] = NULL;
    }

    size_t newSize = hVideo->stagingSize ? hVideo->stagingSize * 2 : SDL3REND_DEFAULT_STAGING_CAPACITY;
    while (newSize < need) newSize *= 2;

    SDL_GPUTransferBufferCreateInfo tci = {0};
    tci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tci.size = newSize;
    SDL_GPUTransferBuffer* tb = SDL_CreateGPUTransferBuffer(hVideo->device, &tci);
    if (!tb) {
        BR_FATAL("SDL3GPU: Failed to create staging transfer buffer.");
        return 0;
    }

    if (hVideo->stagingTransfer[f])
        SDL_ReleaseGPUTransferBuffer(hVideo->device, hVideo->stagingTransfer[f]);
    hVideo->stagingTransfer[f] = tb;
    hVideo->stagingSize = newSize;
    hVideo->stagingOffset[f] = 0;
    hVideo->stagingMapped[f] = SDL_MapGPUTransferBuffer(hVideo->device, tb, false);
    if (!hVideo->stagingMapped[f]) {
        BR_FATAL("SDL3GPU: Failed to map staging transfer buffer.");
        return 0;
    }
    return 1;
}

static int EnsureStagingMapped(HVIDEO hVideo, uint32_t f) {
    if (hVideo->stagingMapped[f]) return 1;
    if (!hVideo->stagingTransfer[f])
        return EnsureStagingCapacity(hVideo, SDL3REND_DEFAULT_STAGING_CAPACITY);
    hVideo->stagingMapped[f] = SDL_MapGPUTransferBuffer(hVideo->device, hVideo->stagingTransfer[f], false);
    return hVideo->stagingMapped[f] != NULL;
}

/*
 * Uploads host data into a GPU buffer through the current frame slot's staging
 * transfer buffer. Same staging/fence discipline as SDL3REND_UploadBufferToImage.
 * Returns 0 on success, nonzero on failure.
 */
int SDL3REND_UploadBufferToBuffer(HVIDEO hVideo, SDL_GPUBuffer* buffer, const void* hostData, size_t size) {
    if (!hostData || size == 0) return -1;

    uint32_t f = hVideo->currentFrame;

    if (hVideo->uploadFence[f]) {
        WaitFence(hVideo->device, hVideo->uploadFence[f]);
        hVideo->uploadFence[f] = NULL;
    }
    if (!hVideo->stagingMapped[f]) {
        if (!EnsureStagingMapped(hVideo, f))
            return -1;
        hVideo->stagingOffset[f] = 0;
    }
    if (hVideo->stagingOffset[f] + size > hVideo->stagingSize) {
        if (!EnsureStagingCapacity(hVideo, size))
            return -1;
    }
    if (!hVideo->stagingMapped[f])
        return -1;

    memcpy((char*)hVideo->stagingMapped[f] + hVideo->stagingOffset[f], hostData, size);
    SDL_UnmapGPUTransferBuffer(hVideo->device, hVideo->stagingTransfer[f]);
    hVideo->stagingMapped[f] = NULL;

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(hVideo->device);
    if (!cmd) {
        BR_FATAL("SDL3GPU: Failed to acquire upload command buffer.");
        return -1;
    }

    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTransferBufferLocation src = { hVideo->stagingTransfer[f], (Uint32)hVideo->stagingOffset[f] };
    SDL_GPUBufferRegion dst = { buffer, 0, (Uint32)size };
    SDL_UploadToGPUBuffer(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);

    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (!fence) {
        BR_FATAL("SDL3GPU: Failed to submit upload command buffer.");
        return -1;
    }
    hVideo->uploadFence[f] = fence;
    hVideo->stagingOffset[f] += (Uint32)size;
    return 0;
}

static int CreateRings(HVIDEO hVideo) {
    hVideo->dynVboCapacity = SDL3REND_DEFAULT_RING_VBO_CAPACITY;
    hVideo->dynIboCapacity = SDL3REND_DEFAULT_RING_IBO_CAPACITY;

    for (uint32_t f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {
        SDL_GPUBufferCreateInfo bi = {0};
        bi.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        bi.size = hVideo->dynVboCapacity;
        hVideo->dynVbo[f] = SDL_CreateGPUBuffer(hVideo->device, &bi);
        if (!hVideo->dynVbo[f]) {
            BR_FATAL("SDL3GPU: Failed to create dynamic VBO.");
            return 0;
        }

        bi.usage = SDL_GPU_BUFFERUSAGE_INDEX;
        bi.size = hVideo->dynIboCapacity;
        hVideo->dynIbo[f] = SDL_CreateGPUBuffer(hVideo->device, &bi);
        if (!hVideo->dynIbo[f]) {
            BR_FATAL("SDL3GPU: Failed to create dynamic IBO.");
            return 0;
        }

        SDL_GPUTransferBufferCreateInfo tci = {0};
        tci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        tci.size = hVideo->dynVboCapacity;
        hVideo->dynVboTransfer[f] = SDL_CreateGPUTransferBuffer(hVideo->device, &tci);
        if (!hVideo->dynVboTransfer[f]) {
            BR_FATAL("SDL3GPU: Failed to create dynamic VBO transfer buffer.");
            return 0;
        }
        hVideo->dynVboMapped[f] = SDL_MapGPUTransferBuffer(hVideo->device, hVideo->dynVboTransfer[f], false);
        if (!hVideo->dynVboMapped[f]) {
            BR_FATAL("SDL3GPU: Failed to map dynamic VBO transfer buffer.");
            return 0;
        }

        tci.size = hVideo->dynIboCapacity;
        hVideo->dynIboTransfer[f] = SDL_CreateGPUTransferBuffer(hVideo->device, &tci);
        if (!hVideo->dynIboTransfer[f]) {
            BR_FATAL("SDL3GPU: Failed to create dynamic IBO transfer buffer.");
            return 0;
        }
        hVideo->dynIboMapped[f] = SDL_MapGPUTransferBuffer(hVideo->device, hVideo->dynIboTransfer[f], false);
        if (!hVideo->dynIboMapped[f]) {
            BR_FATAL("SDL3GPU: Failed to map dynamic IBO transfer buffer.");
            return 0;
        }

        tci.size = SDL3REND_DEFAULT_STAGING_CAPACITY;
        hVideo->stagingTransfer[f] = SDL_CreateGPUTransferBuffer(hVideo->device, &tci);
        if (!hVideo->stagingTransfer[f]) {
            BR_FATAL("SDL3GPU: Failed to create staging transfer buffer.");
            return 0;
        }
        hVideo->stagingMapped[f] = SDL_MapGPUTransferBuffer(hVideo->device, hVideo->stagingTransfer[f], false);
        if (!hVideo->stagingMapped[f]) {
            BR_FATAL("SDL3GPU: Failed to map staging transfer buffer.");
            return 0;
        }
        hVideo->stagingSize = SDL3REND_DEFAULT_STAGING_CAPACITY;
    }
    return 1;
}

static void ReleaseRings(HVIDEO hVideo) {
    SDL_GPUDevice* device = hVideo->device;
    if (!device) return;
    for (uint32_t f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {
        if (hVideo->dynVboMapped[f]) { SDL_UnmapGPUTransferBuffer(device, hVideo->dynVboTransfer[f]); hVideo->dynVboMapped[f] = NULL; }
        if (hVideo->dynIboMapped[f]) { SDL_UnmapGPUTransferBuffer(device, hVideo->dynIboTransfer[f]); hVideo->dynIboMapped[f] = NULL; }
        if (hVideo->stagingMapped[f]) { SDL_UnmapGPUTransferBuffer(device, hVideo->stagingTransfer[f]); hVideo->stagingMapped[f] = NULL; }
        if (hVideo->dynVboTransfer[f]) { SDL_ReleaseGPUTransferBuffer(device, hVideo->dynVboTransfer[f]); hVideo->dynVboTransfer[f] = NULL; }
        if (hVideo->dynIboTransfer[f]) { SDL_ReleaseGPUTransferBuffer(device, hVideo->dynIboTransfer[f]); hVideo->dynIboTransfer[f] = NULL; }
        if (hVideo->stagingTransfer[f]) { SDL_ReleaseGPUTransferBuffer(device, hVideo->stagingTransfer[f]); hVideo->stagingTransfer[f] = NULL; }
        if (hVideo->dynVbo[f]) { SDL_ReleaseGPUBuffer(device, hVideo->dynVbo[f]); hVideo->dynVbo[f] = NULL; }
        if (hVideo->dynIbo[f]) { SDL_ReleaseGPUBuffer(device, hVideo->dynIbo[f]); hVideo->dynIbo[f] = NULL; }
    }
}

SDL_GPUShader* SDL3REND_CreateShader(HVIDEO hVideo, const char* code, size_t code_size, SDL_GPUShaderStage stage) {
    if (!code || code_size == 0) return NULL;

    SDL_GPUShaderCreateInfo ci = {0};
    ci.code = (const Uint8*)code;
    ci.code_size = code_size;
    ci.entrypoint = "main";
    ci.format = SDL_GPU_SHADERFORMAT_SPIRV;
    ci.stage = stage;
    ci.num_samplers = 0;
    ci.num_storage_textures = 0;
    ci.num_storage_buffers = 0;
    ci.num_uniform_buffers = 0;

    if (stage == SDL_GPU_SHADERSTAGE_VERTEX) {
        /* set1: model + scene UBOs. */
        ci.num_uniform_buffers = 2;
    } else {
        /* set2: main_texture sampler; set3: model + scene UBOs. */
        ci.num_samplers = 1;
        ci.num_uniform_buffers = 2;
    }

    SDL_GPUShader* shader = SDL_CreateGPUShader(hVideo->device, &ci);
    if (!shader) {
        BR_FATAL("SDL3GPU: Failed to create shader.");
        return NULL;
    }
    return shader;
}

SDL_GPUGraphicsPipeline* SDL3REND_CreateGraphicsPipeline(HVIDEO hVideo,
    SDL_GPUShader* vertModule, SDL_GPUShader* fragModule,
    const SDL_GPUVertexBufferDescription* bindingDesc,
    const SDL_GPUVertexAttribute* attrDescs, uint32_t attrCount,
    uint32_t width, uint32_t height, bool blendEnable,
    bool depthTestEnable, bool depthWriteEnable) {

    (void)width;
    (void)height;

    SDL_GPUGraphicsPipelineCreateInfo ci = {0};
    ci.vertex_shader = vertModule;
    ci.fragment_shader = fragModule;
    ci.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    ci.vertex_input_state.num_vertex_buffers = bindingDesc ? 1 : 0;
    ci.vertex_input_state.vertex_buffer_descriptions = bindingDesc;
    ci.vertex_input_state.num_vertex_attributes = attrCount;
    ci.vertex_input_state.vertex_attributes = attrDescs;

    ci.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    ci.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    ci.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    ci.rasterizer_state.enable_depth_clip = true;
    ci.rasterizer_state.enable_depth_bias = false;
    ci.rasterizer_state.depth_bias_constant_factor = 0.0f;
    ci.rasterizer_state.depth_bias_clamp = 0.0f;
    ci.rasterizer_state.depth_bias_slope_factor = 0.0f;

    ci.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    ci.multisample_state.sample_mask = 0;
    ci.multisample_state.enable_mask = false;
    ci.multisample_state.enable_alpha_to_coverage = false;

    ci.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    ci.depth_stencil_state.enable_depth_test = depthTestEnable;
    ci.depth_stencil_state.enable_depth_write = depthWriteEnable;
    ci.depth_stencil_state.enable_stencil_test = false;
    ci.depth_stencil_state.compare_mask = 0xFF;
    ci.depth_stencil_state.write_mask = 0xFF;

    SDL_GPUColorTargetBlendState blend = {0};
    blend.color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G |
                             SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;
    blend.enable_blend = blendEnable;
    blend.enable_color_write_mask = false;
    if (blendEnable) {
        blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        blend.color_blend_op = SDL_GPU_BLENDOP_ADD;
        blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        blend.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    }

    /* Pipelines always declare the depth attachment: every render pass targets
     * transferTexture + depthTexture, and depth testing simply stays disabled
     * when the flags say so (mirrors the VK driver). */
    SDL_GPUColorTargetDescription target = {0};
    target.format = hVideo->swapchainTextureFormat;
    target.blend_state = blend;

    ci.target_info.color_target_descriptions = &target;
    ci.target_info.num_color_targets = 1;
    ci.target_info.depth_stencil_format = hVideo->depthFormat;
    ci.target_info.has_depth_stencil_target = true;

    SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(hVideo->device, &ci);
    if (!pipeline) {
        BR_FATAL("SDL3GPU: Failed to create graphics pipeline.");
        return NULL;
    }
    return pipeline;
}

HVIDEO SDL3REND_VideoOpen(HVIDEO hVideo, void* parent,
    const char* vertSpv, size_t vertSpvSize,
    const char* fragSpv, size_t fragSpvSize,
    const char* ovVertSpv, size_t ovVertSpvSize,
    const char* ovFragSpv, size_t ovFragSpvSize,
    const char* defVertSpv, size_t defVertSpvSize,
    const char* defFragSpv, size_t defFragSpvSize,
    br_device_sdl3_callback_procs* callbacks, int width, int height) {

    if (hVideo == NULL) {
        BR_FATAL("VIDEO: Invalid handle.");
        return NULL;
    }

    memset(hVideo, 0, sizeof(VIDEO));
    hVideo->res = parent;

    if (callbacks) {
        hVideo->get_map_mode = callbacks->get_map_mode;
        hVideo->get_window_size = callbacks->get_window_size;
    }

    /* Fall back to the embedded SPIR-V (sdl3_shaders.c) when the caller does
     * not supply a shader. The caller may still override via the arguments. */
    if (!vertSpv) { vertSpv = brender_vert_spv; vertSpvSize = brender_vert_spv_size; }
    if (!fragSpv) { fragSpv = brender_frag_spv; fragSpvSize = brender_frag_spv_size; }
    if (!ovVertSpv) { ovVertSpv = overlay_vert_spv; ovVertSpvSize = overlay_vert_spv_size; }
    if (!ovFragSpv) { ovFragSpv = overlay_frag_spv; ovFragSpvSize = overlay_frag_spv_size; }

    hVideo->window = callbacks && callbacks->get_window ? (SDL_Window*)callbacks->get_window() : NULL;
    if (hVideo->window == NULL) {
        BR_FATAL("SDL3GPU: No window provided (get_window callback missing).");
        return NULL;
    }

    hVideo->device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, NULL);
    if (!hVideo->device) {
        BR_FATAL("SDL3GPU: Failed to create GPU device.");
        return NULL;
    }

    if (!SDL_ClaimWindowForGPUDevice(hVideo->device, hVideo->window)) {
        BR_FATAL("SDL3GPU: Failed to claim window for GPU device.");
        return NULL;
    }

    hVideo->swapchainTextureFormat = SDL_GetGPUSwapchainTextureFormat(hVideo->device, hVideo->window);
    hVideo->depthFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

    if (width <= 0 || height <= 0) {
        SDL_GetWindowSizeInPixels(hVideo->window, &width, &height);
    }
    hVideo->windowWidth = width;
    hVideo->windowHeight = height;

    if (!CreateOffscreenTargets(hVideo))
        goto cleanup;
    if (!CreateSamplers(hVideo))
        goto cleanup;
    if (!CreateRings(hVideo))
        goto cleanup;
    if (!CreateOverlayQuad(hVideo))
        goto cleanup;
    if (!CreateDefaultTexture(hVideo))
        goto cleanup;

    hVideo->brenderVertShader = SDL3REND_CreateShader(hVideo, vertSpv, vertSpvSize, SDL_GPU_SHADERSTAGE_VERTEX);
    hVideo->brenderFragShader = SDL3REND_CreateShader(hVideo, fragSpv, fragSpvSize, SDL_GPU_SHADERSTAGE_FRAGMENT);
    hVideo->overlayVertShader = SDL3REND_CreateShader(hVideo, ovVertSpv, ovVertSpvSize, SDL_GPU_SHADERSTAGE_VERTEX);
    hVideo->overlayFragShader = SDL3REND_CreateShader(hVideo, ovFragSpv, ovFragSpvSize, SDL_GPU_SHADERSTAGE_FRAGMENT);
    if (!hVideo->brenderVertShader || !hVideo->brenderFragShader ||
        !hVideo->overlayVertShader || !hVideo->overlayFragShader)
        goto cleanup_shaders;

    {
        SDL_GPUVertexBufferDescription bindingDesc = {0};
        bindingDesc.slot = 0;
        bindingDesc.pitch = sizeof(sdl3_vertex_f);
        bindingDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        bindingDesc.instance_step_rate = 0;

        SDL_GPUVertexAttribute attrDescs[4] = {0};
        attrDescs[0].location = 0;
        attrDescs[0].buffer_slot = 0;
        attrDescs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrDescs[0].offset = offsetof(sdl3_vertex_f, p);
        attrDescs[1].location = 1;
        attrDescs[1].buffer_slot = 0;
        attrDescs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrDescs[1].offset = offsetof(sdl3_vertex_f, map);
        attrDescs[2].location = 2;
        attrDescs[2].buffer_slot = 0;
        attrDescs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrDescs[2].offset = offsetof(sdl3_vertex_f, n);
        attrDescs[3].location = 3;
        attrDescs[3].buffer_slot = 0;
        attrDescs[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attrDescs[3].offset = offsetof(sdl3_vertex_f, c);

        hVideo->brenderPipeline = SDL3REND_CreateGraphicsPipeline(hVideo,
            hVideo->brenderVertShader, hVideo->brenderFragShader,
            &bindingDesc, attrDescs, 4,
            hVideo->windowWidth, hVideo->windowHeight, false, true, true);
        if (!hVideo->brenderPipeline)
            goto cleanup_shaders;

        hVideo->brenderPipelineNoDepth = SDL3REND_CreateGraphicsPipeline(hVideo,
            hVideo->brenderVertShader, hVideo->brenderFragShader,
            &bindingDesc, attrDescs, 4,
            hVideo->windowWidth, hVideo->windowHeight, false, false, false);
        if (!hVideo->brenderPipelineNoDepth)
            goto cleanup_shaders;

        hVideo->brenderBlendPipeline = SDL3REND_CreateGraphicsPipeline(hVideo,
            hVideo->brenderVertShader, hVideo->brenderFragShader,
            &bindingDesc, attrDescs, 4,
            hVideo->windowWidth, hVideo->windowHeight, true, true, false);
        if (!hVideo->brenderBlendPipeline)
            goto cleanup_shaders;

        hVideo->brenderBlendPipelineNoDepth = SDL3REND_CreateGraphicsPipeline(hVideo,
            hVideo->brenderVertShader, hVideo->brenderFragShader,
            &bindingDesc, attrDescs, 4,
            hVideo->windowWidth, hVideo->windowHeight, true, false, false);
        if (!hVideo->brenderBlendPipelineNoDepth)
            goto cleanup_shaders;
    }

    /* Default shaders/pipeline alias the brender ones when not provided. */
    if (defVertSpv && defVertSpvSize)
        hVideo->defaultVertShader = SDL3REND_CreateShader(hVideo, defVertSpv, defVertSpvSize, SDL_GPU_SHADERSTAGE_VERTEX);
    if (defFragSpv && defFragSpvSize)
        hVideo->defaultFragShader = SDL3REND_CreateShader(hVideo, defFragSpv, defFragSpvSize, SDL_GPU_SHADERSTAGE_FRAGMENT);
    hVideo->defaultPipeline = hVideo->brenderPipeline;

    {
        SDL_GPUVertexBufferDescription bindingDesc = {0};
        bindingDesc.slot = 0;
        bindingDesc.pitch = 4 * sizeof(float);
        bindingDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        bindingDesc.instance_step_rate = 0;

        SDL_GPUVertexAttribute attrDescs[2] = {0};
        attrDescs[0].location = 0;
        attrDescs[0].buffer_slot = 0;
        attrDescs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrDescs[0].offset = 0;
        attrDescs[1].location = 1;
        attrDescs[1].buffer_slot = 0;
        attrDescs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrDescs[1].offset = 2 * sizeof(float);

        hVideo->overlayPipeline = SDL3REND_CreateGraphicsPipeline(hVideo,
            hVideo->overlayVertShader, hVideo->overlayFragShader,
            &bindingDesc, attrDescs, 2,
            hVideo->windowWidth, hVideo->windowHeight, true, false, false);
        if (!hVideo->overlayPipeline)
            goto cleanup_shaders;
    }

    BrLogPrintf("SDL3GPU: GPU device initialized (framebuffer %dx%d)\n",
        hVideo->windowWidth, hVideo->windowHeight);

    g_sdl3rend_video = hVideo;
    return hVideo;

cleanup_shaders:
    if (hVideo->overlayVertShader) SDL_ReleaseGPUShader(hVideo->device, hVideo->overlayVertShader);
    if (hVideo->overlayFragShader) SDL_ReleaseGPUShader(hVideo->device, hVideo->overlayFragShader);
    if (hVideo->brenderFragShader) SDL_ReleaseGPUShader(hVideo->device, hVideo->brenderFragShader);
    if (hVideo->brenderVertShader) SDL_ReleaseGPUShader(hVideo->device, hVideo->brenderVertShader);
cleanup:
    ReleaseRings(hVideo);
    if (hVideo->overlayQuadIbo) { SDL_ReleaseGPUBuffer(hVideo->device, hVideo->overlayQuadIbo); hVideo->overlayQuadIbo = NULL; }
    if (hVideo->overlayQuadVbo) { SDL_ReleaseGPUBuffer(hVideo->device, hVideo->overlayQuadVbo); hVideo->overlayQuadVbo = NULL; }
    if (hVideo->samplerNearest) { SDL_ReleaseGPUSampler(hVideo->device, hVideo->samplerNearest); hVideo->samplerNearest = NULL; }
    if (hVideo->samplerLinear) { SDL_ReleaseGPUSampler(hVideo->device, hVideo->samplerLinear); hVideo->samplerLinear = NULL; }
    if (hVideo->depthTexture) { SDL_ReleaseGPUTexture(hVideo->device, hVideo->depthTexture); hVideo->depthTexture = NULL; }
    if (hVideo->transferTexture) { SDL_ReleaseGPUTexture(hVideo->device, hVideo->transferTexture); hVideo->transferTexture = NULL; }
    if (hVideo->window) { SDL_ReleaseWindowFromGPUDevice(hVideo->device, hVideo->window); }
    if (hVideo->device) { SDL_DestroyGPUDevice(hVideo->device); hVideo->device = NULL; }
    hVideo->window = NULL;
    return NULL;
}

void SDL3REND_VideoClose(HVIDEO hVideo) {
    if (!hVideo || !hVideo->device) return;

    for (uint32_t f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {
        WaitFence(hVideo->device, hVideo->frameFence[f]);
        hVideo->frameFence[f] = NULL;
        WaitFence(hVideo->device, hVideo->ringUploadFence[f]);
        hVideo->ringUploadFence[f] = NULL;
        WaitFence(hVideo->device, hVideo->uploadFence[f]);
        hVideo->uploadFence[f] = NULL;
    }

    ReleaseRings(hVideo);

    if (hVideo->defaultFragShader && hVideo->defaultFragShader != hVideo->brenderFragShader)
        SDL_ReleaseGPUShader(hVideo->device, hVideo->defaultFragShader);
    if (hVideo->defaultVertShader && hVideo->defaultVertShader != hVideo->brenderVertShader)
        SDL_ReleaseGPUShader(hVideo->device, hVideo->defaultVertShader);
    if (hVideo->overlayPipeline) { SDL_ReleaseGPUGraphicsPipeline(hVideo->device, hVideo->overlayPipeline); hVideo->overlayPipeline = NULL; }
    if (hVideo->brenderBlendPipelineNoDepth) { SDL_ReleaseGPUGraphicsPipeline(hVideo->device, hVideo->brenderBlendPipelineNoDepth); hVideo->brenderBlendPipelineNoDepth = NULL; }
    if (hVideo->brenderBlendPipeline) { SDL_ReleaseGPUGraphicsPipeline(hVideo->device, hVideo->brenderBlendPipeline); hVideo->brenderBlendPipeline = NULL; }
    if (hVideo->brenderPipelineNoDepth) { SDL_ReleaseGPUGraphicsPipeline(hVideo->device, hVideo->brenderPipelineNoDepth); hVideo->brenderPipelineNoDepth = NULL; }
    if (hVideo->brenderPipeline) { SDL_ReleaseGPUGraphicsPipeline(hVideo->device, hVideo->brenderPipeline); hVideo->brenderPipeline = NULL; }
    hVideo->defaultPipeline = NULL;
    if (hVideo->overlayFragShader) { SDL_ReleaseGPUShader(hVideo->device, hVideo->overlayFragShader); hVideo->overlayFragShader = NULL; }
    if (hVideo->overlayVertShader) { SDL_ReleaseGPUShader(hVideo->device, hVideo->overlayVertShader); hVideo->overlayVertShader = NULL; }
    if (hVideo->brenderFragShader) { SDL_ReleaseGPUShader(hVideo->device, hVideo->brenderFragShader); hVideo->brenderFragShader = NULL; }
    if (hVideo->brenderVertShader) { SDL_ReleaseGPUShader(hVideo->device, hVideo->brenderVertShader); hVideo->brenderVertShader = NULL; }
    if (hVideo->overlayTexture) { SDL_ReleaseGPUTexture(hVideo->device, hVideo->overlayTexture); hVideo->overlayTexture = NULL; }
    if (hVideo->defaultTexture) { SDL_ReleaseGPUTexture(hVideo->device, hVideo->defaultTexture); hVideo->defaultTexture = NULL; }
    if (hVideo->overlayQuadIbo) { SDL_ReleaseGPUBuffer(hVideo->device, hVideo->overlayQuadIbo); hVideo->overlayQuadIbo = NULL; }
    if (hVideo->overlayQuadVbo) { SDL_ReleaseGPUBuffer(hVideo->device, hVideo->overlayQuadVbo); hVideo->overlayQuadVbo = NULL; }
    if (hVideo->samplerNearest) { SDL_ReleaseGPUSampler(hVideo->device, hVideo->samplerNearest); hVideo->samplerNearest = NULL; }
    if (hVideo->samplerLinear) { SDL_ReleaseGPUSampler(hVideo->device, hVideo->samplerLinear); hVideo->samplerLinear = NULL; }
    if (hVideo->depthTexture) { SDL_ReleaseGPUTexture(hVideo->device, hVideo->depthTexture); hVideo->depthTexture = NULL; }
    if (hVideo->transferTexture) { SDL_ReleaseGPUTexture(hVideo->device, hVideo->transferTexture); hVideo->transferTexture = NULL; }

    if (hVideo->window) SDL_ReleaseWindowFromGPUDevice(hVideo->device, hVideo->window);
    SDL_DestroyGPUDevice(hVideo->device);
    hVideo->device = NULL;
    hVideo->window = NULL;

    if (g_sdl3rend_video == hVideo) g_sdl3rend_video = NULL;
}

void SDL3REND_VideoResize(HVIDEO hVideo) {
    int w = hVideo->windowWidth, h = hVideo->windowHeight;
    if (hVideo->get_window_size) {
        hVideo->get_window_size(&w, &h);
    } else if (hVideo->window) {
        SDL_GetWindowSizeInPixels(hVideo->window, &w, &h);
    }
    if (w <= 0 || h <= 0) return;

    if (hVideo->transferTexture) { SDL_ReleaseGPUTexture(hVideo->device, hVideo->transferTexture); hVideo->transferTexture = NULL; }
    if (hVideo->depthTexture) { SDL_ReleaseGPUTexture(hVideo->device, hVideo->depthTexture); hVideo->depthTexture = NULL; }

    hVideo->windowWidth = w;
    hVideo->windowHeight = h;

    if (!CreateOffscreenTargets(hVideo)) {
        BR_FATAL("SDL3GPU: Failed to recreate offscreen targets after resize.");
    }
}

void SDL3REND_UpdateScene(HVIDEO hVideo, void* data, size_t size) {
    if (size > sizeof(hVideo->sceneData)) size = sizeof(hVideo->sceneData);
    if (data) memcpy(&hVideo->sceneData, data, size);
}

void SDL3REND_SceneBegin(HVIDEO hVideo) {
    SDL3REND_EnsureRecording(hVideo);
    if (!hVideo->commandBuffer) return;
    SDL_PushGPUVertexUniformData(hVideo->commandBuffer, SDL3REND_SCENE_UNIFORM_SLOT,
        &hVideo->sceneData, (Uint32)sizeof(hVideo->sceneData));
    SDL_PushGPUFragmentUniformData(hVideo->commandBuffer, SDL3REND_SCENE_UNIFORM_SLOT,
        &hVideo->sceneData, (Uint32)sizeof(hVideo->sceneData));
}

void SDL3REND_PushModel(HVIDEO hVideo, const void* data, size_t size) {
    if (!data || size == 0) return;
    if (size > sizeof(hVideo->modelData)) size = sizeof(hVideo->modelData);
    SDL3REND_EnsureRecording(hVideo);
    if (!hVideo->commandBuffer) return;
    SDL_PushGPUVertexUniformData(hVideo->commandBuffer, SDL3REND_MODEL_UNIFORM_SLOT,
        data, (Uint32)size);
    SDL_PushGPUFragmentUniformData(hVideo->commandBuffer, SDL3REND_MODEL_UNIFORM_SLOT,
        data, (Uint32)size);
}

void SDL3REND_EnsureRecording(HVIDEO hVideo) {
    if (hVideo->isRecording) return;

    uint32_t f = hVideo->currentFrame;

    if (hVideo->frameFence[f]) { WaitFence(hVideo->device, hVideo->frameFence[f]); hVideo->frameFence[f] = NULL; }
    if (hVideo->ringUploadFence[f]) { WaitFence(hVideo->device, hVideo->ringUploadFence[f]); hVideo->ringUploadFence[f] = NULL; }
    if (hVideo->uploadFence[f]) { WaitFence(hVideo->device, hVideo->uploadFence[f]); hVideo->uploadFence[f] = NULL; }

    /* Re-map the transfer buffers for this frame slot. */
    if (hVideo->dynVboTransfer[f] && !hVideo->dynVboMapped[f])
        hVideo->dynVboMapped[f] = SDL_MapGPUTransferBuffer(hVideo->device, hVideo->dynVboTransfer[f], false);
    if (hVideo->dynIboTransfer[f] && !hVideo->dynIboMapped[f])
        hVideo->dynIboMapped[f] = SDL_MapGPUTransferBuffer(hVideo->device, hVideo->dynIboTransfer[f], false);
    if (hVideo->stagingTransfer[f] && !hVideo->stagingMapped[f])
        hVideo->stagingMapped[f] = SDL_MapGPUTransferBuffer(hVideo->device, hVideo->stagingTransfer[f], false);

    hVideo->dynVboOffset[f] = 0;
    hVideo->dynIboOffset[f] = 0;
    hVideo->dynVboWritten[f] = 0;
    hVideo->dynIboWritten[f] = 0;
    hVideo->stagingOffset[f] = 0;

    hVideo->frameEpoch++;

    hVideo->dimAreaCount = 0;
    hVideo->clearAreaCount = 0;
    hVideo->pratcamAreaCount = 0;

    /* Resize detection. */
    {
        int w = hVideo->windowWidth, h = hVideo->windowHeight;
        SDL3REND_GetWindowSize(hVideo, &w, &h);
        if (w > 0 && h > 0 && (w != hVideo->windowWidth || h != hVideo->windowHeight)) {
            SDL3REND_VideoResize(hVideo);
            hVideo->mainViewportW = 0;
        }
    }

    hVideo->commandBuffer = SDL_AcquireGPUCommandBuffer(hVideo->device);
    if (!hVideo->commandBuffer) {
        BR_FATAL("SDL3GPU: Failed to acquire command buffer.");
        return;
    }
    hVideo->isRecording = 1;
    hVideo->renderPassActive = 0;
    hVideo->currentPass = NULL;
}

void SDL3REND_BeginRenderPass(HVIDEO hVideo) {
    if (hVideo->renderPassActive) return;
    SDL3REND_EnsureRecording(hVideo);
    if (!hVideo->commandBuffer) return;

    SDL_GPUColorTargetInfo color = {0};
    color.texture = hVideo->transferTexture;
    color.clear_color = (SDL_FColor){0.0f, 0.0f, 0.0f, 1.0f};
    color.load_op = SDL_GPU_LOADOP_CLEAR;
    color.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPUDepthStencilTargetInfo depth = {0};
    depth.texture = hVideo->depthTexture;
    depth.clear_depth = 1.0f;
    depth.load_op = SDL_GPU_LOADOP_CLEAR;
    depth.store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

    hVideo->currentPass = SDL_BeginGPURenderPass(hVideo->commandBuffer, &color, 1, &depth);
    if (!hVideo->currentPass) {
        BR_FATAL("SDL3GPU: Failed to begin render pass.");
        return;
    }
    hVideo->renderPassActive = 1;

    /* SDL3 GPU resets all render state at every pass start. */
    hVideo->lastPipeline = NULL;
    hVideo->lastVbo = NULL;
    hVideo->lastIbo = NULL;
    hVideo->lastVboOffset = 0;
    hVideo->lastIboOffset = 0;
    hVideo->lastTexture = NULL;
    hVideo->lastSampler = NULL;
}

void SDL3REND_EndRenderPass(HVIDEO hVideo) {
    if (!hVideo->renderPassActive || !hVideo->currentPass) return;
    SDL_EndGPURenderPass(hVideo->currentPass);
    hVideo->currentPass = NULL;
    hVideo->renderPassActive = 0;
}

int SDL3REND_Present(HVIDEO hVideo) {
    uint32_t f = hVideo->currentFrame;
    SDL_GPUDevice* device = hVideo->device;

    if (hVideo->renderPassActive)
        SDL3REND_EndRenderPass(hVideo);

    if (!hVideo->commandBuffer || !hVideo->isRecording) {
        hVideo->currentFrame = (f + 1) % MAX_FRAMES_IN_FLIGHT;
        return 0;
    }

    /* 1. Ring upload: unmap the transfer buffers and copy the written region
     * into the GPU ring buffers. Submitted before the main submit; the queue
     * is FIFO, so every draw in the main buffer sees this frame's ring data. */
    if (hVideo->dynVboMapped[f]) {
        SDL_UnmapGPUTransferBuffer(device, hVideo->dynVboTransfer[f]);
        hVideo->dynVboMapped[f] = NULL;
    }
    if (hVideo->dynIboMapped[f]) {
        SDL_UnmapGPUTransferBuffer(device, hVideo->dynIboTransfer[f]);
        hVideo->dynIboMapped[f] = NULL;
    }

    if (hVideo->dynVboWritten[f] > 0 || hVideo->dynIboWritten[f] > 0) {
        SDL_GPUCommandBuffer* upCmd = SDL_AcquireGPUCommandBuffer(device);
        if (upCmd) {
            SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(upCmd);
            if (hVideo->dynVboWritten[f] > 0) {
                SDL_GPUTransferBufferLocation src = { hVideo->dynVboTransfer[f], 0 };
                SDL_GPUBufferRegion dst = { hVideo->dynVbo[f], 0, (Uint32)hVideo->dynVboWritten[f] };
                SDL_UploadToGPUBuffer(copy, &src, &dst, false);
            }
            if (hVideo->dynIboWritten[f] > 0) {
                SDL_GPUTransferBufferLocation src = { hVideo->dynIboTransfer[f], 0 };
                SDL_GPUBufferRegion dst = { hVideo->dynIbo[f], 0, (Uint32)hVideo->dynIboWritten[f] };
                SDL_UploadToGPUBuffer(copy, &src, &dst, false);
            }
            SDL_EndGPUCopyPass(copy);
            hVideo->ringUploadFence[f] = SDL_SubmitGPUCommandBufferAndAcquireFence(upCmd);
            if (!hVideo->ringUploadFence[f])
                BR_FATAL("SDL3GPU: Failed to submit ring upload.");
        }
    }

    /* 2. Blit the offscreen frame into the swapchain and submit the main
     * command buffer. */
    SDL_GPUTexture* swapchainTexture = NULL;
    Uint32 sw = 0, sh = 0;
    if (SDL_AcquireGPUSwapchainTexture(hVideo->commandBuffer, hVideo->window, &swapchainTexture, &sw, &sh)) {
        if (swapchainTexture) {
            SDL_GPUBlitInfo blit = {0};
            blit.source.texture = hVideo->transferTexture;
            blit.source.w = hVideo->windowWidth;
            blit.source.h = hVideo->windowHeight;
            blit.destination.texture = swapchainTexture;
            blit.destination.w = sw;
            blit.destination.h = sh;
            blit.load_op = SDL_GPU_LOADOP_DONT_CARE;
            blit.filter = SDL_GPU_FILTER_LINEAR;
            /* BRender's projection is GL-style (NDC y-up); SDL3-GPU NDC
             * is y-down, so the rendered transfer texture is vertically
             * mirrored. Flip it at present time to match the screen. */
            blit.flip_mode = SDL_FLIP_VERTICAL;
            SDL_BlitGPUTexture(hVideo->commandBuffer, &blit);
        }
    } else {
        BrLogPrintf("SDL3GPU: AcquireGPUSwapchainTexture failed: %s\n", SDL_GetError());
    }

    if (g_sdl3rend_external_cb)
        g_sdl3rend_external_cb(hVideo->commandBuffer, g_sdl3rend_external_ud);

    hVideo->frameFence[f] = SDL_SubmitGPUCommandBufferAndAcquireFence(hVideo->commandBuffer);
    if (!hVideo->frameFence[f])
        BR_FATAL("SDL3GPU: Failed to submit frame command buffer.");

    hVideo->commandBuffer = NULL;
    hVideo->isRecording = 0;
    hVideo->renderPassActive = 0;
    hVideo->currentPass = NULL;

    hVideo->currentFrame = (f + 1) % MAX_FRAMES_IN_FLIGHT;
    return 0;
}

void SDL3REND_OverlayDraw(HVIDEO hVideo) {
    if (!hVideo->renderPassActive || !hVideo->currentPass) return;
    if (!hVideo->overlayDirty) return;
    if (!hVideo->overlayTexture || !hVideo->overlayPipeline) return;

    SDL_GPURenderPass* pass = hVideo->currentPass;

    SDL_GPUViewport viewport = {0};
    viewport.max_depth = 1.0f;
    SDL_Rect scissor = {0, 0, hVideo->windowWidth, hVideo->windowHeight};
    if (hVideo->pm_height > 0 && hVideo->windowHeight > 0) {
        /* Same letterbox math as the scene viewport (renderer.c sceneBegin):
         * centre the 4:3 overlay in the window and scale it aspect-preserving,
         * matching glrend. The overlay texture is the game-screen size. */
        float aspect = (float)hVideo->windowWidth / (float)hVideo->windowHeight;
        float target = (float)hVideo->pm_width / (float)hVideo->pm_height;
        int vp_width = hVideo->windowWidth, vp_height = hVideo->windowHeight;
        if (aspect > target) {
            vp_width = (int)((float)hVideo->windowHeight * target + 0.5f);
        } else {
            vp_height = (int)((float)hVideo->windowWidth / target + 0.5f);
        }
        int vp_x = (hVideo->windowWidth - vp_width) / 2;
        int vp_y = (hVideo->windowHeight - vp_height) / 2;
        viewport.x = (float)vp_x;
        viewport.y = (float)vp_y;
        viewport.w = (float)vp_width;
        viewport.h = (float)vp_height;
        scissor.x = vp_x;
        scissor.y = vp_y;
        scissor.w = vp_width;
        scissor.h = vp_height;
    } else {
        viewport.w = (float)hVideo->windowWidth;
        viewport.h = (float)hVideo->windowHeight;
    }
    SDL_SetGPUViewport(pass, &viewport);
    SDL_SetGPUScissor(pass, &scissor);

    if (hVideo->lastPipeline != hVideo->overlayPipeline) {
        SDL_BindGPUGraphicsPipeline(pass, hVideo->overlayPipeline);
        hVideo->lastPipeline = hVideo->overlayPipeline;
    }

    SDL_GPUBufferBinding vbo = { hVideo->overlayQuadVbo, 0 };
    if (hVideo->lastVbo != hVideo->overlayQuadVbo || hVideo->lastVboOffset != 0) {
        SDL_BindGPUVertexBuffers(pass, 0, &vbo, 1);
        hVideo->lastVbo = hVideo->overlayQuadVbo;
        hVideo->lastVboOffset = 0;
    }

    SDL_GPUBufferBinding ibo = { hVideo->overlayQuadIbo, 0 };
    if (hVideo->lastIbo != hVideo->overlayQuadIbo || hVideo->lastIboOffset != 0) {
        SDL_BindGPUIndexBuffer(pass, &ibo, SDL_GPU_INDEXELEMENTSIZE_16BIT);
        hVideo->lastIbo = hVideo->overlayQuadIbo;
        hVideo->lastIboOffset = 0;
    }

    if (hVideo->lastTexture != hVideo->overlayTexture || hVideo->lastSampler != hVideo->overlaySampler) {
        SDL_GPUTextureSamplerBinding tsb = { hVideo->overlayTexture, hVideo->overlaySampler };
        SDL_BindGPUFragmentSamplers(pass, SDL3REND_FRAGMENT_SAMPLER_SLOT, &tsb, 1);
        hVideo->lastTexture = hVideo->overlayTexture;
        hVideo->lastSampler = hVideo->overlaySampler;
    }

    SDL_DrawGPUIndexedPrimitives(pass, SDL3REND_OVERLAY_QUAD_INDICES, 1, 0, 0, 0);
    hVideo->overlayDirty = 0;
}

int SDL3REND_UploadBufferToImage(HVIDEO hVideo, SDL_GPUTexture* texture,
    uint32_t width, uint32_t height, uint32_t dstX, uint32_t dstY,
    const void* hostData, size_t hostDataSize) {

    if (!hostData || hostDataSize == 0) return -1;

    uint32_t f = hVideo->currentFrame;

    /* The slot is never reused while a copy it fed is still pending. */
    if (hVideo->uploadFence[f]) {
        WaitFence(hVideo->device, hVideo->uploadFence[f]);
        hVideo->uploadFence[f] = NULL;
    }

    if (!hVideo->stagingMapped[f]) {
        if (!EnsureStagingMapped(hVideo, f))
            return -1;
        hVideo->stagingOffset[f] = 0;
    }

    if (hVideo->stagingOffset[f] + hostDataSize > hVideo->stagingSize) {
        if (!EnsureStagingCapacity(hVideo, hostDataSize))
            return -1;
    }
    if (!hVideo->stagingMapped[f])
        return -1;

    memcpy((char*)hVideo->stagingMapped[f] + hVideo->stagingOffset[f], hostData, hostDataSize);
    SDL_UnmapGPUTransferBuffer(hVideo->device, hVideo->stagingTransfer[f]);
    hVideo->stagingMapped[f] = NULL;

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(hVideo->device);
    if (!cmd) {
        BR_FATAL("SDL3GPU: Failed to acquire upload command buffer.");
        return -1;
    }

    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureTransferInfo src = {0};
    src.transfer_buffer = hVideo->stagingTransfer[f];
    src.offset = (Uint32)hVideo->stagingOffset[f];
    SDL_GPUTextureRegion dst = {0};
    dst.texture = texture;
    dst.w = width;
    dst.h = height;
    dst.d = 1;
    dst.x = dstX;
    dst.y = dstY;
    SDL_UploadToGPUTexture(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);

    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (!fence) {
        BR_FATAL("SDL3GPU: Failed to submit upload command buffer.");
        return -1;
    }
    hVideo->uploadFence[f] = fence;
    hVideo->stagingOffset[f] += (Uint32)hostDataSize;
    return 0;
}

void SDL3REND_DeferFreeImage(HVIDEO hVideo, SDL_GPUTexture* texture, SDL_GPUSampler* sampler) {
    if (texture) SDL_ReleaseGPUTexture(hVideo->device, texture);
    if (sampler) SDL_ReleaseGPUSampler(hVideo->device, sampler);
}

void SDL3REND_DeferFreeBuffer(HVIDEO hVideo, SDL_GPUBuffer* buffer) {
    if (buffer) SDL_ReleaseGPUBuffer(hVideo->device, buffer);
}

void SDL3REND_GetDeviceInfo(SDL3REND_DeviceInfo* info) {
    if (!info) return;
    memset(info, 0, sizeof(*info));
    if (!g_sdl3rend_video) return;
    info->gpu_device = g_sdl3rend_video->device;
    info->window = g_sdl3rend_video->window;
    info->swapchain_texture_format = (uint32_t)g_sdl3rend_video->swapchainTextureFormat;
}

void SDL3REND_SetExternalRenderCallback(void (*cb)(void* cmd, void* ud), void* ud) {
    g_sdl3rend_external_cb = cb;
    g_sdl3rend_external_ud = ud;
}
