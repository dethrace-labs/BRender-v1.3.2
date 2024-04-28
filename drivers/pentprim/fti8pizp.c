#include "brender.h"
#include "priminfo.h"
#include "pfpsetup.h"
#include "x86emu.h"
#include <stdio.h>

void BR_ASM_CALL TriangleRender_ZPTI_I8_D16_32(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTI_I8_D16_32_FLAT(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPT_I8_D16_32(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTI_I8_D16_64(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // no-op
}
void BR_ASM_CALL TriangleRender_ZPTI_I8_D16_64_FLAT(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}

void BR_ASM_CALL TriangleRender_ZT_I8_D16_POW2(brp_block *block, int pow2, int skip_setup, va_list va);

void print_brp_vertex(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2) {
    printf("v0->flags = %d;\n", v0->flags);
    printf("v1->flags = %d;\n", v1->flags);
    printf("v2->flags = %d;\n", v2->flags);
    for (int i = 0; i < 16; i++) {
        printf("v0->comp_f[%d] = %f;\n", i, v0->comp_f[i]);
        printf("v1->comp_f[%d] = %f;\n", i, v1->comp_f[i]);
        printf("v2->comp_f[%d] = %f;\n", i, v2->comp_f[i]);
    }
}

void BR_ASM_CALL TriangleRender_ZPT_I8_D16_64(brp_block *block, ...) {
    va_list     va;
    va_start(va, block);
    brp_vertex *v0;
    brp_vertex *v1;
    brp_vertex *v2;

	v0 = va_arg(va, brp_vertex *);
    v1 = va_arg(va, brp_vertex *);
    v2 = va_arg(va, brp_vertex *);
    va_end(va);

    print_brp_vertex(v0, v1, v2);

    TriangleSetup_ZPT(v0, v1, v2);

    // jc TriangleRasterise_ZT_I8_D16_64
    if (x86_state.cf) {
        va_list l;
        TriangleRender_ZT_I8_D16_POW2(block, 6, 1, l);
        //TriangleRasterise_ZT_I8_D16_64
    }
}

void BR_ASM_CALL TriangleRender_ZPTI_I8_D16_128(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTI_I8_D16_128_FLAT(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPT_I8_D16_128(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTI_I8_D16_256(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // no-op
}
void BR_ASM_CALL TriangleRender_ZPTI_I8_D16_256_FLAT(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPT_I8_D16_256(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // no-op
}
void BR_ASM_CALL TriangleRender_ZPTI_I8_D16_1024(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTI_I8_D16_1024_FLAT(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPT_I8_D16_1024(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIF_I8_D16_32(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIF_I8_D16_32_FLAT(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTF_I8_D16_32(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIF_I8_D16_64(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIF_I8_D16_64_FLAT(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTF_I8_D16_64(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIF_I8_D16_128(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIF_I8_D16_128_FLAT(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTF_I8_D16_128(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIF_I8_D16_256(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIF_I8_D16_256_FLAT(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTF_I8_D16_256(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIF_I8_D16_1024(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIF_I8_D16_1024_FLAT(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTF_I8_D16_1024(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIB_I8_D16_32(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIB_I8_D16_32_FLAT(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTB_I8_D16_32(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIB_I8_D16_64(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIB_I8_D16_64_FLAT(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTB_I8_D16_64(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIB_I8_D16_128(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIB_I8_D16_128_FLAT(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTB_I8_D16_128(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIB_I8_D16_256(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIB_I8_D16_256_FLAT(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTB_I8_D16_256(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIB_I8_D16_1024(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIB_I8_D16_1024_FLAT(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTB_I8_D16_1024(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIFB_I8_D16_32(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIFB_I8_D16_32_FLAT(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTFB_I8_D16_32(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIFB_I8_D16_64(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIFB_I8_D16_64_FLAT(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTFB_I8_D16_64(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIFB_I8_D16_128(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIFB_I8_D16_128_FLAT(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTFB_I8_D16_128(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIFB_I8_D16_256(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIFB_I8_D16_256_FLAT(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTFB_I8_D16_256(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIFB_I8_D16_1024(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIFB_I8_D16_1024_FLAT(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTFB_I8_D16_1024(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
