#include "brender.h"
#include "priminfo.h"
#include "fpsetup.h"
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
    eax.uint_val = work_pi_current;
    // mov	edx,work_pz_current
    edx.uint_val = work_pz_current;

    // mov	edi,work_&half&_i
    edi.uint_val = *half_i;
    // mov	ebp,work_main_i
    ebp.uint_val = work_main_i;

    // shr	edi,16				; get integer part of end of first scanline
    edi.uint_val >>= 16;
    // mov	ebx,workspace.depthAddress
    ebx.uint_val = workspace.depthAddress;

    // shr	ebp,16				; get integer part of start of first scanline
    ebp.uint_val >>= 16;
    // mov	esi,workspace.scanAddress
    esi.uint_val = workspace.scanAddress;

scan_loop:
    // ; Calculate pixel count and end addresses for line
	// ; Adjust i & z for the inner loop

    // sub	ebp,edi				; calculate pixel count
    ebp.uint_val -= edi.uint_val;
    // jg_&dirn	no_pixels
    if (dirn == DIR_F && ebp.int_val > 0) {
        goto no_pixels;
    } else if (dirn == DIR_B && ebp.int_val < 0) {
        goto no_pixels;
    }

    // add	esi,edi				; calculate end colour buffer pointer
    esi.uint_val += edi.uint_val;
    // lea	edi,[ebx+edi*2]		; calculate end depth buffer pointer
    edi.uint_val = ebx.uint_val + edi.uint_val * 2;

    // ror	eax,16				; swap words of i
    ROR16(eax);
    // mov	ecx,work_pi_grad_x
    ecx.uint_val = work_pi_grad_x;

    // ror	edx,16				; swap words of z
    ROR16(edx);
    // sub_&dirn	eax,ecx		; cancel out first step of i in loop
    if (dirn == DIR_F) {
        SUB_AND_SET_CF(eax.uint_val, ecx.uint_val);
    } else {
        ADD_AND_SET_CF(eax.uint_val, ecx.uint_val);
    }

    // sbb_&dirn	eax,0		; also clear carry
    if (dirn == DIR_F) {
        SBB(eax.uint_val, 0);
    } else {
        ADC(eax.uint_val, 0);
    }
    // mov	ebx,edx				; need same junk in high word of old and new z
    ebx.uint_val = edx.uint_val;

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
    if (dirn == DIR_F) {
        ADC(edx.uint_val, 0);
    } else {
        SBB(edx.uint_val, 0);
    }
    // mov	bl,[edi+ebp*2]		; fetch old z
    ebx.short_val[0] = ((uint16_t *)work.depth.base)[edi.uint_val / 2 + ebp.int_val];

    // add_&dirn	eax,ecx		; step i
    if (dirn == DIR_F) {
        ADD_AND_SET_CF(eax.uint_val, ecx.uint_val);
    } else {
        SUB_AND_SET_CF(eax.uint_val, ecx.uint_val);
    }
    // mov	bh,[edi+ebp*2+1]

    // adc_&dirn	eax,0		; carry into integer part of i
    if (dirn == DIR_F) {
        ADC(eax.uint_val, 0);
    } else {
        SBB(eax.uint_val, 0);
    }
    // cmp	edx,ebx				; compare z (identical junk in top words does not affect result)
    int ja_flag = edx.uint_val > ebx.uint_val;

    // mov	ebx,work_pz_grad_x
    ebx.uint_val = work_pz_grad_x;
    // ja	pixel_behind
    if (ja_flag) {
        goto pixel_behind;
    }

    // mov	[edi+ebp*2],dx		; store pixel and depth (prefix cannot be avoided since
    ((uint16_t *)work.depth.base)[edi.uint_val / 2 + ebp.int_val] = edx.short_val[0];
    // mov	[esi+ebp],al		; two byte writes would fill the write buffers)
    ((uint8_t *)work.colour.base)[esi.uint_val + ebp.uint_val] = eax.bytes[0];

pixel_behind:

    // add_&dirn	edx,ebx		; step z
    // inc_&dirn	ebp			; increment (negative) counter/offset
    if (dirn == DIR_F) {
        edx.uint_val += ebx.uint_val;
        ebp.uint_val++;
    } else {
        edx.uint_val -= ebx.uint_val;
        ebp.uint_val--;
    }

    // mov	ebx,edx				; need same junk in high word of old and new z
    ebx.uint_val = edx.uint_val;
    // jle_&dirn	pixel_loop	; loop
    if (dirn == DIR_F) {
        if (ebp.int_val <= 0) {
            goto pixel_loop;
        }
    } else {
        if (ebp.int_val >= 0) {
            goto pixel_loop;
        }
    }

no_pixels:

	// ; Updates for next scanline:
	// ;
    // mov	esi,workspace.scanAddress
    esi.uint_val = workspace.scanAddress;
    // mov	ecx,work.colour.stride_b
    ecx.uint_val = work.colour.stride_b;

    // mov	ebx,workspace.depthAddress
    ebx.uint_val = workspace.depthAddress;
    // mov	edx,work.depth.stride_b
    edx.uint_val = work.depth.stride_b;

    // add	esi,ecx				; move down one line in colour buffer
    esi.uint_val += ecx.uint_val;
    // add	ebx,edx				; move down one line in depth buffer
    ebx.uint_val += edx.uint_val;

    // mov	workspace.scanAddress,esi
    workspace.scanAddress = esi.uint_val;
    // mov	workspace.depthAddress,ebx
    workspace.depthAddress = ebx.uint_val;

    // mov	ebp,work_main_i
    ebp.uint_val = work_main_i;
    // mov	edi,work_&half&_i
    edi.uint_val = *half_i;

    // add	ebp,work_main_d_i	; step major edge
    ebp.uint_val += work_main_d_i;
    // add	edi,work_&half&_d_i	; step minor edge
    edi.uint_val += *half_d_i;

    // mov	work_main_i,ebp
    work_main_i = ebp.uint_val;
    // mov	work_&half&_i,edi
    *half_i = edi.uint_val;

    // mov	eax,work.main.f
    eax.uint_val = work.main.f;
    // mov	ecx,work.main.d_f
    ecx.uint_val = work.main.d_f;

    // shr	ebp,16				; get integer part of start of first scanline
    ebp.uint_val >>= 16;
    // add	eax,ecx
    ADD_AND_SET_CF(eax.uint_val, ecx.uint_val);

    // sbb	ecx,ecx				; get (0 - carry)
    SBB(ecx.uint_val, ecx.uint_val);

    // shr	edi,16				; get integer part of end of first scanline
    edi.uint_val >>= 16;
    // mov	work.main.f,eax
    work.main.f = eax.uint_val;

    // mov	eax,work_pi_current
    eax.uint_val = work_pi_current;
    // mov	edx,work_pz_current
    edx.uint_val = work_pz_current;

    // add	eax,[work_pi_d_nocarry+ecx*8]	; step i according to carry from major edge	; *4 for old workspace
    eax.uint_val += ((uint32_t *)&work_pi_d_nocarry)[ecx.int_val * 2];
    // add	edx,[work_pz_d_nocarry+ecx*8]	; step z according to carry from major edge	; *4 for old workspace
    edx.uint_val += ((uint32_t *)&work_pz_d_nocarry)[ecx.int_val * 2];

    // mov	work_pi_current,eax
    work_pi_current = eax.uint_val;
    // mov	ecx,work_&half&_count
    ecx.int_val = *half_count;

    // mov	work_pz_current,edx
    work_pz_current = edx.uint_val;
    // dec	ecx					; decrement line counter
    ecx.uint_val--;

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
    eax.ptr_val = v0;
	// 	mov	ecx,pvertex_1
    ecx.ptr_val = v1;

	// 	mov	edx,pvertex_2
    edx.ptr_val = v2;
	// 	mov	workspace.v0,eax
    workspace.v0 = eax.ptr_val;

	// 	mov	workspace.v1,ecx
    workspace.v1 = ecx.ptr_val;
	// 	mov	workspace.v2,edx
    workspace.v2 = edx.ptr_val;

	// ; Call new floating point setup routine
	// ;
    //     call TriangleSetup_ZI
    TriangleSetup_ZI(v0, v1, v2);

    // ; Calculate address of first scanline in colour and depth buffers
	// ;
	// 	mov	esi,workspace.t_y
    esi.uint_val = workspace.t_y;
	// 	mov	eax,work.colour.base
    eax.uint_val = WORK_COLOUR_BASE;

	// 	dec	esi
    esi.uint_val--;
	// 	mov	ebx,work.colour.stride_b
    ebx.uint_val = work.colour.stride_b;

	// 	mov	ecx,work.depth.base
    ecx.uint_val = WORK_DEPTH_BASE;
	// 	mov	edx,work.depth.stride_b
    edx.uint_val = work.depth.stride_b;

	// 	imul	ebx,esi
    ebx.int_val *= esi.int_val;

	// 	imul	edx,esi
    edx.int_val *= esi.int_val;

	// 	add	eax,ebx
    eax.uint_val += ebx.uint_val;
	// 	add	ecx,edx
    ecx.uint_val += edx.uint_val;

	// 	dec	eax
    eax.uint_val--;
	// 	sub	ecx,2
    ecx.uint_val -= 2;

	// 	mov	workspace.scanAddress,eax
    workspace.scanAddress = eax.uint_val;
	// 	mov	workspace.depthAddress,ecx
    workspace.depthAddress = ecx.uint_val;

    // ; Swap integer and fractional parts of major edge starting value and delta and z & i gradients
	// ;
	// ; This will cause carry into fractional part for negative gradients so
	// ; subtract one from the fractional part to adjust accordingly
	// ;
	// 	mov	eax,work_main_i
    eax.uint_val = work_main_i;
	// 	mov	ebx,work_main_d_i
    ebx.uint_val = work_main_d_i;

	// 	shl	eax,16
    eax.uint_val <<= 16;
	// 	mov	ecx,work_pz_grad_x
    ecx.uint_val = work_pz_grad_x;

	// 	shl	ebx,16
    ebx.uint_val <<= 16;
	// 	cmp	ecx,80000000h
    CMP(ecx.uint_val, 0x80000000);

	// 	adc	ecx,-1
    ADC(ecx.uint_val, -1);
	// 	mov	edx,work_pi_grad_x
    edx.uint_val = work_pi_grad_x;

	// 	ror	ecx,16
    //ror(x86_op_reg(&ecx), 16);
    ROR16(ecx);
	// 	cmp	edx,80000000h
    CMP(edx.uint_val, 0x80000000);

	// 	adc	edx,-1
    ADC(edx.uint_val, -1);
	// 	mov	work.main.f,eax
    work.main.f = eax.uint_val;

	// 	ror	edx,16
    ROR16(edx);
	// 	mov	work.main.d_f,ebx
    work.main.d_f = ebx.uint_val;

	// 	mov	work_pz_grad_x,ecx
    work_pz_grad_x = ecx.uint_val;
	// 	mov	work_pi_grad_x,edx
    work_pi_grad_x = edx.uint_val;

    // ; Check scan direction and use appropriate rasteriser
	// ;
    // mov	eax,workspace.flip
    eax.uint_val = workspace.flip;
    // ;vslot

    // test eax,eax
    // zf = 1 when eax=0
    //jnz	reversed
    //jump is zf is 0

    if (eax.uint_val == 0) {
        TRAPEZIUM_ZI_I8_D16(&workspace.topCount, &workspace.x1, &workspace.d_x1, DIR_F);
        TRAPEZIUM_ZI_I8_D16(&workspace.bottomCount, &workspace.x2, &workspace.d_x2, DIR_F);
    } else {
        TRAPEZIUM_ZI_I8_D16(&workspace.topCount, &workspace.x1, &workspace.d_x1, DIR_B);
        TRAPEZIUM_ZI_I8_D16(&workspace.bottomCount, &workspace.x2, &workspace.d_x2, DIR_B);
    }

}
