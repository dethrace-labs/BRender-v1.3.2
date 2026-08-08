#ifndef VIDEO_H_
#define VIDEO_H_

#ifdef __cplusplus
extern "C" {
#endif

struct br_device_pixelmap;

#define BR_STATIC_ASSERT(cond, msg) _Static_assert((cond), msg)

#define MAX_FRAMES_IN_FLIGHT 2

typedef struct VK_DeferredBufferFree {
    VkBuffer buffer;
    VkDeviceMemory memory;
} VK_DeferredBufferFree;

typedef struct VK_DeferredImageFree {
    VkImage image;
    VkImageView view;
    VkSampler sampler;
    VkDeviceMemory memory;
} VK_DeferredImageFree;

/* Per-frame CPU->GPU upload staging. Each VK_UploadBufferToImage writes the data
 * into the current frame slot's staging buffer, records barrier+copy+barrier into
 * the slot's upload command buffer, and submits it immediately with the slot's own
 * uploadFence. The slot is never reused until that fence signals, so uploads are
 * safe to record from anywhere (frame loop or out-of-frame track/asset loading)
 * with no vkQueueWaitIdle and no full GPU drain. */
typedef struct VK_FrameStaging {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceSize size;
    VkDeviceSize offset;
    void* mapped;
} VK_FrameStaging;

#pragma pack(push, 16)
typedef struct shader_data_light {
    alignas(16) br_vector4 position;
    alignas(16) br_vector4 direction;
    alignas(16) br_vector4 half;
    alignas(16) br_vector4 colour;
    alignas(16) br_vector4 iclq;
    alignas(16) br_vector2 spot_angles;
    alignas(4) float _pad0, _pad1;
} shader_data_light;
BR_STATIC_ASSERT(sizeof(shader_data_light) % 16 == 0, "shader_data_light is not aligned");

typedef struct shader_data_scene {
    alignas(16) br_vector4 eye_view;
    alignas(16) shader_data_light lights[BR_MAX_LIGHTS];
    alignas(4) uint32_t num_lights;
    alignas(16) br_vector4 clip_planes[BR_MAX_CLIP_PLANES];
    alignas(4) uint32_t num_clip_planes;
    alignas(4) float hither_z;
    alignas(4) float yon_z;
} shader_data_scene;
BR_STATIC_ASSERT(sizeof(((shader_data_scene*)NULL)->lights) == sizeof(shader_data_light) * BR_MAX_LIGHTS,
    "std::array<shader_data_light> fucked up");

typedef struct shader_data_model {
    alignas(16) br_matrix4 model_view;
    alignas(16) br_matrix4 projection;
    alignas(16) br_matrix4 projection_brender;
    alignas(16) br_matrix4 mvp;
    alignas(16) br_matrix4 normal_matrix;
    alignas(16) br_matrix4 environment_matrix;
    alignas(16) br_matrix4 map_transform;
    alignas(16) br_vector4 surface_colour;
    alignas(16) br_vector4 clear_colour;
    alignas(16) br_vector4 eye_m;
    alignas(4) float ka;
    alignas(4) float ks;
    alignas(4) float kd;
    alignas(4) float power;
    alignas(4) uint32_t lighting;
    alignas(4) uint32_t uv_source;
    alignas(4) uint32_t disable_colour_key;
    alignas(4) uint32_t disable_texture;
    alignas(4) uint32_t fog_enabled;
    alignas(16) br_vector4 fog_colour;
    alignas(4) float fog_min;
    alignas(4) float fog_max;
    alignas(4) float alpha;
    alignas(4) uint32_t prelit;
} shader_data_model;
#pragma pack(pop)

typedef struct _VIDEO {
    void* res;
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    uint32_t graphicsFamilyIndex;
    uint32_t presentFamilyIndex;

    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkFormat swapchainImageFormat;
    VkExtent2D swapchainExtent;
    int windowWidth;
    int windowHeight;
    VkImage* swapchainImages;
    VkImageView* swapchainImageViews;
    uint32_t swapchainImageCount;

    VkRenderPass imguiCompatRenderPass;
    VkPipelineLayout defaultPipelineLayout;
    VkPipelineLayout brenderPipelineLayout;
    VkPipeline defaultPipeline;
    VkPipeline brenderPipeline;
    VkPipeline brenderPipelineNoWrite;
    VkPipeline brenderBlendPipeline;
    VkPipeline brenderPipelineNoDepth;
    VkPipeline brenderBlendPipelineNoDepth;

    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkCommandBuffer* drawCommandBuffers;

    VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkFence inFlightFences[MAX_FRAMES_IN_FLIGHT];
    uint32_t currentFrame;

    uint32_t currentImageIndex;
    int isRecording;
    int renderPassActive;
    int sceneCount;
    uint32_t maxUniformBufferRange;
    uint32_t minUniformBufferOffsetAlignment;
    uint32_t maxVertexInputBindings;
    uint32_t maxVertexInputAttributes;
    uint32_t hostMemType;

    struct {
        VkDescriptorSetLayout layout;
        VkBuffer sceneBuffer;
        VkDeviceMemory sceneMemory;
        void* sceneMapped;
        VkBuffer modelBuffer;
        VkDeviceMemory modelMemory;
        void* modelMapped;
    } brenderDescriptors;

    PFN_vkCmdPushDescriptorSetKHR pfnPushDescriptorSet;
    VkDeviceSize currentModelOffset;
    VkDeviceSize modelBufferCapacity;
    VkDeviceSize currentSceneOffset;
    VkDeviceSize sceneBufferCapacity;
    VkDeviceSize sceneSlotSize;
    VkDeviceSize modelSlotSize;
    int sceneSlotIndex;

    VkPipelineLayout overlayPipelineLayout;
    VkPipeline overlayPipeline;
    VkDescriptorSetLayout overlayDescLayout;
    VkDescriptorPool overlayDescPool;
    VkDescriptorSet overlayDescSet;
    VkSampler overlaySampler;
    VkBuffer overlayQuadVbo;
    VkDeviceMemory overlayQuadVboMemory;
    VkBuffer overlayQuadIbo;
    VkDeviceMemory overlayQuadIboMemory;
    int overlayDirty;
    VkImage overlayImage;
    int dimAreaCount;
    br_rectangle dimAreas[8];
    VkDeviceMemory overlayMemory;
    VkImageView overlayImageView;
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
    int frameFlushed;
    int renderingStarted;
    int clearAreaCount;
    br_rectangle clearAreas[4];
    int pratcamAreaCount;
    br_rectangle pratcamArea;

    VkBuffer lastVbo;
    VkBuffer lastIbo;
    VkDeviceSize lastVboOffset;
    VkDeviceSize lastIboOffset;
    VkImageView lastTextureView;
    VkSampler lastTextureSampler;
    VkPipeline lastPipeline;

    /* Shared persistent dynamic VBO/IBO rings for small models. Models rebuilt
     * per frame (electro-ray segments, sparks, dim quads, pratcam quad) get
     * sub-allocated from these rings via a plain memcpy instead of a full
     * vkCreateBuffer/vkAllocateMemory/vkBindBufferMemory per rebuild, which is
     * ~10x slower than GL's glBufferData and tanked FPS to single digits. Each
     * frame slot has its own ring pair: the cursor is reset in VK_EnsureRecording
     * only after the per-slot fence wait, so a slot's ring data is never reused
     * while the GPU may still reference it. Ring usage is gated on isRecording
     * (models built at load time keep dedicated buffers). */
    VkBuffer dynVbo[MAX_FRAMES_IN_FLIGHT];
    VkDeviceMemory dynVboMemory[MAX_FRAMES_IN_FLIGHT];
    void* dynVboMapped[MAX_FRAMES_IN_FLIGHT];
    VkDeviceSize dynVboOffset[MAX_FRAMES_IN_FLIGHT];
    VkDeviceSize dynVboCapacity;
    VkBuffer dynIbo[MAX_FRAMES_IN_FLIGHT];
    VkDeviceMemory dynIboMemory[MAX_FRAMES_IN_FLIGHT];
    void* dynIboMapped[MAX_FRAMES_IN_FLIGHT];
    VkDeviceSize dynIboOffset[MAX_FRAMES_IN_FLIGHT];
    VkDeviceSize dynIboCapacity;
    /* Monotonic counter bumped once per frame (in VK_EnsureRecording after the
     * ring cursors reset). Stored geometries that sub-allocate from the ring
     * stamp ringEpoch with this value; a mismatch at render time means the
     * ring slot was reset since the model was built, so its geometry is stale
     * and must be re-uploaded. */
    uint32_t frameEpoch;

    VkImage defaultTextureImage;
    VkDeviceMemory defaultTextureMemory;
    VkImageView defaultTextureView;
    VkSampler defaultSampler;
    VkSampler samplerLinear;
    VkSampler samplerNearest;

    VkImage depthImage;
    VkDeviceMemory depthMemory;
    VkImageView depthImageView;
    VkFormat depthFormat;

    shader_data_scene sceneData;
    shader_data_model modelData;
    shader_data_light lightData;

    VK_DeferredBufferFree* deferredBufferFrees[MAX_FRAMES_IN_FLIGHT];
    uint32_t deferredBufferFreeCount[MAX_FRAMES_IN_FLIGHT];
    uint32_t deferredBufferFreeCapacity[MAX_FRAMES_IN_FLIGHT];

    VK_DeferredImageFree* deferredImageFrees[MAX_FRAMES_IN_FLIGHT];
    uint32_t deferredImageFreeCount[MAX_FRAMES_IN_FLIGHT];
    uint32_t deferredImageFreeCapacity[MAX_FRAMES_IN_FLIGHT];

    VK_FrameStaging frameStaging[MAX_FRAMES_IN_FLIGHT];
    VkCommandPool uploadPool[MAX_FRAMES_IN_FLIGHT];
    VkCommandBuffer uploadBuffer[MAX_FRAMES_IN_FLIGHT];
    /* Per-slot fence signalling completion of the slot's upload command buffer and
     * staging data. Every VK_UploadBufferToImage waits this fence before reusing
     * the slot, so uploads may be recorded from anywhere (frame loop or out-of-frame
     * track/asset loading) — the slot is simply unavailable until the GPU is done. */
    VkFence uploadFence[MAX_FRAMES_IN_FLIGHT];
} VIDEO, *HVIDEO;

HVIDEO VK_VideoOpen(HVIDEO hVideo, void* parent, const char* vert_spv_data, size_t vert_spv_size,
    const char* frag_spv_data, size_t frag_spv_size,
    br_device_vk_callback_procs* callbacks, int width, int height);

void VK_VideoClose(HVIDEO hVideo);

void VK_VideoRecreateSwapchain(HVIDEO hVideo);

VkShaderModule VK_CreateShaderModule(HVIDEO hVideo, const char* code, size_t code_size, VkShaderStageFlagBits stage);

VkPipeline VK_CreateGraphicsPipeline(HVIDEO hVideo, VkShaderModule vertModule, VkShaderModule fragModule,
    VkPipelineLayout layout,
    VkVertexInputBindingDescription* bindingDesc,
    VkVertexInputAttributeDescription* attrDescs, uint32_t attrCount,
    uint32_t width, uint32_t height, VkBool32 blendEnable, VkBool32 depthTestEnable, VkBool32 depthWriteEnable);

VkPipelineLayout VK_CreatePipelineLayout(HVIDEO hVideo, VkDescriptorSetLayout* descLayout, uint32_t descLayoutCount);

VkResult VK_CreateBrenderDescriptors(HVIDEO hVideo, uint32_t width, uint32_t height);

VkResult VK_CreateDynamicRings(HVIDEO hVideo);

void VK_UpdateSceneUBO(HVIDEO hVideo, void* data, size_t size, VkDeviceSize offset);

void VK_UpdateModelUBOAtOffset(HVIDEO hVideo, void* data, size_t size, VkDeviceSize offset);

void VK_BeginRenderPass(HVIDEO hVideo, VkCommandBuffer cmd);

void VK_BeginOverlayRenderPass(HVIDEO hVideo, VkCommandBuffer cmd);

void VK_EndRenderPass(HVIDEO hVideo, VkCommandBuffer cmd);

void VK_EnsureRecording(HVIDEO hVideo);

struct br_geometry_stored;
void VK_RefreshRingStored(HVIDEO hVideo, struct br_geometry_stored* self);

void VK_DrawOverlay(HVIDEO hVideo, VkCommandBuffer cmd, struct br_device_pixelmap* screen);

void VK_OverlayDraw(HVIDEO hVideo, VkCommandBuffer cmd);

VkResult VK_Present(HVIDEO hVideo);

br_error VK_BrPixelmapGetTypeDetails(br_uint_8 pmType, VkFormat* format, VkImageTiling* tiling,
    VkImageUsageFlags* usage, VkMemoryPropertyFlags* memProps);

void VK_DeferFreeImage(HVIDEO hVideo, VkImage image, VkImageView view, VkSampler sampler, VkDeviceMemory memory);

/* Uploads `hostDataSize` bytes from host memory into `image` (width x height at
 * dstX,dstY) through the current frame slot's staging buffer. The data is memcpy'd
 * into the staging buffer synchronously, then barrier+copy+barrier is recorded and
 * the slot's upload command buffer is submitted IMMEDIATELY with the slot's own
 * uploadFence. The next call on this slot (two frames later in the steady frame
 * loop) waits that fence before resetting the buffer and staging, so uploads are
 * safe to record from anywhere — frame loop OR out-of-frame track/asset loading —
 * and the wait is on the tiny copy's fence, never a full GPU drain. The image is
 * transitioned oldLayout -> TRANSFER_DST -> newLayout. */
VkResult VK_UploadBufferToImage(HVIDEO hVideo, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
    uint32_t width, uint32_t height, uint32_t dstX, uint32_t dstY, VkImageAspectFlags aspectMask,
    const void* hostData, size_t hostDataSize);

static inline uint32_t VK_FindMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    return UINT32_MAX;
}

/* Fills a screen-space rectangle in the CPU locked buffer with the transparent
 * magenta sentinel so it isn't composited over GPU-rendered content. */
static inline void VK_PurgeRect(int bpp, br_uint_32 magenta, void* pixels,
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
