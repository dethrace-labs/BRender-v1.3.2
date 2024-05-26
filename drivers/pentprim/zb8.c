#include "priminfo.h"
#include <stdarg.h>
#include "work.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "fpsetup.h"
#include "work.h"
#include "x86emu.h"
#include "common.h"

// ;*****************
// ; FLAT RASTERISER
// ;*****************

void DRAW_Z_I8_D16(uint32_t *minorX, uint32_t *d_minorX, char direction, int32_t *halfCount) {
    // local drawPixel,drawLine,done,lineDrawn,noPlot

    // mov ebx,workspace.&half&Count
    ebx.uint_val = *halfCount;
    // mov ebp,workspace.depthAddress
    ebp.uint_val = workspace.depthAddress;
    // cmp ebx,0
    // jl done
    if (ebx.int_val < 0) {
        return;
    }
    // mov eax,workspace.s_z
    eax.uint_val = workspace.s_z;
    // mov esi,workspace.d_z_x
    esi.uint_val = workspace.d_z_x;
    // mov ebx,workspace.minorX
    ebx.uint_val = *minorX;
    // mov ecx,workspace.xm
    ecx.uint_val = workspace.xm;
drawLine:
    // ror eax,16
    ROR16(eax);
    // shr ebx,16
    ebx.uint_val >>= 16;
    // mov edi,workspace.scanAddress
    edi.uint_val = workspace.scanAddress;
    // shr ecx,16
    ecx.uint_val >>= 16;
    // add edi,ebx
    edi.uint_val += ebx.uint_val;
    // sub ecx,ebx
    ecx.uint_val -= ebx.uint_val;
    // jg_d lineDrawn,direction
    JG_D(ecx.int_val, lineDrawn, direction);
    // lea ebp,[ebp+2*ebx]
    ebp.uint_val = ebp.uint_val + 2 * ebx.uint_val;
    // xor ebx,ebx ;used to insure that cf is clear
    ebx.uint_val = 0;
    x86_state.cf = 0;
    // mov edx,eax ;needed to insure upper parts of reg are correct for the first pixel
    edx.uint_val = eax.uint_val;
    // mov bl,byte ptr workspace.colour
    ebx.bytes[0] = workspace.colour & 0xff;
drawPixel:
    // ;regs ebp,depth edi,dest esi,dz eax,c_z ebx, ecx,count edx,old_z
    // adc_d eax,0,direction
    ADC_D(eax.uint_val, 0, direction);
    // mov dl,[ebp+2*ecx]
    // ;The following line needs some more experimentation to prove its usefullness in real application
    // mov dh,[ebp+2*ecx+1]
    edx.short_val[0] = ((uint16_t *)work.depth.base)[ebp.uint_val / 2 + ecx.int_val];
    // cmp eax,edx
    // ja noPlot
    if (eax.uint_val > edx.uint_val) {
        goto noPlot;
    }
    // ; writes
    // mov [ebp+2*ecx],ax
    ((uint16_t *)work.depth.base)[ebp.uint_val / 2 + ecx.int_val] = eax.short_val[0];
    // mov [edi+ecx],bl
    ((uint8_t *)work.colour.base)[edi.uint_val + ecx.uint_val] = ebx.bytes[0];
noPlot:
    // add_d eax,esi,direction ;wastes a cycle here
    if (direction == DIR_F) {
        ADD_AND_SET_CF(eax.uint_val, esi.uint_val);
    } else {
        SUB_AND_SET_CF(eax.uint_val, esi.uint_val);
    }
    // inc_d ecx,direction
    INC_D(ecx.uint_val, direction);
    // mov edx,eax
    edx.uint_val = eax.uint_val;
    // jle_d drawPixel,direction
    JLE_D(ecx.int_val, drawPixel, direction);
lineDrawn:
    // ;perform per line updates
    // mov ebp,workspace.xm_f
    ebp.uint_val = workspace.xm_f;
    // mov edi,workspace.d_xm_f
    edi.uint_val = workspace.d_xm_f;
    // add ebp,edi
    ADD_AND_SET_CF(ebp.uint_val, edi.uint_val);
    // mov eax,workspace.s_z
    eax.uint_val = workspace.s_z;
    // sbb edi,edi
    SBB(edi.uint_val, edi.uint_val);
    // mov workspace.xm_f,ebp
    workspace.xm_f = ebp.uint_val;
    // mov ebp,workspace.depthAddress
    ebp.uint_val = workspace.depthAddress;
    // mov edx,workspace.&half&Count
    edx.uint_val = *halfCount;
    // mov ebx,[workspace.d_z_y_0+edi*8]
    ebx.uint_val = ((uint32_t *)&workspace.d_z_y_0)[2 * edi.int_val];
    // mov edi,workspace.scanAddress
    edi.uint_val = workspace.scanAddress;
    // add eax,ebx
    ADD_AND_SET_CF(eax.uint_val, ebx.uint_val);
    // mov ebx,workspace.minorX
    ebx.uint_val = *minorX;
    // adc eax,0
    ADC(eax.uint_val, 0);
    // mov ecx,workspace.xm
    ecx.uint_val = workspace.xm;
    // add ecx,workspace.d_xm
    ecx.uint_val += workspace.d_xm;
    // add edi,work.colour.stride_b
    edi.uint_val += work.colour.stride_b;
    // mov workspace.s_z,eax
    workspace.s_z = eax.uint_val;
    // mov workspace.scanAddress,edi
    workspace.scanAddress = edi.uint_val;
    // add ebp,work.depth.stride_b
    ebp.uint_val += work.depth.stride_b;
    // add ebx,workspace.d_&minorX
    ebx.uint_val += *d_minorX;
    // mov workspace.minorX,ebx
    *minorX = ebx.uint_val;
    // mov workspace.xm,ecx
    workspace.xm = ecx.uint_val;
    // dec edx
    edx.uint_val--;
    // mov workspace.depthAddress,ebp
    workspace.depthAddress = ebp.uint_val;
    // mov workspace.&half&Count,edx
    *halfCount = edx.uint_val;
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
    eax.ptr_val = v0;
	// mov	ecx,v1
    ecx.ptr_val = v1;
	// mov	edx,v2
    edx.ptr_val = v2;
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
    FSTP64(&workspace.scanAddress);
    // fstp qword ptr workspace.depthAddress
    FSTP64(&workspace.depthAddress);
    // mov eax,workspace.xm
    eax.uint_val = workspace.xm;
    // shl eax,16
    eax.uint_val <<= 16;
    // mov ebx,workspace.d_xm
    ebx.uint_val = workspace.d_xm;
    // shl ebx,16
    ebx.uint_val <<= 16;
    // mov workspace.xm_f,eax
    workspace.xm_f = eax.uint_val;
    // mov edx,workspace.d_z_x
    edx.uint_val = workspace.d_z_x;
    // cmp edx,80000000
    CMP(edx.uint_val, 80000000);
    // adc edx,-1
    ADC(edx.uint_val, -1);
    // ror edx,16
    ROR16(edx);
    // mov workspace.d_xm_f,ebx
    workspace.d_xm_f = ebx.uint_val;
    // mov workspace.d_z_x,edx
    workspace.d_z_x = edx.uint_val;
    // mov	eax,workspace.flip
    eax.uint_val = workspace.flip;
    // ;half cycle wasted
    // test eax,eax
    // jnz	drawRL
    if (eax.uint_val == 0) {
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
