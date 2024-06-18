#include "brender.h"
#include "priminfo.h"
#include "pfpsetup.h"
#include "work.h"
#include "x86emu.h"
#include "common.h"
#include "fpwork.h"
#include <stdio.h>

#define work_main_i				workspace.xm
#define work_main_d_i			workspace.d_xm
#define work_main_y				workspace.t_y

#define work_top_count			workspace.topCount
#define work_top_i				workspace.x1
#define work_top_d_i			workspace.d_x1

#define work_bot_count			workspace.bottomCount
#define work_bot_i				workspace.x2
#define work_bot_d_i			workspace.d_x2

#define work_pz_current			workspace.s_z
#define work_pz_grad_x			workspace.d_z_x
#define work_pz_d_nocarry		workspace.d_z_y_0
#define work_pz_d_carry			workspace.d_z_y_1

#define work_pi_current			workspace.s_i
#define work_pi_grad_x			workspace.d_i_x
#define work_pi_d_nocarry		workspace.d_i_y_0
#define work_pi_d_carry			workspace.d_i_y_1

// The version of BRender shipped with Carmageddon disabled perspective cheating for blended triangles
// BRender v1.3.2 has perspective cheating enabled except for fogged triangles
#define ENABLE_PERSPECTIVE_CHEAT_FOR_BLEND 0

// non-perspective if following cheat mode
void BR_ASM_CALL TriangleRender_ZT_I8_D16_POW2(brp_block *block, int pow2, int skip_setup, va_list va);
void BR_ASM_CALL TriangleRender_ZTB_I8_D16_POW2(brp_block *block, int pow2, int skip_setup, va_list va);
void BR_ASM_CALL TriangleRender_ZTI_I8_D16_POW2(brp_block *block, int pow2, int skip_setup, va_list va);

typedef struct trapezium_render_size_params {
    int pre;
    int incu;
    int decu;
    int incv;
    int decv;
    int post1;
    int post2;
} trapezium_render_size_params;

enum tScan_direction {
    eScan_direction_i,
    eScan_direction_d,
    eScan_direction_b,
};

enum tTrapezium_render_size {
    eTrapezium_render_size_64,
    eTrapezium_render_size_256,
};


// 	<>,\
// 	<inc al>,<dec al>,<inc ah>,<dec ah>,\
// 	<>,<>
// TrapeziumRender_ZPT_I8_D16 256,b,\
// 	<>,\
// 	<inc al>,<dec al>,<inc ah>,<dec ah>,\
// 	<>,<>

trapezium_render_size_params params[] = {
    // 64x64
    { .pre = 2, .incu = 4, .decu = 4, .incv = 1, .decv = 1, .post1 = 2, .post2 = 0x0fff },
    // 256x256
    { .pre = 0, .incu = 1, .decu = 1, .incv = 1, .decv = 1, .post1 = 0, .post2 = 0xffffffff },
};

static inline void ScanlineRender_ZPT_I8_D16(int size, int dirn, int udirn, int vdirn, tFog_enabled fogging, tBlend_enabled blend) {
    // ; Make temporary copies of parameters that change
	// ;

    // mov		edx,work.pu.current
    edx.v = work.pu.current;
    // mov		esi,work.pu.grad_x
    esi.v = work.pu.grad_x;

    if (udirn == eScan_direction_i) {
        // mov		ebx,work.pq.current
        ebx.v = work.pq.current;
        // mov		ecx,work.pq.grad_x
        ecx.v = work.pq.grad_x;
        // sub		edx,ebx				; Move error into the range -1..0
        edx.v -= ebx.v;
        // sub		esi,ecx
        esi.v -= ecx.v;
    }

     // mov		ebp,work.pv.current
    ebp.v = work.pv.current;
    // mov		edi,work.pv.grad_x
    edi.v = work.pv.grad_x;

    // ifidni <vdirn>,<i>
    if (vdirn == eScan_direction_i) {
        // ifdifi <udirn>,<i>
        if (udirn != eScan_direction_i) {
            // mov		ebx,work.pq.current
            ebx.v = work.pq.current;
            // mov		ecx,work.pq.grad_x
            ecx.v = work.pq.grad_x;
        }

        // sub		ebp,ebx				; Move error into the range -1..0
        ebp.v -= ebx.v;
        // sub		edi,ecx
        edi.v -= ecx.v;

    }

    // mov		work.tsl.u_numerator, edx
    work.tsl.u_numerator = edx.v;
    // mov		work.tsl.du_numerator, esi
    work.tsl.du_numerator = esi.v;
    // mov		work.tsl.v_numerator, ebp
    work.tsl.v_numerator = ebp.v;
    // mov		work.tsl.dv_numerator, edi
    work.tsl.dv_numerator = edi.v;
    // mov		esi,work.texture.base
    esi.ptr_v = work.texture.base;
    // mov		eax,work.tsl.source
    eax.v = work.tsl.source;
    // mov		edi,work.tsl.start
    edi.ptr_v = work.tsl.start;
    // mov		ebp,work.tsl.zstart
    ebp.ptr_v = work.tsl.zstart;
    // mov		ebx,work_pz_current
    ebx.v = work_pz_current;
    // mov		edx,work.pq.current
    edx.v = work.pq.current;
    // ror		ebx,16					; Swap z words
    ROR16(ebx);
    // mov		work.tsl.denominator,edx
    work.tsl.denominator = edx.v;
    // mov		work.tsl.z,ebx
    work.tsl.z = ebx.v;
    // mov		work.tsl.dest,edi
    work.tsl.dest = edi.ptr_v;

next_pixel:
	// ; Texel fetch and store section
	// ;
	// ; eax = source offset
	// ; ebx = new z value
	// ; ecx = texel
	// ; edx = old z value, shade table
	// ; esi = texture base
	// ; edi = dest
	// ; ebp = zdest

	// ; Perform z buffer test and get texel
	// ;

    // mov		dl,[ebp]
    edx.short_low = *ebp.ptr_16;
    // mov		cl,[eax+esi]
    ecx.l = esi.ptr_8[eax.v];

    // mov		dh,[ebp+1]
    // no-op - already read both depth bytes
    // mov		edi,work.tsl.dest
    edi.ptr_8 = (uint8_t*)work.tsl.dest;

    // cmp		bx,dx
    // ja		nodraw
    if (ebx.short_low > edx.short_low) {
        goto nodraw;
    }

	// ; Test for transparency
	// ;
    // test	cl,cl
    // jz		nodraw
    if (ecx.l == 0) {
        goto nodraw;
    }

    // ifidni <fogging>,<F>
    if (fogging == eFog_yes) {
        // ifidni <blend>,<B>
        if (blend == eBlend_yes) {
            // ; Look texel up in fog table + blend table, store texel
            // not implemented
            BrAbort();
        } else {
            // ; Look texel up in fog table, store texel and z
            // not implemented
            BrAbort();
        }
    } else if (blend == eBlend_yes) {
        // ; Look texel up in blend table, store texel
        // ;
        // and     ecx,0ffh
        ecx.v &= 0xff;
        // mov     edx,work.blend_table
        edx.ptr_8 = work.blend_table;
        // mov     ch,[edi]
        ecx.h = *edi.ptr_8;
        // ;AGI stall
        // mov     cl,[edx+ecx]
        ecx.l = edx.ptr_8[ecx.v];
        // mov     [edi],cl
        *edi.ptr_8 = ecx.l;

    } else {
        // ; Store texel and z
	    // ;
        // mov     [ebp],bx
        *ebp.ptr_16 = ebx.short_low;
        // mov     [edi],cl
        *edi.ptr_8 = ecx.l;
    }

nodraw:

	// ; Linear interpolation section
	// ;
	// ; eax =
	// ; ebx = z
	// ; ecx = dz
	// ; edx =
	// ; esi =
	// ; edi = dest
	// ; ebp = zdest

	// ; Prepare source offset for modification
	// ;

    // pre
    eax.v <<= params[size].pre;
    // mov		ecx,work.tsl._end
    ecx.ptr_v = work.tsl.end;
    // ; Update destinations and check for end of scan
    // ;
    // inc_&dirn	edi
    INC_D(edi.ptr_8, dirn);
    // add_&dirn	ebp,2
    ADD_D(ebp.ptr_8, 2, dirn);

    // cmp		edi,ecx
    // jg_&dirn    ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_done
    if (dirn == DIR_F && edi.ptr_v > ecx.ptr_v || dirn == DIR_B && edi.ptr_v < ecx.ptr_v) {
        return;
    }

    // ; Interpolate z
    // ;
    // mov		ecx,work.tsl.dz
    ecx.v = work.tsl.dz;
    // mov		work.tsl.dest,edi
    work.tsl.dest = edi.ptr_v;
    // add_&dirn	ebx,ecx
    ADD_SET_CF_D(ebx.v, ecx.v, dirn);
    // mov		work.tsl.zdest,ebp
    work.tsl.zdest = ebp.ptr_v;
    // adc_&dirn	ebx,0		; carry into integer part of z
    ADC_D(ebx.v, 0, dirn);

    // ; Perspective interpolation section
	// ;
	// ; eax = source offset
	// ; ebx = u
	// ; ecx = v
	// ; edx = q
	// ; esi =
	// ; edi = du,dv
	// ; ebp = dq
	// ;

    // mov		edx,work.tsl.denominator
    edx.v = work.tsl.denominator;
    // mov		work.tsl.z,ebx
    work.tsl.z = ebx.v;
    // mov		ebp,work.tsl.ddenominator
    ebp.v = work.tsl.ddenominator;
    // mov		ebx,work.tsl.u_numerator
    ebx.v = work.tsl.u_numerator;
    // mov		edi,work.tsl.du_numerator
    edi.v = work.tsl.du_numerator;
    // ; Interpolate u numerator and denominator
    // ;
    // add_&dirn	edx,ebp
    ADD_D(edx.v, ebp.v, dirn);
    // add_&dirn	ebx,edi
    ADD_D(ebx.v, edi.v, dirn);
    // mov		ecx,work.tsl.v_numerator
    ecx.v = work.tsl.v_numerator;

    // ifidni <udirn>,<b>
    if (udirn == eScan_direction_b) {

        // ; Check for u error going outside range 0..1
        // ;
        // jge		nodecu
        if (ebx.int_val >= 0) {
            goto nodecu;
        }
        // ; Adjust u downward
        // ;
deculoop:
        // decu
        eax.l -= params[size].decu;
        // add		edi,ebp
        edi.v += ebp.v;
        // add		ebx,edx
        ebx.v += edx.v;
        // jl		deculoop
        if (ebx.int_val < 0) {
            goto deculoop;
        }
        // mov		work.tsl.du_numerator,edi
        work.tsl.du_numerator = edi.v;
        // jmp		doneu
        goto doneu;
nodecu:
        // cmp		ebx,edx
        // jl		doneu
        if (ebx.int_val < edx.int_val) {
            goto doneu;
        }
        // ; Adjust u upward
        // ;
inculoop:
        // incu
        eax.l += params[size].incu;
        // sub		edi,ebp
        edi.v -= ebp.v;
        // sub		ebx,edx
        ebx.v -= edx.v;
        // cmp		ebx,edx
        // jge		inculoop
        if (ebx.int_val > edx.int_val) {
            goto inculoop;
        }
        // mov		work.tsl.du_numerator,edi
        work.tsl.du_numerator = edi.v;
        // vslot
        // no op

    } else {

        // ; Check for u error going outside range 0..1
	    // ;
        // jl_&udirn	doneu
        if (udirn == eScan_direction_i && ebx.int_val < 0) {
            goto nodecu;
        } else if (udirn == eScan_direction_b && ebx.int_val > 0) {
            goto nodecu;
        } else if (udirn == eScan_direction_d && ebx.int_val >= 0) {
            goto nodecu;
        }

        // ; Adjust u
	    // ;
stepuloop:
        // ifidni <udirn>,<i>
        // incu
        if (udirn == eScan_direction_i) {
            eax.l += params[size].incu;
        }
        // else
		// decu
        else {
            eax.l -= params[size].decu;
        }
        // endif

        // sub_&udirn	edi,ebp
        // sub_&udirn	ebx,edx
        if (udirn == eScan_direction_i) {
            edi.v -= ebp.v;
            SUB_AND_SET_CF(ebx.v, edx.v);
        } else if (udirn == eScan_direction_b) {
            edi.v += ebp.v;
            ADD_AND_SET_CF(ebx.v, edx.v);
        } else if (udirn == eScan_direction_d) {
            edi.v += ebp.v;
            ADD_AND_SET_CF(ebx.v, edx.v);
        }

        // jge_&udirn	stepuloop
        if (udirn == eScan_direction_i && ebx.int_val >= 0) {
            goto stepuloop;
        } else if (udirn == eScan_direction_b && ebx.int_val <= 0) {
            goto stepuloop;
        } else if (udirn == eScan_direction_d && x86_state.cf == 0) {
            goto stepuloop;
        }

		// mov		work.tsl.du_numerator,edi
        work.tsl.du_numerator = edi.v;
		// vslot

    }


doneu:
    // mov		edi,work.tsl.dv_numerator
    edi.v = work.tsl.dv_numerator;
    // mov		work.tsl.u_numerator,ebx
    work.tsl.u_numerator = ebx.v;
    // ; Interpolate v numerator
    // ;
    // add_&dirn	ecx,edi
    ADD_D(ecx.v, edi.v, dirn);
    // mov		work.tsl.denominator,edx
    work.tsl.denominator = edx.v;


    // ifidni <vdirn>,<b>
    if (vdirn == eScan_direction_b) {
        // ; Check for v error going outside range 0..1
        // ;
        // uslot
        // no-op
        // jge		nodecv
        if (ecx.int_val >= 0) {
            goto nodecv;
        }
        // ; Adjust v downward
        // ;
decvloop:
        // decv
        eax.h -= params[size].decv;
        // add		edi,ebp
        edi.v += ebp.v;
        // add		ecx,edx
        ecx.v += edx.v;
        // jl		decvloop
        if (ecx.int_val < 0) {
            goto decvloop;
        }
        // mov		work.tsl.dv_numerator,edi
        work.tsl.dv_numerator = edi.v;
        // jmp		donev
        goto donev;
nodecv:
        // cmp		ecx,edx
        // jl		donev
        if (ecx.int_val < edx.int_val) {
            goto donev;
        }
        // ; Adjust v upward
        // ;
incvloop:
        // incv
        eax.h += params[size].incv;
        // sub		edi,ebp
        edi.v -= ebp.v;
        // sub		ecx,edx
        ecx.v -= edx.v;
        // cmp		ecx,edx
        // jge		incvloop
        if (ecx.int_val >= edx.int_val) {
            goto incvloop;
        }
        // mov		work.tsl.dv_numerator,edi
        work.tsl.dv_numerator = edi.v;
        // vslot
        // no-op
    } else {
        // ; Check for v error going outside range 0..1
	    // ;
        // uslot
		// jl_&vdirn	donev
        if (vdirn == eScan_direction_i && ecx.int_val < 0) {
            goto donev;
        } else if (vdirn == eScan_direction_b && ecx.int_val > 0) {
            goto donev;
        } else if (vdirn == eScan_direction_d && ecx.int_val >= 0) {
            goto donev;
        }

        // ; Adjust v
        // ;
stepvloop:

        // ifidni <vdirn>,<i>
        //         incv
        // else
        //         decv
        // endif
        if (vdirn == eScan_direction_i) {
            eax.h += params[size].incv;
        } else {
            eax.h -= params[size].decv;
        }

        // sub_&vdirn	edi,ebp
        // sub_&vdirn	ecx,edx
        // jge_&vdirn	stepvloop
        if (vdirn == eScan_direction_i) {
            edi.v -= ebp.v;
            SUB_AND_SET_CF(ecx.v, edx.v);
            if (ecx.int_val >= 0) {
                goto stepvloop;
            }
        } else if (vdirn == eScan_direction_b) {
            edi.v += ebp.v;
            ADD_AND_SET_CF(ecx.v, edx.v);
            if (ecx.int_val <= 0) {
                goto stepvloop;
            }
        } else if (vdirn == eScan_direction_d) {
            edi.v += ebp.v;
            ADD_AND_SET_CF(ecx.v, edx.v);
            if (x86_state.cf == 0) {
                goto stepvloop;
            }
        }

		// mov		work.tsl.dv_numerator,edi
        work.tsl.dv_numerator = edi.v;
		// vslot
        // no-op
    }

donev:

	// ; Fix wrapping of source offset after modification
	// ;
    // post1
    eax.v >>= params[size].post1;
    // mov	work.tsl.v_numerator,ecx
    work.tsl.v_numerator = ecx.v;

    // post2
    eax.v &= params[size].post2;
    // mov		ebp,work.tsl.zdest
    ebp.ptr_v = work.tsl.zdest;

    // mov		ebx,work.tsl.z
    ebx.v = work.tsl.z;
    // jmp		next_pixel
    goto next_pixel;
}

static inline void ScanlineRender_ZPTI_I8_D16(int size, int dirn, int udirn, int vdirn, tFog_enabled fogging, tBlend_enabled blend) {
    // mov		edx,work.pu.current
    edx.v = work.pu.current;
    // mov		esi,work.pu.grad_x
    esi.v = work.pu.grad_x;

    if (udirn == eScan_direction_i) {
        // mov		ebx,work.pq.current
        ebx.v = work.pq.current;
        // mov		ecx,work.pq.grad_x
        ecx.v = work.pq.grad_x;
        // sub		edx,ebx				; Move error into the range -1..0
        edx.v -= ebx.v;
        // sub		esi,ecx
        esi.v -= ecx.v;
    }

     // mov		ebp,work.pv.current
    ebp.v = work.pv.current;
    // mov		edi,work.pv.grad_x
    edi.v = work.pv.grad_x;

    // ifidni <vdirn>,<i>
    if (vdirn == eScan_direction_i) {
        // ifdifi <udirn>,<i>
        if (udirn != eScan_direction_i) {
            // mov		ebx,work.pq.current
            ebx.v = work.pq.current;
            // mov		ecx,work.pq.grad_x
            ecx.v = work.pq.grad_x;
        }

        // sub		ebp,ebx				; Move error into the range -1..0
        ebp.v -= ebx.v;
        // sub		edi,ecx
        edi.v -= ecx.v;

    }

    // mov		work.tsl.u_numerator, edx
    work.tsl.u_numerator = edx.v;
    // mov		work.tsl.du_numerator, esi
    work.tsl.du_numerator = esi.v;
    // mov		work.tsl.v_numerator, ebp
    work.tsl.v_numerator = ebp.v;
    // mov		work.tsl.dv_numerator, edi
    work.tsl.dv_numerator = edi.v;
    // mov		esi,work.texture.base
    esi.ptr_v = work.texture.base;
    // mov		eax,work.tsl.source
    eax.v = work.tsl.source;
    // mov		edi,work.tsl.start
    edi.ptr_v = work.tsl.start;
    // mov		ebp,work.tsl.zstart
    ebp.ptr_v = work.tsl.zstart;
    // mov		ebx,work_pz_current
    ebx.v = work_pz_current;

    // mov		edx,work_pi_current
    edx.v = work_pi_current;
    // ror		ebx,16					; Swap z words
    ROR16(ebx);
    // mov		work.tsl.i,edx
    work.tsl.i = edx.v;
    // mov		work.tsl.z,ebx
    work.tsl.z = ebx.v;
    // mov		edx,work.pq.current
    edx.v = work.pq.current;
    // mov		work.tsl.denominator,edx
    work.tsl.denominator = edx.v;

next_pixel:
	// ; Texel fetch and store section
	// ;
	// ; eax = source offset
	// ; ebx = new z value
	// ; ecx = texel
	// ; edx = old z value, shade table
	// ; esi = texture base
	// ; edi = dest
	// ; ebp = zdest

	// ; Perform z buffer test and get texel
	// ;

    // mov		dx,[ebp]
    edx.short_low = *ebp.ptr_16;
    // xor		ecx,ecx
    ecx.v = 0;
    // mov		ebx,work.tsl.z
    ebx.v = work.tsl.z;
    // mov		cl,[eax+esi]
    ecx.l = esi.ptr_8[eax.v];

    // cmp		bx,dx
    // ja		nodraw
    if (ebx.short_low > edx.short_low) {
        goto nodraw;
    }

    // ; Get intensity
	// ;
    // mov		edx,work.shade_table
    edx.ptr_8 = work.shade_table;
    // mov		ch,byte ptr (work.tsl.i+2)
    ecx.h = BYTE2(work.tsl.i);

	// ; Test for transparency
	// ;
    // test	cl,cl
    // jz		nodraw
    if (ecx.l == 0) {
        goto nodraw;
    }

    // ifidni <fogging>,<F>
    if (fogging == 1) {
        // ifidni <blend>,<B>
        if (blend == 1) {
            // ; Look texel up in shade table, fog table, blend table, store texel
            // not implemented
            BrFailure("Not implemented");
        } else {
            // ; Look texel up in shade table + fog table, store texel and z
            // not implemented
            BrAbort();
        }
    } else if (blend == 1) {
        // ; Look texel up in shade table + blend table, store texel
        // not implemented
        BrAbort();
    } else {
        // ; Look texel up in shade table, store texel and z
	    // ;
        // mov     [ebp],bx
        *ebp.ptr_16 = ebx.short_low;
        // mov     cl,[ecx+edx]
        ecx.l = edx.ptr_8[ecx.v];
        // mov     [edi],cl
        *edi.ptr_8 = ecx.l;
    }

nodraw:

	// ; Linear interpolation section
	// ;
	// ; eax =
	// ; ebx = z
	// ; ecx = dz, i
	// ; edx = di
	// ; esi =
	// ; edi = dest
	// ; ebp = zdest

	// ; Prepare source offset for modification
	// ;

    // pre
    eax.v <<= params[size].pre;
    // mov		ecx,work.tsl._end
    ecx.ptr_8 = (uint8_t*)work.tsl.end;
    // ; Update destinations and check for end of scan
    // ;
    // inc_&dirn	edi
    INC_D(edi.ptr_8, dirn);
    // add_&dirn	ebp,2
    ADD_D(ebp.ptr_8, 2, dirn);
    // cmp		edi,ecx
    // jg_&dirn    ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_done
    if (dirn == DIR_F && edi.ptr_v > ecx.ptr_v || dirn == DIR_B && edi.ptr_v < ecx.ptr_v) {
        return;
    }


    // ; Interpolate z and i
    // ;
    // mov		ecx,work.tsl.dz
    ecx.v = work.tsl.dz;
    // mov		edx,work.tsl._di
    edx.v = work.tsl.di;
    // add_&dirn	ebx,ecx
    ADD_SET_CF_D(ebx.v, ecx.v, dirn);
    // mov		ecx,work.tsl.i
    ecx.v = work.tsl.i;
    // adc_&dirn	ebx,0		; carry into integer part of z
    ADC_D(ebx.v, 0, dirn);
    // add_&dirn	ecx,edx
    ADD_D(ecx.v, edx.v, dirn);
    // mov		work.tsl.dest,edi
    work.tsl.dest = edi.ptr_v;
    // mov		work.tsl.zdest,ebp
    work.tsl.zdest = ebp.ptr_v;
    // mov		work.tsl.z,ebx
    work.tsl.z = ebx.v;
    // mov		work.tsl.i,ecx
    work.tsl.i = ecx.v;

    // ; Perspective interpolation section
	// ;
	// ; eax = source offset
	// ; ebx = u
	// ; ecx = v
	// ; edx = q
	// ; esi =
	// ; edi = du,dv
	// ; ebp = dq
	// ;

    // mov		edx,work.tsl.denominator
    edx.v = work.tsl.denominator;
    // mov		ebp,work.tsl.ddenominator
    ebp.v = work.tsl.ddenominator;
    // mov		ebx,work.tsl.u_numerator
    ebx.v = work.tsl.u_numerator;
    // mov		edi,work.tsl.du_numerator
    edi.v = work.tsl.du_numerator;
    // ; Interpolate u numerator and denominator
    // ;
    // add_&dirn	edx,ebp
    ADD_D(edx.v, ebp.v, dirn);
    // add_&dirn	ebx,edi
    ADD_D(ebx.v, edi.v, dirn);
    // mov		ecx,work.tsl.v_numerator
    ecx.v = work.tsl.v_numerator;

    // ifidni <udirn>,<b>
    if (udirn == eScan_direction_b) {

        // ; Check for u error going outside range 0..1
        // ;
        // jge		nodecu
        if (ebx.int_val >= 0) {
            goto nodecu;
        }
        // ; Adjust u downward
        // ;
deculoop:
        // decu
        eax.l -= params[size].decu;
        // add		edi,ebp
        edi.v += ebp.v;
        // add		ebx,edx
        ebx.v += edx.v;
        // jl		deculoop
        if (ebx.int_val < 0) {
            goto deculoop;
        }
        // mov		work.tsl.du_numerator,edi
        work.tsl.du_numerator = edi.v;
        // jmp		doneu
        goto doneu;
nodecu:
        // cmp		ebx,edx
        // jl		doneu
        if (ebx.int_val < edx.int_val) {
            goto doneu;
        }
        // ; Adjust u upward
        // ;
inculoop:
        // incu
        eax.l += params[size].incu;
        // sub		edi,ebp
        edi.v -= ebp.v;
        // sub		ebx,edx
        ebx.v -= edx.v;
        // cmp		ebx,edx
        // jge		inculoop
        if (ebx.int_val > edx.int_val) {
            goto inculoop;
        }
        // mov		work.tsl.du_numerator,edi
        work.tsl.du_numerator = edi.v;
        // vslot
        // no op

    } else {

        // ; Check for u error going outside range 0..1
	    // ;
        // jl_&udirn	doneu
        if (udirn == eScan_direction_i && ebx.int_val < 0) {
            goto nodecu;
        } else if (udirn == eScan_direction_b && ebx.int_val > 0) {
            goto nodecu;
        } else if (udirn == eScan_direction_d && ebx.int_val >= 0) {
            goto nodecu;
        }

        // ; Adjust u
	    // ;
stepuloop:
        // ifidni <udirn>,<i>
        // incu
        if (udirn == eScan_direction_i) {
            eax.l += params[size].incu;
        }
        // else
		// decu
        else {
            eax.l -= params[size].decu;
        }
        // endif

        // sub_&udirn	edi,ebp
        // sub_&udirn	ebx,edx
        if (udirn == eScan_direction_i) {
            edi.v -= ebp.v;
            SUB_AND_SET_CF(ebx.v, edx.v);
        } else if (udirn == eScan_direction_b) {
            edi.v += ebp.v;
            ADD_AND_SET_CF(ebx.v, edx.v);
        } else if (udirn == eScan_direction_d) {
            edi.v += ebp.v;
            ADD_AND_SET_CF(ebx.v, edx.v);
        }

        // jge_&udirn	stepuloop
        if (udirn == eScan_direction_i && ebx.int_val >= 0) {
            goto stepuloop;
        } else if (udirn == eScan_direction_b && ebx.int_val <= 0) {
            goto stepuloop;
        } else if (udirn == eScan_direction_d && x86_state.cf == 0) {
            goto stepuloop;
        }

		// mov		work.tsl.du_numerator,edi
        work.tsl.du_numerator = edi.v;
		// vslot

    }


doneu:
    // mov		edi,work.tsl.dv_numerator
    edi.v = work.tsl.dv_numerator;
    // mov		work.tsl.u_numerator,ebx
    work.tsl.u_numerator = ebx.v;
    // ; Interpolate v numerator
    // ;
    // add_&dirn	ecx,edi
    ADD_D(ecx.v, edi.v, dirn);
    // mov		work.tsl.denominator,edx
    work.tsl.denominator = edx.v;


    // ifidni <vdirn>,<b>
    if (vdirn == eScan_direction_b) {
        // ; Check for v error going outside range 0..1
        // ;
        // uslot
        // no-op
        // jge		nodecv
        if (ecx.int_val >= 0) {
            goto nodecv;
        }
        // ; Adjust v downward
        // ;
decvloop:
        // decv
        eax.h -= params[size].decv;
        // add		edi,ebp
        edi.v += ebp.v;
        // add		ecx,edx
        ecx.v += edx.v;
        // jl		decvloop
        if (ecx.int_val < 0) {
            goto decvloop;
        }
        // mov		work.tsl.dv_numerator,edi
        work.tsl.dv_numerator = edi.v;
        // jmp		donev
        goto donev;
nodecv:
        // cmp		ecx,edx
        // jl		donev
        if (ecx.int_val < edx.int_val) {
            goto donev;
        }
        // ; Adjust v upward
        // ;
incvloop:
        // incv
        eax.h += params[size].incv;
        // sub		edi,ebp
        edi.v -= ebp.v;
        // sub		ecx,edx
        ecx.v -= edx.v;
        // cmp		ecx,edx
        // jge		incvloop
        if (ecx.int_val >= edx.int_val) {
            goto incvloop;
        }
        // mov		work.tsl.dv_numerator,edi
        work.tsl.dv_numerator = edi.v;
        // vslot
        // no-op
    } else {
        // ; Check for v error going outside range 0..1
	    // ;
        // uslot
		// jl_&vdirn	donev
        if (vdirn == eScan_direction_i && ecx.int_val < 0) {
            goto donev;
        } else if (vdirn == eScan_direction_b && ecx.int_val > 0) {
            goto donev;
        } else if (vdirn == eScan_direction_d && ecx.int_val >= 0) {
            goto donev;
        }

        // ; Adjust v
        // ;
stepvloop:

        // ifidni <vdirn>,<i>
        //         incv
        // else
        //         decv
        // endif
        if (vdirn == eScan_direction_i) {
            eax.h += params[size].incv;
        } else {
            eax.h -= params[size].decv;
        }

        // sub_&vdirn	edi,ebp
        // sub_&vdirn	ecx,edx
        // jge_&vdirn	stepvloop
        if (vdirn == eScan_direction_i) {
            edi.v -= ebp.v;
            SUB_AND_SET_CF(ecx.v, edx.v);
            if (ecx.int_val >= 0) {
                goto stepvloop;
            }
        } else if (vdirn == eScan_direction_b) {
            edi.v += ebp.v;
            ADD_AND_SET_CF(ecx.v, edx.v);
            if (ecx.int_val <= 0) {
                goto stepvloop;
            }
        } else if (vdirn == eScan_direction_d) {
            edi.v += ebp.v;
            ADD_AND_SET_CF(ecx.v, edx.v);
            if (x86_state.cf == 0) {
                goto stepvloop;
            }
        }

		// mov		work.tsl.dv_numerator,edi
        work.tsl.dv_numerator = edi.v;
		// vslot
        // no-op
    }

donev:

	// ; Fix wrapping of source offset after modification
	// ;
    // post1
    eax.v >>= params[size].post1;
    // mov	work.tsl.v_numerator,ecx
    work.tsl.v_numerator = ecx.v;

    // post2
    eax.v &= params[size].post2;
    // mov		ebp,work.tsl.zdest
    ebp.ptr_v = work.tsl.zdest;

    // mov		edi,work.tsl.dest
    edi.ptr_v = work.tsl.dest;
    // jmp		next_pixel
    goto next_pixel;
}

// TrapeziumRender_ZPT_I8_D16 64,f,\
//pre 	<shl eax,2>,\
//incu 	<add al,100b>,
// decu <sub al,100b>,
// incv <inc ah>,
// decv <dec ah>,\
// post1 <shr eax,2>,
// post2 <and eax,0fffh>
// TrapeziumRender_ZPT_I8_D16 64,b,\
// 	<shl eax,2>,\
// 	<add al,100b>,<sub al,100b>,<inc ah>,<dec ah>,\
// 	<shr eax,2>,<and eax,0fffh>

// TrapeziumRender_ZPT_I8_D16 256,f,\
// 	<>,\
// 	<inc al>,<dec al>,<inc ah>,<dec ah>,\
// 	<>,<>
// TrapeziumRender_ZPT_I8_D16 256,b,\
// 	<>,\
// 	<inc al>,<dec al>,<inc ah>,<dec ah>,\
// 	<>,<>


static inline void TrapeziumRender_ZPT_I8_D16(int dirn, int size_param, tFog_enabled fogging, tBlend_enabled blend) {

    // mov		ebx,work_top_count	; check for empty trapezium
    ebx.v = work_top_count;
    // test	ebx,ebx
    // jl		done_trapezium
    if (ebx.int_val < 0) {
        return;
    }
    // mov		edi,work_top_i
    edi.v = work_top_i;
    // mov		ebp,work_main_i
    ebp.v = work_main_i;
    // shr		edi,16				; get integer part of end of first scanline
    edi.v >>= 16;
    // and		ebp,0ffffh			; get integer part of start of first scanline
    ebp.v &= 0xffff;

scan_loop:

	// ; Calculate start and end addresses for next scanline
	// ;
    // cmp		ebp,edi				; calculate pixel count
    // jg_&dirn	no_pixels
    if (dirn == DIR_F && ebp.int_val > edi.int_val || dirn == DIR_B && ebp.int_val < edi.int_val){
        goto no_pixels;
    }
    // mov		eax,workspace.scanAddress
    eax.v = workspace.scanAddress;
    // mov		ebx,workspace.depthAddress
    ebx.v = workspace.depthAddress;
    // add		edi,eax				; calculate end colour buffer pointer
    edi.v += eax.v;
    // add		eax,ebp				; calculate start colour buffer pointer
    eax.v += ebp.v;
    // mov		work.tsl._end,edi
    work.tsl.end =  ((char*)work.colour.base) + edi.v;
    // lea		ebx,[ebx+ebp*2]		; calculate start depth buffer pointer
    ebx.v += ebp.v * 2;
    // mov		work.tsl.start,eax
    work.tsl.start = ((char*)work.colour.base) + eax.v;
    // mov		work.tsl.zstart,ebx
    work.tsl.zstart = ((char*)work.depth.base) + ebx.v;

    // ; Fix up the error values
	// ;
     // mov		eax,work.tsl.source
    eax.v = work.tsl.source;
    // mov		ebx,work.pq.current
    ebx.v = work.pq.current;
    // mov		ecx,work.pq.grad_x
    ecx.v = work.pq.grad_x;
    // mov		edx,work.pq.d_nocarry
    edx.v = work.pq.d_nocarry;
    // pre
    eax.v <<= params[size_param].pre;
    // mov		ebp,work.pu.current
    ebp.v = work.pu.current;
    // mov		edi,work.pu.grad_x
    edi.v = work.pu.grad_x;
    // mov		esi,work.pu.d_nocarry
    esi.v = work.pu.d_nocarry;
    // cmp		ebp,ebx
    // jl		uidone
    if (ebp.int_val < ebx.int_val) {
        goto uidone;
    }

uiloop:
    // incu							; work.tsl.source = incu(work.tsl.source)
    eax.l += params[size_param].incu;
    // sub		edi,ecx					; work.pu.grad_x -= work.pq.grad_x
    edi.v -= ecx.v;
    // sub		esi,edx					; work.pu.d_nocarry -= work.pq.d_nocarry
    esi.v -= edx.v;
    // sub		ebp,ebx					; work.pu.current -= work.pq.current
    ebp.v -= ebx.v;
    // cmp		ebp,ebx
    // jge		uiloop
    if (ebp.int_val >= ebx.int_val) {
        goto uiloop;
    }
    // jmp		uddone
    goto uddone;

uidone:
    // cmp		ebp, 0
    // jge		uddone
    if (ebp.int_val >= 0) {
        goto uddone;
    }
udloop:
    // decu							; work.tsl.source = decu(work.tsl.source)
    eax.l -= params[size_param].decu;
    // add		edi,ecx					; work.pu.grad_x += work.pq.grad_x
    edi.v += ecx.v;
    // add		esi,edx					; work.pu.d_nocarry += work.pq.d_nocarry
    esi.v += edx.v;
    // add		ebp,ebx					; work.pu.current += work.pq.current
    ADD_AND_SET_CF(ebp.v, ebx.v);
    // uslot
    // no-op
    // jl		udloop
    if (ebp.int_val < 0) {
        goto udloop;
    }
uddone:
    // mov		work.pu.current,ebp
    work.pu.current = ebp.v;
    // mov		work.pu.grad_x,edi
    work.pu.grad_x = edi.v;
    // mov		work.pu.d_nocarry,esi
    work.pu.d_nocarry = esi.v;
    // mov		ebp,work.pv.current
    ebp.v = work.pv.current;
    // mov		edi,work.pv.grad_x
    edi.v = work.pv.grad_x;
    // mov		esi,work.pv.d_nocarry
    esi.v = work.pv.d_nocarry;
    // cmp		ebp,ebx
    // jl		vidone
    if (ebp.int_val < ebx.int_val) {
        goto vidone;
    }
viloop:
    // incv							; work.tsl.source = incv(work.tsl.source)
    eax.h += params[size_param].incv;
    // sub		edi,ecx					; work.pv.grad_x -= work.pq.grad_x
    edi.v -= ecx.v;
    // sub		esi,edx					; work.pv.d_nocarry -= work.pq.d_nocarry
    esi.v -= edx.v;
    // sub		ebp,ebx					; work.pv.current -= work.pq.current
    ebp.v -= ebx.v;
    // cmp		ebp,ebx
    // jge		viloop
    if (ebp.int_val >= ebx.int_val) {
        goto viloop;
    }
    // jmp		vddone
    goto vddone;
vidone:
    // test	ebp,ebp
    // jge		vddone
    if (ebp.int_val >= 0) {
        goto vddone;
    }
vdloop:
    // decv							; work.tsl.source = decv(work.tsl.source)
    eax.h -= params[size_param].decv;
    // add		edi,ecx					; work.pv.grad_x += work.pq.grad_x
    edi.v += ecx.v;
    // add		esi,edx					; work.pv.d_nocarry += work.pq.d_nocarry
    esi.v += edx.v;
    // add		ebp,ebx					; work.pv.current += work.pq.current
    ADD_AND_SET_CF(ebp.v, ebx.v);
    // uslot
    // no op
    // jl		vdloop
    if (ebp.int_val < 0) {
        goto vdloop;
    }
vddone:
    // ; Select scanline loop and jump to it
	// ;
    // post1
    eax.v >>= params[size_param].post1;
    // mov		work.pv.current,ebp
    work.pv.current = ebp.v;
    // post2
    eax.v &= params[size_param].post2;
    // mov		work.pv.grad_x,edi
    work.pv.grad_x = edi.v;
    // mov		work.pv.d_nocarry,esi
    work.pv.d_nocarry = esi.v;
    // mov		work.tsl.source,eax
    work.tsl.source = eax.v;
    // mov		esi,work.pu.grad_x
    esi.v = work.pu.grad_x;
    // mov		ebp,work.pq.grad_x
    ebp.v = work.pq.grad_x;
    // test	ebp,ebp
    // jl_&dirn	qd
    JL_D(ebp.int_val, qd, dirn);
    // test	esi,esi
    // jle_&dirn	qi_ud
    JLE_D(esi.int_val, qi_ud, dirn);
    // test	edi,edi
    // jle_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ui_vd
    if (dirn == DIR_F && edi.int_val <= 0 || dirn == DIR_B && edi.int_val >= 0) {
        ScanlineRender_ZPT_I8_D16(size_param, dirn, eScan_direction_i, eScan_direction_d, fogging, blend);
        goto ScanlineRender_ZPT_done;
    }
    // jmp		ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ui_vi
    ScanlineRender_ZPT_I8_D16(size_param, dirn, eScan_direction_i, eScan_direction_i, fogging, blend);
    goto ScanlineRender_ZPT_done;
qi_ud:
    // test	edi,edi
    // jle_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ud_vd
    if (dirn == DIR_F && edi.int_val <= 0 || dirn == DIR_B && edi.int_val >= 0) {
        ScanlineRender_ZPT_I8_D16(size_param, dirn, eScan_direction_d, eScan_direction_d, fogging, blend);
        goto ScanlineRender_ZPT_done;
    }
    // jmp		ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ud_vi
    ScanlineRender_ZPT_I8_D16(size_param, dirn, eScan_direction_d, eScan_direction_i, fogging, blend);
    goto ScanlineRender_ZPT_done;
qd:
    // test	esi,esi
    // jle_&dirn	qd_ud
    JLE_D(esi.int_val, qd_ud, dirn);
qd_ui:
    // test	edi,edi
    // jg_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ui_vi
    if (dirn == DIR_F && edi.int_val > 0 || dirn == DIR_B && edi.int_val < 0) {
        ScanlineRender_ZPT_I8_D16(size_param, dirn, eScan_direction_i, eScan_direction_i, fogging, blend);
        goto ScanlineRender_ZPT_done;
    }
    // cmp		edi,ebp
    // jle_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ui_vd
    if (dirn == DIR_F && edi.int_val <= ebp.int_val || dirn == DIR_B && edi.int_val >= ebp.int_val) {
        ScanlineRender_ZPT_I8_D16(size_param, dirn, eScan_direction_i, eScan_direction_d, fogging, blend);
        goto ScanlineRender_ZPT_done;
    }
    // jmp		ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ub_vb
    ScanlineRender_ZPT_I8_D16(size_param, dirn, eScan_direction_b, eScan_direction_b, fogging, blend);
    goto ScanlineRender_ZPT_done;
qd_ud:
    // cmp		esi,ebp
    // jg_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ub_vb
    if (dirn == DIR_F && esi.int_val > ebp.int_val || dirn == DIR_B && esi.int_val < ebp.int_val) {
        ScanlineRender_ZPT_I8_D16(size_param, dirn, eScan_direction_b, eScan_direction_b, fogging, blend);
        goto ScanlineRender_ZPT_done;
    }
    // test	edi,edi
    // jg_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ud_vi
    if (dirn == DIR_F && edi.int_val > 0 || dirn == DIR_B && edi.int_val < 0) {
        ScanlineRender_ZPT_I8_D16(size_param, dirn, eScan_direction_d, eScan_direction_i, fogging, blend);
        goto ScanlineRender_ZPT_done;
    }
    // cmp		edi,ebp
    // jle_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ud_vd
    if (dirn == DIR_F && edi.int_val <= ebp.int_val || dirn == DIR_B && edi.int_val >= ebp.int_val) {
        ScanlineRender_ZPT_I8_D16(size_param, dirn, eScan_direction_d, eScan_direction_d, fogging, blend);
        goto ScanlineRender_ZPT_done;
    }
    // jmp		ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ub_vb
    ScanlineRender_ZPT_I8_D16(size_param, dirn, eScan_direction_b, eScan_direction_b, fogging, blend);

    // ; Scanline loops return here when finished
	// ;
ScanlineRender_ZPT_done:
no_pixels:
    // ; Updates for next scanline:
	// ;
    // mov		eax,workspace.scanAddress
    eax.v = workspace.scanAddress;
    // mov		ebx,work.colour.stride_b
    ebx.v = work.colour.stride_b;
    // mov		ecx,workspace.depthAddress
    ecx.v = workspace.depthAddress;
    // mov		edx,work.depth.stride_b
    edx.v = work.depth.stride_b;
    // add		eax,ebx				; move down one line in colour buffer
    eax.v += ebx.v;
    // add		ecx,edx				; move down one line in depth buffer
    ecx.v += edx.v;
    // mov		workspace.scanAddress,eax
    workspace.scanAddress = eax.v;
    // mov		workspace.depthAddress,ecx
    workspace.depthAddress = ecx.v;
    // mov		ebp,work_main_i
    ebp.v = work_main_i;
    // mov		eax,work_main_d_i
    eax.v = work_main_d_i;
    // add		ebp,eax				; step major edge
    ADD_AND_SET_CF(ebp.v, eax.v);
    // jc		carry
    if (x86_state.cf) {
        goto carry;
    }
    // mov		edi,work_top_i
    edi.v = work_top_i;
    // mov		work_main_i,ebp
    work_main_i = ebp.v;
    // mov		eax,work_top_d_i
    eax.v = work_top_d_i;
    // add		edi,eax				; step minor edge
    edi.v += eax.v;
    // mov		eax,work.pq.current
    eax.v = work.pq.current;
    // mov		work_top_i,edi
    work_top_i = edi.v;
    // mov		ebx,work.pq.d_nocarry
    ebx.v = work.pq.d_nocarry;
    // shr		edi,16				; get integer part of end of next scanline
    edi.v >>= 16;
    // add		eax,ebx				; step q according to carry from major edge
    eax.v += ebx.v;
    // and		ebp,0ffffh			; get integer part of start of next scanline
    ebp.v &= 0xffff;
    // mov		work.pq.current,eax
    work.pq.current = eax.v;
    // mov		eax,work_pz_current
    eax.v = work_pz_current;
    // mov		ebx,work_pz_d_nocarry
    ebx.v = work_pz_d_nocarry;
    // add		eax,ebx				; step z according to carry from major edge
    eax.v += ebx.v;
    // mov		ebx,work.pv.current
    ebx.v = work.pv.current;
    // mov		work_pz_current,eax
    work_pz_current = eax.v;
    // mov		eax,work.pu.current
    eax.v = work.pu.current;
    // add		eax,work.pu.d_nocarry	; step u according to carry from major edge
    eax.v += work.pu.d_nocarry;
    // add		ebx,work.pv.d_nocarry	; step v according to carry from major edge
    ebx.v += work.pv.d_nocarry;
    // mov		work.pu.current,eax
    work.pu.current = eax.v;
    // mov		ecx,work_top_count
    ecx.v = work_top_count;
    // mov		work.pv.current,ebx
    work.pv.current = ebx.v;
    // dec		ecx					; decrement line counter
    ecx.v--;
    // mov		work_top_count,ecx
    work_top_count = ecx.v;
    // jge		scan_loop
    if (ecx.int_val >= 0) {
        goto scan_loop;
    }
    // ret
    return;

carry:
    // adc		ebp,0
    ADC(ebp.v, 0);
    // mov		edi,work_top_i
    edi.v = work_top_i;
    // mov		work_main_i,ebp
    work_main_i = ebp.v;
    // mov		eax,work_top_d_i
    eax.v = work_top_d_i;
    // add		edi,eax				; step minor edge
    edi.v += eax.v;
    // mov		eax,work.pq.current
    eax.v = work.pq.current;
    // mov		work_top_i,edi
    work_top_i = edi.v;
    // mov		ebx,work.pq.d_carry
    ebx.v = work.pq.d_carry;
    // shr		edi,16				; get integer part of end of next scanline
    edi.v >>= 16;
    // add		eax,ebx				; step q according to carry from major edge
    eax.v += ebx.v;
    // and		ebp,0ffffh			; get integer part of start of next scanline
    ebp.v &= 0xffff;
    // mov		work.pq.current,eax
    work.pq.current = eax.v;
    // mov		eax,work_pz_current
    eax.v = work_pz_current;
    // mov		ebx,work_pz_d_carry
    ebx.v = work_pz_d_carry;
    // add		eax,ebx				; step z according to carry from major edge
    eax.v += ebx.v;
    // mov		ebx,work.pv.current
    ebx.v = work.pv.current;
    // mov		work_pz_current,eax
    work_pz_current = eax.v;
    // mov		eax,work.pu.current
    eax.v = work.pu.current;
    // add		eax,work.pu.d_nocarry	; step u according to carry from major edge
    eax.v += work.pu.d_nocarry;
    // add		ebx,work.pv.d_nocarry	; step v according to carry from major edge
    ebx.v += work.pv.d_nocarry;
    // add		eax,work.pu.grad_x	; avoids the need to fixup nocarry and carry
    eax.v += work.pu.grad_x;
    // add		ebx,work.pv.grad_x	; versions
    ebx.v += work.pv.grad_x;
    // mov		work.pu.current,eax
    work.pu.current = eax.v;
    // mov		ecx,work_top_count
    ecx.v = work_top_count;
    // mov		work.pv.current,ebx
    work.pv.current = ebx.v;
    // dec		ecx					; decrement line counter
    ecx.v--;
    // mov		work_top_count,ecx
    work_top_count = ecx.v;
    // jge		scan_loop
    if (ecx.int_val >= 0) {
        goto scan_loop;
    }
}

static inline void TrapeziumRender_ZPTI_I8_D16(int dirn, int size_param, tFog_enabled fog, tBlend_enabled blend) {

    // mov		ebx,work_top_count	; check for empty trapezium
    ebx.v = work_top_count;
    // test	ebx,ebx
    // jl		done_trapezium
    if (ebx.int_val < 0) {
        return;
    }
    // mov		edi,work_top_i
    edi.v = work_top_i;
    // mov		ebp,work_main_i
    ebp.v = work_main_i;
    // shr		edi,16				; get integer part of end of first scanline
    edi.v >>= 16;
    // and		ebp,0ffffh			; get integer part of start of first scanline
    ebp.v &= 0xffff;

scan_loop:

	// ; Calculate start and end addresses for next scanline
	// ;
    // cmp		ebp,edi				; calculate pixel count
    // jg_&dirn	no_pixels
    if (dirn == DIR_F && ebp.int_val > edi.int_val || dirn == DIR_B && ebp.int_val < edi.int_val){
        goto no_pixels;
    }
    // mov		eax,workspace.scanAddress
    eax.v = workspace.scanAddress;
    // mov		ebx,workspace.depthAddress
    ebx.v = workspace.depthAddress;
    // add		edi,eax				; calculate end colour buffer pointer
    edi.v += eax.v;
    // add		eax,ebp				; calculate start colour buffer pointer
    eax.v += ebp.v;
    // mov		work.tsl._end,edi
    work.tsl.end =  ((char*)work.colour.base) + edi.v;
    // lea		ebx,[ebx+ebp*2]		; calculate start depth buffer pointer
    ebx.v += ebp.v * 2;
    // mov		work.tsl.start,eax
    work.tsl.start = ((char*)work.colour.base) + eax.v;
    // mov		work.tsl.zstart,ebx
    work.tsl.zstart = ((char*)work.depth.base) + ebx.v;

    // ; Fix up the error values
	// ;
     // mov		eax,work.tsl.source
    eax.v = work.tsl.source;
    // mov		ebx,work.pq.current
    ebx.v = work.pq.current;
    // mov		ecx,work.pq.grad_x
    ecx.v = work.pq.grad_x;
    // mov		edx,work.pq.d_nocarry
    edx.v = work.pq.d_nocarry;
    // pre
    eax.v <<= params[size_param].pre;
    // mov		ebp,work.pu.current
    ebp.v = work.pu.current;
    // mov		edi,work.pu.grad_x
    edi.v = work.pu.grad_x;
    // mov		esi,work.pu.d_nocarry
    esi.v = work.pu.d_nocarry;
    // cmp		ebp,ebx
    // jl		uidone
    if (ebp.int_val < ebx.int_val) {
        goto uidone;
    }

uiloop:
    // incu							; work.tsl.source = incu(work.tsl.source)
    eax.l += params[size_param].incu;
    // sub		edi,ecx					; work.pu.grad_x -= work.pq.grad_x
    edi.v -= ecx.v;
    // sub		esi,edx					; work.pu.d_nocarry -= work.pq.d_nocarry
    esi.v -= edx.v;
    // sub		ebp,ebx					; work.pu.current -= work.pq.current
    ebp.v -= ebx.v;
    // cmp		ebp,ebx
    // jge		uiloop
    if (ebp.int_val >= ebx.int_val) {
        goto uiloop;
    }
    // jmp		uddone
    goto uddone;

uidone:
    // cmp		ebp, 0
    // jge		uddone
    if (ebp.int_val >= 0) {
        goto uddone;
    }
udloop:
    // decu							; work.tsl.source = decu(work.tsl.source)
    eax.l -= params[size_param].decu;
    // add		edi,ecx					; work.pu.grad_x += work.pq.grad_x
    edi.v += ecx.v;
    // add		esi,edx					; work.pu.d_nocarry += work.pq.d_nocarry
    esi.v += edx.v;
    // add		ebp,ebx					; work.pu.current += work.pq.current
    ADD_AND_SET_CF(ebp.v, ebx.v);
    // uslot
    // no-op
    // jl		udloop
    if (ebp.int_val < 0) {
        goto udloop;
    }
uddone:
    // mov		work.pu.current,ebp
    work.pu.current = ebp.v;
    // mov		work.pu.grad_x,edi
    work.pu.grad_x = edi.v;
    // mov		work.pu.d_nocarry,esi
    work.pu.d_nocarry = esi.v;
    // mov		ebp,work.pv.current
    ebp.v = work.pv.current;
    // mov		edi,work.pv.grad_x
    edi.v = work.pv.grad_x;
    // mov		esi,work.pv.d_nocarry
    esi.v = work.pv.d_nocarry;
    // cmp		ebp,ebx
    // jl		vidone
    if (ebp.int_val < ebx.int_val) {
        goto vidone;
    }
viloop:
    // incv							; work.tsl.source = incv(work.tsl.source)
    eax.h += params[size_param].incv;
    // sub		edi,ecx					; work.pv.grad_x -= work.pq.grad_x
    edi.v -= ecx.v;
    // sub		esi,edx					; work.pv.d_nocarry -= work.pq.d_nocarry
    esi.v -= edx.v;
    // sub		ebp,ebx					; work.pv.current -= work.pq.current
    ebp.v -= ebx.v;
    // cmp		ebp,ebx
    // jge		viloop
    if (ebp.int_val >= ebx.int_val) {
        goto viloop;
    }
    // jmp		vddone
    goto vddone;
vidone:
    // test	ebp,ebp
    // jge		vddone
    if (ebp.int_val >= 0) {
        goto vddone;
    }
vdloop:
    // decv							; work.tsl.source = decv(work.tsl.source)
    eax.h -= params[size_param].decv;
    // add		edi,ecx					; work.pv.grad_x += work.pq.grad_x
    edi.v += ecx.v;
    // add		esi,edx					; work.pv.d_nocarry += work.pq.d_nocarry
    esi.v += edx.v;
    // add		ebp,ebx					; work.pv.current += work.pq.current
    ADD_AND_SET_CF(ebp.v, ebx.v);
    // uslot
    // no op
    // jl		vdloop
    if (ebp.int_val < 0) {
        goto vdloop;
    }
vddone:
    // ; Select scanline loop and jump to it
	// ;
    // post1
    eax.v >>= params[size_param].post1;
    // mov		work.pv.current,ebp
    work.pv.current = ebp.v;
    // post2
    eax.v &= params[size_param].post2;
    // mov		work.pv.grad_x,edi
    work.pv.grad_x = edi.v;
    // mov		work.pv.d_nocarry,esi
    work.pv.d_nocarry = esi.v;
    // mov		work.tsl.source,eax
    work.tsl.source = eax.v;
    // mov		esi,work.pu.grad_x
    esi.v = work.pu.grad_x;
    // mov		ebp,work.pq.grad_x
    ebp.v = work.pq.grad_x;
    // test	ebp,ebp
    // jl_&dirn	qd
    JL_D(ebp.int_val, qd, dirn);
    // test	esi,esi
    // jle_&dirn	qi_ud
    JLE_D(esi.int_val, qi_ud, dirn);
    // test	edi,edi
    // jle_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ui_vd
    if (dirn == DIR_F && edi.int_val <= 0 || dirn == DIR_B && edi.int_val >= 0) {
        ScanlineRender_ZPTI_I8_D16(size_param, dirn, eScan_direction_i, eScan_direction_d, fog, blend);
        goto ScanlineRender_ZPTI_done;
    }
    // jmp		ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ui_vi
    ScanlineRender_ZPTI_I8_D16(size_param, dirn, eScan_direction_i, eScan_direction_i, fog, blend);
    goto ScanlineRender_ZPTI_done;
qi_ud:
    // test	edi,edi
    // jle_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ud_vd
    if (dirn == DIR_F && edi.int_val <= 0 || dirn == DIR_B && edi.int_val >= 0) {
        ScanlineRender_ZPTI_I8_D16(size_param, dirn, eScan_direction_d, eScan_direction_d, fog, blend);
        goto ScanlineRender_ZPTI_done;
    }
    // jmp		ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ud_vi
    ScanlineRender_ZPTI_I8_D16(size_param, dirn, eScan_direction_d, eScan_direction_i, fog, blend);
    goto ScanlineRender_ZPTI_done;
qd:
    // test	esi,esi
    // jle_&dirn	qd_ud
    JLE_D(esi.int_val, qd_ud, dirn);
qd_ui:
    // test	edi,edi
    // jg_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ui_vi
    if (dirn == DIR_F && edi.int_val > 0 || dirn == DIR_B && edi.int_val < 0) {
        ScanlineRender_ZPTI_I8_D16(size_param, dirn, eScan_direction_i, eScan_direction_i, fog, blend);
        goto ScanlineRender_ZPTI_done;
    }
    // cmp		edi,ebp
    // jle_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ui_vd
    if (dirn == DIR_F && edi.int_val <= ebp.int_val || dirn == DIR_B && edi.int_val >= ebp.int_val) {
        ScanlineRender_ZPTI_I8_D16(size_param, dirn, eScan_direction_i, eScan_direction_d, fog, blend);
        goto ScanlineRender_ZPTI_done;
    }
    // jmp		ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ub_vb
    ScanlineRender_ZPTI_I8_D16(size_param, dirn, eScan_direction_b, eScan_direction_b, fog, blend);
    goto ScanlineRender_ZPTI_done;
qd_ud:
    // cmp		esi,ebp
    // jg_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ub_vb
    if (dirn == DIR_F && esi.int_val > ebp.int_val || dirn == DIR_B && esi.int_val < ebp.int_val) {
        ScanlineRender_ZPTI_I8_D16(size_param, dirn, eScan_direction_b, eScan_direction_b, fog, blend);
        goto ScanlineRender_ZPTI_done;
    }
    // test	edi,edi
    // jg_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ud_vi
    if (dirn == DIR_F && edi.int_val > 0 || dirn == DIR_B && edi.int_val < 0) {
        ScanlineRender_ZPTI_I8_D16(size_param, dirn, eScan_direction_d, eScan_direction_i, fog, blend);
        goto ScanlineRender_ZPTI_done;
    }
    // cmp		edi,ebp
    // jle_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ud_vd
    if (dirn == DIR_F && edi.int_val <= ebp.int_val || dirn == DIR_B && edi.int_val >= ebp.int_val) {
        ScanlineRender_ZPTI_I8_D16(size_param, dirn, eScan_direction_d, eScan_direction_d, fog, blend);
        goto ScanlineRender_ZPTI_done;
    }
    // jmp		ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ub_vb
    ScanlineRender_ZPTI_I8_D16(size_param, dirn, eScan_direction_b, eScan_direction_b, fog, blend);

    // ; Scanline loops return here when finished
	// ;
ScanlineRender_ZPTI_done:
no_pixels:
    // ; Updates for next scanline:
	// ;
    // mov		eax,workspace.scanAddress
    eax.v = workspace.scanAddress;
    // mov		ebx,work.colour.stride_b
    ebx.v = work.colour.stride_b;
    // mov		ecx,workspace.depthAddress
    ecx.v = workspace.depthAddress;
    // mov		edx,work.depth.stride_b
    edx.v = work.depth.stride_b;
    // add		eax,ebx				; move down one line in colour buffer
    eax.v += ebx.v;
    // add		ecx,edx				; move down one line in depth buffer
    ecx.v += edx.v;
    // mov		workspace.scanAddress,eax
    workspace.scanAddress = eax.v;
    // mov		workspace.depthAddress,ecx
    workspace.depthAddress = ecx.v;
    // mov		ebp,work_main_i
    ebp.v = work_main_i;
    // mov		eax,work_main_d_i
    eax.v = work_main_d_i;
    // add		ebp,eax				; step major edge
    ADD_AND_SET_CF(ebp.v, eax.v);
    // jc		carry
    if (x86_state.cf) {
        goto carry;
    }
    // mov		edi,work_top_i
    edi.v = work_top_i;
    // mov		work_main_i,ebp
    work_main_i = ebp.v;
    // mov		eax,work_top_d_i
    eax.v = work_top_d_i;
    // add		edi,eax				; step minor edge
    edi.v += eax.v;
    // mov		eax,work.pq.current
    eax.v = work.pq.current;
    // mov		work_top_i,edi
    work_top_i = edi.v;
    // mov		ebx,work.pq.d_nocarry
    ebx.v = work.pq.d_nocarry;
    // shr		edi,16				; get integer part of end of next scanline
    edi.v >>= 16;
    // add		eax,ebx				; step q according to carry from major edge
    eax.v += ebx.v;
    // and		ebp,0ffffh			; get integer part of start of next scanline
    ebp.v &= 0xffff;
    // mov		work.pq.current,eax
    work.pq.current = eax.v;
    // mov		eax,work_pz_current
    eax.v = work_pz_current;

    // mov		ebx,work_pi_current
    ebx.v = work_pi_current;
    // add		eax,work_pz_d_nocarry	; step z according to carry from major edge
    eax.v += work_pz_d_nocarry;
    // add		ebx,work_pi_d_nocarry	; step i according to carry from major edge
    ebx.v += work_pi_d_nocarry;
    // mov		work_pz_current,eax
    work_pz_current = eax.v;
    // mov		work_pi_current,ebx
    work_pi_current = ebx.v;     // ZPTI
    // mov		eax,work.pu.current
    eax.v = work.pu.current;
    // mov		ebx,work.pv.current     // ZPTI
    ebx.v = work.pv.current;

    // add		eax,work.pu.d_nocarry	; step u according to carry from major edge
    eax.v += work.pu.d_nocarry;
    // add		ebx,work.pv.d_nocarry	; step v according to carry from major edge
    ebx.v += work.pv.d_nocarry;
    // mov		work.pu.current,eax
    work.pu.current = eax.v;
    // mov		ecx,work_top_count
    ecx.v = work_top_count;
    // mov		work.pv.current,ebx
    work.pv.current = ebx.v;
    // dec		ecx					; decrement line counter
    ecx.v--;
    // mov		work_top_count,ecx
    work_top_count = ecx.v;
    // jge		scan_loop
    if (ecx.int_val >= 0) {
        goto scan_loop;
    }
    // ret
    return;

carry:
    // adc		ebp,0
    ADC(ebp.v, 0);
    // mov		edi,work_top_i
    edi.v = work_top_i;
    // mov		work_main_i,ebp
    work_main_i = ebp.v;
    // mov		eax,work_top_d_i
    eax.v = work_top_d_i;
    // add		edi,eax				; step minor edge
    edi.v += eax.v;
    // mov		eax,work.pq.current
    eax.v = work.pq.current;
    // mov		work_top_i,edi
    work_top_i = edi.v;
    // mov		ebx,work.pq.d_carry
    ebx.v = work.pq.d_carry;
    // shr		edi,16				; get integer part of end of next scanline
    edi.v >>= 16;
    // add		eax,ebx				; step q according to carry from major edge
    eax.v += ebx.v;
    // and		ebp,0ffffh			; get integer part of start of next scanline
    ebp.v &= 0xffff;
    // mov		work.pq.current,eax
    work.pq.current = eax.v;
    // mov		eax,work_pz_current
    eax.v = work_pz_current;
    // mov		ebx,work_pz_d_carry

    // mov		ebx,work_pi_current
    ebx.v = work_pi_current;
    // add		eax,work_pz_d_carry	; step z according to carry from major edge
    eax.v += work_pz_d_carry;
    // add		ebx,work_pi_d_carry	; step i according to carry from major edge
    ebx.v += work_pi_d_carry;


    // mov		work_pz_current,eax
    work_pz_current = eax.v;
    // mov		work_pi_current,ebx
    work_pi_current = ebx.v;
    // mov		eax,work.pu.current
    eax.v = work.pu.current;
    // mov		ebx,work.pv.current
    ebx.v = work.pv.current;
    // add		eax,work.pu.d_nocarry	; step u according to carry from major edge
    eax.v += work.pu.d_nocarry;
    // add		ebx,work.pv.d_nocarry	; step v according to carry from major edge
    ebx.v += work.pv.d_nocarry;
    // add		eax,work.pu.grad_x	; avoids the need to fixup nocarry and carry
    eax.v += work.pu.grad_x;
    // add		ebx,work.pv.grad_x	; versions
    ebx.v += work.pv.grad_x;
    // mov		work.pu.current,eax
    work.pu.current = eax.v;
    // mov		ecx,work_top_count
    ecx.v = work_top_count;
    // mov		work.pv.current,ebx
    work.pv.current = ebx.v;
    // dec		ecx					; decrement line counter
    ecx.v--;
    // mov		work_top_count,ecx
    work_top_count = ecx.v;
    // jge		scan_loop
    if (ecx.int_val >= 0) {
        goto scan_loop;
    }
}

void BR_ASM_CALL TriangleRender_ZPTI_I8_D16_32(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTI_I8_D16_32_FLAT(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPT_I8_D16_32(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTI_I8_D16_64(brp_block *block, ...) {
    va_list     va;
    va_start(va, block);
    brp_vertex *v0;
    brp_vertex *v1;
    brp_vertex *v2;

	v0 = va_arg(va, brp_vertex *);
    v1 = va_arg(va, brp_vertex *);
    v2 = va_arg(va, brp_vertex *);
    va_end(va);

    TriangleSetup_ZPTI(v0, v1, v2);
    // jc TriangleRasterise_ZTI_I8_D16_64
    if (x86_state.cf) {
        va_list l;
        TriangleRender_ZTI_I8_D16_POW2(block, 6, 1, l);
        return;
    }

    // ; Calculate address of first scanline in colour and depth buffers
	// ;
    // mov		esi,work_main_y
    esi.v = work_main_y;
    // mov		eax,work.colour.base
    eax.v = WORK_COLOUR_BASE;
    // dec		esi
    esi.v--;
    // mov		ebx,work.colour.stride_b
    ebx.v = work.colour.stride_b;
    // mov		ecx,work.depth.base
    ecx.v = WORK_DEPTH_BASE;
    // mov		edx,work.depth.stride_b
    edx.v = work.depth.stride_b;
    // imul	ebx,esi
    ebx.int_val *= esi.int_val;
    // imul	edx,esi
    edx.int_val *= esi.int_val;
    // add		eax,ebx
    eax.v += ebx.v;
    // add		ecx,edx
    ecx.v += edx.v;
    // dec		eax
    eax.v--;
    // sub		ecx,2
    ecx.v -= 2;
    // mov		workspace.scanAddress,eax
    workspace.scanAddress = eax.v;
    // mov		workspace.depthAddress,ecx
    workspace.depthAddress = ecx.v;

    // ; Swap integer and fractional parts of major edge starting value and delta and z gradient
	// ; Copy some values into perspective texture mappng workspace
	// ; Calculate offset of starting pixel in texture map
	// ;
    // mov		eax,work_main_i
    eax.v = work_main_i;
    // mov		ebx,work_main_d_i
    ebx.v = work_main_d_i;
    // ror		eax,16
    ROR16(eax);
    // cmp		ebx,80000000h
    CMP(ebx.v, 0x80000000);
    // adc		ebx,-1
    ADC(ebx.v, -1);
    // mov		ecx,work_pz_grad_x
    ecx.v = work_pz_grad_x;
    // ror		ebx,16
    ROR16(ebx);
    // cmp		ecx,80000000h
    CMP(ecx.v, 0x80000000);
    // adc		ecx,-1
    ADC(ecx.v, -1);


    // mov		edx,work_pi_grad_x
    edx.v = work_pi_grad_x;
	// 	ror		ecx,16
    ROR16(ecx);
	// 	cmp		edx,80000000h
    CMP(edx.v, 0x80000000);
	// 	adc		edx,-1
    ADC(edx.v, -1);

    // mov		work_main_i,eax
    work_main_i = eax.v;


    // mov		al,byte ptr work.awsl.u_current
    eax.l = work.awsl.u_current;

    // mov		work_main_d_i,ebx
    work_main_d_i = ebx.v;

    // mov		ah,byte ptr work.awsl.v_current
    eax.h = work.awsl.v_current;

    // mov		work.tsl.dz,ecx
    work.tsl.dz = ecx.v;

    // shl		al,2
    eax.l <<= 2;
    // mov		work.tsl._di,edx
    work.tsl.di = edx.v;

    // shr		eax,2
    eax.v >>= 2;
    // mov		ebx,work.pq.grad_x
    ebx.v = work.pq.grad_x;
    // and		eax,63*65
    eax.v &= 63*65;
    // mov		work.tsl.ddenominator,ebx
    work.tsl.ddenominator = ebx.v;
    // mov		work.tsl.source,eax
    work.tsl.source = eax.v;
    // mov		eax,work.tsl.direction
    eax.v = work.tsl.direction;

    // ; Check scan direction and use appropriate rasteriser
	// ;
    // test	eax,eax
    // jnz		reversed
    if (eax.v != 0) {
        goto reversed;
    }
    // call    TrapeziumRender_ZPT_I8_D16_64_f
    TrapeziumRender_ZPTI_I8_D16(DIR_F, eTrapezium_render_size_64, eFog_no, eBlend_no);
    // mov		eax,work_bot_i
    eax.v = work_bot_i;
    // mov		ebx,work_bot_d_i
    ebx.v = work_bot_d_i;
    // mov		ecx,work_bot_count
    ecx.v = work_bot_count;
    // mov		work_top_i,eax
    work_top_i = eax.v;
    // mov		work_top_d_i,ebx
    work_top_d_i = ebx.v;
    // mov		work_top_count,ecx
    work_top_count = ecx.v;
    // call    TrapeziumRender_ZPT_I8_D16_64_f
   TrapeziumRender_ZPTI_I8_D16(DIR_F, eTrapezium_render_size_64, eFog_no, eBlend_no);
    // ret
    return;

reversed:

    // call    TrapeziumRender_ZPT_I8_D16_64_b
   TrapeziumRender_ZPTI_I8_D16(DIR_B, eTrapezium_render_size_64, eFog_no, eBlend_no);
    // mov		eax,work_bot_i
    eax.v = work_bot_i;
    // mov		ebx,work_bot_d_i
    ebx.v = work_bot_d_i;
    // mov		ecx,work_bot_count
    ecx.v = work_bot_count;
    // mov		work_top_i,eax
    work_top_i = eax.v;
    // mov		work_top_d_i,ebx
    work_top_d_i = ebx.v;
    // mov		work_top_count,ecx
    work_top_count = ecx.v;
    // call    TrapeziumRender_ZPT_I8_D16_64_b
    TrapeziumRender_ZPTI_I8_D16(DIR_B, eTrapezium_render_size_64, eFog_no, eBlend_no);
}

void BR_ASM_CALL TriangleRender_ZPTI_I8_D16_64_FLAT(brp_block *block, ...) {
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
    // jc TriangleRasterise_ZT_I8_D16_64
    if (x86_state.cf) {
        va_list l;
        TriangleRender_ZT_I8_D16_POW2(block, 6, 1, l);
        return;
    }

    // ; Calculate address of first scanline in colour and depth buffers
	// ;
    // mov		esi,work_main_y
    esi.v = work_main_y;
    // mov		eax,work.colour.base
    eax.v = WORK_COLOUR_BASE;
    // dec		esi
    esi.v--;
    // mov		ebx,work.colour.stride_b
    ebx.v = work.colour.stride_b;
    // mov		ecx,work.depth.base
    ecx.v = WORK_DEPTH_BASE;
    // mov		edx,work.depth.stride_b
    edx.v = work.depth.stride_b;
    // imul	ebx,esi
    ebx.int_val *= esi.int_val;
    // imul	edx,esi
    edx.int_val *= esi.int_val;
    // add		eax,ebx
    eax.v += ebx.v;
    // add		ecx,edx
    ecx.v += edx.v;
    // dec		eax
    eax.v--;
    // sub		ecx,2
    ecx.v -= 2;
    // mov		workspace.scanAddress,eax
    workspace.scanAddress = eax.v;
    // mov		workspace.depthAddress,ecx
    workspace.depthAddress = ecx.v;

    // ; Swap integer and fractional parts of major edge starting value and delta and z gradient
	// ; Copy some values into perspective texture mappng workspace
	// ; Calculate offset of starting pixel in texture map
	// ;
    // mov		eax,work_main_i
    eax.v = work_main_i;
    // mov		ebx,work_main_d_i
    ebx.v = work_main_d_i;
    // ror		eax,16
    ROR16(eax);
    // cmp		ebx,80000000h
    CMP(ebx.v, 0x80000000);
    // adc		ebx,-1
    ADC(ebx.v, -1);
    // mov		ecx,work_pz_grad_x
    ecx.v = work_pz_grad_x;
    // ror		ebx,16
    ROR16(ebx);
    // cmp		ecx,80000000h
    CMP(ecx.v, 0x80000000);
    // adc		ecx,-1
    ADC(ecx.v, -1);
    // mov		work_main_i,eax
    work_main_i = eax.v;
    // ror		ecx,16
    ROR16(ecx);
    // mov		al,byte ptr work.awsl.u_current
    eax.l = work.awsl.u_current;
    // mov		ah,byte ptr work.awsl.v_current
    eax.h = work.awsl.v_current;
    // mov		work_main_d_i,ebx
    work_main_d_i = ebx.v;
    // shl		al,2
    eax.l <<= 2;
    // mov		work.tsl.dz,ecx
    work.tsl.dz = ecx.v;
    // shr		eax,2
    eax.v >>= 2;
    // mov		ebx,work.pq.grad_x
    ebx.v = work.pq.grad_x;
    // and		eax,63*65
    eax.v &= 63*65;
    // mov		work.tsl.ddenominator,ebx
    work.tsl.ddenominator = ebx.v;
    // mov		work.tsl.source,eax
    work.tsl.source = eax.v;
    // mov		eax,work.tsl.direction
    eax.v = work.tsl.direction;

    // ; Check scan direction and use appropriate rasteriser
	// ;
    // test	eax,eax
    // jnz		reversed
    if (eax.v != 0) {
        goto reversed;
    }
    // call    TrapeziumRender_ZPT_I8_D16_64_f
    TrapeziumRender_ZPT_I8_D16(DIR_F, eTrapezium_render_size_64, eFog_no, eBlend_no);
    // mov		eax,work_bot_i
    eax.v = work_bot_i;
    // mov		ebx,work_bot_d_i
    ebx.v = work_bot_d_i;
    // mov		ecx,work_bot_count
    ecx.v = work_bot_count;
    // mov		work_top_i,eax
    work_top_i = eax.v;
    // mov		work_top_d_i,ebx
    work_top_d_i = ebx.v;
    // mov		work_top_count,ecx
    work_top_count = ecx.v;
    // call    TrapeziumRender_ZPT_I8_D16_64_f
   TrapeziumRender_ZPT_I8_D16(DIR_F, eTrapezium_render_size_64, eFog_no, eBlend_no);
    // ret
    return;

reversed:

    // call    TrapeziumRender_ZPT_I8_D16_64_b
   TrapeziumRender_ZPT_I8_D16(DIR_B, eTrapezium_render_size_64, eFog_no, eBlend_no);
    // mov		eax,work_bot_i
    eax.v = work_bot_i;
    // mov		ebx,work_bot_d_i
    ebx.v = work_bot_d_i;
    // mov		ecx,work_bot_count
    ecx.v = work_bot_count;
    // mov		work_top_i,eax
    work_top_i = eax.v;
    // mov		work_top_d_i,ebx
    work_top_d_i = ebx.v;
    // mov		work_top_count,ecx
    work_top_count = ecx.v;
    // call    TrapeziumRender_ZPT_I8_D16_64_b
    TrapeziumRender_ZPT_I8_D16(DIR_B, eTrapezium_render_size_64, eFog_no, eBlend_no);

}

void BR_ASM_CALL TriangleRender_ZPTI_I8_D16_128(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}

void BR_ASM_CALL TriangleRender_ZPTI_I8_D16_128_FLAT(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}

void BR_ASM_CALL TriangleRender_ZPT_I8_D16_128(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}

void BR_ASM_CALL TriangleRender_ZPTI_I8_D16_256(brp_block *block, ...) {
    va_list     va;
    va_start(va, block);
    brp_vertex *v0;
    brp_vertex *v1;
    brp_vertex *v2;

	v0 = va_arg(va, brp_vertex *);
    v1 = va_arg(va, brp_vertex *);
    v2 = va_arg(va, brp_vertex *);
    va_end(va);

    TriangleSetup_ZPTI(v0, v1, v2);
    // jc TriangleRasterise_ZTI_I8_D16_256
    if (x86_state.cf) {
        va_list l;
        TriangleRender_ZTI_I8_D16_POW2(block, 8, 1, l);
        return;
    }

    // ; Calculate address of first scanline in colour and depth buffers
	// ;
    // mov		esi,work_main_y
    esi.v = work_main_y;
    // mov		eax,work.colour.base
    eax.v = WORK_COLOUR_BASE;
    // dec		esi
    esi.v--;
    // mov		ebx,work.colour.stride_b
    ebx.v = work.colour.stride_b;
    // mov		ecx,work.depth.base
    ecx.v = WORK_DEPTH_BASE;
    // mov		edx,work.depth.stride_b
    edx.v = work.depth.stride_b;
    // imul	ebx,esi
    ebx.int_val *= esi.int_val;
    // imul	edx,esi
    edx.int_val *= esi.int_val;
    // add		eax,ebx
    eax.v += ebx.v;
    // add		ecx,edx
    ecx.v += edx.v;
    // dec		eax
    eax.v--;
    // sub		ecx,2
    ecx.v -= 2;
    // mov		workspace.scanAddress,eax
    workspace.scanAddress = eax.v;
    // mov		workspace.depthAddress,ecx
    workspace.depthAddress = ecx.v;

    // ; Swap integer and fractional parts of major edge starting value and delta and z gradient
	// ; Copy some values into perspective texture mappng workspace
	// ; Calculate offset of starting pixel in texture map
	// ;
    // mov		eax,work_main_i
    eax.v = work_main_i;
    // mov		ebx,work_main_d_i
    ebx.v = work_main_d_i;
    // ror		eax,16
    ROR16(eax);
    // cmp		ebx,80000000h
    CMP(ebx.v, 0x80000000);
    // adc		ebx,-1
    ADC(ebx.v, -1);
    // mov		ecx,work_pz_grad_x
    ecx.v = work_pz_grad_x;
    // ror		ebx,16
    ROR16(ebx);
    // cmp		ecx,80000000h
    CMP(ecx.v, 0x80000000);
    // adc		ecx,-1
    ADC(ecx.v, -1);
    // mov		edx,work_pi_grad_x
    edx.v = work_pi_grad_x;
    // ror		ecx,16
    ROR16(ecx);
    // cmp		edx,80000000h
    CMP(edx.v, 0x80000000);
    // adc		edx,-1
    ADC(edx.v, -1);
    // mov		work_main_i,eax
    work_main_i = eax.v;
    // xor		eax,eax
    eax.v = 0;
    // mov		work_main_d_i,ebx
    work_main_d_i = ebx.v;
    // mov		al,byte ptr work.awsl.u_current
    eax.l = work.awsl.u_current & 0xff;
    // mov		work.tsl.dz,ecx
    work.tsl.dz = ecx.v;
    // mov		ah,byte ptr work.awsl.v_current
    eax.h = work.awsl.v_current & 0xff;
    // mov		work.tsl._di,edx
    work.tsl.di = edx.v;
    // mov		work.tsl.source,eax
    work.tsl.source = eax.v;
    // mov		ebx,work.pq.grad_x
    ebx.v = work.pq.grad_x;
    // mov		work.tsl.ddenominator,ebx
    work.tsl.ddenominator = ebx.v;
    // mov		eax,work.tsl.direction
    eax.v = work.tsl.direction;

    // ; Check scan direction and use appropriate rasteriser
	// ;
    // test	eax,eax
    // jnz		reversed
    if (eax.v != 0) {
        goto reversed;
    }
    // call    TrapeziumRender_ZPTI_I8_D16_256_f
    TrapeziumRender_ZPTI_I8_D16(DIR_F, eTrapezium_render_size_256, eFog_no, eBlend_no);
    // mov		eax,work_bot_i
    eax.v = work_bot_i;
    // mov		ebx,work_bot_d_i
    ebx.v = work_bot_d_i;
    // mov		ecx,work_bot_count
    ecx.v = work_bot_count;
    // mov		work_top_i,eax
    work_top_i = eax.v;
    // mov		work_top_d_i,ebx
    work_top_d_i = ebx.v;
    // mov		work_top_count,ecx
    work_top_count = ecx.v;
    // call    TrapeziumRender_ZPTI_I8_D16_256_f
   TrapeziumRender_ZPTI_I8_D16(DIR_F, eTrapezium_render_size_256, eFog_no, eBlend_no);
    // ret
    return;

reversed:

    // call    TrapeziumRender_ZPTI_I8_D16_64_b
   TrapeziumRender_ZPTI_I8_D16(DIR_B, eTrapezium_render_size_256, eFog_no, eBlend_no);
    // mov		eax,work_bot_i
    eax.v = work_bot_i;
    // mov		ebx,work_bot_d_i
    ebx.v = work_bot_d_i;
    // mov		ecx,work_bot_count
    ecx.v = work_bot_count;
    // mov		work_top_i,eax
    work_top_i = eax.v;
    // mov		work_top_d_i,ebx
    work_top_d_i = ebx.v;
    // mov		work_top_count,ecx
    work_top_count = ecx.v;
    // call    TrapeziumRender_ZPTI_I8_D16_64_b
    TrapeziumRender_ZPTI_I8_D16(DIR_B, eTrapezium_render_size_256, eFog_no, eBlend_no);
}

void BR_ASM_CALL TriangleRender_ZPTI_I8_D16_256_FLAT(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}

void BR_ASM_CALL TriangleRender_ZPT_I8_D16_256(brp_block *block, ...) {
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
    // jc TriangleRasterise_ZT_I8_D16_256
    if (x86_state.cf) {
        va_list l;
        TriangleRender_ZT_I8_D16_POW2(block, 8, 1, l);
        return;
    }

    // ; Calculate address of first scanline in colour and depth buffers
	// ;
    // mov		esi,work_main_y
    esi.v = work_main_y;
    // mov		eax,work.colour.base
    eax.v = WORK_COLOUR_BASE;
    // dec		esi
    esi.v--;
    // mov		ebx,work.colour.stride_b
    ebx.v = work.colour.stride_b;
    // mov		ecx,work.depth.base
    ecx.v = WORK_DEPTH_BASE;
    // mov		edx,work.depth.stride_b
    edx.v = work.depth.stride_b;
    // imul	ebx,esi
    ebx.int_val *= esi.int_val;
    // imul	edx,esi
    edx.int_val *= esi.int_val;
    // add		eax,ebx
    eax.v += ebx.v;
    // add		ecx,edx
    ecx.v += edx.v;
    // dec		eax
    eax.v--;
    // sub		ecx,2
    ecx.v -= 2;
    // mov		workspace.scanAddress,eax
    workspace.scanAddress = eax.v;
    // mov		workspace.depthAddress,ecx
    workspace.depthAddress = ecx.v;

    // ; Swap integer and fractional parts of major edge starting value and delta and z gradient
	// ; Copy some values into perspective texture mappng workspace
	// ; Calculate offset of starting pixel in texture map
	// ;
    // mov		eax,work_main_i
    eax.v = work_main_i;
    // mov		ebx,work_main_d_i
    ebx.v = work_main_d_i;
    // ror		eax,16
    ROR16(eax);
    // cmp		ebx,80000000h
    CMP(ebx.v, 0x80000000);
    // adc		ebx,-1
    ADC(ebx.v, -1);
    // mov		ecx,work_pz_grad_x
    ecx.v = work_pz_grad_x;
    // ror		ebx,16
    ROR16(ebx);
    // cmp		ecx,80000000h
    CMP(ecx.v, 0x80000000);
    // adc		ecx,-1
    ADC(ecx.v, -1);
    // mov		work_main_i,eax
    work_main_i = eax.v;
    // ror		ecx,16
    ROR16(ecx);
    // mov		work_main_d_i,ebx
    work_main_d_i = ebx.v;
    // xor eax,eax
    eax.v = 0;
    // mov		work.tsl.dz,ecx
    work.tsl.dz = ecx.v;
    // mov		al,byte ptr work.awsl.u_current
    eax.l = work.awsl.u_current;
    // mov		ebx,work.pq.grad_x
    ebx.v = work.pq.grad_x;
    // mov		ah,byte ptr work.awsl.v_current
    eax.h = work.awsl.v_current;
    // mov		work.tsl.ddenominator,ebx
    work.tsl.ddenominator = ebx.v;
    // mov		work.tsl.source,eax
    work.tsl.source = eax.v;
    // mov		eax,work.tsl.direction
    eax.v = work.tsl.direction;

    // ; Check scan direction and use appropriate rasteriser
	// ;
    // test	eax,eax
    // jnz		reversed
    if (eax.v != 0) {
        goto reversed;
    }
    // call    TrapeziumRender_ZPT_I8_D16_256_f
    TrapeziumRender_ZPT_I8_D16(DIR_F, eTrapezium_render_size_256, eFog_no, eBlend_no);
    // mov		eax,work_bot_i
    eax.v = work_bot_i;
    // mov		ebx,work_bot_d_i
    ebx.v = work_bot_d_i;
    // mov		ecx,work_bot_count
    ecx.v = work_bot_count;
    // mov		work_top_i,eax
    work_top_i = eax.v;
    // mov		work_top_d_i,ebx
    work_top_d_i = ebx.v;
    // mov		work_top_count,ecx
    work_top_count = ecx.v;
    // call    TrapeziumRender_ZPT_I8_D16_256_f
   TrapeziumRender_ZPT_I8_D16(DIR_F, eTrapezium_render_size_256, eFog_no, eBlend_no);
    // ret
    return;

reversed:

    // call    TrapeziumRender_ZPT_I8_D16_64_b
   TrapeziumRender_ZPT_I8_D16(DIR_B, eTrapezium_render_size_256, eFog_no, eBlend_no);
    // mov		eax,work_bot_i
    eax.v = work_bot_i;
    // mov		ebx,work_bot_d_i
    ebx.v = work_bot_d_i;
    // mov		ecx,work_bot_count
    ecx.v = work_bot_count;
    // mov		work_top_i,eax
    work_top_i = eax.v;
    // mov		work_top_d_i,ebx
    work_top_d_i = ebx.v;
    // mov		work_top_count,ecx
    work_top_count = ecx.v;
    // call    TrapeziumRender_ZPT_I8_D16_64_b
    TrapeziumRender_ZPT_I8_D16(DIR_B, eTrapezium_render_size_256, eFog_no, eBlend_no);
}

void BR_ASM_CALL TriangleRender_ZPTI_I8_D16_1024(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTI_I8_D16_1024_FLAT(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPT_I8_D16_1024(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIF_I8_D16_32(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIF_I8_D16_32_FLAT(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTF_I8_D16_32(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIF_I8_D16_64(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIF_I8_D16_64_FLAT(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTF_I8_D16_64(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIF_I8_D16_128(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIF_I8_D16_128_FLAT(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTF_I8_D16_128(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIF_I8_D16_256(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIF_I8_D16_256_FLAT(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTF_I8_D16_256(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIF_I8_D16_1024(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIF_I8_D16_1024_FLAT(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTF_I8_D16_1024(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIB_I8_D16_32(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIB_I8_D16_32_FLAT(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTB_I8_D16_32(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIB_I8_D16_64(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIB_I8_D16_64_FLAT(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTB_I8_D16_64(brp_block *block, ...) {
    va_list     va;
    va_start(va, block);
    brp_vertex *v0;
    brp_vertex *v1;
    brp_vertex *v2;

	v0 = va_arg(va, brp_vertex *);
    v1 = va_arg(va, brp_vertex *);
    v2 = va_arg(va, brp_vertex *);
    va_end(va);

#if ENABLE_PERSPECTIVE_CHEAT_FOR_BLEND==1
    TriangleSetup_ZPT(v0, v1, v2);
    // jc TriangleRasterise_ZTB_I8_D16_64
    if (x86_state.cf) {
        va_list l;
        TriangleRender_ZTB_I8_D16_POW2(block, 6, 1, l);
        return;
    }
#else
    TriangleSetup_ZPT_NOCHEAT(v0, v1, v2);
#endif

    // ; Calculate address of first scanline in colour and depth buffers
	// ;
    // mov		esi,work_main_y
    esi.v = work_main_y;
    // mov		eax,work.colour.base
    eax.v = WORK_COLOUR_BASE;
    // dec		esi
    esi.v--;
    // mov		ebx,work.colour.stride_b
    ebx.v = work.colour.stride_b;
    // mov		ecx,work.depth.base
    ecx.v = WORK_DEPTH_BASE;
    // mov		edx,work.depth.stride_b
    edx.v = work.depth.stride_b;
    // imul	ebx,esi
    ebx.int_val *= esi.int_val;
    // imul	edx,esi
    edx.int_val *= esi.int_val;
    // add		eax,ebx
    eax.v += ebx.v;
    // add		ecx,edx
    ecx.v += edx.v;
    // dec		eax
    eax.v--;
    // sub		ecx,2
    ecx.v -= 2;
    // mov		workspace.scanAddress,eax
    workspace.scanAddress = eax.v;
    // mov		workspace.depthAddress,ecx
    workspace.depthAddress = ecx.v;

    // ; Swap integer and fractional parts of major edge starting value and delta and z gradient
	// ; Copy some values into perspective texture mappng workspace
	// ; Calculate offset of starting pixel in texture map
	// ;
    // mov		eax,work_main_i
    eax.v = work_main_i;
    // mov		ebx,work_main_d_i
    ebx.v = work_main_d_i;
    // ror		eax,16
    ROR16(eax);
    // cmp		ebx,80000000h
    CMP(ebx.v, 0x80000000);
    // adc		ebx,-1
    ADC(ebx.v, -1);
    // mov		ecx,work_pz_grad_x
    ecx.v = work_pz_grad_x;
    // ror		ebx,16
    ROR16(ebx);
    // cmp		ecx,80000000h
    CMP(ecx.v, 0x80000000);
    // adc		ecx,-1
    ADC(ecx.v, -1);
    // mov		work_main_i,eax
    work_main_i = eax.v;
    // ror		ecx,16
    ROR16(ecx);
    // mov		al,byte ptr work.awsl.u_current
    eax.l = work.awsl.u_current;
    // mov		ah,byte ptr work.awsl.v_current
    eax.h = work.awsl.v_current;
    // mov		work_main_d_i,ebx
    work_main_d_i = ebx.v;
    // shl		al,2
    eax.l <<= 2;
    // mov		work.tsl.dz,ecx
    work.tsl.dz = ecx.v;
    // shr		eax,2
    eax.v >>= 2;
    // mov		ebx,work.pq.grad_x
    ebx.v = work.pq.grad_x;
    // and		eax,63*65
    eax.v &= 63*65;
    // mov		work.tsl.ddenominator,ebx
    work.tsl.ddenominator = ebx.v;
    // mov		work.tsl.source,eax
    work.tsl.source = eax.v;
    // mov		eax,work.tsl.direction
    eax.v = work.tsl.direction;

    // ; Check scan direction and use appropriate rasteriser
	// ;
    // test	eax,eax
    // jnz		reversed
    if (eax.v != 0) {
        goto reversed;
    }
    // call    TrapeziumRender_ZPTB_I8_D16_64_f
    TrapeziumRender_ZPT_I8_D16(DIR_F, eTrapezium_render_size_64, eFog_no, eBlend_yes);
    // mov		eax,work_bot_i
    eax.v = work_bot_i;
    // mov		ebx,work_bot_d_i
    ebx.v = work_bot_d_i;
    // mov		ecx,work_bot_count
    ecx.v = work_bot_count;
    // mov		work_top_i,eax
    work_top_i = eax.v;
    // mov		work_top_d_i,ebx
    work_top_d_i = ebx.v;
    // mov		work_top_count,ecx
    work_top_count = ecx.v;
    // call    TrapeziumRender_ZPTB_I8_D16_64_f
   TrapeziumRender_ZPT_I8_D16(DIR_F, eTrapezium_render_size_64, eFog_no, eBlend_yes);
    // ret
    return;

reversed:

    // call    TrapeziumRender_ZPTB_I8_D16_64_b
   TrapeziumRender_ZPT_I8_D16(DIR_B, eTrapezium_render_size_64, eFog_no, eBlend_yes);
    // mov		eax,work_bot_i
    eax.v = work_bot_i;
    // mov		ebx,work_bot_d_i
    ebx.v = work_bot_d_i;
    // mov		ecx,work_bot_count
    ecx.v = work_bot_count;
    // mov		work_top_i,eax
    work_top_i = eax.v;
    // mov		work_top_d_i,ebx
    work_top_d_i = ebx.v;
    // mov		work_top_count,ecx
    work_top_count = ecx.v;
    // call    TrapeziumRender_ZPTB_I8_D16_64_b
    TrapeziumRender_ZPT_I8_D16(DIR_B, eTrapezium_render_size_64, eFog_no, eBlend_yes);

}

void BR_ASM_CALL TriangleRender_ZPTIB_I8_D16_128(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIB_I8_D16_128_FLAT(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTB_I8_D16_128(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIB_I8_D16_256(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIB_I8_D16_256_FLAT(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTB_I8_D16_256(brp_block *block, ...) {
    va_list     va;
    va_start(va, block);
    brp_vertex *v0;
    brp_vertex *v1;
    brp_vertex *v2;

	v0 = va_arg(va, brp_vertex *);
    v1 = va_arg(va, brp_vertex *);
    v2 = va_arg(va, brp_vertex *);
    va_end(va);

#if ENABLE_PERSPECTIVE_CHEAT_FOR_BLEND==1
    TriangleSetup_ZPT(v0, v1, v2);
    // jc TriangleRasterise_ZTB_I8_D16_256
    if (x86_state.cf) {
        va_list l;
        TriangleRender_ZTB_I8_D16_POW2(block, 8, 1, l);
        return;
    }
#else
    TriangleSetup_ZPT_NOCHEAT(v0, v1, v2);
#endif

    // ; Calculate address of first scanline in colour and depth buffers
	// ;
    // mov		esi,work_main_y
    esi.v = work_main_y;
    // mov		eax,work.colour.base
    eax.v = WORK_COLOUR_BASE;
    // dec		esi
    esi.v--;
    // mov		ebx,work.colour.stride_b
    ebx.v = work.colour.stride_b;
    // mov		ecx,work.depth.base
    ecx.v = WORK_DEPTH_BASE;
    // mov		edx,work.depth.stride_b
    edx.v = work.depth.stride_b;
    // imul	ebx,esi
    ebx.int_val *= esi.int_val;
    // imul	edx,esi
    edx.int_val *= esi.int_val;
    // add		eax,ebx
    eax.v += ebx.v;
    // add		ecx,edx
    ecx.v += edx.v;
    // dec		eax
    eax.v--;
    // sub		ecx,2
    ecx.v -= 2;
    // mov		workspace.scanAddress,eax
    workspace.scanAddress = eax.v;
    // mov		workspace.depthAddress,ecx
    workspace.depthAddress = ecx.v;

    // ; Swap integer and fractional parts of major edge starting value and delta and z gradient
	// ; Copy some values into perspective texture mappng workspace
	// ; Calculate offset of starting pixel in texture map
	// ;
    // mov		eax,work_main_i
    eax.v = work_main_i;
    // mov		ebx,work_main_d_i
    ebx.v = work_main_d_i;
    // ror		eax,16
    ROR16(eax);
    // cmp		ebx,80000000h
    CMP(ebx.v, 0x80000000);
    // adc		ebx,-1
    ADC(ebx.v, -1);
    // mov		ecx,work_pz_grad_x
    ecx.v = work_pz_grad_x;
    // ror		ebx,16
    ROR16(ebx);
    // cmp		ecx,80000000h
    CMP(ecx.v, 0x80000000);
    // adc		ecx,-1
    ADC(ecx.v, -1);
    // mov		work_main_i,eax
    work_main_i = eax.v;
    // ror		ecx,16
    ROR16(ecx);
    // mov		work_main_d_i,ebx
    work_main_d_i = ebx.v;
    // xor eax,eax
    eax.v = 0;
    // mov		work.tsl.dz,ecx
    work.tsl.dz = ecx.v;
    // mov		al,byte ptr work.awsl.u_current
    eax.l = work.awsl.u_current;
    // mov		ebx,work.pq.grad_x
    ebx.v = work.pq.grad_x;
    // mov		ah,byte ptr work.awsl.v_current
    eax.h = work.awsl.v_current;
    // mov		work.tsl.ddenominator,ebx
    work.tsl.ddenominator = ebx.v;
    // mov		work.tsl.source,eax
    work.tsl.source = eax.v;
    // mov		eax,work.tsl.direction
    eax.v = work.tsl.direction;

    // ; Check scan direction and use appropriate rasteriser
	// ;
    // test	eax,eax
    // jnz		reversed
    if (eax.v != 0) {
        goto reversed;
    }
    // call    TrapeziumRender_ZPT_I8_D16_256_f
    TrapeziumRender_ZPT_I8_D16(DIR_F, eTrapezium_render_size_256, eFog_no, eBlend_yes);
    // mov		eax,work_bot_i
    eax.v = work_bot_i;
    // mov		ebx,work_bot_d_i
    ebx.v = work_bot_d_i;
    // mov		ecx,work_bot_count
    ecx.v = work_bot_count;
    // mov		work_top_i,eax
    work_top_i = eax.v;
    // mov		work_top_d_i,ebx
    work_top_d_i = ebx.v;
    // mov		work_top_count,ecx
    work_top_count = ecx.v;
    // call    TrapeziumRender_ZPT_I8_D16_256_f
   TrapeziumRender_ZPT_I8_D16(DIR_F, eTrapezium_render_size_256, eFog_no, eBlend_yes);
    // ret
    return;

reversed:

    // call    TrapeziumRender_ZPT_I8_D16_64_b
   TrapeziumRender_ZPT_I8_D16(DIR_B, eTrapezium_render_size_256, eFog_no, eBlend_yes);
    // mov		eax,work_bot_i
    eax.v = work_bot_i;
    // mov		ebx,work_bot_d_i
    ebx.v = work_bot_d_i;
    // mov		ecx,work_bot_count
    ecx.v = work_bot_count;
    // mov		work_top_i,eax
    work_top_i = eax.v;
    // mov		work_top_d_i,ebx
    work_top_d_i = ebx.v;
    // mov		work_top_count,ecx
    work_top_count = ecx.v;
    // call    TrapeziumRender_ZPT_I8_D16_64_b
    TrapeziumRender_ZPT_I8_D16(DIR_B, eTrapezium_render_size_256, eFog_no, eBlend_yes);
}

void BR_ASM_CALL TriangleRender_ZPTIB_I8_D16_1024(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIB_I8_D16_1024_FLAT(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTB_I8_D16_1024(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIFB_I8_D16_32(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIFB_I8_D16_32_FLAT(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTFB_I8_D16_32(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIFB_I8_D16_64(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIFB_I8_D16_64_FLAT(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTFB_I8_D16_64(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIFB_I8_D16_128(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIFB_I8_D16_128_FLAT(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTFB_I8_D16_128(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIFB_I8_D16_256(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIFB_I8_D16_256_FLAT(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTFB_I8_D16_256(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIFB_I8_D16_1024(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTIFB_I8_D16_1024_FLAT(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZPTFB_I8_D16_1024(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
