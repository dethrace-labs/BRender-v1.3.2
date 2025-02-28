#include <stdint.h>
#include "../softrend/ddi/priminfo.h"
#include "x86emu.h"
#include "fpsetup.h"
#include "fpwork.h"
#include <stdio.h>
#include <assert.h>
#include "work.h"

#define work_main_i				workspace.xm
#define work_main_d_i			workspace.d_xm
#define work_main_y				workspace.t_y

#define work_top_count			workspace.topCount
#define work_top_i				workspace.x1
#define work_top_d_i			workspace.d_x1

#define work_bot_count			workspace.bottomCount
#define work_bot_i				workspace.x2
#define work_bot_i_double		workspace.x2_double
#define work_bot_d_i			workspace.d_x2

#define work_pz_current			workspace.s_z
#define work_pz_grad_x			workspace.d_z_x
#define work_pz_d_nocarry		workspace.d_z_y_0
#define work_pz_d_carry			workspace.d_z_y_1

#define work_pu_current			workspace.s_u
#define work_pu_grad_x			workspace.d_u_x
#define work_pu_d_nocarry		workspace.d_u_y_0
#define work_pu_d_carry			workspace.d_u_y_1

#define work_pv_current			workspace.s_v
#define work_pv_grad_x			workspace.d_v_x
#define work_pv_d_nocarry		workspace.d_v_y_0
#define work_pv_d_carry			workspace.d_v_y_1

#define work_pi_current			workspace.s_i
#define work_pi_grad_x			workspace.d_i_x
#define work_pi_d_nocarry		workspace.d_i_y_0
#define work_pi_d_carry			workspace.d_i_y_1

#define workspace_flip			workspace.flip

#define workspace_iarea			workspace.iarea

#define workspace_dx1_a			workspace.dx1_a
#define workspace_dx2_a			workspace.dx2_a
#define workspace_dy1_a			workspace.dy1_a
#define workspace_dy2_a			workspace.dy2_a

#define workspace_m_y			m_y

#define workspace_t_dx			workspace.t_dx
#define workspace_t_dy			workspace.t_dy

#define workspace_xstep_0		workspace.xstep_0
#define workspace_xstep_1		workspace.xstep_1

enum {
    CHEAT_NO = 0,
    CHEAT_YES = 1
};

#define MASK_MANTISSA   0x007fffff
#define IMPLICIT_ONE    1 << 23
#define EXPONENT_SHIFT 23
#define EXPONENT_BIAS	127
#define EXPONENT_OFFSET ((127 + 23) << 23) | 0x07fffff

#define BRP_VERTEX(reg) ((brp_vertex *)reg.ptr_v)

// ; Three sorted vertex pointers
// ;


// Changed to array to guarantee variables are sequential in memory
// brp_vertex *top_vertex;
// brp_vertex *mid_vertex;
// brp_vertex *bot_vertex;
brp_vertex* top_mid_bot_vertices[3];

uint32_t xr_yr;
uint32_t wr_sr;
uint32_t wmin_4;
uint32_t u_base;
uint32_t v_base;
uint32_t q0;
uint32_t q1;
uint32_t q2;
uint32_t maxuv;

static int SETUP_FLOAT(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2)
{
	// local count_cont,exit,top_zero,bottom_zero,empty_triangle
    // assume eax: ptr brp_vertex, /*ebx: ptr brp_vertex,*/ ecx: ptr brp_vertex, edx: ptr brp_vertex

    assert(x86_state.x87_stack_top == -1);
    eax.ptr_v = v0;
    ecx.ptr_v = v1;
    edx.ptr_v = v2;

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

	workspace.v0 = eax.ptr_v;
    eax.float_val = ((brp_vertex *)eax.ptr_v)->comp_f[C_SY];
	workspace.v1 = ecx.ptr_v;
    ecx.float_val = ((brp_vertex *)ecx.ptr_v)->comp_f[C_SY];
    FSUBP_ST(1, 0); //	2area	dy1		dy2		dx1		dx2
    ebx.v ^= ebx.v;
    CMP(ecx.v, eax.v);
	workspace.v2 = edx.ptr_v;
    edx.float_val = ((brp_vertex *)edx.ptr_v)->comp_f[C_SY];
    FDIVR(fp_one); //	1/2area	dy1		dy2		dx1		dx2
	RCL_1(ebx.v);
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

	// mov top_vertex,eax
	top_mid_bot_vertices[0] = eax.ptr_v;

    // ;V
    // 			xor			esi,edi			; Build LR flag in bit 31
    esi.v ^= edi.v;
	// mov		mid_vertex,ebx
	top_mid_bot_vertices[1] = ebx.ptr_v;
    // ;V
    // 			shr			esi,31			; move down to bit 0
    esi.v >>= 31;
	// mov		bot_vertex,edx
	top_mid_bot_vertices[2] = edx.ptr_v;

	// mov		work.tsl.direction,esi
	work.tsl.direction = esi.int_val;

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
	//      fstp	real8 ptr work_bot_i		;
	FSTP64(&work_bot_i_double);

    // mov		work_main_d_i,edx
	work_main_d_i = edx.v;
	// mov		work_top_d_i,esi
	work_top_d_i = esi.v;
	// mov		work_bot_d_i,edi
	work_bot_d_i = edi.v;
	// mov		ecx,work_main_i
	ecx.v = work_main_i;
    // 		sar			ecx,16
    ecx.int_val >>= 16;
    // mov		edx,work_main_d_i
	edx.v = work_main_d_i;
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
    // fild	workspace_xstep_0			;	xstep_0	t_dx
	FILD(workspace.xstep_0);
	// fild	workspace_xstep_1			;	xstep_1 xstep_0	t_dx
	FILD(workspace.xstep_1);
	// fxch	st(2)						;	tdx		xstep_0	xstep_1
	FXCH(2);
	// fstp	workspace_t_dx				;	xstep_0	xstep_1
	FSTP(&workspace.t_dx);
	// mov		ecx,[workspace_v1] 			; Start preparing for parameter setup
	ecx.ptr_v = workspace.v1;
	// fstp	workspace_xstep_0			;	step_1
	FSTP(&workspace.xstep_0);
	// fstp	workspace_xstep_1			;
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

// ; Determine if this triangle will look sufficiently good with linear interpolation
// ;
// ; Total 40-44 cycles
// ;
// ;	eax: ptr to top vertex
// ;	ebx: ptr to vertex0
// ;	ecx: ptr to vertex1
// ;	edx: ptr to vertex2
// ;

int SETUP_FLOAT_CHECK_PERSPECTIVE_CHEAT() {

    assert(x86_state.x87_stack_top == -1);

	// ; Sort the vertices in SX order (relies on all positive values)
	// ;
	// ; 7 cycles
	// ;
	// mov		esi,top_vertex
	esi.ptr_v = top_mid_bot_vertices[0];
	// mov		edi,mid_vertex
	edi.ptr_v = top_mid_bot_vertices[1];

	// mov		ebp,bot_vertex
	ebp.ptr_v = top_mid_bot_vertices[2];
	// xor		ebx,ebx
	ebx.v = 0;

	// mov		eax,[esi].comp_f[C_SX*4]
	eax.float_val = BRP_VERTEX(esi)->comp_f[C_SX];
	// mov		ecx,[edi].comp_f[C_SX*4]
	ecx.float_val = BRP_VERTEX(edi)->comp_f[C_SX];

	// mov		edx,[ebp].comp_f[C_SX*4]
	edx.float_val = BRP_VERTEX(ebp)->comp_f[C_SX];
	// cmp		ecx,eax
	CMP(ecx.v, eax.v);

	// rcl		ebx,1
	RCL_1(ebx.v);
	// cmp		edx,eax
	CMP(edx.v, eax.v);

	// rcl		ebx,1
	RCL_1(ebx.v);
	// cmp		edx,ecx
	CMP(edx.v, ecx.v);

	// rcl		ebx,1			; ebx now has 3 bit number characterising the order of the vertices.
	RCL_1(ebx.v);
	// xor		ecx,ecx
	ecx.v = 0;

	// ; Determine the range of SX and SY values covered by the three vertices
	// ;
	// ; 6 cycles
	// 										;	0		1		2		3		4		5		6		7
	// fld		[ebp].comp_f[C_SY*4]		;	yb
	FLD(BRP_VERTEX(ebp)->comp_f[C_SY]);

	// mov		eax,sort_table_0[ebx*4]
	eax.v = sort_table_0[ebx.v];
	// mov		edx,sort_table_2[ebx*4]
	edx.v = sort_table_2[ebx.v];

	// fsub	[esi].comp_f[C_SY*4]		;	yrange
	FSUB(BRP_VERTEX(esi)->comp_f[C_SY]);

	// mov		eax,[top_vertex+eax]
	eax.ptr_v = top_mid_bot_vertices[eax.v];
	// mov		edx,[top_vertex+edx]
	edx.ptr_v = top_mid_bot_vertices[edx.v];

	// fld		[edx].comp_f[C_SX*4]		;	xr		yrange
	FLD(BRP_VERTEX(edx)->comp_f[C_SX]);
	// fsub	[eax].comp_f[C_SX*4]		;	xrange	yrange
	FSUB(BRP_VERTEX(eax)->comp_f[C_SX]);

	// ; Sort the vertices in W order (relies on all positive values)
	// ;
	// ; 10 cycles
	// ;
	// mov		eax,[esi].comp_f[C_W*4]
    eax.float_val = BRP_VERTEX(esi)->comp_f[C_W];
	// mov		ebx,[edi].comp_f[C_W*4]
    ebx.float_val = BRP_VERTEX(edi)->comp_f[C_W];

	// mov		edx,[ebp].comp_f[C_W*4]
    edx.float_val = BRP_VERTEX(ebp)->comp_f[C_W];
	// cmp		ebx,eax
    CMP(ebx.float_val, eax.float_val);

	// fld		st(0)						;	xrange	xrange	yrange
    FLD_ST(0);
	// fsub	st(0),st(2)					;	xr-yr	xrange	yrange
    FSUB_ST(0, 2);

	// rcl		ecx,1
    RCL_1(ecx.v);
	// cmp		edx,eax
    CMP(edx.float_val, eax.float_val);

	// rcl		ecx,1
    RCL_1(ecx.v);
	// cmp		edx,ebx
    CMP(edx.float_val, ebx.float_val);

	// fstp	xr_yr
    FSTP(&xr_yr);

	// rcl		ecx,1			; ebx now has 3 bit number characterising the order of the vertices.
    RCL_1(ecx.v);
	// mov		ebx,xr_yr
    ebx.v = xr_yr;

	// mov		eax,sort_table_0[ecx*4]
    eax.v = sort_table_0[ecx.v];
	// mov		edx,sort_table_2[ecx*4]
    edx.v = sort_table_2[ecx.v];

	// ; Select the larger of the SX and SY ranges by checking the sign of xrange-yrange
	// ;
	// ; 4-5 cycles + 0-3 cycles branch misprediction
	// ;
	// test	ebx,080000000h
	// je		xrange_larger
    if ((ebx.v & 0x80000000) == 0) {
        goto xrange_larger;
    }
	// fxch	st(1)						;	yrange	xrange
    FXCH(1);
xrange_larger:
	// fstp	st(1)						;	srange
    FSTP_ST(1);

	// mov		eax,[top_vertex+eax]
    eax.ptr_v = top_mid_bot_vertices[eax.v];
	// mov		edx,[top_vertex+edx]
    edx.ptr_v = top_mid_bot_vertices[edx.v];

	// ; Determine the minimum and range of W values covered by the three vertices
	// ;
	// ; 3 cycles
	// ;
	// fld		[edx].comp_f[C_W*4]			;	wmax	srange
    FLD(BRP_VERTEX(edx)->comp_f[C_W]);
	// fld		[eax].comp_f[C_W*4]			;	wmin	wmax	srange
    FLD(BRP_VERTEX(eax)->comp_f[C_W]);
	// fsub	st(1),st(0) 					;	wmin	wrange	srange
    FSUB_ST(1, 0);
	// fxch	st(2)  	 				    	;	srange	wrange	wmin
    FXCH(2);

	// ; Multiply the ranges together and the minimum W by the cutoff factor (4.0)
	// ;
	// ; 3 cycles
	// ;
    // fmul								;	wr*sr	wmin
    FMUL_0();
    // fxch	st(1)						;	wmin	wr*sr
    FXCH(1);
    // fmul	fp_four						;	wmin*4	wr*sr
    FMUL(fp_four);
    // fxch	st(1)						;	wr*sr	wmin*4
    FXCH(1);

    // mov		eax,top_vertex	; restore pointers
    eax.ptr_v = top_mid_bot_vertices[0];
    // mov		ebx,workspace_v0
    ebx.ptr_v = workspace.v0;

	// ; Store them out
	// ;
	// ; 5 cycles
	// ;
    // fstp	wr_sr
    FSTP(&wr_sr);
    // fstp	wmin_4
    FSTP(&wmin_4);

    // mov		ecx,wr_sr
    ecx.v = wr_sr;
    // mov		edx,wmin_4
    edx.v = wmin_4;

	// ; Cheat if w_range * srange < w_min * cutoff
	// ;
	// ; 2 cycles
	// ;
    // cmp		ecx,edx
    CMP(ecx.v, edx.v);
    int jb_flag = ecx.v < edx.v;
    // mov		ecx,workspace_v1
    ecx.ptr_v = workspace.v1;

    // mov		edx,workspace_v2
    edx.ptr_v = workspace.v2;
    // jb		cheat
    assert(x86_state.x87_stack_top == -1);
    if (jb_flag) {
        return CHEAT_YES;
    }
    return CHEAT_NO;
}

// ; Do the per-parameter calculations for perspective correct texture mapping
// ;
// ; Total 210-235 cycles
// ;
// ; Total setup is therefore:
// ;
// ;  Unlit  z-sorted   perspective: 423-453		cheat: 293-297		linear: 253 (172 more)
// ;  Smooth z-buffered perspective: 490-533		cheat: 373-377		linear: 333
// ;
// ; Proper perspective setup is 172 cycles more than for linear
// ; Cheat cases are 40 more than for linear
// ;
// ;	esi: ptr to top vertex
// ;	edi: ptr to middle vertex
// ;	ebp: ptr to bottom vertex
// ;
void SETUP_FLOAT_UV_PERSPECTIVE() {
    assert(x86_state.x87_stack_top == -1);
    // 		assume esi: ptr brp_vertex, edi: ptr brp_vertex, ebp: ptr brp_vertex

    // 	; N.B. in this code, u0, v0, w0 etc. refer to the top vertex, u1 etc. to the middle and
    // 	; u2 etc. to the bottom vertex.
    // 	;
    // 	; We use the sorted vertices because we need some intermediate results for the top vertex.
    // 	; The gradients must be recalculated, and the sign of the area calculation may have been
    // 	; changed by the sorting process.
    // 	;
    // 	; N.B. It may be possible to save a few cycles by doing the gradient calculations using the
    // 	; unsorted vertices and the calculations for the start values and some of the terms in the
    // 	; maxuv calculations using the top vertex, but this is awkward to arrange.
    // 	;
    // 	; 22-23 cycles + 0-3 cycles branch misprediction
    // 											;	0		1		2		3		4		5		6		7
    // fld		workspace_iarea				;	1/2area
    FLD(workspace_iarea);

    // cmp		workspace_flip,0
    // je		no_flip
    if (workspace_flip == 0) {
        goto no_flip;
    }

    // fchs								;	-1/2area
    FCHS();

no_flip:

    // fld	[edi].comp_f[C_SX*4]		;	x1		1/2area
    FLD(BRP_VERTEX(edi)->comp_f[C_SX]);
    // fsub	[esi].comp_f[C_SX*4]		;	dx1		1/2area
    FSUB(BRP_VERTEX(esi)->comp_f[C_SX]);
    // fld	[ebp].comp_f[C_SX*4]		;	x2		dx1		1/2area
    FLD(BRP_VERTEX(ebp)->comp_f[C_SX]);
    // fsub	[esi].comp_f[C_SX*4]		;	dx2		dx1		1/2area
    FSUB(BRP_VERTEX(esi)->comp_f[C_SX]);
    // fxch	st(1)						;	dx1		dx2		1/2area
    FXCH(1);
    // fmul	st(0),st(2)					;	dx1*a	dx2		1/2area
    FMUL_ST(0, 2);
    // fld	[edi].comp_f[C_SY*4]		;	y1		dx1*a	dx2		1/2area
    FLD(BRP_VERTEX(edi)->comp_f[C_SY]);
    // fxch	st(2)						;	dx2		dx1*a	y1		1/2area
    FXCH(2);
    // fmul	st(0),st(3)					;	dx2*a	dx1*a	y1		1/2area
    FMUL_ST(0, 3);
    // fxch	st(2)						;	y1		dx1*a	dx2*a	1/2area
    FXCH(2);
    // fsub	[esi].comp_f[C_SY*4]		;	dy1		dx1*a	dx2*a	1/2area
    FSUB(BRP_VERTEX(esi)->comp_f[C_SY]);
    // fld	[ebp].comp_f[C_SY*4]		;	y2		dy1		dx1*a	dx2*a	1/2area
    FLD(BRP_VERTEX(ebp)->comp_f[C_SY]);
    // fsub	[esi].comp_f[C_SY*4]		;	dy2		dy1		dx1*a	dx2*a	1/2area
    FSUB(BRP_VERTEX(esi)->comp_f[C_SY]);
    // fxch	st(1)						;	dy1		dy2		dx1*a	dx2*a	1/2area
    FXCH(1);
    // fmul	st(0),st(4)					;	dy1*a	dy2		dx1*a	dx2*a	1/2area
    FMUL_ST(0, 4);
    // fxch	st(2)						;	dx1*a	dy2		dy1*a	dx2*a	1/2area
    FXCH(2);
    // fstp	workspace_dx1_a				;	dy2		dy1*a	dx2*a	1/2area
    FSTP(&workspace_dx1_a);
    // fmulp st(3),st(0)				;	dy1*a	dx2*a	dy2*a
    FMULP_ST(3, 0);
    // fstp	workspace_dy1_a				;	dx2*a	dy2*a
    FSTP(&workspace_dy1_a);
    // fstp	workspace_dx2_a				;	dy2*a
    FSTP(&workspace_dx2_a);
    // fstp	workspace_dy2_a				;
    FSTP(&workspace_dy2_a);

    // ; Calculate integral base texture coords and write out as integers.
	// ;
	// ; Round by storing as integer and reading back
	// ;
	// ; 17 cycles and a latency of two before vb is available

    // fld		[esi].comp_f[C_U*4]			;	u0
    FLD(BRP_VERTEX(esi)->comp_f[C_U]);
    // fistp	work.awsl.u_current			;
    FISTP(&work.awsl.u_current);
    // fild	work.awsl.u_current			;	ub
    FILD(work.awsl.u_current);
    // fld		[esi].comp_f[C_V*4]			;	v0		ub
    FLD(BRP_VERTEX(esi)->comp_f[C_V]);
    // fistp	work.awsl.v_current			;	ub
    FISTP(&work.awsl.v_current);
    // fild	work.awsl.v_current			;	vb		ub
    FILD(work.awsl.v_current);
    // fxch	st(1)						;	ub		vb
    FXCH(1);
    // fld		[edi].comp_f[C_W*4]			;	w1		ub		vb
    FLD(BRP_VERTEX(edi)->comp_f[C_W]);

    // ; Calculate u' and v' (deltas from base coords) and q' (= q * product(w)) for vertex 0
	// ;
	// ; 5 cycles
                                     // ;	0		1		2		3		4		5		6		7
    // fmul	[ebp].comp_f[C_W*4]			;	q'0		ub		vb
    FMUL(BRP_VERTEX(ebp)->comp_f[C_W]);
    // fld		[esi].comp_f[C_U*4]		;	u0		q'0		ub		vb
    FLD(BRP_VERTEX(esi)->comp_f[C_U]);
    // fsub	st(0),st(2)					;	u'0		q'0		ub		vb
    FSUB_ST(0, 2);
    // fld		[esi].comp_f[C_V*4]		;	v0		u'0		q'0		ub		vb
    FLD(BRP_VERTEX(esi)->comp_f[C_V]);
    // fsub	st(0),st(4)					;	v'0		u'0		q'0		ub		vb
    FSUB_ST(0, 4);

    // ; Calculate |u'0| + |v'0| + |q'0|
	// ;
	// ; 10 cycles

    // fld		st(2)						;	q'0		v'0		u'0		q'0		ub		vb
    FLD_ST(2);
    // fst		q0							;	q'0		v'0		u'0		q'0		ub		vb
    FST(&q0);
    // fabs								;	|q'0|	v'0		u'0		q'0		ub		vb
    FABS();
    // fld		st(2)						;	u'0		|q'0|	v'0		u'0		q'0		ub		vb
    FLD_ST(2);
    // fabs								;	|u'0|	|q'0|	v'0		u'0		q'0		ub		vb
    FABS();
    // fadd								;	maxuv	v'0		u'0		q'0		ub		vb
    FADD_0();
    // fld		st(1)						;	v'0		maxuv	v'0		u'0		q'0		ub		vb
    FLD_ST(1);
    // fabs								;	|v'0|	maxuv	v'0		u'0		q'0		ub		vb
    FABS();
    // fadd
    FADD_0();

    // ; Multiply u'0 and v'0 by q'0
	// ;
	// ; 5 cycles

    // fxch	st(2)						;	u'0		v'0		maxuv	q'0		ub		vb
    FXCH(2);
    // fmul	st(0),st(3)					;	u'q'0	v'0		maxuv	q'0		ub		vb
    FMUL_ST(0, 3);
    // fld		[ebp].comp_f[C_W*4]			;	w2		u'q'0	v'0		maxuv	q'0		ub		vb
    FLD(BRP_VERTEX(ebp)->comp_f[C_W]);
    // fxch	st(4)						;	q'0		u'q'0	v'0		maxuv	w2		ub		vb
    FXCH(4);
    // fmulp	st(2),st(0)					;	u'q'0	v'q'0	maxuv	w2		ub		vb
    FMULP_ST(2, 0);
    // fxch	st(2)						;	maxuv	v'q'0	u'q'0	w2		ub		vb
    FXCH(2);
    // fstp	maxuv						;	v'q'0	u'q'0	w2		ub		vb
    FSTP(&maxuv);

    // ; Calculate u' and v' (deltas from base coords) and q' (= q * product(w)) for vertex 1
	// ;
	// ; 5 cycles
	// 										;	0		1		2		3		4		5		6		7
    // fld		[edi].comp_f[C_U*4]			;	u1		v'q'0	u'q'0	w2		ub		vb
    FLD(BRP_VERTEX(edi)->comp_f[C_U]);
    // fxch	st(3)						;	w2		v'q'0	u'q'0	u1		ub		vb
    FXCH(3);
    // fmul	[esi].comp_f[C_W*4]			;	q'1		v'q'0	u'q'0	u1		ub		vb
    FMUL(BRP_VERTEX(esi)->comp_f[C_W]);
    // fxch	st(3)						;	u1		v'q'0	u'q'0	q'1		ub		vb
    FXCH(3);
    // fsub	st(0),st(4)					;	u'1		v'q'0	u'q'0	q'1		ub		vb
    FSUB_ST(0, 4);
    // fld		[edi].comp_f[C_V*4]			;	v1		u'1		v'q'0	u'q'0	q'1		ub		vb
    FLD(BRP_VERTEX(edi)->comp_f[C_V]);
    // fsub	st(0),st(6)					;	v'1		u'1		v'q'0	u'q'0	q'1		ub		vb
    FSUB_ST(0, 6);

    // ; Multiply u'1 and v'1 by q'1
	// ;
	// ; 5 cycles

    // fxch	st(1)						;	u'1		v'1		v'q'0	u'q'0	q'1		ub		vb
    FXCH(1);
    // fmul	st(0),st(4)					;	u'q'1	v'1		v'q'0	u'q'0	q'1		ub		vb
    FMUL_ST(0,4);
    // fxch	st(4)						;	q'1		v'1		v'q'0	u'q'0	u'q'1	ub		vb
    FXCH(4);
    // fld		[esi].comp_f[C_W*4]			;	w0		q'1		v'1		v'q'0	u'q'0	u'q'1	ub		vb
    FLD(BRP_VERTEX(esi)->comp_f[C_W]);
    // fxch	st(1)						;	q'1		w0		v'1		v'q'0	u'q'0	u'q'1	ub		vb
    FXCH(1);
    // fmul	st(2),st(0)					;	q'1		w0		v'q'1	v'q'0	u'q'0	u'q'1	ub		vb
    FMUL_ST(2,0);
    // fstp	q1							;	w0		v'q'1	v'q'0	u'q'0	u'q'1	ub		vb
    FSTP(&q1);

    // ; Calculate u' and v' (deltas from base coords) and q' (= q * product(w)) for vertex 2
	// ;
	// ; 4 cycles
    // fmul	[edi].comp_f[C_W*4]			;	q'2		v'q'1	v'q'0	u'q'0	u'q'1	ub		vb
    FMUL(BRP_VERTEX(edi)->comp_f[C_W]);
    // fxch	st(5)						;	ub		v'q'1	v'q'0	u'q'0	u'q'1	q'2		vb
    FXCH(5);
    // fsubr	[ebp].comp_f[C_U*4]			;	u'2		v'q'1	v'q'0	u'q'0	u'q'1	q'2		vb
    FSUBR(BRP_VERTEX(ebp)->comp_f[C_U]);
    // fxch	st(6)						;	vb		v'q'1	v'q'0	u'q'0	u'q'1	q'2		u'2
    FXCH(6);
    // fsubr	[ebp].comp_f[C_V*4]			;	v'2		v'q'1	v'q'0	u'q'0	u'q'1	q'2		u'2
    FSUBR(BRP_VERTEX(ebp)->comp_f[C_V]);
    // fld		maxuv						;	maxuv	v'2		v'q'1	v'q'0	u'q'0	u'q'1	q'2		u'2
    FLD(maxuv);
    // fxch	st(6)						;	q'2		v'2		v'q'1	v'q'0	u'q'0	u'q'1	maxuv	u'2
    FXCH(6);

    // ; Multiply u'2 and v'2 by q'2
	// ;
	// ; 4 cycles
    // fmul	st(7),st(0)					;	q'2		v'2		v'q'1	v'q'0	u'q'0	u'q'1	maxuv	u'q'2
    FMUL_ST(7,0);
    // fst		q2							;	q'2		v'2		v'q'1	v'q'0	u'q'0	u'q'1	maxuv	u'q'2
    FST(&q2);
    // fmulp	st(1),st(0)					;	v'q'2	v'q'1	v'q'0	u'q'0	u'q'1	maxuv	u'q'2
    FMULP_ST(1,0);

    // ; Calculate |u'0| + |v'0| + |q'0| + sum(|u'q'| + |v'q'|) (vertex 0 is the top vertex)
	// ;
	// ; 18 cycles

    // fld		st(3)						;	u'q'0	v'q'2	v'q'1	v'q'0	u'q'0	u'q'1	maxuv	u'q'2
    FLD_ST(3);
    // fabs								;	|u'q'0|	v'q'2	v'q'1	v'q'0	u'q'0	u'q'1	maxuv	u'q'2
    FABS();
    // faddp	st(6),st(0)					;	v'q'2	v'q'1	v'q'0	u'q'0	u'q'1	maxuv	u'q'2
    FADDP_ST(6,0);
    // fld		st(2)						;	v'q'0	v'q'2	v'q'1	v'q'0	u'q'0	u'q'1	maxuv	u'q'2
    FLD_ST(2);
    // fabs								;	|v'q'0|	v'q'2	v'q'1	v'q'0	u'q'0	u'q'1	maxuv	u'q'2
    FABS();
    // faddp	st(6),st(0)					;	v'q'2	v'q'1	v'q'0	u'q'0	u'q'1	maxuv	u'q'2
    FADDP_ST(6,0);
    // fld		st(4)						;	u'q'1	v'q'2	v'q'1	v'q'0	u'q'0	u'q'1	maxuv	u'q'2
    FLD_ST(4);
    // fabs								;	|u'q'1|	v'q'2	v'q'1	v'q'0	u'q'0	u'q'1	maxuv	u'q'2
    FABS();
    // faddp	st(6),st(0)					;	v'q'2	v'q'1	v'q'0	u'q'0	u'q'1	maxuv	u'q'2
    FADDP_ST(6,0);
    // fld		st(1)						;	v'q'1	v'q'2	v'q'1	v'q'0	u'q'0	u'q'1	maxuv	u'q'2
    FLD_ST(1);
    // fabs								;	|v'q'1|	v'q'2	v'q'1	v'q'0	u'q'0	u'q'1	maxuv	u'q'2
    FABS();
    // faddp	st(6),st(0)					;	v'q'2	v'q'1	v'q'0	u'q'0	u'q'1	maxuv	u'q'2
    FADDP_ST(6,0);
    // fld		st(6)						;	u'q'2	v'q'2	v'q'1	v'q'0	u'q'0	u'q'1	maxuv	u'q'2
    FLD_ST(6);
    // fabs								;	|u'q'2|	v'q'2	v'q'1	v'q'0	u'q'0	u'q'1	maxuv	u'q'2
    FABS();
    // faddp	st(6),st(0)					;	v'q'2	v'q'1	v'q'0	u'q'0	u'q'1	maxuv	u'q'2
    FADDP_ST(6,0);
    // fld		st(0)						;	v'q'2	v'q'2	v'q'1	v'q'0	u'q'0	u'q'1	maxuv	u'q'2
    FLD_ST(0);
    // fabs								;	|v'q'2|	v'q'2	v'q'1	v'q'0	u'q'0	u'q'1	maxuv	u'q'2
    FABS();
    // faddp	st(6),st(0)					;	v'q'2	v'q'1	v'q'0	u'q'0	u'q'1	maxuv	u'q'2
    FADDP_ST(6,0);

    // ; Calculate deltas for u'q' and v'q'
	// ;
	// ; 6 cycles

    // fxch	st(2)						;	v'q'0	v'q'1	v'q'2	u'q'0	u'q'1	maxuv	u'q'2
    FXCH(2);
    // fsub	st(1),st(0)					;	v'q'0	dv1		v'q'2	u'q'0	u'q'1	maxuv	u'q'2
    FSUB_ST(1, 0);
    // fsub	st(2),st(0)					;	v'q'0	dv1		dv2		u'q'0	u'q'1	maxuv	u'q'2
    FSUB_ST(2, 0);
    // fxch	st(3)						;	u'q'0	dv1		dv2		v'q'0	u'q'1	maxuv	u'q'2
    FXCH(3);
    // fsub	st(4),st(0)					;	u'q'0	dv1		dv2		v'q'0	du1		maxuv	u'q'2
    FSUB_ST(4, 0);
    // fsub	st(6),st(0)					;	u'q'0	dv1		dv2		v'q'0	du1		maxuv	du2
    FSUB_ST(6, 0);
    // fxch	st(5)						;	maxuv	dv1		dv2		v'q'0	du1		u'q'0	du2
    FXCH(5);
    // fstp	maxuv						;	dv1		dv2		v'q'0	du1		u'q'0	du2
    FSTP(&maxuv);

    // ; Calculate normalisation factor to ensure that numerator and denominator
	// ; values do not overflow at any point in the rasterisation.
	// ;
	// ; Dan's code now calculates 2^28/maxuv and multiplies everything by this.
	// ; However, it should be sufficient to use 2^(28-ceil(log2(maxuv))).
	// ;
	// ; In fact, I think the largest value that will ever appear in the
	// ; calculations is bounded by:
	// ;
	// ; max(|q'0|,|q'1|,|q'2|) + max(|u'0|,|u'1|,|u'2|,|v'0|,|v'1|,|v'2|) * max(dq/dx,dq/dy_carry))
	// ;
	// ; 4-6 cycles + 0-3 cycles misprediction penalty
	// ;
    // mov		eax,maxuv
    eax.v = maxuv;
	// ; Round up to a power of two
	// ;
    // test	eax,MASK_MANTISSA
    // je		exact
    if ((eax.v & MASK_MANTISSA) == 0) {
        goto exact;
    }
    // and		eax,not MASK_MANTISSA
    eax.v &= ~MASK_MANTISSA;
    // add		eax,1 shl EXPONENT_SHIFT
    eax.v += (1 << EXPONENT_SHIFT);

exact:
    // ; Multiply reciprocal by 2^28
	// ;
	// neg		eax
    eax.v = -eax.v;
    eax.v += 0x8D000000;  // ((EXPONENT_BIAS * 2 + 28) << EXPONENT_SHIFT) calculation caused undefined behavior warning

    // ; Do parameter calculations for u and v
	// ;
	// ; pdx = dp1 * dy2_a - dp2 * dy1_a
	// ; pdy = dp2 * dx1_a - dp1 * dx2_a
	// ;
	// ; pdy_0 = pdy + xstep_0 * pdx
	// ; pstart = param_t + pdx * fdx + pdy * fdy
	// ;
	// ; Final results are multiplied by the normalisation factor.  d_carry is
	// ; not calculated as the outer loop adds grad_x separately.
	// ;
	// ; 64 cycles

    // fld		st(3)						;	du1		dv1		dv2		v'q'0	du1		u'q'0	du2
    FLD_ST(3);
    // fmul	workspace_dy2_a				;	du1*b	dv1		dv2		v'q'0	du1		u'q'0	du2
    FMUL(workspace_dy2_a);
    // fld		st(6)						;	du2		du1*b	dv1		dv2		v'q'0	du1		u'q'0	du2
    FLD_ST(6);
    // fmul	workspace_dy1_a				;	du2*a	du1*b	dv1		dv2		v'q'0	du1		u'q'0	du2
    FMUL(workspace_dy1_a);
    // fxch	st(5)						;	du1		du1*b	dv1		dv2		v'q'0	du2*a	u'q'0	du2
    FXCH(5);
    // mov		maxuv,eax
    maxuv = eax.v;
    // fmul	workspace_dx2_a				;	du1*d	du1*b	dv1		dv2		v'q'0	du2*a	u'q'0	du2
    FMUL(workspace_dx2_a);
    // fxch	st(1)						;	du1*b	du1*d	dv1		dv2		v'q'0	du2*a	u'q'0	du2
    FXCH(1);
    // fsubrp	st(5),st(0)					;	du1*d	dv1		dv2		v'q'0	udx		u'q'0	du2
    FSUBRP_ST(5, 0);
    // fxch	st(6)						;	du2		dv1		dv2		v'q'0	udx		u'q'0	du1*d
    FXCH(6);
    // fmul	workspace_dx1_a				;	du2*c	dv1		dv2		v'q'0	udx		u'q'0	du1*d
    FMUL(workspace_dx1_a);
    // fld		workspace_t_dx				;	fdx		du2*c	dv1		dv2		v'q'0	udx		u'q'0	du1*d
    FLD(workspace_t_dx);
    // fmul	st(0),st(5)					;	fdx*udx	du2*c	dv1		dv2		v'q'0	udx		u'q'0	du1*d
    FMUL_ST(0, 5);
    // fxch	st(7)						;	du1*d	du2*c	dv1		dv2		v'q'0	udx		u'q'0	fdx*udx
    FXCH(7);
    // fsubp	st(1),st(0)					;	udy		dv1		dv2		v'q'0	udx		u'q'0	fdx*udx
    FSUBP_ST(1, 0);
    // fld		workspace_t_dy				;	fdy		udy		dv1		dv2		v'q'0	udx		u'q'0	fdx*udx
    FLD(workspace_t_dy);
    // fxch	st(7)						;	fdx*udx	udy		dv1		dv2		v'q'0	udx		u'q'0	fdy
    FXCH(7);
    // faddp	st(6),st(0)					;	udy		dv1		dv2		v'q'0	udx		fux+ut	fdy
    FADDP_ST(6, 0);
    // fmul	st(6),st(0)					;	udy		dv1		dv2		v'q'0	udx		fux+ut	fdy*udy
    FMUL_ST(6, 0);
    // fld		st(4)						;	udx		udy		dv1		dv2		v'q'0	udx		fux+ut	fdy*udy
    FLD_ST(4);
    // fxch	st(7)						;	fdy*udy	udy		dv1		dv2		v'q'0	udx		fux+ut	udx
    FXCH(7);
    // faddp	st(6),st(0)					;	udy		dv1		dv2		v'q'0	udx		ustart	udx
    FADDP_ST(6, 0);
    // fxch	st(6)						;	udx		dv1		dv2		v'q'0	udx		ustart	udy
    FXCH(6);
    // fmul	workspace_xstep_0			;	udx*xs0	dv1		dv2		v'q'0	udx		ustart	udy
    FMUL(workspace_xstep_0);
    // fld		st(1)						;	dv1		udx*xs0	dv1		dv2		v'q'0	udx		ustart	udy
    FLD_ST(1);
    // fxch	st(6)						;	ustart	udx*xs0	dv1		dv2		v'q'0	udx		dv1		udy
    FXCH(6);
    // fmul	maxuv						;	ustart'	udx*xs0	dv1		dv2		v'q'0	udx		dv1		udy
    FMUL(maxuv);
    // fxch	st(1)						;	udx*xs0	ustart'	dv1		dv2		v'q'0	udx		dv1		udy
    FXCH(1);
    // faddp	st(7),st(0)					;	ustart'	dv1		dv2		v'q'0	udx		dv1		udy_0
    FADDP_ST(7, 0);
    // fxch	st(4)						;	udx		dv1		dv2		v'q'0	ustart'	dv1		udy_0
    FXCH(4);
    // fmul	maxuv						;	udx'	dv1		dv2		v'q'0	ustart'	dv1		udy_0
    FMUL(maxuv);
    // fxch	st(4)						;	ustart'	dv1		dv2		v'q'0	udx'	dv1		udy_0
    FXCH(4);
    // fadd	fp_conv_d					;	ustrt+C	dv1		dv2		v'q'0	udx'	dv1		udy_0
    FADD(fp_conv_d);
    // fxch	st(6)						;	udy_0	dv1		dv2		v'q'0	udx'	dv1		ustrt+C
    FXCH(6);
    // fmul	maxuv						;	udy_0'	dv1		dv2		v'q'0	udx'	dv1		ustrt+C
    FMUL(maxuv);
    // fxch	st(4)						;	udx'	dv1		dv2		v'q'0	udy_0'	dv1		ustrt+C
    FXCH(4);
    // fadd	fp_conv_d					;	udx+C	dv1		dv2		v'q'0	udy_0'	dv1		ustrt+C
    FADD(fp_conv_d);
    // fxch	st(6)						;	ustrt+C	dv1		dv2		v'q'0	udy_0'	dv1		udx+C
    FXCH(6);
    // fstp	real8 ptr work.pu.currentpix ;	dv1		dv2		v'q'0	udy_0'	dv1		udx+C
    FSTP64(&work.pu.currentpix_double);
    // fld		st(1)						;	dv2		dv1		dv2		v'q'0	udy_0'	dv1		udx+C
    FLD_ST(1);
    // fxch	st(4)						;	udy_0'	dv1		dv2		v'q'0	dv2		dv1		udx+C
    FXCH(4);
    // fadd	fp_conv_d					;	udy_0+C	dv1		dv2		v'q'0	dv2		dv1		udx+C
    FADD(fp_conv_d);
    // fxch	st(6)						;	udx+C	dv1		dv2		v'q'0	dv2		dv1		udy_0+C
    FXCH(6);
    // fstp	real8 ptr work.pu.grad_x	;	dv1		dv2		v'q'0	dv2		dv1		udy_0+C
    FSTP64(&work.pu.grad_x_double);
    // fmul	workspace_dy2_a				;	dv1*b	dv2		v'q'0	dv2		dv1		udy_0+C
    FMUL(workspace_dy2_a);
    // fxch	st(5)						;	udy_0+C	dv2		v'q'0	dv2		dv1		dv1*b
    FXCH(5);
    // fstp	real8 ptr work.pu.d_carry	;	dv2		v'q'0	dv2		dv1		dv1*b
    FSTP64(&work.pu.d_carry_double);
    // fmul	workspace_dy1_a				;	dv2*a	v'q'0	dv2		dv1		dv1*b
    FMUL(workspace_dy1_a);
    // fxch	st(3)						;	dv1		v'q'0	dv2		dv2*a	dv1*b
    FXCH(3);
    // mov		eax,work.pu.currentpix
    eax.v = work.pu.currentpix;
    // mov		ebx,work.pu.d_carry
    ebx.v = work.pu.d_carry;
    // fmul	workspace_dx2_a				;	dv1*d	v'q'0	dv2		dv2*a	dv1*b
    FMUL(workspace_dx2_a);
    // fxch	st(2)						;	dv2		v'q'0	dv1*d	dv2*a	dv1*b
    FXCH(2);
    // mov		work.pu.current,eax
    work.pu.current = eax.v;
    // mov		work.pu.d_nocarry,ebx
    work.pu.d_nocarry = ebx.v;
    // fmul	workspace_dx1_a				;	dv2*c	v'q'0	dv1*d	dv2*a	dv1*b
    FMUL(workspace_dx1_a);
    // fxch	st(4)						;	dv1*b	v'q'0	dv1*d	dv2*a	dv2*c
    FXCH(4);
    // fsubrp	st(3),st(0)					;	v'q'0	dv1*d	vdx		dv2*c
    FSUBRP_ST(3, 0);
    // fld		workspace_t_dx				;	fdx		v'q'0	dv1*d	vdx		dv2*c
    FLD(workspace_t_dx);
    // fxch	st(2)						;	dv1*d	v'q'0	fdx		vdx		dv2*c
    FXCH(2);
    // fsubp	st(4),st(0)					;	v'q'0	fdx		vdx		vdy
    FSUBP_ST(4, 0);
    // fxch	st(1)						;	fdx		v'q'0	vdx		vdy
    FXCH(1);
    // fmul	st(0),st(2)					;	fdx*vdx	v'q'0	vdx		vdy
    FMUL_ST(0, 2);
    // fld		workspace_t_dy				;	fdy		fdx*vdx	v'q'0	vdx		vdy
    FLD(workspace_t_dy);
    // fmul	st(0),st(4)					;	fdy*vdy	fdx*vdx	v'q'0	vdx		vdy
    FMUL_ST(0, 4);
    // fxch	st(1)						;	fdx*vdx	fdy*vdy	v'q'0	vdx		vdy
    FXCH(1);
    // faddp	st(2),st(0)					;	fdy*vdy	fvx+ft	vdx		vdy
    FADDP_ST(2, 0);
    // fld		st(2)						;	vdx		fdy*vdy	fvx+ft	vdx		vdy
    FLD_ST(2);
    // fxch	st(1)						;	fdy*vdy	vdx		fvx+ft	vdx		vdy
    FXCH(1);
    // faddp	st(2),st(0)					;	vdx		vstart	vdx		vdy
    FADDP_ST(2, 0);
    // fmul	workspace_xstep_0			;	vdx*xs0	vstart	vdx		vdy
    FMUL(workspace_xstep_0);
    // fxch	st(2)						;	vdx		vstart	vdx*xs0	vdy
    FXCH(2);
    // ;1 cycle
    // fmul	maxuv						;	vdx'	vstart	vdx*xs0	vdy
    float f = *(float *)&maxuv;
    FMUL(maxuv);
    // fxch	st(2)						;	vdx*xs0	vstart	vdx'	vdy
    FXCH(2);
    // faddp	st(3),st(0)					;	vstart	vdx'	vdy_0
    FADDP_ST(3, 0);
    // fmul	maxuv						;	vstart'	vdx'	vdy_0
    FMUL(maxuv);
    // fxch	st(1)						;	vdx'	vstart'	vdy_0
    FXCH(1);
    // fadd	fp_conv_d					;	vdx+C	vstart'	vdy_0
    FADD(fp_conv_d);
    // fxch	st(2)						;	vdy_0	vstart'	vdx+C
    FXCH(2);
    // fmul	maxuv						;	vdy_0'	vstart'	vdx+C
    FMUL(maxuv);
    // fxch	st(1)						;	vstart'	vdy_0'	vdx+C
    FXCH(1);
    // fadd	fp_conv_d					;	vstrt+C	vdy_0'	vdx+C
    FADD(fp_conv_d);
    // fxch	st(1)						;	vdy_0'	vstrt+C	vdx+C
    FXCH(1);
    // fadd	fp_conv_d					;	vdy_0+C	vstrt+C	vdx+C
    FADD(fp_conv_d);
    // fxch	st(2)						;	vdx+C	vstrt+C	vdy_0+C
    FXCH(2);
    // fstp	real8 ptr work.pv.grad_x	;	vstrt+C	vdy_0+C
    FSTP64(&work.pv.grad_x_double);
    // fstp	real8 ptr work.pv.currentpix ;	vdy_0+C
    FSTP64(&work.pv.currentpix_double);
    // fstp	real8 ptr work.pv.d_carry	;
    FSTP64(&work.pv.d_carry_double);

    // mov		eax,work.pv.currentpix
    eax.v = work.pv.currentpix;
    // mov		ebx,work.pv.d_carry
    ebx.v = work.pv.d_carry;
    // mov		work.pv.current,eax
    work.pv.current = eax.v;
    // mov		work.pv.d_nocarry,ebx
    work.pv.d_nocarry = ebx.v;

    // ; Do parameter calculations for q'
	// ;
	// ; pdx = dp1 * dy2_a - dp2 * dy1_a
	// ; pdy = dp2 * dx1_a - dp1 * dx2_a
	// ;
	// ; pdy_0 = pdy + xstep_0 * pdx
	// ; pdy_1 = pdy + xstep_1 * pdx
	// ; pstart = param_t + pdx * fdx + pdy * fdy
	// ;
	// ; Final results are multiplied by the normalisation factor.
	// ;
	// ; 44 cycles
    // fld		q2							;	q2
    FLD(q2);
    // fsub	q0							;	dq2
    FSUB(q0);
    // fld		q1							;	q1		dq2
    FLD(q1);
    // fsub	q0							;	dq1		dq2
    FSUB(q0);
    // fld		st(1)						;	dq2		dq1		dq2
    FLD_ST(1);
    // fmul	workspace_dy1_a				;	dq2*a	dq1		dq2
    FMUL(workspace_dy1_a);
    // fld		st(1)						;	dq1		dq2*a	dq1		dq2
    FLD_ST(1);
    // fmul	workspace_dy2_a				;	dq1*b	dq2*a	dq1		dq2
    FMUL(workspace_dy2_a);
    // fld		workspace_t_dx				;	fdx		dq1*b	dq2*a	dq1		dq2
    FLD(workspace_t_dx);
    // fxch	st(4)						;	dq2		dq1*b	dq2*a	dq1		fdx
    FXCH(4);
    // fmul	workspace_dx1_a				;	dq2*c	dq1*b	dq2*a	dq1		fdx
    FMUL(workspace_dx1_a);
    // fld		workspace_t_dy				;	fdy		dq2*c	dq1*b	dq2*a	dq1		fdx
    FLD(workspace_t_dy);
    // fxch	st(4)						;	dq1		dq2*c	dq1*b	dq2*a	fdy		fdx
    FXCH(4);
    // fmul	workspace_dx2_a				;	dq1*d	dq2*c	dq1*b	dq2*a	fdy		fdx
    FMUL(workspace_dx2_a);
    // fxch	st(3)						;	dq2*a	dq2*c	dq1*b	dq1*d	fdy		fdx
    FXCH(3);
    // fsubp	st(2),st					;	dq2*c	d1b-d2a	dq1*d	fdy		fdx
    FSUBP_ST(2, 0);
    // fld		q0							;	q'0		dq2*c	d1b-d2a	dq1*d	fdy		fdx
    FLD(q0);
    // fxch	st(3)						;	dq1*d	dq2*c	d1b-d2a	q'0		fdy		fdx
    FXCH(3);
    // fsubp	st(1),st					;	d2c-d1d	d1b-d2a	q'0		fdy		fdx
    FSUBP_ST(1, 0);
    //                                      ;	qdy		qdx		q'0		fdy		fdx
    // fld		st(1)						;	qdx		qdy		qdx		q'0		fdy		fdx
    FLD_ST(1);
    // fmul	workspace_xstep_0			;	qdx*xs0	qdy		qdx		q'0		fdy		fdx
    FMUL(workspace_xstep_0);
    // fld		st(2)						;	qdx		qdx*xs0	qdy		qdx		q'0		fdy		fdx
    FLD_ST(2);
    // fmul	workspace_xstep_1			;	qdx*xs1	qdx*xs0	qdy		qdx		q'0		fdy		fdx
    FMUL(workspace_xstep_1);
    // fxch	st(1)						;	qdx*xs0	qdx*xs1	qdy		qdx		q'0		fdy		fdx
    FXCH(1);
    // fadd	st,st(2)					;	qdy_0	qdx*xs1	qdy		qdx		q'0		fdy		fdx
    FADD_ST(0, 2);
    // fxch	st(3)						;	qdx		qdx*xs1	qdy		qdy_0	q'0		fdy		fdx
    FXCH(3);
    // fmul	st(6),st					;	qdx		qdx*xs1	qdy		qdy_0	q'0		fdy		fdx*qdx
    FMUL_ST(6, 0);
    // fxch	st(2)						;	qdy		qdx*xs1	qdx		qdy_0	q'0		fdy		fdx*qdx
    FXCH(2);
    // fadd	st(1),st					;	qdy		qdy_1	qdx		qdy_0	q'0		fdy		fdx*qdx
    FADD_ST(1, 0);
    // fmulp	st(5),st					;	qdy_1	qdx		qdy_0	q'0		fdy*qdy	fdx*qdx
    FMULP_ST(5, 0);
    // fxch	st(3)						;	q'0		qdx		qdy_0	qdy_1	fdy*qdy	fdx*qdx
    FXCH(3);
    // faddp	st(5),st					;	qdx		qdy_0	qdy_1	fdy*qdy	fpx+qt
    FADDP_ST(5, 0);
    // fxch	st(1)						;	qdy_0	qdx		qdy_1	fdy*qdy	fpx+qt
    FXCH(1);
    // fmul	maxuv						;	qdy_0'	qdx		qdy_1	fdy*qdy	fpx+qt
    FMUL(maxuv);
    // fxch	st(3)						;	fdy*qdy	qdx		qdy_1	qdy_0'	fpx+qt
    FXCH(3);
    // fxch	st(2)						;	qdy_1	qdx		fdy*qdy	qdy_0'	fpx+qt
    FXCH(2);
    // fmul	maxuv						;	qdy_1'	qdx		fdy*qdy	qdy_0'	fpx+qt
    FMUL(maxuv);
    // fxch	st(4)						;	fpx+qt	qdx		fdy*qdy	qdy_0'	qdy_1'
    FXCH(4);
    // faddp	st(2),st					;	qdx		qstart	qdy_0'	qdy_1'
    FADDP_ST(2, 0);
    // fmul	maxuv						;	qdx'	qstart	qdy_0'	qdy_1'
    FMUL(maxuv);
    // fxch	st(2)						;	qdy_0'	qstart	qdx'	qdy_1'
    FXCH(2);
    // fadd	fp_conv_d					;	C+qdy_0	qstart	qdx'	qdy_1'
    FADD(fp_conv_d);
    // fxch	st(1)						;	qstart	C+qdy_0	qdx'	qdy_1'
    FXCH(1);
    // fmul	maxuv						;	qstart'	C+qdy_0	qdx'	qdy_1'
    FMUL(maxuv);
    // fxch	st(3)						;	qdy_1'	C+qdy_0	qdx'	qstart'
    FXCH(3);
    // fadd	fp_conv_d					;	C+qdy_1	C+qdy_0	qdx'	qstart'
    FADD(fp_conv_d);
    // fxch	st(2)						;	qdx'	C+qdy_0	C+qdy_1	qstart'
    FXCH(2);
    // fadd	fp_conv_d					;	C+qdx	C+qdy_0	C+qdy_1	qstart'
    FADD(fp_conv_d);
    // fxch	st(3)						;	qstart'	C+qdy_0	C+qdy_1	C+qdx
    FXCH(3);
    // fadd	fp_conv_d					;	C+qstrt	C+qdy_0	C+qdy_1	C+qdx
    FADD(fp_conv_d);
    // fxch	st(2)						;	C+qdy_1	C+qdy_0	C+qstrt	C+qdx
    FXCH(2);
    // fstp	real8 ptr work.pq.d_carry	;	C+qdy_0	C+qstrt	C+qdx
    FSTP64(&work.pq.d_carry_double);
    // fstp	real8 ptr work.pq.grad_x	;	C+qstrt	C+qdx
    FSTP64(&work.pq.grad_x_double);
    // fstp	real8 ptr work.pq.currentpix ;	C+qdx
    FSTP64(&work.pq.currentpix_double);
    // mov		eax,work.pq.grad_x
    eax.v = work.pq.grad_x;
    // mov		ebx,work.pq.currentpix
    ebx.v = work.pq.currentpix;
    // fstp	real8 ptr work.pq.grad_x	;
    FSTP64(&work.pq.grad_x_double);
    // mov		work.pq.d_nocarry,eax
    work.pq.d_nocarry = eax.v;
    // mov		work.pq.current,ebx
    work.pq.current = ebx.v;
}

void TriangleSetup_ZPT(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2) {
	if (SETUP_FLOAT(v0, v1, v2) != FPSETUP_SUCCESS) {
        return;
    }
	//SETUP_FLOAT_PARAM C_SZ,pz,fp_conv_d16,1
	SETUP_FLOAT_PARAM(C_SZ,"_z",(fp64_t *)&workspace.s_z_double,(fp64_t *)&workspace.d_z_x_double,fp_conv_d16,1);
	if (SETUP_FLOAT_CHECK_PERSPECTIVE_CHEAT() == CHEAT_YES) {
		// mov	esi,work.tsl.direction
		// mov	workspace.flip,esi
		workspace.flip = work.tsl.direction;

		// SETUP_FLOAT_PARAM C_U,pu,fp_conv_d16
		// SETUP_FLOAT_PARAM C_V,pv,fp_conv_d16
		SETUP_FLOAT_PARAM(C_U,"_u",(fp64_t *)&workspace.s_u_double,(fp64_t *)&workspace.d_u_x_double,fp_conv_d16, 0);
		SETUP_FLOAT_PARAM(C_V,"_v",(fp64_t *)&workspace.s_v_double,(fp64_t *)&workspace.d_v_x_double,fp_conv_d16, 0);
		//stc
		x86_state.cf = 1;
	} else {
		SETUP_FLOAT_UV_PERSPECTIVE();
		//clc
		x86_state.cf = 0;
	}
}

void TriangleSetup_ZPT_NOCHEAT(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2) {
	if (SETUP_FLOAT(v0, v1, v2) != FPSETUP_SUCCESS) {
        return;
    }
	//SETUP_FLOAT_PARAM C_SZ,pz,fp_conv_d16,1
	SETUP_FLOAT_PARAM(C_SZ,"_z",(fp64_t *)&workspace.s_z_double,(fp64_t *)&workspace.d_z_x_double,fp_conv_d16,1);
    // mov		esi,top_vertex
    esi.ptr_v = top_mid_bot_vertices[0];
    // mov		edi,mid_vertex
    edi.ptr_v = top_mid_bot_vertices[1];
    // mov		ebp,bot_vertex
    ebp.ptr_v = top_mid_bot_vertices[2];
    SETUP_FLOAT_UV_PERSPECTIVE();
}

void TriangleSetup_ZPTI(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2) {
	if (SETUP_FLOAT(v0, v1, v2) != FPSETUP_SUCCESS) {
        return;
    }
	//SETUP_FLOAT_PARAM C_SZ,pz,fp_conv_d16,1
	SETUP_FLOAT_PARAM(C_SZ,"_z",(fp64_t *)&workspace.s_z_double,(fp64_t *)&workspace.d_z_x_double,fp_conv_d16,1);
    // SETUP_FLOAT_PARAM C_I,pi,fp_conv_d16
    SETUP_FLOAT_PARAM(C_I,"_i",(fp64_t *)&workspace.s_i_double,(fp64_t *)&workspace.d_i_x_double,fp_conv_d16, 0);
	if (SETUP_FLOAT_CHECK_PERSPECTIVE_CHEAT() == CHEAT_YES) {
		// mov	esi,work.tsl.direction
		// mov	workspace.flip,esi
		workspace.flip = work.tsl.direction;

		// SETUP_FLOAT_PARAM C_U,pu,fp_conv_d16
		// SETUP_FLOAT_PARAM C_V,pv,fp_conv_d16
		SETUP_FLOAT_PARAM(C_U,"_u",(fp64_t *)&workspace.s_u_double,(fp64_t *)&workspace.d_u_x_double,fp_conv_d16, 0);
		SETUP_FLOAT_PARAM(C_V,"_v",(fp64_t *)&workspace.s_v_double,(fp64_t *)&workspace.d_v_x_double,fp_conv_d16, 0);
		//stc
		x86_state.cf = 1;
	} else {
		SETUP_FLOAT_UV_PERSPECTIVE();
		//clc
		x86_state.cf = 0;
	}
}

void TriangleSetup_ZPTI_NOCHEAT(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2) {
	if (SETUP_FLOAT(v0, v1, v2) != FPSETUP_SUCCESS) {
        return;
    }
	//SETUP_FLOAT_PARAM C_SZ,pz,fp_conv_d16,1
	SETUP_FLOAT_PARAM(C_SZ,"_z",(fp64_t *)&workspace.s_z_double,(fp64_t *)&workspace.d_z_x_double,fp_conv_d16,1);
    // SETUP_FLOAT_PARAM C_I,pi,fp_conv_d16
    SETUP_FLOAT_PARAM(C_I,"_i",(fp64_t *)&workspace.s_i_double,(fp64_t *)&workspace.d_i_x_double,fp_conv_d16, 0);
    // mov		esi,top_vertex
    esi.ptr_v = top_mid_bot_vertices[0];
    // mov		edi,mid_vertex
    edi.ptr_v = top_mid_bot_vertices[1];
    // mov		ebp,bot_vertex
    ebp.ptr_v = top_mid_bot_vertices[2];
    SETUP_FLOAT_UV_PERSPECTIVE();
}
