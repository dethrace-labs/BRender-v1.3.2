#ifndef _DEVPIXMP_H_
#define _DEVPIXMP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <vulkan/vulkan.h>



typedef struct {
    float x, y, z;
    float r, g, b;
    float u, v;
} br_device_pixelmap_vk_tri;

typedef struct br_device_pixelmap_vk_quad {
    br_device_pixelmap_vk_tri tris[4];
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    VkBuffer buffer;
    VkDeviceMemory memory;
} br_device_pixelmap_vk_quad;

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
            br_device_vk_callback_procs callbacks;
            VIDEO video;
            void* vk_context;
            const char* vk_version;
            const char* vk_vendor;
            const char* vk_renderer;
            VkDevice vk_device;
            VkPhysicalDevice vk_physical_device;
            br_int_32 num_refs;
        } asFront;
        struct {
            struct br_device_pixelmap* depthbuffer;
            VkImage vkImage;
            VkDeviceMemory vkMemory;
            VkImageView vkImageView;
            VkFramebuffer vkFramebuffer;
            VkCommandBuffer vkCommandBuffer;
            void* lockedPixels;
            VkImage overlayImage;
            VkDeviceMemory overlayMemory;
            VkImageView overlayImageView;
            int possiblyDirty;
            int locked;
        } asBack;
        struct {
            struct br_device_pixelmap* backbuffer;
            VkImage vkDepth;
            VkDeviceMemory vkDepthMemory;
            VkImageView vkDepthView;
        } asDepth;
    };
    struct br_device_clut* clut;
} br_device_pixelmap;

/* Forward declarations for br_device_pixelmap_vk methods defined in devpixmp.c */
extern void BR_CMETHOD_DECL(br_device_pixelmap_vk, free)(br_object* _self);
extern const char* BR_CMETHOD_DECL(br_device_pixelmap_vk, identifier)(br_object* self);
extern br_token BR_CMETHOD_DECL(br_device_pixelmap_vk, type)(br_object* self);
extern br_boolean BR_CMETHOD_DECL(br_device_pixelmap_vk, isType)(br_object* self, br_token t);
extern br_device* BR_CMETHOD_DECL(br_device_pixelmap_vk, device)(br_object* self);
extern br_size_t BR_CMETHOD_DECL(br_device_pixelmap_vk, space)(br_object* self);
extern struct br_tv_template* BR_CMETHOD_DECL(br_device_pixelmap_vk, templateQuery)(br_object* _self);
extern br_error BR_CMETHOD_DECL(br_device_pixelmap_vk, resize)(br_device_pixelmap* self, br_int_32 width, br_int_32 height);
extern br_error BR_CMETHOD_DECL(br_device_pixelmap_vk, match)(br_device_pixelmap* self, br_device_pixelmap** newpm, br_token_value* tv);
extern br_error BR_CMETHOD_DECL(br_device_pixelmap_vk, rectangleStretchCopy)(br_device_pixelmap* self, br_rectangle* d, br_device_pixelmap* src, br_rectangle* s);
extern br_error BR_CMETHOD_DECL(br_device_pixelmap_vk, rectangleCopy)(br_device_pixelmap* self, br_point* p, br_device_pixelmap* src, br_rectangle* sr);
extern br_error BR_CMETHOD_DECL(br_device_pixelmap_vk, rectangleCopyTo)(br_device_pixelmap* self, br_point* p, br_device_pixelmap* src, br_rectangle* sr);
extern br_error BR_CMETHOD_DECL(br_device_pixelmap_vk, rectangleFill)(br_device_pixelmap* self, br_rectangle* rect, br_uint_32 colour);
extern br_error BR_CMETHOD_DECL(br_device_pixelmap_vk, allocateSub)(br_device_pixelmap* self, br_device_pixelmap** newpm, br_rectangle* rect);
extern br_error BR_CMETHOD_DECL(br_device_pixelmap_vk, flush)(br_device_pixelmap* self);
extern br_error BR_CMETHOD_DECL(br_device_pixelmap_vk, directLock)(br_device_pixelmap* self, br_boolean block);
extern br_error BR_CMETHOD_DECL(br_device_pixelmap_vk, directUnlock)(br_device_pixelmap* self);

#endif

#ifdef __cplusplus
};
#endif
#endif
