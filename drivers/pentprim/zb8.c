#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "brender.h"
#include <stdio.h>
#include "fpsetup.h"
#include "fpwork.h"
#include "work.h"
#include "x86emu.h"
#include "common.h"

// ;*****************
// ; FLAT RASTERISER
// ;*****************

void DRAW_Z_I8_D16(uint32_t *minorX, uint32_t *d_minorX, char direction, int32_t *halfCount) {
    // local drawPixel,drawLine,done,lineDrawn,noPlot

    // mov ebx,workspace.&half&Count
    ebx.v = *halfCount;
    // mov ebp,workspace.depthAddress
    ebp.v = workspace.depthAddress;
    // cmp ebx,0
    // jl done
    if (ebx.int_val < 0) {
        return;
    }
    // mov eax,workspace.s_z
    eax.v = workspace.s_z;
    // mov esi,workspace.d_z_x
    esi.v = workspace.d_z_x;
    // mov ebx,workspace.minorX
    ebx.v = *minorX;
    // mov ecx,workspace.xm
    ecx.v = workspace.xm;
drawLine:
    // ror eax,16
    ROR16(eax);
    // shr ebx,16
    ebx.v >>= 16;
    // mov edi,workspace.scanAddress
    edi.v = workspace.scanAddress;
    // shr ecx,16
    ecx.v >>= 16;
    // add edi,ebx
    edi.v += ebx.v;
    // sub ecx,ebx
    ecx.v -= ebx.v;
    // jg_d lineDrawn,direction
    JG_D(ecx.int_val, lineDrawn, direction);
    // lea ebp,[ebp+2*ebx]
    ebp.v = ebp.v + 2 * ebx.v;
    // xor ebx,ebx ;used to insure that cf is clear
    ebx.v = 0;
    x86_state.cf = 0;
    // mov edx,eax ;needed to insure upper parts of reg are correct for the first pixel
    edx.v = eax.v;
    // mov bl,byte ptr workspace.colour
    ebx.l = workspace.colour & 0xff;
drawPixel:
    // ;regs ebp,depth edi,dest esi,dz eax,c_z ebx, ecx,count edx,old_z
    // adc_d eax,0,direction
    ADC_D(eax.v, 0, direction);
    // mov dl,[ebp+2*ecx]
    // ;The following line needs some more experimentation to prove its usefullness in real application
    // mov dh,[ebp+2*ecx+1]
    edx.short_low = ((uint16_t *)work.depth.base)[ebp.v / 2 + ecx.int_val];
    // cmp eax,edx
    // ja noPlot
    if (eax.v > edx.v) {
        goto noPlot;
    }
    // ; writes
    // mov [ebp+2*ecx],ax
    ((uint16_t *)work.depth.base)[ebp.v / 2 + ecx.int_val] = eax.short_low;
    // mov [edi+ecx],bl
    ((uint8_t *)work.colour.base)[edi.v + ecx.v] = ebx.l;
noPlot:
    // add_d eax,esi,direction ;wastes a cycle here
    ADD_SET_CF_D(eax.v, esi.v, direction);
    // inc_d ecx,direction
    INC_D(ecx.v, direction);
    // mov edx,eax
    edx.v = eax.v;
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
    // mov eax,workspace.s_z
    eax.v = workspace.s_z;
    // sbb edi,edi
    SBB(edi.v, edi.v);
    // mov workspace.xm_f,ebp
    workspace.xm_f = ebp.v;
    // mov ebp,workspace.depthAddress
    ebp.v = workspace.depthAddress;
    // mov edx,workspace.&half&Count
    edx.v = *halfCount;
    // mov ebx,[workspace.d_z_y_0+edi*8]
    ebx.v = ((uint32_t *)&workspace.d_z_y_0)[2 * edi.int_val];
    // mov edi,workspace.scanAddress
    edi.v = workspace.scanAddress;
    // add eax,ebx
    ADD_AND_SET_CF(eax.v, ebx.v);
    // mov ebx,workspace.minorX
    ebx.v = *minorX;
    // adc eax,0
    ADC(eax.v, 0);
    // mov ecx,workspace.xm
    ecx.v = workspace.xm;
    // add ecx,workspace.d_xm
    ecx.v += workspace.d_xm;
    // add edi,work.colour.stride_b
    edi.v += work.colour.stride_b;
    // mov workspace.s_z,eax
    workspace.s_z = eax.v;
    // mov workspace.scanAddress,edi
    workspace.scanAddress = edi.v;
    // add ebp,work.depth.stride_b
    ebp.v += work.depth.stride_b;
    // add ebx,workspace.d_&minorX
    ebx.v += *d_minorX;
    // mov workspace.minorX,ebx
    *minorX = ebx.v;
    // mov workspace.xm,ecx
    workspace.xm = ecx.v;
    // dec edx
    edx.v--;
    // mov workspace.depthAddress,ebp
    workspace.depthAddress = ebp.v;
    // mov workspace.&half&Count,edx
    *halfCount = edx.v;
    // jge drawLine
    if (edx.int_val >= 0) {
        goto drawLine;
    }
}

void BR_ASM_CALL TriangleRender_Z_I8_D16(brp_block *block, ...) {
    brp_vertex *v0;
    brp_vertex *v1;
    brp_vertex *v2;
    va_list     va;
    va_start(va, block);
    v0 = va_arg(va, brp_vertex *);
    v1 = va_arg(va, brp_vertex *);
    v2 = va_arg(va, brp_vertex *);
    va_end(va);

    workspace.v0 = v0;
    workspace.v1 = v1;
    workspace.v2 = v2;

    // mov	eax,v0
    eax.ptr_v = v0;
	// mov	ecx,v1
    ecx.ptr_v = v1;
	// mov	edx,v2
    edx.ptr_v = v2;
	// mov workspace.v0,eax
    workspace.v0 = v0;
	// mov workspace.v1,ecx
    workspace.v1 = v1;
	// mov workspace.v2,edx
    workspace.v2 = v2;

    // ; half cycle wasted
    TriangleSetup_Z(v0, v1, v2);

    // ; Floating point address calculation - 20 cycles, (Integer=26)
    // ;										st(0)		st(1)		st(2)		st(3)		st(4)		st(5)		st(6)		st(7)
    // fild work.colour.base			;	cb
    FILD(0);
    // fild workspace.t_y				;	ty			cb
    FILD(workspace.t_y);
    // fild work.depth.base			;	db			ty			cb
    FILD(0);
    // fild work.colour.stride_b		;	cs			db			ty			cb
    FILD(work.colour.stride_b);
    // fild work.depth.stride_b		;	ds			cs			db			ty			cb
    FILD(work.depth.stride_b);
    // fxch st(4)						;	cb			cs			db			ty			ds
    FXCH(4);
    // fsub fp_one						;	cb-1		cs			db			ty			ds
    FSUB(fp_one);
    // fxch st(3)						;	ty			cs			db			cb-1		ds
    FXCH(3);
    // fsub fp_one						;	ty-1		cs			db			cb-1		ds
    FSUB(fp_one);
    // fxch st(2)						;	db			cs			ty-1		cb-1		ds
    FXCH(2);
    // fsub fp_two						;	db-2		cs			ty-1		cb-1		ds
    FSUB(fp_two);
    // fxch st(3)						;	cb-1		cs			ty-1		db-2		ds
    FXCH(3);
    // fadd fp_conv_d					;	cb-1I		cs			ty-1		db-2		ds
    FADD(fp_conv_d);
    // fxch st(1)						;	cs			cb-1I		ty-1		db-2		ds
    FXCH(1);
    // fmul st,st(2)					;	csy			cb-1I		ty-1		db-2		ds
    FMUL_ST(0, 2);
    // fxch st(3)						;	db-2		cb-1I		ty-1		csy			ds
    FXCH(3);
    // fadd fp_conv_d					;	db-2I		cb-1I		ty-1		csy			ds
    FADD(fp_conv_d);
    // fxch st(2)						;	ty-1		cb-1I		db-2I		csy			ds
    FXCH(2);
    // fmulp st(4),st					;	cb-1I		db-2I		csy			dsy
    FMULP_ST(4, 0);
    // faddp st(2),st					;	db-2I		ca			dsy
    FADDP_ST(2, 0);
    // ;stall
    // faddp st(2),st					;	ca			da
    FADDP_ST(2, 0);
    // fstp qword ptr workspace.scanAddress
    FSTP64(&workspace.scanAddress_double);
    // fstp qword ptr workspace.depthAddress
    FSTP64(&workspace.depthAddress_double);
    // mov eax,workspace.xm
    eax.v = workspace.xm;
    // shl eax,16
    eax.v <<= 16;
    // mov ebx,workspace.d_xm
    ebx.v = workspace.d_xm;
    // shl ebx,16
    ebx.v <<= 16;
    // mov workspace.xm_f,eax
    workspace.xm_f = eax.v;
    // mov edx,workspace.d_z_x
    edx.v = workspace.d_z_x;
    // cmp edx,80000000
    CMP(edx.v, 80000000);
    // adc edx,-1
    ADC(edx.v, -1);
    // ror edx,16
    ROR16(edx);
    // mov workspace.d_xm_f,ebx
    workspace.d_xm_f = ebx.v;
    // mov workspace.d_z_x,edx
    workspace.d_z_x = edx.v;
    // mov	eax,workspace.flip
    eax.v = workspace.flip;
    // ;half cycle wasted
    // test eax,eax
    // jnz	drawRL
    if (eax.v == 0) {
        // DRAW_Z_I8_D16 x1,DRAW_LR,top
        DRAW_Z_I8_D16(&workspace.x1, &workspace.d_x1, DRAW_LR, &workspace.topCount);
        // DRAW_Z_I8_D16 x2,DRAW_LR,bottom
        DRAW_Z_I8_D16(&workspace.x2, &workspace.d_x2, DRAW_LR, &workspace.bottomCount);
    } else {
        // DRAW_Z_I8_D16 x1,DRAW_RL,top
        DRAW_Z_I8_D16(&workspace.x1, &workspace.d_x1, DRAW_RL, &workspace.topCount);
        // DRAW_Z_I8_D16 x2,DRAW_RL,bottom
        DRAW_Z_I8_D16(&workspace.x2, &workspace.d_x2, DRAW_RL, &workspace.bottomCount);
    }
}

void BR_ASM_CALL TriangleRender_Z_I8_D16_ShadeTable(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
