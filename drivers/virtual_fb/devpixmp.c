/*
 * Device pixelmap methods
 */
#include <stddef.h>
#include <string.h>

#include "brassert.h"
#include "drv.h"
#include "host.h"
#include "pm.h"
#include "shortcut.h"

BR_RCS_ID("$Id: devpixmp.c 1.2 1998/10/21 15:41:12 jon Exp $");

/*
 * Display mode and stride are fixed for MCGA
 */
#define BIOS_MODE 0x13
#define BIOS_STRIDE 320

/*
 * Default dispatch table for device pixelmap (defined at end of file)
 */
static struct br_device_pixelmap_dispatch devicePixelmapDispatch;

/*
 * Device pixelmap info. template
 */
#define F(f) offsetof(struct br_device_pixelmap, f)

static struct br_tv_template_entry devicePixelmapTemplateEntries[] = {
    {
        BRT_WIDTH_I32,
        0,
        F(pm_width),
        BRTV_QUERY | BRTV_ALL,
        BRTV_CONV_I32_U16,
    },
    {
        BRT_HEIGHT_I32,
        0,
        F(pm_height),
        BRTV_QUERY | BRTV_ALL,
        BRTV_CONV_I32_U16,
    },
    {
        BRT_PIXEL_TYPE_U8,
        0,
        F(pm_type),
        BRTV_QUERY | BRTV_ALL,
        BRTV_CONV_I32_U8,
    },
    //	{BRT_PIXEL_CHANNELS_TL,	0,	0,					BRTV_QUERY | BRTV_ALL,	BRTV_CONV_CUSTOM, },
    //	{BRT_INDEXED_B,			0,	F(indexed),			BRTV_QUERY | BRTV_ALL,	BRTV_CONV_COPY, },
    {
        BRT_OUTPUT_FACILITY_O,
        0,
        F(output_facility),
        BRTV_QUERY | BRTV_ALL,
        BRTV_CONV_COPY,
    },
    {
        BRT_FACILITY_O,
        0,
        F(output_facility),
        BRTV_QUERY,
        BRTV_CONV_COPY,
    },
    {
        BRT_IDENTIFIER_CSTR,
        0,
        F(pm_identifier),
        BRTV_QUERY | BRTV_ALL,
        BRTV_CONV_COPY,
    },
    {
        BRT_CLUT_O,
        0,
        F(clut),
        BRTV_QUERY | BRTV_ALL,
        BRTV_CONV_COPY,
    }
};
#undef F

struct pixelmapNewTokens {
    br_int_32 width;
    br_int_32 height;

    br_device_virtualfb_callback_procs* callbacks;
};

#define F(f) offsetof(struct pixelmapNewTokens, f)
static struct br_tv_template_entry pixelmapNewTemplateEntries[] = {
    { BRT(WIDTH_I32), F(width), BRTV_SET, BRTV_CONV_COPY },
    { BRT(HEIGHT_I32), F(height), BRTV_SET, BRTV_CONV_COPY },
    { BRT(VIRTUALFB_CALLBACKS_P), F(callbacks), BRTV_SET, BRTV_CONV_COPY }
};
#undef F

/*
 * Create a new device pixelmap and set a display mode
 */
br_device_pixelmap* DevicePixelmapVirtualFBAllocate(br_device* dev, br_output_facility* facility, br_token_value* tv) {
    br_device_pixelmap* self;
    br_error r;
    br_uint_16 original_mode;
    br_uint_16 sel;
    br_int_32 count;

    struct pixelmapNewTokens pt = {
        .width = -1,
        .height = -1,
        .callbacks = NULL,
    };

    if (dev->active)
        return NULL;

    if (dev->templates.devicePixelmapNewTemplate == NULL) {
        dev->templates.devicePixelmapNewTemplate = BrTVTemplateAllocate(dev, pixelmapNewTemplateEntries,
            BR_ASIZE(pixelmapNewTemplateEntries));
    }

    dev->active = BR_TRUE;

    self = BrResAllocate(DeviceVirtualFBResource(dev), sizeof(*self), BR_MEMORY_OBJECT_DATA);

    BrTokenValueSetMany(&pt, &count, NULL, tv, dev->templates.devicePixelmapNewTemplate);

    self->dispatch = &devicePixelmapDispatch;
    self->pm_identifier = ObjectIdentifier(facility);
    self->device = dev;
    self->restore_mode = BR_TRUE;
    self->original_mode = original_mode;

    self->pm_type = BR_PMT_INDEX_8;
    self->pm_origin_x = 0;
    self->pm_origin_y = 0;

    self->pm_flags = BR_PMF_ROW_WHOLEPIXELS | BR_PMF_LINEAR;
    self->pm_base_x = 0;
    self->pm_base_y = 0;

    self->pm_width = pt.width;
    self->pm_row_bytes = pt.width;
    self->pm_height = pt.height;
    self->callbacks = pt.callbacks;

    self->pm_pixels = BrResAllocate(self, self->pm_row_bytes * self->pm_height, BR_MEMORY_PIXELS);

    self->output_facility = facility;
    self->clut = DeviceVirtualFBClut(dev);
    self->clut->callbacks = pt.callbacks;

    ObjectContainerAddFront(facility, (br_object*)self);

    return self;
}

static void BR_CMETHOD_DECL(br_device_pixelmap_virtualfb, free)(br_device_pixelmap* self) {

    ObjectContainerRemove(self->output_facility, (br_object*)self);
    self->output_facility->num_instances--;

    /*
     * Free up pixelmap structure
     */
    BrResFreeNoCallback(self);
}

static br_device* BR_CMETHOD_DECL(br_device_pixelmap_virtualfb, device)(br_device_pixelmap* self) {
    return self->device;
}

static br_token BR_CMETHOD_DECL(br_device_pixelmap_virtualfb, type)(br_device_pixelmap* self) {
    return BRT_DEVICE_PIXELMAP;
}

static br_boolean BR_CMETHOD_DECL(br_device_pixelmap_virtualfb, isType)(br_device_pixelmap* self, br_token t) {
    return (t == BRT_DEVICE_PIXELMAP) || (t == BRT_OBJECT);
}

static br_int_32 BR_CMETHOD_DECL(br_device_pixelmap_virtualfb, space)(br_device_pixelmap* self) {
    return sizeof(br_device_pixelmap);
}

static struct br_tv_template* BR_CMETHOD_DECL(br_device_pixelmap_virtualfb, queryTemplate)(br_device_pixelmap* self) {
    if (self->device->templates.devicePixelmapTemplate == NULL)
        self->device->templates.devicePixelmapTemplate = BrTVTemplateAllocate(self->device,
            devicePixelmapTemplateEntries,
            BR_ASIZE(devicePixelmapTemplateEntries));

    return self->device->templates.devicePixelmapTemplate;
}

static br_error BR_CMETHOD_DECL(br_device_pixelmap_virtualfb, doubleBuffer)(br_device_pixelmap* self, br_device_pixelmap* src) {
    br_error result;

    if (self->callbacks && self->callbacks->swap_buffers) {
        self->callbacks->swap_buffers(src);
    }

    return BRE_OK;
}

/*
 * Default dispatch table for device pixelmap
 */
static struct br_device_pixelmap_dispatch devicePixelmapDispatch = {
    NULL,
    NULL,
    NULL,
    NULL,
    BR_CMETHOD_REF(br_device_pixelmap_virtualfb, free),
    BR_CMETHOD_REF(br_object_virtualfb, identifier),
    BR_CMETHOD_REF(br_device_pixelmap_virtualfb, type),
    BR_CMETHOD_REF(br_device_pixelmap_virtualfb, isType),
    BR_CMETHOD_REF(br_device_pixelmap_virtualfb, device),
    BR_CMETHOD_REF(br_device_pixelmap_virtualfb, space),

    BR_CMETHOD_REF(br_device_pixelmap_virtualfb, queryTemplate),
    BR_CMETHOD_REF(br_object, query),
    BR_CMETHOD_REF(br_object, queryBuffer),
    BR_CMETHOD_REF(br_object, queryMany),
    BR_CMETHOD_REF(br_object, queryManySize),
    BR_CMETHOD_REF(br_object, queryAll),
    BR_CMETHOD_REF(br_object, queryAllSize),

    BR_CMETHOD_REF(br_device_pixelmap_mem, validSource),
    BR_CMETHOD_REF(br_device_pixelmap_mem, resize),
    BR_CMETHOD_REF(br_device_pixelmap_mem, match),
    BR_CMETHOD_REF(br_device_pixelmap_mem, allocateSub),

    BR_CMETHOD_REF(br_device_pixelmap_mem, copyTo),
    BR_CMETHOD_REF(br_device_pixelmap_mem, copyTo),
    BR_CMETHOD_REF(br_device_pixelmap_mem, copyFrom),
    BR_CMETHOD_REF(br_device_pixelmap_mem, fill),
    BR_CMETHOD_REF(br_device_pixelmap_virtualfb, doubleBuffer),

    BR_CMETHOD_REF(br_device_pixelmap_gen, copyDirty),
    BR_CMETHOD_REF(br_device_pixelmap_gen, copyToDirty),
    BR_CMETHOD_REF(br_device_pixelmap_gen, copyFromDirty),
    BR_CMETHOD_REF(br_device_pixelmap_gen, fillDirty),
    BR_CMETHOD_REF(br_device_pixelmap_gen, doubleBufferDirty),

    BR_CMETHOD_REF(br_device_pixelmap_gen, rectangle),
    BR_CMETHOD_REF(br_device_pixelmap_gen, rectangle2),
    BR_CMETHOD_REF(br_device_pixelmap_mem, rectangleCopyTo),
    BR_CMETHOD_REF(br_device_pixelmap_mem, rectangleCopyTo),
    BR_CMETHOD_REF(br_device_pixelmap_mem, rectangleCopyFrom),
    BR_CMETHOD_REF(br_device_pixelmap_mem, rectangleStretchCopyTo),
    BR_CMETHOD_REF(br_device_pixelmap_mem, rectangleStretchCopyTo),
    BR_CMETHOD_REF(br_device_pixelmap_mem, rectangleStretchCopyFrom),
    BR_CMETHOD_REF(br_device_pixelmap_mem, rectangleFill),
    BR_CMETHOD_REF(br_device_pixelmap_mem, pixelSet),
    BR_CMETHOD_REF(br_device_pixelmap_mem, line),
    BR_CMETHOD_REF(br_device_pixelmap_mem, copyBits),

    BR_CMETHOD_REF(br_device_pixelmap_gen, text),
    BR_CMETHOD_REF(br_device_pixelmap_gen, textBounds),

    BR_CMETHOD_REF(br_device_pixelmap_mem, rowSize),
    BR_CMETHOD_REF(br_device_pixelmap_mem, rowQuery),
    BR_CMETHOD_REF(br_device_pixelmap_mem, rowSet),

    BR_CMETHOD_REF(br_device_pixelmap_mem, pixelQuery),
    BR_CMETHOD_REF(br_device_pixelmap_mem, pixelAddressQuery),

    BR_CMETHOD_REF(br_device_pixelmap_mem, pixelAddressSet),
    BR_CMETHOD_REF(br_device_pixelmap_mem, originSet),

    BR_CMETHOD_REF(br_device_pixelmap_gen, flush),
    BR_CMETHOD_REF(br_device_pixelmap_gen, synchronise),
    BR_CMETHOD_REF(br_device_pixelmap_gen, directLock),
    BR_CMETHOD_REF(br_device_pixelmap_gen, directUnlock),

    BR_CMETHOD_REF(br_device_pixelmap_gen, getControls),
    BR_CMETHOD_REF(br_device_pixelmap_gen, setControls),
};
