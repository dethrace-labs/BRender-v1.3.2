#ifndef _DEVCLUT_H_
#define _DEVCLUT_H_

#ifdef __cplusplus
extern "C" {
#endif

#define CLUT_SIZE 256

typedef struct br_device_clut {
    struct br_device_clut_dispatch* dispatch;
    char* identifier;
    br_colour entries[CLUT_SIZE];
    br_device* device;
    br_uint_32 revision;
} br_device_clut;

#ifdef __cplusplus
};
#endif
#endif
