/*
 * Local methods for default object object
 *
 */
#include <stddef.h>
#include <string.h>

#include "brassert.h"
#include "drv.h"
#include "shortcut.h"

BR_RCS_ID("$Id: object.c 1.1 1997/12/10 16:45:44 jon Exp $");

/*
 * Get identifier associated with a object
 */
char* BR_CMETHOD_DECL(br_object_virtualfb, identifier)(br_object* self) {
    return self->identifier;
}

/*
 * Find the device assocaited with a object
 */
br_device* BR_CMETHOD_DECL(br_object_virtualfb, device)(br_object* self) {
    return self->device;
}
