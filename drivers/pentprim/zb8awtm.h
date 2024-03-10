
#include "priminfo.h"
#include <stdarg.h>

void TriangleRender_ZT_I8_D16(brp_block *block, ...);

void Draw_ZT_I8_NWLR();

void Draw_ZT_I8_NWRL();

void Draw_ZT_I8_DWLR();
void Draw_ZT_I8_DWRL();

void DRAW_ZT_I8(uint32_t *minorX, uint32_t *d_minorX, char direction, int32_t *halfCount, char wrap_flag, char fogging,
                char blend);
void PER_SCAN_ZT(int32_t *halfCount, char wrap_flag, uint32_t *minorX, uint32_t *d_minorX);
