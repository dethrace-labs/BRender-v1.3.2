#include "drv.h"

void StateVKInit(state_all *state, void *res)
{
    state->res = res;

    StateVKInitMatrix(state);
    StateVKInitClip(state);
    StateVKInitCull(state);
    StateVKInitSurface(state);
    StateVKInitPrimitive(state);
    StateVKInitOutput(state);
    StateVKInitHidden(state);
    StateVKInitLight(state);

    state->current = state->stack;
    StateVKCopy(state->current, &state->default_, ~0u);
}

void StateVKCopy(state_stack *dst, state_stack *src, uint32_t mask)
{
    mask &= src->valid;

    dst->valid |= mask;

    if(mask & MASK_STATE_MATRIX)
        dst->matrix = src->matrix;

    if(mask & MASK_STATE_CLIP) {
        for(int i = 0; i < MAX_STATE_CLIP_PLANES; ++i)
            dst->clip[i] = src->clip[i];
    }

    if(mask & MASK_STATE_CULL)
        dst->cull = src->cull;

    if(mask & MASK_STATE_SURFACE)
        dst->surface = src->surface;

    if(mask & MASK_STATE_PRIMITIVE)
        dst->prim = src->prim;

    if(mask & MASK_STATE_OUTPUT)
        dst->output = src->output;

    if(mask & MASK_STATE_LIGHT)
        for(int i = 0; i < MAX_STATE_LIGHTS; ++i)
            dst->light[i] = src->light[i];
}

br_boolean StateVKPush(state_all *state, uint32_t mask)
{
    if(state->top >= MAX_STATE_STACK)
        return BR_FALSE;

    ++state->top;
    ++state->current;

    *state->current = *(state->current - 1);
    return BR_TRUE;
}

br_boolean StateVKPop(state_all *state, uint32_t mask)
{
    if(state->top <= 0)
        return BR_FALSE;

    --state->top;
    --state->current;

    return BR_TRUE;
}

void StateVKDefault(state_all *state, uint32_t mask)
{
    StateVKCopy(state->current, &state->default_, mask);
}

struct br_tv_template *StateVKGetStateTemplate(state_all *state, br_token part, br_int_32 index)
{
    switch(part) {
        case BRT_MATRIX:
            return state->templates.matrix;

        case BRT_CLIP:
            if(index >= MAX_STATE_CLIP_PLANES)
                return NULL;

            return state->templates.clip[index];

        case BRT_CULL:
            return state->templates.cull;

        case BRT_SURFACE:
            return state->templates.surface;

        case BRT_PRIMITIVE:
            return state->templates.prim;

        case BRT_HIDDEN_SURFACE:
            return state->templates.hidden;
        case BRT_LIGHT:
            if(index >= MAX_STATE_LIGHTS)
                return NULL;

            return state->templates.light[index];

        case BRT_OUTPUT:
            return state->templates.output;

        case BRT_ENABLE:
        case BRT_BOUNDS:
        default:
            break;
    }

    return NULL;
}
