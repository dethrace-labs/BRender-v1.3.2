#ifndef FPWORK_H
#define FPWORK_H

#include <stdint.h>
#include "../softrend/ddi/priminfo.h"

// ; Layout of workspace for floating point Pentium optimized setup for rasterisers
// ;

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
    uint32_t pad10;  // jeffh: added to keep alignment

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
} __attribute__((aligned(8)));

extern struct workspace_t               workspace;
extern struct ArbitraryWidthWorkspace_t workspaceA;

#endif
