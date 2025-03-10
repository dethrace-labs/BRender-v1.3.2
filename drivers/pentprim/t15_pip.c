#include "brender.h"

#include "drv.h"
#include "work.h"
#include <stdbool.h>
#include <stdint.h>

static inline void scan_line_pitip(int BLEND) {
    br_fixed_ls u_numerator = work.pu.current;
    br_fixed_ls v_numerator = work.pv.current;
    br_fixed_ls i_numerator = work.pi.current;
    br_fixed_ls denominator = work.pq.current;

    br_fixed_ls du_numerator = work.pu.grad_x;
    br_fixed_ls dv_numerator = work.pv.grad_x;
    br_fixed_ls di_numerator = work.pi.grad_x;
    br_fixed_ls ddenominator = work.pq.grad_x;

    int source = work.tsl.source;

    const char* const end = work.tsl.end;
    char* dest = work.tsl.start;

    bool RIGHT_TO_LEFT = (dest > end);
    bool LEFT_TO_RIGHT = !RIGHT_TO_LEFT;

    while ((RIGHT_TO_LEFT && dest >= end) || (LEFT_TO_RIGHT && dest <= end)) {
        uint8_t* based = work.texture.base;
        based += source;
        uint8_t offset_low = *based;

        // Check if transparent
        if (offset_low != 0) {
            br_uint_8* shade = work.shade_table;
            uint16_t offset = (((i_numerator & 0xFF) + 2) << 8) | offset_low;
            const uint16_t* texel_ptr = (uint16_t*)shade + 2 * offset; // Address of lit texel
            uint16_t texel = *(uint16_t*)texel_ptr;                    // Get lit texel

            uint16_t pixel = *(uint16_t*)dest; // Get screen pixel
            switch (BLEND) {
            case 15:
                texel /= 2;             // Halve src & dest colours
                pixel = pixel & 0x7BDE; // 0111101111011110
                pixel /= 2;             //
                texel = texel & 0x3DEF; // 0011110111101111
                texel += pixel;         // Add together to blend them
                break;
            case 16:
                texel /= 2;             // Halve src & dest colours
                pixel = pixel & 0xF7DE; // 1111011111011110
                pixel /= 2;             //
                texel = texel & 0x7BEF; // 0111101111101111
                texel += pixel;         // Add together to blend them
                break;
            }
            *(uint16_t*)dest = texel; // Store texel
        }

        if (RIGHT_TO_LEFT)
            i_numerator -= di_numerator; // i += di // ???
        else
            i_numerator += di_numerator; // i += di
        if (RIGHT_TO_LEFT) {
            dest += 2 * sizeof(dest);
            if (dest <= end)
                return;
        } else {
            dest -= 2 * sizeof(dest);
            if (dest >= end)
                return;
        }

        // Core perspective calculations
        if (RIGHT_TO_LEFT) {
            u_numerator += du_numerator;
            denominator -= ddenominator;
        } else {
            u_numerator -= du_numerator;
            denominator += ddenominator;
        }
        if (u_numerator >= denominator) {
            // Jump if u not too large
            do {
                u_numerator -= denominator;
                ++source; // incu // Increment u
                du_numerator += ddenominator;
            } while (u_numerator >= denominator);

        } else if (u_numerator < 0) {
            /* Original code:
             * deculoop:
             *   decu
             *   sub ebp,work.pq.grad_x
             *   add esi,edx
             *   jnc deculoop
             *
             * This is weird for a couple of reasons:
             * 1) The comment says subtract but the code does an add
             * 2) u_numerator is used as signed just in the section above
             *    and now as unsigned jump
             */
            unsigned CARRY_DETECTOR = (unsigned)u_numerator;
            do {
                u_numerator = (int)CARRY_DETECTOR;
                --source; // decu
                du_numerator -= ddenominator;

                // typo in comment...
                CARRY_DETECTOR = (unsigned)u_numerator + ddenominator;
            }
            // ... or bug in code?
            while (CARRY_DETECTOR < (unsigned)u_numerator);
            u_numerator = (int)CARRY_DETECTOR;

            // test esi,esi
            // jge L37
        }

        if (v_numerator < dv_numerator) {
            if (RIGHT_TO_LEFT)
                v_numerator += dv_numerator;
            else
                v_numerator -= dv_numerator;

            /* Original code:
             * decvloop:
             *   decv
             *   sub edi,ecx
             *   add ebx,edx
             *   jnc decvloop
             *
             * See above
             */
            unsigned CARRY_DETECTOR = (unsigned)v_numerator;
            do {
                v_numerator = (int)CARRY_DETECTOR;
                source -= 0x100; // decv
                dv_numerator -= ddenominator;
                CARRY_DETECTOR = (unsigned)v_numerator + ddenominator;
            } while (CARRY_DETECTOR < (unsigned)v_numerator);
            v_numerator = (int)CARRY_DETECTOR;

            continue;
        }

        if (RIGHT_TO_LEFT)
            v_numerator += dv_numerator;
        else
            v_numerator -= dv_numerator;

        if (v_numerator < 0)
            continue; // Jump if v not too small

        do {
            v_numerator -= denominator;
            source += 0x100; // incv
            dv_numerator += ddenominator;
        } while (v_numerator >= denominator);
    }
    return;
}

#if PARTS & PART_15
void ScanLinePITIP256_RGB_555() { scan_line_pitip(0); }
void ScanLinePITIPB256_RGB_555() { scan_line_pitip(15); }
#endif

#if PARTS & PART_16
void ScanLinePITIPB256_RGB_565() { scan_line_pitip(16); }
#endif

