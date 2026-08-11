#ifndef VIDEO_H_
#define VIDEO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include "shader_data.h"

struct br_device_pixelmap;

#define MAX_FRAMES_IN_FLIGHT 2

/* Uniform slot / sampler slot mapping (matches the shared GLSL bindings). */
#define SDL3REND_MODEL_UNIFORM_SLOT     0
#define SDL3REND_SCENE_UNIFORM_SLOT     1
#define SDL3REND_FRAGMENT_SAMPLER_SLOT  0

/*
 * SDL3 GPU uniform slot mapping (matches the shared GLSL bindings):
 *
 *   set1 binding0 = shader_data_model  (vertex)  -> vertex   uniform slot 0
 *   set1 binding1 = shader_data_scene  (vertex)  -> vertex   uniform slot 1
 *   set2 binding0 = main_texture       (fragment)-> fragment sampler slot 0
 *   set3 binding0 = shader_data_model  (fragment)-> fragment uniform slot 0
 *   set3 binding1 = shader_data_scene  (fragment)-> fragment uniform slot 1
 *
 * Uniform data is pushed with SDL_PushGPUVertexUniformData /
 * SDL_PushGPUFragmentUniformData. NOTE: each SDL3 GPU backend caps the
 * readable window of a pushed uniform slot at 4096 bytes
 * (MAX_UBO_SECTION_SIZE in SDL3's backends) even though the push itself can
 * be larger, so everything a shader reads must live within the first 4096
 * bytes of its block (see the field order comment on shader_data_scene).
 * No UBO buffers, no descriptor sets, no per-draw bind changes.
 *
 * The scene is pushed once per sceneBegin (slots 1); the model is pushed per
 * draw (slots 0) by modelrender.c. Uniform slot data is stored on the command
 * buffer, so pushes done outside a render pass persist for the whole frame.
 */

typedef struct _VIDEO {
    void* res;

    SDL_GPUDevice* device;
    SDL_Window* window;
    SDL_GPUTextureFormat swapchainTextureFormat;
    SDL_GPUTextureFormat depthFormat;
    int windowWidth;
    int windowHeight;

    /* Offscreen render target (isle-portable pattern). Every render pass
     * targets transferTexture + depthTexture, so there is exactly one depth
     * attachment for the whole frame and the pipelines are created once
     * against a fixed format. At present the frame is blitted to the
     * swapchain texture (SDL_BlitGPUTexture), decoupling rendering from the
     * swapchain lifecycle entirely. */
    SDL_GPUTexture* transferTexture;
    SDL_GPUTexture* depthTexture;

    /* Shaders (SPIR-V from drivers/commonrend/*.glsl, embedded via
     * sdl3_shaders.c) and pipelines. Pipelines render into transferTexture's
     * format, so they never need rebuilding on swapchain recreation. */
    SDL_GPUShader* brenderVertShader;
    SDL_GPUShader* brenderFragShader;
    SDL_GPUShader* overlayVertShader;
    SDL_GPUShader* overlayFragShader;
    SDL_GPUShader* defaultVertShader;
    SDL_GPUShader* defaultFragShader;

    SDL_GPUGraphicsPipeline* brenderPipeline;
    SDL_GPUGraphicsPipeline* brenderPipelineNoDepth;
    SDL_GPUGraphicsPipeline* brenderBlendPipeline;
    SDL_GPUGraphicsPipeline* brenderBlendPipelineNoDepth;
    SDL_GPUGraphicsPipeline* defaultPipeline;
    SDL_GPUGraphicsPipeline* overlayPipeline;

    /* Command recording: one SDL_GPUCommandBuffer per frame, alternating copy
     * passes and render passes (SDL3 GPU forbids nesting but allows
     * interleaving). renderPassActive tracks whether a render pass is open. */
    SDL_GPUCommandBuffer* commandBuffer;
    SDL_GPURenderPass* currentPass;
    int isRecording;
    int renderPassActive;
    int sceneCount;
    uint32_t currentFrame;
    int frameFlushed;
    int renderingStarted;

    /* Fences per frame slot. frameFence signals the main submit; ringUploadFence
     * signals the ring upload submit. The queue is FIFO so waiting frameFence
     * also implies the ring upload completed, but both are waited before the
     * slot's transfer buffers are reused in SDL3REND_EnsureRecording. */
    SDL_GPUFence* frameFence[MAX_FRAMES_IN_FLIGHT];
    SDL_GPUFence* ringUploadFence[MAX_FRAMES_IN_FLIGHT];

    /* Per-frame CPU->GPU staging for texture uploads. SDL3REND_UploadBufferToImage
     * memcpy's into the current slot's mapped transfer buffer, then records a
     * copy pass into the slot's own upload command buffer and submits it
     * immediately with uploadFence. The slot is never reused until that fence
     * signals, so uploads are safe to record from anywhere (frame loop or
     * out-of-frame asset loading). No manual barriers — SDL3 GPU inserts them. */
    SDL_GPUTransferBuffer* stagingTransfer[MAX_FRAMES_IN_FLIGHT];
    void* stagingMapped[MAX_FRAMES_IN_FLIGHT];
    size_t stagingOffset[MAX_FRAMES_IN_FLIGHT];
    size_t stagingSize;
    SDL_GPUFence* uploadFence[MAX_FRAMES_IN_FLIGHT];

    /* Shared persistent dynamic VBO/IBO rings for small models (electro-ray
     * segments, sparks, dim quads, pratcam quad), mirroring the sdl3rend
     * driver. SDL3 GPU cannot bind host-visible memory as vertex/index buffers,
     * so the ring is split in two halves:
     *
     *   dyn*Transfer (mapped SDL_GPUTransferBuffer)  — written by memcpy during
     *       frame recording (build_vbo/build_ibo/SDL3REND_RefreshRingStored).
     *   dynVbo/dynIbo (SDL_GPUBuffer, VERTEX/INDEX usage) — bound by draws.
     *
     * The cursors only advance within a frame. At present, before the main
     * submit, a ring-upload command buffer copies the written region
     * [0, dyn*Written) from the transfer buffer into the GPU ring. Because it
     * is submitted first and the queue is FIFO, every draw in the main buffer
     * sees the ring data regardless of where in the frame it was written —
     * RefreshRingStored mid-render-pass works exactly like the VK driver.
     * The per-slot ring data is never reused while the GPU may still reference
     * it (cursors reset only after the fence waits in EnsureRecording).
     * Ring usage is gated on isRecording (models built at load time keep
     * dedicated buffers). */
    SDL_GPUTransferBuffer* dynVboTransfer[MAX_FRAMES_IN_FLIGHT];
    void* dynVboMapped[MAX_FRAMES_IN_FLIGHT];
    SDL_GPUBuffer* dynVbo[MAX_FRAMES_IN_FLIGHT];
    size_t dynVboOffset[MAX_FRAMES_IN_FLIGHT];
    size_t dynVboWritten[MAX_FRAMES_IN_FLIGHT];
    size_t dynVboCapacity;
    SDL_GPUTransferBuffer* dynIboTransfer[MAX_FRAMES_IN_FLIGHT];
    void* dynIboMapped[MAX_FRAMES_IN_FLIGHT];
    SDL_GPUBuffer* dynIbo[MAX_FRAMES_IN_FLIGHT];
    size_t dynIboOffset[MAX_FRAMES_IN_FLIGHT];
    size_t dynIboWritten[MAX_FRAMES_IN_FLIGHT];
    size_t dynIboCapacity;
    /* Monotonic counter bumped once per frame (in SDL3REND_EnsureRecording after
     * the ring cursors reset). Stored geometries that sub-allocate from the ring
     * stamp ringEpoch with this value; a mismatch at render time means the ring
     * slot was reset since the model was built, so its geometry is stale and
     * must be re-uploaded. */
    uint32_t frameEpoch;

    /* Overlay / 3DFX 2D composite state (unchanged semantics from sdl3rend).
     * lockedPixels holds the CPU 2D surface; dirty regions are uploaded into
     * overlayTexture; the overlay quad (overlayPipeline) is drawn inside the
     * scene's render pass so the 2D content composites on top of the 3D. */
    SDL_GPUSampler* overlaySampler;
    SDL_GPUBuffer* overlayQuadVbo;
    SDL_GPUBuffer* overlayQuadIbo;
    int overlayDirty;
    SDL_GPUTexture* overlayTexture;
    int dimAreaCount;
    br_rectangle dimAreas[8];
    int clearAreaCount;
    br_rectangle clearAreas[4];
    int pratcamAreaCount;
    br_rectangle pratcamArea;
    void* lockedPixels;
    int pm_type;
    int pm_width;
    int pm_height;
    int pm_row_bytes;
    struct br_device_pixelmap* primaryColourTarget;

    int viewportX;
    int viewportY;
    int viewportW;
    int viewportH;
    int mainViewportX;
    int mainViewportY;
    int mainViewportW;
    int mainViewportH;

    /* Draw-state caching to avoid redundant binds. */
    SDL_GPUBuffer* lastVbo;
    SDL_GPUBuffer* lastIbo;
    size_t lastVboOffset;
    size_t lastIboOffset;
    SDL_GPUTexture* lastTexture;
    SDL_GPUSampler* lastSampler;
    SDL_GPUGraphicsPipeline* lastPipeline;

    /* Default / fallback resources. */
    SDL_GPUTexture* defaultTexture;
    SDL_GPUSampler* samplerLinear;
    SDL_GPUSampler* samplerNearest;

    shader_data_scene sceneData;
    shader_data_model modelData;
    shader_data_light lightData;

    /* Host-side hooks copied from br_device_sdl3_callback_procs at
     * SDL3REND_VideoOpen. Optional — the driver tolerates NULL (no map mode,
     * no resize detection), which is what keeps the driver buildable/runnable
     * outside dethrace. */
    br_device_sdl3_get_map_mode_cbfn      *get_map_mode;
    br_device_sdl3_get_window_size_cbfn   *get_window_size;
} VIDEO, *HVIDEO;

static inline int SDL3REND_IsMapMode(HVIDEO hVideo) {
    return hVideo->get_map_mode ? hVideo->get_map_mode() : 0;
}

static inline void SDL3REND_GetWindowSize(HVIDEO hVideo, int* width, int* height) {
    if (hVideo->get_window_size) {
        hVideo->get_window_size(width, height);
    } else {
        *width = 0;
        *height = 0;
    }
}

HVIDEO SDL3REND_VideoOpen(HVIDEO hVideo, void* parent,
    const char* brender_vert_spv, size_t brender_vert_size,
    const char* brender_frag_spv, size_t brender_frag_size,
    const char* overlay_vert_spv, size_t overlay_vert_size,
    const char* overlay_frag_spv, size_t overlay_frag_size,
    const char* default_vert_spv, size_t default_vert_size,
    const char* default_frag_spv, size_t default_frag_size,
    br_device_sdl3_callback_procs* callbacks, int width, int height);

void SDL3REND_VideoClose(HVIDEO hVideo);

void SDL3REND_VideoResize(HVIDEO hVideo);

SDL_GPUShader* SDL3REND_CreateShader(HVIDEO hVideo, const char* code, size_t code_size, SDL_GPUShaderStage stage);

SDL_GPUGraphicsPipeline* SDL3REND_CreateGraphicsPipeline(HVIDEO hVideo,
    SDL_GPUShader* vertModule, SDL_GPUShader* fragModule,
    const SDL_GPUVertexBufferDescription* bindingDesc,
    const SDL_GPUVertexAttribute* attrDescs, uint32_t attrCount,
    uint32_t width, uint32_t height, bool blendEnable,
    bool depthTestEnable, bool depthWriteEnable);

/* Copies the scene UBO payload into hVideo->sceneData. The actual push to the
 * GPU happens in SDL3REND_SceneBegin (uniform slot 1 on both stages), so the
 * scene only needs one push per pass, not one per draw. */
void SDL3REND_UpdateScene(HVIDEO hVideo, void* data, size_t size);

/* Pushes the current scene UBO (hVideo->sceneData) to uniform slot 1 on both
 * stages. Called once per scene before the first model draw. */
void SDL3REND_SceneBegin(HVIDEO hVideo);

/* Pushes the per-draw model payload to uniform slot 0 on both stages. Called
 * by modelrender.c immediately before the draw. */
void SDL3REND_PushModel(HVIDEO hVideo, const void* data, size_t size);

void SDL3REND_BeginRenderPass(HVIDEO hVideo);

void SDL3REND_EndRenderPass(HVIDEO hVideo);

void SDL3REND_EnsureRecording(HVIDEO hVideo);

/* Uploads the current render pass's framebuffer contents to the swapchain and
 * submits the frame. First submits the ring upload (copy ring transfer buffer
 * -> ring GPU buffer) so the main submit's draws see this frame's ring data,
 * then submits the main command buffer. Returns nonzero on error. */
int SDL3REND_Present(HVIDEO hVideo);

struct br_geometry_stored;
void SDL3REND_RefreshRingStored(HVIDEO hVideo, struct br_geometry_stored* self);

/* Draws the overlay quad (samples overlayTexture) into the currently active
 * render pass. */
void SDL3REND_OverlayDraw(HVIDEO hVideo);

/* Uploads `hostDataSize` bytes of host memory into `texture` (width x height
 * at dstX,dstY) through the current frame slot's staging transfer buffer. The
 * data is memcpy'd synchronously, then a copy pass is recorded into the slot's
 * upload command buffer and submitted immediately with the slot's uploadFence.
 * The next call on this slot waits that fence before reusing the staging, so
 * uploads are safe from the frame loop or out-of-frame track/asset loading.
 * SDL3 GPU handles the layout transitions. Returns 0 on success, nonzero on
 * failure. */
int SDL3REND_UploadBufferToImage(HVIDEO hVideo, SDL_GPUTexture* texture,
    uint32_t width, uint32_t height, uint32_t dstX, uint32_t dstY,
    const void* hostData, size_t hostDataSize);

/* Uploads host memory into a GPU-local buffer (vertex/index/any) through the
 * current frame slot's staging transfer buffer. Same lifecycle as
 * SDL3REND_UploadBufferToImage; safe from the frame loop or out-of-frame
 * asset loading. Returns 0 on success, nonzero on failure. */
int SDL3REND_UploadBufferToBuffer(HVIDEO hVideo, SDL_GPUBuffer* buffer,
    const void* hostData, size_t hostDataSize);

/* Releases a texture/sampler/buffer/pipeline that may still be in use by the
 * GPU. SDL3 GPU resources are reference-counted and SDL_ReleaseGPU* schedules
 * the safe destruction, so this is just a direct release — no deferred-free
 * lists needed (unlike the VK driver's manual memory management). */
void SDL3REND_DeferFreeImage(HVIDEO hVideo, SDL_GPUTexture* texture, SDL_GPUSampler* sampler);
void SDL3REND_DeferFreeBuffer(HVIDEO hVideo, SDL_GPUBuffer* buffer);

/* Fills a screen-space rectangle in the CPU locked buffer with the transparent
 * magenta sentinel so it isn't composited over GPU-rendered content. */
static inline void SDL3REND_PurgeRect(int bpp, br_uint_32 magenta, void* pixels,
    int pm_width, int pm_height, int pm_row_bytes,
    int x, int y, int w, int h) {
    int row_w = pm_row_bytes / bpp;
    for (int dy = 0; dy < h; dy++) {
        int py = y + dy;
        if (py < 0 || py >= pm_height) continue;
        int off = py * row_w + x;
        int cw = w;
        if (x < 0) { off -= x; cw += x; }
        if (x + cw > pm_width) cw = pm_width - x;
        if (off < 0) continue;
        for (int dx = 0; dx < cw; dx++) {
            if (bpp == 2)
                ((br_uint_16*)pixels)[off + dx] = (br_uint_16)magenta;
            else
                ((br_uint_32*)pixels)[off + dx] = magenta;
        }
    }
}

#ifdef __cplusplus
};
#endif

#endif
