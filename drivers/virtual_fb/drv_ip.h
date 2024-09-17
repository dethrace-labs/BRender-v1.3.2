/*
 * Prototypes for functions internal to driver
 */
#ifndef _DRV_IP_H_
#define _DRV_IP_H_

#ifndef NO_PROTOTYPES

#ifdef __cplusplus
extern "C" {
#endif

/*
 * driver.c
 */
extern br_device DriverDeviceVirtualFB;

/*
 * object.c
 */
char* BR_CMETHOD_DECL(br_object_virtualfb, identifier)(br_object* self);
br_device* BR_CMETHOD_DECL(br_object_virtualfb, device)(br_object* self);

/*
 * device.c
 */
br_device* DeviceVirtualFBAllocate(char* identifier);

/*
 * outclass.c
 */
br_output_facility* OutputFacilityVirtualFBAllocate(br_device* dev, char* identifier,
    br_int_32 mode, br_int_32 width, br_int_32 height, br_int_32 bits, br_int_32 type, br_boolean indexed);

/*
 * devpixmp.c
 */
br_device_pixelmap* DevicePixelmapVirtualFBAllocate(br_device* dev, br_output_facility* type, br_uint_16 w, br_uint_16 h);

/*
 * devclut.c
 */
br_device_clut* DeviceClutVirtualFBAllocate(br_device* dev, char* identifier);

/*
 * ocfree.c
 */
br_error ObjectContainerFree(struct br_object_container* self, br_token type, char* pattern, br_token_value* tv);

#ifdef __cplusplus
};
#endif

#endif
#endif
