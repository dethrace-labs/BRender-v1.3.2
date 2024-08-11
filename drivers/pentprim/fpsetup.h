#ifndef FPSETUP_H
#define FPSETUP_H

#include "priminfo.h"

extern uint64_t  fconv_d16_12[2];
extern uint64_t  fconv_d16_m[2];
extern float fp_one        ;
extern float fp_two        ;
extern float fp_four        ;

extern uint32_t fp_conv_d;
extern uint32_t fp_conv_d8;
extern uint32_t fp_conv_d8r;
extern uint32_t fp_conv_d16 ;
extern uint32_t fp_conv_d24 ;
extern uint32_t fp_conv_d32 ;

extern uint16_t fp_single_cw ;
extern uint16_t fp_double_cw ;
extern uint16_t fp_extended_cw ;

extern int      sort_table_1[];
extern int      sort_table_0[];
extern int      sort_table_2[];
extern uint32_t flip_table[8];

enum {
    FPSETUP_SUCCESS,
    FPSETUP_EMPTY_TRIANGLE
};

enum {
    SETUP_SUCCESS,
    SETUP_ERROR
};



void TriangleSetup_Z(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2);
void TriangleSetup_ZI(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2);
void TriangleSetup_ZT(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2);
void TriangleSetup_ZTI(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2);

void TriangleSetup_ZT_ARBITRARY(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2);
void TriangleSetup_ZTI_ARBITRARY(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2);

// was a macro. Used by pfpsetup
typedef union fp64_t fp64_t;
void SETUP_FLOAT_PARAM(int comp, char *param /*unused*/, fp64_t *s_p, fp64_t *d_p_x, uint32_t conv, int is_unsigned);

#endif
