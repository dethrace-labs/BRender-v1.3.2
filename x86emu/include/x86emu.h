#ifndef X86_EMU
#define X86_EMU

#include <stdint.h>
#include <math.h>

// enum X86_OP {
//     X86_OP_REG,
//     X86_OP_MEM32,
//     X86_OP_PTR,
//     X86_OP_IMM
// };

// enum {
//     X87_OP_FLOAT,
//     X87_OP_DOUBLE,
//     X87_OP_ST,
//     X87_OP_MEM32,
//     X87_OP_MEM64
// };

typedef struct x86_reg {
    union {
        uint32_t      v;
        int32_t       int_val;
        float         float_val;
        //unsigned char bytes[8];
        struct {
#if BR_ENDIAN_BIG
            uint16_t pad1;
            // "high" byte
            uint8_t h;
            // "low" byte
            uint8_t l;
#else
            // "low" byte
            uint8_t l;
            // "high" byte
            uint8_t h;
#endif
        };
        //uint16_t    short_val[2];
        struct {
#if BR_ENDIAN_BIG
            uint16_t short_high;
            uint16_t short_low;
#else
            uint16_t short_low;
            uint16_t short_high;
#endif
        };
        // these are a bit of a hack to allow 64bit pointers on occasion
        void         *ptr_v;
        uint8_t     *ptr_8;
        uint16_t     *ptr_16;
    };

} x86_reg;

// typedef union x86_mem {
    // uint32_t      uint_val;
    // float         float_val;
    // void *ptr_val;
    // unsigned char bytes[8];

// } x86_mem;

// typedef struct x86_operand {
//     union {
//         x86_reg *reg;
//         x86_mem  mem;
//         void    *ptr;
//         uint32_t imm;
//     };
//     char type;
// } x86_operand;

// typedef struct x87_operand {
//     struct { // union?
//         float         float_val;
//         double        double_val;
//         int           st_index;
//         void         *mem;
//         unsigned char bytes[8];
//     };
//     char type;
// } x87_operand;

// x87_operand x87_op_f(float f);
// x87_operand x87_op_i(int i);
// x87_operand x87_op_mem32(void *ptr);
// x87_operand x87_op_mem64(void *ptr);
// x86_operand x86_op_reg(x86_reg *r);
// x86_operand x86_op_mem32(void *bytes);
// x86_operand x86_op_ptr(void *ptr);
// x86_operand x86_op_imm(uint32_t imm);

// extern x86_reg *eax, *ebx, *ecx, *edx, *esi, *ebp, *edi;
// void            x86emu_init();

// int  x86emu_fpu_stack_top();
// void fld(x87_operand op);
// void fild(int val);
// void fsub(float val);
// void fadd(x87_operand op);
// void fadd_st(int dest, int src);
// void faddp(x87_operand op);
// void fsub_2(x87_operand dest, x87_operand src);
// void fsubp_2(x87_operand dest, x87_operand src);
// void fmul(float);
// void fmul_2(x87_operand dest, x87_operand src);
// void fmulp_2(x87_operand dest, x87_operand src);
// void fdivr(float f);
// void fdivrp(int dest, int src);
// void fxch(int i);
// void fst(x87_operand dest);
// void fstp(x87_operand dest);
// void fldi(double);

// void mov(x86_operand dest, x86_operand src);
// void xor_(x86_operand dest, x86_operand src);
// void cmp(x86_operand dest, x86_operand src);
// void rcl(x86_operand dest, int count);
// void sub(x86_operand dest, x86_operand src);
// void sbb(x86_operand dest, x86_operand src);
// void and (x86_operand dest, x86_operand src);
// void add(x86_operand dest, x86_operand src);
// void adc(x86_operand dest, x86_operand src);
// void or (x86_operand dest, x86_operand src);
// void shr(x86_operand dest, int count);
// void sar(x86_operand dest, int count);
// void shl(x86_operand dest, int count);
// void ror(x86_operand dest, int count);

// hack
// void fild_ptr(intptr_t val);

typedef struct x86emu_state_t {
    long double x87_stack[8];
    long double x87_swap;
    int  x87_stack_top;
    int cf;
    int zf;
    int sf;
    int of;
    uint32_t x86_swap;
} x86emu_state_t;

extern x86emu_state_t x86_state;
extern x86_reg eax, ebx, ecx, edx, ebp, edi, esi;

// 0-based
#define BYTE2(x) ((unsigned int)x >> (2 << 3)) & 0xff

#define ST_(i) x86_state.x87_stack[x86_state.x87_stack_top - i]

#define X87_POP() \
    ST_(0) = 0; \
    x86_state.x87_stack_top--;

#define FXCH(i) \
    x86_state.x87_swap = ST_(0);\
    ST_(0) = ST_(i);            \
    ST_(i) = x86_state.x87_swap;

#define FLD_ST(i) \
    do { \
    double v = ST_(i); \
    x86_state.x87_stack[++x86_state.x87_stack_top] = v; \
    } while (0);

#define FLD(val_ptr) \
    x86_state.x87_stack[++x86_state.x87_stack_top] = *(float *)&val_ptr;

#define FILD(val) \
    x86_state.x87_stack[++x86_state.x87_stack_top] = (int)val;

#define FSUB(val) \
    ST_(0) -= *(float *)&val;

#define FSUBR(val) \
    ST_(0) = val - ST_(0);

#define FSUBRP_ST(dest, src) \
    ST_(dest) = ST_(src) - ST_(dest); \
    X87_POP();

#define FMUL(val) \
    ST_(0) *= *(float *)&val;

#define FMUL_ST(dest, src) \
    ST_(dest) *= ST_(src);

// no-operand pops the stack
// https://c9x.me/x86/html/file_module_x86_id_104.html
#define FMUL_0() \
    ST_(1) *= ST_(0); \
    X87_POP();

#define FMULP_ST(dest, src) \
    ST_(dest) *= ST_(src); \
    X87_POP();

#define FSUB_ST(dest, src) \
    ST_(dest) -= ST_(src); \

#define FSUBP_ST(dest, src) \
    ST_(dest) -= ST_(src); \
    X87_POP();

#define FADD_0() \
    ST_(1) += ST_(0); \
    X87_POP();

#define FADD(val) \
    ST_(0) += *(float *)&val;

#define FADD64(val) \
    ST_(0) += *(double *)&val;

#define FST(dest) \
    *(float*)dest = (float)ST_(0);

#define FSTP(dest) \
    *(float*)dest = (float)ST_(0); \
    X87_POP();

#define FISTP(dest) \
    *(int32_t*)dest = round(ST_(0)); \
    X87_POP();

#define FABS() \
    ST_(0) = fabs(ST_(0));

#define FSTP_ST(dest) \
    ST_(dest) = ST_(0); \
    X87_POP();

#define FSTP64(dest) \
    *(double*)dest = (double)ST_(0); \
    X87_POP();

#define FDIV(val) \
    ST_(0) = ST_(0) / val;

#define FDIVR(val) \
    ST_(0) = val / ST_(0);

#define FDIVRP_ST(dest, src) \
    ST_(dest) = ST_(src) / ST_(dest); \
    X87_POP();

#define FADD_ST(dest, src) \
    ST_(dest) += ST_(src); \

#define FADDP_ST(dest, src) \
    ST_(dest) += ST_(src); \
    X87_POP();

#define FCHS() \
    ST_(0) *= -1;

#define CMP(val1, val2) \
    x86_state.cf = val1 < val2; \
    x86_state.zf = val1 == val2;

#define RCL_1(val) \
    do { \
    int msb = val & 0x80000000; \
    /* rotate CF flag into lsb */ \
    val = (val << 1) + x86_state.cf; \
    /* rotate msb into CF */ \
    x86_state.cf = msb; \
    } while(0)

#define ADD_AND_SET_CF(val1, val2) \
    val1 += val2; \
	x86_state.cf = val1 < val2;

#define SUB_AND_SET_CF(val1, val2) \
    x86_state.cf = val1 < val2; \
    val1 -= val2;

#define ADC(val1, val2) \
    val1 += val2 + x86_state.cf;

#define SBB(val1, val2) \
    val1 = val1 - (val2 + x86_state.cf);

#define ROR16(dest) \
    x86_state.x86_swap = dest.short_low; \
	dest.short_low = dest.short_high; \
	dest.short_high = x86_state.x86_swap;


#endif
