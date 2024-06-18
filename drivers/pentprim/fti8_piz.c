#include "brender.h"
#include "fpsetup.h"
#include "fpwork.h"
#include <stdarg.h>
#include <stdio.h>
#include "work.h"
#include "x86emu.h"
#include "common.h"

#define work_main_i				workspace.xm
#define work_main_d_i			workspace.d_xm

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

void TRAPEZIUM_ZI_I8_D16(int32_t *half_count, uint32_t *half_i, uint32_t *half_d_i, int dirn) {

    // mov	ebx,work_&half&_count	; check for empty trapezium
    ebx.int_val = *half_count;
    // ;vslot

    // test	ebx,ebx
    // jl	done_trapezium
    if (ebx.int_val < 0) {
        return;
    }

    // mov	eax,work_pi_current
    eax.v = work_pi_current;
    // mov	edx,work_pz_current
    edx.v = work_pz_current;

    // mov	edi,work_&half&_i
    edi.v = *half_i;
    // mov	ebp,work_main_i
    ebp.v = work_main_i;

    // shr	edi,16				; get integer part of end of first scanline
    edi.v >>= 16;
    // mov	ebx,workspace.depthAddress
    ebx.v = workspace.depthAddress;

    // shr	ebp,16				; get integer part of start of first scanline
    ebp.v >>= 16;
    // mov	esi,workspace.scanAddress
    esi.v = workspace.scanAddress;

scan_loop:
    // ; Calculate pixel count and end addresses for line
	// ; Adjust i & z for the inner loop

    // sub	ebp,edi				; calculate pixel count
    ebp.v -= edi.v;
    // jg_&dirn	no_pixels
    JG_D(ebp.int_val, no_pixels, dirn);

    // add	esi,edi				; calculate end colour buffer pointer
    esi.v += edi.v;
    // lea	edi,[ebx+edi*2]		; calculate end depth buffer pointer
    edi.v = ebx.v + edi.v * 2;

    // ror	eax,16				; swap words of i
    ROR16(eax);
    // mov	ecx,work_pi_grad_x
    ecx.v = work_pi_grad_x;

    // ror	edx,16				; swap words of z
    ROR16(edx);
    // sub_&dirn	eax,ecx		; cancel out first step of i in loop
    if (dirn == DIR_F) {
        SUB_AND_SET_CF(eax.v, ecx.v);
    } else {
        ADD_AND_SET_CF(eax.v, ecx.v);
    }

    // sbb_&dirn	eax,0		; also clear carry
    if (dirn == DIR_F) {
        SBB(eax.v, 0);
    } else {
        ADC(eax.v, 0);
    }
    // mov	ebx,edx				; need same junk in high word of old and new z
    ebx.v = edx.v;

    // TOOD: not sure here
    x86_state.cf = 0;

    // ; eax = i
	// ; ebx = old z, dz
	// ; ecx =	di
	// ; edx =	z
	// ; ebp =	count
	// ; esi =	frame buffer ptr
	// ; edi =	z buffer ptr

pixel_loop:

    // adc_&dirn	edx,0		; carry into integer part of z
    ADC_D(edx.v, 0, dirn);
    // mov	bl,[edi+ebp*2]		; fetch old z
    ebx.short_low = ((uint16_t *)work.depth.base)[edi.v / 2 + ebp.int_val];

    // add_&dirn	eax,ecx		; step i
    ADD_SET_CF_D(eax.v, ecx.v, dirn);
    // mov	bh,[edi+ebp*2+1]

    // adc_&dirn	eax,0		; carry into integer part of i
    ADC_D(eax.v, 0, dirn);
    // cmp	edx,ebx				; compare z (identical junk in top words does not affect result)
    int ja_flag = edx.v > ebx.v;

    // mov	ebx,work_pz_grad_x
    ebx.v = work_pz_grad_x;
    // ja	pixel_behind
    if (ja_flag) {
        goto pixel_behind;
    }

    // mov	[edi+ebp*2],dx		; store pixel and depth (prefix cannot be avoided since
    ((uint16_t *)work.depth.base)[edi.v / 2 + ebp.int_val] = edx.short_low;
    // mov	[esi+ebp],al		; two byte writes would fill the write buffers)
    ((uint8_t *)work.colour.base)[esi.v + ebp.v] = eax.l;

pixel_behind:

    // add_&dirn	edx,ebx		; step z
    // inc_&dirn	ebp			; increment (negative) counter/offset
    if (dirn == DIR_F) {
        edx.v += ebx.v;
        ebp.v++;
    } else {
        edx.v -= ebx.v;
        ebp.v--;
    }

    // mov	ebx,edx				; need same junk in high word of old and new z
    ebx.v = edx.v;
    // jle_&dirn	pixel_loop	; loop
    JLE_D(ebp.int_val, pixel_loop, dirn);

no_pixels:

	// ; Updates for next scanline:
	// ;
    // mov	esi,workspace.scanAddress
    esi.v = workspace.scanAddress;
    // mov	ecx,work.colour.stride_b
    ecx.v = work.colour.stride_b;

    // mov	ebx,workspace.depthAddress
    ebx.v = workspace.depthAddress;
    // mov	edx,work.depth.stride_b
    edx.v = work.depth.stride_b;

    // add	esi,ecx				; move down one line in colour buffer
    esi.v += ecx.v;
    // add	ebx,edx				; move down one line in depth buffer
    ebx.v += edx.v;

    // mov	workspace.scanAddress,esi
    workspace.scanAddress = esi.v;
    // mov	workspace.depthAddress,ebx
    workspace.depthAddress = ebx.v;

    // mov	ebp,work_main_i
    ebp.v = work_main_i;
    // mov	edi,work_&half&_i
    edi.v = *half_i;

    // add	ebp,work_main_d_i	; step major edge
    ebp.v += work_main_d_i;
    // add	edi,work_&half&_d_i	; step minor edge
    edi.v += *half_d_i;

    // mov	work_main_i,ebp
    work_main_i = ebp.v;
    // mov	work_&half&_i,edi
    *half_i = edi.v;

    // mov	eax,work.main.f
    eax.v = work.main.f;
    // mov	ecx,work.main.d_f
    ecx.v = work.main.d_f;

    // shr	ebp,16				; get integer part of start of first scanline
    ebp.v >>= 16;
    // add	eax,ecx
    ADD_AND_SET_CF(eax.v, ecx.v);

    // sbb	ecx,ecx				; get (0 - carry)
    SBB(ecx.v, ecx.v);

    // shr	edi,16				; get integer part of end of first scanline
    edi.v >>= 16;
    // mov	work.main.f,eax
    work.main.f = eax.v;

    // mov	eax,work_pi_current
    eax.v = work_pi_current;
    // mov	edx,work_pz_current
    edx.v = work_pz_current;

    // add	eax,[work_pi_d_nocarry+ecx*8]	; step i according to carry from major edge	; *4 for old workspace
    eax.v += ((uint32_t *)&work_pi_d_nocarry)[ecx.int_val * 2];
    // add	edx,[work_pz_d_nocarry+ecx*8]	; step z according to carry from major edge	; *4 for old workspace
    edx.v += ((uint32_t *)&work_pz_d_nocarry)[ecx.int_val * 2];

    // mov	work_pi_current,eax
    work_pi_current = eax.v;
    // mov	ecx,work_&half&_count
    ecx.int_val = *half_count;

    // mov	work_pz_current,edx
    work_pz_current = edx.v;
    // dec	ecx					; decrement line counter
    ecx.v--;

    // mov	work_&half&_count,ecx
    *half_count = ecx.int_val;
    // jge scan_loop
    if (*half_count >= 0) {
        goto scan_loop;
    }
}



void BR_ASM_CALL TriangleRender_ZI_I8_D16(brp_block *block, ...) {

    brp_vertex *v0;
    brp_vertex *v1;
    brp_vertex *v2;

    va_list     va;
    va_start(va, block);
    v0 = va_arg(va, brp_vertex *);
    v1 = va_arg(va, brp_vertex *);
    v2 = va_arg(va, brp_vertex *);
	va_end(va);

    // ; Get pointers to vertex structures
	// ;
	// 	mov	eax,pvertex_0
    eax.ptr_v = v0;
	// 	mov	ecx,pvertex_1
    ecx.ptr_v = v1;

	// 	mov	edx,pvertex_2
    edx.ptr_v = v2;
	// 	mov	workspace.v0,eax
    workspace.v0 = eax.ptr_v;

	// 	mov	workspace.v1,ecx
    workspace.v1 = ecx.ptr_v;
	// 	mov	workspace.v2,edx
    workspace.v2 = edx.ptr_v;

	// ; Call new floating point setup routine
	// ;
    //     call TriangleSetup_ZI
    TriangleSetup_ZI(v0, v1, v2);

    // ; Calculate address of first scanline in colour and depth buffers
	// ;
	// 	mov	esi,workspace.t_y
    esi.v = workspace.t_y;
	// 	mov	eax,work.colour.base
    eax.v = WORK_COLOUR_BASE;

	// 	dec	esi
    esi.v--;
	// 	mov	ebx,work.colour.stride_b
    ebx.v = work.colour.stride_b;

	// 	mov	ecx,work.depth.base
    ecx.v = WORK_DEPTH_BASE;
	// 	mov	edx,work.depth.stride_b
    edx.v = work.depth.stride_b;

	// 	imul	ebx,esi
    ebx.int_val *= esi.int_val;

	// 	imul	edx,esi
    edx.int_val *= esi.int_val;

	// 	add	eax,ebx
    eax.v += ebx.v;
	// 	add	ecx,edx
    ecx.v += edx.v;

	// 	dec	eax
    eax.v--;
	// 	sub	ecx,2
    ecx.v -= 2;

	// 	mov	workspace.scanAddress,eax
    workspace.scanAddress = eax.v;
	// 	mov	workspace.depthAddress,ecx
    workspace.depthAddress = ecx.v;

    // ; Swap integer and fractional parts of major edge starting value and delta and z & i gradients
	// ;
	// ; This will cause carry into fractional part for negative gradients so
	// ; subtract one from the fractional part to adjust accordingly
	// ;
	// 	mov	eax,work_main_i
    eax.v = work_main_i;
	// 	mov	ebx,work_main_d_i
    ebx.v = work_main_d_i;

	// 	shl	eax,16
    eax.v <<= 16;
	// 	mov	ecx,work_pz_grad_x
    ecx.v = work_pz_grad_x;

	// 	shl	ebx,16
    ebx.v <<= 16;
	// 	cmp	ecx,80000000h
    CMP(ecx.v, 0x80000000);

	// 	adc	ecx,-1
    ADC(ecx.v, -1);
	// 	mov	edx,work_pi_grad_x
    edx.v = work_pi_grad_x;

	// 	ror	ecx,16
    //ror(x86_op_reg(&ecx), 16);
    ROR16(ecx);
	// 	cmp	edx,80000000h
    CMP(edx.v, 0x80000000);

	// 	adc	edx,-1
    ADC(edx.v, -1);
	// 	mov	work.main.f,eax
    work.main.f = eax.v;

	// 	ror	edx,16
    ROR16(edx);
	// 	mov	work.main.d_f,ebx
    work.main.d_f = ebx.v;

	// 	mov	work_pz_grad_x,ecx
    work_pz_grad_x = ecx.v;
	// 	mov	work_pi_grad_x,edx
    work_pi_grad_x = edx.v;

    // ; Check scan direction and use appropriate rasteriser
	// ;
    // mov	eax,workspace.flip
    eax.v = workspace.flip;
    // ;vslot

    // test eax,eax
    // zf = 1 when eax=0
    //jnz	reversed
    //jump is zf is 0

    if (eax.v == 0) {
        TRAPEZIUM_ZI_I8_D16(&workspace.topCount, &workspace.x1, &workspace.d_x1, DIR_F);
        TRAPEZIUM_ZI_I8_D16(&workspace.bottomCount, &workspace.x2, &workspace.d_x2, DIR_F);
    } else {
        TRAPEZIUM_ZI_I8_D16(&workspace.topCount, &workspace.x1, &workspace.d_x1, DIR_B);
        TRAPEZIUM_ZI_I8_D16(&workspace.bottomCount, &workspace.x2, &workspace.d_x2, DIR_B);
    }

}
