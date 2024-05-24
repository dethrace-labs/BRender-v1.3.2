#include "brender.h"
#include "priminfo.h"
#include "fpsetup.h"
#include "pfpsetup.h"
#include "x86emu.h"
#include "work.h"
#include <stdio.h>

#define DIR_F 0
#define DIR_B 1

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

void BR_ASM_CALL TriangleRender_ZT_I8_D16_POW2(brp_block *block, int pow2, int skip_setup, va_list va);
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

void ScanlineRender_ZPT_I8_D16(int size, int dirn, int udirn, int vdirn, int fogging, int blend) {
    // ; Make temporary copies of parameters that change
	// ;

    // mov		edx,work.pu.current
    edx.uint_val = work.pu.current;
    // mov		esi,work.pu.grad_x
    esi.uint_val = work.pu.grad_x;

    if (udirn == eScan_direction_i) {
        // mov		ebx,work.pq.current
        ebx.uint_val = work.pq.current;
        // mov		ecx,work.pq.grad_x
        ecx.uint_val = work.pq.grad_x;
        // sub		edx,ebx				; Move error into the range -1..0
        edx.uint_val -= ebx.uint_val;
        // sub		esi,ecx
        esi.uint_val -= ecx.uint_val;
    }

     // mov		ebp,work.pv.current
    ebp.uint_val = work.pv.current;
    // mov		edi,work.pv.grad_x
    edi.uint_val = work.pv.grad_x;

    // ifidni <vdirn>,<i>
    if (vdirn == eScan_direction_i) {
        // ifdifi <udirn>,<i>
        if (udirn != eScan_direction_i) {
            // mov		ebx,work.pq.current
            ebx.uint_val = work.pq.current;
            // mov		ecx,work.pq.grad_x
            ecx.uint_val = work.pq.grad_x;
        }

        // sub		ebp,ebx				; Move error into the range -1..0
        ebp.uint_val -= ebx.uint_val;
        // sub		edi,ecx
        edi.uint_val -= ecx.uint_val;

    }

    // mov		work.tsl.u_numerator, edx
    work.tsl.u_numerator = edx.uint_val;
    // mov		work.tsl.du_numerator, esi
    work.tsl.du_numerator = esi.uint_val;
    // mov		work.tsl.v_numerator, ebp
    work.tsl.v_numerator = ebp.uint_val;
    // mov		work.tsl.dv_numerator, edi
    work.tsl.dv_numerator = edi.uint_val;
    // mov		esi,work.texture.base
    esi.ptr_val = work.texture.base;
    // mov		eax,work.tsl.source
    eax.uint_val = work.tsl.source;
    // mov		edi,work.tsl.start
    edi.ptr_val = work.tsl.start;
    // mov		ebp,work.tsl.zstart
    ebp.ptr_val = work.tsl.zstart;
    // mov		ebx,work_pz_current
    ebx.uint_val = work_pz_current;
    // mov		edx,work.pq.current
    edx.uint_val = work.pq.current;
    // ror		ebx,16					; Swap z words
    ROR16(ebx);
    // mov		work.tsl.denominator,edx
    work.tsl.denominator = edx.uint_val;
    // mov		work.tsl.z,ebx
    work.tsl.z = ebx.uint_val;
    // mov		work.tsl.dest,edi
    work.tsl.dest = edi.ptr_val;

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
    edx.short_val[0] = *((uint16_t*)ebp.ptr_val);
    // mov		cl,[eax+esi]
    ecx.bytes[0] = ((char*)esi.ptr_val)[eax.uint_val];

    // mov		dh,[ebp+1]
    // no-op - already read both depth bytes
    // mov		edi,work.tsl.dest
    edi.ptr_val = work.tsl.dest;

    // cmp		bx,dx
    // ja		nodraw
    if (ebx.short_val[0] > edx.short_val[0]) {
        goto nodraw;
    }

	// ; Test for transparency
	// ;
    // test	cl,cl
    // jz		nodraw
    if (ecx.bytes[0] == 0) {
        goto nodraw;
    }

    // ifidni <fogging>,<F>
    if (fogging == 1) {
        // not implemented
        BrAbort();
        // ifidni <blend>,<B>
        if (blend == 1) {
            // not implemented

        }
    } else if (blend == 1) {
        BrAbort();
    } else {
        // ; Store texel and z
	    // ;
        // mov     [ebp],bx
        *((uint16_t*)ebp.ptr_val) = ebx.short_val[0];
        // mov     [edi],cl
        *((uint8_t*)edi.ptr_val) = ecx.bytes[0];
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
    eax.uint_val <<= params[size].pre;
    // mov		ecx,work.tsl._end
    ecx.ptr_val = work.tsl.end;
    // ; Update destinations and check for end of scan
    // ;
    // inc_&dirn	edi
    if (dirn == DIR_F) {
        edi.ptr_val++;
    } else {
        edi.ptr_val--;
    }
    // add_&dirn	ebp,2
    if (dirn == DIR_F) {
        ebp.ptr_val += 2;
    } else {
        ebp.ptr_val -= 2;
    }
    // cmp		edi,ecx
    // jg_&dirn    ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_done
    if (dirn == DIR_F && edi.ptr_val > ecx.ptr_val || dirn == DIR_B && edi.ptr_val < ecx.ptr_val) {
        return;
    }


    // ; Interpolate z
    // ;
    // mov		ecx,work.tsl.dz
    ecx.uint_val = work.tsl.dz;
    // mov		work.tsl.dest,edi
    work.tsl.dest = edi.ptr_val;
    // add_&dirn	ebx,ecx
    if (dirn == DIR_F) {
    ADD_AND_SET_CF(ebx.uint_val, ecx.uint_val);
    } else {
        SUB_AND_SET_CF(ebx.uint_val, ecx.uint_val);
    }
    // mov		work.tsl.zdest,ebp
    work.tsl.zdest = ebp.ptr_val;
    // adc_&dirn	ebx,0		; carry into integer part of z
    if (dirn == DIR_F) {
        ADC(ebx.uint_val, 0);
    } else {
        SBB(ebx.uint_val, 0);
    }

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
    edx.uint_val = work.tsl.denominator;
    // mov		work.tsl.z,ebx
    work.tsl.z = ebx.uint_val;
    // mov		ebp,work.tsl.ddenominator
    ebp.uint_val = work.tsl.ddenominator;
    // mov		ebx,work.tsl.u_numerator
    ebx.uint_val = work.tsl.u_numerator;
    // mov		edi,work.tsl.du_numerator
    edi.uint_val = work.tsl.du_numerator;
    // ; Interpolate u numerator and denominator
    // ;
    // add_&dirn	edx,ebp
    if (dirn == DIR_F) {
        edx.uint_val += ebp.uint_val;
    } else {
        edx.uint_val -= ebp.uint_val;
    }
    // add_&dirn	ebx,edi
    if (dirn == DIR_F) {
        ebx.uint_val += edi.uint_val;
    } else {
        ebx.uint_val -= edi.uint_val;
    }
    // mov		ecx,work.tsl.v_numerator
    ecx.uint_val = work.tsl.v_numerator;

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
        eax.bytes[0] -= params[size].decu;
        // add		edi,ebp
        edi.uint_val += ebp.uint_val;
        // add		ebx,edx
        ebx.uint_val += edx.uint_val;
        // jl		deculoop
        if (ebx.int_val < 0) {
            goto deculoop;
        }
        // mov		work.tsl.du_numerator,edi
        work.tsl.du_numerator = edi.uint_val;
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
        eax.bytes[0] += params[size].incu;
        // sub		edi,ebp
        edi.uint_val -= ebp.uint_val;
        // sub		ebx,edx
        ebx.uint_val -= edx.uint_val;
        // cmp		ebx,edx
        // jge		inculoop
        if (ebx.int_val > edx.int_val) {
            goto inculoop;
        }
        // mov		work.tsl.du_numerator,edi
        work.tsl.du_numerator = edi.uint_val;
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
            eax.bytes[0] += params[size].incu;
        }
        // else
		// decu
        else {
            eax.bytes[0] -= params[size].decu;
        }
        // endif

        // sub_&udirn	edi,ebp
        // sub_&udirn	ebx,edx
        if (udirn == eScan_direction_i) {
            edi.uint_val -= ebp.uint_val;
            SUB_AND_SET_CF(ebx.uint_val, edx.uint_val);
        } else if (udirn == eScan_direction_b) {
            edi.uint_val += ebp.uint_val;
            ADD_AND_SET_CF(ebx.uint_val, edx.uint_val);
        } else if (udirn == eScan_direction_d) {
            edi.uint_val += ebp.uint_val;
            ADD_AND_SET_CF(ebx.uint_val, edx.uint_val);
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
        work.tsl.du_numerator = edi.uint_val;
		// vslot

    }


doneu:
    // mov		edi,work.tsl.dv_numerator
    edi.uint_val = work.tsl.dv_numerator;
    // mov		work.tsl.u_numerator,ebx
    work.tsl.u_numerator = ebx.uint_val;
    // ; Interpolate v numerator
    // ;
    // add_&dirn	ecx,edi
    if (dirn == DIR_F) {
        ecx.uint_val += edi.uint_val;
    } else {
        ecx.uint_val -= edi.uint_val;
    }
    // mov		work.tsl.denominator,edx
    work.tsl.denominator = edx.uint_val;


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
        eax.bytes[1] -= params[size].decv;
        // add		edi,ebp
        edi.uint_val += ebp.uint_val;
        // add		ecx,edx
        ecx.uint_val += edx.uint_val;
        // jl		decvloop
        if (ecx.int_val < 0) {
            goto decvloop;
        }
        // mov		work.tsl.dv_numerator,edi
        work.tsl.dv_numerator = edi.uint_val;
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
        eax.bytes[1] += params[size].incv;
        // sub		edi,ebp
        edi.uint_val -= ebp.uint_val;
        // sub		ecx,edx
        ecx.uint_val -= edx.uint_val;
        // cmp		ecx,edx
        // jge		incvloop
        if (ecx.int_val >= edx.int_val) {
            goto incvloop;
        }
        // mov		work.tsl.dv_numerator,edi
        work.tsl.dv_numerator = edi.uint_val;
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
            eax.bytes[1] += params[size].incv;
        } else {
            eax.bytes[1] -= params[size].decv;
        }

        // sub_&vdirn	edi,ebp
        // sub_&vdirn	ecx,edx
        // jge_&vdirn	stepvloop
        if (vdirn == eScan_direction_i) {
            edi.uint_val -= ebp.uint_val;
            SUB_AND_SET_CF(ecx.uint_val, edx.uint_val);
            if (ecx.int_val >= 0) {
                goto stepvloop;
            }
        } else if (vdirn == eScan_direction_b) {
            edi.uint_val += ebp.uint_val;
            ADD_AND_SET_CF(ecx.uint_val, edx.uint_val);
            if (ecx.int_val <= 0) {
                goto stepvloop;
            }
        } else if (vdirn == eScan_direction_d) {
            edi.uint_val += ebp.uint_val;
            ADD_AND_SET_CF(ecx.uint_val, edx.uint_val);
            if (x86_state.cf == 0) {
                goto stepvloop;
            }
        }

		// mov		work.tsl.dv_numerator,edi
        work.tsl.dv_numerator = edi.uint_val;
		// vslot
        // no-op
    }

donev:

	// ; Fix wrapping of source offset after modification
	// ;
    // post1
    eax.uint_val >>= params[size].post1;
    // mov	work.tsl.v_numerator,ecx
    work.tsl.v_numerator = ecx.uint_val;

    // post2
    eax.uint_val &= params[size].post2;
    // mov		ebp,work.tsl.zdest
    ebp.ptr_val = work.tsl.zdest;

    // mov		ebx,work.tsl.z
    ebx.uint_val = work.tsl.z;
    // jmp		next_pixel
    goto next_pixel;
}

void ScanlineRender_ZPTI_I8_D16(int size, int dirn, int udirn, int vdirn, int fogging, int blend) {
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


void TrapeziumRender_ZPT_I8_D16(int dirn, int size_param) {

    // mov		ebx,work_top_count	; check for empty trapezium
    ebx.uint_val = work_top_count;
    // test	ebx,ebx
    // jl		done_trapezium
    if (ebx.int_val < 0) {
        goto done_trapezium;
    }
    // mov		edi,work_top_i
    edi.uint_val = work_top_i;
    // mov		ebp,work_main_i
    ebp.uint_val = work_main_i;
    // shr		edi,16				; get integer part of end of first scanline
    edi.uint_val >>= 16;
    // and		ebp,0ffffh			; get integer part of start of first scanline
    ebp.uint_val &= 0xffff;

scan_loop:

	// ; Calculate start and end addresses for next scanline
	// ;
    // cmp		ebp,edi				; calculate pixel count
    // jg_&dirn	no_pixels
    if (dirn == DIR_F && ebp.int_val > edi.int_val || dirn == DIR_B && ebp.int_val < edi.int_val){
        goto no_pixels;
    }
    // mov		eax,workspace.scanAddress
    eax.uint_val = workspace.scanAddress;
    // mov		ebx,workspace.depthAddress
    ebx.uint_val = workspace.depthAddress;
    // add		edi,eax				; calculate end colour buffer pointer
    edi.uint_val += eax.uint_val;
    // add		eax,ebp				; calculate start colour buffer pointer
    eax.uint_val += ebp.uint_val;
    // mov		work.tsl._end,edi
    work.tsl.end =  ((char*)work.colour.base) + edi.uint_val;
    // lea		ebx,[ebx+ebp*2]		; calculate start depth buffer pointer
    ebx.uint_val += ebp.uint_val * 2;
    // mov		work.tsl.start,eax
    work.tsl.start = ((char*)work.colour.base) + eax.uint_val;
    // mov		work.tsl.zstart,ebx
    work.tsl.zstart = ((char*)work.depth.base) + ebx.uint_val;

    // ; Fix up the error values
	// ;
     // mov		eax,work.tsl.source
    eax.uint_val = work.tsl.source;
    // mov		ebx,work.pq.current
    ebx.uint_val = work.pq.current;
    // mov		ecx,work.pq.grad_x
    ecx.uint_val = work.pq.grad_x;
    // mov		edx,work.pq.d_nocarry
    edx.uint_val = work.pq.d_nocarry;
    // pre
    eax.uint_val <<= params[size_param].pre;
    // mov		ebp,work.pu.current
    ebp.uint_val = work.pu.current;
    // mov		edi,work.pu.grad_x
    edi.uint_val = work.pu.grad_x;
    // mov		esi,work.pu.d_nocarry
    esi.uint_val = work.pu.d_nocarry;
    // cmp		ebp,ebx
    // jl		uidone
    if (ebp.int_val < ebx.int_val) {
        goto uidone;
    }

uiloop:
    // incu							; work.tsl.source = incu(work.tsl.source)
    eax.bytes[0] += params[size_param].incu;
    // sub		edi,ecx					; work.pu.grad_x -= work.pq.grad_x
    edi.uint_val -= ecx.uint_val;
    // sub		esi,edx					; work.pu.d_nocarry -= work.pq.d_nocarry
    esi.uint_val -= edx.uint_val;
    // sub		ebp,ebx					; work.pu.current -= work.pq.current
    ebp.uint_val -= ebx.uint_val;
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
    eax.bytes[0] -= params[size_param].decu;
    // add		edi,ecx					; work.pu.grad_x += work.pq.grad_x
    edi.uint_val += ecx.uint_val;
    // add		esi,edx					; work.pu.d_nocarry += work.pq.d_nocarry
    esi.uint_val += edx.uint_val;
    // add		ebp,ebx					; work.pu.current += work.pq.current
    ADD_AND_SET_CF(ebp.uint_val, ebx.uint_val);
    // uslot
    // no-op
    // jl		udloop
    if (x86_state.sf != x86_state.of) {
        goto udloop;
    }
uddone:
    // mov		work.pu.current,ebp
    work.pu.current = ebp.uint_val;
    // mov		work.pu.grad_x,edi
    work.pu.grad_x = edi.uint_val;
    // mov		work.pu.d_nocarry,esi
    work.pu.d_nocarry = esi.uint_val;
    // mov		ebp,work.pv.current
    ebp.uint_val = work.pv.current;
    // mov		edi,work.pv.grad_x
    edi.uint_val = work.pv.grad_x;
    // mov		esi,work.pv.d_nocarry
    esi.uint_val = work.pv.d_nocarry;
    // cmp		ebp,ebx
    // jl		vidone
    if (ebp.int_val < ebx.int_val) {
        goto vidone;
    }
viloop:
    // incv							; work.tsl.source = incv(work.tsl.source)
    eax.bytes[1] += params[size_param].incv;
    // sub		edi,ecx					; work.pv.grad_x -= work.pq.grad_x
    edi.uint_val -= ecx.uint_val;
    // sub		esi,edx					; work.pv.d_nocarry -= work.pq.d_nocarry
    esi.uint_val -= edx.uint_val;
    // sub		ebp,ebx					; work.pv.current -= work.pq.current
    ebp.uint_val -= ebx.uint_val;
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
    eax.bytes[1] -= params[size_param].decv;
    // add		edi,ecx					; work.pv.grad_x += work.pq.grad_x
    edi.uint_val += ecx.uint_val;
    // add		esi,edx					; work.pv.d_nocarry += work.pq.d_nocarry
    esi.uint_val += edx.uint_val;
    // add		ebp,ebx					; work.pv.current += work.pq.current
    ADD_AND_SET_CF(ebp.uint_val, ebx.uint_val);
    // uslot
    // no op
    // jl		vdloop
    if (x86_state.sf != x86_state.of) {
        goto vdloop;
    }
vddone:
    // ; Select scanline loop and jump to it
	// ;
    // post1
    eax.uint_val >>= params[size_param].post1;
    // mov		work.pv.current,ebp
    work.pv.current = ebp.uint_val;
    // post2
    eax.uint_val &= params[size_param].post2;
    // mov		work.pv.grad_x,edi
    work.pv.grad_x = edi.uint_val;
    // mov		work.pv.d_nocarry,esi
    work.pv.d_nocarry = esi.uint_val;
    // mov		work.tsl.source,eax
    work.tsl.source = eax.uint_val;
    // mov		esi,work.pu.grad_x
    esi.uint_val = work.pu.grad_x;
    // mov		ebp,work.pq.grad_x
    ebp.uint_val = work.pq.grad_x;
    // test	ebp,ebp
    // jl_&dirn	qd
    if (dirn == DIR_F && ebp.int_val < 0 || dirn == DIR_B && ebp.int_val > 0) {
        goto qd;
    }
    // test	esi,esi
    // jle_&dirn	qi_ud
    if (dirn == DIR_F && esi.int_val <= 0 || dirn == DIR_B && esi.int_val >= 0) {
        goto qi_ud;
    }
    // test	edi,edi
    // jle_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ui_vd
    if (dirn == DIR_F && edi.int_val <= 0 || dirn == DIR_B && edi.int_val >= 0) {
        ScanlineRender_ZPT_I8_D16(size_param, dirn, eScan_direction_i, eScan_direction_d, 0, 0);
        goto ScanlineRender_ZPT_done;
    }
    // jmp		ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ui_vi
    ScanlineRender_ZPT_I8_D16(size_param, dirn, eScan_direction_i, eScan_direction_i, 0, 0);
    goto ScanlineRender_ZPT_done;
qi_ud:
    // test	edi,edi
    // jle_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ud_vd
    if (dirn == DIR_F && edi.int_val <= 0 || dirn == DIR_B && edi.int_val >= 0) {
        ScanlineRender_ZPT_I8_D16(size_param, dirn, eScan_direction_d, eScan_direction_d, 0, 0);
        goto ScanlineRender_ZPT_done;
    }
    // jmp		ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ud_vi
    ScanlineRender_ZPT_I8_D16(size_param, dirn, eScan_direction_d, eScan_direction_i, 0, 0);
    goto ScanlineRender_ZPT_done;
qd:
    // test	esi,esi
    // jle_&dirn	qd_ud
    if (dirn == DIR_F && esi.int_val <= 0 || dirn == DIR_B && esi.int_val >= 0) {
        goto qd_ud;
    }
qd_ui:
    // test	edi,edi
    // jg_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ui_vi
    if (dirn == DIR_F && edi.int_val > 0 || dirn == DIR_B && edi.int_val < 0) {
        ScanlineRender_ZPT_I8_D16(size_param, dirn, eScan_direction_i, eScan_direction_i, 0, 0);
        goto ScanlineRender_ZPT_done;
    }
    // cmp		edi,ebp
    // jle_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ui_vd
    if (dirn == DIR_F && edi.int_val <= ebp.int_val || dirn == DIR_B && edi.int_val >= ebp.int_val) {
        ScanlineRender_ZPT_I8_D16(size_param, dirn, eScan_direction_i, eScan_direction_d, 0, 0);
        goto ScanlineRender_ZPT_done;
    }
    // jmp		ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ub_vb
    ScanlineRender_ZPT_I8_D16(size_param, dirn, eScan_direction_b, eScan_direction_b, 0, 0);
    goto ScanlineRender_ZPT_done;
qd_ud:
    // cmp		esi,ebp
    // jg_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ub_vb
    if (dirn == DIR_F && esi.int_val > ebp.int_val || dirn == DIR_B && esi.int_val < ebp.int_val) {
        ScanlineRender_ZPT_I8_D16(size_param, dirn, eScan_direction_b, eScan_direction_b, 0, 0);
        goto ScanlineRender_ZPT_done;
    }
    // test	edi,edi
    // jg_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ud_vi
    if (dirn == DIR_F && edi.int_val > 0 || dirn == DIR_B && edi.int_val < 0) {
        ScanlineRender_ZPT_I8_D16(size_param, dirn, eScan_direction_d, eScan_direction_i, 0, 0);
        goto ScanlineRender_ZPT_done;
    }
    // cmp		edi,ebp
    // jle_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ud_vd
    if (dirn == DIR_F && edi.int_val <= ebp.int_val || dirn == DIR_B && edi.int_val >= ebp.int_val) {
        ScanlineRender_ZPT_I8_D16(size_param, dirn, eScan_direction_d, eScan_direction_d, 0, 0);
        goto ScanlineRender_ZPT_done;
    }
    // jmp		ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ub_vb
    ScanlineRender_ZPT_I8_D16(size_param, dirn, eScan_direction_b, eScan_direction_b, 0, 0);

    // ; Scanline loops return here when finished
	// ;
ScanlineRender_ZPT_done:
no_pixels:
    // ; Updates for next scanline:
	// ;
    // mov		eax,workspace.scanAddress
    eax.uint_val = workspace.scanAddress;
    // mov		ebx,work.colour.stride_b
    ebx.uint_val = work.colour.stride_b;
    // mov		ecx,workspace.depthAddress
    ecx.uint_val = workspace.depthAddress;
    // mov		edx,work.depth.stride_b
    edx.uint_val = work.depth.stride_b;
    // add		eax,ebx				; move down one line in colour buffer
    eax.uint_val += ebx.uint_val;
    // add		ecx,edx				; move down one line in depth buffer
    ecx.uint_val += edx.uint_val;
    // mov		workspace.scanAddress,eax
    workspace.scanAddress = eax.uint_val;
    // mov		workspace.depthAddress,ecx
    workspace.depthAddress = ecx.uint_val;
    // mov		ebp,work_main_i
    ebp.uint_val = work_main_i;
    // mov		eax,work_main_d_i
    eax.uint_val = work_main_d_i;
    // add		ebp,eax				; step major edge
    ADD_AND_SET_CF(ebp.uint_val, eax.uint_val);
    // jc		carry
    if (x86_state.cf) {
        goto carry;
    }
    // mov		edi,work_top_i
    edi.uint_val = work_top_i;
    // mov		work_main_i,ebp
    work_main_i = ebp.uint_val;
    // mov		eax,work_top_d_i
    eax.uint_val = work_top_d_i;
    // add		edi,eax				; step minor edge
    edi.uint_val += eax.uint_val;
    // mov		eax,work.pq.current
    eax.uint_val = work.pq.current;
    // mov		work_top_i,edi
    work_top_i = edi.uint_val;
    // mov		ebx,work.pq.d_nocarry
    ebx.uint_val = work.pq.d_nocarry;
    // shr		edi,16				; get integer part of end of next scanline
    edi.uint_val >>= 16;
    // add		eax,ebx				; step q according to carry from major edge
    eax.uint_val += ebx.uint_val;
    // and		ebp,0ffffh			; get integer part of start of next scanline
    ebp.uint_val &= 0xffff;
    // mov		work.pq.current,eax
    work.pq.current = eax.uint_val;
    // mov		eax,work_pz_current
    eax.uint_val = work_pz_current;
    // mov		ebx,work_pz_d_nocarry
    ebx.uint_val = work_pz_d_nocarry;
    // add		eax,ebx				; step z according to carry from major edge
    eax.uint_val += ebx.uint_val;
    // mov		ebx,work.pv.current
    ebx.uint_val = work.pv.current;
    // mov		work_pz_current,eax
    work_pz_current = eax.uint_val;
    // mov		eax,work.pu.current
    eax.uint_val = work.pu.current;
    // add		eax,work.pu.d_nocarry	; step u according to carry from major edge
    eax.uint_val += work.pu.d_nocarry;
    // add		ebx,work.pv.d_nocarry	; step v according to carry from major edge
    ebx.uint_val += work.pv.d_nocarry;
    // mov		work.pu.current,eax
    work.pu.current = eax.uint_val;
    // mov		ecx,work_top_count
    ecx.uint_val = work_top_count;
    // mov		work.pv.current,ebx
    work.pv.current = ebx.uint_val;
    // dec		ecx					; decrement line counter
    ecx.uint_val--;
    // mov		work_top_count,ecx
    work_top_count = ecx.uint_val;
    // jge		scan_loop
    if (ecx.int_val >= 0) {
        goto scan_loop;
    }
    // ret
    return;

carry:
    // adc		ebp,0
    ADC(ebp.uint_val, 0);
    // mov		edi,work_top_i
    edi.uint_val = work_top_i;
    // mov		work_main_i,ebp
    work_main_i = ebp.uint_val;
    // mov		eax,work_top_d_i
    eax.uint_val = work_top_d_i;
    // add		edi,eax				; step minor edge
    edi.uint_val += eax.uint_val;
    // mov		eax,work.pq.current
    eax.uint_val = work.pq.current;
    // mov		work_top_i,edi
    work_top_i = edi.uint_val;
    // mov		ebx,work.pq.d_carry
    ebx.uint_val = work.pq.d_carry;
    // shr		edi,16				; get integer part of end of next scanline
    edi.uint_val >>= 16;
    // add		eax,ebx				; step q according to carry from major edge
    eax.uint_val += ebx.uint_val;
    // and		ebp,0ffffh			; get integer part of start of next scanline
    ebp.uint_val &= 0xffff;
    // mov		work.pq.current,eax
    work.pq.current = eax.uint_val;
    // mov		eax,work_pz_current
    eax.uint_val = work_pz_current;
    // mov		ebx,work_pz_d_carry
    ebx.uint_val = work_pz_d_carry;
    // add		eax,ebx				; step z according to carry from major edge
    eax.uint_val += ebx.uint_val;
    // mov		ebx,work.pv.current
    ebx.uint_val = work.pv.current;
    // mov		work_pz_current,eax
    work_pz_current = eax.uint_val;
    // mov		eax,work.pu.current
    eax.uint_val = work.pu.current;
    // add		eax,work.pu.d_nocarry	; step u according to carry from major edge
    eax.uint_val += work.pu.d_nocarry;
    // add		ebx,work.pv.d_nocarry	; step v according to carry from major edge
    ebx.uint_val += work.pv.d_nocarry;
    // add		eax,work.pu.grad_x	; avoids the need to fixup nocarry and carry
    eax.uint_val += work.pu.grad_x;
    // add		ebx,work.pv.grad_x	; versions
    ebx.uint_val += work.pv.grad_x;
    // mov		work.pu.current,eax
    work.pu.current = eax.uint_val;
    // mov		ecx,work_top_count
    ecx.uint_val = work_top_count;
    // mov		work.pv.current,ebx
    work.pv.current = ebx.uint_val;
    // dec		ecx					; decrement line counter
    ecx.uint_val--;
    // mov		work_top_count,ecx
    work_top_count = ecx.uint_val;
    // jge		scan_loop
    if (ecx.int_val >= 0) {
        goto scan_loop;
    }

done_trapezium:

}

void TrapeziumRender_ZPTI_I8_D16(int dirn, int size_param) {

    // mov		ebx,work_top_count	; check for empty trapezium
    ebx.uint_val = work_top_count;
    // test	ebx,ebx
    // jl		done_trapezium
    if (ebx.int_val < 0) {
        goto done_trapezium;
    }
    // mov		edi,work_top_i
    edi.uint_val = work_top_i;
    // mov		ebp,work_main_i
    ebp.uint_val = work_main_i;
    // shr		edi,16				; get integer part of end of first scanline
    edi.uint_val >>= 16;
    // and		ebp,0ffffh			; get integer part of start of first scanline
    ebp.uint_val &= 0xffff;

scan_loop:

	// ; Calculate start and end addresses for next scanline
	// ;
    // cmp		ebp,edi				; calculate pixel count
    // jg_&dirn	no_pixels
    if (dirn == DIR_F && ebp.int_val > edi.int_val || dirn == DIR_B && ebp.int_val < edi.int_val){
        goto no_pixels;
    }
    // mov		eax,workspace.scanAddress
    eax.uint_val = workspace.scanAddress;
    // mov		ebx,workspace.depthAddress
    ebx.uint_val = workspace.depthAddress;
    // add		edi,eax				; calculate end colour buffer pointer
    edi.uint_val += eax.uint_val;
    // add		eax,ebp				; calculate start colour buffer pointer
    eax.uint_val += ebp.uint_val;
    // mov		work.tsl._end,edi
    work.tsl.end =  ((char*)work.colour.base) + edi.uint_val;
    // lea		ebx,[ebx+ebp*2]		; calculate start depth buffer pointer
    ebx.uint_val += ebp.uint_val * 2;
    // mov		work.tsl.start,eax
    work.tsl.start = ((char*)work.colour.base) + eax.uint_val;
    // mov		work.tsl.zstart,ebx
    work.tsl.zstart = ((char*)work.depth.base) + ebx.uint_val;

    // ; Fix up the error values
	// ;
     // mov		eax,work.tsl.source
    eax.uint_val = work.tsl.source;
    // mov		ebx,work.pq.current
    ebx.uint_val = work.pq.current;
    // mov		ecx,work.pq.grad_x
    ecx.uint_val = work.pq.grad_x;
    // mov		edx,work.pq.d_nocarry
    edx.uint_val = work.pq.d_nocarry;
    // pre
    eax.uint_val <<= params[size_param].pre;
    // mov		ebp,work.pu.current
    ebp.uint_val = work.pu.current;
    // mov		edi,work.pu.grad_x
    edi.uint_val = work.pu.grad_x;
    // mov		esi,work.pu.d_nocarry
    esi.uint_val = work.pu.d_nocarry;
    // cmp		ebp,ebx
    // jl		uidone
    if (ebp.int_val < ebx.int_val) {
        goto uidone;
    }

uiloop:
    // incu							; work.tsl.source = incu(work.tsl.source)
    eax.bytes[0] += params[size_param].incu;
    // sub		edi,ecx					; work.pu.grad_x -= work.pq.grad_x
    edi.uint_val -= ecx.uint_val;
    // sub		esi,edx					; work.pu.d_nocarry -= work.pq.d_nocarry
    esi.uint_val -= edx.uint_val;
    // sub		ebp,ebx					; work.pu.current -= work.pq.current
    ebp.uint_val -= ebx.uint_val;
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
    eax.bytes[0] -= params[size_param].decu;
    // add		edi,ecx					; work.pu.grad_x += work.pq.grad_x
    edi.uint_val += ecx.uint_val;
    // add		esi,edx					; work.pu.d_nocarry += work.pq.d_nocarry
    esi.uint_val += edx.uint_val;
    // add		ebp,ebx					; work.pu.current += work.pq.current
    ADD_AND_SET_CF(ebp.uint_val, ebx.uint_val);
    // uslot
    // no-op
    // jl		udloop
    if (x86_state.sf != x86_state.of) {
        goto udloop;
    }
uddone:
    // mov		work.pu.current,ebp
    work.pu.current = ebp.uint_val;
    // mov		work.pu.grad_x,edi
    work.pu.grad_x = edi.uint_val;
    // mov		work.pu.d_nocarry,esi
    work.pu.d_nocarry = esi.uint_val;
    // mov		ebp,work.pv.current
    ebp.uint_val = work.pv.current;
    // mov		edi,work.pv.grad_x
    edi.uint_val = work.pv.grad_x;
    // mov		esi,work.pv.d_nocarry
    esi.uint_val = work.pv.d_nocarry;
    // cmp		ebp,ebx
    // jl		vidone
    if (ebp.int_val < ebx.int_val) {
        goto vidone;
    }
viloop:
    // incv							; work.tsl.source = incv(work.tsl.source)
    eax.bytes[1] += params[size_param].incv;
    // sub		edi,ecx					; work.pv.grad_x -= work.pq.grad_x
    edi.uint_val -= ecx.uint_val;
    // sub		esi,edx					; work.pv.d_nocarry -= work.pq.d_nocarry
    esi.uint_val -= edx.uint_val;
    // sub		ebp,ebx					; work.pv.current -= work.pq.current
    ebp.uint_val -= ebx.uint_val;
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
    eax.bytes[1] -= params[size_param].decv;
    // add		edi,ecx					; work.pv.grad_x += work.pq.grad_x
    edi.uint_val += ecx.uint_val;
    // add		esi,edx					; work.pv.d_nocarry += work.pq.d_nocarry
    esi.uint_val += edx.uint_val;
    // add		ebp,ebx					; work.pv.current += work.pq.current
    ADD_AND_SET_CF(ebp.uint_val, ebx.uint_val);
    // uslot
    // no op
    // jl		vdloop
    if (x86_state.sf != x86_state.of) {
        goto vdloop;
    }
vddone:
    // ; Select scanline loop and jump to it
	// ;
    // post1
    eax.uint_val >>= params[size_param].post1;
    // mov		work.pv.current,ebp
    work.pv.current = ebp.uint_val;
    // post2
    eax.uint_val &= params[size_param].post2;
    // mov		work.pv.grad_x,edi
    work.pv.grad_x = edi.uint_val;
    // mov		work.pv.d_nocarry,esi
    work.pv.d_nocarry = esi.uint_val;
    // mov		work.tsl.source,eax
    work.tsl.source = eax.uint_val;
    // mov		esi,work.pu.grad_x
    esi.uint_val = work.pu.grad_x;
    // mov		ebp,work.pq.grad_x
    ebp.uint_val = work.pq.grad_x;
    // test	ebp,ebp
    // jl_&dirn	qd
    if (dirn == DIR_F && ebp.int_val < 0 || dirn == DIR_B && ebp.int_val > 0) {
        goto qd;
    }
    // test	esi,esi
    // jle_&dirn	qi_ud
    if (dirn == DIR_F && esi.int_val <= 0 || dirn == DIR_B && esi.int_val >= 0) {
        goto qi_ud;
    }
    // test	edi,edi
    // jle_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ui_vd
    if (dirn == DIR_F && edi.int_val <= 0 || dirn == DIR_B && edi.int_val >= 0) {
        ScanlineRender_ZPTI_I8_D16(size_param, dirn, eScan_direction_i, eScan_direction_d, 0, 0);
        goto ScanlineRender_ZPTI_done;
    }
    // jmp		ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ui_vi
    ScanlineRender_ZPTI_I8_D16(size_param, dirn, eScan_direction_i, eScan_direction_i, 0, 0);
    goto ScanlineRender_ZPTI_done;
qi_ud:
    // test	edi,edi
    // jle_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ud_vd
    if (dirn == DIR_F && edi.int_val <= 0 || dirn == DIR_B && edi.int_val >= 0) {
        ScanlineRender_ZPTI_I8_D16(size_param, dirn, eScan_direction_d, eScan_direction_d, 0, 0);
        goto ScanlineRender_ZPTI_done;
    }
    // jmp		ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ud_vi
    ScanlineRender_ZPTI_I8_D16(size_param, dirn, eScan_direction_d, eScan_direction_i, 0, 0);
    goto ScanlineRender_ZPTI_done;
qd:
    // test	esi,esi
    // jle_&dirn	qd_ud
    if (dirn == DIR_F && esi.int_val <= 0 || dirn == DIR_B && esi.int_val >= 0) {
        goto qd_ud;
    }
qd_ui:
    // test	edi,edi
    // jg_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ui_vi
    if (dirn == DIR_F && edi.int_val > 0 || dirn == DIR_B && edi.int_val < 0) {
        ScanlineRender_ZPTI_I8_D16(size_param, dirn, eScan_direction_i, eScan_direction_i, 0, 0);
        goto ScanlineRender_ZPTI_done;
    }
    // cmp		edi,ebp
    // jle_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ui_vd
    if (dirn == DIR_F && edi.int_val <= ebp.int_val || dirn == DIR_B && edi.int_val >= ebp.int_val) {
        ScanlineRender_ZPTI_I8_D16(size_param, dirn, eScan_direction_i, eScan_direction_d, 0, 0);
        goto ScanlineRender_ZPTI_done;
    }
    // jmp		ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ub_vb
    ScanlineRender_ZPTI_I8_D16(size_param, dirn, eScan_direction_b, eScan_direction_b, 0, 0);
    goto ScanlineRender_ZPTI_done;
qd_ud:
    // cmp		esi,ebp
    // jg_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ub_vb
    if (dirn == DIR_F && esi.int_val > ebp.int_val || dirn == DIR_B && esi.int_val < ebp.int_val) {
        ScanlineRender_ZPTI_I8_D16(size_param, dirn, eScan_direction_b, eScan_direction_b, 0, 0);
        goto ScanlineRender_ZPTI_done;
    }
    // test	edi,edi
    // jg_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ud_vi
    if (dirn == DIR_F && edi.int_val > 0 || dirn == DIR_B && edi.int_val < 0) {
        ScanlineRender_ZPTI_I8_D16(size_param, dirn, eScan_direction_d, eScan_direction_i, 0, 0);
        goto ScanlineRender_ZPTI_done;
    }
    // cmp		edi,ebp
    // jle_&dirn	ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ud_vd
    if (dirn == DIR_F && edi.int_val <= ebp.int_val || dirn == DIR_B && edi.int_val >= ebp.int_val) {
        ScanlineRender_ZPTI_I8_D16(size_param, dirn, eScan_direction_d, eScan_direction_d, 0, 0);
        goto ScanlineRender_ZPTI_done;
    }
    // jmp		ScanlineRender_ZPT&fogging&&blend&_I8_D16_&size&_&dirn&_ub_vb
    ScanlineRender_ZPTI_I8_D16(size_param, dirn, eScan_direction_b, eScan_direction_b, 0, 0);

    // ; Scanline loops return here when finished
	// ;
ScanlineRender_ZPTI_done:
no_pixels:
    // ; Updates for next scanline:
	// ;
    // mov		eax,workspace.scanAddress
    eax.uint_val = workspace.scanAddress;
    // mov		ebx,work.colour.stride_b
    ebx.uint_val = work.colour.stride_b;
    // mov		ecx,workspace.depthAddress
    ecx.uint_val = workspace.depthAddress;
    // mov		edx,work.depth.stride_b
    edx.uint_val = work.depth.stride_b;
    // add		eax,ebx				; move down one line in colour buffer
    eax.uint_val += ebx.uint_val;
    // add		ecx,edx				; move down one line in depth buffer
    ecx.uint_val += edx.uint_val;
    // mov		workspace.scanAddress,eax
    workspace.scanAddress = eax.uint_val;
    // mov		workspace.depthAddress,ecx
    workspace.depthAddress = ecx.uint_val;
    // mov		ebp,work_main_i
    ebp.uint_val = work_main_i;
    // mov		eax,work_main_d_i
    eax.uint_val = work_main_d_i;
    // add		ebp,eax				; step major edge
    ADD_AND_SET_CF(ebp.uint_val, eax.uint_val);
    // jc		carry
    if (x86_state.cf) {
        goto carry;
    }
    // mov		edi,work_top_i
    edi.uint_val = work_top_i;
    // mov		work_main_i,ebp
    work_main_i = ebp.uint_val;
    // mov		eax,work_top_d_i
    eax.uint_val = work_top_d_i;
    // add		edi,eax				; step minor edge
    edi.uint_val += eax.uint_val;
    // mov		eax,work.pq.current
    eax.uint_val = work.pq.current;
    // mov		work_top_i,edi
    work_top_i = edi.uint_val;
    // mov		ebx,work.pq.d_nocarry
    ebx.uint_val = work.pq.d_nocarry;
    // shr		edi,16				; get integer part of end of next scanline
    edi.uint_val >>= 16;
    // add		eax,ebx				; step q according to carry from major edge
    eax.uint_val += ebx.uint_val;
    // and		ebp,0ffffh			; get integer part of start of next scanline
    ebp.uint_val &= 0xffff;
    // mov		work.pq.current,eax
    work.pq.current = eax.uint_val;
    // mov		eax,work_pz_current
    eax.uint_val = work_pz_current;

    // mov		ebx,work_pi_current
    ebx.uint_val = work_pi_current;
    // add		eax,work_pz_d_nocarry	; step z according to carry from major edge
    eax.uint_val += work_pz_d_nocarry;
    // add		ebx,work_pi_d_nocarry	; step i according to carry from major edge
    ebx.uint_val += work_pi_d_nocarry;
    // mov		work_pz_current,eax
    work_pz_current = eax.uint_val;
    // mov		work_pi_current,ebx
    work_pi_current = ebx.uint_val;     // ZPTI
    // mov		eax,work.pu.current
    eax.uint_val = work.pu.current;
    // mov		ebx,work.pv.current     // ZPTI
    ebx.uint_val = work.pv.current;

    // add		eax,work.pu.d_nocarry	; step u according to carry from major edge
    eax.uint_val += work.pu.d_nocarry;
    // add		ebx,work.pv.d_nocarry	; step v according to carry from major edge
    ebx.uint_val += work.pv.d_nocarry;
    // mov		work.pu.current,eax
    work.pu.current = eax.uint_val;
    // mov		ecx,work_top_count
    ecx.uint_val = work_top_count;
    // mov		work.pv.current,ebx
    work.pv.current = ebx.uint_val;
    // dec		ecx					; decrement line counter
    ecx.uint_val--;
    // mov		work_top_count,ecx
    work_top_count = ecx.uint_val;
    // jge		scan_loop
    if (ecx.int_val >= 0) {
        goto scan_loop;
    }
    // ret
    return;

carry:
    // adc		ebp,0
    ADC(ebp.uint_val, 0);
    // mov		edi,work_top_i
    edi.uint_val = work_top_i;
    // mov		work_main_i,ebp
    work_main_i = ebp.uint_val;
    // mov		eax,work_top_d_i
    eax.uint_val = work_top_d_i;
    // add		edi,eax				; step minor edge
    edi.uint_val += eax.uint_val;
    // mov		eax,work.pq.current
    eax.uint_val = work.pq.current;
    // mov		work_top_i,edi
    work_top_i = edi.uint_val;
    // mov		ebx,work.pq.d_carry
    ebx.uint_val = work.pq.d_carry;
    // shr		edi,16				; get integer part of end of next scanline
    edi.uint_val >>= 16;
    // add		eax,ebx				; step q according to carry from major edge
    eax.uint_val += ebx.uint_val;
    // and		ebp,0ffffh			; get integer part of start of next scanline
    ebp.uint_val &= 0xffff;
    // mov		work.pq.current,eax
    work.pq.current = eax.uint_val;
    // mov		eax,work_pz_current
    eax.uint_val = work_pz_current;
    // mov		ebx,work_pz_d_carry

    // mov		ebx,work_pi_current
    ebx.uint_val = work_pi_current;
    // add		eax,work_pz_d_carry	; step z according to carry from major edge
    eax.uint_val += work_pz_d_carry;
    // add		ebx,work_pi_d_carry	; step i according to carry from major edge
    ebx.uint_val += work_pi_d_carry;


    // mov		work_pz_current,eax
    work_pz_current = eax.uint_val;
    // mov		work_pi_current,ebx
    work_pi_current = ebx.uint_val;
    // mov		eax,work.pu.current
    eax.uint_val = work.pu.current;
    // mov		ebx,work.pv.current
    ebx.uint_val = work.pv.current;
    // add		eax,work.pu.d_nocarry	; step u according to carry from major edge
    eax.uint_val += work.pu.d_nocarry;
    // add		ebx,work.pv.d_nocarry	; step v according to carry from major edge
    ebx.uint_val += work.pv.d_nocarry;
    // add		eax,work.pu.grad_x	; avoids the need to fixup nocarry and carry
    eax.uint_val += work.pu.grad_x;
    // add		ebx,work.pv.grad_x	; versions
    ebx.uint_val += work.pv.grad_x;
    // mov		work.pu.current,eax
    work.pu.current = eax.uint_val;
    // mov		ecx,work_top_count
    ecx.uint_val = work_top_count;
    // mov		work.pv.current,ebx
    work.pv.current = ebx.uint_val;
    // dec		ecx					; decrement line counter
    ecx.uint_val--;
    // mov		work_top_count,ecx
    work_top_count = ecx.uint_val;
    // jge		scan_loop
    if (ecx.int_val >= 0) {
        goto scan_loop;
    }

done_trapezium:

}

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
    esi.uint_val = work_main_y;
    // mov		eax,work.colour.base
    eax.uint_val = WORK_COLOUR_BASE;
    // dec		esi
    esi.uint_val--;
    // mov		ebx,work.colour.stride_b
    ebx.uint_val = work.colour.stride_b;
    // mov		ecx,work.depth.base
    ecx.uint_val = WORK_DEPTH_BASE;
    // mov		edx,work.depth.stride_b
    edx.uint_val = work.depth.stride_b;
    // imul	ebx,esi
    ebx.int_val *= esi.int_val;
    // imul	edx,esi
    edx.int_val *= esi.int_val;
    // add		eax,ebx
    eax.uint_val += ebx.uint_val;
    // add		ecx,edx
    ecx.uint_val += edx.uint_val;
    // dec		eax
    eax.uint_val--;
    // sub		ecx,2
    ecx.uint_val -= 2;
    // mov		workspace.scanAddress,eax
    workspace.scanAddress = eax.uint_val;
    // mov		workspace.depthAddress,ecx
    workspace.depthAddress = ecx.uint_val;

    // ; Swap integer and fractional parts of major edge starting value and delta and z gradient
	// ; Copy some values into perspective texture mappng workspace
	// ; Calculate offset of starting pixel in texture map
	// ;
    // mov		eax,work_main_i
    eax.uint_val = work_main_i;
    // mov		ebx,work_main_d_i
    ebx.uint_val = work_main_d_i;
    // ror		eax,16
    ROR16(eax);
    // cmp		ebx,80000000h
    CMP(ebx.uint_val, 0x80000000);
    // adc		ebx,-1
    ADC(ebx.uint_val, -1);
    // mov		ecx,work_pz_grad_x
    ecx.uint_val = work_pz_grad_x;
    // ror		ebx,16
    ROR16(ebx);
    // cmp		ecx,80000000h
    CMP(ecx.uint_val, 0x80000000);
    // adc		ecx,-1
    ADC(ecx.uint_val, -1);


    // mov		edx,work_pi_grad_x
    edx.uint_val = work_pi_grad_x;
	// 	ror		ecx,16
    ROR16(ecx);
	// 	cmp		edx,80000000h
    CMP(edx.uint_val, 0x80000000);
	// 	adc		edx,-1
    ADC(edx.uint_val, -1);

    // mov		work_main_i,eax
    work_main_i = eax.uint_val;


    // mov		al,byte ptr work.awsl.u_current
    eax.bytes[0] = work.awsl.u_current;

    // mov		work_main_d_i,ebx
    work_main_d_i = ebx.uint_val;

    // mov		ah,byte ptr work.awsl.v_current
    eax.bytes[1] = work.awsl.v_current;

    // mov		work.tsl.dz,ecx
    work.tsl.dz = ecx.uint_val;

    // shl		al,2
    eax.bytes[0] <<= 2;
    // mov		work.tsl._di,edx
    work.tsl.di = edx.uint_val;

    // shr		eax,2
    eax.uint_val >>= 2;
    // mov		ebx,work.pq.grad_x
    ebx.uint_val = work.pq.grad_x;
    // and		eax,63*65
    eax.uint_val &= 63*65;
    // mov		work.tsl.ddenominator,ebx
    work.tsl.ddenominator = ebx.uint_val;
    // mov		work.tsl.source,eax
    work.tsl.source = eax.uint_val;
    // mov		eax,work.tsl.direction
    eax.uint_val = work.tsl.direction;

    // ; Check scan direction and use appropriate rasteriser
	// ;
    // test	eax,eax
    // jnz		reversed
    if (eax.uint_val != 0) {
        goto reversed;
    }
    // call    TrapeziumRender_ZPT_I8_D16_64_f
    TrapeziumRender_ZPTI_I8_D16(DIR_F, eTrapezium_render_size_64);
    // mov		eax,work_bot_i
    eax.uint_val = work_bot_i;
    // mov		ebx,work_bot_d_i
    ebx.uint_val = work_bot_d_i;
    // mov		ecx,work_bot_count
    ecx.uint_val = work_bot_count;
    // mov		work_top_i,eax
    work_top_i = eax.uint_val;
    // mov		work_top_d_i,ebx
    work_top_d_i = ebx.uint_val;
    // mov		work_top_count,ecx
    work_top_count = ecx.uint_val;
    // call    TrapeziumRender_ZPT_I8_D16_64_f
   TrapeziumRender_ZPTI_I8_D16(DIR_F, eTrapezium_render_size_64);
    // ret
    return;

reversed:

    // call    TrapeziumRender_ZPT_I8_D16_64_b
   TrapeziumRender_ZPTI_I8_D16(DIR_B, eTrapezium_render_size_64);
    // mov		eax,work_bot_i
    eax.uint_val = work_bot_i;
    // mov		ebx,work_bot_d_i
    ebx.uint_val = work_bot_d_i;
    // mov		ecx,work_bot_count
    ecx.uint_val = work_bot_count;
    // mov		work_top_i,eax
    work_top_i = eax.uint_val;
    // mov		work_top_d_i,ebx
    work_top_d_i = ebx.uint_val;
    // mov		work_top_count,ecx
    work_top_count = ecx.uint_val;
    // call    TrapeziumRender_ZPT_I8_D16_64_b
    TrapeziumRender_ZPTI_I8_D16(DIR_B, eTrapezium_render_size_64);
}

void BR_ASM_CALL TriangleRender_ZPTI_I8_D16_64_FLAT(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}

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
    esi.uint_val = work_main_y;
    // mov		eax,work.colour.base
    eax.uint_val = WORK_COLOUR_BASE;
    // dec		esi
    esi.uint_val--;
    // mov		ebx,work.colour.stride_b
    ebx.uint_val = work.colour.stride_b;
    // mov		ecx,work.depth.base
    ecx.uint_val = WORK_DEPTH_BASE;
    // mov		edx,work.depth.stride_b
    edx.uint_val = work.depth.stride_b;
    // imul	ebx,esi
    ebx.int_val *= esi.int_val;
    // imul	edx,esi
    edx.int_val *= esi.int_val;
    // add		eax,ebx
    eax.uint_val += ebx.uint_val;
    // add		ecx,edx
    ecx.uint_val += edx.uint_val;
    // dec		eax
    eax.uint_val--;
    // sub		ecx,2
    ecx.uint_val -= 2;
    // mov		workspace.scanAddress,eax
    workspace.scanAddress = eax.uint_val;
    // mov		workspace.depthAddress,ecx
    workspace.depthAddress = ecx.uint_val;

    // ; Swap integer and fractional parts of major edge starting value and delta and z gradient
	// ; Copy some values into perspective texture mappng workspace
	// ; Calculate offset of starting pixel in texture map
	// ;
    // mov		eax,work_main_i
    eax.uint_val = work_main_i;
    // mov		ebx,work_main_d_i
    ebx.uint_val = work_main_d_i;
    // ror		eax,16
    ROR16(eax);
    // cmp		ebx,80000000h
    CMP(ebx.uint_val, 0x80000000);
    // adc		ebx,-1
    ADC(ebx.uint_val, -1);
    // mov		ecx,work_pz_grad_x
    ecx.uint_val = work_pz_grad_x;
    // ror		ebx,16
    ROR16(ebx);
    // cmp		ecx,80000000h
    CMP(ecx.uint_val, 0x80000000);
    // adc		ecx,-1
    ADC(ecx.uint_val, -1);
    // mov		work_main_i,eax
    work_main_i = eax.uint_val;
    // ror		ecx,16
    ROR16(ecx);
    // mov		al,byte ptr work.awsl.u_current
    eax.bytes[0] = work.awsl.u_current;
    // mov		ah,byte ptr work.awsl.v_current
    eax.bytes[1] = work.awsl.v_current;
    // mov		work_main_d_i,ebx
    work_main_d_i = ebx.uint_val;
    // shl		al,2
    eax.bytes[0] <<= 2;
    // mov		work.tsl.dz,ecx
    work.tsl.dz = ecx.uint_val;
    // shr		eax,2
    eax.uint_val >>= 2;
    // mov		ebx,work.pq.grad_x
    ebx.uint_val = work.pq.grad_x;
    // and		eax,63*65
    eax.uint_val &= 63*65;
    // mov		work.tsl.ddenominator,ebx
    work.tsl.ddenominator = ebx.uint_val;
    // mov		work.tsl.source,eax
    work.tsl.source = eax.uint_val;
    // mov		eax,work.tsl.direction
    eax.uint_val = work.tsl.direction;

    // ; Check scan direction and use appropriate rasteriser
	// ;
    // test	eax,eax
    // jnz		reversed
    if (eax.uint_val != 0) {
        goto reversed;
    }
    // call    TrapeziumRender_ZPT_I8_D16_64_f
    TrapeziumRender_ZPT_I8_D16(DIR_F, eTrapezium_render_size_64);
    // mov		eax,work_bot_i
    eax.uint_val = work_bot_i;
    // mov		ebx,work_bot_d_i
    ebx.uint_val = work_bot_d_i;
    // mov		ecx,work_bot_count
    ecx.uint_val = work_bot_count;
    // mov		work_top_i,eax
    work_top_i = eax.uint_val;
    // mov		work_top_d_i,ebx
    work_top_d_i = ebx.uint_val;
    // mov		work_top_count,ecx
    work_top_count = ecx.uint_val;
    // call    TrapeziumRender_ZPT_I8_D16_64_f
   TrapeziumRender_ZPT_I8_D16(DIR_F, eTrapezium_render_size_64);
    // ret
    return;

reversed:

    // call    TrapeziumRender_ZPT_I8_D16_64_b
   TrapeziumRender_ZPT_I8_D16(DIR_B, eTrapezium_render_size_64);
    // mov		eax,work_bot_i
    eax.uint_val = work_bot_i;
    // mov		ebx,work_bot_d_i
    ebx.uint_val = work_bot_d_i;
    // mov		ecx,work_bot_count
    ecx.uint_val = work_bot_count;
    // mov		work_top_i,eax
    work_top_i = eax.uint_val;
    // mov		work_top_d_i,ebx
    work_top_d_i = ebx.uint_val;
    // mov		work_top_count,ecx
    work_top_count = ecx.uint_val;
    // call    TrapeziumRender_ZPT_I8_D16_64_b
    TrapeziumRender_ZPT_I8_D16(DIR_B, eTrapezium_render_size_64);

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
    // no-op triangle
}

void BR_ASM_CALL TriangleRender_ZPTI_I8_D16_256_FLAT(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
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
        va_list unused;
        TriangleRender_ZT_I8_D16_POW2(block, 8, 1, unused);
        return;
    }

    // ; Calculate address of first scanline in colour and depth buffers
	// ;
    // mov		esi,work_main_y
    esi.uint_val = work_main_y;
    // mov		eax,work.colour.base
    eax.uint_val = WORK_COLOUR_BASE;
    // dec		esi
    esi.uint_val--;
    // mov		ebx,work.colour.stride_b
    ebx.uint_val = work.colour.stride_b;
    // mov		ecx,work.depth.base
    ecx.uint_val = WORK_DEPTH_BASE;
    // mov		edx,work.depth.stride_b
    edx.uint_val = work.depth.stride_b;
    // imul	ebx,esi
    ebx.int_val *= esi.int_val;
    // imul	edx,esi
    edx.int_val *= esi.int_val;
    // add		eax,ebx
    eax.uint_val += ebx.uint_val;
    // add		ecx,edx
    ecx.uint_val += edx.uint_val;
    // dec		eax
    eax.uint_val--;
    // sub		ecx,2
    ecx.uint_val -= 2;
    // mov		workspace.scanAddress,eax
    workspace.scanAddress = eax.uint_val;
    // mov		workspace.depthAddress,ecx
    workspace.depthAddress = ecx.uint_val;

    // ; Swap integer and fractional parts of major edge starting value and delta and z gradient
	// ; Copy some values into perspective texture mappng workspace
	// ; Calculate offset of starting pixel in texture map
	// ;
    // mov		eax,work_main_i
    eax.uint_val = work_main_i;
    // mov		ebx,work_main_d_i
    ebx.uint_val = work_main_d_i;
    // ror		eax,16
    ROR16(eax);
    // cmp		ebx,80000000h
    CMP(ebx.uint_val, 0x80000000);
    // adc		ebx,-1
    ADC(ebx.uint_val, -1);
    // mov		ecx,work_pz_grad_x
    ecx.uint_val = work_pz_grad_x;
    // ror		ebx,16
    ROR16(ebx);
    // cmp		ecx,80000000h
    CMP(ecx.uint_val, 0x80000000);
    // adc		ecx,-1
    ADC(ecx.uint_val, -1);
    // mov		work_main_i,eax
    work_main_i = eax.uint_val;
    // ror		ecx,16
    ROR16(ecx);
    // mov		work_main_d_i,ebx
    work_main_d_i = ebx.uint_val;
    // xor eax,eax
    eax.uint_val = 0;
    // mov		work.tsl.dz,ecx
    work.tsl.dz = ecx.uint_val;
    // mov		al,byte ptr work.awsl.u_current
    eax.bytes[0] = work.awsl.u_current;
    // mov		ebx,work.pq.grad_x
    ebx.uint_val = work.pq.grad_x;
    // mov		ah,byte ptr work.awsl.v_current
    eax.bytes[1] = work.awsl.v_current;
    // mov		work.tsl.ddenominator,ebx
    work.tsl.ddenominator = ebx.uint_val;
    // mov		work.tsl.source,eax
    work.tsl.source = eax.uint_val;
    // mov		eax,work.tsl.direction
    eax.uint_val = work.tsl.direction;

    // ; Check scan direction and use appropriate rasteriser
	// ;
    // test	eax,eax
    // jnz		reversed
    if (eax.uint_val != 0) {
        goto reversed;
    }
    // call    TrapeziumRender_ZPT_I8_D16_256_f
    TrapeziumRender_ZPT_I8_D16(DIR_F, eTrapezium_render_size_256);
    // mov		eax,work_bot_i
    eax.uint_val = work_bot_i;
    // mov		ebx,work_bot_d_i
    ebx.uint_val = work_bot_d_i;
    // mov		ecx,work_bot_count
    ecx.uint_val = work_bot_count;
    // mov		work_top_i,eax
    work_top_i = eax.uint_val;
    // mov		work_top_d_i,ebx
    work_top_d_i = ebx.uint_val;
    // mov		work_top_count,ecx
    work_top_count = ecx.uint_val;
    // call    TrapeziumRender_ZPT_I8_D16_256_f
   TrapeziumRender_ZPT_I8_D16(DIR_F, eTrapezium_render_size_256);
    // ret
    return;

reversed:

    // call    TrapeziumRender_ZPT_I8_D16_64_b
   TrapeziumRender_ZPT_I8_D16(DIR_B, eTrapezium_render_size_256);
    // mov		eax,work_bot_i
    eax.uint_val = work_bot_i;
    // mov		ebx,work_bot_d_i
    ebx.uint_val = work_bot_d_i;
    // mov		ecx,work_bot_count
    ecx.uint_val = work_bot_count;
    // mov		work_top_i,eax
    work_top_i = eax.uint_val;
    // mov		work_top_d_i,ebx
    work_top_d_i = ebx.uint_val;
    // mov		work_top_count,ecx
    work_top_count = ecx.uint_val;
    // call    TrapeziumRender_ZPT_I8_D16_64_b
    TrapeziumRender_ZPT_I8_D16(DIR_B, eTrapezium_render_size_256);
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
    // no-op triangle
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
