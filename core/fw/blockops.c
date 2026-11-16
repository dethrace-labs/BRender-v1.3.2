#include "fw.h"

// Added by JeffH for portability. See the original blockops.asm + blockops.h

void BrBlockFill(void *dest_ptr, int value, int dwords) {
    BrMemSet(dest_ptr, value, dwords * 4);
}
