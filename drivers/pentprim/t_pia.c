#include "brender.h"

#include "drv.h"
#include "priminfo.h"
#include "work.h"
#include <complex.h>
#include <stdbool.h>
#include <stdint.h>

// I have no idea why this is a thing
// but the optimizer should throw this out anyway
static const bool WRAP = 1;
static const bool WRAPROW = 1;
static const bool TRANSP = 1;

static inline void scan_inc(const bool carry, const bool light, const short sbpp) {
    // -- Update current  -- //
    br_fixed_ls current = *(br_fixed_ls*)work.awsl.start;
    br_fixed_ls d = work.main.d_i;

    if (light) {
        current = work.pi.current;
        d = carry ? work.pi.d_carry : work.pi.d_nocarry;
    }
    work.pi.current = current + d;

    // -- Update u_current -- //
    unsigned u_current = work.awsl.u_current;
    unsigned du = carry ? work.awsl.du_carry : work.awsl.du_nocarry;

    unsigned new_u_current = u_current + du;
    work.awsl.u_current = new_u_current;

    int CF = 0;
    // Unsigned overflow
    if (new_u_current < u_current) {
        CF = -1;
    }

    if (WRAP) {
        // -- Update u_int_current -- //
        short u_int_current = work.awsl.u_int_current; // dword ptr
        u_int_current += carry ? work.awsl.du_int_carry : work.awsl.du_nocarry;
        u_int_current -= CF;
        work.awsl.u_int_current = u_int_current;
    }

    // -- Update v_current -- //
    work.awsl.v_current += carry ? work.awsl.dv_carry : work.awsl.dv_nocarry;

    // -- Update source_current -- //
    char* source_current = work.awsl.source_current;
    source_current += carry ? work.awsl.dsource_carry : work.awsl.dsource_nocarry;

    CF *= sbpp;
    source_current -= CF;

    if (CF) {
        source_current += work.texture.stride_b;
    }

    work.awsl.source_current = source_current;
}

static inline char* scan_forward(const bool light, const bool bump, const uint8_t sbpp, const uint8_t dbpp) {
    int noffset = work.bump.base - work.texture.base;

    char* start = work.awsl.start;
    const char* end = work.awsl.end;

    unsigned u_current = work.awsl.u_current;
    unsigned v_current = work.awsl.v_current;

    short _t = work.awsl.u_int_current;
    short swapped_u_int_current = (_t >> 8) | (_t << 8);
    u_current = swapped_u_int_current;

    char* source_current = work.awsl.source_current;

    while (start < end) {
        bool transparent = false;
        if (TRANSP) {
            for (uint8_t i = 0; i < sbpp; ++i) {
                transparent = transparent && (source_current[i] == 0);
            }
        }

        if (light) {
            if (!transparent) {
                br_uint_8 lighting_index = ((br_uint_8*)&work.pi.currentpix)[2];
                br_uint_8 texel = *source_current;

                uint32_t offset = 0;
                offset = (lighting_index << 8) | texel;
                br_uint_8* shade_table_ptr = work.shade_table;

                shade_table_ptr += offset * dbpp; // Light texel
                *start = *shade_table_ptr;        // Store texel
            }
            start += dbpp;
            work.pi.currentpix += work.pi.grad_x;

        } else if (bump) {
            if (!transparent) {
                br_uint_8 texel = *source_current;
                br_uint_8 normal = *(source_current + noffset);
                br_uint_8 light_level = *(normal + work.lighting_table);

                uint32_t offset = (light_level << 8) | texel;

                const br_uint_8* light_texel = work.shade_table + offset;
                *start = *light_texel;
            }
            start += dbpp;

        } else {
            if (!transparent) {
                for (br_uint_8 i = 0; i < sbpp; ++i) {
                    start[i] = source_current[i];
                }
            }
            start += dbpp;
        }

        // NOTE FROM MASM: Redo this with high and low halfwords of du swapped

        unsigned new_u_current = u_current + work.awsl.du;

        int CF = 0;
        if (new_u_current < u_current) {
            // Overflow
            CF = -1;
        }
        u_current = new_u_current;
        source_current += work.awsl.dsource;

        u_current -= CF;

        CF *= sbpp;

        source_current -= CF;

        u_current += work.awsl.du_int;

        unsigned new_v_current = v_current + work.awsl.dv;
        if (new_v_current < v_current) {
            source_current += work.texture.stride_b;
        }
        v_current = new_v_current;

        // test ah,ah
        if ((u_current & 0xFF00)) {
            br_uint_32 width_p = work.texture.width_p;
            u_current += width_p;
            source_current += width_p * sbpp;
        }

        if ((void*)source_current < work.texture.base) {
            // Have we underrun?
            source_current += work.texture.size;
        }
    }

    return source_current;
}

static inline char* scan_backward(const bool light, const bool bump, const uint8_t sbpp, const uint8_t dbpp) {
    int noffset = work.bump.base - work.texture.base;

    char* start = work.awsl.start;
    const char* end = work.awsl.end;

    unsigned u_current = work.awsl.u_current;
    unsigned v_current = work.awsl.v_current;

    short _t = work.awsl.u_int_current;
    short swapped_u_int_current = (_t >> 8) | (_t << 8);
    u_current = swapped_u_int_current;

    char* source_current = work.awsl.source_current;

    while (start > end) {
        if (light) {
            work.pi.currentpix -= work.pi.grad_x;
        }

        // TODO: In MASM: Redo this with high and low halfwords of du swapped
        unsigned new_u_current = u_current + work.awsl.du;
        int CF = 0;
        if (new_u_current < u_current) {
            CF = -1;
        }

        source_current += work.awsl.dsource;
        u_current -= CF;
        u_current += work.awsl.du_int;

        source_current -= CF * sbpp;

        unsigned new_v_current = v_current + work.awsl.dv;
        if (new_v_current < v_current) {
            source_current += work.texture.stride_b;
        }
        v_current = new_v_current;

        // test ah,ah
        if ((u_current & 0xFF00)) {
            br_uint_32 width_p = work.texture.width_p;
            u_current += width_p;
            source_current += width_p * sbpp;
        }

        if ((void*)source_current < work.texture.base)
            source_current += work.texture.size; // _size

        start -= dbpp;

        if (TRANSP) {
            bool transparent = false;
            for (uint8_t i = 0; i < sbpp; ++i) {
                transparent = transparent && (source_current[i] == 0);
            }
            if (transparent)
                continue; // Skip if transparent
        }

        switch (sbpp) {
        case 1:;
            ;
            br_uint_8 texel = *source_current; // Get texel
            br_uint_8 light_index = ((br_uint_8*)&work.pi.currentpix)[2];
            switch (dbpp) {
            case 1:
                if (light) {
                    texel = *(br_uint_8*)((light_index << 8) + texel + work.shade_table); // Light texel
                }

                if (bump) {
                    br_uint_8 light_level = *(work.lighting_table + *(br_uint_8*)(source_current + noffset)); // calculate light level from normal
                    texel = *(br_uint_8*)((light_level << 8) + texel + work.shade_table);                     // Light texel
                }

                *start = texel;
                break;
            case 2:
                if (light)
                    *(br_uint_16*)start = *(br_uint_16*)(work.shade_table + ((light_index << 8) + texel) * 2);
                break;
            case 3:
                if (light) {
                    *(br_uint_16*)start = *(br_uint_16*)(work.shade_table + ((light_index << 8) + texel) * 4);
                    *(br_uint_8*)(start + 2) = ((br_uint_8*)(work.shade_table + ((light_index << 8) + texel) * 4))[2];
                }
                break;
            }
            break;
        case 2:
            *(br_uint_16*)start = *(br_uint_16*)source_current;
            break;
        case 3:
            *(br_uint_16*)start = *(br_uint_16*)source_current;
            ((br_uint_8*)start)[3] = ((br_uint_8*)source_current)[3];
            break;
        case 4:
            *(br_uint_32*)start = *(br_uint_32*)source_current;
            break;
        }
    }

    return source_current;
}

static inline void triangle_render(const bool light, const bool bump, const uint8_t sbpp, const uint8_t dbpp) {
    // int noffset = 0;
    struct scan_edge* edge = work.awsl.edge;

    for (; edge->count; --edge->count) {
        char* start = work.awsl.start;
        char* end = work.awsl.end; // _end?

        if (light) {
            work.pi.currentpix = work.pi.current;
        }

        // unused variable: // char *source_current;
        if (start < end)
            /* source_current = */ scan_forward(light, bump, sbpp, dbpp);
        else
            /* source_current = */ scan_backward(light, bump, sbpp, dbpp);

        // Per scan line updates
        br_int_32 new_f = edge->f + edge->d_f;
        br_uint_32 d_i = edge->d_i;
        end = work.awsl.end;

        int CF = 0;
        if (new_f < edge->f) {
            CF = 1;
        }
        edge->f = new_f;

        end += (d_i + CF) * dbpp; // work.awsl.end += dbpp*(edge->d_i+carry)

        work.awsl.end = end;

        new_f = work.main.f + work.main.d_f;
        CF = 0;
        if (new_f < work.main.f)
            CF = 1;
        work.main.f = new_f;

        start = work.awsl.start;
        d_i = work.main.d_i;

        if (CF) {
            d_i += CF;
        }

        // This is not used for some reason
        // unused code line: // source_current += 2*d_i;
        start += d_i * dbpp;
        work.awsl.start = start;
        scan_inc(CF, light, sbpp);

        br_uint_16 u_int_current = work.awsl.u_int_current;
        // test bh, bh
        // Test sign of low(?) halfword
        if ((u_int_current & 0xFF00)) {
            br_uint_32 width_p = work.texture.width_p;
            work.awsl.u_int_current += width_p;

            work.awsl.source_current += width_p * sbpp;
        }

        if ((void*)work.awsl.source_current < work.texture.base)
            work.awsl.source_current += work.texture.size;
    }
}

#if PARTS & PART_BUMP
void BR_ASM_CALL TrapezoidRenderPITAN() { triangle_render(false, true, 1, 1); } // Normal map
#endif

#if PARTS & PART_8
void BR_ASM_CALL TrapezoidRenderPITA() { triangle_render(false, false, 1, 1); }
void BR_ASM_CALL TrapezoidRenderPITIA() { triangle_render(true, false, 1, 1); }
#endif

#if PARTS & PART_15
void BR_ASM_CALL TrapezoidRenderPITA15() { triangle_render(false, false, 2, 2); }
void BR_ASM_CALL TrapezoidRenderPITIA_RGB_555() { triangle_render(true, false, 1, 2); }
#endif

#if PARTS & PART_24
void BR_ASM_CALL TrapezoidRenderPITA24() { triangle_render(false, false, 3, 3); }
void BR_ASM_CALL TrapezoidRenderPITIA_RGB_888() { triangle_render(true, false, 1, 3); }
#endif

