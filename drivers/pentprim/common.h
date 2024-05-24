#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define DRAW_LR           0
#define DRAW_RL           1
#define NON_WRAPPED       0
#define WRAPPED           1

#define DIR_F 0
#define DIR_B 1

// To deal with 32 / 64 pointer manipulation issues,
// we only add the base texture pointer when accessing pixels
#define WORK_COLOUR_BASE 0
#define WORK_DEPTH_BASE 0
#define WORK_TEXTURE_BASE 0
#define WORK_SHADE_BASE 0

static uint32_t low_bit_mask[] = {
	0,
	0,
	0,
	0,
	0,
	0,
	0x3f, // (2^6 = 64), 00111111
	0,
	0xff  // (2^8 = 256), 11111111
 };

static void MAKE_N_LOW_BIT_MASK(uint32_t *name, int n) {
	*name = low_bit_mask[n];
}

// Helpers for conditional logic based on directional

#define ADC_DIRN(dirn, op1, op2) \
    if (dirn == DIR_F) { \
        ADC(op1, op2); \
    } else { \
        SBB(op1, op2); \
    }

#define ADD_DIRN(dirn, op1, op2) \
    if (dirn == DIR_F) { \
        op1 += op2; \
    } else { \
        op1 -= op2; \
    }

#endif
