#include <stdint.h>
#include "../softrend/ddi/priminfo.h"
#include "x86emu.h"
#include "fpsetup.h"
#include "work.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

long  fconv_d16_12[2] = {0x04238000000000000, 0x04238000000010000};
long  fconv_d16_m[2]  = {0x04238000000010000, 0x04238000000000000};
float fp_one          = 1.0f;
float fp_two          = 2.0f;

uint32_t fp_conv_d   = 0x59C00000;
uint32_t fp_conv_d8  = 0x55C00000;
uint32_t fp_conv_d8r = 0x5DC00000;
uint32_t fp_conv_d16 = 0x51C00000;
uint32_t fp_conv_d24 = 0x4DC00000;
uint32_t fp_conv_d32 = 0x49C00000;

uint16_t fp_single_cw   = 0x107f;
uint16_t fp_double_cw   = 0x127f;
uint16_t fp_extended_cw = 0x137f;

int      sort_table_1[] = {1, 2, 0, 0, 0, 0, 2, 1};
int      sort_table_0[] = {0, 0, 0, 2, 1, 0, 1, 2};
int      sort_table_2[] = {2, 1, 0, 1, 2, 0, 0, 0};
uint32_t flip_table[8]  = {0x000000000, 0x080000000, 0x080000000, 0x000000000,
                           0x080000000, 0x000000000, 0x000000000, 0x080000000};

#define MASK_MANTISSA   0x007fffff
#define IMPLICIT_ONE    1 << 23
#define EXPONENT_OFFSET ((127 + 23) << 23) | 0x07fffff

float                            temp;
struct workspace_t               workspace;
struct ArbitraryWidthWorkspace_t workspaceA;

void TriangleSetup_ZT(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2)
{
    if(SETUP_FLOAT(v0, v1, v2) != FPSETUP_SUCCESS) {
        return;
    }
    SETUP_FLOAT_PARAM(C_SZ,"_z",&workspace.s_z,&workspace.d_z_x,fp_conv_d16,1);
    SETUP_FLOAT_PARAM(C_U,"_u",&workspace.s_u,&workspace.d_u_x,fp_conv_d16, 0);
    SETUP_FLOAT_PARAM(C_V,"_v",&workspace.s_v,&workspace.d_v_x,fp_conv_d16, 0);
}

void TriangleSetup_ZT_ARBITRARY(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2)
{
    if(SETUP_FLOAT(v0, v1, v2) != FPSETUP_SUCCESS) {
        return;
    }

    // assert(workspace.xm == 0x1011A80u);
    // assert(workspace.d_xm == 0u);
    // assert(workspace.x1 == 0x1010000u);
    // assert(workspace.d_x1 == 0x10000u);
    // assert(workspace.x2 == 0x173907Cu);
    // assert(workspace.d_x2 == 0xFF7F3500u);

    // assert(workspace.iarea == 0xB87CDB7Cu);
    // assert(workspace.dx1_a == 0x3BFE6C80u);
    // assert(workspace.dx2_a == 0x3BFE6C80u);
    // assert(workspace.dy1_a == 0x3BFE6C80u);
    // assert(workspace.dy2_a == 0x80000000u);
    // assert(workspace.xstep_1 == 0x3F800000u);
    // assert(workspace.xstep_0 == 0u);
    // assert(workspace.t_dx == 0x3F658000u);
    // assert(workspace.t_dy == 0x3F658000u);
    // assert(workspace.t_y == 0xB1u);

    SETUP_FLOAT_PARAM(C_SZ, "_z", &workspace.s_z, &workspace.d_z_x, fp_conv_d16, 1);

    // assert(workspace.s_z == 0xDD3B5200u);
    // assert(workspace.d_z_x == 0u);

    SETUP_FLOAT_PARAM(C_U, "_u", &workspace.s_u, &workspace.d_u_x, fp_conv_d24, 0);
    SETUP_FLOAT_PARAM(C_V, "_v", &workspace.s_v, &workspace.d_v_x, fp_conv_d24, 0);

    // assert(workspace.s_z == 0xDD3B5200u);

    // assert(workspace.s_z == 3711652352u);
    // assert(workspace.d_z_y_1 == 0u);
    // assert(workspace.d_z_x == 0u);
    // assert(workspace.d_z_y_0 == 0u);
    // assert(workspace.s_i == 0u);
    // assert(workspace.d_i_y_1 == 0u);
    // assert(workspace.d_i_x == 0u);
    // assert(workspace.d_i_y_0 == 0u);
    // assert(workspace.s_u == 116781u);
    // assert(workspace.d_u_y_1 == 130265u);
    // assert(workspace.d_u_x == 130265u);
    // assert(workspace.d_u_y_0 == 0u);
    // assert(workspace.s_v == 16660435u);
    // assert(workspace.d_v_y_1 == 4294837031u);
    // assert(workspace.d_v_x == 0u);
    // assert(workspace.d_v_y_0 == 4294837031u);
    // assert(workspace.s_s == 0u);
    // assert(workspace.d_s_y_1 == 0u);
    // assert(workspace.d_s_x == 0u);
    // assert(workspace.d_s_y_0 == 0u);

    ARBITRARY_SETUP();

    // assert(workspace.xm == 16849536u);
    // assert(workspace.d_xm == 0u);
    // assert(workspace.x1 == 16842752u);
    // assert(workspace.d_x1 == 65536u);
    // assert(workspace.x2 == 24350844u);
    // assert(workspace.d_x2 == 4286526720u);

    // assert(workspace.d_u_x == 130265u);
    // assert(workspace.d_u_y_1 == 130265u);

    // assert(workspace.d_u_y_0 == 0u);
    // assert(workspace.s_v == 16660435u);
    // assert(workspace.d_v_y_1 == 4294837031u);
    // assert(workspace.d_v_x == 0u);
    // assert(workspace.d_v_y_0 == 4294837031u);
    // assert(workspace.s_s == 0u);
    // assert(workspace.d_s_y_1 == 0u);
    // assert(workspace.d_s_x == 0u);
    // assert(workspace.d_s_y_0 == 0u);

    // assert(workspace.s_z == 3711652352u);
    // assert(workspace.d_z_y_1 == 0u);
    // assert(workspace.d_z_x == 0u);
    // assert(workspace.d_z_y_0 == 0u);
    // assert(workspace.s_i == 0u);
    // assert(workspace.d_i_y_1 == 0u);
    // assert(workspace.d_i_x == 0u);
    // assert(workspace.d_i_y_0 == 0u);
    // assert(workspace.s_u == 116781u);

    // assert(workspace.scanAddress == 0);
    // assert(workspace.scanAddressTrashed == 0u);
    // assert(workspace.depthAddress == 0);
    // assert(workspace.depthAddressTrashed == 0u);
    // assert(workspace.iarea == 3095190396u);
    // assert(workspace.dx1_a == 1006529664u);
    // assert(workspace.dx2_a == 1006529664u);
    // assert(workspace.dy1_a == 1006529664u);
    // assert(workspace.dy2_a == 2147483648u);
    // assert(workspace.xstep_1 == 1065353216);
    // assert(workspace.xstep_0 == 0);
    // assert(workspace.t_dx == 1063616512u);
    // assert(workspace.t_dy == 1063616512u);
    // assert(workspace.t_y == 177u);

    // assert(workspace.scratch2 == 0u);
    // assert(workspace.scratch3 == 0x42380000u);
    // assert(workspace.scratch4 == 0x10000u);
    // assert(workspace.scratch5 == 0x42380000u);
    // assert(workspace.scratch6 == 0u);
    // assert(workspace.scratch7 == 0x42380000u);
    // assert(workspace.scratch8 == 0x10000u);
    // assert(workspace.scratch9 == 0x42380000u);
    // assert(workspace.scratch10 == 0u);
    // assert(workspace.scratch11 == 0x42380000u);

    // TILTED
    // assert(workspace.xm == 0xFBBDA7u);
    // assert(workspace.d_xm == 0x1664u);
    // assert(workspace.x1 == 0xFAC913u);
    // assert(workspace.d_x1 == 0x13113u);
    // assert(workspace.x2 == 0x17B4AB5u);
    // assert(workspace.d_x2 == 0xFFF49131u);
    // assert(workspace.s_z == 0xDD3B5200u);
    // assert(workspace.d_z_y_1 == 0u);
    // assert(workspace.d_z_x == 0u);
    // assert(workspace.d_z_y_0 == 0u);
    // assert(workspace.s_i == 0u);
    // assert(workspace.d_i_y_1 == 0u);
    // assert(workspace.d_i_x == 0u);
    // assert(workspace.d_i_y_0 == 0u);
    // assert(workspace.s_u == 0x8361u);
    // assert(workspace.d_u_y_1 == 0x1CE93u);
    // assert(workspace.d_u_x == 0x1FAEAu);
    // assert(workspace.d_u_y_0 == 0xFFFFD3A9u);
    // assert(workspace.s_v == 0xFFDFDFu);
    // assert(workspace.d_v_y_1 == 0xFFFDD8C0u);
    // assert(workspace.d_v_x == 0xFFFFD3A9u);
    // assert(workspace.d_v_y_0 == 0xFFFE0516u);
    // assert(workspace.s_s == 0u);
    // assert(workspace.d_s_y_1 == 0u);
    // assert(workspace.d_s_x == 0u);
    // assert(workspace.d_s_y_0 == 0u);
    // assert(workspace.iarea == 0xB87CDB77u);
    // assert(workspace.dx1_a == 0x3C09D006u);
    // assert(workspace.dx2_a == 0x3BFD74C3u);
    // assert(workspace.dy1_a == 0x3BE74979u);
    // assert(workspace.dy2_a == 0xBA315A54u);
    // assert(workspace.xstep_1 == 0x3F800000u);
    // assert(workspace.xstep_0 == 0u);
    // assert(workspace.t_dx == 0x3E868200u);
    // assert(workspace.t_dy == 0x3D258000u);
    // assert(workspace.t_y == 0xB6u);
    // assert(workspace.flip == 0u);
    // assert(workspace.c_z == 0u);
    // assert(workspace.c_u == 0u);
    // assert(workspace.c_v == 0u);
    // assert(workspace.c_i == 0u);
    // assert(workspace.v0_x == 0u);
    // assert(workspace.v1_x == 0u);
    // assert(workspace.v2_x == 0u);
    // assert(workspace.v0_y == 0u);
    // assert(workspace.v1_y == 0u);
    // assert(workspace.v2_y == 0u);
    // assert(workspace.top_vertex == 0u);
    // assert(workspace.xm_f == 0u);
    // assert(workspace.d_xm_f == 0u);
    // assert(workspace.topCount == 0x75u);
    // assert(workspace.bottomCount == 0xAu);
    // assert(workspace.scratch0 == 0u);
    // assert(workspace.scratch1 == 0x42380000u);
    // assert(workspace.scratch2 == 0u);
    // assert(workspace.scratch3 == 0x42380000u);
    // assert(workspace.scratch4 == 0x10000u);
    // assert(workspace.scratch5 == 0x42380000u);
    // assert(workspace.scratch6 == 0u);
    // assert(workspace.scratch7 == 0x42380000u);
    // assert(workspace.scratch8 == 0x10000u);
    // assert(workspace.scratch9 == 0x42380000u);

    // assert(workspaceA.su == 8408u);
    // assert(workspaceA.pad0 == 1136132096u);
    // assert(workspaceA.dux == 32442u);
    // assert(workspaceA.pad1 == 1136132096u);
    // // assert(workspaceA.duy1 == 29604u);  //off by 1
    // assert(workspaceA.pad2 == 1136132096u);
    // assert(workspaceA.duy0 == 4294964458u);
    // assert(workspaceA.pad3 == 1136132095u);
    // assert(workspaceA.sv == 4032u);
    // assert(workspaceA.pad4 == 1127743488u);
    // // assert(workspaceA.dvy1 == 4294967232u);
    // // assert(workspaceA.pad5 == 1127743487u);
    // // assert(workspaceA.dvy1c == 0u);
    // // assert(workspaceA.pad6 == 0u);
    // assert(workspaceA.dvy0 == 4294967232u);
    // assert(workspaceA.pad7 == 1127743487u);
    // assert(workspaceA.dvy0c == 0u);
    // assert(workspaceA.dvxc == 0u);
    // assert(workspaceA.pad8 == 0u);
    // assert(workspaceA.dvx == 4294967232u);
    // assert(workspaceA.pad9 == 1127743487u);
    // assert(workspaceA.svf == 4160159744u);
    // assert(workspaceA.dvxf == 4108976128u);
    // assert(workspaceA.dvy1f == 1982857216u);
    // assert(workspaceA.dvy0f == 2168782848u);
    // assert(workspaceA.uUpperBound == 0x400000);
    // // vUpperBound = 0x7AD4FB8);

    // workspaceA.su = 0x180Du, workspaceA.pad0 = 0x43B80000u, workspaceA.dux = 0x796Fu, workspaceA.pad1 = 0x43B80000u,
    // workspaceA.duy1 = 0xBB7u, workspaceA.pad2 = 0x43B80000u, workspaceA.duy0 = 0xFFFF9248u,
    // workspaceA.pad3 = 0x43B7FFFFu, workspaceA.sv = 0xFC0u, workspaceA.pad4 = 0x43380000u, workspaceA.dvy1 =
    // 0xFFFFFFC0u, workspaceA.pad5 = 0x4337FFFFu, workspaceA.dvy1c = 0u, workspaceA.pad6 = 0u, workspaceA.dvy0 =
    // 0xFFFFFFC0u, workspaceA.pad7 = 0x4337FFFFu, workspaceA.dvy0c = 0u, workspaceA.dvxc = 0x40u, workspaceA.pad8 = 0u,
    // workspaceA.dvx = 0u, workspaceA.pad9 = 0x43380000u, workspaceA.svf = 0xE12D0000u, workspaceA.dvxf = 0u,
    // workspaceA.dvy1f = 0x4BF60000u, workspaceA.dvy0f = 0x4BF60000u, workspaceA.uUpperBound = 0x400000;
}

int SETUP_FLOAT(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2)
{
    x86emu_init();

    eax->ptr_val = v0;
    ecx->ptr_val = v1;
    edx->ptr_val = v2;
    // local count_cont,exit,top_zero,bottom_zero,empty_triangle

    // assume eax: ptr brp_vertex, /*ebx: ptr brp_vertex,*/ ecx: ptr brp_vertex, edx: ptr brp_vertex

    //; Calculate area of triangle and generate dx1/2area, dx1/2area, dx1/2area and dx1/2area
    //;
    //; Also sort the vertices in Y order whilst divide is happening
    //;
    //;	0		1		2		3		4		5		6		7

    // fld			[edx].comp_f[C_SX*4]		;	x2
    fld(x87_op_f(((brp_vertex *)edx->ptr_val)->comp_f[C_SX])); //	x2
    // fsub		[eax].comp_f[C_SX*4]		;	dx2
    fsub(((brp_vertex *)eax->ptr_val)->comp_f[C_SX]); //	dx2
    // fld			[ecx].comp_f[C_SX*4]		;	x1		dx2
    fld(x87_op_f(((brp_vertex *)ecx->ptr_val)->comp_f[C_SX])); //	x1		dx2
    // fsub		[eax].comp_f[C_SX*4]		;	dx1		dx2
    fsub(((brp_vertex *)eax->ptr_val)->comp_f[C_SX]); //	dx1		dx2
    // fld			[edx].comp_f[C_SY*4]		;	y2		dx1		dx2
    fld(x87_op_f(((brp_vertex *)edx->ptr_val)->comp_f[C_SY])); //	y2		dx1		dx2
    // fsub		[eax].comp_f[C_SY*4]		;	dy2		dx1		dx2
    fsub(((brp_vertex *)eax->ptr_val)->comp_f[C_SY]);          //	dy2		dx1		dx2
    fld(x87_op_f(((brp_vertex *)ecx->ptr_val)->comp_f[C_SY])); //	y1		dy2		dx1		dx2
    fsub(((brp_vertex *)eax->ptr_val)->comp_f[C_SY]);          //	dy1		dy2		dx1		dx2

    fld(x87_op_i(2)); //	dx1		dy1		dy2		dx1		dx2

    fmul_2(x87_op_i(0), x87_op_i(2)); //	dx1*dy2	dy1		dy2		dx1		dx2

    fld(x87_op_i(4));                 //	dx2		dx1*dy2	dy1		dy2		dx1		dx2
    fmul_2(x87_op_i(0), x87_op_i(2)); //	dx2*dy1	dx1*dy2	dy1		dy2		dx1		dx2

    mov(x86_op_reg(eax), x86_op_mem32(&((brp_vertex *)eax->ptr_val)->comp_f[C_SY]));

    mov(x86_op_reg(ecx), x86_op_mem32(&((brp_vertex *)ecx->ptr_val)->comp_f[C_SY]));

    fsubp_2(x87_op_i(1), x87_op_i(0)); //	2area	dy1		dy2		dx1		dx2

    xor_(x86_op_reg(ebx), x86_op_reg(ebx));
    cmp(x86_op_reg(ecx), x86_op_reg(eax));

    rcl(x86_op_reg(ebx), 1);
    mov(x86_op_reg(edx), x86_op_mem32(&((brp_vertex *)edx->ptr_val)->comp_f[C_SY]));

    fdivr(fp_one); //	1/2area	dy1		dy2		dx1		dx2

    cmp(x86_op_reg(edx), x86_op_reg(eax));

    rcl(x86_op_reg(ebx), 1);
    cmp(x86_op_reg(edx), x86_op_reg(ecx));

    rcl(x86_op_reg(ebx), 1); // ebx now has 3 bit number characterising the order of the vertices.

    mov(x86_op_reg(eax), x86_op_mem32(&sort_table_0[ebx->uint_val]));
    mov(x86_op_reg(edx), x86_op_mem32(&sort_table_2[ebx->uint_val]));

    mov(x86_op_reg(esi), x86_op_mem32(&flip_table[ebx->uint_val]));
    mov(x86_op_reg(ebx), x86_op_mem32(&sort_table_1[ebx->uint_val]));

    // Load eax,ebx,edx with pointers to the three vertices in vertical order
    mov(x86_op_reg(eax), x86_op_ptr(&workspace.v0_array[eax->uint_val]));
    mov(x86_op_reg(edx), x86_op_ptr(&workspace.v0_array[edx->uint_val]));
    mov(x86_op_reg(ebx), x86_op_ptr(&workspace.v0_array[ebx->uint_val]));
    mov(x86_op_mem32(&workspace.flip), x86_op_reg(esi));

    // Work out Y extents of triangle
    // ; Convert float to int using integer instructions, because FPU is in use doing division
    mov(x86_op_reg(ebp), x86_op_mem32(&((brp_vertex *)eax->ptr_val)->comp_f[C_SY]));
    mov(x86_op_reg(ecx), x86_op_imm(EXPONENT_OFFSET));

    sub(x86_op_reg(ecx), x86_op_reg(ebp));           // Offset exponent to get shift value
    and(x86_op_reg(ebp), x86_op_imm(MASK_MANTISSA)); // Mask out mantissa

    shr(x86_op_reg(ecx), 23);                       //				; Move shift value to low bits
    or (x86_op_reg(ebp), x86_op_imm(IMPLICIT_ONE)); //	; Put the 1 back in top of mantissa
    // shr		 ebp,cl				; EBP = y_t
    shr(x86_op_reg(ebp), ecx->uint_val & 0xff);

    mov(x86_op_reg(esi), x86_op_mem32(&((brp_vertex *)ebx->ptr_val)->comp_f[C_SY]));
    mov(x86_op_reg(ecx), x86_op_imm(EXPONENT_OFFSET));

    sub(x86_op_reg(ecx), x86_op_reg(esi));           // Offset exponent to get shift value
    and(x86_op_reg(esi), x86_op_imm(MASK_MANTISSA)); // Mask out mantissa

    shr(x86_op_reg(ecx), 23);                       //				; Move shift value to low bits
    or (x86_op_reg(esi), x86_op_imm(IMPLICIT_ONE)); //	; Put the 1 back in top of mantissa
    // shr		 ebp,cl				; ESI = y_m
    shr(x86_op_reg(esi), ecx->uint_val & 0xff);

    mov(x86_op_reg(edi), x86_op_mem32(&((brp_vertex *)edx->ptr_val)->comp_f[C_SY]));
    mov(x86_op_reg(ecx), x86_op_imm(EXPONENT_OFFSET));

    sub(x86_op_reg(ecx), x86_op_reg(edi));           // Offset exponent to get shift value
    and(x86_op_reg(edi), x86_op_imm(MASK_MANTISSA)); // Mask out mantissa

    shr(x86_op_reg(ecx), 23);                       //				; Move shift value to low bits
    or (x86_op_reg(edi), x86_op_imm(IMPLICIT_ONE)); //	; Put the 1 back in top of mantissa
    // shr		 edi,cl				; edi = y_b
    shr(x86_op_reg(edi), ecx->uint_val & 0xff);

    // Catch special cases of empty top or bottom trapezoids

    // cmp(ebp, esi);
    // je(top_zero);
    if(ebp->uint_val == esi->uint_val) {
        goto top_zero;
    }

    // cmp(esi, edi);
    // je(bottom_zero);
    if(esi->uint_val == edi->uint_val) {
        goto bottom_zero;
    }

    //; Parameter gradient startup and Y deltas for edge gradients

    //	0		1		2		3		4		5		6		7
    fmul_2(x87_op_i(1), x87_op_i(0));                          //	1/2area	dy1*a	dy2		dx1		dx2
    fld(x87_op_f(((brp_vertex *)ebx->ptr_val)->comp_f[C_SY])); //	sy2		1/2area	dy1*a	dy2		dx1		dx2
    fsub((((brp_vertex *)eax->ptr_val)->comp_f[C_SY]));        //   dsy1	1/2area	dy1*a	dy2		dx1		dx2
    fxch(3);                                                   //   dy2  	1/2area	dy1*a	dsy1	dx1		dx2
    fmul_2(x87_op_i(0), x87_op_i(1));                          //	dy2*a  	1/2area	dy1*a	dsy1	dx1		dx2
    fld(x87_op_f(((brp_vertex *)edx->ptr_val)->comp_f[C_SY])); //   sy3	 dy2*a  	1/2area	dy1*a	dsy1	dx1 dx2
    fsub((((brp_vertex *)ebx->ptr_val)->comp_f[C_SY]));        //   dsy2	dy2*a  	1/2area	dy1*a	dsy1	dx1		dx2
    fxch(5);                                                   //   dx1	  dy2*a 	1/2area	dy1*a	dsy1	dsy2	dx2

count_cont:
    fmul_2(x87_op_i(0), x87_op_i(2));                          //	dx1*a   dy2*a  	1/2area	dy1*a	dsy1	dsy2	dx2
    fld(x87_op_f(((brp_vertex *)edx->ptr_val)->comp_f[C_SY])); //   sy3	 dx1*a   dy2*a  	1/2area	dy1*a	dsy1 dsy2 dx2
    fsub(((brp_vertex *)eax->ptr_val)->comp_f[C_SY]);          //   dsy3	dx1*a   dy2*a  	1/2area	dy1*a	dsy1	dsy2	dx2
    fxch(7);                                                   //   dx2		dx1*a   dy2*a  	1/2area	dy1*a	dsy1	dsy2	dsy3
    fmul_2(x87_op_i(0), x87_op_i(3));                          //   dx2*a	dx1*a   dy2*a  	1/2area	dy1*a	dsy1	dsy2	dsy3
    fxch(3);                                                   //   1/2area	dx1*a   dy2*a  	dx2*a	dy1*a	dsy1	dsy2	dsy3

    fstp(x87_op_mem32(&workspace.iarea));
    fstp(x87_op_mem32(&workspace.dx1_a));
    fstp(x87_op_mem32(&workspace.dy2_a));
    fstp(x87_op_mem32(&workspace.dx2_a));
    fstp(x87_op_mem32(&workspace.dy1_a)); //  	dy1		dy2		dy3

    //; Find edge gradients of triangle
    //;
    //; R = 1/(dy1.dy2.dy3)
    //;
    //; gradient_major = dy1.dx2.dy3.R
    //; gradient_minor1 = dx1.dy2.dy3.R
    //; gradient_minor2 = dy1.dy2.dx3.R
    //;
    // 													;	0		1		2		3		4		5		6 7 fld st(2) ;	dy3		dy1		dy2		dy3
    fld(x87_op_i(2));
    // 			fmul		st,st(2)					;	dy2*dy3	dy1		dy2		dy3
    fmul_2(x87_op_i(0), x87_op_i(2));
    // 			fld			[ebx].comp_f[C_SX*4]		;	x2		dy2*dy3	dy1		dy2		dy3
    fld(x87_op_f(((brp_vertex *)ebx->ptr_val)->comp_f[C_SX]));
    // 			fsub		[eax].comp_f[C_SX*4]		;	dx1		dy2*dy3	dy1		dy2		dy3
    fsub((((brp_vertex *)eax->ptr_val)->comp_f[C_SX]));
    // 			fld			st(1)						;	dy2*dy3 dx1		dy2*dy3	dy1		dy2		dy3
    fld(x87_op_i(1));
    // 			fmul		st,st(3)					;	dy123	dx1		dy2*dy3	dy1		dy2		dy3
    fmul_2(x87_op_i(0), x87_op_i(3));

    // 			fld			[edx].comp_f[C_SX*4]		;	x3		dy123	dx1		dy2*dy3	dy1		dy2		dy3
    fld(x87_op_f(((brp_vertex *)edx->ptr_val)->comp_f[C_SX]));
    // 			fsub		[ebx].comp_f[C_SX*4]		;	dx2		dy123	dx1		dy2*dy3	dy1		dy2		dy3
    fsub((((brp_vertex *)ebx->ptr_val)->comp_f[C_SX]));
    // 			 fxch		 st(2)						;	dx1		dy123	dx2		dy2*dy3	dy1		dy2		dy3
    fxch(2);
    // 			fld			fp_one						;	1.0		dx1		dy123	dx2		dy2*dy3	dy1		dy2 dy3
    fld(x87_op_f(fp_one));
    // 			fdivrp		st(2),st					;	dx1		R		dx2		dy2*dy3	dy1		dy2		dy3
    fdivrp(2, 0);

    // ; Generate counts
    // ;
    // 		inc			ebp
    ebp->uint_val++;
    // 		mov			ecx,esi
    mov(x86_op_reg(ecx), x86_op_reg(esi));
    // 		sub			ecx,ebp				;  count_t = (y_m-y_t)-1
    sub(x86_op_reg(ecx), x86_op_reg(ebp));
    // 		mov			[workspace.t_y],ebp			; save for X intercept calcs
    mov(x86_op_mem32(&workspace.t_y), x86_op_reg(ebp));
    // 		mov			[workspace.topCount],ecx
    mov(x86_op_mem32(&workspace.topCount), x86_op_reg(ecx));
    // 		inc			esi
    esi->uint_val++;
    // 		sub			edi,esi				;  count_b = (y_b-y_m)-1
    sub(x86_op_reg(edi), x86_op_reg(esi));
    // 		mov			m_y,esi				; save for X intercept calcs
    mov(x86_op_mem32(&m_y), x86_op_reg(esi));
    // 		mov			[workspace].bottomCount,edi
    mov(x86_op_mem32(&workspace.bottomCount), x86_op_reg(edi));
    // 		mov			esi,[workspace.flip]
    mov(x86_op_reg(esi), x86_op_mem32(&workspace.flip));

    //     	; Generate LR/RL flag into esi (used to index convertion numbers below)
    // 	;
    // 			mov			edi,workspace.iarea
    mov(x86_op_reg(edi), x86_op_mem32(&workspace.iarea));
    // ;V
    // 			xor			esi,edi			; Build LR flag in bit 31
    xor_(x86_op_reg(esi), x86_op_reg(edi));
    // ;V
    // 			shr			esi,31			; move down to bit 0
    shr(x86_op_reg(esi), 31);
    // ;V
    // 			mov			[workspace.flip],esi
    mov(x86_op_mem32(&workspace.flip), x86_op_reg(esi));

    // ;XXX Setup screen pointers and strides
    // ;

    // ;XXX Work out which scan convertion function to use
    // ;

    // 	; Finish of gradient calculations, interleaved with working out t_dy, and m_dy, the fractions
    // 	; that the top and middle vertices are from the integer scanline boundaries
    // 	;
    // 	; t_dy = (yt+1) - vt->y
    // 	; m_dy = (ym+1) - vm->y
    // 	;
    // 												;	0		1		2		3		4		5		6		7
    // 		fmulp		st(3),st					;	R		dx2		XYY		dy1		dy2		dy3
    fmulp_2(x87_op_i(3), x87_op_i(0));
    // 		fld			[edx].comp_f[C_SX*4]		;	x3		R		dx2		XYY		dy1		dy2		dy3
    fld(x87_op_f(((brp_vertex *)edx->ptr_val)->comp_f[C_SX]));
    // 		 fxch		st(3)						;	XYY		R		dx2		x3		dy1		dy2		dy3
    fxch(3);
    // 		fmul		st,st(1)					;	XYY*R	R		dx2		x3		dy1		dy2		dy3
    fmul_2(x87_op_i(0), x87_op_i(1));
    // 		 fxch		st(3)						;	x3		R		dx2		XYY*R	dy1		dy2		dy3
    fxch(3);
    // 		fsub		[eax].comp_f[C_SX*4]		;	dx3		R		dx2		XYY*R	dy1		dy2		dy3
    fsub((((brp_vertex *)eax->ptr_val)->comp_f[C_SX]));
    // 		 fxch		st(1)						;	R		dx3		dx2		XYY*R	dy1		dy2		dy3
    fxch(1);
    // 		fmulp		st(4),st					;	dx3		dx2		XYY*R	dy1*R	dy2		dy3
    fmulp_2(x87_op_i(4), x87_op_i(0));
    // 		 fxch		st(2)						;	XYY*R	dx2		dx3		dy1*R	dy2		dy3
    fxch(2);
    // 		fild		m_y				            ;	m_y		XYY*R	dx2		dx3		dy1*R	dy2		dy3
    fild(m_y);
    // 		 fxch		st(2)						;	dx2		XYY*R	m_y		dx3		dy1*R	dy2		dy3
    fxch(2);
    // 		fmulp		st(6),st		            ;	XYY*R	m_y		dx3		dy1*R	dy2		dx2*dy3
    fmulp_2(x87_op_i(6), x87_op_i(0));
    // 		fild		[workspace.t_y]				;	t_y		XYY*R	m_y		dx3		dy1*R	dy2 dx2*dy3
    fild(workspace.t_y);
    // fxch		st(3)			            ;	dx3		XYY*R	m_y		t_y		dy1*R	dy2		dx2*dy3
    fxch(3);
    // 		fmulp		st(5),st					;	XYY*R	m_y		t_y		dy1*R	dy2*dx3	dx2*dy3
    fmulp_2(x87_op_i(5), x87_op_i(0));
    // 		 fxch		st(1)			            ;	m_y		XYY*R	t_y		dy1*R	dy2*dx3	dx2*dy3
    fxch(1);
    // 		fsub		[ebx].comp_f[C_SY*4]		;	m_dy	XYY*R	t_y		dy1*R	dy2*dx3	dx2*dy3
    fsub((((brp_vertex *)ebx->ptr_val)->comp_f[C_SY]));
    // 		 fxch		st(3)						;	dy1*R	XYY*R	t_y		m_dy	dy2*dx3	dx2*dy3
    fxch(3);
    // 		fmul		st(4),st		            ;	dy1*R	XYY*R	t_y		m_dy	YYX*R	dx2*dy3
    fmul_2(x87_op_i(4), x87_op_i(0));
    // 		 fxch		st(2)						;	t_y		XYY*R	dy1*R	m_dy	YYX*R	dx2*dy3
    fxch(2);
    // 		fsub		[eax].comp_f[C_SY*4]		;	t_dy	XYY*R	dy1*R	m_dy	YYX*R	dx2*dy3
    fsub((((brp_vertex *)eax->ptr_val)->comp_f[C_SY]));
    // 		 fxch		st(2)						;	dy1*R	XYY*R	t_dy	m_dy	YYX*R	dx2*dy3
    fxch(2);
    // 		fmulp		st(5),st		            ;	XYY*R	t_dy	m_dy	YYX*R	YXY*R
    fmulp_2(x87_op_i(5), x87_op_i(0));
    // 		 fxch		st(2)						;	m_dy	t_dy	XYY*R	YYX*R	YXY*R
    fxch(2);
    // 												;	m_dy	t_dy	g1		gm		g2

    // ; Work out initial X intercepts with top and middle scanlines
    // ;
    // ; x_major  = gm * t_dy + vt->x
    // ; x_minor1 = g1 * t_dy + vt->x
    // ; x_minor2 = g2 * m_dy + vm->x
    // ;
    // 												;	0		1		2		3		4		5		6		7
    // 		fld			st(1)						;	t_dy	m_dy	t_dy	g1		gm		g2
    fld(x87_op_i(1));
    // 		fxch		st(1)			            ;	m_dy	t_dy	t_dy	g1		gm		g2
    fxch(1);
    // 		fmul		st,st(5)		            ;	m_dy*g2	t_dy	t_dy	g1		gm		g2
    fmul_2(x87_op_i(0), x87_op_i(5));
    // 		fxch		st(2)						;	t_dy	t_dy	m_dy*g2	g1		gm		g2
    fxch(2);
    // 		fst			[workspace.t_dy]
    fst(x87_op_mem32(&workspace.t_dy));
    // 		fmul		st,st(3)					;	t_dy*g1	t_dy	m_dy*g2	g1		gm		g2
    fmul_2(x87_op_i(0), x87_op_i(3));
    // 		 fxch		st(2)			            ;	m_dy*g2	t_dy	t_dy*g1	g1		gm		g2
    fxch(2);
    // 		fadd		[ebx].comp_f[C_SX*4]		;	x_2		t_dy	t_dy*g1	g1		gm		g2
    fadd(x87_op_f(((brp_vertex *)ebx->ptr_val)->comp_f[C_SX]));
    // 		 fxch		st(1)						;	t_dy	x_2		t_dy*g1	g1		gm		g2
    fxch(1);
    // 		fmul		st,st(4)		            ;	t_dy*gm	x_2		t_dy*g1	g1		gm		g2
    fmul_2(x87_op_i(0), x87_op_i(4));
    // 		 fxch		st(2)						;	t_dy*g1	x_2		t_dy*gm	g1		gm		g2
    fxch(2);
    // 		fadd		[eax].comp_f[C_SX*4]		;	x_1		x_2		t_dy*gm	g1		gm		g2
    fadd(x87_op_f(((brp_vertex *)eax->ptr_val)->comp_f[C_SX]));
    // 		 fxch		st(3)						;	g1		x_2		t_dy*gm	x_1		gm		g2
    fxch(3);
    // 		fadd		fp_conv_d16		            ;	g1+C	x_2		t_dy*gm	x_1		gm		g2
    fadd(x87_op_mem32(&fp_conv_d16));
    // 		 fxch		st(2)						;	t_dy*gm	x_2		g1+C	x_1		gm		g2
    fxch(2);
    // 		fadd		[eax].comp_f[C_SX*4]		;	x_m		x_2		g1+C	x_1		gm		g2
    fadd(x87_op_f(((brp_vertex *)eax->ptr_val)->comp_f[C_SX]));
    // 		 fxch		st(4)						;	gm		x_2		g1+C	x_1		x_m		g2
    fxch(4);
    // 		fadd		fp_conv_d16		            ;	gm+C	x_2		g1+C	x_1		x_m		g2
    fadd(x87_op_mem32(&fp_conv_d16));
    // 		 fxch		st(1)						;	x_2		gm+C	g1+C	x_1		x_m		g2
    fxch(1);
    // 		fadd	fconv_d16_12[esi*8]	            ;	x_2+C	gm+C	g1+C	x_1		x_m		g2
    assert(esi->uint_val >= 0 && esi->uint_val <= 1);
    fadd(x87_op_mem64(&fconv_d16_12[esi->uint_val]));
    // 		 fxch		st(5)						;	g2		gm+C	g1+C	x_1		x_m		x_2+C
    fxch(5);
    // 		fadd		fp_conv_d16		              ;	g2+C	gm+C	g1+C	x_1		x_m		x_2+C
    fadd(x87_op_mem32(&fp_conv_d16));
    // 		 fxch		st(2)						;	g1+C	gm+C	g2+C	x_1		x_m		x_2+C
    fxch(2);
    // 		fstp real8 ptr [workspace].x1			;	gm+C	g2+C	x_1		x_m		x_2+C
    fstp(x87_op_mem64(&workspace.x1));
    // 		fstp real8 ptr [workspace].xm			;	g2+C	x_1		x_m		x_2+C
    fstp(x87_op_mem64(&workspace.xm));
    // 		fstp real8 ptr [workspace].x2			;	x_1		x_m		x_2+C
    fstp(x87_op_mem64(&workspace.x2));
    // 		fadd	fconv_d16_12[esi*8]				;	x_1+C	x_m		x_2+C
    fadd(x87_op_mem64(&fconv_d16_12[esi->uint_val]));
    // 		fxch		st(1)						;	x_m		x_1+C	x_2+C
    fxch(1);
    // 		fadd	fconv_d16_m[esi*8]				;	x_m+C	x_1+C	x_2+C
    fadd(x87_op_mem64(&fconv_d16_m[esi->uint_val]));

    // 	; Load deltas back in registers
    // 	;
    // 		mov			edx,[workspace].xm	; read fixed d_xm
    mov(x86_op_reg(edx), x86_op_mem32(&workspace.xm));
    // 		mov			esi,[workspace].x1	; read fixed d_x1
    mov(x86_op_reg(esi), x86_op_mem32(&workspace.x1));
    // 		mov			edi,[workspace].x2	; read fixed d_x2
    mov(x86_op_reg(edi), x86_op_mem32(&workspace.x2));
    // 		mov		ebx,[workspace.v0]				; Start preparing for parmeter setup
    mov(x86_op_reg(ebx), x86_op_ptr(&workspace.v0));
    // 		fstp real8 ptr [workspace].xm			;	x_1+C	x_2+C
    fstp(x87_op_mem64(&workspace.xm));
    // 		fstp real8 ptr [workspace].x1			;	x_2+C
    fstp(x87_op_mem64(&workspace.x1));

    // 		mov			ecx,[workspace].xm
    mov(x86_op_reg(ecx), x86_op_mem32(&workspace.xm));
    // 		mov			[workspace].xm+4,edx
    mov(x86_op_mem32(&workspace.d_xm), x86_op_reg(edx));
    // 		sar			ecx,16
    sar(x86_op_reg(ecx), 16);
    // 		mov			[workspace].x1+4,esi
    mov(x86_op_mem32(&workspace.d_x1), x86_op_reg(esi));
    // 		sar			edx,16			; get integer part of x delta down major edge
    sar(x86_op_reg(edx), 16);
    // 		mov			[workspace.t_dx],ecx
    mov(x86_op_mem32(&workspace.t_dx), x86_op_reg(ecx));
    // 		fild		[workspace.t_dx]			;	t_x		x_2+C
    fild(workspace.t_dx);
    // 	; Generate floating point versions of x delta and x delta+4
    // 	;
    // 		mov			[workspace.xstep_0],edx
    mov(x86_op_mem32(&workspace.xstep_0), x86_op_reg(edx));
    // 		inc edx
    edx->uint_val++;
    // 		mov			[workspace.xstep_1],edx
    mov(x86_op_mem32(&workspace.xstep_1), x86_op_reg(edx));
    // 		mov			edx,[workspace.v2]				; Start preparing for parmeter setup
    mov(x86_op_reg(edx), x86_op_ptr(&workspace.v2));
    // 												;	0		1		2		3		4		5		6		7
    // 		fsub		[eax].comp_f[C_SX*4]		;	t_dx	x_2+C
    fsub((((brp_vertex *)eax->ptr_val)->comp_f[C_SX]));
    // 		 fxch		st(1)						;	x_2+C	t_dx
    fxch(1);
    // 		fstp real8 ptr [workspace].x2			;	t_dx
    fstp(x87_op_mem64(&workspace.x2));

    // 		fild		[workspace.xstep_0]			;	xstep_0	t_dx
    fild(workspace.xstep_0);
    // 		fild		[workspace.xstep_1]			;	xstep_1 xstep_0	t_dx
    fild(workspace.xstep_1);
    // 		 fxch		st(2)			;	tdx		xstep_0	xstep_1
    fxch(2);
    // 		fstp		[workspace.t_dx]			;	xstep_0	xstep_1
    fstp(x87_op_mem32(&workspace.t_dx));
    // 		mov			[workspace].x2+4,edi
    mov(x86_op_mem32(&workspace.d_x2), x86_op_reg(edi));
    // 		mov		ecx,[workspace.v1]				; Start preparing for parmeter setup
    mov(x86_op_reg(ecx), x86_op_ptr(&workspace.v1));
    // 		fstp		[workspace.xstep_0]			;	step_1
    fstp(x87_op_mem32(&workspace.xstep_0));
    // 		fstp		[workspace.xstep_1]			;
    fstp(x87_op_mem32(&workspace.xstep_1));

    // 		jmp			exit
    goto exit;
    // ; Special cases for top or bottom counts == 0
    // ;

top_zero:
    // cmp			ebp,edi			; Check for completely empty triangle
    // je			empty_triangle
    if(ebp->uint_val == edi->uint_val) {
        goto empty_triangle;
    }
    // 										;	0		1		2		3		4		5		6		7
    fmul_2(x87_op_i(1), x87_op_i(0));                          //	1/2area	dy1*a	dy2		dx1		dx2
    fld(x87_op_f(fp_one));                                     //	1.0		1/2area	dy1*a	dy2		dx1		dx2
    fxch(3);                                                   //   dy2  	1/2area	dy1*a	1.0		dx1		dx2
    fmul_2(x87_op_i(0), x87_op_i(1));                          //	dy2*a  	1/2area	dy1*a	1.0		dx1		dx2
    fld(x87_op_f(((brp_vertex *)edx->ptr_val)->comp_f[C_SY])); //   sy3	 dy2*a  	1/2area	dy1*a	1.0		dx1 dx2
    // fsub		[ebx].comp_f[C_SY*4]		;   dsy2	dy2*a  	1/2area	dy1*a	1.0		dx1		dx2
    fsub(((brp_vertex *)ebx->ptr_val)->comp_f[C_SY]);
    //  fxch	   st(5)						;   dx1	  dy2*a 	1/2area	dy1*a	1.0		dsy2	dx2
    fxch(5);

    // jmp		count_cont
    goto count_cont;

bottom_zero:
    // 												;	0		1		2		3		4		5		6		7
    // 		fmul		st(1),st					;	1/2area	dy1*a	dy2		dx1		dx2
    fmul_2(x87_op_i(1), x87_op_i(0));
    // 		fld			[ebx].comp_f[C_SY*4]		;	sy2		1/2area	dy1*a	dy2		dx1		dx2
    fld(x87_op_f(((brp_vertex *)ebx->ptr_val)->comp_f[C_SY]));
    // 		fsub		[eax].comp_f[C_SY*4]		;   dsy1	1/2area	dy1*a	dy2		dx1		dx2
    fsub(((brp_vertex *)eax->ptr_val)->comp_f[C_SY]);
    // 		 fxch	   st(3)						;   dy2  	1/2area	dy1*a	dsy1	dx1		dx2
    fxch(3);
    // 		fmul		st,st(1)					;	dy2*a  	1/2area	dy1*a	dsy1	dx1		dx2
    fmul_2(x87_op_i(0), x87_op_i(1));
    // 		fld			fp_one						;   1.0	 dy2*a  	1/2area	dy1*a	dsy1	dx1		dx2
    fld(x87_op_f(fp_one));
    // 		 fxch	   st(5)						;   dx1	  dy2*a 	1/2area	dy1*a	dsy1	1.0		dx2
    fxch(5);
    // 		jmp		count_cont
    goto count_cont;

    // ; Fill in block with NULL count and exit
    // ;
empty_triangle:
    // mov workspace.topCount,-1
    workspace.topCount = -1;
    // mov workspace.bottomCount,-1
    workspace.bottomCount = -1;
    fstp(x87_op_i(0));
    fstp(x87_op_i(0));
    fstp(x87_op_i(0));
    fstp(x87_op_i(0));
    fstp(x87_op_i(0));
    return FPSETUP_EMPTY_TRIANGLE;

exit:
    assert(x86emu_fpu_stack_top() == -1);
    return FPSETUP_SUCCESS;
}

/*; Do all the per-triangle work for a single float parameter
;
;
eax : ptr to top   vertex;
ebx : ptr to       vertex0;
ecx : ptr to       vertex1;
edx : ptr to       vertex2;
ebp : ptr to param block;
;*/
void SETUP_FLOAT_PARAM(int comp, char *param /*unused*/, uint32_t *s_p, uint32_t *d_p_x, uint32_t conv, int is_unsigned)
{

    // 		assume eax: ptr brp_vertex, ebx: ptr brp_vertex, ecx: ptr brp_vertex, edx: ptr brp_vertex

    // 	; work out parameter deltas
    // 	; dp1 = param_1 - param_0
    // 	; dp2 = param_2 - param_0
    // 	;
    // 	; 4 cycles
    // 	;
    // 												;	0		1		2		3		4		5		6		7
    // 			fld		[edx].comp_f[comp*4]		;	p2
    fld(x87_op_f(((brp_vertex *)edx->ptr_val)->comp_f[comp]));
    // 			fsub	[ebx].comp_f[comp*4]		;	dp2
    fsub(((brp_vertex *)ebx->ptr_val)->comp_f[comp]);
    // 			fld		[ecx].comp_f[comp*4]		;	p1		dp2
    fld(x87_op_f(((brp_vertex *)ecx->ptr_val)->comp_f[comp]));
    // 			fsub	[ebx].comp_f[comp*4]		;	dp1		dp2
    fsub(((brp_vertex *)ebx->ptr_val)->comp_f[comp]);

    // 	; Multiply deltas by precomputed values to get x & y gradients
    // 	; (Also interleaved load of parameter start and fractional x & y offsets of start position)
    // 	;
    // 	; pdx = dp1 * dy2_a - dp2 * dy1_a
    // 	; pdy = dp2 * dx1_a - dp1 * dx2_a
    // 	;
    // 	; 11 cycles
    // 	;
    // 												;	0		1		2		3		4		5		6		7
    // 			fld		st(1)						;	dp2		dp1		dp2
    fld(x87_op_i(1));
    // 			fmul	[workspace.dy1_a]			;	dp2*a	dp1		dp2
    fmul(*(float *)&workspace.dy1_a);
    // 			fld		st(1)						;	dp1		dp2*a	dp1		dp2
    fld(x87_op_i(1));
    // 			fmul	[workspace.dy2_a]			;	dp1*b	dp2*a	dp1		dp2
    fmul(*(float *)&workspace.dy2_a);
    // 			fld		[workspace.t_dx]			;	fdx		dp1*b	dp2*a	dp1		dp2
    fld(x87_op_f(*(float *)&workspace.t_dx));
    // 			 fxch	st(4)						;	dp2		dp1*b	dp2*a	dp1		fdx
    fxch(4);
    // 			fmul	[workspace.dx1_a]			;	dp2*c	dp1*b	dp2*a	dp1		fdx
    fmul(*(float *)&workspace.dx1_a);
    // 			fld		[workspace.t_dy]			; 	fdy		dp2*c	dp1*b	dp2*a	dp1		fdx
    fld(x87_op_f(*(float *)&workspace.t_dy));
    // 			 fxch	st(4)						; 	dp1		dp2*c	dp1*b	dp2*a	fdy		fdx
    fxch(4);
    // 			fmul	[workspace.dx2_a]			; 	dp1*d	dp2*c	dp1*b	dp2*a	fdy		fdx
    fmul(*(float *)&workspace.dx2_a);
    // 			 fxch	st(3)						; 	dp2*a	dp2*c	dp1*b	dp1*d	fdy		fdx
    fxch(3);
    // 			fsubp	st(2),st					; 	dp2*c	d1b-d2a	dp1*d	fdy		fdx
    fsubp_2(x87_op_i(2), x87_op_i(0));
    // 			fld		[eax].comp_f[comp*4]		; 	param_t	dp2*c	d1b-d2a	dp1*d	fdy		fdx
    fld(x87_op_f(((brp_vertex *)eax->ptr_val)->comp_f[comp]));
    // 			 fxch	st(3)						; 	dp1*d	dp2*c	d1b-d2a	param_t	fdy		fdx
    fxch(3);
    // 			fsubp	st(1),st					; 	d2c-d1d	d1b-d2a	param_t	fdy		fdx
    fsubp_2(x87_op_i(1), x87_op_i(0));
    // 												; 	pdy		pdx		param_t	fdy		fdx

    // 	; Build the inputs to the rasteriser
    // 	;
    // 	; pdy_0 = pdy + xstep_0 * pdx
    // 	; pdy_1 = pdy + xstep_1 * pdx
    // 	; pstart = param_t + pdx * fdx + pdy * fdy
    // 	;
    // 	; (A couple of the fixed points convertions are interleaved into this block)
    // 	; 12 cycles
    // 	;
    // 												;	0		1		2		3		4		5		6		7
    // 			fld		st(1)						; 	pdx		pdy		pdx		param_t	fdy		fdx
    fld(x87_op_i(1));
    // 			fmul	[workspace.xstep_0]			; 	pdx*xs0	pdy		pdx		param_t	fdy		fdx
    fmul(*(float *)&workspace.xstep_0);
    // 			fld		st(2)						; 	pdx		pdx*xs0	pdy		pdx		param_t	fdy		fdx
    fld(x87_op_i(2));
    // 			fmul	[workspace.xstep_1]			; 	pdx*xs1	pdx*xs0	pdy		pdx		param_t	fdy		fdx
    fmul(*(float *)&workspace.xstep_1);
    // 			 fxch	st(1)						; 	pdx*xs0	pdx*xs1	pdy		pdx		param_t	fdy		fdx
    fxch(1);
    // 			fadd	st,st(2)					; 	pdy_0	pdx*xs1	pdy		pdx		param_t	fdy		fdx
    fadd_st(0, 2);
    // 			 fxch	st(3)						; 	pdx		pdx*xs1	pdy		pdy_0	param_t	fdy		fdx
    fxch(3);
    // 			fmul	st(6),st					; 	pdx		pdx*xs1	pdy		pdy_0	param_t	fdy		fdx*pdx
    fmul_2(x87_op_i(6), x87_op_i(0));
    // 			 fxch	st(2)						; 	pdy		pdx*xs1	pdx		pdy_0	param_t	fdy		fdx*pdx
    fxch(2);
    // 			fadd	st(1),st					; 	pdy		pdy_1	pdx		pdy_0	param_t	fdy		fdx*pdx
    fadd_st(1, 0);
    // 			fmulp	st(5),st					; 	pdy_1	pdx		pdy_0	param_t	fdy*pdy	fdx*pdx
    fmulp_2(x87_op_i(5), x87_op_i(0));
    // 			 fxch	st(3)						; 	param_t	pdx		pdy_0	pdy_1	fdy*pdy	fdx*pdx
    fxch(3);
    // 			faddp	st(5),st					; 	pdx		pdy_0	pdy_1	fdy*pdy	fpx+pt
    faddp(x87_op_i(5));
    // 			 fxch	st(1)						; 	pdy_0	pdx		pdy_1	fdy*pdy	fpx+pt
    fxch(1);
    // 			fadd	conv						; 	C+pdy_0	pdx		pdy_1	fdy*pdy	fpx+pt
    fadd(x87_op_mem32(&conv));
    // 			 fxch	st(2)						; 	pdy_1	pdx		C+pdy_0	fdy*pdy	fpx+pt
    fxch(2);
    // 			fadd	conv						; 	C+pdy_1	pdx		C+pdy_0	fdy*pdy	fpx+pt
    fadd(x87_op_mem32(&conv));
    // 			 fxch	st(3)						; 	fdy*pdy	pdx		C+pdy_0	C+pdy_1	fpx+pt
    fxch(3);
    // 			faddp	st(4),st					;	pdx		C+pdy_0	C+pdy_1	pstart
    faddp(x87_op_i(4));

    // 	; Convert to fixed point, pack and store in output block
    // 	;
    // 	; tsb->d_p_y0 = convert(pdy_0)
    // 	; tsb->d_p_y1 = convert(pdy_1)
    // 	; tsb->d_p_x = convert(pdx)
    // 	; tsb->s_p = convert(pstart)
    // 	;
    // 	; 13 cycles
    // 									;	0		1		2		3		4		5		6		7
    // 			fadd	conv			; 	C+pdx	C+pdy_0	C+pdy_1	pstart
    fadd(x87_op_mem32(&conv));
    // 			 fxch	st(3)			; 	pstart	C+pdy_0	C+pdy_1	C+pdx
    fxch(3);
    // ; 1 clock delay

    // 			fadd	conv			; 	C+pstrt	C+pdy_0	C+pdy_1	C+pdx
    fadd(x87_op_mem32(&conv));
    // 			 fxch	st(2)			; 	C+pdy_1	C+pdy_0	C+pstrt	C+pdx
    fxch(2);
    // 			fstp	real8 ptr s_p
    fstp(x87_op_mem64(s_p));
    // 			fstp	real8 ptr d_p_x
    fstp(x87_op_mem64(d_p_x));
    // 			mov		esi,dword ptr s_p
    mov(x86_op_reg(esi), x86_op_mem32(s_p));
    // 			mov		edi,dword ptr d_p_x
    mov(x86_op_reg(edi), x86_op_mem32(d_p_x));
    // 			fstp	real8 ptr s_p	;
    fstp(x87_op_mem64(s_p));
    // 			fstp	real8 ptr d_p_x	;
    fstp(x87_op_mem64(d_p_x));
    // 			mov		dword ptr s_p+4,esi
    mov(x86_op_mem32(s_p + 1), x86_op_reg(esi));
    // 			mov		dword ptr d_p_x+4,edi
    mov(x86_op_mem32(d_p_x + 1), x86_op_reg(edi));
    // if unsigned
    if(is_unsigned) {
        // 	; Remap from -1 to 1 signed to 0 to 1 unsigned
        // 	;
        // 			mov		esi,dword ptr s_p
        mov(x86_op_reg(esi), x86_op_mem32(s_p));
        // 			xor		esi,080000000h
        xor_(x86_op_reg(esi), x86_op_imm(0x080000000));
        // 			mov		dword ptr s_p,esi
        mov(x86_op_mem32(s_p), x86_op_reg(esi));
        // endif
    }
}

void ARBITRARY_SETUP()
{
    // SETUP_FPU
    SETUP_FLAGS();
    REMOVE_INTEGER_PARTS_OF_PARAMETERS();

    // MULTIPLY_UP_PARAM_VALUES(u, width_p, fp_conv_d8r);
    MULTIPLY_UP_PARAM_VALUES(workspace.s_u, workspace.d_u_x, workspace.d_u_y_0, workspace.d_u_y_1, &workspaceA.su,
                             &workspaceA.dux, &workspaceA.duy1, &workspaceA.duy0, work.texture.width_p, fp_conv_d8r);

    // MULTIPLY_UP_PARAM_VALUES(v, height, fp_conv_d8r);
    MULTIPLY_UP_PARAM_VALUES(workspace.s_v, workspace.d_v_x, workspace.d_v_y_0, workspace.d_v_y_1, &workspaceA.sv,
                             &workspaceA.dvx, &workspaceA.dvy1, &workspaceA.dvy0, work.texture.height, fp_conv_d8r);

    SPLIT_INTO_INTEGER_AND_FRACTIONAL_PARTS();
    MULTIPLY_UP_V_BY_STRIDE(fp_conv_d);
    CREATE_CARRY_VERSIONS();
    WRAP_SETUP();
    // RESTORE_FPU
}

// SETUP_FLAGS macro ; approx 21 cycles fixed, 45 cycles float
void SETUP_FLAGS()
{
    // 	mov edx,workspace.v2
    mov(x86_op_reg(edx), x86_op_ptr(&workspace.v2));
    // 	mov eax,workspace.v0
    mov(x86_op_reg(eax), x86_op_ptr(&workspace.v0));
    // 	mov ecx,workspace.v1
    mov(x86_op_reg(ecx), x86_op_ptr(&workspace.v1));
    // 	mov esi,2
    mov(x86_op_reg(esi), x86_op_imm(2));
    // if BASED_FIXED
    // 	mov ebx,dword ptr[edx+4*C_U]
    // 	mov ebp,dword ptr[eax+4*C_U]
    // 	mov edi,dword ptr[ecx+4*C_U]
    // else
    // 	fld dword ptr[edx+4*C_U]
    fld(x87_op_f(((brp_vertex *)edx->ptr_val)->comp_f[C_U]));
    // 	fadd fp_conv_d16
    fadd(x87_op_mem32(&fp_conv_d16));
    // 	fld dword ptr[eax+4*C_U]
    fld(x87_op_f(((brp_vertex *)eax->ptr_val)->comp_f[C_U]));
    // 	fadd fp_conv_d16
    fadd(x87_op_mem32(&fp_conv_d16));
    // 	fld dword ptr[ecx+4*C_U]
    fld(x87_op_f(((brp_vertex *)ecx->ptr_val)->comp_f[C_U]));
    // 	fadd fp_conv_d16
    fadd(x87_op_mem32(&fp_conv_d16));
    // 	fld dword ptr[edx+4*C_V]
    fld(x87_op_f(((brp_vertex *)edx->ptr_val)->comp_f[C_V]));
    // 	fadd fp_conv_d16
    fadd(x87_op_mem32(&fp_conv_d16));
    // 	fld dword ptr[eax+4*C_V]
    fld(x87_op_f(((brp_vertex *)eax->ptr_val)->comp_f[C_V]));
    // 	fadd fp_conv_d16
    fadd(x87_op_mem32(&fp_conv_d16));
    // 	fld dword ptr[ecx+4*C_V]
    fld(x87_op_f(((brp_vertex *)ecx->ptr_val)->comp_f[C_V]));
    // 	fadd fp_conv_d16
    fadd(x87_op_mem32(&fp_conv_d16));
    // 	 fxch st(2)
    fxch(2);
    // 	fstp qword ptr workspace.scratch0
    fstp(x87_op_mem64(&workspace.scratch0));
    // 	fstp qword ptr workspace.scratch2
    fstp(x87_op_mem64(&workspace.scratch2));
    // 	fstp qword ptr workspace.scratch4
    fstp(x87_op_mem64(&workspace.scratch4));
    // 	fstp qword ptr workspace.scratch6
    fstp(x87_op_mem64(&workspace.scratch6));
    // 	fstp qword ptr workspace.scratch8
    fstp(x87_op_mem64(&workspace.scratch8));
    // 	fstp qword ptr workspace.scratch10
    fstp(x87_op_mem64(&workspace.scratch10));
    // 	mov ebx,workspace.scratch6
    mov(x86_op_reg(ebx), x86_op_mem32(&workspace.scratch6));
    // 	mov ebp,workspace.scratch8
    mov(x86_op_reg(ebp), x86_op_mem32(&workspace.scratch8));
    // 	mov edi,workspace.scratch10
    mov(x86_op_reg(edi), x86_op_mem32(&workspace.scratch10));
    // endif
    // 	and ebx,0ffff0000h
    and(x86_op_reg(ebx), x86_op_imm(0xffff0000));
    // 	and ebp,0ffff0000h
    and(x86_op_reg(ebp), x86_op_imm(0xffff0000));
    // 	and edi,0ffff0000h
    and(x86_op_reg(edi), x86_op_imm(0xffff0000));
    // 	cmp ebx,ebp
    // 	jne wrapped
    if(ebx->uint_val != ebp->uint_val) {
        goto wrapped;
    }
    // 	cmp ebx,edi
    // 	jne wrapped
    if(ebx->uint_val != edi->uint_val) {
        goto wrapped;
    }
    // if BASED_FIXED
    // 	mov ebx,dword ptr[edx+4*C_V]
    // 	mov ebp,dword ptr[eax+4*C_V]
    // 	mov edi,dword ptr[ecx+4*C_V]
    // else
    // 	mov ebx,workspace.scratch0
    mov(x86_op_reg(ebx), x86_op_mem32(&workspace.scratch0));
    // 	mov ebp,workspace.scratch2
    mov(x86_op_reg(ebp), x86_op_mem32(&workspace.scratch2));
    // 	mov edi,workspace.scratch4
    mov(x86_op_reg(edi), x86_op_mem32(&workspace.scratch4));
    // endif
    and(x86_op_reg(ebx), x86_op_imm(0xffff0000));
    // 	and ebp,0ffff0000h
    and(x86_op_reg(ebp), x86_op_imm(0xffff0000));
    // 	and edi,0ffff0000h
    and(x86_op_reg(edi), x86_op_imm(0xffff0000));
    // 	cmp ebx,ebp
    // 	jne wrapped
    if(ebx->uint_val != ebp->uint_val) {
        goto wrapped;
    }
    // 	cmp ebx,edi
    // 	jne wrapped
    if(ebx->uint_val != edi->uint_val) {
        goto wrapped;
    }

    // 	mov esi,0
    mov(x86_op_reg(esi), x86_op_imm(0));
wrapped:
    // 	mov eax,workspace.flip
    mov(x86_op_reg(eax), x86_op_mem32(&workspace.flip));
    // 	or eax,esi
    or (x86_op_reg(eax), x86_op_reg(esi));
    // 	mov workspaceA.flags,eax
    mov(x86_op_mem32(&workspaceA.flags), x86_op_reg(eax));
    // endm
}

void REMOVE_INTEGER_PARTS_OF_PARAMETERS()
{
    // ; assumes 8.24 format
    // 	mov edi,0ffffffh
    mov(x86_op_reg(edi), x86_op_imm(0xffffff));
    // 	mov esi,0ff000000h
    mov(x86_op_reg(esi), x86_op_imm(0xff000000));
    // 	and workspace.s_u,0ffffffh
    and(x86_op_mem32(&workspace.s_u), x86_op_imm(0xffffff));
    // 	and workspace.s_v,0ffffffh
    and(x86_op_mem32(&workspace.s_v), x86_op_imm(0xffffff));

    REMOVE_INTEGER_PARTS_OF_PARAM(&workspace.d_u_x);
    REMOVE_INTEGER_PARTS_OF_PARAM(&workspace.d_u_y_0);
    REMOVE_INTEGER_PARTS_OF_PARAM(&workspace.d_u_y_1);

    REMOVE_INTEGER_PARTS_OF_PARAM(&workspace.d_v_x);
    REMOVE_INTEGER_PARTS_OF_PARAM(&workspace.d_v_y_0);
    REMOVE_INTEGER_PARTS_OF_PARAM(&workspace.d_v_y_1);
    // endm
}

void REMOVE_INTEGER_PARTS_OF_PARAM(void *param)
{
    // local paramNegative
    // 	mov ebp,esi
    mov(x86_op_reg(ebp), x86_op_reg(esi));
    // 	mov eax,param
    mov(x86_op_reg(eax), x86_op_mem32(param));

    // 	test eax,80000000h
    // 	jnz paramNegative
    if((eax->uint_val & 0x80000000) == 0) {
        // zf = 1;
    } else {
        // zf = 0;
        goto paramNegative;
    }

    // 	mov ebp,edi
    mov(x86_op_reg(ebp), x86_op_reg(edi));

    // 	and eax,ebp
    and(x86_op_reg(eax), x86_op_reg(ebp));
paramNegative:
    // 	and ebp,esi
    and(x86_op_reg(ebp), x86_op_reg(esi));

    // 	or eax,ebp
    or (x86_op_reg(eax), x86_op_reg(ebp));

    // 	mov param,eax
    mov(x86_op_mem32(param), x86_op_reg(eax));
    // endm
}

// MULTIPLY_UP_PARAM_VALUES macro param,dimension,magic ;24 cycles
void MULTIPLY_UP_PARAM_VALUES(int32_t s_p, int32_t d_p_x, int32_t d_p_y_0, int32_t d_p_y_1, void *a_sp, void *a_dpx,
                              void *a_dpy1, void *a_dpy0, uint32_t dimension, uint32_t magic)
{
    // ;										st(0)		st(1)		st(2)		st(3)		st(4)		st(5) st(6) st(7)
    assert(x86emu_fpu_stack_top() == -1);

    // fild work.texture.dimension;         d
    fild(dimension);
    // 	fild workspace.s_&param			;	sp			d
    fild(s_p);
    // 	fild workspace.d_&param&_x		;	dpdx		sp			d
    fild(d_p_x);
    // 	fild workspace.d_&param&_y_0	;	dpdy0		dpdx		sp			d
    fild(d_p_y_0);
    // 	fxch st(2)						;	sp			dpdx		dpdy0		d
    fxch(2);
    // 	fmul st,st(3)					;	spd			dpdx		dpdy0		d
    fmul_2(x87_op_i(0), x87_op_i(3));
    // 	fild workspace.d_&param&_y_1	;	dpdy1		spd			dpdx		dpdy0		d
    fild(d_p_y_1);
    // 	fxch st(2)						;	dpdx		spd			dpdy1		dpdy0		d
    fxch(2);
    // 	fmul st,st(4)					;	dpdxd		spd			dpdy1		dpdy0		d
    fmul_2(x87_op_i(0), x87_op_i(4));
    // 	 fxch st(1)						;	spd			dpdxd		dpdy1		dpdy0		d
    fxch(1);
    // 	fadd magic						;	spdx		dpdxd		dpdy1		dpdy0		d
    fadd(x87_op_mem32(&magic));
    // 	 fxch st(3)						;	dpdy0		dpdxd		dpdy1		spdx		d
    fxch(3);
    // 	fmul st,st(4)					;	dpdy0d		dpdxd		dpdy1		spdx		d
    fmul_2(x87_op_i(0), x87_op_i(4));
    // 	 fxch st(1)						;	dpdxd		dpdy0d		dpdy1		spdx		d
    fxch(1);
    // 	fadd magic						;	dpdxdx		dpdy0d		dpdy1		spdx		d
    fadd(x87_op_mem32(&magic));
    // 	 fxch st(2)						;	dpdy1		dpdy0d		dpdxdx		spdx		d
    fxch(2);
    // 	fmul st,st(4)					;	dpdy1d		dpdy0d		dpdxdx		spdx		d
    fmul_2(x87_op_i(0), x87_op_i(4));
    // 	 fxch st(4)						;	d			dpdy0d		dpdxdx		spdx		dpdy1d
    fxch(4);
    // 	fstp st(0)						;	dpdy0d		dpdxdx		spdx		dpdy1d
    fstp(x87_op_i(0));
    // 	fadd magic						;	dpdy0dx		dpdxdx		spdx		dpdy1d
    fadd(x87_op_mem32(&magic));
    // 	 fxch st(3)						;	dpdy1d		dpdxdx		spdx		dpdy0dx
    fxch(3);
    // 	fadd magic						;	dpdy1dx		dpdxdx		spdx		dpdy0dx
    fadd(x87_op_mem32(&magic));
    // 	 fxch st(2)						;	spdx		dpdxdx		dpdy1dx		dpdy0dx
    fxch(2);
    // 	fstp qword ptr workspaceA.s&param		;	dpdxdx		dpdy1dx		dpdy0dx
    fstp(x87_op_mem64(a_sp));
    // 	fstp qword ptr workspaceA.d&param&x	;	dpdy1dx		dpdy0dx
    fstp(x87_op_mem64(a_dpx));
    // 	fstp qword ptr workspaceA.d&param&y1	;	dpdy0dx
    fstp(x87_op_mem64(a_dpy1));
    // 	fstp qword ptr workspaceA.d&param&y0	;
    fstp(x87_op_mem64(a_dpy0));
    // endm
}

// SPLIT_INTO_INTEGER_AND_FRACTIONAL_PARTS macro ; 24 cycles
void SPLIT_INTO_INTEGER_AND_FRACTIONAL_PARTS()
{
    // 	mov ebx,workspaceA.sv
    mov(x86_op_reg(ebx), x86_op_mem32(&workspaceA.sv));

    // 	shl ebx,16
    shl(x86_op_reg(ebx), 16);
    // 	mov edx,workspaceA.dvx
    mov(x86_op_reg(edx), x86_op_mem32(&workspaceA.dvx));
    // 	shl edx,16
    shl(x86_op_reg(edx), 16);
    // 	mov workspaceA.svf,ebx
    mov(x86_op_mem32(&workspaceA.svf), x86_op_reg(ebx));
    // 	mov ebx,workspaceA.dvy0
    mov(x86_op_reg(ebx), x86_op_mem32(&workspaceA.dvy0));
    // 	mov workspaceA.dvxf,edx
    mov(x86_op_mem32(&workspaceA.dvxf), x86_op_reg(edx));
    // 	shl ebx,16
    shl(x86_op_reg(ebx), 16);
    // 	mov edx,workspaceA.dvy1
    mov(x86_op_reg(edx), x86_op_mem32(&workspaceA.dvy1));

    // 	shl edx,16
    shl(x86_op_reg(edx), 16);
    // 	mov workspaceA.dvy0f,ebx
    mov(x86_op_mem32(&workspaceA.dvy0f), x86_op_reg(ebx));
    // 	mov workspaceA.dvy1f,edx
    mov(x86_op_mem32(&workspaceA.dvy1f), x86_op_reg(edx));

    // ;integer parts

    // 	mov ebx,workspaceA.sv
    mov(x86_op_reg(ebx), x86_op_mem32(&workspaceA.sv));

    // 	sar ebx,16
    sar(x86_op_reg(ebx), 16);
    // 	mov edx,workspaceA.dvx
    mov(x86_op_reg(edx), x86_op_mem32(&workspaceA.dvx));

    // 	sar edx,16
    sar(x86_op_reg(edx), 16);
    // 	mov workspaceA.sv,ebx
    mov(x86_op_mem32(&workspaceA.sv), x86_op_reg(ebx));

    // 	mov ebx,workspaceA.dvy0
    mov(x86_op_reg(ebx), x86_op_mem32(&workspaceA.dvy0));
    // 	mov workspaceA.dvx,edx
    mov(x86_op_mem32(&workspaceA.dvx), x86_op_reg(edx));

    // 	sar ebx,16
    sar(x86_op_reg(ebx), 16);
    // 	mov edx,workspaceA.dvy1
    mov(x86_op_reg(edx), x86_op_mem32(&workspaceA.dvy1));

    // 	sar edx,16
    sar(x86_op_reg(edx), 16);
    // 	mov workspaceA.dvy0,ebx
    mov(x86_op_mem32(&workspaceA.dvy0), x86_op_reg(ebx));

    // 	mov workspaceA.dvy1,edx
    mov(x86_op_mem32(&workspaceA.dvy1), x86_op_reg(edx));

    // endm
}

// MULTIPLY_UP_V_BY_STRIDE macro magic; 24 cycles
void MULTIPLY_UP_V_BY_STRIDE(uint32_t magic)
{
    //  ;										st(0)		st(1)		st(2)		st(3)		st(4)		st(5) st(6) st(7)

    // 	fild work.texture.stride_b		;	d
    fild(work.texture.stride_b);
    // 	fild workspaceA.sv					;	sp			d
    fild(workspaceA.sv);
    // 	fild workspaceA.dvx					;	dpdx		sp			d
    fild(workspaceA.dvx);
    // 	fild workspaceA.dvy0					;	dpdy0		dpdx		sp			d
    fild(workspaceA.dvy0);
    // 	fxch st(2)						;	sp			dpdx		dpdy0		d
    fxch(2);
    // 	fmul st,st(3)					;	spd			dpdx		dpdy0		d
    fmul_2(x87_op_i(0), x87_op_i(3));
    // 	fild workspaceA.dvy1					;	dpdy1		spd			dpdx		dpdy0		d
    fild(workspaceA.dvy1);
    // 	fxch st(2)						;	dpdx		spd			dpdy1		dpdy0		d
    fxch(2);
    // 	fmul st,st(4)					;	dpdxd		spd			dpdy1		dpdy0		d
    fmul_2(x87_op_i(0), x87_op_i(4));
    // 	 fxch st(1)						;	spd			dpdxd		dpdy1		dpdy0		d
    fxch(1);
    // 	fadd magic						;	spdx		dpdxd		dpdy1		dpdy0		d
    fadd(x87_op_mem32(&magic));
    // 	 fxch st(3)						;	dpdy0		dpdxd		dpdy1		spdx		d
    fxch(3);
    // 	fmul st,st(4)					;	dpdy0d		dpdxd		dpdy1		spdx		d
    fmul_2(x87_op_i(0), x87_op_i(4));
    // 	 fxch st(1)						;	dpdxd		dpdy0d		dpdy1		spdx		d
    fxch(1);
    // 	fadd magic						;	dpdxdx		dpdy0d		dpdy1		spdx		d
    fadd(x87_op_mem32(&magic));
    // 	 fxch st(2)						;	dpdy1		dpdy0d		dpdxdx		spdx		d
    fxch(2);
    // 	fmul st,st(4)					;	dpdy1d		dpdy0d		dpdxdx		spdx		d
    fmul_2(x87_op_i(0), x87_op_i(4));
    // 	 fxch st(4)						;	d			dpdy0d		dpdxdx		spdx		dpdy1d
    fxch(4);
    // 	fstp st(0)						;	dpdy0d		dpdxdx		spdx		dpdy1d
    fstp(x87_op_i(0));
    // 	fadd magic						;	dpdy0dx		dpdxdx		spdx		dpdy1d
    fadd(x87_op_mem32(&magic));
    // 	 fxch st(3)						;	dpdy1d		dpdxdx		spdx		dpdy0dx
    fxch(3);
    // 	fadd magic						;	dpdy1dx		dpdxdx		spdx		dpdy0dx
    fadd(x87_op_mem32(&magic));
    // 	 fxch st(2)						;	spdx		dpdxdx		dpdy1dx		dpdy0dx
    fxch(2);
    // 	fstp qword ptr workspaceA.sv			;	dpdxdx		dpdy1dx		dpdy0dx
    fstp(x87_op_mem64(&workspaceA.sv));
    // 	fstp qword ptr workspaceA.dvx			;	dpdy1dx		dpdy0dx
    fstp(x87_op_mem64(&workspaceA.dvx));
    // 	fstp qword ptr workspaceA.dvy1		;	dpdy0dx
    fstp(x87_op_mem64(&workspaceA.dvy1));
    // 	fstp qword ptr workspaceA.dvy0		;
    fstp(x87_op_mem64(&workspaceA.dvy0));
    // endm
}

void CREATE_CARRY_VERSIONS()
{
    // mov eax,workspaceA.dvy0
    mov(x86_op_reg(eax), x86_op_mem32(&workspaceA.dvy0));
    // mov ebx,workspaceA.dvy1
    mov(x86_op_reg(ebx), x86_op_mem32(&workspaceA.dvy1));

    // add eax,work.texture.stride_b
    add(x86_op_reg(eax), x86_op_mem32(&work.texture.stride_b));
    // mov ecx,workspaceA.dvx
    mov(x86_op_reg(ecx), x86_op_mem32(&workspaceA.dvx));

    // add ebx,work.texture.stride_b
    add(x86_op_reg(ebx), x86_op_mem32(&work.texture.stride_b));
    // add ecx,work.texture.stride_b
    add(x86_op_reg(ecx), x86_op_mem32(&work.texture.stride_b));

    // mov workspaceA.dvy0c,eax
    mov(x86_op_mem32(&workspaceA.dvy0c), x86_op_reg(eax));
    // mov workspaceA.dvy1c,ebx
    mov(x86_op_mem32(&workspaceA.dvy1c), x86_op_reg(ebx));

    // mov workspaceA.dvxc,ecx
    mov(x86_op_mem32(&workspaceA.dvxc), x86_op_reg(ecx));
}

void WRAP_SETUP()
{
    // mov ecx,
    mov(x86_op_reg(ecx), x86_op_mem32(&work.texture.width_p));
    // mov eax,work.texture._size
    mov(x86_op_reg(eax), x86_op_mem32(&work.texture.size));
    // shl ecx,16
    shl(x86_op_reg(ecx), 16);
    // add eax,work.texture.base
    // add(x86_op_reg(eax), x86_op_ptr(work.texture.base));  //doesnt work with pointers
    // mov workspaceA.uUpperBound,ecx
    mov(x86_op_mem32(&workspaceA.uUpperBound), x86_op_reg(ecx));

    // mov workspaceA.vUpperBound,eax
    // mov(x86_op_mem32(&workspaceA.vUpperBound), x86_op_reg(eax)); doesn't work with pointers
    //workspaceA.vUpperBound = ((uint8_t *)work.texture.base) + work.texture.size;
    workspaceA.vUpperBound = 0 + work.texture.size;
}

void MAKE_N_LOW_BIT_MASK(uint32_t *name, int n) {
    int count;
    for (count = 0; count < n; count++) {
        *name |= 1 << count;
    }
}
