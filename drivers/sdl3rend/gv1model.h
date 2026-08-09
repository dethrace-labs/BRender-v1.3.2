#ifndef _GV1MODEL_H_
#define _GV1MODEL_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef BR_GEOMETRY_V1_MODEL_PRIVATE

typedef struct br_geometry_v1_model {
    const struct br_geometry_v1_model_dispatch *dispatch;
    const char *identifier;
    struct br_device *device;
    struct br_renderer_facility *renderer_facility;
} br_geometry_v1_model;

#endif

#ifdef __cplusplus
};
#endif
#endif
