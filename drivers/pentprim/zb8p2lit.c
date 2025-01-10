#include "brender.h"
#include "fpsetup.h"
#include "fpwork.h"
#include <stdarg.h>
#include "work.h"
#include "x86emu.h"
#include "common.h"

void DRAW_ZTI_I8_D16_POW2(uint32_t *minorX, uint32_t *d_minorX, char direction, int32_t *halfCount, int pow2) {
    // local drawPixel,drawLine,done,lineDrawn,noPlot,mask
    uint32_t mask=0;
// ; height test
	MAKE_N_LOW_BIT_MASK(&mask,pow2);

    // mov edx,workspace.&half&Count
    edx.v = *halfCount;
    // mov ebx,workspace.s_z
    ebx.v = workspace.s_z;
    // cmp edx,0
    // jl done
    if (edx.int_val < 0) {
        return;
    }
    // mov edx,workspace.minorX
    edx.v = *minorX;
    // mov esi,workspace.s_i
    esi.v = workspace.s_i;
    // mov ecx,workspace.xm
    ecx.v = workspace.xm;
    // mov eax,workspace.s_u
    eax.v = workspace.s_u;
    // mov workspace.c_i,esi
    workspace.c_i = esi.v;
    // mov edi,workspace.s_v
    edi.v = workspace.s_v;
    // mov workspace.c_u,eax
    workspace.c_u = eax.v;
    // mov workspace.c_v,edi
    workspace.c_v = edi.v;
    // mov ebp,workspace.depthAddress
    ebp.v = workspace.depthAddress;
drawLine:
    // shr edx,16
    edx.v >>= 16;
    // mov esi,workspace.scanAddress
    esi.v = workspace.scanAddress;
    // shr ecx,16
    ecx.v >>= 16;
    // add esi,edx
    esi.v += edx.v;
    // lea ebp,[ebp+2*edx]
    ebp.v += edx.v * 2;
    // mov workspace.scratch0,esi
    workspace.scratch0 = esi.v;
    // sub ecx,edx
    ecx.v -= edx.v;
    // jg_d lineDrawn,direction
    JG_D(ecx.int_val, lineDrawn, direction);
    // ror ebx,16
    ROR16(ebx);
    // mov workspace.scratch1,ebp
    workspace.scratch1 = ebp.v;
    // mov edi,workspace.s_v
    edi.v = workspace.s_v;

drawPixel:
    // shr eax,16
    eax.v >>= 16;
    // mov ebp,workspace.scratch1
    ebp.v = workspace.scratch1;
    // shr edi,16-pow2
    edi.v >>= (16-pow2);
    // and eax,mask
    eax.v &= mask;
    // mov esi,work.texture.base
    esi.v = WORK_TEXTURE_BASE;
    // and edi,mask shl pow2
    edi.v &= (mask << pow2);
    // mov dl,[ebp+2*ecx]
    edx.v = ((uint16_t *)work.depth.base)[ebp.v / 2 + ecx.int_val]; //grab both bytes at once
    // or eax,edi
    eax.v |= edi.v;
    // mov dh,[ebp+2*ecx+1]
    // -
    // mov edi,work.shade_table
    edi.v = WORK_SHADE_BASE;
    // cmp bx,dx ;two cycles

    // mov al,[esi+eax]
    eax.l = ((uint8_t *)work.texture.base)[esi.v + eax.v];
    // mov ah,byte ptr[workspace.c_i+2]
    eax.h = BYTE2(workspace.c_i);
    // ja noPlot
    if (ebx.short_low > edx.short_low) {
        goto noPlot;
    }
    // test al,al
    int original_pixel_is_0 = eax.l == 0;
    // mov esi,workspace.scratch0
    esi.v = workspace.scratch0;
    // mov al,[edi+eax]
    eax.l = ((uint8_t *)work.shade_table)[edi.v + eax.v]; // get lit pixel
    // jz noPlot
    if (original_pixel_is_0) {
        goto noPlot;
    }
    // mov [ebp+2*ecx],bx ;two cycles
    ((uint16_t *)work.depth.base)[ebp.v / 2 + ecx.int_val] = ebx.short_low;
    // mov [esi+ecx],al
    ((uint8_t *)work.colour.base)[esi.v + ecx.v] = eax.l;

noPlot:
    // mov eax,workspace.c_i
    eax.v = workspace.c_i;
    // mov ebp,workspace.d_z_x
    ebp.v = workspace.d_z_x;
    // add_d ebx,ebp,direction
    ADD_SET_CF_D(ebx.v, ebp.v, direction);
    // mov esi,workspace.d_i_x
    esi.v = workspace.d_i_x;
    // adc_d ebx,0,direction
    ADC_D(ebx.v, 0, direction);
    // mov edi,workspace.c_v
    edi.v = workspace.c_v;
    // add_d eax,esi,direction
    ADD_D(eax.v, esi.v, direction);
    // mov ebp,workspace.d_v_x
    ebp.v = workspace.d_v_x;
    // add_d edi,ebp,direction
    ADD_D(edi.v, ebp.v, direction);
    // mov workspace.c_i,eax
    workspace.c_i = eax.v;
    // mov eax,workspace.c_u
    eax.v = workspace.c_u;
    // mov ebp,workspace.d_u_x
    ebp.v = workspace.d_u_x;
    // add_d eax,ebp,direction
    ADD_D(eax.v, ebp.v, direction);
    // mov workspace.c_v,edi
    workspace.c_v = edi.v;
    // mov workspace.c_u,eax
    workspace.c_u = eax.v;
    // inc_d ecx,direction
    INC_D(ecx.v, direction);
    // ; cycle wasted
    // jle_d drawPixel,direction
    JLE_D(ecx.int_val, drawPixel, direction);

lineDrawn:
    // ;perform per line updates

    // mov ebp,workspace.xm_f
    ebp.v = workspace.xm_f;
    // mov edi,workspace.d_xm_f
    edi.v = workspace.d_xm_f;
    // add ebp,edi
    ADD_AND_SET_CF(ebp.v, edi.v);
    // mov edi,workspace.s_i
    edi.v = workspace.s_i;
    // sbb edx,edx
    SBB(edx.v, edx.v);
    // mov workspace.xm_f,ebp
    workspace.xm_f = ebp.v;
    // mov ebx,workspace.s_z
    ebx.v = workspace.s_z;
    // mov ebp,workspace.depthAddress
    ebp.v = workspace.depthAddress;
    // mov esi,[workspace.d_i_y_0+edx*8]
    esi.v = ((uint32_t *)&workspace.d_i_y_0)[2 * edx.int_val];
    // mov eax,workspace.s_u
    eax.v = workspace.s_u;
    // add edi,esi
    edi.v += esi.v;
    // mov ecx,[workspace.d_z_y_0+edx*8]
    ecx.v = ((uint32_t *)&workspace.d_z_y_0)[2 * edx.int_val];
    // mov workspace.c_i,edi
    workspace.c_i = edi.v;
    // add ebx,ecx
    ebx.v += ecx.v;
    // mov workspace.s_i,edi
    workspace.s_i = edi.v;
    // mov workspace.s_z,ebx
    workspace.s_z = ebx.v;
    // mov ecx,[workspace.d_u_y_0+edx*8]
    ecx.v = ((uint32_t *)&workspace.d_u_y_0)[2 * edx.int_val];
    // mov edi,workspace.s_v
    edi.v = workspace.s_v;
    // add eax,ecx
    eax.v += ecx.v;
    // mov esi,[workspace.d_v_y_0+edx*8]
    esi.v = ((uint32_t *)&workspace.d_v_y_0)[2 * edx.int_val];
    // mov workspace.s_u,eax
    workspace.s_u = eax.v;
    // add edi,esi
    edi.v += esi.v;
    // mov workspace.c_u,eax
    workspace.c_u = eax.v;
    // mov workspace.c_v,edi
    workspace.c_v = edi.v;
    // mov workspace.s_v,edi
    workspace.s_v = edi.v;
    // mov ecx,work.depth.stride_b
    ecx.v = work.depth.stride_b;
    // add ebp,ecx
    ebp.v += ecx.v;
    // mov edi,workspace.scanAddress
    edi.v = workspace.scanAddress;
    // mov workspace.depthAddress,ebp
    workspace.depthAddress = ebp.v;
    // mov esi,work.colour.stride_b
    esi.v = work.colour.stride_b;
    // add edi,esi
    edi.v += esi.v;
    // mov edx,workspace.minorX
    edx.v = *minorX;
    // mov workspace.scanAddress,edi
    workspace.scanAddress = edi.v;
    // mov ecx,workspace.d_&minorX
    ecx.v = *d_minorX;
    // add edx,ecx
    edx.v += ecx.v;
    // mov ecx,workspace.xm
    ecx.v = workspace.xm;
    // mov esi,workspace.d_xm
    esi.v = workspace.d_xm;
    // mov workspace.minorX,edx
    *minorX = edx.v;
    // add ecx,esi
    ecx.v += esi.v;
    // mov esi,workspace.&half&Count
    esi.v = *halfCount;
    // dec esi
    esi.v--;
    // mov workspace.xm,ecx
    workspace.xm = ecx.v;
    // mov workspace.&half&Count,esi
    *halfCount = esi.v;
    // jge drawLine
    if (esi.int_val >= 0) {
        goto drawLine;
    }
}

// #include <stdio.h>
// static void print_brp_vertex(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2) {
//     printf("v0->flags = %d;\n", v0->flags);
//     printf("v1->flags = %d;\n", v1->flags);
//     printf("v2->flags = %d;\n", v2->flags);
//     for (int i = 0; i < 16; i++) {
//         printf("v0->comp_f[%d] = %f;\n", i, v0->comp_f[i]);
//         printf("v1->comp_f[%d] = %f;\n", i, v1->comp_f[i]);
//         printf("v2->comp_f[%d] = %f;\n", i, v2->comp_f[i]);
//     }
// }

void BR_ASM_CALL TriangleRender_ZTI_I8_D16_POW2(brp_block *block, int pow2, int skip_setup, va_list va) {
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

		TriangleSetup_ZTI(v0, v1, v2);
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
		DRAW_ZTI_I8_D16_POW2 (&workspace.x1, &workspace.d_x1, DRAW_LR,&workspace.topCount,pow2);
		DRAW_ZTI_I8_D16_POW2 (&workspace.x2, &workspace.d_x2, DRAW_LR,&workspace.bottomCount,pow2);
	} else {
		DRAW_ZTI_I8_D16_POW2 (&workspace.x1, &workspace.d_x1, DRAW_RL,&workspace.topCount,pow2);
		DRAW_ZTI_I8_D16_POW2 (&workspace.x2, &workspace.d_x2, DRAW_RL,&workspace.bottomCount,pow2);
	}
}

void BR_ASM_CALL TriangleRender_ZTI_I8_D16_8(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZTI_I8_D16_16(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZTI_I8_D16_32(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZTI_I8_D16_64(brp_block *block, ...) {
    va_list     va;
    va_start(va, block);
	TriangleRender_ZTI_I8_D16_POW2(block, 6, 0, va);
	va_end(va);
}
void BR_ASM_CALL TriangleRender_ZTI_I8_D16_128(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZTI_I8_D16_256(brp_block *block, ...) {
    va_list     va;
    va_start(va, block);
	TriangleRender_ZTI_I8_D16_POW2(block, 8, 0, va);
	va_end(va);
}
void BR_ASM_CALL TriangleRender_ZTI_I8_D16_1024(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
