#ifndef PFPSETUP_H
#define PFPSETUP_H

#include <stdint.h>
#include "../softrend/ddi/priminfo.h"

void TriangleSetup_ZPT(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2);
void TriangleSetup_ZPTI(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2);

void TriangleSetup_ZPT_NOCHEAT(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2);
void TriangleSetup_ZPTI_NOCHEAT(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2);

#endif
