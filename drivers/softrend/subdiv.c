#include "brender.h"
#include "ddi/priminfo.h"
#include "x86emu.h"
#include "drv.h"


static float fp_half         = 0.5f;
static float fp_one         = 1.0f;
static float dotProduct;

#define BRP_VERTEX(reg) ((brp_vertex *)reg.ptr_val)
#define BR_RENDERER(reg) ((struct br_renderer *)reg.ptr_val)

int rightLeftTable[] = {
     OUTCODE_RIGHT | OUTCODE_N_RIGHT,
	 OUTCODE_RIGHT | OUTCODE_N_RIGHT,
	 0,
	 0,
	 OUTCODE_RIGHT | OUTCODE_N_RIGHT,
	 OUTCODE_RIGHT | OUTCODE_N_RIGHT,
	 OUTCODE_RIGHT | OUTCODE_N_RIGHT | OUTCODE_LEFT | OUTCODE_N_LEFT,
	 0,
	 0,
	 OUTCODE_LEFT | OUTCODE_N_LEFT,
	 0,
	 0,
	 OUTCODE_RIGHT | OUTCODE_N_RIGHT | OUTCODE_LEFT | OUTCODE_N_LEFT,
	 OUTCODE_LEFT | OUTCODE_N_LEFT,
	 OUTCODE_RIGHT | OUTCODE_N_RIGHT | OUTCODE_LEFT | OUTCODE_N_LEFT,
	 0
};

int topBottomTable[] = {
     OUTCODE_TOP | OUTCODE_N_TOP,
	 OUTCODE_TOP | OUTCODE_N_TOP,
	 0,
	 0,
	 OUTCODE_TOP | OUTCODE_N_TOP,
	 OUTCODE_TOP | OUTCODE_N_TOP,
	 OUTCODE_TOP | OUTCODE_N_TOP | OUTCODE_BOTTOM | OUTCODE_N_BOTTOM,
	 0,
	 0,
	 OUTCODE_BOTTOM | OUTCODE_N_BOTTOM,
	 0,
	 0,
	 OUTCODE_TOP | OUTCODE_N_TOP | OUTCODE_BOTTOM | OUTCODE_N_BOTTOM,
	 OUTCODE_BOTTOM | OUTCODE_N_BOTTOM,
	 OUTCODE_TOP | OUTCODE_N_TOP | OUTCODE_BOTTOM | OUTCODE_N_BOTTOM,
	 0
};

int hitherYonTable[] = {
     OUTCODE_HITHER | OUTCODE_N_HITHER,
	 OUTCODE_HITHER | OUTCODE_N_HITHER,
	 0,
	 0,
	 OUTCODE_HITHER | OUTCODE_N_HITHER,
	 OUTCODE_HITHER | OUTCODE_N_HITHER,
	 OUTCODE_HITHER | OUTCODE_N_HITHER | OUTCODE_YON | OUTCODE_N_YON,
	 0,
	 0,
	 OUTCODE_YON | OUTCODE_N_YON,
	 0,
	 0,
	 OUTCODE_HITHER | OUTCODE_N_HITHER | OUTCODE_YON | OUTCODE_N_YON,
	 OUTCODE_YON | OUTCODE_N_YON,
	 OUTCODE_HITHER | OUTCODE_N_HITHER | OUTCODE_YON | OUTCODE_N_YON,
	 0
};

// OUTCODE_ORDINATE macro lValue,rValue,tableName1,reg0,reg1
void OUTCODE_ORDINATE(float lValue, float rValue, int*tableName1, x86_reg *reg0, x86_reg* reg1) {
    // ;assumes edx contains outcode flags,esi target for discriptor edi,reg0,reg1
    // ;14 cycles - (at least 2.5 wasted cycles)

    // mov reg0,rValue
    reg0->float_val = rValue;
    // mov esi,080000000h
    esi.uint_val = 0x80000000;
    // and esi,reg0
    esi.uint_val &= reg0->uint_val;
    // mov reg1,lValue
    reg1->float_val = lValue;
    // mov edi,080000000h
    edi.uint_val = 0x80000000;
    // and reg0,07fffffffh
    reg0->uint_val &= 0x7fffffff;
    // shr esi,1
    esi.uint_val >>= 1;
    // and edi,reg1
    edi.uint_val &= reg1->uint_val;
    // or esi,edi
    esi.uint_val |= edi.uint_val;
    // mov edi,reg0
    edi.uint_val = reg0->uint_val;
    // and reg1,07fffffffh
    reg1->uint_val &= 0x7fffffff;
    // ;stall
    // sub edi,reg1
    edi.uint_val -= reg1->uint_val;
    // sub reg1,reg0
    reg1->uint_val -= reg0->uint_val;
    // shr edi,3
    edi.uint_val >>= 3;
    // and reg1,080000000h
    reg1->uint_val &= 0x80000000;
    // shr reg1,2
    reg1->uint_val >>= 2;
    // or esi,edi
    esi.uint_val |= edi.uint_val;
    // or esi,reg1
    esi.uint_val |= reg1->uint_val;
    // lea reg0,tableName1
    reg0->ptr_val = tableName1;
    // shr esi,28
    esi.uint_val >>= 28;
    // ;stall
    // ;stall
    // ;stall
    // mov reg0,[reg0+4*esi]
    reg0->uint_val = ((uint32_t*)reg0->ptr_val)[esi.uint_val];
    // ;stall
    // xor edx,reg0
    edx.uint_val ^= reg0->uint_val;
}
// ;assumes edx contains outcode flags,esi target for discriptor edi,reg0,reg1
// ;14 cycles - (at least 2.5 wasted cycles)

void COMPUTE_COMPONENT_MID_POINT_VALUES(int C_P) {
	// assume eax:ptr dword, ebx:ptr dword, ecx:ptr dword, edx:ptr dword, edi:ptr dword, esi:ptr dword
	// ; compiler = 27 cycles per component
	// ;15 cycles per component

	// fld	[edx+4*C_P]					;	p0
    FLD(BRP_VERTEX(edx)->comp_f[C_P]);
    // fadd [edi+4*C_P]				;	m2*2
    FADD(BRP_VERTEX(edi)->comp_f[C_P]);
    // fld	[edi+4*C_P]					;	p1			m2*2
    FLD(BRP_VERTEX(edi)->comp_f[C_P]);
    // fadd [esi+4*C_P]				;	m0*2		m2*2
    FADD(BRP_VERTEX(esi)->comp_f[C_P]);
    // fxch st(1)						;	m2*2		m0*2
    FXCH(1);
    // fmul fp_half					;	m2			m0*2
    FMUL(fp_half);
    // fld [edx+4*C_P]					;	p0			m2			m0*2
    FLD(BRP_VERTEX(edx)->comp_f[C_P]);
    // fxch st(2)						;	m0*2		m2			p0
    FXCH(2);
    // fmul fp_half					;	m0			m2			p0
    FMUL(fp_half);
    // fxch st(2)						;	p0			m2			m0
    FXCH(2);
    // fadd [esi+4*C_P]				;	m1*2		m2			m0
    FADD(BRP_VERTEX(esi)->comp_f[C_P]);
    // fxch st(1)						;	m2			m1*2		m0
    FXCH(1);
    // fstp [ecx+4*C_P]				;	m1*2		m0
    FSTP(&BRP_VERTEX(ecx)->comp_f[C_P]);
    // fmul fp_half					;	m1			m0
    FMUL(fp_half);
    // fxch st(1)						;	m0			m1
    FXCH(1);
    // fstp [eax+4*C_P]				;	m1
    FSTP(&BRP_VERTEX(eax)->comp_f[C_P]);
    // fstp [ebx+4*C_P]				;
    FSTP(&BRP_VERTEX(ebx)->comp_f[C_P]);
}

void PROJECT_COMPONENT(int C_P, int C_SP) {
	// assume ebp:ptr br_renderer
	// ;21 cycles

    br_renderer *renderer = BR_RENDERER(ebp);

	// fld [ebp]._state.cache.comp_scales[4*C_SP]		;	s			1/w1		1/w0		1/w2
    FLD(renderer->state.cache.comp_scales[C_SP]);
    // fmul [eax+4*C_P]								;   sc0			1/w1		1/w0		1/w2
    FMUL(BRP_VERTEX(eax)->comp_f[C_P]);
    // fld [ebp]._state.cache.comp_scales[4*C_SP]		;	s			sc0			1/w1		1/w0		1/w2
    FLD(renderer->state.cache.comp_scales[C_SP]);
    // fmul [ebx+4*C_P]								;	sc1			sc0			1/w1		1/w0		1/w2
    FMUL(BRP_VERTEX(ebx)->comp_f[C_P]);
    // fld [ebp]._state.cache.comp_scales[4*C_SP]		;   s			sc1			sc0			1/w1		1/w0		1/w2
    FLD(renderer->state.cache.comp_scales[C_SP]);
    // fxch st(2)										;   sc0			sc1			s			1/w1		1/w0		1/w2
    FXCH(2);
    // fmul st,st(4)									;	sc0/w0		sc1			s			1/w1		1/w0		1/w2
    FMUL_ST(0, 4);
    // fxch st(1)										;	sc1			sc0/w0		s			1/w1		1/w0		1/w2
    FXCH(1);

	//;stall

    // fmul st,st(3)									;	sc1/w1		sc0/w0		s			1/w1		1/w0		1/w2
    FMUL_ST(0, 3);
    // fxch st(1)										;	sc0/w0		sc1/w1		s			1/w1		1/w0		1/w2
    FXCH(1);
    // fadd [ebp]._state.cache.comp_offsets[4*C_SP]	;	p0			sc1/w1		s			1/w1		1/w0		1/w2
    FADD(renderer->state.cache.comp_offsets[C_SP]);
    // fxch st(2)										;	s			sc1/w1		p0			1/w1		1/w0		1/w2
    FXCH(2);
    // fmul [ecx+4*C_P]								;	sc2			sc1/w1		p0			1/w1		1/w0		1/w2
    FMUL(BRP_VERTEX(ecx)->comp_f[C_P]);
    // fxch st(1)										;	sc1/w1		sc2			p0			1/w1		1/w0		1/w2
    FXCH(1);
    // fadd [ebp]._state.cache.comp_offsets[4*C_SP]	;	p1			sc2			p0			1/w1		1/w0		1/w2
    FADD(renderer->state.cache.comp_offsets[C_SP]);
    // fxch st(2)										;	p0			sc2			p1			1/w1		1/w0		1/w2
    FXCH(2);
    // fstp [eax+4*C_SP]								;	sc2			p1			1/w1		1/w0		1/w2
    FSTP(&BRP_VERTEX(eax)->comp_f[C_P]);
    // fmul st,st(4)									;	sc2/w2		p1			1/w1		1/w0		1/w2
    FMUL_ST(0, 4);
    // fxch st(1)										;	p1			sc2/w2		1/w1		1/w0		1/w2
    FXCH(1);
    // fstp [ebx+4*C_SP]								;	sc2/w2		1/w1		1/w0		1/w2
    FSTP(&BRP_VERTEX(ebx)->comp_f[C_P]);
    // fadd [ebp]._state.cache.comp_offsets[4*C_SP]	;	p2			1/w1		1/w0		1/w2
    FADD(renderer->state.cache.comp_offsets[C_SP]);
    // ;stall
    // ;stall
	// fstp [ecx+4*C_SP]								;	1/w1		1/w0		1/w2
    FSTP(&BRP_VERTEX(ecx)->comp_f[C_P]);
}

void TEST_AND_PROJECT_VERTEX(x86_reg *reg) {

    br_renderer *renderer = BR_RENDERER(ebp);

    // mov edx,[reg]
    edx.uint_val = BRP_VERTEX((*reg))->flags;
    // test edx,OUTCODES_ALL
    // jnz done
    if (edx.uint_val != OUTCODES_ALL) {
        goto done;
    }
    // fld fp_one										;	1
    FLD(fp_one);
    // fdiv [reg+4*C_W]								;	1/w
    FDIV(BRP_VERTEX((*reg))->comp_f[C_W]);
    // ; wait a long time
    // fst	[reg+4*C_Q]									;	1/w
    FST(&BRP_VERTEX((*reg))->comp_f[C_Q]);
    // fld [reg+4*C_X]									;	x			1/w
    FLD(BRP_VERTEX((*reg))->comp_f[C_X]);
    // fmul st,st(1)									;	x/w			1/w
    FMUL_ST(0, 1);
    // fld [reg+4*C_Y]									;	y			x/w			1/w
    FLD(BRP_VERTEX((*reg))->comp_f[C_Y]);
    // fmul st,st(2)									;	y/w			x/w			1/w
    FMUL_ST(0, 2);
    // fxch st(2)										;	1/w			x/w			y/w
    FXCH(2);
    // ;stall
    // fmul [reg+4*C_Z]								;	z/w			x/w			y/w
    FMUL(BRP_VERTEX((*reg))->comp_f[C_Z]);
    // fxch st(1)										;	x/w			z/w			y/w
    FXCH(1);
    // ;stall
    // fmul [ebp]._state.cache.comp_scales[4*C_SX]		;	sx/w		z/w			y/w
    FMUL(renderer->state.cache.comp_scales[C_SX]);
    // fxch st(2)										;	y/w			z/w			sx/w
    FXCH(2);
    // ;stall
    // fmul [ebp]._state.cache.comp_scales[4*C_SY]		;	sy/w		z/w			sx/w
    FMUL(renderer->state.cache.comp_scales[4*C_SY]);
    // fxch st(2)										;	sx/w		z/w			sy/w
    FXCH(2);
    // fadd [ebp]._state.cache.comp_offsets[4*C_SX]	;	SX			z/w			sy/w
    FADD(renderer->state.cache.comp_offsets[4*C_SX]);
    // fxch st(1)										;	z/w			SX			sy/w
    FXCH(1);
    // fmul [ebp]._state.cache.comp_scales[4*C_SZ]		;	sz/w		SX			sy/w
    FMUL(renderer->state.cache.comp_scales[4*C_SZ]);
    // fxch st(2)										;	sy/w		SX			sz/w
    FXCH(2);
    // fadd [ebp]._state.cache.comp_offsets[4*C_SY]	;	SY			SX			sz/w
    FADD(renderer->state.cache.comp_offsets[4*C_SY]);
    // fxch st(1)										;	SX			SY			sz/w
    FXCH(1);
    // fstp [reg+4*C_SX]								;	SY			sz/w
    FSTP(&BRP_VERTEX((*reg))->comp_f[C_X]);
    // fstp [reg+4*C_SY]								;	sz/w
    FSTP(&BRP_VERTEX((*reg))->comp_f[C_Y]);
    // fadd [ebp]._state.cache.comp_offsets[4*C_SZ]	;   SZ
    FADD(renderer->state.cache.comp_offsets[4*C_SZ]);
    // ;stall
    // ;stall
    // fstp [reg+4*C_SZ]
    FSTP(&BRP_VERTEX((*reg))->comp_f[C_Z]);
done:
}

void averageVerticesOnScreen(struct br_renderer *renderer, brp_vertex *dest1, brp_vertex *dest2,
                                         brp_vertex *dest3, brp_vertex *src1, brp_vertex *src2, brp_vertex *src3) {

	// mov edi,v1
    edi.ptr_val = src2;
    // mov esi,v2
    esi.ptr_val = src3;
    // mov ecx,m2
    ecx.ptr_val = dest3;
    // mov edx,v0
    edx.ptr_val = src1;
    // mov eax,m0
    eax.ptr_val = dest1;
    // mov ebx,m1
    ebx.ptr_val = dest2;

    COMPUTE_COMPONENT_MID_POINT_VALUES(C_X);
	COMPUTE_COMPONENT_MID_POINT_VALUES(C_Y);
	COMPUTE_COMPONENT_MID_POINT_VALUES(C_Z);
	COMPUTE_COMPONENT_MID_POINT_VALUES(C_W);

    // ;get the projection divide going
    // ;41 cycles
    // ;	0			1			2			3			4			5			6			7
    // fld [eax+4*C_W]					;	w0
    FLD(BRP_VERTEX(eax)->comp_f[C_W]);
    // fmul [ebx+4*C_W]				;	w1w0
    FMUL(BRP_VERTEX(ebx)->comp_f[C_W]);
    // fld [ebx+4*C_W]					;	w1			w1w0
    FLD(BRP_VERTEX(ebx)->comp_f[C_W]);
    // fmul [ecx+4*C_W]				;   w1w2		w1w0
    FMUL(BRP_VERTEX(ecx)->comp_f[C_W]);
    // fld st(1)						;	w1w0		w1w2		w1w0
    FLD_ST(1);
    // fmul [ecx+4*C_W]				;	w2w1w0		w1w2		w1w0
    FMUL(BRP_VERTEX(ecx)->comp_f[C_W]);
    // fld [eax+4*C_W]					;	w0			w2w1w0		w1w2		w1w0
    FLD(BRP_VERTEX(eax)->comp_f[C_W]);
    // fmul [ecx+4*C_W]				;	w2w0		w2w1w0		w1w2		w1w0
    FMUL(BRP_VERTEX(ecx)->comp_f[C_W]);
    // fxch st(1)						;	w2w1w0		w2w0		w1w2		w1w0
    FXCH(1);
    // fdivr fp_one					;	1/w2w1w0	w2w0		w1w2		w1w0
    FDIVR(fp_one);

    // ;perform a check to see if we can avoid doing any work.
    // ;doesn't need to be fast as its hidden by the divide
	// push ebp
	// mov ebp,rend.block
    // assume ebp:ptr brp_block
	// mov ebp,[ebp].convert_mask_f
	// and ebp,CM_R or CM_G or CM_B
	// pop ebp
	// mov ebp,renderer
	// jz commonComponents
    if ((rend.block->convert_mask_f & (CM_R | CM_G | CM_B)) == 0) {
        goto commonComponents;
    }

    COMPUTE_COMPONENT_MID_POINT_VALUES(C_R);
	COMPUTE_COMPONENT_MID_POINT_VALUES(C_G);
	COMPUTE_COMPONENT_MID_POINT_VALUES(C_B);

commonComponents:

    COMPUTE_COMPONENT_MID_POINT_VALUES(C_I);
	COMPUTE_COMPONENT_MID_POINT_VALUES(C_U);
	COMPUTE_COMPONENT_MID_POINT_VALUES(C_V);

    // ;complete projection
    // ;	0			1			2			3			4			5			6			7
    // ;5 cycles
    // fmul st(2),st									;	1/w2w1w0	w2w0		1/w0		w1w0
    FMUL_ST(2, 0);
    // ;stall
    // fmul st(1),st									;	1/w2w1w0	1/w1		1/w0		w1w0
    FMUL_ST(1, 0);
    // ;stall
    // fmulp st(3),st									;	1/w1		1/w0		1/w2
    FMULP_ST(3, 0);

    PROJECT_COMPONENT(C_X,C_SX);
	PROJECT_COMPONENT(C_Y,C_SY);
	PROJECT_COMPONENT(C_Z,C_SZ);

    // ;6 cycles
	// fstp [ebx+4*C_Q]
    FSTP(&BRP_VERTEX(ebx)->comp_f[C_Q]);
	// fstp [eax+4*C_Q]
    FSTP(&BRP_VERTEX(eax)->comp_f[C_Q]);
	// fstp [ecx+4*C_Q]
    FSTP(&BRP_VERTEX(ecx)->comp_f[C_Q]);

}
void averageVertices(struct br_renderer *renderer, brp_vertex *m0, brp_vertex *m1, brp_vertex *m2,
                                 brp_vertex *v0, brp_vertex *v1, brp_vertex *v2) {

    struct state_clip *clip;
    br_vector4 *brv4;

	// mov edi,v1
    edi.ptr_val = v1;
    // mov esi,v2
    esi.ptr_val = v2;
    // mov ecx,m2
    ecx.ptr_val = m2;
    // mov edx,v0
    edx.ptr_val = v0;
    // mov eax,m0
    eax.ptr_val = m0;
    // mov ebx,m1
    ebx.ptr_val = m1;

    // ;perform averaging.
	COMPUTE_COMPONENT_MID_POINT_VALUES(C_X);
	COMPUTE_COMPONENT_MID_POINT_VALUES(C_Y);
	COMPUTE_COMPONENT_MID_POINT_VALUES(C_Z);
	COMPUTE_COMPONENT_MID_POINT_VALUES(C_W);
	COMPUTE_COMPONENT_MID_POINT_VALUES(C_R);
	COMPUTE_COMPONENT_MID_POINT_VALUES(C_G);
	COMPUTE_COMPONENT_MID_POINT_VALUES(C_B);
	COMPUTE_COMPONENT_MID_POINT_VALUES(C_I);
	COMPUTE_COMPONENT_MID_POINT_VALUES(C_U);
	COMPUTE_COMPONENT_MID_POINT_VALUES(C_V);

    // ;outcoding
    // mov edx,OUTCODES_NOT
    edx.uint_val = OUTCODES_NOT;
	// mov eax,m0
    eax.ptr_val = m0;
	// OUTCODE_ORDINATE [eax+4*C_X],[eax+4*C_W],rightLeftTable,ebx,ecx
    OUTCODE_ORDINATE(m0->comp_f[C_X], m0->comp_f[C_W], &rightLeftTable, &ebx, &ecx);
	// OUTCODE_ORDINATE [eax+4*C_Y],[eax+4*C_W],topBottomTable,ebx,ecx
    OUTCODE_ORDINATE(m0->comp_f[C_Y], m0->comp_f[C_W], &topBottomTable, &ebx, &ecx);
	// OUTCODE_ORDINATE [eax+4*C_Z],[eax+4*C_W],hitherYonTable,ebx,ecx
    OUTCODE_ORDINATE(m0->comp_f[C_Z], m0->comp_f[C_W], &hitherYonTable, &ebx, &ecx);
	// mov [eax],edx
    m0->flags = edx.uint_val;

	// mov edx,OUTCODES_NOT
    edx.uint_val = OUTCODES_NOT;
	// mov eax,m1
	OUTCODE_ORDINATE(m1->comp_f[C_X], m1->comp_f[C_W], &rightLeftTable, &ebx, &ecx);
	// OUTCODE_ORDINATE [eax+4*C_Y],[eax+4*C_W],topBottomTable,ebx,ecx
    OUTCODE_ORDINATE(m1->comp_f[C_Y], m1->comp_f[C_W], &topBottomTable, &ebx, &ecx);
	// OUTCODE_ORDINATE [eax+4*C_Z],[eax+4*C_W],hitherYonTable,ebx,ecx
    OUTCODE_ORDINATE(m1->comp_f[C_Z], m1->comp_f[C_W], &hitherYonTable, &ebx, &ecx);
	// mov [eax],edx
    m1->flags = edx.uint_val;

	// mov edx,OUTCODES_NOT
    edx.uint_val = OUTCODES_NOT;
	// mov eax,m2
	// OUTCODE_ORDINATE [eax+4*C_X],[eax+4*C_W],rightLeftTable,ebx,ecx
    OUTCODE_ORDINATE(m2->comp_f[C_X], m2->comp_f[C_W], &rightLeftTable, &ebx, &ecx);
	// OUTCODE_ORDINATE [eax+4*C_Y],[eax+4*C_W],topBottomTable,ebx,ecx
    OUTCODE_ORDINATE(m2->comp_f[C_Y], m2->comp_f[C_W], &topBottomTable, &ebx, &ecx);
	// OUTCODE_ORDINATE [eax+4*C_Z],[eax+4*C_W],hitherYonTable,ebx,ecx
    OUTCODE_ORDINATE(m2->comp_f[C_Z], m2->comp_f[C_W], &hitherYonTable, &ebx, &ecx);
	// mov [eax],edx
    m2->flags = edx.uint_val;
	// push ebp

	// mov ecx,m2
	// mov eax,m0

	// mov ebx,m1
	// mov ebp,renderer
    ebp.ptr_val = renderer;

    // ; perform clip plane outcoding if neccessary.
    // mov edx,scache.user_clip_active
    edx.uint_val = scache.user_clip_active;
    // mov edi,MAX_STATE_CLIP_PLANES-1
    edi.uint_val = MAX_STATE_CLIP_PLANES-1;
    // test edx,edx
    // jz clipDone
    if (edx.uint_val == 0) {
        goto clipDone;
    }

clipPlane:
    // lea ebp,[ebp]._state.clip+MAX_STATE_CLIP_PLANES*sizeof(state_clip)
    clip = &renderer->state.clip[MAX_STATE_CLIP_PLANES];
    // assume ebp:ptr state_clip
    // cmp esi,BRT_PLANE
    // jne clipNext
    if (esi.uint_val != BRT_PLANE) {
        goto clipNext;
    }
    // lea edx,[ebp].plane
    // lea esi,[eax+4*C_X]
    // DOT_PRODUCT_4 esi,edx,dotProduct
    dotProduct = BrVector4Dot((br_vector4*)&BRP_VERTEX(eax)->comp_f[C_X], &clip->plane);
    // mov edx,dotProduct
    edx.uint_val = dotProduct;
    // mov esi,[eax]
    esi.uint_val = BRP_VERTEX(eax)->flags;
    // test edx,080000000h
    // jz clip1
    if (edx.uint_val == 0x80000000) {
        goto clip1;
    }
    // mov edx,OUTCODE_USER or OUTCODE_N_USER
    edx.uint_val = OUTCODE_USER | OUTCODE_N_USER;
    // push ecx
    // -
    // mov ecx,edi
    // -
    // shl edx,cl
    edx.uint_val <<= edi.bytes[0];  //using edi here
    // pop ecx
    // -
    // xor esi,edx
    esi.uint_val ^= edx.uint_val;
    // mov [eax],esi
    BRP_VERTEX(eax)->flags = esi.uint_val;

clip1:
    // lea esi,[ebx+4*C_X]
    // DOT_PRODUCT_4 esi,edx,dotProduct
    dotProduct = BrVector4Dot((br_vector4*)&BRP_VERTEX(esi)->comp_f[C_X], &clip->plane);
    // mov edx,dotProduct
    edx.uint_val = dotProduct;
    // mov esi,[ebx]
    esi.uint_val = BRP_VERTEX(ebx)->flags;
    // test edx,080000000h
    // jz clip1
    if (edx.uint_val == 0x80000000) {
        goto clip1;
    }
    // mov edx,OUTCODE_USER or OUTCODE_N_USER
    edx.uint_val = OUTCODE_USER | OUTCODE_N_USER;
    // push ecx
    // -
    // mov ecx,edi
    // -
    // shl edx,cl
    edx.uint_val <<= edi.bytes[0];
    // pop ecx
    // -
    // xor esi,edx
    esi.uint_val ^= edx.uint_val;
    // mov [ebx],esi
    BRP_VERTEX(ebx)->flags = esi.uint_val;

clip2:
    // lea esi,[ecx+4*C_X]
    // DOT_PRODUCT_4 esi,edx,dotProduct
    dotProduct = BrVector4Dot((br_vector4*)&BRP_VERTEX(ecx)->comp_f[C_X], &clip->plane);
    // mov edx,dotProduct
    edx.uint_val = dotProduct;
    // mov esi,[ecx]
    esi.uint_val = BRP_VERTEX(ecx)->flags;
    // test edx,080000000h
    // jz clip1
    if (edx.uint_val == 0x80000000) {
        goto clip1;
    }
    // mov edx,OUTCODE_USER or OUTCODE_N_USER
    edx.uint_val = OUTCODE_USER | OUTCODE_N_USER;
    // push ecx
    // -
    // mov ecx,edi
    // -
    // shl edx,cl
    edx.uint_val <<= edi.bytes[0];
    // pop ecx
    // -
    // xor esi,edx
    esi.uint_val ^= edx.uint_val;
    // mov [ebx],esi
    BRP_VERTEX(ecx)->flags = esi.uint_val;

clipNext:
	// sub ebp,sizeof(state_clip)
    clip--;
	// dec edi
    edi.uint_val--;
	// jge clipPlane
    if (edi.int_val >= 0) {
        goto clipPlane;
    }

clipDone:
    // ;project if neccesary
    // mov edx,[eax]
    edx.uint_val = BRP_VERTEX(ecx)->flags;
    // pop ebp
    // -
    // mov edi,[ebx]
    edi.uint_val = BRP_VERTEX(ebx)->flags;
    // mov esi,[ecx]
    esi.uint_val = BRP_VERTEX(ecx)->flags;
    // mov ebp,renderer
    // -
    // test edx,OUTCODES_ALL
    // jnz selectiveProjection
    if (edx.uint_val != OUTCODES_ALL) {
        goto selectiveProjection;
    }
    // test edi,OUTCODES_ALL
    // jnz selectiveProjection
    if (edi.uint_val != OUTCODES_ALL) {
        goto selectiveProjection;
    }
    // test esi,OUTCODES_ALL
    // jnz selectiveProjection
    if (edi.uint_val != OUTCODES_ALL) {
        goto selectiveProjection;
    }

    // ;41 cycles
    // ;	0			1			2			3			4			5			6			7
    // fld [eax+4*C_W]					;	w0
    FLD(BRP_VERTEX(eax)->comp_f[C_W]);
    // fmul [ebx+4*C_W]				;	w1w0
    FMUL(BRP_VERTEX(ebx)->comp_f[C_W]);
    // fld [ebx+4*C_W]					;	w1			w1w0
    FLD(BRP_VERTEX(ebx)->comp_f[C_W]);
    // fmul [ecx+4*C_W]				;   w1w2		w1w0
    FMUL(BRP_VERTEX(ecx)->comp_f[C_W]);
    // fld st(1)						;	w1w0		w1w2		w1w0
    FLD_ST(1);
    // fmul [ecx+4*C_W]				;	w2w1w0		w1w2		w1w0
    FMUL(BRP_VERTEX(ecx)->comp_f[C_W]);
    // fld [eax+4*C_W]					;	w0			w2w1w0		w1w2		w1w0
    FLD(BRP_VERTEX(eax)->comp_f[C_W]);
    // fmul [ecx+4*C_W]				;	w2w0		w2w1w0		w1w2		w1w0
    FMUL(BRP_VERTEX(ecx)->comp_f[C_W]);
    // fxch st(1)						;	w2w1w0		w2w0		w1w2		w1w0
    FXCH(1);
    // fdivr fp_one					;	1/w2w1w0	w2w0		w1w2		w1w0
    FDIVR(fp_one);
    // ;complete projection
    // ;	0			1			2			3			4			5			6			7
    // ;5 cycles
    // fmul st(2),st					;	1/w2w1w0	w2w0		1/w0		w1w0
    FMUL_ST(2, 0);
    // ;stall
    // fmul st(1),st					;	1/w2w1w0	1/w1		1/w0		w1w0
    FMUL_ST(1, 0);
    // ;stall
    // fmulp st(3),st					;	1/w1		1/w0		1/w2
    FMULP_ST(3, 0);

    PROJECT_COMPONENT (C_X,C_SX);
	PROJECT_COMPONENT (C_Y,C_SY);
	PROJECT_COMPONENT (C_Z,C_SZ);

    // ;6 cycles
	// fstp [ebx+4*C_Q]
    FSTP(&BRP_VERTEX(ebx)->comp_f[C_Q]);
	// fstp [eax+4*C_Q]
    FSTP(&BRP_VERTEX(eax)->comp_f[C_Q]);
	// fstp [ecx+4*C_Q]
    FSTP(&BRP_VERTEX(ecx)->comp_f[C_Q]);
	// ret
    return;

selectiveProjection:
	// TEST_AND_PROJECT_VERTEX eax
	// TEST_AND_PROJECT_VERTEX ebx
	// TEST_AND_PROJECT_VERTEX ecx
    TEST_AND_PROJECT_VERTEX(&eax);
    TEST_AND_PROJECT_VERTEX(&ebx);
    TEST_AND_PROJECT_VERTEX(&ecx);
	// ret
}
