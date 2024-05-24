#include "priminfo.h"
#include <stdarg.h>
#include "work.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "fpsetup.h"
#include "work.h"

void BR_ASM_CALL TriangleRender_Z_I8_D16(brp_block *block, brp_vertex *v0, brp_vertex *v1,brp_vertex *v2) {
    // no-op triangle
}

void BR_ASM_CALL TriangleRender_Z_I8_D16_ShadeTable(brp_block *block, brp_vertex *v0, brp_vertex *v1,brp_vertex *v2) {
    // Not implemented
    BrAbort();
}
