#include "drv.h"
#include "commonrend.h"

/*
 * Fill the backend-agnostic shader_data_model material fields from the
 * current (or stored) renderer state. Backend texture binding and the
 * disable_texture/disable_colour_key flags (which depend on the backend's
 * texture handles) stay in each driver.
 */
void BREND_FN(State, FillModel)(state_stack* state, uint32_t states, shader_data_model* model) {
    /* Only use the states we want (if valid). */
    states = state->valid & states;

    if (states & MASK_STATE_SURFACE) {
        if (state->surface.colour_source == BRT_SURFACE) {
            br_uint_32 colour = state->surface.colour;
            float r = BR_RED(colour) / 255.0f;
            float g = BR_GRN(colour) / 255.0f;
            float b = BR_BLU(colour) / 255.0f;
            BrVector4Set(&model->surface_colour, r, g, b, state->surface.opacity);
        } else {
            BrVector4Set(&model->surface_colour, 0.0f, 1.0f, 1.0f, state->surface.opacity);
        }

        model->ka = state->surface.ka;
        model->ks = state->surface.ks;
        model->kd = state->surface.kd;
        model->power = state->surface.power;

        switch (state->surface.mapping_source) {
        case BRT_GEOMETRY_MAP:
        default:
            model->uv_source = 0;
            break;

        case BRT_ENVIRONMENT_LOCAL:
            model->uv_source = 1;
            break;

        case BRT_ENVIRONMENT_INFINITE:
            model->uv_source = 2;
            break;
        }

        BrMatrix4Copy23(&model->map_transform, &state->surface.map_transform);

        model->prelit = state->surface.prelighting;
        model->lighting = state->surface.lighting;
    }

    if (states & MASK_STATE_PRIMITIVE) {
        model->fog_enabled = state->prim.fog_enabled;
        BrVector4Set(&model->fog_colour, BR_RED(state->prim.fog_colour) / 255.0f,
            BR_GRN(state->prim.fog_colour) / 255.0f, BR_BLU(state->prim.fog_colour) / 255.0f, 0.0f);
        model->fog_min = state->prim.fog_min;
        model->fog_max = state->prim.fog_max;
    }
}
