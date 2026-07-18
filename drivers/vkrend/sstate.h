#ifndef _SSTATE_H_
#define _SSTATE_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef BR_RENDERER_STATE_STORED_PRIVATE

typedef struct br_renderer_state_stored {
    const struct br_renderer_state_stored_dispatch *dispatch;
    const char *identifier;
    struct br_device *device;
    struct br_renderer *renderer;
    state_stack state;
} br_renderer_state_stored;

#endif

#ifdef __cplusplus
};
#endif
#endif
