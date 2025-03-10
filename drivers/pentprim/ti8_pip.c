#include "brender.h"

#include "drv.h"
#include "work.h"
#include <complex.h>
#include <stdbool.h>
#include <stdint.h>

static const bool TRANSP = 1;
static const bool DITHER = 1;

static inline unsigned rotr(unsigned x, unsigned n) {
    return (x >> n % 32) | (x << (32 - n) % 32);
}

static inline int32_t dither(int32_t a, int32_t b, int32_t frac) {
    // b int32_t due to sar - not sure if necessary
    // TODO: Swap out with faster frac switch if needed (see asm)
    // This will result in off by one results on negative numbers
    // due to the difference in IDIV and SAR rounding
    return a + (b * frac / 16);
}

static inline void scan_line_pitpd_n(uint32_t base_u, uint32_t base_v, uint32_t mask, const uint8_t* dx) {
    int u_numerator = work.pu.current;
    int v_numerator = work.pv.current;
    int du_numerator = work.pu.grad_x;
    int dv_numerator = work.pv.grad_x;
    int denominator = work.pq.current;
    int d_denominator = work.pq.grad_x;

    int source = work.tsl.source;
    char* dest = work.tsl.start;
    const char* const end = work.tsl.end; // EBP

    // Lowest two bits
    br_uint_8 DBASE = (size_t)dest & 0x3;

    if (dest == end)
        return;

    bool RIGHT_TO_LEFT = dest > end;

    // Dithering?
    if (DITHER) {
        u_numerator = dither(u_numerator, denominator, dx[DBASE]);
        v_numerator = dither(v_numerator, denominator, dx[DBASE]);
    }

    while (dest != end) {
        source *= base_u; // pre

        if (u_numerator >= denominator) {
            do {
                u_numerator -= denominator;    // u_numerator -= denominator;
                source += base_u;              // incu
                du_numerator += d_denominator; // du_numerator += d_denominator
            } while (u_numerator >= denominator);
        } else if (u_numerator < 0) {
            /* Original code:
             * @@:
             *   decu
             *   sub ebp,ecx ; du_numerator -= d_denominator
             *   add esi,edx ; u_numerator -= denominator
             *   jnc @B
             *
             * This is weird for a couple of reasons:
             * 1) The comment says subtract but the code does an add
             * 2) u_numerator is used as signed just in the section above
             *    and now as unsigned jump
             */

            unsigned CARRY_DETECTOR = (unsigned)u_numerator;
            do {
                u_numerator = (int)CARRY_DETECTOR;
                source -= base_u;              // decu
                du_numerator -= d_denominator; // du_numerator -= d_denominator

                // typo in comment...
                CARRY_DETECTOR = (unsigned)(u_numerator + denominator);
            }
            // ... or bug in code?
            while (CARRY_DETECTOR < (unsigned)u_numerator);
            u_numerator = (int)CARRY_DETECTOR;
        }

        if (v_numerator < 0) {
            /* Original code:
             * @@:
             *   decv
             *   sub edi,ecx ; du_numerator -= d_denominator
             *   add ebx,edx ; u_numerator -= denominator
             *   jnc @B
             *
             * Same as above
             */

            unsigned CARRY_DETECTOR = (unsigned)v_numerator;
            do {
                v_numerator = (int)CARRY_DETECTOR;
                source -= base_v;              // decv
                dv_numerator -= d_denominator; // dv_numerator -= d_denominator
                v_numerator += denominator;

                // typo in comment...
                CARRY_DETECTOR = (unsigned)(v_numerator + denominator);
            }
            // ... or bug in code?
            while (CARRY_DETECTOR < (unsigned)v_numerator);
            v_numerator = (int)CARRY_DETECTOR;
        } else if (v_numerator >= denominator) { // compare v_numerator and denominator
            // Skip this if proper fraction

            do {
                v_numerator -= denominator;
                source += base_v; // incv
                dv_numerator += d_denominator;
            } while (v_numerator >= denominator);
        }

        // post1 and post2
        source /= base_u;
        source = (source & mask);

        // U,V and W increments
        if (RIGHT_TO_LEFT) {
            u_numerator += du_numerator;  // u_numerator += du_numerator
            v_numerator += dv_numerator;  // v_numerator += dv_numerator
            denominator -= d_denominator; // denominator -= d_denominator

            // Dithering?
            if (DITHER) {
                // Dither backwards
                // For dithering need esi (u_numerator) += edx*fraction
                //                and ebx (v_numerator) += edx*fraction
                u_numerator = dither(u_numerator, denominator, dx[(DBASE - 1) % 4] - dx[DBASE]);
                v_numerator = dither(v_numerator, denominator, dx[(DBASE - 1) % 4] - dx[DBASE]);
            }
        } else {                          // LEFT_TO_RIGHT
            u_numerator -= du_numerator;  // u_numerator -= du_numerator
            v_numerator -= dv_numerator;  // v_numerator -= dv_numerator
            denominator += d_denominator; // denominator += d_denominator

            // Dithering?
            if (DITHER) {
                // Dither forwards
                // For dithering need esi (u_numerator) += edx*fraction
                //                and ebx (v_numerator) += edx*fraction
                u_numerator = dither(u_numerator, denominator, dx[(DBASE + 1) % 4] - dx[DBASE]);
                v_numerator = dither(v_numerator, denominator, dx[(DBASE + 1) % 4] - dx[DBASE]);
            }
        }

        // End of calcs

        if (RIGHT_TO_LEFT) {
            if (dest <= end)
                return;
        } else if (dest >= end)
            return;

        if (RIGHT_TO_LEFT) {
            v_numerator = rotr(v_numerator, 16);
        }

        // Fetch texel and test for transparency
        const br_uint_8* texel = source + (br_uint_8*)work.texture.base;

        if (!TRANSP || (*texel != 0)) // Transparent?
            *dest = *texel;           // Store texel and z value

        if (RIGHT_TO_LEFT) {
            if (--dest <= end)
                return;
        } else if (++dest >= end)
            return;
    }
    return;
}

static inline void scan_line_pitpd(uint32_t base_u, uint32_t base_v, uint32_t mask) {
    int y = work.tsl.y;
    y = y & 3;
    const uint8_t dx_0[4] = { 13, 1, 4, 16 };
    const uint8_t dx_1[4] = { 8, 12, 9, 5 };
    const uint8_t dx_2[4] = { 10, 6, 7, 11 };
    const uint8_t dx_3[4] = { 3, 15, 14, 2 };
    switch (y) {
    case 0:
        scan_line_pitpd_n(base_u, base_v, mask, dx_0);
        break;
    case 1:
        scan_line_pitpd_n(base_u, base_v, mask, dx_1);
        break;
    case 2:
        scan_line_pitpd_n(base_u, base_v, mask, dx_2);
        break;
    case 3:
        scan_line_pitpd_n(base_u, base_v, mask, dx_3);
        break;
    }
}

#if PARTS & PART_DITHER
void BR_ASM_CALL ScanLinePITPD64() { scan_line_pitpd(4, 0x100, 0x0FFF); }
void BR_ASM_CALL ScanLinePITPD128() { scan_line_pitpd(2, 0x100, 0x3FFF); }
void BR_ASM_CALL ScanLinePITPD256() { scan_line_pitpd(1, 0x100, 0x0); }
void BR_ASM_CALL ScanLinePITPD1024() { scan_line_pitpd(0x40, 0x10000, 0xFFFFF); }
#endif

