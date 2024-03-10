#ifndef X86_EMU
#define X86_EMU

#include <stdint.h>

enum X86_OP {
    X86_OP_REG,
    X86_OP_MEM32,
    X86_OP_PTR,
    X86_OP_IMM
};

enum {
    X87_OP_FLOAT,
    X87_OP_DOUBLE,
    X87_OP_ST,
    X87_OP_MEM32,
    X87_OP_MEM64
};

typedef struct x86_reg {
    union {
        uint32_t      uint_val;
        int32_t       int_val;
        float         float_val;
        unsigned char bytes[8];
        void         *ptr_val;
    };

} x86_reg;

typedef union x86_mem {
    // uint32_t      uint_val;
    // float         float_val;
    void *ptr_val;
    // unsigned char bytes[8];

} x86_mem;

typedef struct x86_operand {
    union {
        x86_reg *reg;
        x86_mem  mem;
        void    *ptr;
        uint32_t imm;
    };
    char type;
} x86_operand;

typedef struct x87_operand {
    struct { // union?
        float         float_val;
        double        double_val;
        int           st_index;
        void         *mem;
        unsigned char bytes[8];
    };
    char type;
} x87_operand;

x87_operand x87_op_f(float f);
x87_operand x87_op_i(int i);
x87_operand x87_op_mem32(void *ptr);
x87_operand x87_op_mem64(void *ptr);
x86_operand x86_op_reg(x86_reg *r);
x86_operand x86_op_mem32(void *bytes);
x86_operand x86_op_ptr(void *ptr);
x86_operand x86_op_imm(uint32_t imm);

extern x86_reg *eax, *ebx, *ecx, *edx, *esi, *ebp, *edi;
void            x86emu_init();

int  x86emu_fpu_stack_top();
void fld(x87_operand op);
void fild(int val);
void fsub(float val);
void fadd(x87_operand op);
void fadd_st(int dest, int src);
void faddp(x87_operand op);
void fsub_2(x87_operand dest, x87_operand src);
void fsubp_2(x87_operand dest, x87_operand src);
void fmul(float);
void fmul_2(x87_operand dest, x87_operand src);
void fmulp_2(x87_operand dest, x87_operand src);
void fdivr(float f);
void fdivrp(int dest, int src);
void fxch(int i);
void fst(x87_operand dest);
void fstp(x87_operand dest);
void fldi(double);

void mov(x86_operand dest, x86_operand src);
void xor_(x86_operand dest, x86_operand src);
void cmp(x86_operand dest, x86_operand src);
void rcl(x86_operand dest, int count);
void sub(x86_operand dest, x86_operand src);
void sbb(x86_operand dest, x86_operand src);
void and (x86_operand dest, x86_operand src);
void add(x86_operand dest, x86_operand src);
void or (x86_operand dest, x86_operand src);
void shr(x86_operand dest, int count);
void sar(x86_operand dest, int count);
void shl(x86_operand dest, int count);

// hack
void fild_ptr(intptr_t val);

#endif
