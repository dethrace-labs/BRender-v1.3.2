#include "brender.h"
#include "host.h"

void CPUInfo(br_token *cpu_type, br_uint_32 *features) {
    *cpu_type = BRT_INTEL_PENTIUM;
    *features = 0;
}

br_error BR_RESIDENT_ENTRY HostExceptionGet(br_uint_8 exception, br_uint_32 *offp, br_uint_16 *selp) {
    // Not implemented
    BrAbort();
    return BRE_FAIL;
}
br_error BR_RESIDENT_ENTRY HostExceptionSet(br_uint_8 exception, br_uint_32 off, br_uint_16 sel) {
    // Not implemented
    BrAbort();
    return BRE_FAIL;
}

void BR_RESIDENT_ENTRY HostFarBlockWrite(br_uint_32 offset, br_uint_16 sel, void *block, br_uint_32 count) {
    // Not implemented
    BrAbort();
}
void BR_RESIDENT_ENTRY HostFarBlockRead(br_uint_32 offset, br_uint_16 sel, void *block, br_uint_32 count) {
    // Not implemented
    BrAbort();
}

br_uint_32 BR_RESIDENT_ENTRY HostFarStringWrite(br_uint_32 offset, br_uint_16 sel, br_uint_8 *string, br_uint_32 max) {
    // Not implemented
    BrAbort();
    return 0;
}
br_uint_32 BR_RESIDENT_ENTRY HostFarStringRead(br_uint_32 offset, br_uint_16 sel, br_uint_8 *string, br_uint_32 max) {
    // Not implemented
    BrAbort();
    return 0;
}

void BR_RESIDENT_ENTRY HostFarBlockFill(br_uint_32 offset, br_uint_16 sel, br_uint_8 value, br_uint_32 count) {
    // Not implemented
    BrAbort();
}

void BR_RESIDENT_ENTRY HostFarByteWrite(br_uint_32 offset, br_uint_16 sel, br_uint_8 value) {
    // Not implemented
    BrAbort();
}

void BR_RESIDENT_ENTRY HostFarWordWrite(br_uint_32 offset, br_uint_16 sel, br_uint_16 value) {
    // Not implemented
    BrAbort();
}

void BR_RESIDENT_ENTRY HostFarDWordWrite(br_uint_32 offset, br_uint_16 sel, br_uint_32 value) {
    // Not implemented
    BrAbort();
}

br_uint_8 BR_RESIDENT_ENTRY HostFarByteRead(br_uint_32 offset, br_uint_16 sel) {
    // Not implemented
    BrAbort();
    return 0;
}

br_uint_16 BR_RESIDENT_ENTRY HostFarWordRead(br_uint_32 offset, br_uint_16 sel) {
    // Not implemented
    BrAbort();
    return 0;
}

br_uint_32 BR_RESIDENT_ENTRY HostFarDWordRead(br_uint_32 offset, br_uint_16 sel) {
    // Not implemented
    BrAbort();
    return 0;
}

br_error BR_RESIDENT_ENTRY HostInterruptCall(br_uint_8 vector, union host_regs *regs) {
    // Not implemented
    BrAbort();
    return BRE_FAIL;
}

br_error BR_RESIDENT_ENTRY HostRegistersGet(union host_regs *regs) {
    // Not implemented
    BrAbort();
    return BRE_FAIL;
}

br_error BR_RESIDENT_ENTRY HostInterruptGet(br_uint_8 vector, br_uint_32 *offp, br_uint_16 *selp) {
    // Not implemented
    BrAbort();
    return BRE_FAIL;
}

br_error BR_RESIDENT_ENTRY HostInterruptSet(br_uint_8 vector, br_uint_32 off, br_uint_16 sel) {
    // Not implemented
    BrAbort();
    return BRE_FAIL;
}

br_error BR_RESIDENT_ENTRY HostRealAllocate(struct host_real_memory *mem, br_uint_32 size) {
    // Not implemented
    BrAbort();
    return BRE_FAIL;
}

br_error BR_RESIDENT_ENTRY HostRealFree(struct host_real_memory *mem) {
    // Not implemented
    BrAbort();
    return BRE_FAIL;
}

br_error BR_RESIDENT_ENTRY HostRealInterruptGet(br_uint_8 vector, br_uint_16 *offp, br_uint_16 *vsegp) {
    // Not implemented
    BrAbort();
    return BRE_FAIL;
}

br_error BR_RESIDENT_ENTRY HostRealInterruptSet(br_uint_8 vector, br_uint_16 voff, br_uint_16 vseg) {
    // Not implemented
    BrAbort();
    return BRE_FAIL;
}

br_error BR_RESIDENT_ENTRY HostRealInterruptCall(br_uint_8 vector, union host_regs *regs) {
    // Not implemented
    BrAbort();
    return BRE_FAIL;
}

void BR_RESIDENT_ENTRY HostRealBlockWrite(br_uint_16 offset, br_uint_16 seg, void *block, br_uint_32 count) {
    // Not implemented
    BrAbort();
}

void BR_RESIDENT_ENTRY HostRealBlockRead(br_uint_16 offset, br_uint_16 seg, void *block, br_uint_32 count) {
    // Not implemented
    BrAbort();
}

br_uint_32 BR_RESIDENT_ENTRY HostRealStringWrite(br_uint_16 offset, br_uint_16 seg, br_uint_8 *string, br_uint_32 max) {
    // Not implemented
    BrAbort();
    return 0;
}

br_uint_32 BR_RESIDENT_ENTRY HostRealStringRead(br_uint_16 offset, br_uint_16 seg, br_uint_8 *string, br_uint_32 max) {
    // Not implemented
    BrAbort();
    return 0;
}

void BR_RESIDENT_ENTRY HostRealBlockFill(br_uint_16 offset, br_uint_16 seg, br_uint_8 value, br_uint_32 count) {
    // Not implemented
    BrAbort();
}

void BR_RESIDENT_ENTRY HostRealByteWrite(br_uint_16 offset, br_uint_16 seg, br_uint_8 value) {
    // Not implemented
    BrAbort();
}

void BR_RESIDENT_ENTRY HostRealWordWrite(br_uint_16 offset, br_uint_16 seg, br_uint_16 value) {
    // Not implemented
    BrAbort();
}

void BR_RESIDENT_ENTRY HostRealDWordWrite(br_uint_16 offset, br_uint_16 seg, br_uint_32 value) {
    // Not implemented
    BrAbort();
}

br_uint_8 BR_RESIDENT_ENTRY HostRealByteRead(br_uint_16 offset, br_uint_16 seg) {
    // Not implemented
    BrAbort();
    return 0;
}

br_uint_16 BR_RESIDENT_ENTRY HostRealWordRead(br_uint_16 offset, br_uint_16 seg) {
    // Not implemented
    BrAbort();
    return 0;
}

br_uint_32 BR_RESIDENT_ENTRY HostRealDWordRead(br_uint_16 offset, br_uint_16 seg) {
    // Not implemented
    BrAbort();
    return 0;
}

br_error BR_RESIDENT_ENTRY HostSelectorReal(br_uint_16 *selp) {
    // Not implemented
    BrAbort();
    return BRE_FAIL;
}

br_error BR_RESIDENT_ENTRY HostSelectorDS(br_uint_16 *selp) {
    // Not implemented
    BrAbort();
    return BRE_FAIL;
}

br_error BR_RESIDENT_ENTRY HostSelectorCS(br_uint_16 *selp) {
    // Not implemented
    BrAbort();
    return BRE_FAIL;
}

br_error BR_RESIDENT_ENTRY HostSelectorSS(br_uint_16 *selp) {
    // Not implemented
    BrAbort();
    return BRE_FAIL;
}

br_error BR_RESIDENT_ENTRY HostSelectorES(br_uint_16 *selp) {
    // Not implemented
    BrAbort();
    return BRE_FAIL;
}

br_error BR_ASM_CALL RealSelectorBegin(void) {
    // Not implemented
    return BRE_FAIL;
}

void BR_ASM_CALL RealSelectorEnd(void) {
    // Not implemented
}
