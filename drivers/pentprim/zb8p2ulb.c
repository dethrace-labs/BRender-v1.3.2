#include <stdarg.h>
#include "x86emu.h"
#include <stdlib.h>
#include <stdio.h>
#include "fpsetup.h"
#include "fpwork.h"
#include "common.h"
#include "work.h"

// ;**********************************
// ; Blended UnLit Textured Power Of 2
// ;**********************************

void DRAW_ZTB_I8_D16_POW2(uint32_t *minorX, uint32_t *d_minorX, char direction, int32_t *halfCount, int pow2) {
// 	local drawPixel,drawLine,done,lineDrawn,noPlot,mask
	int32_t mask=0;
// ; height test
	MAKE_N_LOW_BIT_MASK(&mask,pow2);

// 	mov ebp,workspace.&half&Count
	ebp.uint_val = *halfCount;
// 	mov edx,workspace.s_z
	edx.uint_val = workspace.s_z;

// 	cmp ebp,0
// 	jl done
	if (ebp.int_val < 0) {
		goto done;
	}

// 	mov ebx,workspace.minorX
	ebx.uint_val = *minorX;
// 	mov eax,workspace.s_u
	eax.uint_val = workspace.s_u;

// 	mov ecx,workspace.xm
	ecx.uint_val = workspace.xm;
// 	mov esi,workspace.s_v
	esi.uint_val = workspace.s_v;

// 	mov workspace.c_u,eax
	workspace.c_u = eax.uint_val;
// 	mov workspace.c_v,esi
	workspace.c_v = esi.uint_val;

// 	mov edi,workspace.scanAddress
	edi.uint_val = workspace.scanAddress;
// 	; unpaired instruction
drawLine:
// 	shr ebx,16
	ebx.uint_val = ebx.uint_val >> 16;
// 	mov ebp,workspace.depthAddress
	ebp.uint_val = workspace.depthAddress;

// 	shr ecx,16
	ecx.uint_val >>= 16;
// 	add edi,ebx
	edi.uint_val += ebx.uint_val;

// 	ror edx,16
	int16_t tmp = edx.short_val[0];
	edx.short_val[0] = edx.short_val[1];
	edx.short_val[1] = tmp;

// 	lea ebp,[ebp+2*ebx]
	ebp.uint_val += ebx.uint_val * 2;

// 	sub ecx,ebx
	ecx.uint_val -= ebx.uint_val;
// 	jg_d lineDrawn,direction
	if(direction == DRAW_LR) {
		if (ecx.int_val > 0) {
			goto lineDrawn;
		}
	} else {
		if (ecx.int_val < 0) {
			goto lineDrawn;
		}
	}

drawPixel:
// 	shr esi,16-pow2
	esi.uint_val >>= (16 - pow2);
// 	mov bl,[ebp+2*ecx]
	ebx.uint_val = ((uint16_t *)work.depth.base)[ebp.uint_val / 2 + ecx.int_val];

// 	shr eax,16
	eax.uint_val >>= 16;
// 	and esi,mask shl pow2
	esi.uint_val &= (mask << pow2);

// 	and eax,mask
	eax.uint_val &= mask;
// 	mov bh,[ebp+2*ecx+1]

// 	or eax,esi
	eax.uint_val |= esi.uint_val;
// 	mov esi,work.texture.base
	esi.uint_val = WORK_TEXTURE_BASE;

// 	cmp dx,bx ;two cycles
// 	ja noPlot
    if(edx.short_val[0] > ebx.short_val[0]) {
        goto noPlot;
    }


// 	mov al,[esi+eax]
	eax.uint_val = ((uint8_t *)work.texture.base)[esi.uint_val + eax.uint_val];

	// mov esi,work.blend_table
	esi.ptr_val = work.blend_table;

// 	test al,al
// 	jz noPlot
	if(eax.uint_val == 0) {
        goto noPlot;
    }

	// mov ah,[edi+ecx]
	eax.bytes[1] = ((uint8_t *)work.colour.base)[ecx.uint_val];

// 	mov [ebp+2*ecx],dl
// 	mov [ebp+2*ecx+1],dh
	((uint16_t *)work.depth.base)[ebp.uint_val / 2 + ecx.int_val] = edx.short_val[0];

	// and eax,0ffffh
	eax.uint_val &= 0xffff;
	// ;stall
	// ;stall
	// mov al,[esi+eax]
	eax.bytes[0] = ((uint8_t *)esi.ptr_val)[eax.uint_val];
	// ;stall

	// mov [edi+ecx],al
	((uint8_t *)work.colour.base)[edi.uint_val + ecx.uint_val] = eax.bytes[0];

noPlot:
// 	mov ebx,workspace.d_z_x
	ebx.uint_val = workspace.d_z_x;

// 	add_d edx,ebx,direction
	if (direction == DRAW_LR) {
		ADD_AND_SET_CF(edx.uint_val, ebx.uint_val);
	} else {
		SUB_AND_SET_CF(edx.uint_val, ebx.uint_val);
	}
// 	mov esi,workspace.c_v
	esi.uint_val = workspace.c_v;

// 	adc_d edx,0,direction
	if (direction == DRAW_LR) {
		ADC(edx.uint_val, 0);
	} else {
		SBB(edx.uint_val, 0);
	}
// 	mov ebx,workspace.d_v_x
	ebx.uint_val = workspace.d_v_x;

// 	add_d esi,ebx,direction
	if (direction == DRAW_LR) {
		esi.uint_val += ebx.uint_val;
	} else {
		esi.uint_val -= ebx.uint_val;
	}
// 	mov eax,workspace.c_u
	eax.uint_val = workspace.c_u;

// 	mov workspace.c_v,esi
	workspace.c_v = esi.uint_val;
// 	mov ebx,workspace.d_u_x
	ebx.uint_val = workspace.d_u_x;

// 	add_d eax,ebx,direction
	if (direction == DRAW_LR) {
		eax.uint_val += ebx.uint_val;
	} else {
		eax.uint_val -= ebx.uint_val;
	}
// 	inc_d ecx,direction
	if (direction == DRAW_LR) {
		ecx.uint_val++;
	} else {
		ecx.uint_val--;
	}

// 	mov workspace.c_u,eax
	workspace.c_u = eax.uint_val;
// 	jle_d drawPixel,direction
	if (direction == DRAW_LR) {
		if (ecx.int_val <= 0) {
			goto drawPixel;
		}
	} else {
		if (ecx.int_val >= 0) {
			goto drawPixel;
		}
	}

lineDrawn:
// ;perform per line updates

// 	mov ebp,workspace.xm_f
	ebp.uint_val = workspace.xm_f;
// 	mov edi,workspace.d_xm_f
	edi.uint_val = workspace.d_xm_f;

// 	add ebp,edi
	ADD_AND_SET_CF(ebp.uint_val, edi.uint_val);

// 	mov eax,workspace.s_u
	eax.uint_val = workspace.s_u;

// 	sbb edx,edx
	SBB(edx.uint_val, edx.uint_val);
// 	mov workspace.xm_f,ebp
	workspace.xm_f = ebp.uint_val;

// 	mov ebp,workspace.depthAddress
	ebp.uint_val = workspace.depthAddress;
// 	mov ecx,work.depth.stride_b
	ecx.uint_val = work.depth.stride_b;

// 	add ebp,ecx
	ebp.uint_val += ecx.uint_val;
// 	mov ecx,[workspace.d_u_y_0+edx*8]
	ecx.uint_val = ((uint32_t *)&workspace.d_u_y_0)[2 * edx.int_val];

// 	mov workspace.depthAddress,ebp
	workspace.depthAddress = ebp.uint_val;
// 	mov esi,workspace.s_v
	esi.uint_val = workspace.s_v;

// 	add eax,ecx
	eax.uint_val += ecx.uint_val;
// 	mov ebx,[workspace.d_v_y_0+edx*8]
	ebx.uint_val = ((uint32_t *)&workspace.d_v_y_0)[2 * edx.int_val];

// 	mov workspace.s_u,eax
	workspace.s_u = eax.uint_val;
// 	add esi,ebx
	esi.uint_val += ebx.uint_val;

// 	mov workspace.c_u,eax
	workspace.c_u = eax.uint_val;
// 	mov workspace.c_v,esi
	workspace.c_v = esi.uint_val;

// 	mov ebp,workspace.s_z
	ebp.uint_val = workspace.s_z;
// 	mov edx,[workspace.d_z_y_0+edx*8]
	edx.uint_val = ((uint32_t *)&workspace.d_z_y_0)[2 * edx.int_val];

// 	mov workspace.s_v,esi
	workspace.s_v = esi.uint_val;
// 	add edx,ebp
	edx.uint_val += ebp.uint_val;

// 	mov workspace.s_z,edx
	workspace.s_z = edx.uint_val;
// 	mov edi,workspace.scanAddress
	edi.uint_val = workspace.scanAddress;

// 	mov ebx,work.colour.stride_b
	ebx.uint_val = work.colour.stride_b;
// 	mov ecx,workspace.d_&minorX
	ecx.uint_val = *d_minorX;

// 	add edi,ebx
	edi.uint_val += ebx.uint_val;
// 	mov ebx,workspace.minorX
	ebx.uint_val = *minorX;

// 	mov workspace.scanAddress,edi
	workspace.scanAddress = edi.uint_val;
// 	add ebx,ecx
	ebx.uint_val += ecx.uint_val;

// 	mov workspace.minorX,ebx
	*minorX = ebx.uint_val;
// 	mov ecx,workspace.xm
	ecx.uint_val = workspace.xm;

// 	mov ebp,workspace.d_xm
	ebp.uint_val = workspace.d_xm;
// 	;unpaired instruction

// 	add ecx,ebp
	ecx.uint_val += ebp.uint_val;
// 	mov ebp,workspace.&half&Count
	ebp.uint_val = *halfCount;

// 	dec ebp
	ebp.uint_val--;
// 	mov workspace.xm,ecx
	workspace.xm = ecx.uint_val;

// 	mov workspace.&half&Count,ebp
	*halfCount = ebp.uint_val;
// 	jge drawLine
	if (*halfCount >= 0) {
		goto drawLine;
	}

done:

}

void BR_ASM_CALL TriangleRender_ZTB_I8_D16_POW2(brp_block *block, int pow2, int skip_setup, va_list va) {
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
    eax.uint_val = workspace.xm;
// 	mov ebx,workspace.d_xm
	ebx.uint_val = workspace.d_xm;

// 	faddp st(2),st					;	ca			da
	FADDP_ST(2, 0);
// 	;x-stall
// 	;x-stall

// 	shl eax,16
	eax.uint_val <<= 16;
// 	mov edx,workspace.d_z_x
	edx.uint_val = workspace.d_z_x;

// 	shl ebx,16
	ebx.uint_val <<= 16;
// 	mov workspace.xm_f,eax
	workspace.xm_f = eax.uint_val;

// 	fstp qword ptr workspace.scanAddress
	FSTP64(&workspace.scanAddress);
// 	fstp qword ptr workspace.depthAddress
	FSTP64(&workspace.depthAddress);

// 	mov workspace.d_xm_f,ebx
	workspace.d_xm_f = ebx.uint_val;
// 	cmp edx,80000000
	CMP(edx.uint_val, 80000000);

// 	adc edx,-1
	ADC(edx.uint_val, -1);

// 	mov	eax,workspace.flip
	eax.uint_val = workspace.flip;

// 	ror edx,16
	int16_t tmp = edx.short_val[0];
	edx.short_val[0] = edx.short_val[1];
	edx.short_val[1] = tmp;

// 	test eax,eax

// 	mov workspace.d_z_x,edx
	workspace.d_z_x = edx.uint_val;
// 	jnz	drawRL

// 	DRAW_ZT_I8_D16_POW2 x1,DRAW_LR,top,pow2
// 	DRAW_ZT_I8_D16_POW2 x2,DRAW_LR,bottom,pow2
// 	ret

// drawRL:
// 	DRAW_ZT_I8_D16_POW2 x1,DRAW_RL,top,pow2
// 	DRAW_ZT_I8_D16_POW2 x2,DRAW_RL,bottom,pow2
// 	ret
// eax is 0, ZF=1
	if (eax.uint_val == 0) {
		DRAW_ZTB_I8_D16_POW2 (&workspace.x1, &workspace.d_x1, DRAW_LR,&workspace.topCount,pow2);
		DRAW_ZTB_I8_D16_POW2 (&workspace.x2, &workspace.d_x2, DRAW_LR,&workspace.bottomCount,pow2);
	} else {
		DRAW_ZTB_I8_D16_POW2 (&workspace.x1, &workspace.d_x1, DRAW_RL,&workspace.topCount,pow2);
		DRAW_ZTB_I8_D16_POW2 (&workspace.x2, &workspace.d_x2, DRAW_RL,&workspace.bottomCount,pow2);
	}
}

void BR_ASM_CALL TriangleRender_ZTB_I8_D16_8(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZTB_I8_D16_16(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZTB_I8_D16_32(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZTB_I8_D16_64(brp_block *block, ...) {
    va_list     va;
    va_start(va, block);
	TriangleRender_ZTB_I8_D16_POW2(block, 6, 0, va);
	va_end(va);
}
void BR_ASM_CALL TriangleRender_ZTB_I8_D16_128(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZTB_I8_D16_256(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZTB_I8_D16_1024(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
