#ifndef _DEVICE_H_
#define _DEVICE_H_

#ifdef BR_DEVICE_PRIVATE

typedef struct br_device {
    const struct br_device_dispatch* dispatch;
    const char* identifier;
    struct br_device* device;
    void* object_list;
    void* res;
    struct device_templates templates;
    struct br_output_facility* output_facility;
    struct br_renderer_facility* renderer_facility;
    struct br_device_clut* clut;
} br_device;

#endif /* BR_DEVICE_PRIVATE */
#endif /* _DEVICE_H_ */
