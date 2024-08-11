#include "brender.h"
#include "fpsetup.h"
#include "x86emu.h"
#include <stdarg.h>
#include "work.h"
#include "stdio.h"
#include "common.h"
#include "fpwork.h"

// TriangleRender_ZT_I8_D16_POW2 3,_8
// TriangleRender_ZT_I8_D16_POW2 4,_16
// TriangleRender_ZT_I8_D16_POW2 5,_32
// TriangleRender_ZT_I8_D16_POW2 6,_64
// TriangleRender_ZT_I8_D16_POW2 7,_128
// TriangleRender_ZT_I8_D16_POW2 8,_256
// TriangleRender_ZT_I8_D16_POW2 10,_1024

// ;**************************
// ; UnLit Textured Power Of 2
// ;**************************

void DRAW_ZT_I8_D16_POW2(uint32_t *minorX, uint32_t *d_minorX, char direction, int32_t *halfCount, int pow2) {
// 	local drawPixel,drawLine,done,lineDrawn,noPlot,mask
	uint32_t mask=0;
// ; height test
	MAKE_N_LOW_BIT_MASK(&mask,pow2);

// 	mov ebp,workspace.&half&Count
	ebp.v = *halfCount;
// 	mov edx,workspace.s_z
	edx.v = workspace.s_z;

// 	cmp ebp,0
// 	jl done
	if (ebp.int_val < 0) {
		return;
	}

// 	mov ebx,workspace.minorX
	ebx.v = *minorX;
// 	mov eax,workspace.s_u
	eax.v = workspace.s_u;

// 	mov ecx,workspace.xm
	ecx.v = workspace.xm;
// 	mov esi,workspace.s_v
	esi.v = workspace.s_v;

// 	mov workspace.c_u,eax
	workspace.c_u = eax.v;
// 	mov workspace.c_v,esi
	workspace.c_v = esi.v;

// 	mov edi,workspace.scanAddress
	edi.v = workspace.scanAddress;
// 	; unpaired instruction
drawLine:
// 	shr ebx,16
	ebx.v = ebx.v >> 16;
// 	mov ebp,workspace.depthAddress
	ebp.v = workspace.depthAddress;

// 	shr ecx,16
	ecx.v >>= 16;
// 	add edi,ebx
	edi.v += ebx.v;

// 	ror edx,16
	ROR16(edx);

// 	lea ebp,[ebp+2*ebx]
	ebp.v += ebx.v * 2;

// 	sub ecx,ebx
	ecx.v -= ebx.v;
// 	jg_d lineDrawn,direction
	JG_D(ecx.int_val, lineDrawn, direction);

drawPixel:
// 	shr esi,16-pow2
	esi.v >>= (16 - pow2);
// 	mov bl,[ebp+2*ecx]
	ebx.v = ((uint16_t *)work.depth.base)[ebp.v / 2 + ecx.int_val];

// 	shr eax,16
	eax.v >>= 16;
// 	and esi,mask shl pow2
	esi.v &= (mask << pow2);

// 	and eax,mask
	eax.v &= mask;
// 	mov bh,[ebp+2*ecx+1]

// 	or eax,esi
	eax.v |= esi.v;
// 	mov esi,work.texture.base
	esi.v = WORK_TEXTURE_BASE;

// 	cmp dx,bx ;two cycles
// 	ja noPlot
    if(edx.short_low > ebx.short_low) {
        goto noPlot;
    }


// 	mov al,[esi+eax]
	eax.v = ((uint8_t *)work.texture.base)[esi.v + eax.v];

// 	test al,al
// 	jz noPlot
	if(eax.v == 0) {
        goto noPlot;
    }

// 	mov [ebp+2*ecx],dl
// 	mov [ebp+2*ecx+1],dh
	((uint16_t *)work.depth.base)[ebp.v / 2 + ecx.int_val] = edx.short_low;
// 	mov [edi+ecx],al
	((uint8_t *)work.colour.base)[edi.v + ecx.v] = eax.l;

noPlot:
// 	mov ebx,workspace.d_z_x
	ebx.v = workspace.d_z_x;

// 	add_d edx,ebx,direction
	ADD_SET_CF_D(edx.v, ebx.v, direction);
// 	mov esi,workspace.c_v
	esi.v = workspace.c_v;

// 	adc_d edx,0,direction
	ADC_D(edx.v, 0, direction);
// 	mov ebx,workspace.d_v_x
	ebx.v = workspace.d_v_x;

// 	add_d esi,ebx,direction
	ADD_D(esi.v, ebx.v, direction);
// 	mov eax,workspace.c_u
	eax.v = workspace.c_u;

// 	mov workspace.c_v,esi
	workspace.c_v = esi.v;
// 	mov ebx,workspace.d_u_x
	ebx.v = workspace.d_u_x;

// 	add_d eax,ebx,direction
	ADD_D(eax.v, ebx.v, direction);
// 	inc_d ecx,direction
	INC_D(ecx.v, direction);

// 	mov workspace.c_u,eax
	workspace.c_u = eax.v;
// 	jle_d drawPixel,direction
	JLE_D(ecx.int_val, drawPixel, direction);

lineDrawn:
// ;perform per line updates

// 	mov ebp,workspace.xm_f
	ebp.v = workspace.xm_f;
// 	mov edi,workspace.d_xm_f
	edi.v = workspace.d_xm_f;

// 	add ebp,edi
	ADD_AND_SET_CF(ebp.v, edi.v);

// 	mov eax,workspace.s_u
	eax.v = workspace.s_u;

// 	sbb edx,edx
	SBB(edx.v, edx.v);
// 	mov workspace.xm_f,ebp
	workspace.xm_f = ebp.v;

// 	mov ebp,workspace.depthAddress
	ebp.v = workspace.depthAddress;
// 	mov ecx,work.depth.stride_b
	ecx.v = work.depth.stride_b;

// 	add ebp,ecx
	ebp.v += ecx.v;
// 	mov ecx,[workspace.d_u_y_0+edx*8]
	ecx.v = ((uint32_t *)&workspace.d_u_y_0)[2 * edx.int_val];

// 	mov workspace.depthAddress,ebp
	workspace.depthAddress = ebp.v;
// 	mov esi,workspace.s_v
	esi.v = workspace.s_v;

// 	add eax,ecx
	eax.v += ecx.v;
// 	mov ebx,[workspace.d_v_y_0+edx*8]
	ebx.v = ((uint32_t *)&workspace.d_v_y_0)[2 * edx.int_val];

// 	mov workspace.s_u,eax
	workspace.s_u = eax.v;
// 	add esi,ebx
	esi.v += ebx.v;

// 	mov workspace.c_u,eax
	workspace.c_u = eax.v;
// 	mov workspace.c_v,esi
	workspace.c_v = esi.v;

// 	mov ebp,workspace.s_z
	ebp.v = workspace.s_z;
// 	mov edx,[workspace.d_z_y_0+edx*8]
	edx.v = ((uint32_t *)&workspace.d_z_y_0)[2 * edx.int_val];

// 	mov workspace.s_v,esi
	workspace.s_v = esi.v;
// 	add edx,ebp
	edx.v += ebp.v;

// 	mov workspace.s_z,edx
	workspace.s_z = edx.v;
// 	mov edi,workspace.scanAddress
	edi.v = workspace.scanAddress;

// 	mov ebx,work.colour.stride_b
	ebx.v = work.colour.stride_b;
// 	mov ecx,workspace.d_&minorX
	ecx.v = *d_minorX;

// 	add edi,ebx
	edi.v += ebx.v;
// 	mov ebx,workspace.minorX
	ebx.v = *minorX;

// 	mov workspace.scanAddress,edi
	workspace.scanAddress = edi.v;
// 	add ebx,ecx
	ebx.v += ecx.v;

// 	mov workspace.minorX,ebx
	*minorX = ebx.v;
// 	mov ecx,workspace.xm
	ecx.v = workspace.xm;

// 	mov ebp,workspace.d_xm
	ebp.v = workspace.d_xm;
// 	;unpaired instruction

// 	add ecx,ebp
	ecx.v += ebp.v;
// 	mov ebp,workspace.&half&Count
	ebp.v = *halfCount;

// 	dec ebp
	ebp.v--;
// 	mov workspace.xm,ecx
	workspace.xm = ecx.v;

// 	mov workspace.&half&Count,ebp
	*halfCount = ebp.v;
// 	jge drawLine
	if (*halfCount >= 0) {
		goto drawLine;
	}
}

void BR_ASM_CALL TriangleRender_ZT_I8_D16_POW2(brp_block *block, int pow2, int skip_setup, va_list va) {
	brp_vertex *v0; // [esp+18h] [ebp+Ch]
    brp_vertex *v1; // [esp+1Ch] [ebp+10h]
    brp_vertex *v2; // [esp+20h] [ebp+14h]

	if (!skip_setup) {
		v0 = va_arg(va, brp_vertex *);
		v1 = va_arg(va, brp_vertex *);
		v2 = va_arg(va, brp_vertex *);
		va_end(va);

		workspace.v0 = v0;
		workspace.v1 = v1;
		workspace.v2 = v2;

		TriangleSetup_ZT(v0, v1, v2);
	}

	intptr_t cb = 0;
    intptr_t db = 0;

// ;										st(0)		st(1)		st(2)		st(3)		st(4)		st(5)		st(6)		st(7)
// 	fild work.colour.base			;	cb
	FILD(cb);
// 	fild workspace.t_y				;	ty			cb
	FILD(workspace.t_y);
// 	fild work.depth.base			;	db			ty			cb
	FILD(db);
// 	fild work.colour.stride_b		;	cs			db			ty			cb
	FILD(work.colour.stride_b);
// 	fild work.depth.stride_b		;	ds			cs			db			ty			cb
	FILD(work.depth.stride_b);
// 	fxch st(4)						;	cb			cs			db			ty			ds
	FXCH(4);
// 	fsub fp_one						;	cb-1		cs			db			ty			ds
	FSUB(fp_one);
// 	 fxch st(3)						;	ty			cs			db			cb-1		ds
	FXCH(3);
// 	fsub fp_one						;	ty-1		cs			db			cb-1		ds
	FSUB(fp_one);
// 	fxch st(2)						;	db			cs			ty-1		cb-1		ds
	FXCH(2);
// 	fsub fp_two						;	db-2		cs			ty-1		cb-1		ds
	FSUB(fp_two);
// 	 fxch st(3)						;	cb-1		cs			ty-1		db-2		ds
	FXCH(3);
// 	fadd fp_conv_d					;	cb-1I		cs			ty-1		db-2		ds
	FADD(fp_conv_d);
// 	 fxch st(1)						;	cs			cb-1I		ty-1		db-2		ds
	FXCH(1);
// 	fmul st,st(2)					;	csy			cb-1I		ty-1		db-2		ds
	FMUL_ST(0, 2);
// 	 fxch st(3)						;	db-2		cb-1I		ty-1		csy			ds
	FXCH(3);
// 	fadd fp_conv_d					;	db-2I		cb-1I		ty-1		csy			ds
	FADD(fp_conv_d);
// 	 fxch st(2)						;	ty-1		cb-1I		db-2I		csy			ds
	FXCH(2);
// 	fmulp st(4),st					;	cb-1I		db-2I		csy			dsy
	FMULP_ST(4, 0);
// 	faddp st(2),st					;	db-2I		ca			dsy
	FADDP_ST(2, 0);
// 	;x-stall
// 	mov eax,workspace.xm
    eax.v = workspace.xm;
// 	mov ebx,workspace.d_xm
	ebx.v = workspace.d_xm;

// 	faddp st(2),st					;	ca			da
	FADDP_ST(2, 0);
// 	;x-stall
// 	;x-stall

// 	shl eax,16
	eax.v <<= 16;
// 	mov edx,workspace.d_z_x
	edx.v = workspace.d_z_x;

// 	shl ebx,16
	ebx.v <<= 16;
// 	mov workspace.xm_f,eax
	workspace.xm_f = eax.v;

// 	fstp qword ptr workspace.scanAddress
	FSTP64(&workspace.scanAddress_double);
// 	fstp qword ptr workspace.depthAddress
	FSTP64(&workspace.depthAddress_double);

// 	mov workspace.d_xm_f,ebx
	workspace.d_xm_f = ebx.v;
// 	cmp edx,80000000
	CMP(edx.v, 80000000);

// 	adc edx,-1
	ADC(edx.v, -1);

// 	mov	eax,workspace.flip
	eax.v = workspace.flip;

// 	ror edx,16
	ROR16(edx);

// 	test eax,eax

// 	mov workspace.d_z_x,edx
	workspace.d_z_x = edx.v;
// 	jnz	drawRL

// 	DRAW_ZT_I8_D16_POW2 x1,DRAW_LR,top,pow2
// 	DRAW_ZT_I8_D16_POW2 x2,DRAW_LR,bottom,pow2
// 	ret

// drawRL:
// 	DRAW_ZT_I8_D16_POW2 x1,DRAW_RL,top,pow2
// 	DRAW_ZT_I8_D16_POW2 x2,DRAW_RL,bottom,pow2
// 	ret
// eax is 0, ZF=1
	if (eax.v == 0) {
		DRAW_ZT_I8_D16_POW2 (&workspace.x1, &workspace.d_x1, DRAW_LR,&workspace.topCount,pow2);
		DRAW_ZT_I8_D16_POW2 (&workspace.x2, &workspace.d_x2, DRAW_LR,&workspace.bottomCount,pow2);
	} else {
		DRAW_ZT_I8_D16_POW2 (&workspace.x1, &workspace.d_x1, DRAW_RL,&workspace.topCount,pow2);
		DRAW_ZT_I8_D16_POW2 (&workspace.x2, &workspace.d_x2, DRAW_RL,&workspace.bottomCount,pow2);
	}
}

void BR_ASM_CALL TriangleRender_ZT_I8_D16_8(brp_block *block, brp_vertex *v0, brp_vertex *v1,brp_vertex *v2) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZT_I8_D16_16(brp_block *block, brp_vertex *v0, brp_vertex *v1,brp_vertex *v2) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZT_I8_D16_32(brp_block *block, brp_vertex *v0, brp_vertex *v1,brp_vertex *v2) {
    // Not implemented
    BrAbort();
}

void BR_ASM_CALL TriangleRender_ZT_I8_D16_64(brp_block *block, ...) {
    va_list     va;
    va_start(va, block);
	TriangleRender_ZT_I8_D16_POW2(block, 6, 0, va);
	va_end(va);
}

void BR_ASM_CALL TriangleRender_ZT_I8_D16_128(brp_block *block, brp_vertex *v0, brp_vertex *v1,brp_vertex *v2) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZT_I8_D16_256(brp_block *block, ...) {
    va_list     va;
    va_start(va, block);
	TriangleRender_ZT_I8_D16_POW2(block, 8, 0, va);
	va_end(va);
}
void BR_ASM_CALL TriangleRender_ZT_I8_D16_1024(brp_block *block, brp_vertex *v0, brp_vertex *v1,brp_vertex *v2) {
    // Not implemented
    BrAbort();
}
