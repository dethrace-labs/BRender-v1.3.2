/*
 * Private object structure
 */
#ifndef _OBJECT_H_
#define _OBJECT_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Private state of device pixelmap
 */
typedef struct br_object {
    /*
     * Dispatch table
     */
    struct br_object_dispatch* dispatch;

    /*
     * Standard object identifier
     */
    char* identifier;

    /*
     * Device pointer
     */
    br_device* device;

} br_object;

#ifdef __cplusplus
};
#endif
#endif
