#include <stdint.h>
#include "../softrend/ddi/priminfo.h"
#include "x86emu.h"
#include "fpsetup.h"
#include "fpwork.h"
#include "work.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "common.h"

uint64_t  fconv_d16_12[2] = {0x04238000000000000, 0x04238000000010000};
uint64_t  fconv_d16_m[2]  = {0x04238000000010000, 0x04238000000000000};

float fp_one          = 1.0f;
float fp_two          = 2.0f;
float fp_four         = 4.0f;

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
double                         temporary_intensity;
struct workspace_t               workspace;
struct ArbitraryWidthWorkspace_t workspaceA;

// // FIXME: These are originally macros. Functions for now
// static int SETUP_FLOAT(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2);
// static void ARBITRARY_SETUP();
// static void SETUP_FLAGS();
// static void REMOVE_INTEGER_PARTS_OF_PARAMETERS();
// static void REMOVE_INTEGER_PARTS_OF_PARAM(uint32_t *param);
// static void MULTIPLY_UP_PARAM_VALUES(int32_t s_p, int32_t d_p_x, int32_t d_p_y_0, int32_t d_p_y_1, void *a_sp, void *a_dpx,
//                               void *a_dpy1, void *a_dpy0, uint32_t dimension, uint32_t magic);
// static void SPLIT_INTO_INTEGER_AND_FRACTIONAL_PARTS();
// static void MULTIPLY_UP_V_BY_STRIDE(uint32_t magic);
// static void CREATE_CARRY_VERSIONS();
// static void WRAP_SETUP();


static int SETUP_FLOAT(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2)
{
    assert(x86_state.x87_stack_top == -1);
    eax.ptr_v = v0;
    ecx.ptr_v = v1;
    edx.ptr_v = v2;
    // local count_cont,exit,top_zero,bottom_zero,empty_triangle

    // assume eax: ptr brp_vertex, /*ebx: ptr brp_vertex,*/ ecx: ptr brp_vertex, edx: ptr brp_vertex

    //; Calculate area of triangle and generate dx1/2area, dx1/2area, dx1/2area and dx1/2area
    //;
    //; Also sort the vertices in Y order whilst divide is happening
    //;
    //;	0		1		2		3		4		5		6		7

    // fld			[edx].comp_f[C_SX*4]		;	x2
    FLD(((brp_vertex *)edx.ptr_v)->comp_f[C_SX]); //	x2
    // fsub		[eax].comp_f[C_SX*4]		;	dx2
    FSUB(((brp_vertex *)eax.ptr_v)->comp_f[C_SX]); //	dx2
    // fld			[ecx].comp_f[C_SX*4]		;	x1		dx2
    FLD(((brp_vertex *)ecx.ptr_v)->comp_f[C_SX]); //	x1		dx2
    // fsub		[eax].comp_f[C_SX*4]		;	dx1		dx2
    FSUB(((brp_vertex *)eax.ptr_v)->comp_f[C_SX]); //	dx1		dx2
    // fld			[edx].comp_f[C_SY*4]		;	y2		dx1		dx2
    FLD(((brp_vertex *)edx.ptr_v)->comp_f[C_SY]); //	y2		dx1		dx2
    // fsub		[eax].comp_f[C_SY*4]		;	dy2		dx1		dx2
    FSUB(((brp_vertex *)eax.ptr_v)->comp_f[C_SY]);          //	dy2		dx1		dx2
    FLD(((brp_vertex *)ecx.ptr_v)->comp_f[C_SY]); //	y1		dy2		dx1		dx2
    FSUB(((brp_vertex *)eax.ptr_v)->comp_f[C_SY]);          //	dy1		dy2		dx1		dx2

    FLD_ST(2); //	dx1		dy1		dy2		dx1		dx2

    FMUL_ST(0, 2); //	dx1*dy2	dy1		dy2		dx1		dx2

    FLD_ST(4);                 //	dx2		dx1*dy2	dy1		dy2		dx1		dx2
    FMUL_ST(0, 2); //	dx2*dy1	dx1*dy2	dy1		dy2		dx1		dx2

    eax.float_val = ((brp_vertex *)eax.ptr_v)->comp_f[C_SY];
    ecx.float_val = ((brp_vertex *)ecx.ptr_v)->comp_f[C_SY];

    FSUBP_ST(1, 0); //	2area	dy1		dy2		dx1		dx2

    ebx.v ^= ebx.v;
    CMP(ecx.v, eax.v);

    RCL_1(ebx.v);
    edx.float_val = ((brp_vertex *)edx.ptr_v)->comp_f[C_SY];

    FDIVR(fp_one); //	1/2area	dy1		dy2		dx1		dx2

    CMP(edx.v, eax.v);
    RCL_1(ebx.v);


    CMP(edx.v, ecx.v)
    RCL_1(ebx.v); // ebx now has 3 bit number characterising the order of the vertices.

    eax.v = sort_table_0[ebx.v];
    edx.v = sort_table_2[ebx.v];
    esi.v = flip_table[ebx.v];
    ebx.v = sort_table_1[ebx.v];

    // Load eax,ebx,edx with pointers to the three vertices in vertical order
    eax.ptr_v = workspace.v0_array[eax.v];
    edx.ptr_v = workspace.v0_array[edx.v];
    ebx.ptr_v = workspace.v0_array[ebx.v];
    workspace.flip = esi.v;

    // Work out Y extents of triangle
    // ; Convert float to int using integer instructions, because FPU is in use doing division
    ebp.float_val = ((brp_vertex *)eax.ptr_v)->comp_f[C_SY];
    ecx.v = EXPONENT_OFFSET;
    ecx.v -= ebp.v;
    ebp.v &= MASK_MANTISSA;
    ecx.v >>= 23;
    ebp.v |= IMPLICIT_ONE;
    ebp.v >>= ecx.l;
    esi.float_val = ((brp_vertex *)ebx.ptr_v)->comp_f[C_SY];
    ecx.v = EXPONENT_OFFSET;
    ecx.v -= esi.v;
    esi.v &= MASK_MANTISSA;
    ecx.v >>= 23;
    esi.v |= IMPLICIT_ONE;
    // shr		 ebp,cl				; ESI = y_m
    esi.v >>= ecx.l;

    edi.float_val = ((brp_vertex *)edx.ptr_v)->comp_f[C_SY];
    ecx.v = EXPONENT_OFFSET;
    ecx.v -= edi.v;
    edi.v &= MASK_MANTISSA;
    ecx.v >>= 23;
    edi.v |= IMPLICIT_ONE;
    // shr		 edi,cl				; edi = y_b
    edi.v >>= ecx.l;

    // Catch special cases of empty top or bottom trapezoids

    // cmp(ebp, esi);
    // je(top_zero);
    if(ebp.v == esi.v) {
        goto top_zero;
    }

    // cmp(esi, edi);
    // je(bottom_zero);
    if(esi.v == edi.v) {
        goto bottom_zero;
    }

    //; Parameter gradient startup and Y deltas for edge gradients

    //	0		1		2		3		4		5		6		7
    FMUL_ST(1, 0);                          //	1/2area	dy1*a	dy2		dx1		dx2
    FLD(((brp_vertex *)ebx.ptr_v)->comp_f[C_SY]); //	sy2		1/2area	dy1*a	dy2		dx1		dx2
    FSUB((((brp_vertex *)eax.ptr_v)->comp_f[C_SY]));        //   dsy1	1/2area	dy1*a	dy2		dx1		dx2
    FXCH(3);                                                   //   dy2  	1/2area	dy1*a	dsy1	dx1		dx2
    FMUL_ST(0, 1);                          //	dy2*a  	1/2area	dy1*a	dsy1	dx1		dx2
    FLD(((brp_vertex *)edx.ptr_v)->comp_f[C_SY]); //   sy3	 dy2*a  	1/2area	dy1*a	dsy1	dx1 dx2
    FSUB((((brp_vertex *)ebx.ptr_v)->comp_f[C_SY]));        //   dsy2	dy2*a  	1/2area	dy1*a	dsy1	dx1		dx2
    FXCH(5);                                                   //   dx1	  dy2*a 	1/2area	dy1*a	dsy1	dsy2	dx2

count_cont:
    FMUL_ST(0, 2);                          //	dx1*a   dy2*a  	1/2area	dy1*a	dsy1	dsy2	dx2
    FLD(((brp_vertex *)edx.ptr_v)->comp_f[C_SY]); //   sy3	 dx1*a   dy2*a  	1/2area	dy1*a	dsy1 dsy2 dx2
    FSUB(((brp_vertex *)eax.ptr_v)->comp_f[C_SY]);          //   dsy3	dx1*a   dy2*a  	1/2area	dy1*a	dsy1	dsy2	dx2
    FXCH(7);                                                   //   dx2		dx1*a   dy2*a  	1/2area	dy1*a	dsy1	dsy2	dsy3
    FMUL_ST(0, 3);                          //   dx2*a	dx1*a   dy2*a  	1/2area	dy1*a	dsy1	dsy2	dsy3
    FXCH(3);                                                   //   1/2area	dx1*a   dy2*a  	dx2*a	dy1*a	dsy1	dsy2	dsy3

    FSTP(&workspace.iarea);
    FSTP(&workspace.dx1_a);
    FSTP(&workspace.dy2_a);
    FSTP(&workspace.dx2_a);
    FSTP(&workspace.dy1_a); //  	dy1		dy2		dy3

    //; Find edge gradients of triangle
    //;
    //; R = 1/(dy1.dy2.dy3)
    //;
    //; gradient_major = dy1.dx2.dy3.R
    //; gradient_minor1 = dx1.dy2.dy3.R
    //; gradient_minor2 = dy1.dy2.dx3.R
    //;
    // 													;	0		1		2		3		4		5		6 7
    // fld st(2)                                        ;	dy3		dy1		dy2		dy3
    FLD_ST(2);
    // 			fmul		st,st(2)					;	dy2*dy3	dy1		dy2		dy3
    FMUL_ST(0, 2);
    // 			fld			[ebx].comp_f[C_SX*4]		;	x2		dy2*dy3	dy1		dy2		dy3
    FLD(((brp_vertex *)ebx.ptr_v)->comp_f[C_SX]);
    // 			fsub		[eax].comp_f[C_SX*4]		;	dx1		dy2*dy3	dy1		dy2		dy3
    FSUB((((brp_vertex *)eax.ptr_v)->comp_f[C_SX]));
    // 			fld			st(1)						;	dy2*dy3 dx1		dy2*dy3	dy1		dy2		dy3
    FLD_ST(1);
    // 			fmul		st,st(3)					;	dy123	dx1		dy2*dy3	dy1		dy2		dy3
    FMUL_ST(0, 3);

    // 			fld			[edx].comp_f[C_SX*4]		;	x3		dy123	dx1		dy2*dy3	dy1		dy2		dy3
    FLD(((brp_vertex *)edx.ptr_v)->comp_f[C_SX]);
    // 			fsub		[ebx].comp_f[C_SX*4]		;	dx2		dy123	dx1		dy2*dy3	dy1		dy2		dy3
    FSUB((((brp_vertex *)ebx.ptr_v)->comp_f[C_SX]));
    // 			 FXCH		 st(2)						;	dx1		dy123	dx2		dy2*dy3	dy1		dy2		dy3
    FXCH(2);
    // 			fld			fp_one						;	1.0		dx1		dy123	dx2		dy2*dy3	dy1		dy2 dy3
    FLD(fp_one);
    // 			fdivrp		st(2),st					;	dx1		R		dx2		dy2*dy3	dy1		dy2		dy3
    FDIVRP_ST(2, 0);

    // ; Generate counts
    // ;
    // 		inc			ebp
    ebp.v++;
    // 		mov			ecx,esi
    ecx.v = esi.v;
    // 		sub			ecx,ebp				;  count_t = (y_m-y_t)-1
    ecx.v -= ebp.v;
    // 		mov			[workspace.t_y],ebp			; save for X intercept calcs
    workspace.t_y = ebp.v;
    // 		mov			[workspace.topCount],ecx
    workspace.topCount = ecx.int_val;
    // 		inc			esi
    esi.v++;
    // 		sub			edi,esi				;  count_b = (y_b-y_m)-1
    edi.v -= esi.v;
    // 		mov			m_y,esi				; save for X intercept calcs
    m_y = esi.v;
    // 		mov			[workspace].bottomCount,edi
    workspace.bottomCount = edi.int_val;
    // 		mov			esi,[workspace.flip]
    esi.v = workspace.flip;

    //     	; Generate LR/RL flag into esi (used to index convertion numbers below)
    // 	;
    // 			mov			edi,workspace.iarea
    edi.v = workspace.iarea;
    // ;V
    // 			xor			esi,edi			; Build LR flag in bit 31
    esi.v ^= edi.v;
    // ;V
    // 			shr			esi,31			; move down to bit 0
    esi.v >>= 31;
    // ;V
    // 			mov			[workspace.flip],esi
    workspace.flip = esi.v;

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
    FMULP_ST(3, 0);
    // 		fld			[edx].comp_f[C_SX*4]		;	x3		R		dx2		XYY		dy1		dy2		dy3
    FLD(((brp_vertex *)edx.ptr_v)->comp_f[C_SX]);
    // 		 FXCH		st(3)						;	XYY		R		dx2		x3		dy1		dy2		dy3
    FXCH(3);
    // 		fmul		st,st(1)					;	XYY*R	R		dx2		x3		dy1		dy2		dy3
    FMUL_ST(0, 1);
    // 		 FXCH		st(3)						;	x3		R		dx2		XYY*R	dy1		dy2		dy3
    FXCH(3);
    // 		fsub		[eax].comp_f[C_SX*4]		;	dx3		R		dx2		XYY*R	dy1		dy2		dy3
    FSUB((((brp_vertex *)eax.ptr_v)->comp_f[C_SX]));
    // 		 FXCH		st(1)						;	R		dx3		dx2		XYY*R	dy1		dy2		dy3
    FXCH(1);
    // 		fmulp		st(4),st					;	dx3		dx2		XYY*R	dy1*R	dy2		dy3
    FMULP_ST(4, 0);
    // 		 FXCH		st(2)						;	XYY*R	dx2		dx3		dy1*R	dy2		dy3
    FXCH(2);
    // 		fild		m_y				            ;	m_y		XYY*R	dx2		dx3		dy1*R	dy2		dy3
    FILD(m_y);
    // 		 FXCH		st(2)						;	dx2		XYY*R	m_y		dx3		dy1*R	dy2		dy3
    FXCH(2);
    // 		fmulp		st(6),st		            ;	XYY*R	m_y		dx3		dy1*R	dy2		dx2*dy3
    FMULP_ST(6, 0);
    // 		fild		[workspace.t_y]				;	t_y		XYY*R	m_y		dx3		dy1*R	dy2 dx2*dy3
    FILD(workspace.t_y);
    // FXCH		st(3)			            ;	dx3		XYY*R	m_y		t_y		dy1*R	dy2		dx2*dy3
    FXCH(3);
    // 		fmulp		st(5),st					;	XYY*R	m_y		t_y		dy1*R	dy2*dx3	dx2*dy3
    FMULP_ST(5, 0);
    // 		 FXCH		st(1)			            ;	m_y		XYY*R	t_y		dy1*R	dy2*dx3	dx2*dy3
    FXCH(1);
    // 		fsub		[ebx].comp_f[C_SY*4]		;	m_dy	XYY*R	t_y		dy1*R	dy2*dx3	dx2*dy3
    FSUB((((brp_vertex *)ebx.ptr_v)->comp_f[C_SY]));
    // 		 FXCH		st(3)						;	dy1*R	XYY*R	t_y		m_dy	dy2*dx3	dx2*dy3
    FXCH(3);
    // 		fmul		st(4),st		            ;	dy1*R	XYY*R	t_y		m_dy	YYX*R	dx2*dy3
    FMUL_ST(4, 0);
    // 		 FXCH		st(2)						;	t_y		XYY*R	dy1*R	m_dy	YYX*R	dx2*dy3
    FXCH(2);
    // 		fsub		[eax].comp_f[C_SY*4]		;	t_dy	XYY*R	dy1*R	m_dy	YYX*R	dx2*dy3
    FSUB((((brp_vertex *)eax.ptr_v)->comp_f[C_SY]));
    // 		 FXCH		st(2)						;	dy1*R	XYY*R	t_dy	m_dy	YYX*R	dx2*dy3
    FXCH(2);
    // 		fmulp		st(5),st		            ;	XYY*R	t_dy	m_dy	YYX*R	YXY*R
    FMULP_ST(5, 0);
    // 		 FXCH		st(2)						;	m_dy	t_dy	XYY*R	YYX*R	YXY*R
    FXCH(2);
    // 												;	m_dy	t_dy	g1		gm		g2

    // ; Work out initial X intercepts with top and middle scanlines
    // ;
    // ; x_major  = gm * t_dy + vt->x
    // ; x_minor1 = g1 * t_dy + vt->x
    // ; x_minor2 = g2 * m_dy + vm->x
    // ;
    // 												;	0		1		2		3		4		5		6		7
    // 		fld			st(1)						;	t_dy	m_dy	t_dy	g1		gm		g2
    FLD_ST(1);
    // 		FXCH		st(1)			            ;	m_dy	t_dy	t_dy	g1		gm		g2
    FXCH(1);
    // 		fmul		st,st(5)		            ;	m_dy*g2	t_dy	t_dy	g1		gm		g2
    FMUL_ST(0, 5);
    // 		FXCH		st(2)						;	t_dy	t_dy	m_dy*g2	g1		gm		g2
    FXCH(2);
    // 		fst			[workspace.t_dy]
    FST(&workspace.t_dy);
    // 		fmul		st,st(3)					;	t_dy*g1	t_dy	m_dy*g2	g1		gm		g2
    FMUL_ST(0, 3);
    // 		 FXCH		st(2)			            ;	m_dy*g2	t_dy	t_dy*g1	g1		gm		g2
    FXCH(2);
    // 		fadd		[ebx].comp_f[C_SX*4]		;	x_2		t_dy	t_dy*g1	g1		gm		g2
    FADD(((brp_vertex *)ebx.ptr_v)->comp_f[C_SX]);
    // 		 FXCH		st(1)						;	t_dy	x_2		t_dy*g1	g1		gm		g2
    FXCH(1);
    // 		fmul		st,st(4)		            ;	t_dy*gm	x_2		t_dy*g1	g1		gm		g2
    FMUL_ST(0, 4);
    // 		 FXCH		st(2)						;	t_dy*g1	x_2		t_dy*gm	g1		gm		g2
    FXCH(2);
    // 		fadd		[eax].comp_f[C_SX*4]		;	x_1		x_2		t_dy*gm	g1		gm		g2
    FADD(((brp_vertex *)eax.ptr_v)->comp_f[C_SX]);
    // 		 FXCH		st(3)						;	g1		x_2		t_dy*gm	x_1		gm		g2
    FXCH(3);
    // 		fadd		fp_conv_d16		            ;	g1+C	x_2		t_dy*gm	x_1		gm		g2
    FADD(fp_conv_d16);
    // 		 FXCH		st(2)						;	t_dy*gm	x_2		g1+C	x_1		gm		g2
    FXCH(2);
    // 		fadd		[eax].comp_f[C_SX*4]		;	x_m		x_2		g1+C	x_1		gm		g2
    FADD(((brp_vertex *)eax.ptr_v)->comp_f[C_SX]);
    // 		 FXCH		st(4)						;	gm		x_2		g1+C	x_1		x_m		g2
    FXCH(4);
    // 		fadd		fp_conv_d16		            ;	gm+C	x_2		g1+C	x_1		x_m		g2
    FADD(fp_conv_d16);
    // 		 FXCH		st(1)						;	x_2		gm+C	g1+C	x_1		x_m		g2
    FXCH(1);
    // 		fadd	fconv_d16_12[esi*8]	            ;	x_2+C	gm+C	g1+C	x_1		x_m		g2
    assert(esi.v >= 0 && esi.v <= 1);
    FADD64(fconv_d16_12[esi.v]);
    // 		 FXCH		st(5)						;	g2		gm+C	g1+C	x_1		x_m		x_2+C
    FXCH(5);
    // 		fadd		fp_conv_d16		              ;	g2+C	gm+C	g1+C	x_1		x_m		x_2+C
    FADD(fp_conv_d16);
    // 		 FXCH		st(2)						;	g1+C	gm+C	g2+C	x_1		x_m		x_2+C
    FXCH(2);
    // 		fstp real8 ptr [workspace].x1			;	gm+C	g2+C	x_1		x_m		x_2+C
    FSTP64(&workspace.x1_double);
    // 		fstp real8 ptr [workspace].xm			;	g2+C	x_1		x_m		x_2+C
    FSTP64(&workspace.xm_double);
    // 		fstp real8 ptr [workspace].x2			;	x_1		x_m		x_2+C
    FSTP64(&workspace.x2_double);
    // 		fadd	fconv_d16_12[esi*8]				;	x_1+C	x_m		x_2+C
    FADD64(fconv_d16_12[esi.v]);
    // 		FXCH		st(1)						;	x_m		x_1+C	x_2+C
    FXCH(1);
    // 		fadd	fconv_d16_m[esi*8]				;	x_m+C	x_1+C	x_2+C
    FADD64(fconv_d16_m[esi.v]);

    // 	; Load deltas back in registers
    // 	;
    // 		mov			edx,[workspace].xm	; read fixed d_xm
    edx.v = workspace.xm;
    // 		mov			esi,[workspace].x1	; read fixed d_x1
    esi.v = workspace.x1;
    // 		mov			edi,[workspace].x2	; read fixed d_x2
    edi.v = workspace.x2;
    // 		mov		ebx,[workspace.v0]				; Start preparing for parmeter setup
    ebx.ptr_v = workspace.v0;
    // 		fstp real8 ptr [workspace].xm			;	x_1+C	x_2+C
    FSTP64(&workspace.xm_double);
    // 		fstp real8 ptr [workspace].x1			;	x_2+C
    FSTP64(&workspace.x1_double);

    // 		mov			ecx,[workspace].xm
    ecx.v = workspace.xm;
    // 		mov			[workspace].xm+4,edx
    workspace.d_xm = edx.v;
    // 		sar			ecx,16
    ecx.int_val >>= 16;
    // 		mov			[workspace].x1+4,esi
    workspace.d_x1 = esi.v;
    // 		sar			edx,16			; get integer part of x delta down major edge
    edx.int_val >>= 16;
    // 		mov			[workspace.t_dx],ecx
    workspace.t_dx = ecx.v;
    // 		fild		[workspace.t_dx]			;	t_x		x_2+C
    FILD(workspace.t_dx);
    // 	; Generate floating point versions of x delta and x delta+4
    // 	;
    // 		mov			[workspace.xstep_0],edx
    workspace.xstep_0 = edx.v;
    // 		inc edx
    edx.v++;
    // 		mov			[workspace.xstep_1],edx
    workspace.xstep_1 = edx.v;
    // 		mov			edx,[workspace.v2]				; Start preparing for parmeter setup
    edx.ptr_v = workspace.v2;
    // 												;	0		1		2		3		4		5		6		7
    // 		fsub		[eax].comp_f[C_SX*4]		;	t_dx	x_2+C
    FSUB((((brp_vertex *)eax.ptr_v)->comp_f[C_SX]));
    // 		 FXCH		st(1)						;	x_2+C	t_dx
    FXCH(1);
    // 		fstp real8 ptr [workspace].x2			;	t_dx
    FSTP64(&workspace.x2_double);

    // 		fild		[workspace.xstep_0]			;	xstep_0	t_dx
    FILD(workspace.xstep_0);
    // 		fild		[workspace.xstep_1]			;	xstep_1 xstep_0	t_dx
    FILD(workspace.xstep_1);
    // 		 FXCH		st(2)			;	tdx		xstep_0	xstep_1
    FXCH(2);
    // 		fstp		[workspace.t_dx]			;	xstep_0	xstep_1
    FSTP(&workspace.t_dx);
    // 		mov			[workspace].x2+4,edi
    workspace.d_x2 = edi.v;
    // 		mov		ecx,[workspace.v1]				; Start preparing for parmeter setup
    ecx.ptr_v = workspace.v1;
    // 		fstp		[workspace.xstep_0]			;	step_1
    FSTP(&workspace.xstep_0);
    // 		fstp		[workspace.xstep_1]			;
    FSTP(&workspace.xstep_1);

    // 		jmp			exit
    goto exit;
    // ; Special cases for top or bottom counts == 0
    // ;

top_zero:
    // cmp			ebp,edi			; Check for completely empty triangle
    // je			empty_triangle
    if(ebp.v == edi.v) {
        goto empty_triangle;
    }
    // 										;	0		1		2		3		4		5		6		7
    FMUL_ST(1, 0);                          //	1/2area	dy1*a	dy2		dx1		dx2
    FLD(fp_one);                                     //	1.0		1/2area	dy1*a	dy2		dx1		dx2
    FXCH(3);                                                   //   dy2  	1/2area	dy1*a	1.0		dx1		dx2
    FMUL_ST(0, 1);                          //	dy2*a  	1/2area	dy1*a	1.0		dx1		dx2
    FLD(((brp_vertex *)edx.ptr_v)->comp_f[C_SY]); //   sy3	 dy2*a  	1/2area	dy1*a	1.0		dx1 dx2
    // fsub		[ebx].comp_f[C_SY*4]		;   dsy2	dy2*a  	1/2area	dy1*a	1.0		dx1		dx2
    FSUB(((brp_vertex *)ebx.ptr_v)->comp_f[C_SY]);
    //  FXCH	   st(5)						;   dx1	  dy2*a 	1/2area	dy1*a	1.0		dsy2	dx2
    FXCH(5);

    // jmp		count_cont
    goto count_cont;

bottom_zero:
    // 												;	0		1		2		3		4		5		6		7
    // 		fmul		st(1),st					;	1/2area	dy1*a	dy2		dx1		dx2
    FMUL_ST(1, 0);
    // 		fld			[ebx].comp_f[C_SY*4]		;	sy2		1/2area	dy1*a	dy2		dx1		dx2
    FLD(((brp_vertex *)ebx.ptr_v)->comp_f[C_SY]);
    // 		fsub		[eax].comp_f[C_SY*4]		;   dsy1	1/2area	dy1*a	dy2		dx1		dx2
    FSUB(((brp_vertex *)eax.ptr_v)->comp_f[C_SY]);
    // 		 FXCH	   st(3)						;   dy2  	1/2area	dy1*a	dsy1	dx1		dx2
    FXCH(3);
    // 		fmul		st,st(1)					;	dy2*a  	1/2area	dy1*a	dsy1	dx1		dx2
    FMUL_ST(0, 1);
    // 		fld			fp_one						;   1.0	 dy2*a  	1/2area	dy1*a	dsy1	dx1		dx2
    FLD(fp_one);
    // 		 FXCH	   st(5)						;   dx1	  dy2*a 	1/2area	dy1*a	dsy1	1.0		dx2
    FXCH(5);
    // 		jmp		count_cont
    goto count_cont;

    // ; Fill in block with NULL count and exit
    // ;
empty_triangle:
    // mov workspace.topCount,-1
    workspace.topCount = -1;
    // mov workspace.bottomCount,-1
    workspace.bottomCount = -1;
    FSTP_ST(0);
    FSTP_ST(0);
    FSTP_ST(0);
    FSTP_ST(0);
    FSTP_ST(0);
    return FPSETUP_EMPTY_TRIANGLE;

exit:
    assert(x86_state.x87_stack_top == -1);
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
void SETUP_FLOAT_PARAM(int comp, char *param /*unused*/, fp64_t *s_p, fp64_t *d_p_x, uint32_t conv, int is_unsigned)
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
    FLD(((brp_vertex *)edx.ptr_v)->comp_f[comp]);
    // 			fsub	[ebx].comp_f[comp*4]		;	dp2
    FSUB(((brp_vertex *)ebx.ptr_v)->comp_f[comp]);
    // 			fld		[ecx].comp_f[comp*4]		;	p1		dp2
    FLD(((brp_vertex *)ecx.ptr_v)->comp_f[comp]);
    // 			fsub	[ebx].comp_f[comp*4]		;	dp1		dp2
    FSUB(((brp_vertex *)ebx.ptr_v)->comp_f[comp]);

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
    FLD_ST(1);
    // 			fmul	[workspace.dy1_a]			;	dp2*a	dp1		dp2
    FMUL(*(float *)&workspace.dy1_a);
    // 			fld		st(1)						;	dp1		dp2*a	dp1		dp2
    FLD_ST(1);
    // 			fmul	[workspace.dy2_a]			;	dp1*b	dp2*a	dp1		dp2
    FMUL(*(float *)&workspace.dy2_a);
    // 			fld		[workspace.t_dx]			;	fdx		dp1*b	dp2*a	dp1		dp2
    FLD(*(float *)&workspace.t_dx);
    // 			 FXCH	st(4)						;	dp2		dp1*b	dp2*a	dp1		fdx
    FXCH(4);
    // 			fmul	[workspace.dx1_a]			;	dp2*c	dp1*b	dp2*a	dp1		fdx
    FMUL(*(float *)&workspace.dx1_a);
    // 			fld		[workspace.t_dy]			; 	fdy		dp2*c	dp1*b	dp2*a	dp1		fdx
    FLD(*(float *)&workspace.t_dy);
    // 			 FXCH	st(4)						; 	dp1		dp2*c	dp1*b	dp2*a	fdy		fdx
    FXCH(4);
    // 			fmul	[workspace.dx2_a]			; 	dp1*d	dp2*c	dp1*b	dp2*a	fdy		fdx
    FMUL(*(float *)&workspace.dx2_a);
    // 			 FXCH	st(3)						; 	dp2*a	dp2*c	dp1*b	dp1*d	fdy		fdx
    FXCH(3);
    // 			fsubp	st(2),st					; 	dp2*c	d1b-d2a	dp1*d	fdy		fdx
    FSUBP_ST(2, 0);
    // 			fld		[eax].comp_f[comp*4]		; 	param_t	dp2*c	d1b-d2a	dp1*d	fdy		fdx
    FLD(((brp_vertex *)eax.ptr_v)->comp_f[comp]);
    // 			 FXCH	st(3)						; 	dp1*d	dp2*c	d1b-d2a	param_t	fdy		fdx
    FXCH(3);
    // 			fsubp	st(1),st					; 	d2c-d1d	d1b-d2a	param_t	fdy		fdx
    FSUBP_ST(1, 0);
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
    FLD_ST(1);
    // 			fmul	[workspace.xstep_0]			; 	pdx*xs0	pdy		pdx		param_t	fdy		fdx
    FMUL(*(float *)&workspace.xstep_0);
    // 			fld		st(2)						; 	pdx		pdx*xs0	pdy		pdx		param_t	fdy		fdx
    FLD_ST(2);
    // 			fmul	[workspace.xstep_1]			; 	pdx*xs1	pdx*xs0	pdy		pdx		param_t	fdy		fdx
    FMUL(*(float *)&workspace.xstep_1);
    // 			 FXCH	st(1)						; 	pdx*xs0	pdx*xs1	pdy		pdx		param_t	fdy		fdx
    FXCH(1);
    // 			fadd	st,st(2)					; 	pdy_0	pdx*xs1	pdy		pdx		param_t	fdy		fdx
    FADD_ST(0, 2);
    // 			 FXCH	st(3)						; 	pdx		pdx*xs1	pdy		pdy_0	param_t	fdy		fdx
    FXCH(3);
    // 			fmul	st(6),st					; 	pdx		pdx*xs1	pdy		pdy_0	param_t	fdy		fdx*pdx
    FMUL_ST(6, 0);
    // 			 FXCH	st(2)						; 	pdy		pdx*xs1	pdx		pdy_0	param_t	fdy		fdx*pdx
    FXCH(2);
    // 			fadd	st(1),st					; 	pdy		pdy_1	pdx		pdy_0	param_t	fdy		fdx*pdx
    FADD_ST(1, 0);
    // 			fmulp	st(5),st					; 	pdy_1	pdx		pdy_0	param_t	fdy*pdy	fdx*pdx
    FMULP_ST(5, 0);
    // 			 FXCH	st(3)						; 	param_t	pdx		pdy_0	pdy_1	fdy*pdy	fdx*pdx
    FXCH(3);
    // 			faddp	st(5),st					; 	pdx		pdy_0	pdy_1	fdy*pdy	fpx+pt
    FADDP_ST(5, 0);
    // 			 FXCH	st(1)						; 	pdy_0	pdx		pdy_1	fdy*pdy	fpx+pt
    FXCH(1);
    // 			fadd	conv						; 	C+pdy_0	pdx		pdy_1	fdy*pdy	fpx+pt
    FADD(conv);
    // 			 FXCH	st(2)						; 	pdy_1	pdx		C+pdy_0	fdy*pdy	fpx+pt
    FXCH(2);
    // 			fadd	conv						; 	C+pdy_1	pdx		C+pdy_0	fdy*pdy	fpx+pt
    FADD(conv);
    // 			 FXCH	st(3)						; 	fdy*pdy	pdx		C+pdy_0	C+pdy_1	fpx+pt
    FXCH(3);
    // 			faddp	st(4),st					;	pdx		C+pdy_0	C+pdy_1	pstart
    FADDP_ST(4, 0);

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
    FADD(conv);
    // 			 FXCH	st(3)			; 	pstart	C+pdy_0	C+pdy_1	C+pdx
    FXCH(3);
    // ; 1 clock delay

    // 			fadd	conv			; 	C+pstrt	C+pdy_0	C+pdy_1	C+pdx
    FADD(conv);
    // 			 FXCH	st(2)			; 	C+pdy_1	C+pdy_0	C+pstrt	C+pdx
    FXCH(2);
    // 			fstp	real8 ptr s_p
    FSTP64(&s_p->double_val);
    // 			fstp	real8 ptr d_p_x
    FSTP64(&d_p_x->double_val);
    // 			mov		esi,dword ptr s_p
    esi.v = s_p->dword_low;
    // 			mov		edi,dword ptr d_p_x
    edi.v = d_p_x->dword_low;
    // 			fstp	real8 ptr s_p	;
    FSTP64(&s_p->double_val);
    // 			fstp	real8 ptr d_p_x	;
    FSTP64(&d_p_x->double_val);
    // 			mov		dword ptr s_p+4,esi
    s_p->dword_high = esi.v;
    // 			mov		dword ptr d_p_x+4,edi
    d_p_x->dword_high = edi.v;
    // if unsigned
    if(is_unsigned) {
        // 	; Remap from -1 to 1 signed to 0 to 1 unsigned
        // 	;
        // 			mov		esi,dword ptr s_p
        //mov(x86_op_reg(esi), x86_op_mem32(s_p));
        esi.v = s_p->dword_low;
        // 			xor		esi,080000000h
        esi.v ^= 0x080000000;
        // 			mov		dword ptr s_p,esi
        s_p->dword_low = esi.v;
        // endif
    }
}

// SETUP_FLAGS macro ; approx 21 cycles fixed, 45 cycles float
static void SETUP_FLAGS()
{
    // 	mov edx,workspace.v2
    edx.ptr_v = workspace.v2;
    // 	mov eax,workspace.v0
    eax.ptr_v = workspace.v0;
    // 	mov ecx,workspace.v1
    ecx.ptr_v = workspace.v1;
    // 	mov esi,2
    esi.v = 2;
    // if BASED_FIXED
    // 	mov ebx,dword ptr[edx+4*C_U]
    // 	mov ebp,dword ptr[eax+4*C_U]
    // 	mov edi,dword ptr[ecx+4*C_U]
    // else
    // 	fld dword ptr[edx+4*C_U]
    FLD(((brp_vertex *)edx.ptr_v)->comp_f[C_U]);
    // 	fadd fp_conv_d16
    FADD(fp_conv_d16);
    // 	fld dword ptr[eax+4*C_U]
    FLD(((brp_vertex *)eax.ptr_v)->comp_f[C_U]);
    // 	fadd fp_conv_d16
    FADD(fp_conv_d16);
    // 	fld dword ptr[ecx+4*C_U]
    FLD(((brp_vertex *)ecx.ptr_v)->comp_f[C_U]);
    // 	fadd fp_conv_d16
    FADD(fp_conv_d16);
    // 	fld dword ptr[edx+4*C_V]
    FLD(((brp_vertex *)edx.ptr_v)->comp_f[C_V]);
    // 	fadd fp_conv_d16
    FADD(fp_conv_d16);
    // 	fld dword ptr[eax+4*C_V]
    FLD(((brp_vertex *)eax.ptr_v)->comp_f[C_V]);
    // 	fadd fp_conv_d16
    FADD(fp_conv_d16);
    // 	fld dword ptr[ecx+4*C_V]
    FLD(((brp_vertex *)ecx.ptr_v)->comp_f[C_V]);
    // 	fadd fp_conv_d16
    FADD(fp_conv_d16);
    // 	 FXCH st(2)
    FXCH(2);
    // 	fstp qword ptr workspace.scratch0
    FSTP64(&workspace.scratch0_double);
    // 	fstp qword ptr workspace.scratch2
    FSTP64(&workspace.scratch2_double);
    // 	fstp qword ptr workspace.scratch4
    FSTP64(&workspace.scratch4_double);
    // 	fstp qword ptr workspace.scratch6
    FSTP64(&workspace.scratch6_double);
    // 	fstp qword ptr workspace.scratch8
    FSTP64(&workspace.scratch8_double);
    // 	fstp qword ptr workspace.scratch10
    FSTP64(&workspace.scratch10_double);
    // 	mov ebx,workspace.scratch6
    ebx.v = workspace.scratch6;
    // 	mov ebp,workspace.scratch8
    ebp.v = workspace.scratch8;
    // 	mov edi,workspace.scratch10
    edi.v = workspace.scratch10;
    // endif
    // 	and ebx,0ffff0000h
    //and(x86_op_reg(&ebx), x86_op_imm(0xffff0000));
    ebx.v &= 0xffff0000;
    // 	and ebp,0ffff0000h
    //and(x86_op_reg(&ebp), x86_op_imm(0xffff0000));
    ebp.v &= 0xffff0000;
    // 	and edi,0ffff0000h
    //and(x86_op_reg(&edi), x86_op_imm(0xffff0000));
    edi.v &= 0xffff0000;
    // 	cmp ebx,ebp
    // 	jne wrapped
    if(ebx.v != ebp.v) {
        goto wrapped;
    }
    // 	cmp ebx,edi
    // 	jne wrapped
    if(ebx.v != edi.v) {
        goto wrapped;
    }
    // if BASED_FIXED
    // 	mov ebx,dword ptr[edx+4*C_V]
    // 	mov ebp,dword ptr[eax+4*C_V]
    // 	mov edi,dword ptr[ecx+4*C_V]
    // else
    // 	mov ebx,workspace.scratch0
    ebx.v = workspace.scratch0;
    // 	mov ebp,workspace.scratch2
    ebp.v = workspace.scratch2;
    // 	mov edi,workspace.scratch4
    edi.v = workspace.scratch4;
    // endif
    ebx.v &= 0xffff0000;
    // 	and ebp,0ffff0000h
    ebp.v &= 0xffff0000;
    // 	and edi,0ffff0000h
    edi.v &= 0xffff0000;
    // 	cmp ebx,ebp
    // 	jne wrapped
    if(ebx.v != ebp.v) {
        goto wrapped;
    }
    // 	cmp ebx,edi
    // 	jne wrapped
    if(ebx.v != edi.v) {
        goto wrapped;
    }

    // 	mov esi,0
    esi.v = 0;
wrapped:
    // 	mov eax,workspace.flip
    eax.v = workspace.flip;
    // 	or eax,esi
    eax.v |= esi.v;
    // 	mov workspaceA.flags,eax
    workspaceA.flags = eax.v;
    // endm
}

static void REMOVE_INTEGER_PARTS_OF_PARAM(uint32_t *param)
{
    // local paramNegative
    // 	mov ebp,esi
    ebp.v = esi.v;
    // 	mov eax,param
    eax.v = *param;

    // 	test eax,80000000h
    // 	jnz paramNegative
    if((eax.v & 0x80000000) == 0) {
        // zf = 1;
    } else {
        // zf = 0;
        goto paramNegative;
    }

    // 	mov ebp,edi
    ebp.v = edi.v;

    // 	and eax,ebp
    eax.v &= ebp.v;
paramNegative:
    // 	and ebp,esi
    ebp.v &= esi.v;

    // 	or eax,ebp
    eax.v |= ebp.v;

    // 	mov param,eax
    *param = eax.v;
    // endm
}

static void REMOVE_INTEGER_PARTS_OF_PARAMETERS()
{
    // ; assumes 8.24 format
    // 	mov edi,0ffffffh
    edi.v = 0xffffff;
    // 	mov esi,0ff000000h
    esi.v = 0xff000000;
    // 	and workspace.s_u,0ffffffh
    workspace.s_u &= 0xffffff;
    // 	and workspace.s_v,0ffffffh
    workspace.s_v &= 0xffffff;

    REMOVE_INTEGER_PARTS_OF_PARAM(&workspace.d_u_x);
    REMOVE_INTEGER_PARTS_OF_PARAM(&workspace.d_u_y_0);
    REMOVE_INTEGER_PARTS_OF_PARAM(&workspace.d_u_y_1);

    REMOVE_INTEGER_PARTS_OF_PARAM(&workspace.d_v_x);
    REMOVE_INTEGER_PARTS_OF_PARAM(&workspace.d_v_y_0);
    REMOVE_INTEGER_PARTS_OF_PARAM(&workspace.d_v_y_1);
    // endm
}

// MULTIPLY_UP_PARAM_VALUES macro param,dimension,magic ;24 cycles
static void MULTIPLY_UP_PARAM_VALUES(int32_t s_p, int32_t d_p_x, int32_t d_p_y_0, int32_t d_p_y_1, fp64_t *a_sp, fp64_t *a_dpx,
                              fp64_t *a_dpy1, fp64_t *a_dpy0, uint32_t dimension, uint32_t magic)
{
    // ;										st(0)		st(1)		st(2)		st(3)		st(4)		st(5) st(6) st(7)
    assert(x86_state.x87_stack_top == -1);

    // fild work.texture.dimension;         d
    FILD(dimension);
    // 	fild workspace.s_&param			;	sp			d
    FILD(s_p);
    // 	fild workspace.d_&param&_x		;	dpdx		sp			d
    FILD(d_p_x);
    // 	fild workspace.d_&param&_y_0	;	dpdy0		dpdx		sp			d
    FILD(d_p_y_0);
    // 	FXCH st(2)						;	sp			dpdx		dpdy0		d
    FXCH(2);
    // 	fmul st,st(3)					;	spd			dpdx		dpdy0		d
    FMUL_ST(0, 3);
    // 	fild workspace.d_&param&_y_1	;	dpdy1		spd			dpdx		dpdy0		d
    FILD(d_p_y_1);
    // 	FXCH st(2)						;	dpdx		spd			dpdy1		dpdy0		d
    FXCH(2);
    // 	fmul st,st(4)					;	dpdxd		spd			dpdy1		dpdy0		d
    FMUL_ST(0, 4);
    // 	 FXCH st(1)						;	spd			dpdxd		dpdy1		dpdy0		d
    FXCH(1);
    // 	fadd magic						;	spdx		dpdxd		dpdy1		dpdy0		d
    FADD(magic);
    // 	 FXCH st(3)						;	dpdy0		dpdxd		dpdy1		spdx		d
    FXCH(3);
    // 	fmul st,st(4)					;	dpdy0d		dpdxd		dpdy1		spdx		d
    FMUL_ST(0, 4);
    // 	 FXCH st(1)						;	dpdxd		dpdy0d		dpdy1		spdx		d
    FXCH(1);
    // 	fadd magic						;	dpdxdx		dpdy0d		dpdy1		spdx		d
    FADD(magic);
    // 	 FXCH st(2)						;	dpdy1		dpdy0d		dpdxdx		spdx		d
    FXCH(2);
    // 	fmul st,st(4)					;	dpdy1d		dpdy0d		dpdxdx		spdx		d
    FMUL_ST(0, 4);
    // 	 FXCH st(4)						;	d			dpdy0d		dpdxdx		spdx		dpdy1d
    FXCH(4);
    // 	fstp st(0)						;	dpdy0d		dpdxdx		spdx		dpdy1d
    FSTP_ST(0);
    // 	fadd magic						;	dpdy0dx		dpdxdx		spdx		dpdy1d
    FADD(magic);
    // 	 FXCH st(3)						;	dpdy1d		dpdxdx		spdx		dpdy0dx
    FXCH(3);
    // 	fadd magic						;	dpdy1dx		dpdxdx		spdx		dpdy0dx
    FADD(magic);
    // 	 FXCH st(2)						;	spdx		dpdxdx		dpdy1dx		dpdy0dx
    FXCH(2);
    // 	fstp qword ptr workspaceA.s&param		;	dpdxdx		dpdy1dx		dpdy0dx
    FSTP64(&a_sp->double_val);
    // 	fstp qword ptr workspaceA.d&param&x	;	dpdy1dx		dpdy0dx
    FSTP64(&a_dpx->double_val);
    // 	fstp qword ptr workspaceA.d&param&y1	;	dpdy0dx
    FSTP64(&a_dpy1->double_val);
    // 	fstp qword ptr workspaceA.d&param&y0	;
    FSTP64(&a_dpy0->double_val);
    // endm
}

// SPLIT_INTO_INTEGER_AND_FRACTIONAL_PARTS macro ; 24 cycles
static void SPLIT_INTO_INTEGER_AND_FRACTIONAL_PARTS()
{
    // 	mov ebx,workspaceA.sv
    ebx.v = workspaceA.sv;
    // 	shl ebx,16
    ebx.v <<= 16;
    // 	mov edx,workspaceA.dvx
    edx.v = workspaceA.dvx;
    // 	shl edx,16
    edx.v <<= 16;
    // 	mov workspaceA.svf,ebx
    workspaceA.svf = ebx.v;
    // 	mov ebx,workspaceA.dvy0
    ebx.v = workspaceA.dvy0;
    // 	mov workspaceA.dvxf,edx
    workspaceA.dvxf = edx.v;
    // 	shl ebx,16
    ebx.v <<= 16;
    // 	mov edx,workspaceA.dvy1
    edx.v = workspaceA.dvy1;
    // 	shl edx,16
    edx.v <<= 16;
    // 	mov workspaceA.dvy0f,ebx
    workspaceA.dvy0f = ebx.v;
    // 	mov workspaceA.dvy1f,edx
    workspaceA.dvy1f = edx.v;

    // ;integer parts

    // 	mov ebx,workspaceA.sv
    ebx.v = workspaceA.sv;
    // 	sar ebx,16
    ebx.int_val >>= 16;
    // 	mov edx,workspaceA.dvx
    edx.v = workspaceA.dvx;

    // 	sar edx,16
    edx.int_val >>= 16;
    // 	mov workspaceA.sv,ebx
    workspaceA.sv = ebx.v;

    // 	mov ebx,workspaceA.dvy0
    ebx.v = workspaceA.dvy0;
    // 	mov workspaceA.dvx,edx
    workspaceA.dvx = edx.v;
    // 	sar ebx,16
    ebx.int_val >>= 16;
    // 	mov edx,workspaceA.dvy1
    edx.v = workspaceA.dvy1;
    // 	sar edx,16
    edx.int_val >>= 16;
    // 	mov workspaceA.dvy0,ebx
    workspaceA.dvy0 = ebx.v;
    // 	mov workspaceA.dvy1,edx
    workspaceA.dvy1 = edx.v;

    // endm
}

// MULTIPLY_UP_V_BY_STRIDE macro magic; 24 cycles
static void MULTIPLY_UP_V_BY_STRIDE(uint32_t magic)
{
    //  ;										st(0)		st(1)		st(2)		st(3)		st(4)		st(5) st(6) st(7)

    // 	fild work.texture.stride_b		;	d
    FILD(work.texture.stride_b);
    // 	fild workspaceA.sv					;	sp			d
    FILD(workspaceA.sv);
    // 	fild workspaceA.dvx					;	dpdx		sp			d
    FILD(workspaceA.dvx);
    // 	fild workspaceA.dvy0					;	dpdy0		dpdx		sp			d
    FILD(workspaceA.dvy0);
    // 	FXCH st(2)						;	sp			dpdx		dpdy0		d
    FXCH(2);
    // 	fmul st,st(3)					;	spd			dpdx		dpdy0		d
    FMUL_ST(0, 3);
    // 	fild workspaceA.dvy1					;	dpdy1		spd			dpdx		dpdy0		d
    FILD(workspaceA.dvy1);
    // 	FXCH st(2)						;	dpdx		spd			dpdy1		dpdy0		d
    FXCH(2);
    // 	fmul st,st(4)					;	dpdxd		spd			dpdy1		dpdy0		d
    FMUL_ST(0, 4);
    // 	 FXCH st(1)						;	spd			dpdxd		dpdy1		dpdy0		d
    FXCH(1);
    // 	fadd magic						;	spdx		dpdxd		dpdy1		dpdy0		d
    FADD(magic);
    // 	 FXCH st(3)						;	dpdy0		dpdxd		dpdy1		spdx		d
    FXCH(3);
    // 	fmul st,st(4)					;	dpdy0d		dpdxd		dpdy1		spdx		d
    FMUL_ST(0, 4);
    // 	 FXCH st(1)						;	dpdxd		dpdy0d		dpdy1		spdx		d
    FXCH(1);
    // 	fadd magic						;	dpdxdx		dpdy0d		dpdy1		spdx		d
    FADD(magic);
    // 	 FXCH st(2)						;	dpdy1		dpdy0d		dpdxdx		spdx		d
    FXCH(2);
    // 	fmul st,st(4)					;	dpdy1d		dpdy0d		dpdxdx		spdx		d
    FMUL_ST(0, 4);
    // 	 FXCH st(4)						;	d			dpdy0d		dpdxdx		spdx		dpdy1d
    FXCH(4);
    // 	fstp st(0)						;	dpdy0d		dpdxdx		spdx		dpdy1d
    FSTP_ST(0);
    // 	fadd magic						;	dpdy0dx		dpdxdx		spdx		dpdy1d
    FADD(magic);
    // 	 FXCH st(3)						;	dpdy1d		dpdxdx		spdx		dpdy0dx
    FXCH(3);
    // 	fadd magic						;	dpdy1dx		dpdxdx		spdx		dpdy0dx
    FADD(magic);
    // 	 FXCH st(2)						;	spdx		dpdxdx		dpdy1dx		dpdy0dx
    FXCH(2);
    // 	fstp qword ptr workspaceA.sv			;	dpdxdx		dpdy1dx		dpdy0dx
    FSTP64(&workspaceA.sv_double);
    // 	fstp qword ptr workspaceA.dvx			;	dpdy1dx		dpdy0dx
    FSTP64(&workspaceA.dvx_double);
    // 	fstp qword ptr workspaceA.dvy1		;	dpdy0dx
    FSTP64(&workspaceA.dvy1_double);
    // 	fstp qword ptr workspaceA.dvy0		;
    FSTP64(&workspaceA.dvy0_double);
    // endm
}

static void CREATE_CARRY_VERSIONS()
{
    // mov eax,workspaceA.dvy0
    eax.v = workspaceA.dvy0;
    // mov ebx,workspaceA.dvy1
    ebx.v = workspaceA.dvy1;
    // add eax,work.texture.stride_b
    eax.v += work.texture.stride_b;
    // mov ecx,workspaceA.dvx
    ecx.v = workspaceA.dvx;
    // add ebx,work.texture.stride_b
    ebx.v += work.texture.stride_b;
    // add ecx,work.texture.stride_b
    ecx.v += work.texture.stride_b;

    // mov workspaceA.dvy0c,eax
    workspaceA.dvy0c = eax.v;
    // mov workspaceA.dvy1c,ebx
    workspaceA.dvy1c = ebx.v;
    // mov workspaceA.dvxc,ecx
    workspaceA.dvxc = ecx.v;
}

static void WRAP_SETUP()
{
    // mov ecx,
    ecx.v = work.texture.width_p;
    // mov eax,work.texture._size
    eax.v = work.texture.size;

    // shl ecx,16
    ecx.v <<= 16;
    // add eax,work.texture.base
    eax.v += WORK_TEXTURE_BASE;
    // mov workspaceA.uUpperBound,ecx
    workspaceA.uUpperBound = ecx.int_val;

    // mov workspaceA.vUpperBound,eax
    workspaceA.vUpperBound = eax.v;
}

static void ARBITRARY_SETUP()
{
    // SETUP_FPU
    SETUP_FLAGS();
    REMOVE_INTEGER_PARTS_OF_PARAMETERS();

    // MULTIPLY_UP_PARAM_VALUES(u, width_p, fp_conv_d8r);
    MULTIPLY_UP_PARAM_VALUES(workspace.s_u, workspace.d_u_x, workspace.d_u_y_0, workspace.d_u_y_1, &workspaceA.su_double,
                             &workspaceA.dux_double, &workspaceA.duy1_double, &workspaceA.duy0_double, work.texture.width_p, fp_conv_d8r);

    // MULTIPLY_UP_PARAM_VALUES(v, height, fp_conv_d8r);
    MULTIPLY_UP_PARAM_VALUES(workspace.s_v, workspace.d_v_x, workspace.d_v_y_0, workspace.d_v_y_1, &workspaceA.sv_double,
                             &workspaceA.dvx_double, &workspaceA.dvy1_double, &workspaceA.dvy0_double, work.texture.height, fp_conv_d8r);

    SPLIT_INTO_INTEGER_AND_FRACTIONAL_PARTS();
    MULTIPLY_UP_V_BY_STRIDE(fp_conv_d);
    CREATE_CARRY_VERSIONS();
    WRAP_SETUP();
    // RESTORE_FPU
}

void SETUP_FLOAT_COLOUR(brp_vertex *v0) {
    // fld dword ptr [eax+4*C_I]
	// fadd fp_conv_d16
	// fstp qword ptr temporary_intensity
	// mov bl, byte ptr temporary_intensity+2
	// mov byte ptr workspace.colour,bl
    workspace.colour = (int)v0->comp_f[C_I];
}


void TriangleSetup_Z(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2) {
    SETUP_FLOAT_COLOUR(v0);
    if(SETUP_FLOAT(v0, v1, v2) != FPSETUP_SUCCESS) {
        return;
    }
    SETUP_FLOAT_PARAM(C_SZ,"_z",&workspace.s_z_double,&workspace.d_z_x_double,fp_conv_d16,1);
}

void TriangleSetup_ZI(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2) {
    if(SETUP_FLOAT(v0, v1, v2) != FPSETUP_SUCCESS) {
        return;
    }
    SETUP_FLOAT_PARAM(C_SZ,"_z",&workspace.s_z_double,&workspace.d_z_x_double,fp_conv_d16,1);
    SETUP_FLOAT_PARAM(C_I,"_i",&workspace.s_i_double,&workspace.d_i_x_double,fp_conv_d16, 0);
}

void TriangleSetup_ZTI(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2) {
    if(SETUP_FLOAT(v0, v1, v2) != FPSETUP_SUCCESS) {
        return;
    }
    SETUP_FLOAT_PARAM(C_SZ,"_z",&workspace.s_z_double,&workspace.d_z_x_double,fp_conv_d16,1);
    SETUP_FLOAT_PARAM(C_U,"_u",&workspace.s_u_double,&workspace.d_u_x_double,fp_conv_d16, 0);
    SETUP_FLOAT_PARAM(C_V,"_v",&workspace.s_v_double,&workspace.d_v_x_double,fp_conv_d16, 0);
    SETUP_FLOAT_PARAM(C_I,"_i",&workspace.s_i_double,&workspace.d_i_x_double,fp_conv_d16, 0);
}

void TriangleSetup_ZT(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2)
{
    if(SETUP_FLOAT(v0, v1, v2) != FPSETUP_SUCCESS) {
        return;
    }
    SETUP_FLOAT_PARAM(C_SZ,"_z",&workspace.s_z_double,&workspace.d_z_x_double,fp_conv_d16,1);
    SETUP_FLOAT_PARAM(C_U,"_u",&workspace.s_u_double,&workspace.d_u_x_double,fp_conv_d16, 0);
    SETUP_FLOAT_PARAM(C_V,"_v",&workspace.s_v_double,&workspace.d_v_x_double,fp_conv_d16, 0);
}

void TriangleSetup_ZT_ARBITRARY(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2)
{
    if(SETUP_FLOAT(v0, v1, v2) != FPSETUP_SUCCESS) {
        return;
    }

    SETUP_FLOAT_PARAM(C_SZ, "_z", &workspace.s_z_double, &workspace.d_z_x_double, fp_conv_d16, 1);
    SETUP_FLOAT_PARAM(C_U, "_u", &workspace.s_u_double, &workspace.d_u_x_double, fp_conv_d24, 0);
    SETUP_FLOAT_PARAM(C_V, "_v", &workspace.s_v_double, &workspace.d_v_x_double, fp_conv_d24, 0);
    ARBITRARY_SETUP();
}

void TriangleSetup_ZTI_ARBITRARY(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2)
{
    if(SETUP_FLOAT(v0, v1, v2) != FPSETUP_SUCCESS) {
        return;
    }

    SETUP_FLOAT_PARAM(C_SZ, "_z", &workspace.s_z_double, &workspace.d_z_x_double, fp_conv_d16, 1);
    SETUP_FLOAT_PARAM(C_U, "_u", &workspace.s_u_double, &workspace.d_u_x_double, fp_conv_d24, 0);
    SETUP_FLOAT_PARAM(C_V, "_v", &workspace.s_v_double, &workspace.d_v_x_double, fp_conv_d24, 0);
    SETUP_FLOAT_PARAM(C_I, "_i", &workspace.s_i_double, &workspace.d_i_x_double, fp_conv_d16, 0);
    ARBITRARY_SETUP();
}
