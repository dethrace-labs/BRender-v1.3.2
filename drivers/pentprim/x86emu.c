#include "x86emu.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

long double fpu_stack[8];
int         fpu_st0_ptr = -1;
int         ZF          = 0;
int         CF          = 0;
int         SF          = 0;
static int  inited      = 0;

x86_reg *eax, *ebx, *ecx, *edx, *esi, *ebp, *edi;

// #define ST_(i) fpu_stack[fpu_st0_ptr - i]

long double *st(int i)
{
    assert(fpu_st0_ptr - i >= 0);
    assert(fpu_st0_ptr - i <= 7);
    return &fpu_stack[fpu_st0_ptr - i];
}

int x86emu_fpu_stack_top()
{
    return fpu_st0_ptr;
}

// float MEM32x(float)
// {
//     return 1;
// }

void x86emu_init()
{
    if(inited) {
        return;
    }
    eax    = malloc(sizeof(x86_reg));
    ebx    = malloc(sizeof(x86_reg));
    ecx    = malloc(sizeof(x86_reg));
    edx    = malloc(sizeof(x86_reg));
    esi    = malloc(sizeof(x86_reg));
    ebp    = malloc(sizeof(x86_reg));
    edi    = malloc(sizeof(x86_reg));
    inited = 1;
}
void fail()
{
    printf("x86emu::fail\n");
    exit(1);
}

void fpu_pop()
{
    fpu_stack[fpu_st0_ptr] = 0;
    fpu_st0_ptr--;
    assert(fpu_st0_ptr >= -1);
}

x87_operand x87_op_f(float f)
{
    x87_operand o;
    o.type      = X87_OP_FLOAT;
    o.float_val = f;
    return o;
}

x87_operand x87_op_d(double d)
{
    x87_operand o;
    o.type       = X87_OP_DOUBLE;
    o.double_val = d;
    return o;
}

x87_operand x87_op_i(int i)
{
    x87_operand o;
    o.type     = X87_OP_ST;
    o.st_index = i;
    return o;
}

x87_operand x87_op_mem32(void *ptr)
{
    x87_operand o;
    o.type = X87_OP_MEM32;
    o.mem  = ptr;
    return o;
}

x87_operand x87_op_mem64(void *ptr)
{
    x87_operand o;
    o.type = X87_OP_MEM64;
    o.mem  = ptr;
    return o;
}

x86_operand x86_op_reg(x86_reg *r)
{
    x86_operand o;
    o.type = X86_OP_REG;
    o.reg  = r;
    return o;
}

x86_operand x86_op_imm(uint32_t imm)
{
    x86_operand o;
    o.type = X86_OP_IMM;
    o.imm  = imm;
    return o;
}

x86_operand x86_op_mem32(void *bytes)
{
    x86_operand o;
    o.type        = X86_OP_MEM32;
    o.mem.ptr_val = bytes;
    return o;
}

x86_operand x86_op_ptr(void *ptr)
{
    x86_operand o;
    o.type = X86_OP_PTR;
    o.ptr  = ptr;
    return o;
}

x87_operand make_x87_op(unsigned char *bytes, char type)
{
    x87_operand o;
    o.type = type;
    switch(type) {
        case X87_OP_FLOAT:
            memcpy(o.bytes, bytes, 4);
            break;
        case X87_OP_DOUBLE:
            memcpy(o.bytes, bytes, 8);
            break;
        case X87_OP_ST:
            memcpy(o.bytes, bytes, 8);
            break;
        default:
            fail();
    }
    return o;
}

void fld(x87_operand op)
{
    /*
fld(10)
0: 10
fld(20)
0: 10  <- ST(1)
1: 20  <- ST(0)
*/

    switch(op.type) {
        case X87_OP_ST:
            fpu_stack[fpu_st0_ptr + 1] = *st(op.st_index);
            break;
        case X87_OP_FLOAT:
            fpu_stack[fpu_st0_ptr + 1] = op.float_val;
            break;
        case X87_OP_DOUBLE:
            fpu_stack[fpu_st0_ptr + 1] = op.double_val;
            break;
        default:
            fail();
    }
    fpu_st0_ptr++;
    assert(fpu_st0_ptr < 8);
}

void fild(int val)
{
    fpu_stack[fpu_st0_ptr + 1] = val;
    fpu_st0_ptr++;
    assert(fpu_st0_ptr < 7);
}

void fild_ptr(intptr_t val)
{
    fpu_stack[fpu_st0_ptr + 1] = val;
    fpu_st0_ptr++;
    assert(fpu_st0_ptr < 7);
}

void fsub(float v)
{
    *st(0) -= v;
}

void fsub_2(x87_operand dest, x87_operand src)
{
    *st(dest.st_index) -= *st(src.st_index);
}

void fsubp_2(x87_operand dest, x87_operand src)
{
    fsub_2(dest, src);
    fpu_pop();
}

void fmul(float val)
{
    *st(0) *= val;
}

void fmul_2(x87_operand dest, x87_operand src)
{
    *st(dest.st_index) *= *st(src.st_index);
}

void fmulp_2(x87_operand dest, x87_operand src)
{
    fmul_2(dest, src);
    fpu_pop();
}

void fdivr(float f)
{
    *st(0) = f / *st(0);
}

void fdivrp(int dest, int src)
{
    // src is always 0
    *st(dest) = *st(0) / *st(dest);
    fpu_pop();
}

void fxch(int i)
{
    long double tmp = *st(0);
    *st(0)          = *st(i);
    *st(i)          = tmp;
}

void fst(x87_operand dest)
{
    switch(dest.type) {
        case X87_OP_MEM32:
            *(float *)dest.mem = (float)*st(0);
            break;
        case X87_OP_MEM64:
            *(double *)dest.mem = (double)*st(0);
            break;
        case X87_OP_ST:
            assert(dest.st_index == 0);
            // no op
            break;
        default:
            fail();
    }
}

void fstp(x87_operand dest)
{
    fst(dest);
    fpu_pop();
}

void fadd_st(int dest, int src)
{
    *st(dest) += *st(src);
}

void fadd(x87_operand op)
{
    float  f;
    double d;

    switch(op.type) {
        case X87_OP_MEM32:
            memcpy(&f, op.mem, 4);
            *st(0) += f;
            break;
        case X87_OP_MEM64:
            memcpy(&d, op.mem, 8);
            *st(0) += d;
            break;
        case X87_OP_FLOAT:
            *st(0) += op.float_val;
            break;
        case X87_OP_DOUBLE:
            *st(0) += op.double_val;
            break;
        case X87_OP_ST:
            *st(op.st_index) += *st(0);
            break;
        default:
            fail();
    }
}

void faddp(x87_operand op)
{
    fadd(op);
    fpu_pop();
}

void mov(x86_operand dest, x86_operand src)
{
    void *src_val;
    int   size;
    switch(src.type) {
        case X86_OP_MEM32:
            src_val = src.mem.ptr_val;
            size    = 4;
            break;
        case X86_OP_REG:
            src_val = src.reg->bytes;
            size    = 4;
            break;
        case X86_OP_PTR:
            src_val = src.ptr;
            size    = 8; // TODO
            break;
        case X86_OP_IMM:
            src_val = &src.imm;
            size    = 4;
            break;
        default:
            fail();
    }

    switch(dest.type) {
        case X86_OP_REG:
            memcpy(dest.reg->bytes, src_val, size);
            break;
        case X86_OP_MEM32:
            memcpy(dest.mem.ptr_val, src_val, size);
            break;
    }
}

void xor_(x86_operand dest, x86_operand src)
{
    uint8_t *src_val;
    int      size;
    switch(src.type) {
        case X86_OP_MEM32:
            src_val = src.mem.ptr_val;
            size    = 4;
            break;
        case X86_OP_REG:
            src_val = src.reg->bytes;
            size    = 4;
            break;
        case X86_OP_IMM:
            src_val = &src.imm;
            size    = 4;
            break;
        default:
            fail();
    }

    switch(dest.type) {
        case X86_OP_REG:
            for(int i = 0; i < size; i++) {
                dest.reg->bytes[i] ^= src_val[i];
            }
            break;
        case X86_OP_MEM32:
            fail();
    }
}

void cmp(x86_operand dest, x86_operand src)
{
    uint8_t *src_val;
    int      size;
    switch(src.type) {
        case X86_OP_MEM32:
            src_val = src.mem.ptr_val;
            size    = 4;
            break;
        case X86_OP_REG:
            src_val = src.reg->bytes;
            size    = 4;
            break;
        case X86_OP_IMM:
            src_val = &src.imm;
            size    = 4;
            break;
        default:
            fail();
    }

    switch(dest.type) {
        case X86_OP_REG:
            if(dest.reg->uint_val < *(uint32_t *)src_val) {
                CF = 1;
                ZF = 0;
                SF = 1;
            } else if(dest.reg->uint_val == *(uint32_t *)src_val) {
                CF = 0;
                ZF = 1;
            } else {
                CF = 0;
                ZF = 0;
            }
            break;
        default:
            fail();
    }
}

void rcl(x86_operand dest, int count)
{
    switch(dest.type) {
        case X86_OP_REG:
            while(count != 0) {
                int msb = dest.reg->uint_val & 0x80000000;
                // rotate CF flag into lsb
                dest.reg->uint_val = (dest.reg->uint_val << 1) + CF;
                // rotate msb into CF
                CF = msb;
                count--;
            }
            break;
        default:
            fail();
    }
}

void sub(x86_operand dest, x86_operand src)
{
    void *src_val;
    int   size;
    switch(src.type) {
        case X86_OP_MEM32:
            src_val = src.mem.ptr_val;
            size    = 4;
            break;
        case X86_OP_REG:
            src_val = src.reg->bytes;
            size    = 4;
            break;
        case X86_OP_IMM:
            src_val = &src.imm;
            size    = 4;
            break;
        default:
            fail();
    }
    switch(dest.type) {
        case X86_OP_REG:
            if(*(uint32_t *)src_val > dest.reg->uint_val) {
                CF = 1;
            } else {
                CF = 0;
            }
            dest.reg->uint_val -= *(uint32_t *)src_val;
            break;
        default:
            fail();
    }
}

void sbb(x86_operand dest, x86_operand src)
{
    void *src_val;
    int   size;
    switch(src.type) {
        case X86_OP_REG:
            src_val = src.reg->bytes;
            size    = 4;
            break;
        default:
            fail();
    }
    switch(dest.type) {
        case X86_OP_REG:
            dest.reg->uint_val = (dest.reg->uint_val - (*(uint32_t *)src_val + CF));
            break;
        default:
            fail();
    }
}

void and (x86_operand dest, x86_operand src)
{
    void *src_val;
    int   size;
    switch(src.type) {
        case X86_OP_MEM32:
            src_val = src.mem.ptr_val;
            size    = 4;
            break;
        case X86_OP_REG:
            src_val = src.reg->bytes;
            size    = 4;
            break;
        case X86_OP_IMM:
            src_val = &src.imm;
            break;
        default:
            fail();
    }
    switch(dest.type) {
        case X86_OP_REG:
            dest.reg->uint_val &= *(uint32_t *)src_val;
            break;
        case X86_OP_MEM32:
            *(uint32_t *)dest.mem.ptr_val &= *(uint32_t *)src_val;
            break;
        default:
            fail();
    }
}

void add(x86_operand dest, x86_operand src)
{
    void *src_val;
    switch(src.type) {
        case X86_OP_MEM32:
            src_val = src.mem.ptr_val;
            break;
        case X86_OP_IMM:
            src_val = &src.imm;
            break;
        case X86_OP_REG:
            src_val = &src.reg->uint_val;
            break;
        default:
            fail();
    }
    switch(dest.type) {
        case X86_OP_REG:
            dest.reg->uint_val += *(uint32_t *)src_val;
            if(dest.reg->uint_val < *(uint32_t *)src_val) {
                CF = 1;
            } else {
                CF = 0;
            }
            break;
        default:
            fail();
    }
}

void adc(x86_operand dest, x86_operand src) {
    void *src_val;
    switch(src.type) {
        case X86_OP_MEM32:
            src_val = src.mem.ptr_val;
            break;
        case X86_OP_IMM:
            src_val = &src.imm;
            break;
        case X86_OP_REG:
            src_val = &src.reg->uint_val;
            break;
        default:
            fail();
    }
    switch(dest.type) {
        case X86_OP_REG:
            dest.reg->uint_val += *(uint32_t *)src_val + CF;
            if(dest.reg->uint_val < *(uint32_t *)src_val) {
                CF = 1;
            } else {
                CF = 0;
            }
            break;
        default:
            fail();
    }
}

void or (x86_operand dest, x86_operand src)
{
    void *src_val;
    switch(src.type) {
        case X86_OP_MEM32:
            src_val = src.mem.ptr_val;
            break;
        case X86_OP_REG:
            src_val = src.reg->bytes;
            break;
        case X86_OP_IMM:
            src_val = &src.imm;
            break;
        default:
            fail();
    }
    switch(dest.type) {
        case X86_OP_REG:
            dest.reg->uint_val |= *(uint32_t *)src_val;
            break;
        default:
            fail();
    }
}

void shr(x86_operand dest, int count)
{
    assert(dest.type == X86_OP_REG);

    while(count != 0) {
        CF            = dest.reg->uint_val & 1;
        uint32_t res1 = dest.reg->uint_val / 2; // Unsigned divide
        uint32_t res2 = dest.reg->uint_val >> 1;
        if(res1 != res2) {
            int a = 0;
        }
        dest.reg->uint_val = res1;
        count--;
    }
}

void sar(x86_operand dest, int count)
{
    assert(dest.type == X86_OP_REG);
    int msb = dest.reg->int_val < 0;

    while(count != 0) {
        CF                = dest.reg->uint_val & 1;
        dest.reg->int_val = dest.reg->int_val >> 1; // signed divide
        // if(msb) {
        //     dest.reg->uint_val |= (1 << 31);
        // }
        count--;
    }
}

void shl(x86_operand dest, int count)
{
    assert(dest.type == X86_OP_REG);

    while(count != 0) {
        CF                 = dest.reg->uint_val & 0x80000000; // msb
        dest.reg->uint_val = dest.reg->uint_val << 1;
        count--;
    }
}

void ror(x86_operand dest, int count) {
    int i;
    for (i = 0; i < count; i++) {
        int lsb = dest.reg->uint_val & 1;
        dest.reg->uint_val >>= 1;
        dest.reg->uint_val += (lsb << 31);
    }
}

void fldi(double d)
{
    *st(0) = d;
}
