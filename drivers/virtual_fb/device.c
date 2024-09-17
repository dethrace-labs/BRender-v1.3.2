/*
 * Device methods
 */
#include <stddef.h>
#include <string.h>

#include "brassert.h"
#include "drv.h"
#include "shortcut.h"

BR_RCS_ID("$Id: device.c 1.1 1997/12/10 16:45:34 jon Exp $");

#define DEVICE_VERSION BR_VERSION(1, 0, 0)

/*
 * Default dispatch table for device (defined at end of file)
 */
static struct br_device_dispatch deviceDispatch;

/*
 * Device info. template
 */
static const char deviceTitle[] = "VirtualFB VGA v1.0";

static const char deviceCreator[] = "jeff@1amstudios.com";
static const char deviceProduct[] = "VirtualFB VGA";

#define _F(f) offsetof(br_device, f)
#define _A(f) ((br_int_32)(f))
static const struct br_tv_template_entry deviceTemplateEntries[] = {
    {
        BRT_IDENTIFIER_CSTR,
        0,
        _F(identifier),
        BRTV_QUERY | BRTV_ALL,
        BRTV_CONV_COPY,
    },
    {
        BRT_CLUT_O,
        0,
        _F(clut),
        BRTV_QUERY | BRTV_ALL,
        BRTV_CONV_COPY,
    }
};
#undef _A
#undef _F

/*
 * Set up a static device object
 */
br_device* DeviceVirtualFBAllocate(char* identifier) {
    br_device* self;

    /*
     * Set up device block and resource anchor
     */
    self = BrResAllocate(NULL, sizeof(*self), BR_MEMORY_OBJECT_DATA);
    self->res = BrResAllocate(self, 0, BR_MEMORY_DRIVER);

    self->identifier = identifier;
    self->dispatch = &deviceDispatch;
    self->device = self;

    self->object_list = BrObjectListAllocate(self->res);

    /*
     * Build CLUT object
     */
    self->clut = DeviceClutVirtualFBAllocate(self, "Hardware-CLUT");

    return self;
}

static void BR_CMETHOD_DECL(br_device_virtualfb, free)(br_device* self) {
    /*
     * Remove attached objects
     */
    BrObjectContainerFree((br_object_container*)self, BR_NULL_TOKEN, NULL, NULL);

    /*
     * Throw away attached resources
     */
    BrResFreeNoCallback(self);
}

static br_token BR_CMETHOD_DECL(br_device_virtualfb, type)(br_device* self) {
    return BRT_DEVICE;
}

static br_boolean BR_CMETHOD_DECL(br_device_virtualfb, isType)(br_device* self, br_token t) {
    return (t == BRT_DEVICE) || (t == BRT_OBJECT_CONTAINER) || (t == BRT_OBJECT);
}

static br_int_32 BR_CMETHOD_DECL(br_device_virtualfb, space)(br_device* self) {
    return sizeof(br_device);
}

static struct br_tv_template* BR_CMETHOD_DECL(br_device_virtualfb, templateQuery)(br_device* self) {
    if (self->templates.deviceTemplate == NULL)
        self->templates.deviceTemplate = BrTVTemplateAllocate(self,
            deviceTemplateEntries,
            BR_ASIZE(deviceTemplateEntries));

    return self->templates.deviceTemplate;
}

static void* BR_CMETHOD_DECL(br_device_virtualfb, listQuery)(br_device* self) {
    return self->object_list;
}

/*
 * Default dispatch table for device
 */
static struct br_device_dispatch deviceDispatch = {
    NULL,
    NULL,
    NULL,
    NULL,
    BR_CMETHOD_REF(br_device_virtualfb, free),
    BR_CMETHOD_REF(br_object_virtualfb, identifier),
    BR_CMETHOD_REF(br_device_virtualfb, type),
    BR_CMETHOD_REF(br_device_virtualfb, isType),
    BR_CMETHOD_REF(br_object_virtualfb, device),
    BR_CMETHOD_REF(br_device_virtualfb, space),

    BR_CMETHOD_REF(br_device_virtualfb, templateQuery),
    BR_CMETHOD_REF(br_object, query),
    BR_CMETHOD_REF(br_object, queryBuffer),
    BR_CMETHOD_REF(br_object, queryMany),
    BR_CMETHOD_REF(br_object, queryManySize),
    BR_CMETHOD_REF(br_object, queryAll),
    BR_CMETHOD_REF(br_object, queryAllSize),

    BR_CMETHOD_REF(br_device_virtualfb, listQuery),
    BR_CMETHOD_REF(br_object_container, tokensMatchBegin),
    BR_CMETHOD_REF(br_object_container, tokensMatch),
    BR_CMETHOD_REF(br_object_container, tokensMatchEnd),
    BR_CMETHOD_REF(br_object_container, addFront),
    BR_CMETHOD_REF(br_object_container, removeFront),
    BR_CMETHOD_REF(br_object_container, remove),
    BR_CMETHOD_REF(br_object_container, find),
    BR_CMETHOD_REF(br_object_container, findMany),
    BR_CMETHOD_REF(br_object_container, count),
};
