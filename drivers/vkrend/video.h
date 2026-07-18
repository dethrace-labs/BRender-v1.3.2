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

    struct {
        VkDescriptorSetLayout layout;
        VkBuffer sceneBuffer;
        VkDeviceMemory sceneMemory;
        VkBuffer modelBuffer;
        VkDeviceMemory modelMemory;
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
    br_rectangle dimAreas[4];
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
    VkImageView lastTextureView;
    VkSampler lastTextureSampler;

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

void VK_UpdateSceneUBO(HVIDEO hVideo, void* data, size_t size, VkDeviceSize offset);

void VK_UpdateModelUBOAtOffset(HVIDEO hVideo, void* data, size_t size, VkDeviceSize offset);

void VK_BeginRenderPass(HVIDEO hVideo, VkCommandBuffer cmd);

void VK_BeginOverlayRenderPass(HVIDEO hVideo, VkCommandBuffer cmd);

void VK_EndRenderPass(HVIDEO hVideo, VkCommandBuffer cmd);

void VK_OverlayDraw(HVIDEO hVideo, VkCommandBuffer cmd);

VkResult VK_Present(HVIDEO hVideo);

br_error VK_BrPixelmapGetTypeDetails(br_uint_8 pmType, VkFormat* format, VkImageTiling* tiling,
    VkImageUsageFlags* usage, VkMemoryPropertyFlags* memProps);

void VK_DeferFreeImage(HVIDEO hVideo, VkImage image, VkImageView view, VkSampler sampler, VkDeviceMemory memory);

VkResult VK_CreateStagingBuffer(HVIDEO hVideo, VkDeviceSize size, VkBuffer* outBuffer, VkDeviceMemory* outMemory);

static inline uint32_t VK_FindMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    return UINT32_MAX;
}

#ifdef __cplusplus
};
#endif

#endif
