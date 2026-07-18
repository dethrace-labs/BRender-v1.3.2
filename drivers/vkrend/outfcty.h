#ifndef _OUTFCTY_H_
#define _OUTFCTY_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef BR_OUTPUT_FACILITY_PRIVATE

typedef struct br_output_facility {
    const struct br_output_facility_dispatch *dispatch;
    const char *identifier;
    struct br_device *device;
    void *object_list;
    struct br_renderer_facility *renderer_facility;
} br_output_facility;

#endif /* BR_OUTPUT_FACILITY_PRIVATE */

#ifdef __cplusplus
};
#endif
#endif
