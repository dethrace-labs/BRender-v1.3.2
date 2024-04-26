#include "brender.h"
#include "priminfo.h"
#include "pfpsetup.h"

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

    TriangleSetup_ZPT(v0, v1, v2);
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
