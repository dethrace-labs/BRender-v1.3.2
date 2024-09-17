/*
 * Driver interface functions
 */
#include <stddef.h>
#include <string.h>

#include "brassert.h"
#include "drv.h"
#include "shortcut.h"

BR_RCS_ID("$Id: driver.c 1.1 1997/12/10 16:45:39 jon Exp $");

/*
 * Main entry point for device - this may get redefined by the makefile
 */
br_device* BR_EXPORT BrDrv1VirtualFBBegin(char* arguments) {
    int i, type_count;
    br_device* device;
    br_token_value args_tv[256], *tp;

    /*
     * Set up device
     */
    device = DeviceVirtualFBAllocate("virtualframebuffer");

    if (device == NULL)
        return NULL;

    /*
     * Setup the output facility
     */
    if (OutputFacilityVirtualFBAllocate(device, "v_fb:320x200", 0x13, 320, 200, 8, BR_PMT_INDEX_8, BR_TRUE) == NULL) {
        /*
         * If nothing is available, then don't admit to being a device
         */
        ObjectFree(device);
        return NULL;
    }

    if (OutputFacilityVirtualFBAllocate(device, "v_fb:640x480", 0x13, 640, 480, 8, BR_PMT_INDEX_8, BR_TRUE) == NULL) {
        /*
         * If nothing is available, then don't admit to being a device
         */
        ObjectFree(device);
        return NULL;
    }

    return device;
}
