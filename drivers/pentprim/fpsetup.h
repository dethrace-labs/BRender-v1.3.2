#ifndef FPSETUP_H
#define FPSETUP_H

#include <stdint.h>
#include "../softrend/ddi/priminfo.h"
#include "x86emu.h"

#define DRAW_LR           0
#define DRAW_RL           1
#define NON_WRAPPED       0
#define WRAPPED           1
#define WORK_TEXTURE_BASE 0

extern struct workspace_t               workspace;
extern struct ArbitraryWidthWorkspace_t workspaceA;
extern float                            fp_one;
extern float                            fp_two;
extern uint32_t                         fp_conv_d;
;

enum {
    FPSETUP_SUCCESS,
    FPSETUP_EMPTY_TRIANGLE
};

enum {
    SETUP_SUCCESS,
    SETUP_ERROR
};

typedef struct counts_tag_t {
    union {
        uint32_t l;
        uint16_t s[2];
    } data;
} counts_tag_t;

struct workspace_t {
    // qwords start here

    uint32_t xm;
    uint32_t d_xm;

    uint32_t x1;
    uint32_t d_x1;

    uint32_t x2;
    uint32_t d_x2;

    uint32_t s_z;
    uint32_t d_z_y_1;

    uint32_t d_z_x;
    uint32_t d_z_y_0;

    uint32_t s_i;
    uint32_t d_i_y_1;

    uint32_t d_i_x;
    uint32_t d_i_y_0;

    uint32_t s_u;
    uint32_t d_u_y_1;

    uint32_t d_u_x;
    uint32_t d_u_y_0;

    uint32_t s_v;
    uint32_t d_v_y_1;

    uint32_t d_v_x;
    uint32_t d_v_y_0;

    uint32_t s_s;
    uint32_t d_s_y_1;

    uint32_t d_s_x;
    uint32_t d_s_y_0;

    // scanAddress in the original 32 bit code was a pointer into the color buffer
    // now is an offset that is added to `work.colour.base`
    uint32_t scanAddress;
    uint32_t scanAddressTrashed;

    // depthAddress in the original 32 bit code was a pointer into the color buffer
    // now is an offset that is added to `work.depth.base`
    uint32_t depthAddress;
    uint32_t depthAddressTrashed;

    // qwords end here
    union {
        struct {
            brp_vertex *v0;
            brp_vertex *v1;
            brp_vertex *v2;
        };
        brp_vertex *v0_array[3];
    };
    uint32_t     iarea;
    uint32_t     dx1_a;
    uint32_t     dx2_a;
    uint32_t     dy1_a;
    uint32_t     dy2_a;
    uint32_t     xstep_1;
    uint32_t     xstep_0;
    uint32_t     t_dx;
    uint32_t     t_dy;
    uint32_t     t_y;
    uint32_t     flip;
    uint32_t     c_z;
    int32_t     c_u;
    uint32_t     c_v;
    uint32_t     c_i;
    counts_tag_t counts;
    uint32_t     v0_x;
    uint32_t     v1_x;
    uint32_t     v2_x;
    uint32_t     v0_y;
    uint32_t     v1_y;
    uint32_t     v2_y;
    uint32_t     top_vertex;
    uint32_t     xm_f;
    uint32_t     d_xm_f;
    int32_t      topCount;
    int32_t      bottomCount;
    uint32_t     colour;
    uint32_t     colourExtension;
    uint32_t     shadeTable;
    uint32_t     scratch0;
    uint32_t     scratch1;
    uint32_t     scratch2;
    uint32_t     scratch3;
    uint32_t     scratch4;
    uint32_t     scratch5;
    uint32_t     scratch6;
    uint32_t     scratch7;
    uint32_t     scratch8;
    uint32_t     scratch9;
    uint32_t     scratch10;
    uint32_t     scratch11;
    uint32_t     scratch12;
    uint32_t     scratch13;
    uint32_t     scratch14;
    uint32_t     scratch15;
} workspace_t;

#define m_y        workspace.t_dy
#define sort_value workspace.top_vertex

struct ArbitraryWidthWorkspace_t {
    uint32_t su;
    uint32_t pad0;

    uint32_t dux;
    uint32_t pad1;

    uint32_t duy1;
    uint32_t pad2;

    uint32_t duy0;
    uint32_t pad3;

    uint32_t pad10;

    // originally a pointer into work.texture.base
    // now only an offset
    uint32_t sv;
    uint32_t pad4;

    uint32_t dvy1;
    uint32_t pad5;
    uint32_t dvy1c;
    uint32_t pad6;
    uint32_t dvy0;
    uint32_t pad7;
    uint32_t dvy0c;

    uint32_t dvxc;
    uint32_t pad8;
    uint32_t dvx;
    uint32_t pad9;

    uint32_t svf;
    uint32_t dvxf;
    uint32_t dvy1f;
    uint32_t dvy0f;

    int32_t  uUpperBound;
    uint32_t vUpperBound;

    uint32_t flags;
    char    *retAddress;
};

/*
typedef struct workspace_t {
    // qwords start here
    uint32_t xm;
    uint32_t d_xm;
    uint32_t x1;
    uint32_t d_x1;
    uint32_t x2;
    uint32_t d_x2;

    uint32_t s_z;
    uint32_t d_z_y_1;
    uint32_t d_z_x;
    uint32_t d_z_y_0;

    uint32_t s_i;
    uint32_t d_i_y_1;
    uint32_t d_i_x;
    uint32_t d_i_y_0;

    uint32_t s_u;
    uint32_t d_u_y_1;
    uint32_t d_u_x;
    uint32_t d_u_y_0;

    uint32_t s_v;
    uint32_t d_v_y_1;
    uint32_t d_v_x;
    uint32_t d_v_y_0;

    uint32_t s_s;
    uint32_t d_s_y_1;
    uint32_t d_s_x;
    uint32_t d_s_y_0;

    uint32_t scanAddress;
    uint32_t scanAddressTrashed;
    uint32_t depthAddress;
    uint32_t depthAddressTrashed;

    // qwords end here

    // input to setup code
    brp_block *v0;
    brp_block *v1;
    brp_block *v2;

    // these are only used within the setup code
    uint32_t iarea;
    uint32_t dx1_a;
    uint32_t dx2_a;
    uint32_t dy1_a;
    uint32_t dy2_a;
    uint32_t xstep_1;
    uint32_t xstep_0;
    uint32_t t_dx;
    uint32_t t_dy;

    // results from the setup code
    uint32_t t_y;
    uint32_t flip;

    // used in some rasterisers
    uint32_t c_z;
    uint32_t c_u;
    uint32_t c_v;
    uint32_t c_i;

    // tsbHeader stuff aka h
    counts_tag_t counts;

    // Fixed setup only
    uint32_t v0_x;

    // converted vertex coordinates - maps to three overlapping
    uint32_t v1_x;

    // 'converted_vertex' structures
    uint32_t v2_x;
    uint32_t v0_y;
    uint32_t v1_y;
    uint32_t v2_y;
    uint32_t top_vertex;

    //;my stuff, currently in here for my convenience -JohnG
    uint32_t xm_f;
    uint32_t d_xm_f;

    uint32_t topCount;
    uint32_t bottomCount;

    // align 8 colour;
    uint32_t colour;
    uint32_t colourExtension;
    uint32_t shadeTable;
    // align 8 scratch0;
    uint32_t scratch0;
    uint32_t scratch1;
    uint32_t scratch2;
    uint32_t scratch3;
    uint32_t scratch4;
    uint32_t scratch5;
    uint32_t scratch6;
    uint32_t scratch7;
    uint32_t scratch8;
    uint32_t scratch9;
    uint32_t scratch10;
    uint32_t scratch11;
    uint32_t scratch12;
    uint32_t scratch13;
    uint32_t scratch14;
    uint32_t scratch15;
} workspace_t;
*/

void TriangleSetup_ZT_ARBITRARY(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2);
int  SETUP_FLOAT(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2);
void SETUP_FLOAT_PARAM(int comp, char *param /*unused*/, uint32_t *s_p, uint32_t *d_p_x, uint32_t conv, int is_unsigned);
void ARBITRARY_SETUP();
void SETUP_FLAGS();
void REMOVE_INTEGER_PARTS_OF_PARAMETERS();
void REMOVE_INTEGER_PARTS_OF_PARAM(void *param);
void MULTIPLY_UP_PARAM_VALUES(int32_t s_p, int32_t d_p_x, int32_t d_p_y_0, int32_t d_p_y_1, void *a_sp, void *a_dpx,
                              void *a_dpy1, void *a_dpy0, uint32_t dimension, uint32_t magic);
void SPLIT_INTO_INTEGER_AND_FRACTIONAL_PARTS();
void MULTIPLY_UP_V_BY_STRIDE(uint32_t magic);
void CREATE_CARRY_VERSIONS();
void WRAP_SETUP();

void TriangleSetup_ZT(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2);

// TOOD: should be in common.h
void MAKE_N_LOW_BIT_MASK(uint32_t *name, int n);

#endif
