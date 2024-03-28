#include "priminfo.h"
#include <stdarg.h>
#include "work.h"
#include "zb8awtm.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "fpsetup.h"
#include "work.h"

void TriangleRender_ZT_I8_D16(brp_block *block, ...)
{
    brp_vertex *v0; // [esp+18h] [ebp+Ch]
    brp_vertex *v1; // [esp+1Ch] [ebp+10h]
    brp_vertex *v2; // [esp+20h] [ebp+14h]
    va_list     va; // [esp+24h] [ebp+18h] BYREF
    va_start(va, block);
    v0 = va_arg(va, brp_vertex *);
    v1 = va_arg(va, brp_vertex *);
    v2 = va_arg(va, brp_vertex *);
    va_end(va);

    workspace.v0 = v0;
    workspace.v1 = v1;
    workspace.v2 = v2;

    // printf("TriangleRender_ZT_I8_D16\n");

    TriangleSetup_ZT_ARBITRARY(v0, v1, v2);

    //     ; Floating point address calculation - 20 cycles, (Integer=26)
    // ;										st(0)		st(1)		st(2)		st(3)		st(4)		st(5) st(6) st(7)

    intptr_t cb = 0; //(intptr_t)work.colour.base;
    intptr_t db = 0; //(intptr_t)work.depth.base;

    // 	fild work.colour.base			;	cb
    fild(cb);
    // 	fild workspace.t_y				;	ty			cb
    fild(workspace.t_y);
    // 	fild work.depth.base			;	db			ty			cb
    fild(db);
    // 	fild work.colour.stride_b		;	cs			db			ty			cb
    fild(work.colour.stride_b);
    // 	fild work.depth.stride_b		;	ds			cs			db			ty			cb
    fild(work.depth.stride_b);
    // 	fxch st(4)						;	cb			cs			db			ty			ds
    fxch(4);
    // 	fsub fp_one						;	cb-1		cs			db			ty			ds
    fsub(fp_one);
    // 	 fxch st(3)						;	ty			cs			db			cb-1		ds
    fxch(3);
    // 	fsub fp_one						;	ty-1		cs			db			cb-1		ds
    fsub(fp_one);
    // 	 fxch st(2)						;	db			cs			ty-1		cb-1		ds
    fxch(2);
    // 	fsub fp_two						;	db-2		cs			ty-1		cb-1		ds
    fsub(fp_two);
    // 	 fxch st(3)						;	cb-1		cs			ty-1		db-2		ds
    fxch(3);
    // 	fadd fp_conv_d					;	cb-1I		cs			ty-1		db-2		ds
    fadd(x87_op_mem32(&fp_conv_d));
    // 	 fxch st(1)						;	cs			cb-1I		ty-1		db-2		ds
    fxch(1);
    // 	fmul st,st(2)					;	csy			cb-1I		ty-1		db-2		ds
    fmul_2(x87_op_i(0), x87_op_i(2));
    // 	 fxch st(3)						;	db-2		cb-1I		ty-1		csy			ds
    fxch(3);
    // 	fadd fp_conv_d					;	db-2I		cb-1I		ty-1		csy			ds
    fadd(x87_op_mem32(&fp_conv_d));
    // 	 fxch st(2)						;	ty-1		cb-1I		db-2I		csy			ds
    fxch(2);
    // 	fmulp st(4),st					;	cb-1I		db-2I		csy			dsy
    fmulp_2(x87_op_i(4), x87_op_i(0));
    // 	faddp st(2),st					;	db-2I		ca			dsy
    faddp(x87_op_i(2));
    // 	;stall
    // 	faddp st(2),st					;	ca			da
    faddp(x87_op_i(2));
    // 	fstp qword ptr workspace.scanAddress
    fstp(x87_op_mem64(&workspace.scanAddress));
    // 	fstp qword ptr workspace.depthAddress
    fstp(x87_op_mem64(&workspace.depthAddress));

    // 	mov eax,work.texture.base
    mov(x86_op_reg(eax), x86_op_imm(0)); // TODO?
    // 	mov ebx,workspaceA.sv
    mov(x86_op_reg(ebx), x86_op_mem32(&workspaceA.sv));

    // 	add ebx,eax
    add(x86_op_reg(ebx), x86_op_reg(eax));
    // 	mov eax,workspace.xm
    mov(x86_op_reg(eax), x86_op_mem32(&workspace.xm));

    // 	shl eax,16
    shl(x86_op_reg(eax), 16);
    // 	mov workspaceA.sv,ebx
    mov(x86_op_mem32(&workspaceA.sv), x86_op_reg(ebx));

    // 	mov	edx,workspaceA.flags
    mov(x86_op_reg(edx), x86_op_mem32(&workspaceA.flags));
    // 	mov ebx,workspace.d_xm
    mov(x86_op_reg(ebx), x86_op_mem32(&workspace.d_xm));

    // 	shl ebx,16
    shl(x86_op_reg(ebx), 16);
    // 	mov workspace.xm_f,eax
    mov(x86_op_mem32(&workspace.xm_f), x86_op_reg(eax));

    // 	mov workspace.d_xm_f,ebx
    mov(x86_op_mem32(&workspace.d_xm_f), x86_op_reg(ebx));
    // 	jmp ecx
    switch(edx->uint_val) {
        case 0:
            Draw_ZT_I8_NWLR();
            break;
        case 1:
            Draw_ZT_I8_NWRL();
            break;
        case 2:
            Draw_ZT_I8_DWLR();
            break;
        case 3:
            Draw_ZT_I8_DWRL();
            break;
        default:
            exit(1);
    }
}

void Draw_ZT_I8_NWLR()
{
    DRAW_ZT_I8(&workspace.x1, &workspace.d_x1, DRAW_LR, &workspace.topCount, WRAPPED, 0, 0);
    DRAW_ZT_I8(&workspace.x2, &workspace.d_x2, DRAW_LR, &workspace.bottomCount, WRAPPED, 0, 0);
}

void Draw_ZT_I8_NWRL()
{
    DRAW_ZT_I8(&workspace.x1, &workspace.d_x1, DRAW_RL, &workspace.topCount, WRAPPED, 0, 0);
    DRAW_ZT_I8(&workspace.x2, &workspace.d_x2, DRAW_RL, &workspace.bottomCount, WRAPPED, 0, 0);
}

void Draw_ZT_I8_DWLR()
{
    DRAW_ZT_I8(&workspace.x1, &workspace.d_x1, DRAW_LR, &workspace.topCount, WRAPPED, 0, 0);
    DRAW_ZT_I8(&workspace.x2, &workspace.d_x2, DRAW_LR, &workspace.bottomCount, WRAPPED, 0, 0);
}

void Draw_ZT_I8_DWRL()
{
    DRAW_ZT_I8(&workspace.x1, &workspace.d_x1, DRAW_RL, &workspace.topCount, WRAPPED, 0, 0);
    DRAW_ZT_I8(&workspace.x2, &workspace.d_x2, DRAW_RL, &workspace.bottomCount, WRAPPED, 0, 0);
}

// DRAW_ZT_I8 macro minorX,direction,half,wrap_flag,fogging,blend
void DRAW_ZT_I8(uint32_t *minorX, uint32_t *d_minorX, char direction, int32_t *halfCount, char wrap_flag, char fogging, char blend)
{
    int      cf = 0;
    uint32_t orig;
    // 	local drawPixel,drawLine,done,lineDrawn,noPlot,noCarry,returnAddress
    // 	local uPerPixelNoWrapNegative,uPerPixelNoWrapPositive,vPerPixelNoWrapNegative,vPerPixelNoWrapPositive
    // 	local uAboveRetest,uBelowRetest,vAboveRetest,vBelowRetest
    // ; height test

    // 	mov ebx,workspace.&half&Count
    // mov(x86_op_reg(ebx), x86_op_mem32(halfCount));
    ebx->int_val = *halfCount;
    // 	lea	eax,returnAddress
    // 	mov workspaceA.retAddress,eax

    // 	cmp ebx,0
    // 	jl done
    if(ebx->int_val < 0) {
        goto done;
    }

    // 	mov eax,workspaceA.su
    eax->uint_val = workspaceA.su;
    // 	mov ebx,workspaceA.svf
    ebx->uint_val = workspaceA.svf;

    // 	mov ecx,workspace.s_z
    ecx->uint_val = workspace.s_z;
    // 	mov workspace.c_u,eax
    workspace.c_u = eax->uint_val;
    // 	mov workspace.c_v,ebx
    workspace.c_v = ebx->uint_val;
    // 	mov workspace.c_z,ecx
    workspace.c_z = ecx->uint_val;

    // 	mov ebx,workspace.minorX
    ebx->uint_val = *minorX;
    // 	mov ecx,workspace.xm
    ecx->uint_val = workspace.xm;

drawLine:
    // 	mov ebp,workspace.depthAddress
    ebp->uint_val = workspace.depthAddress;

    // 	shr ebx,16
    // shr(x86_op_reg(ebx), 16);
    ebx->uint_val >>= 16;
    // 	mov edi,workspace.scanAddress
    edi->uint_val = workspace.scanAddress;

    // 	shr ecx,16
    // shr(x86_op_reg(ecx), 16);
    ecx->uint_val >>= 16;
    // 	add edi,ebx
    edi->uint_val += ebx->uint_val;

    // 	sub ecx,ebx
    ecx->uint_val -= ebx->uint_val;

    // ifidni <wrap_flag>,<WRAPPED>
    if(wrap_flag == WRAPPED) {
        // 	jg_d lineDrawn,direction
        if(direction == DRAW_LR) {
            // jg address
            if(ecx->int_val > 0) {
                goto lineDrawn;
            }
        } else {
            // jl address
            if(ecx->int_val < 0) {
                goto lineDrawn;
            }
        }
        // else
    } else {
        exit(1);
        // if blend
        // if fogging
        // 	jg_d half&ZeroWidthHandler_ZTFB,direction
        // else
        // 	jg_d half&ZeroWidthHandler_ZTB,direction
        // endif
        // else
        // if fogging
        // 	jg_d half&ZeroWidthHandler_ZTF,direction
        // else
        // 	jg_d half&ZeroWidthHandler_ZT,direction
        // endif
        // endif
        // endif
    }

    // 	mov esi,workspaceA.sv
    esi->uint_val = workspaceA.sv;
    // 	mov eax,workspaceA.su
    eax->uint_val = workspaceA.su;

    // 	shr eax,16
    // shr(x86_op_reg(eax), 16);
    eax->uint_val >>= 16;
    // 	lea ebp,[ebp+2*ebx]
    ebp->uint_val += ebx->uint_val * 2;
    // TODO?

drawPixel:
    // 	mov bx,[ebp+2*ecx]
    ebx->uint_val = ((uint16_t *)work.depth.base)[ebp->uint_val / 2 + ecx->int_val];
    // 	mov dx,word ptr workspace.c_z+2
    edx->uint_val = ((uint16_t *)&workspace.c_z)[1];

    // 	cmp dx,bx
    // 	ja noPlot
    if(edx->uint_val > ebx->uint_val) {
        goto noPlot;
    }

    // 	mov bl,[esi+eax]
    ebx->uint_val = ((uint8_t *)work.texture.base)[esi->uint_val + eax->uint_val];
    // 	test bl,bl
    // 	jz noPlot
    if(ebx->uint_val == 0) {
        goto noPlot;
    }

    // ; writes
    // if fogging
    // if blend
    //     mov eax,work.fog_table
    //     mov bh,dh

    //     mov bl,[ebx+eax]
    //     mov eax,work.blend_table

    //     mov bh,[edi+ecx]

    //     mov bl,[eax+ebx]

    //     mov [edi+ecx],bl
    // else
    //     mov eax,work.fog_table
    //     mov bh,dh

    //     mov [ebp+2*ecx],dx
    //     mov bl,[ebx+eax]

    //     mov [edi+ecx],bl
    // endif
    // else
    // if blend
    //     mov eax,work.blend_table
    //     mov bh,[edi+ecx]

    //     mov bl,[eax+ebx]

    //     mov [edi+ecx],bl
    // else
    //     mov [ebp+2*ecx],dx
    // printf("depth %d: val %d (%d) ", (ebp->uint_val + ecx->int_val) / 2, *(uint16_t *)&edx->uint_val,
    //        ((uint16_t *)work.depth.base)[ebp->uint_val / 2 + ecx->int_val]);

    ((uint16_t *)work.depth.base)[ebp->uint_val / 2 + ecx->int_val] = *(uint16_t *)&edx->uint_val;

    // // 	   mov [edi+ecx],bl
    ((uint8_t *)work.colour.base)[edi->uint_val + ecx->uint_val] = ebx->bytes[0];
    // printf("screen %d\n ", (edi->uint_val + ecx->int_val));

    // endif
    // endif

noPlot:

    // 	mov edx,workspace.c_v
    edx->uint_val = workspace.c_v;
    // 	add_d edx,workspaceA.dvxf,direction
    cf = 0;
    if(direction == DRAW_LR) {
        // add(x86_op_reg(edx), x86_op_mem32(&workspaceA.dvxf));

        edx->uint_val += workspaceA.dvxf;
        if(edx->uint_val < workspaceA.dvxf) {
            cf = 1;
        }
    } else {
        // sub(x86_op_reg(edx), x86_op_mem32(&workspaceA.dvxf));
        if(workspaceA.dvxf > edx->uint_val) {
            cf = 1;
        }
        edx->uint_val -= workspaceA.dvxf;
    }
    // 	mov workspace.c_v,edx
    workspace.c_v = edx->uint_val;

    // 	sbb edx,edx
    // sbb(x86_op_reg(edx), x86_op_reg(edx));
    edx->uint_val = edx->uint_val - (edx->uint_val + cf);

    // 	add_d esi,[workspaceA.dvx+8*edx],direction
    if(direction == DRAW_LR) {
        // add(x86_op_reg(esi), x86_op_imm(((uint32_t *)&workspaceA.dvx)[2 * edx->int_val]));
        esi->uint_val += ((uint32_t *)&workspaceA.dvx)[2 * edx->int_val];
    } else {
        // sub(x86_op_reg(esi), x86_op_imm(((uint32_t *)&workspaceA.dvx)[2 * edx->int_val]));
        esi->uint_val -= ((uint32_t *)&workspaceA.dvx)[2 * edx->int_val];
    }
    // ifidni <wrap_flag>,<WRAPPED>
vBelowRetest:
    // 	cmp esi,work.texture.base
    // 	jae vPerPixelNoWrapNegative
    if(esi->int_val >= WORK_TEXTURE_BASE) {
        goto vPerPixelNoWrapNegative;
    }
    // 	add esi,work.texture._size
    esi->int_val += work.texture.size;
    // 	jmp vBelowRetest
    goto vBelowRetest;
vPerPixelNoWrapNegative:
vAboveRetest:
    // 	cmp esi,workspaceA.vUpperBound
    // 	jb vPerPixelNoWrapPositive
    if(esi->int_val < workspaceA.vUpperBound) {
        goto vPerPixelNoWrapPositive;
    }
    // 	sub esi,work.texture._size
    esi->int_val -= work.texture.size;
    // 	jmp vAboveRetest
    goto vAboveRetest;
vPerPixelNoWrapPositive:
    //  endif

    // 	mov eax,workspace.c_u
    eax->uint_val = workspace.c_u;
    // 	add_d eax,workspaceA.dux,direction
    if(direction == DRAW_LR) {
        // add(x86_op_reg(eax), x86_op_mem32(&workspaceA.dux));
        eax->uint_val += workspaceA.dux;
    } else {
        // sub(x86_op_reg(eax), x86_op_mem32(&workspaceA.dux));
        eax->uint_val -= workspaceA.dux;
    }
    // ifidni <wrap_flag>,<WRAPPED>
uBelowRetest:
    // 	cmp eax,0
    // 	jge uPerPixelNoWrapNegative
    if(eax->int_val >= 0) {
        goto uPerPixelNoWrapNegative;
    }
    // 	add eax,workspaceA.uUpperBound
    eax->int_val += workspaceA.uUpperBound;
    // 	jmp uBelowRetest
    goto uBelowRetest;
uPerPixelNoWrapNegative:
uAboveRetest:
    // 	cmp eax,workspaceA.uUpperBound
    // 	jl uPerPixelNoWrapPositive
    if(eax->int_val < workspaceA.uUpperBound) {
        goto uPerPixelNoWrapPositive;
    }
    // 	sub eax,workspaceA.uUpperBound
    eax->int_val -= workspaceA.uUpperBound;
    // 	jmp uAboveRetest
    goto uAboveRetest;
uPerPixelNoWrapPositive:
    // endif
    // 	mov workspace.c_u,eax
    workspace.c_u = eax->uint_val;
    // 	sar eax,16
    // sar(x86_op_reg(eax), 16);
    eax->int_val >>= 16;

    // 	mov edx,workspace.c_z
    edx->uint_val = workspace.c_z;
    // 	add_d edx,workspace.d_z_x,direction
    if(direction == DRAW_LR) {
        // add(x86_op_reg(edx), x86_op_mem32(&workspace.d_z_x));
        edx->uint_val += workspace.d_z_x;
    } else {
        // sub(x86_op_reg(edx), x86_op_mem32(&workspace.d_z_x));
        edx->uint_val -= workspace.d_z_x;
    }
    // 	mov workspace.c_z,edx
    workspace.c_z = edx->uint_val;
    // 	shr edx,16
    // shr(x86_op_reg(edx), 16);
    edx->uint_val >>= 16;

    // 	inc_d ecx,direction
    if(direction == DRAW_LR) {
        ecx->int_val++;
    } else {
        ecx->int_val--;
    }

    // 	jle_d drawPixel,direction
    if(direction == DRAW_LR) {
        if(ecx->int_val <= 0) {
            goto drawPixel;
        }
    } else {
        if(ecx->int_val >= 0) {
            goto drawPixel;
        }
    }

lineDrawn:
    // ;perform per line updates

    // 	PER_SCAN_ZT half,wrap_flag,minorX
    PER_SCAN_ZT(halfCount, wrap_flag, minorX, d_minorX);

returnAddress:
    // 	mov workspace.&half&Count,edx
    // 	jge drawLine
    *halfCount = edx->int_val;
    if(edx->int_val >= 0) {
        goto drawLine;
    }

done:
    ecx->uint_val = 0;
    // endm
}

// PER_SCAN_ZT macro half,wrap_flag,minorX
void PER_SCAN_ZT(int32_t *halfCount, char wrap_flag, uint32_t *minorX, uint32_t *d_minorX)
{
    // 	local uPerLineNoWrapNegative,uPerLineNoWrapPositive,vPerLineNoWrapNegative,vPerLineNoWrapPositive
    // 	local uAboveRetest,uBelowRetest,vAboveRetest,vBelowRetest

    // printf("PER_SCAN_ZT\n");

    // 	mov ebp,workspace.xm_f
    ebp->uint_val = workspace.xm_f;
    // 	mov edi,workspace.d_xm_f
    edi->uint_val = workspace.d_xm_f;

    // 	add ebp,edi
    add(x86_op_reg(ebp), x86_op_reg(edi));
    // 	mov eax,workspaceA.svf
    eax->uint_val = workspaceA.svf;

    // 	sbb edi,edi
    sbb(x86_op_reg(edi), x86_op_reg(edi));
    // 	mov workspace.xm_f,ebp
    workspace.xm_f = ebp->uint_val;

    // 	mov esi,edi
    esi->uint_val = edi->uint_val;
    // 	mov ecx,workspace.s_z
    ecx->uint_val = workspace.s_z;

    // 	mov edx,workspaceA.su
    edx->uint_val = workspaceA.su;
    // 	mov ebp,[workspace.d_z_y_0+8*edi]
    if(edi->uint_val != 0) {
        int a = 0;
    }
    ebp->uint_val = ((uint32_t *)&workspace.d_z_y_0)[2 * edi->int_val];

    // 	add edx,[workspaceA.duy0+8*edi]
    edx->uint_val += ((uint32_t *)&workspaceA.duy0)[2 * edi->int_val];
    // 	add eax,[workspaceA.dvy0f+4*edi]
    add(x86_op_reg(eax), x86_op_imm(((uint32_t *)&workspaceA.dvy0f)[edi->int_val]));

    // 	rcl esi,1
    rcl(x86_op_reg(esi), 1);
    // 	mov workspaceA.svf,eax
    workspaceA.svf = eax->uint_val;

    // 	mov workspace.c_v,eax
    workspace.c_v = eax->uint_val;
    // 	add ecx,ebp
    ecx->uint_val += ebp->uint_val;

    // 	mov eax,workspaceA.sv
    eax->uint_val = workspaceA.sv;
    // 	mov workspace.s_z,ecx
    workspace.s_z = ecx->uint_val;

    // 	mov workspace.c_z,ecx
    workspace.c_z = ecx->uint_val;
    // 	nop

    // 	mov ebx,workspace.minorX
    ebx->uint_val = *minorX;
    // 	mov ecx,workspace.xm
    ecx->uint_val = workspace.xm;

    // 	add eax,[workspaceA.dvy0+8*esi]
    eax->uint_val += ((uint32_t *)&workspaceA.dvy0)[2 * esi->int_val];
    // 	add ebx,workspace.d_&minorX
    ebx->uint_val += *d_minorX;

// ifidni <wrap_flag>,<WRAPPED>
vBelowRetest:
    // 	cmp eax,work.texture.base
    // 	jae vPerLineNoWrapNegative
    if(eax->int_val >= WORK_TEXTURE_BASE) {
        goto vPerLineNoWrapNegative;
    }

    // 	add eax,work.texture._size
    eax->int_val += work.texture.size;
    // 	jmp vBelowRetest
    goto vBelowRetest;
vPerLineNoWrapNegative:
vAboveRetest:
    // 	cmp eax,workspaceA.vUpperBound
    // 	jb vPerLineNoWrapPositive
    if(eax->int_val < workspaceA.vUpperBound) {
        goto vPerLineNoWrapPositive;
    }
    // 	sub eax,work.texture._size
    eax->int_val -= work.texture.size;
    // 	jmp vAboveRetest
    goto vAboveRetest;
vPerLineNoWrapPositive:

uBelowRetest:
    // 	cmp edx,0
    // 	jge uPerLineNoWrapNegative
    if(edx->int_val >= 0) {
        goto uPerLineNoWrapNegative;
    }
    // 	add edx,workspaceA.uUpperBound
    edx->uint_val += workspaceA.uUpperBound;
    // 	jmp uBelowRetest
    goto uBelowRetest;
uPerLineNoWrapNegative:
uAboveRetest:
    // 	cmp edx,workspaceA.uUpperBound
    // 	jl uPerLineNoWrapPositive
    if(edx->int_val < workspaceA.uUpperBound) {
        goto uPerLineNoWrapPositive;
    }
    // 	sub edx,workspaceA.uUpperBound
    edx->uint_val -= workspaceA.uUpperBound;
    // 	jmp uAboveRetest
    goto uAboveRetest;
uPerLineNoWrapPositive:
    // endif

    // 	mov workspaceA.sv,eax
    workspaceA.sv = eax->uint_val;
    // 	mov ebp,workspace.depthAddress
    ebp->uint_val = workspace.depthAddress;

    // 	add ebp,work.depth.stride_b ;two cycles
    ebp->uint_val += work.depth.stride_b;
    // 	mov workspaceA.su,edx
    workspaceA.su = edx->uint_val;

    // 	mov workspace.c_u,edx
    workspace.c_u = edx->uint_val;
    // 	mov workspace.minorX,ebx
    *minorX = ebx->uint_val;

    // 	mov edi,workspace.scanAddress
    edi->uint_val = workspace.scanAddress;
    // 	mov edx,workspace.&half&Count
    edx->uint_val = *halfCount;

    // 	add ecx,workspace.d_xm
    ecx->uint_val += workspace.d_xm;
    // 	add edi,work.colour.stride_b
    edi->uint_val += work.colour.stride_b;

    // 	mov workspace.xm,ecx
    workspace.xm = ecx->uint_val;
    // 	mov workspace.scanAddress,edi
    workspace.scanAddress = edi->uint_val;

    // 	dec edx
    edx->uint_val--;
    // 	mov workspace.depthAddress,ebp
    workspace.depthAddress = ebp->uint_val;

    // endm
}


void BR_ASM_CALL TriangleRender_ZTI_I8_D16(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZTI_I8_D16_FLAT(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZTIF_I8_D16(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZTIF_I8_D16_FLAT(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZTIB_I8_D16(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZTIB_I8_D16_FLAT(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZTIFB_I8_D16(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZTIFB_I8_D16_FLAT(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}

void BR_ASM_CALL TriangleRender_ZTF_I8_D16(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZTB_I8_D16(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZTFB_I8_D16(brp_block *block, brp_vertex *a,brp_vertex *b,brp_vertex *c) {
    // Not implemented
    BrAbort();
}
