#ifndef _RENDFCTY_H_
#define _RENDFCTY_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef BR_RENDERER_FACILITY_PRIVATE

typedef struct br_renderer_facility {
    const struct br_renderer_facility_dispatch *dispatch;
    const char *identifier;
    struct br_device *device;
    void *object_list;
} br_renderer_facility;

#endif

#ifdef __cplusplus
};
#endif
#endif
