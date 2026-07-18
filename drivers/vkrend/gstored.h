#ifndef _GSTORED_H_
#define _GSTORED_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <vulkan/vulkan.h>

typedef struct vk_vertex_f {
    br_vector3_f p;
    br_vector2_f map;
    br_vector3_f n;
    br_vector4_f c;
} vk_vertex_f;

typedef struct vk_groupinfo {
    uint32_t count;
    uint32_t offset;
    struct v11group* group;
    br_renderer_state_stored* stored;
    br_renderer_state_stored* default_state;
} vk_groupinfo;

#ifdef BR_GEOMETRY_STORED_PRIVATE

typedef struct br_geometry_stored {
    const struct br_geometry_stored_dispatch* dispatch;
    const char* identifier;
    struct br_device* device;

    struct br_geometry_v1_model* gv1model;

    br_boolean shared;
    struct v11model* model;

    VkBuffer vbo;
    VkDeviceMemory vboMemory;
    VkBuffer ibo;
    VkDeviceMemory iboMemory;

    int num_groups;

    VkDevice deviceHandle;
    struct _VIDEO* hVideo;

    vk_groupinfo* vk_groups;
} br_geometry_stored;

#endif

#ifdef __cplusplus
};
#endif
#endif /* _GSTORED_H_ */
