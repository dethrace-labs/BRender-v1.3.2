#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define DRAW_LR           0
#define DRAW_RL           1
#define NON_WRAPPED       0
#define WRAPPED           1

#define DIR_F 0
#define DIR_B 1

typedef enum {
    eBlend_no,
    eBlend_yes
} tBlend_enabled;

typedef enum {
    eFog_no,
    eFog_yes,
} tFog_enabled;

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
	0x8,    // 2^3 = 8
	0x10,   // 2^4 = 16
	0x20,   // 2^5 = 32
	0x3f,   // 2^6 = 64
	0x80,   // 2^7 = 128
	0xff    // 2^8 = 256
 };

static void MAKE_N_LOW_BIT_MASK(uint32_t *name, int n) {
	*name = low_bit_mask[n];
}

// Helpers for conditional logic based on directional

#define ADC_D(op1, op2, dirn) \
    if (dirn == DIR_F) { \
        ADC(op1, op2); \
    } else { \
        SBB(op1, op2); \
    }

#define ADD_D(op1, op2, dirn) \
    if (dirn == DIR_F) { \
        op1 += op2; \
    } else { \
        op1 -= op2; \
    }

#define ADD_SET_CF_D(op1, op2, dirn) \
    if (dirn == DIR_F) { \
        ADD_AND_SET_CF(op1, op2); \
    } else { \
        SUB_AND_SET_CF(op1, op2); \
    }

#define INC_D(op1, dirn) \
    if (dirn == DIR_F) { \
        op1++; \
    } else { \
        op1--; \
    }

#define JG_D(val, label, dirn) \
    if ((dirn == DIR_F && val > 0) || (dirn == DIR_B && val < 0)) { \
        goto label; \
    }

#define JL_D(val, label, dirn) \
    if ((dirn == DIR_F && val < 0) || (dirn == DIR_B && val > 0)) { \
        goto label; \
    }

#define JLE_D(val, label, dirn) \
    if ((dirn == DIR_F && val <= 0) || (dirn == DIR_B && val >= 0)) { \
        goto label; \
    }


#endif
