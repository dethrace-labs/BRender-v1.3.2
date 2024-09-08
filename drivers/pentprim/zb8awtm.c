#include "priminfo.h"
#include <stdarg.h>
#include "work.h"
#include "x86emu.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "fpsetup.h"
#include "fpwork.h"
#include "common.h"

enum tDraw_ZT_I8_direction {
    Draw_ZT_I8_NWLR,
    Draw_ZT_I8_NWRL,
    Draw_ZT_I8_DWLR,
    Draw_ZT_I8_DWRL
};


// ; Z-Buffered Arbitrary Width Textured
static inline void PER_SCAN_ZT(int32_t *halfCount, char wrap_flag, uint32_t *minorX, uint32_t *d_minorX)
{
    // 	mov ebp,workspace.xm_f
    ebp.v = workspace.xm_f;
    // 	mov edi,workspace.d_xm_f
    edi.v = workspace.d_xm_f;

    // 	add ebp,edi
    ADD_AND_SET_CF(ebp.v, edi.v);
    // 	mov eax,workspaceA.svf
    eax.v = workspaceA.svf;

    // 	sbb edi,edi
    SBB(edi.v, edi.v);
    // 	mov workspace.xm_f,ebp
    workspace.xm_f = ebp.v;

    // 	mov esi,edi
    esi.v = edi.v;
    // 	mov ecx,workspace.s_z
    ecx.v = workspace.s_z;

    // 	mov edx,workspaceA.su
    edx.v = workspaceA.su;
    // 	mov ebp,[workspace.d_z_y_0+8*edi]
    ebp.v = ((uint32_t *)&workspace.d_z_y_0)[2 * edi.int_val];

    // 	add edx,[workspaceA.duy0+8*edi]
    edx.v += ((uint32_t *)&workspaceA.duy0)[2 * edi.int_val];
    // 	add eax,[workspaceA.dvy0f+4*edi]
    ADD_AND_SET_CF(eax.v, ((uint32_t *)&workspaceA.dvy0f)[edi.int_val]);

    // 	rcl esi,1
    RCL_1(esi.v);
    // 	mov workspaceA.svf,eax
    workspaceA.svf = eax.v;

    // 	mov workspace.c_v,eax
    workspace.c_v = eax.v;
    // 	add ecx,ebp
    ecx.v += ebp.v;

    // 	mov eax,workspaceA.sv
    eax.v = workspaceA.sv;
    // 	mov workspace.s_z,ecx
    workspace.s_z = ecx.v;

    // 	mov workspace.c_z,ecx
    workspace.c_z = ecx.v;
    // 	nop

    // 	mov ebx,workspace.minorX
    ebx.v = *minorX;
    // 	mov ecx,workspace.xm
    ecx.v = workspace.xm;

    // 	add eax,[workspaceA.dvy0+8*esi]
    eax.v += ((uint32_t *)&workspaceA.dvy0)[2 * esi.int_val];
    // 	add ebx,workspace.d_&minorX
    ebx.v += *d_minorX;

// ifidni <wrap_flag>,<WRAPPED>
vBelowRetest:
    // 	cmp eax,work.texture.base
    // 	jae vPerLineNoWrapNegative
    if(eax.int_val >= WORK_TEXTURE_BASE) {
        goto vPerLineNoWrapNegative;
    }

    // 	add eax,work.texture._size
    eax.int_val += work.texture.size;
    // 	jmp vBelowRetest
    goto vBelowRetest;
vPerLineNoWrapNegative:
vAboveRetest:
    // 	cmp eax,workspaceA.vUpperBound
    // 	jb vPerLineNoWrapPositive
    if(eax.int_val < workspaceA.vUpperBound) {
        goto vPerLineNoWrapPositive;
    }
    // 	sub eax,work.texture._size
    eax.int_val -= work.texture.size;
    // 	jmp vAboveRetest
    goto vAboveRetest;
vPerLineNoWrapPositive:

uBelowRetest:
    // 	cmp edx,0
    // 	jge uPerLineNoWrapNegative
    if(edx.int_val >= 0) {
        goto uPerLineNoWrapNegative;
    }
    // 	add edx,workspaceA.uUpperBound
    edx.v += workspaceA.uUpperBound;
    // 	jmp uBelowRetest
    goto uBelowRetest;
uPerLineNoWrapNegative:
uAboveRetest:
    // 	cmp edx,workspaceA.uUpperBound
    // 	jl uPerLineNoWrapPositive
    if(edx.int_val < workspaceA.uUpperBound) {
        goto uPerLineNoWrapPositive;
    }
    // 	sub edx,workspaceA.uUpperBound
    edx.v -= workspaceA.uUpperBound;
    // 	jmp uAboveRetest
    goto uAboveRetest;
uPerLineNoWrapPositive:
    // endif

    // 	mov workspaceA.sv,eax
    workspaceA.sv = eax.v;
    // 	mov ebp,workspace.depthAddress
    ebp.v = workspace.depthAddress;

    // 	add ebp,work.depth.stride_b ;two cycles
    ebp.v += work.depth.stride_b;
    // 	mov workspaceA.su,edx
    workspaceA.su = edx.v;

    // 	mov workspace.c_u,edx
    workspace.c_u = edx.v;
    // 	mov workspace.minorX,ebx
    *minorX = ebx.v;

    // 	mov edi,workspace.scanAddress
    edi.v = workspace.scanAddress;
    // 	mov edx,workspace.&half&Count
    edx.v = *halfCount;

    // 	add ecx,workspace.d_xm
    ecx.v += workspace.d_xm;
    // 	add edi,work.colour.stride_b
    edi.v += work.colour.stride_b;

    // 	mov workspace.xm,ecx
    workspace.xm = ecx.v;
    // 	mov workspace.scanAddress,edi
    workspace.scanAddress = edi.v;

    // 	dec edx
    edx.v--;
    // 	mov workspace.depthAddress,ebp
    workspace.depthAddress = ebp.v;

    // endm
}

// ; Arbitrary Width Lit Textured
static inline void PER_SCAN_ZTI(int32_t *halfCount, char wrap_flag, uint32_t *minorX, uint32_t *d_minorX)
{
    // 	mov ebp,workspace.xm_f
    ebp.v = workspace.xm_f;
    // 	mov edi,workspace.d_xm_f
    edi.v = workspace.d_xm_f;

    // 	add ebp,edi
    ADD_AND_SET_CF(ebp.v, edi.v);
    // 	mov eax,workspaceA.svf
    eax.v = workspaceA.svf;

    // 	sbb edi,edi
    SBB(edi.v, edi.v);
    // mov ecx,workspace.s_i
    ecx.v = workspace.s_i;
    // 	mov workspace.xm_f,ebp
    workspace.xm_f = ebp.v;
    // 	mov esi,edi
    esi.v = edi.v;

    // add ecx,[workspace.d_i_y_0+8*edi]
    ecx.v += ((uint32_t *)&workspace.d_i_y_0)[2 * edi.int_val];
	// mov edx,workspaceA.su
    edx.v = workspaceA.su;
	// mov workspace.s_i,ecx
    workspace.s_i = ecx.v;
	// mov ebp,[workspace.d_z_y_0+8*edi]
    ebp.v = ((uint32_t *)&workspace.d_z_y_0)[2 * edi.int_val];
	// mov workspace.c_i,ecx
    workspace.c_i = ecx.v;
	// mov ecx,workspace.s_z
    ecx.v = workspace.s_z;

    // 	add edx,[workspaceA.duy0+8*edi]
    edx.v += ((uint32_t *)&workspaceA.duy0)[2 * edi.int_val];
    // 	add eax,[workspaceA.dvy0f+4*edi]
    ADD_AND_SET_CF(eax.v, ((uint32_t *)&workspaceA.dvy0f)[edi.int_val]);

    // 	rcl esi,1
    RCL_1(esi.v);
    // 	mov workspaceA.svf,eax
    workspaceA.svf = eax.v;

    // 	mov workspace.c_v,eax
    workspace.c_v = eax.v;
    // 	add ecx,ebp
    ecx.v += ebp.v;

    // 	mov eax,workspaceA.sv
    eax.v = workspaceA.sv;
    // 	mov workspace.s_z,ecx
    workspace.s_z = ecx.v;

    // 	mov workspace.c_z,ecx
    workspace.c_z = ecx.v;
    // 	nop

    // 	mov ebx,workspace.minorX
    ebx.v = *minorX;
    // 	mov ecx,workspace.xm
    ecx.v = workspace.xm;

    // 	add eax,[workspaceA.dvy0+8*esi]
    eax.v += ((uint32_t *)&workspaceA.dvy0)[2 * esi.int_val];
    // 	add ebx,workspace.d_&minorX
    ebx.v += *d_minorX;

// ifidni <wrap_flag>,<WRAPPED>
vBelowRetest:
    // 	cmp eax,work.texture.base
    // 	jae vPerLineNoWrapNegative
    if(eax.int_val >= WORK_TEXTURE_BASE) {
        goto vPerLineNoWrapNegative;
    }

    // 	add eax,work.texture._size
    eax.int_val += work.texture.size;
    // 	jmp vBelowRetest
    goto vBelowRetest;
vPerLineNoWrapNegative:
vAboveRetest:
    // 	cmp eax,workspaceA.vUpperBound
    // 	jb vPerLineNoWrapPositive
    if(eax.int_val < workspaceA.vUpperBound) {
        goto vPerLineNoWrapPositive;
    }
    // 	sub eax,work.texture._size
    eax.int_val -= work.texture.size;
    // 	jmp vAboveRetest
    goto vAboveRetest;
vPerLineNoWrapPositive:

uBelowRetest:
    // 	cmp edx,0
    // 	jge uPerLineNoWrapNegative
    if(edx.int_val >= 0) {
        goto uPerLineNoWrapNegative;
    }
    // 	add edx,workspaceA.uUpperBound
    edx.v += workspaceA.uUpperBound;
    // 	jmp uBelowRetest
    goto uBelowRetest;
uPerLineNoWrapNegative:
uAboveRetest:
    // 	cmp edx,workspaceA.uUpperBound
    // 	jl uPerLineNoWrapPositive
    if(edx.int_val < workspaceA.uUpperBound) {
        goto uPerLineNoWrapPositive;
    }
    // 	sub edx,workspaceA.uUpperBound
    edx.v -= workspaceA.uUpperBound;
    // 	jmp uAboveRetest
    goto uAboveRetest;
uPerLineNoWrapPositive:
    // endif

    // 	mov workspaceA.sv,eax
    workspaceA.sv = eax.v;
    // 	mov ebp,workspace.depthAddress
    ebp.v = workspace.depthAddress;

    // 	add ebp,work.depth.stride_b ;two cycles
    ebp.v += work.depth.stride_b;
    // 	mov workspaceA.su,edx
    workspaceA.su = edx.v;

    // 	mov workspace.c_u,edx
    workspace.c_u = edx.v;
    // 	mov workspace.minorX,ebx
    *minorX = ebx.v;

    // 	mov edi,workspace.scanAddress
    edi.v = workspace.scanAddress;
    // 	mov edx,workspace.&half&Count
    edx.v = *halfCount;

    // 	add ecx,workspace.d_xm
    ecx.v += workspace.d_xm;
    // 	add edi,work.colour.stride_b
    edi.v += work.colour.stride_b;

    // 	mov workspace.xm,ecx
    workspace.xm = ecx.v;
    // 	mov workspace.scanAddress,edi
    workspace.scanAddress = edi.v;

    // 	dec edx
    edx.v--;
    // 	mov workspace.depthAddress,ebp
    workspace.depthAddress = ebp.v;
}

// DRAW_ZT_I8 macro minorX,direction,half,wrap_flag,fogging,blend
static inline void DRAW_ZT_I8(uint32_t *minorX, uint32_t *d_minorX, char direction, int32_t *halfCount, char wrap_flag, tFog_enabled fogging, tBlend_enabled blend)
{
    // 	mov ebx,workspace.&half&Count
    ebx.int_val = *halfCount;
    // 	lea	eax,returnAddress
    // 	mov workspaceA.retAddress,eax

    // 	cmp ebx,0
    // 	jl done
    if(ebx.int_val < 0) {
        return;
    }

    // 	mov eax,workspaceA.su
    eax.v = workspaceA.su;
    // 	mov ebx,workspaceA.svf
    ebx.v = workspaceA.svf;

    // 	mov ecx,workspace.s_z
    ecx.v = workspace.s_z;
    // 	mov workspace.c_u,eax
    workspace.c_u = eax.v;
    // 	mov workspace.c_v,ebx
    workspace.c_v = ebx.v;
    // 	mov workspace.c_z,ecx
    workspace.c_z = ecx.v;

    // 	mov ebx,workspace.minorX
    ebx.v = *minorX;
    // 	mov ecx,workspace.xm
    ecx.v = workspace.xm;

drawLine:
    // 	mov ebp,workspace.depthAddress
    ebp.v = workspace.depthAddress;

    // 	shr ebx,16
    ebx.v >>= 16;
    // 	mov edi,workspace.scanAddress
    edi.v = workspace.scanAddress;

    // 	shr ecx,16
    ecx.v >>= 16;
    // 	add edi,ebx
    edi.v += ebx.v;
    // 	sub ecx,ebx
    ecx.v -= ebx.v;

    // 	jg_d lineDrawn,direction
    JG_D(ecx.int_val, lineDrawn, direction);

    // 	mov esi,workspaceA.sv
    esi.v = workspaceA.sv;
    // 	mov eax,workspaceA.su
    eax.v = workspaceA.su;

    // 	shr eax,16
    eax.v >>= 16;
    // 	lea ebp,[ebp+2*ebx]
    ebp.v += ebx.v * 2;

drawPixel:
    // 	mov bx,[ebp+2*ecx]
    ebx.short_low = ((uint16_t *)work.depth.base)[ebp.v / 2 + ecx.int_val];
    // 	mov dx,word ptr workspace.c_z+2
    edx.short_low = workspace.c_z >> 16;

    // 	cmp dx,bx
    // 	ja noPlot
    if(edx.short_low > ebx.short_low) {
        goto noPlot;
    }

    // 	mov bl,[esi+eax]
    ebx.l = ((uint8_t *)work.texture.base)[esi.v + eax.v];
    // 	test bl,bl
    // 	jz noPlot
    if(ebx.l == 0) {
        goto noPlot;
    }

    // ; writes
    // if fogging
    if (fogging == eFog_yes) {
        // if blend
        if (blend == eBlend_yes) {
            BrFailure("Not implemented\n");
            //     mov eax,work.fog_table
            //     mov bh,dh
            //     mov bl,[ebx+eax]
            //     mov eax,work.blend_table
            //     mov bh,[edi+ecx]
            //     mov bl,[eax+ebx]
            //     mov [edi+ecx],bl
        // else
        } else {
            BrFailure("Not implemented\n");
            //     mov eax,work.fog_table
            //     mov bh,dh
            //     mov [ebp+2*ecx],dx
            //     mov bl,[ebx+eax]
            //     mov [edi+ecx],bl
            // endif
        }
    // else
    } else {
        // if blend
        if (blend == eBlend_yes) {
            //     mov eax,work.blend_table
            //     mov bh,[edi+ecx]
            ebx.h = ((uint8_t *)work.colour.base)[edi.v + ecx.v];
            //     mov bl,[eax+ebx]
            ebx.l = ((uint8_t *)work.blend_table)[ebx.v];
            //     mov [edi+ecx],bl
            ((uint8_t *)work.colour.base)[edi.v + ecx.v] = ebx.l;
        // else
        } else {
            //     mov [ebp+2*ecx],dx
            ((uint16_t *)work.depth.base)[ebp.v / 2 + ecx.int_val] = edx.short_low;
            // 	   mov [edi+ecx],bl
            ((uint8_t *)work.colour.base)[edi.v + ecx.v] = ebx.l;
        // endif
        }
    // endif
    }

noPlot:

    // 	mov edx,workspace.c_v
    edx.v = workspace.c_v;
    // 	add_d edx,workspaceA.dvxf,direction
    if(direction == DRAW_LR) {
        ADD_AND_SET_CF(edx.v, workspaceA.dvxf);
    } else {
        SUB_AND_SET_CF(edx.v, workspaceA.dvxf);
    }
    // 	mov workspace.c_v,edx
    workspace.c_v = edx.v;
    // 	sbb edx,edx
    SBB(edx.v, edx.v);

    // 	add_d esi,[workspaceA.dvx+8*edx],direction
    ADD_D(esi.v, ((uint32_t *)&workspaceA.dvx)[2 * edx.int_val], direction);
    // ifidni <wrap_flag>,<WRAPPED>
vBelowRetest:
    // 	cmp esi,work.texture.base
    // 	jae vPerPixelNoWrapNegative
    if(esi.int_val >= WORK_TEXTURE_BASE) {
        goto vPerPixelNoWrapNegative;
    }
    // 	add esi,work.texture._size
    esi.v += work.texture.size;
    // 	jmp vBelowRetest
    goto vBelowRetest;
vPerPixelNoWrapNegative:
vAboveRetest:
    // 	cmp esi,workspaceA.vUpperBound
    // 	jb vPerPixelNoWrapPositive
    if(esi.int_val < workspaceA.vUpperBound) {
        goto vPerPixelNoWrapPositive;
    }
    // 	sub esi,work.texture._size
    esi.v -= work.texture.size;
    // 	jmp vAboveRetest
    goto vAboveRetest;
vPerPixelNoWrapPositive:
    //  endif

    // 	mov eax,workspace.c_u
    eax.v = workspace.c_u;
    // 	add_d eax,workspaceA.dux,direction
    ADD_D(eax.v, workspaceA.dux, direction);
    // ifidni <wrap_flag>,<WRAPPED>
uBelowRetest:
    // 	cmp eax,0
    // 	jge uPerPixelNoWrapNegative
    if(eax.int_val >= 0) {
        goto uPerPixelNoWrapNegative;
    }
    // 	add eax,workspaceA.uUpperBound
    eax.v += workspaceA.uUpperBound;
    // 	jmp uBelowRetest
    goto uBelowRetest;
uPerPixelNoWrapNegative:
uAboveRetest:
    // 	cmp eax,workspaceA.uUpperBound
    // 	jl uPerPixelNoWrapPositive
    if(eax.int_val < workspaceA.uUpperBound) {
        goto uPerPixelNoWrapPositive;
    }
    // 	sub eax,workspaceA.uUpperBound
    eax.int_val -= workspaceA.uUpperBound;
    // 	jmp uAboveRetest
    goto uAboveRetest;
uPerPixelNoWrapPositive:
    // endif
    // 	mov workspace.c_u,eax
    workspace.c_u = eax.v;
    // 	sar eax,16
    eax.int_val >>= 16;
    // 	mov edx,workspace.c_z
    edx.v = workspace.c_z;
    // 	add_d edx,workspace.d_z_x,direction
    ADD_D(edx.v, workspace.d_z_x, direction);
    // 	mov workspace.c_z,edx
    workspace.c_z = edx.v;
    // 	shr edx,16
    edx.v >>= 16;

    // 	inc_d ecx,direction
    INC_D(ecx.v, direction);

    // 	jle_d drawPixel,direction
    JLE_D(ecx.int_val, drawPixel, direction);

lineDrawn:
    // ;perform per line updates
    // 	PER_SCAN_ZT half,wrap_flag,minorX
    PER_SCAN_ZT(halfCount, wrap_flag, minorX, d_minorX);

returnAddress:
    // 	mov workspace.&half&Count,edx
    *halfCount = edx.int_val;
    // 	jge drawLine
    if(edx.int_val >= 0) {
        goto drawLine;
    }
}

// DRAW_ZT_I8 macro minorX,direction,half,wrap_flag,fogging,blend
static inline void DRAW_ZTI_I8(uint32_t *minorX, uint32_t *d_minorX, char direction, int32_t *halfCount, char wrap_flag, tFog_enabled fogging, tBlend_enabled blend)
{
    // 	mov ebx,workspace.&half&Count
    ebx.int_val = *halfCount;
    // 	lea	eax,returnAddress
    // 	mov workspaceA.retAddress,eax

    // 	cmp ebx,0
    // 	jl done
    if(ebx.int_val < 0) {
        return;
    }

    // 	mov eax,workspaceA.su
    eax.v = workspaceA.su;
    // 	mov ebx,workspaceA.svf
    ebx.v = workspaceA.svf;

    // 	mov ecx,workspace.s_z
    ecx.v = workspace.s_z;
    // 	mov workspace.c_u,eax
    workspace.c_u = eax.v;
    // 	mov workspace.c_v,ebx
    workspace.c_v = ebx.v;
    // mov ebx,workspace.s_i
    ebx.v = workspace.s_i;
    // 	mov workspace.c_z,ecx
    workspace.c_z = ecx.v;
    // mov workspace.c_i,ebx
    workspace.c_i = ebx.v;

    // 	mov ebx,workspace.minorX
    ebx.v = *minorX;
    // 	mov ecx,workspace.xm
    ecx.v = workspace.xm;

drawLine:
    // 	mov ebp,workspace.depthAddress
    ebp.v = workspace.depthAddress;

    // 	shr ebx,16
    ebx.v >>= 16;
    // 	mov edi,workspace.scanAddress
    edi.v = workspace.scanAddress;

    // 	shr ecx,16
    ecx.v >>= 16;
    // 	add edi,ebx
    edi.v += ebx.v;
    // 	sub ecx,ebx
    ecx.v -= ebx.v;

    // 	jg_d lineDrawn,direction
    JG_D(ecx.int_val, lineDrawn, direction);

    // 	mov esi,workspaceA.sv
    esi.v = workspaceA.sv;
    // 	mov eax,workspaceA.su
    eax.v = workspaceA.su;

    // 	shr eax,16
    eax.v >>= 16;
    // 	lea ebp,[ebp+2*ebx]
    ebp.v += ebx.v * 2;

drawPixel:
    // 	mov bx,[ebp+2*ecx]
    ebx.short_low = ((uint16_t *)work.depth.base)[ebp.v / 2 + ecx.int_val];
    // 	mov dx,word ptr workspace.c_z+2
    edx.short_low = workspace.c_z >> 16;

    // 	cmp dx,bx
    // 	ja noPlot
    if(edx.short_low > ebx.short_low) {
        goto noPlot;
    }

    // 	mov bl,[esi+eax]
    ebx.l = ((uint8_t *)work.texture.base)[esi.v + eax.v];

    // mov eax,work.shade_table
    //eax.ptr_val = work.shade_table;

	// mov bh,byte ptr workspace.c_i+2
    ebx.h = BYTE2(workspace.c_i);

    // 	test bl,bl
    // 	jz noPlot
    if(ebx.l == 0) {
        goto noPlot;
    }

    // ; writes
    // if fogging
    if (fogging == eFog_yes) {
        // if blend
        if (blend == eBlend_yes) {
            BrFailure("Not implemented\n");
            //     mov eax,work.fog_table
            //     mov bh,dh
            //     mov bl,[ebx+eax]
            //     mov eax,work.blend_table
            //     mov bh,[edi+ecx]
            //     mov bl,[eax+ebx]
            //     mov [edi+ecx],bl
        // else
        } else {
            BrFailure("Not implemented\n");
            //     mov eax,work.fog_table
            //     mov bh,dh
            //     mov [ebp+2*ecx],dx
            //     mov bl,[ebx+eax]
            //     mov [edi+ecx],bl
            // endif
        }
    // else
    } else {
        // if blend
        if (blend == eBlend_yes) {
            // mov bl,byte ptr [eax+ebx]
            ebx.l = ((uint8_t *)work.shade_table)[ebx.v];
            //     mov eax,work.blend_table
            //     mov bh,[edi+ecx]
            ebx.h = ((uint8_t *)work.colour.base)[edi.v + ecx.v];
            //     mov bl,[eax+ebx]
            ebx.l = ((uint8_t *)work.blend_table)[ebx.v];
            //     mov [edi+ecx],bl
            ((uint8_t *)work.colour.base)[edi.v + ecx.v] = ebx.l;
        // else
        } else {
            // mov bl,byte ptr [eax+ebx]
            ebx.l = ((uint8_t *)work.shade_table)[ebx.v];
	        // mov [ebp+2*ecx],dx
            ((uint16_t *)work.depth.base)[ebp.v / 2 + ecx.int_val] = edx.short_low;
            // 	   mov [edi+ecx],bl
            ((uint8_t *)work.colour.base)[edi.v + ecx.v] = ebx.l;
        // endif
        }
    // endif
    }

noPlot:
    // mov eax,workspace.c_i
    eax.v = workspace.c_i;
    // 	mov edx,workspace.c_v
    edx.v = workspace.c_v;

    // add_d eax,workspace.d_i_x,direction
    ADD_D(eax.v, workspace.d_i_x, direction);
    // 	add_d edx,workspaceA.dvxf,direction
    if(direction == DRAW_LR) {
        ADD_AND_SET_CF(edx.v, workspaceA.dvxf);
    } else {
        SUB_AND_SET_CF(edx.v, workspaceA.dvxf);
    }
    // mov workspace.c_i,eax
    workspace.c_i = eax.v;
    // 	mov workspace.c_v,edx
    workspace.c_v = edx.v;
    // 	sbb edx,edx
    SBB(edx.v, edx.v);

    // 	add_d esi,[workspaceA.dvx+8*edx],direction
    ADD_D(esi.v, ((uint32_t *)&workspaceA.dvx)[2 * edx.int_val], direction);
    // ifidni <wrap_flag>,<WRAPPED>
vBelowRetest:
    // 	cmp esi,work.texture.base
    // 	jae vPerPixelNoWrapNegative
    if(esi.int_val >= WORK_TEXTURE_BASE) {
        goto vPerPixelNoWrapNegative;
    }
    // 	add esi,work.texture._size
    esi.v += work.texture.size;
    // 	jmp vBelowRetest
    goto vBelowRetest;
vPerPixelNoWrapNegative:
vAboveRetest:
    // 	cmp esi,workspaceA.vUpperBound
    // 	jb vPerPixelNoWrapPositive
    if(esi.int_val < workspaceA.vUpperBound) {
        goto vPerPixelNoWrapPositive;
    }
    // 	sub esi,work.texture._size
    esi.v -= work.texture.size;
    // 	jmp vAboveRetest
    goto vAboveRetest;
vPerPixelNoWrapPositive:
    //  endif

    // 	mov eax,workspace.c_u
    eax.v = workspace.c_u;
    // 	add_d eax,workspaceA.dux,direction
    ADD_D(eax.v, workspaceA.dux, direction);
    // ifidni <wrap_flag>,<WRAPPED>
uBelowRetest:
    // 	cmp eax,0
    // 	jge uPerPixelNoWrapNegative
    if(eax.int_val >= 0) {
        goto uPerPixelNoWrapNegative;
    }
    // 	add eax,workspaceA.uUpperBound
    eax.v += workspaceA.uUpperBound;
    // 	jmp uBelowRetest
    goto uBelowRetest;
uPerPixelNoWrapNegative:
uAboveRetest:
    // 	cmp eax,workspaceA.uUpperBound
    // 	jl uPerPixelNoWrapPositive
    if(eax.int_val < workspaceA.uUpperBound) {
        goto uPerPixelNoWrapPositive;
    }
    // 	sub eax,workspaceA.uUpperBound
    eax.int_val -= workspaceA.uUpperBound;
    // 	jmp uAboveRetest
    goto uAboveRetest;
uPerPixelNoWrapPositive:
    // endif
    // 	mov workspace.c_u,eax
    workspace.c_u = eax.v;
    // 	sar eax,16
    eax.int_val >>= 16;
    // 	mov edx,workspace.c_z
    edx.v = workspace.c_z;
    // 	add_d edx,workspace.d_z_x,direction
    ADD_D(edx.v, workspace.d_z_x, direction);
    // 	mov workspace.c_z,edx
    workspace.c_z = edx.v;
    // 	shr edx,16
    edx.v >>= 16;

    // 	inc_d ecx,direction
    INC_D(ecx.v, direction);

    // 	jle_d drawPixel,direction
    JLE_D(ecx.int_val, drawPixel, direction);

lineDrawn:
    // ;perform per line updates
    // 	PER_SCAN_ZT half,wrap_flag,minorX
    PER_SCAN_ZTI(halfCount, wrap_flag, minorX, d_minorX);

returnAddress:
    // 	mov workspace.&half&Count,edx
    *halfCount = edx.int_val;
    // 	jge drawLine
    if(edx.int_val >= 0) {
        goto drawLine;
    }
}

void TriangleRender_ZT_I8_D16(brp_block *block, ...)
{
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

    TriangleSetup_ZT_ARBITRARY(v0, v1, v2);

    //     ; Floating point address calculation - 20 cycles, (Integer=26)
    // ;										st(0)		st(1)		st(2)		st(3)		st(4)		st(5) st(6) st(7)
    // 	fild work.colour.base			;	cb
    FILD(WORK_COLOUR_BASE);
    // 	fild workspace.t_y				;	ty			cb
    FILD(workspace.t_y);
    // 	fild work.depth.base			;	db			ty			cb
    FILD(WORK_DEPTH_BASE);
    // 	fild work.colour.stride_b		;	cs			db			ty			cb
    FILD(work.colour.stride_b);
    // 	fild work.depth.stride_b		;	ds			cs			db			ty			cb
    FILD(work.depth.stride_b);
    // 	FXCH st(4)						;	cb			cs			db			ty			ds
    FXCH(4);
    // 	fsub fp_one						;	cb-1		cs			db			ty			ds
    FSUB(fp_one);
    // 	 FXCH st(3)						;	ty			cs			db			cb-1		ds
    FXCH(3);
    // 	fsub fp_one						;	ty-1		cs			db			cb-1		ds
    FSUB(fp_one);
    // 	 FXCH st(2)						;	db			cs			ty-1		cb-1		ds
    FXCH(2);
    // 	fsub fp_two						;	db-2		cs			ty-1		cb-1		ds
    FSUB(fp_two);
    // 	 FXCH st(3)						;	cb-1		cs			ty-1		db-2		ds
    FXCH(3);
    // 	fadd fp_conv_d					;	cb-1I		cs			ty-1		db-2		ds
    FADD(fp_conv_d);
    // 	 FXCH st(1)						;	cs			cb-1I		ty-1		db-2		ds
    FXCH(1);
    // 	fmul st,st(2)					;	csy			cb-1I		ty-1		db-2		ds
    FMUL_ST(0, 2);
    // 	 FXCH st(3)						;	db-2		cb-1I		ty-1		csy			ds
    FXCH(3);
    // 	fadd fp_conv_d					;	db-2I		cb-1I		ty-1		csy			ds
    FADD(fp_conv_d);
    // 	 FXCH st(2)						;	ty-1		cb-1I		db-2I		csy			ds
    FXCH(2);
    // 	fmulp st(4),st					;	cb-1I		db-2I		csy			dsy
    FMULP_ST(4, 0);
    // 	faddp st(2),st					;	db-2I		ca			dsy
    FADDP_ST(2, 0);
    // 	;stall
    // 	faddp st(2),st					;	ca			da
    FADDP_ST(2, 0);
    // 	fstp qword ptr workspace.scanAddress
    FSTP64(&workspace.scanAddress_double);
    // 	fstp qword ptr workspace.depthAddress
    FSTP64(&workspace.depthAddress_double);

    // 	mov eax,work.texture.base
    eax.v = WORK_TEXTURE_BASE;
    // 	mov ebx,workspaceA.sv
    ebx.v = workspaceA.sv;

    // 	add ebx,eax
    ebx.v += eax.v;
    // 	mov eax,workspace.xm
    eax.v = workspace.xm;

    // 	shl eax,16
    eax.v <<= 16;
    // 	mov workspaceA.sv,ebx
    workspaceA.sv = ebx.v;

    // 	mov	edx,workspaceA.flags
    edx.v = workspaceA.flags;
    // 	mov ebx,workspace.d_xm
    ebx.v = workspace.d_xm;

    // 	shl ebx,16
    ebx.v <<= 16;
    // 	mov workspace.xm_f,eax
    workspace.xm_f = eax.v;

    // 	mov workspace.d_xm_f,ebx
    workspace.d_xm_f = ebx.v;
    // 	jmp ecx
    switch(edx.v) {
        case Draw_ZT_I8_NWLR:
            DRAW_ZT_I8(&workspace.x1, &workspace.d_x1, DRAW_LR, &workspace.topCount, WRAPPED, eFog_no, eBlend_no);
            DRAW_ZT_I8(&workspace.x2, &workspace.d_x2, DRAW_LR, &workspace.bottomCount, WRAPPED, eFog_no, eBlend_no);
            break;
        case Draw_ZT_I8_NWRL:
            DRAW_ZT_I8(&workspace.x1, &workspace.d_x1, DRAW_RL, &workspace.topCount, WRAPPED, eFog_no, eBlend_no);
            DRAW_ZT_I8(&workspace.x2, &workspace.d_x2, DRAW_RL, &workspace.bottomCount, WRAPPED, eFog_no, eBlend_no);
            break;
        case Draw_ZT_I8_DWLR:
            DRAW_ZT_I8(&workspace.x1, &workspace.d_x1, DRAW_LR, &workspace.topCount, WRAPPED, eFog_no, eBlend_no);
            DRAW_ZT_I8(&workspace.x2, &workspace.d_x2, DRAW_LR, &workspace.bottomCount, WRAPPED, eFog_no, eBlend_no);
            break;
        case Draw_ZT_I8_DWRL:
            DRAW_ZT_I8(&workspace.x1, &workspace.d_x1, DRAW_RL, &workspace.topCount, WRAPPED, eFog_no, eBlend_no);
            DRAW_ZT_I8(&workspace.x2, &workspace.d_x2, DRAW_RL, &workspace.bottomCount, WRAPPED, eFog_no, eBlend_no);
            break;
        default:
            BrFailure("Invalid enum value");
    }
}


void BR_ASM_CALL TriangleRender_ZTI_I8_D16(brp_block *block, ...) {
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

    TriangleSetup_ZTI_ARBITRARY(v0, v1, v2);

    //     ; Floating point address calculation - 20 cycles, (Integer=26)
    // ;										st(0)		st(1)		st(2)		st(3)		st(4)		st(5) st(6) st(7)
    // 	fild work.colour.base			;	cb
    FILD(WORK_COLOUR_BASE);
    // 	fild workspace.t_y				;	ty			cb
    FILD(workspace.t_y);
    // 	fild work.depth.base			;	db			ty			cb
    FILD(WORK_DEPTH_BASE);
    // 	fild work.colour.stride_b		;	cs			db			ty			cb
    FILD(work.colour.stride_b);
    // 	fild work.depth.stride_b		;	ds			cs			db			ty			cb
    FILD(work.depth.stride_b);
    // 	FXCH st(4)						;	cb			cs			db			ty			ds
    FXCH(4);
    // 	fsub fp_one						;	cb-1		cs			db			ty			ds
    FSUB(fp_one);
    // 	 FXCH st(3)						;	ty			cs			db			cb-1		ds
    FXCH(3);
    // 	fsub fp_one						;	ty-1		cs			db			cb-1		ds
    FSUB(fp_one);
    // 	 FXCH st(2)						;	db			cs			ty-1		cb-1		ds
    FXCH(2);
    // 	fsub fp_two						;	db-2		cs			ty-1		cb-1		ds
    FSUB(fp_two);
    // 	 FXCH st(3)						;	cb-1		cs			ty-1		db-2		ds
    FXCH(3);
    // 	fadd fp_conv_d					;	cb-1I		cs			ty-1		db-2		ds
    FADD(fp_conv_d);
    // 	 FXCH st(1)						;	cs			cb-1I		ty-1		db-2		ds
    FXCH(1);
    // 	fmul st,st(2)					;	csy			cb-1I		ty-1		db-2		ds
    FMUL_ST(0, 2);
    // 	 FXCH st(3)						;	db-2		cb-1I		ty-1		csy			ds
    FXCH(3);
    // 	fadd fp_conv_d					;	db-2I		cb-1I		ty-1		csy			ds
    FADD(fp_conv_d);
    // 	 FXCH st(2)						;	ty-1		cb-1I		db-2I		csy			ds
    FXCH(2);
    // 	fmulp st(4),st					;	cb-1I		db-2I		csy			dsy
    FMULP_ST(4, 0);
    // 	faddp st(2),st					;	db-2I		ca			dsy
    FADDP_ST(2, 0);
    // 	;stall
    // 	faddp st(2),st					;	ca			da
    FADDP_ST(2, 0);
    // 	fstp qword ptr workspace.scanAddress
    FSTP64(&workspace.scanAddress_double);
    // 	fstp qword ptr workspace.depthAddress
    FSTP64(&workspace.depthAddress_double);

    // 	mov eax,work.texture.base
    eax.v = WORK_TEXTURE_BASE;
    // 	mov ebx,workspaceA.sv
    ebx.v = workspaceA.sv;

    // 	add ebx,eax
    ebx.v += eax.v;
    // 	mov eax,workspace.xm
    eax.v = workspace.xm;

    // 	shl eax,16
    eax.v <<= 16;
    // 	mov workspaceA.sv,ebx
    workspaceA.sv = ebx.v;

    // 	mov	edx,workspaceA.flags
    edx.v = workspaceA.flags;
    // 	mov ebx,workspace.d_xm
    ebx.v = workspace.d_xm;

    // 	shl ebx,16
    ebx.v <<= 16;
    // 	mov workspace.xm_f,eax
    workspace.xm_f = eax.v;

    // 	mov workspace.d_xm_f,ebx
    workspace.d_xm_f = ebx.v;
    // 	jmp ecx
    switch(edx.v) {
        case Draw_ZT_I8_NWLR:
            DRAW_ZTI_I8(&workspace.x1, &workspace.d_x1, DRAW_LR, &workspace.topCount, WRAPPED, eFog_no, eBlend_no);
            DRAW_ZTI_I8(&workspace.x2, &workspace.d_x2, DRAW_LR, &workspace.bottomCount, WRAPPED, eFog_no, eBlend_no);
            break;
        case Draw_ZT_I8_NWRL:
            DRAW_ZTI_I8(&workspace.x1, &workspace.d_x1, DRAW_RL, &workspace.topCount, WRAPPED, eFog_no, eBlend_no);
            DRAW_ZTI_I8(&workspace.x2, &workspace.d_x2, DRAW_RL, &workspace.bottomCount, WRAPPED, eFog_no, eBlend_no);
            break;
        case Draw_ZT_I8_DWLR:
            DRAW_ZTI_I8(&workspace.x1, &workspace.d_x1, DRAW_LR, &workspace.topCount, WRAPPED, eFog_no, eBlend_no);
            DRAW_ZTI_I8(&workspace.x2, &workspace.d_x2, DRAW_LR, &workspace.bottomCount, WRAPPED, eFog_no, eBlend_no);
            break;
        case Draw_ZT_I8_DWRL:
            DRAW_ZTI_I8(&workspace.x1, &workspace.d_x1, DRAW_RL, &workspace.topCount, WRAPPED, eFog_no, eBlend_no);
            DRAW_ZTI_I8(&workspace.x2, &workspace.d_x2, DRAW_RL, &workspace.bottomCount, WRAPPED, eFog_no, eBlend_no);
            break;
        default:
            BrFailure("Invalid enum value");
    }
}

void BR_ASM_CALL TriangleRender_ZTI_I8_D16_FLAT(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZTIF_I8_D16(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZTIF_I8_D16_FLAT(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZTIB_I8_D16(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZTIB_I8_D16_FLAT(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZTIFB_I8_D16(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZTIFB_I8_D16_FLAT(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}

void BR_ASM_CALL TriangleRender_ZTF_I8_D16(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
void BR_ASM_CALL TriangleRender_ZTB_I8_D16(brp_block *block, ...) {
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

    TriangleSetup_ZT_ARBITRARY(v0, v1, v2);

    //     ; Floating point address calculation - 20 cycles, (Integer=26)
    // ;										st(0)		st(1)		st(2)		st(3)		st(4)		st(5) st(6) st(7)
    // 	fild work.colour.base			;	cb
    FILD(WORK_COLOUR_BASE);
    // 	fild workspace.t_y				;	ty			cb
    FILD(workspace.t_y);
    // 	fild work.depth.base			;	db			ty			cb
    FILD(WORK_DEPTH_BASE);
    // 	fild work.colour.stride_b		;	cs			db			ty			cb
    FILD(work.colour.stride_b);
    // 	fild work.depth.stride_b		;	ds			cs			db			ty			cb
    FILD(work.depth.stride_b);
    // 	FXCH st(4)						;	cb			cs			db			ty			ds
    FXCH(4);
    // 	fsub fp_one						;	cb-1		cs			db			ty			ds
    FSUB(fp_one);
    // 	 FXCH st(3)						;	ty			cs			db			cb-1		ds
    FXCH(3);
    // 	fsub fp_one						;	ty-1		cs			db			cb-1		ds
    FSUB(fp_one);
    // 	 FXCH st(2)						;	db			cs			ty-1		cb-1		ds
    FXCH(2);
    // 	fsub fp_two						;	db-2		cs			ty-1		cb-1		ds
    FSUB(fp_two);
    // 	 FXCH st(3)						;	cb-1		cs			ty-1		db-2		ds
    FXCH(3);
    // 	fadd fp_conv_d					;	cb-1I		cs			ty-1		db-2		ds
    FADD(fp_conv_d);
    // 	 FXCH st(1)						;	cs			cb-1I		ty-1		db-2		ds
    FXCH(1);
    // 	fmul st,st(2)					;	csy			cb-1I		ty-1		db-2		ds
    FMUL_ST(0, 2);
    // 	 FXCH st(3)						;	db-2		cb-1I		ty-1		csy			ds
    FXCH(3);
    // 	fadd fp_conv_d					;	db-2I		cb-1I		ty-1		csy			ds
    FADD(fp_conv_d);
    // 	 FXCH st(2)						;	ty-1		cb-1I		db-2I		csy			ds
    FXCH(2);
    // 	fmulp st(4),st					;	cb-1I		db-2I		csy			dsy
    FMULP_ST(4, 0);
    // 	faddp st(2),st					;	db-2I		ca			dsy
    FADDP_ST(2, 0);
    // 	;stall
    // 	faddp st(2),st					;	ca			da
    FADDP_ST(2, 0);
    // 	fstp qword ptr workspace.scanAddress
    FSTP64(&workspace.scanAddress_double);
    // 	fstp qword ptr workspace.depthAddress
    FSTP64(&workspace.depthAddress_double);

    // 	mov eax,work.texture.base
    eax.v = WORK_TEXTURE_BASE;
    // 	mov ebx,workspaceA.sv
    ebx.v = workspaceA.sv;

    // 	add ebx,eax
    ebx.v += eax.v;
    // 	mov eax,workspace.xm
    eax.v = workspace.xm;

    // 	shl eax,16
    eax.v <<= 16;
    // 	mov workspaceA.sv,ebx
    workspaceA.sv = ebx.v;

    // 	mov	edx,workspaceA.flags
    edx.v = workspaceA.flags;
    // 	mov ebx,workspace.d_xm
    ebx.v = workspace.d_xm;

    // 	shl ebx,16
    ebx.v <<= 16;
    // 	mov workspace.xm_f,eax
    workspace.xm_f = eax.v;

    // 	mov workspace.d_xm_f,ebx
    workspace.d_xm_f = ebx.v;
    // 	jmp ecx
    switch(edx.v) {
        case Draw_ZT_I8_NWLR:
            DRAW_ZT_I8(&workspace.x1, &workspace.d_x1, DRAW_LR, &workspace.topCount, WRAPPED, eFog_no, eBlend_yes);
            DRAW_ZT_I8(&workspace.x2, &workspace.d_x2, DRAW_LR, &workspace.bottomCount, WRAPPED, eFog_no, eBlend_yes);
            break;
        case Draw_ZT_I8_NWRL:
            DRAW_ZT_I8(&workspace.x1, &workspace.d_x1, DRAW_RL, &workspace.topCount, WRAPPED, eFog_no, eBlend_yes);
            DRAW_ZT_I8(&workspace.x2, &workspace.d_x2, DRAW_RL, &workspace.bottomCount, WRAPPED, eFog_no, eBlend_yes);
            break;
        case Draw_ZT_I8_DWLR:
            DRAW_ZT_I8(&workspace.x1, &workspace.d_x1, DRAW_LR, &workspace.topCount, WRAPPED, eFog_no, eBlend_yes);
            DRAW_ZT_I8(&workspace.x2, &workspace.d_x2, DRAW_LR, &workspace.bottomCount, WRAPPED, eFog_no, eBlend_yes);
            break;
        case Draw_ZT_I8_DWRL:
            DRAW_ZT_I8(&workspace.x1, &workspace.d_x1, DRAW_RL, &workspace.topCount, WRAPPED, eFog_no, eBlend_yes);
            DRAW_ZT_I8(&workspace.x2, &workspace.d_x2, DRAW_RL, &workspace.bottomCount, WRAPPED, eFog_no, eBlend_yes);
            break;
        default:
            BrFailure("Invalid enum value");
    }
}
void BR_ASM_CALL TriangleRender_ZTFB_I8_D16(brp_block *block, ...) {
    // Not implemented
    BrAbort();
}
