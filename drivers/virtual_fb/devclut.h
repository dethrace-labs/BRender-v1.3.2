/*
 * Private device CLUT state
 */
#ifndef _DEVCLUT_H_
#define _DEVCLUT_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Private state of device CLUT
 */
typedef struct br_device_clut {
    /*
     * Dispatch table
     */
    struct br_device_clut_dispatch* dispatch;

    /*
     * Standard handle identifier
     */
    char* identifier;

    /*
     * Device pointer
     */
    br_device* device;

} br_device_clut;

#define VGA_PAL_READ 0x3c7
#define VGA_PAL_WRITE 0x3c8
#define VGA_PAL_DATA 0x3c9

#ifdef __cplusplus
};
#endif
#endif
