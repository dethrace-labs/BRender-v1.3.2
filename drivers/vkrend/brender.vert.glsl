#version 450

#define MAX_LIGHTS                      48
#define MAX_CLIP_PLANES                 6
#define SPECULARPOW_CUTOFF              0.6172
#define BR_SCALAR_EPSILON               1.192092896e-7f

#define DEBUG_DISABLE_LIGHTS            1
#define DEBUG_DISABLE_LIGHT_AMBIENT     0
#define DEBUG_DISABLE_LIGHT_DIRECTIONAL 0
#define DEBUG_DISABLE_LIGHT_POINT       0
#define DEBUG_DISABLE_LIGHT_POINTATTEN  0
#define DEBUG_DISABLE_LIGHT_SPOT        0
#define DEBUG_DISABLE_LIGHT_SPECULAR    0
#define ENABLE_PSX_SIMULATION           0

struct br_light
{
    vec4 position;
    vec4 direction;
    vec4 half_;
    vec4 colour;
    vec4 iclq;
    vec2 spot_angles;
};

layout(std140, binding = 1) uniform br_scene_state
{
    vec4 eye_view;
    br_light lights[MAX_LIGHTS];
    uint num_lights;

    vec4 clip_planes[MAX_CLIP_PLANES];
    uint num_clip_planes;

    float hither_z;
    float yon_z;
};

layout(std140, binding = 2) uniform br_model_state
{
    mat4 model_view;
    mat4 projection;
    mat4 projection_brender;
    mat4 mvp;
    mat4 normal_matrix;
    mat4 environment;
    mat4 map_transform;
    vec4 surface_colour;
    vec4 clear_colour;
    vec4 eye_m;

    float ka;
    float ks;
    float kd;
    float power;
    uint lighting;
    int uv_source;
    bool disable_colour_key;
    bool disable_texture;
    bool fog_enabled;
    vec4 fog_colour;
    float fog_min;
    float fog_max;
    float alpha;
    uint prelit;
};

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec4 aColour;

layout(location = 0) out vec4 position;
layout(location = 1) out vec2 uv;
layout(location = 2) out vec4 normal;
layout(location = 3) out vec4 colour;

layout(location = 4) out vec3 rawPosition;
layout(location = 5) out vec3 rawNormal;

layout(location = 6) out vec3 v_frag_pos;
layout(location = 7) out float v_view_z;

bool directLightExists;

#define SPECULAR_DOT()                    \
    {                                     \
        float rd = dot(dirn_norm, n) * 2.0; \
        vec4 r = n - rd;                  \
        r = r - dirn_norm;                \
                                          \
        _dot = dot(eye_view, r);          \
    }

#define SPECULAR_POWER(l) (_dot * (l)) / (power - (power * _dot) + _dot)

float calculateAttenuation(in br_light alp, in float dist)
{
    if (dist > alp.iclq.w)
        return 0.0;

    float attn;

    if (dist > alp.iclq.y)
        attn = (dist - alp.iclq.y) * alp.iclq.z;
    else
        attn = 0.0;

    return 1.0 - attn;
}

float shadingFilter(in float i)
{
#if ENABLE_PSX_SIMULATION
    i = floor(i * 255.0) / 255.0;
#endif
    return i;
}

vec3 lightingColourAmbient(in vec4 p, in vec4 n, in br_light alp)
{
    return ka * alp.iclq.x * alp.colour.xyz;
}

vec3 lightingColourDirect(in vec4 p, in vec4 n, in br_light alp)
{
    float _dot = max(dot(n, alp.direction), 0.0) * kd;

    _dot = shadingFilter(_dot);

    vec3 outColour = alp.colour.xyz * _dot;

#if !DEBUG_DISABLE_LIGHT_SPECULAR
    if (ks <= 0.0) {
        _dot = dot(n, alp.half_);

        if (_dot > SPECULARPOW_CUTOFF)
            outColour += SPECULAR_POWER(ks * alp.iclq.x);
    }
#endif
    return outColour;
}

vec3 lightingColourPoint(in vec4 p, in vec4 n, in br_light alp)
{
    float _dot;
    vec4 dirn, dirn_norm;

    dirn = alp.position - p;
    dirn_norm = normalize(dirn);

    float dist = length(dirn);

    _dot = max(dot(n, dirn_norm), 0.0) * kd;

    _dot = shadingFilter(_dot);

    vec3 outColour = _dot * (alp.iclq.x * alp.colour.xyz);

#if !DEBUG_DISABLE_LIGHT_SPECULAR
    if (ks != 0.0) {
        SPECULAR_DOT();

        if (_dot > SPECULARPOW_CUTOFF)
            outColour += SPECULAR_POWER(ks);
    }
#endif
    return (outColour);
}

vec3 lightingColourPointAtten(in vec4 p, in vec4 n, in br_light alp)
{
    float _dot;
    vec4 dirn, dirn_norm;

    dirn = alp.position - p;
    dirn_norm = normalize(dirn);
    _dot = ((max(dot(n, dirn_norm), 0.0) * kd));

    _dot = shadingFilter(_dot);

    float dist = length(dirn);
    float atten = calculateAttenuation(alp, dist);

    vec3 outColour = _dot * alp.colour.xyz;

#if !DEBUG_DISABLE_LIGHT_SPECULAR
    if (ks != 0.0) {
        SPECULAR_DOT();

        if (_dot > SPECULARPOW_CUTOFF)
            outColour += SPECULAR_POWER(ks);
    }
#endif
    return outColour * atten;
}

vec3 lightingColourSpot(in vec4 p, in vec4 n, in br_light alp)
{
    return vec3(0);
}

vec3 lightingColourSpotAtten(in vec4 p, in vec4 n, in br_light alp)
{
    return vec3(0);
}

vec4 fragmain()
{
    if (num_lights == 0u || lighting == 0u) {
        return surface_colour;
    }

#if DEBUG_DISABLE_LIGHTS
    return surface_colour;
#else

    vec3 _colour = surface_colour.xyz;

    vec3 lightColour = vec3(0.0);
    vec3 directLightColour = vec3(0.0);
    directLightExists = false;

    for (uint i = 0u; i < num_lights; ++i) {
        if(lights[i].colour.w != 0.0) {
            lightColour += lightingColourAmbient(position, normal, lights[i]);
            continue;
        }
        if (lights[i].position.w == 0.0) {
            directLightExists = true;
            directLightColour += lightingColourDirect(position, normal, lights[i]);
        } else {
            if (lights[i].spot_angles == vec2(0.0, 0.0)) {
                if (lights[i].iclq.zw == vec2(0)) {
                    lightColour += lightingColourPoint(position, normal, lights[i]);
                } else {
                    lightColour += lightingColourPointAtten(position, normal, lights[i]);
                }
            } else {
                if (lights[i].iclq.zw == vec2(0))
                    lightColour += lightingColourSpot(position, normal, lights[i]);
                else
                    lightColour += lightingColourSpotAtten(position, normal, lights[i]);
            }
        }
    }

    lightColour += directLightColour;
    lightColour *= _colour;

    lightColour = clamp(lightColour, 0.0, 1.0);
    return vec4(lightColour, surface_colour.a);
#endif
}

#if ENABLE_PSX_SIMULATION
vec4 PSXify_pos(in vec4 vertex, in vec2 resolution)
{
    vec4 snappedPos = vertex;
    snappedPos.xyz = vertex.xyz / vertex.w;
    snappedPos.xy = floor(resolution.xy * snappedPos.xy) / resolution;
    snappedPos.xyz *= vertex.w;
    return snappedPos;
}
#endif

void main()
{
    v_frag_pos = vec3((projection_brender * model_view) * vec4(aPosition, 1.0));
    vec4 pos = vec4(aPosition, 1.0);

    position = model_view * pos;
    v_view_z = position.z;
    normal = vec4(normalize(mat3(normal_matrix) * aNormal), 0);
    uv = aUV;

    if (lighting == 1u) {
        if (prelit == 1u) {
            colour = vec4(aColour.rgb, 1);
        } else {
            colour = fragmain();
        }
    }

    rawPosition = aPosition;
    rawNormal   = aNormal;

    if (!directLightExists && num_lights > 0u && lighting == 1u) {
        colour += vec4(clear_colour.rgb, 0.0);
    }

    pos = projection * model_view * pos;

    // Match GL depth mapping: GL maps NDC z [-1,1] → depth [0,1] via (ndc+1)/2.
    // After negate_z_column near→+1 far→-1, so GL depth near→1 far→0.
    // VK has no depthRange remap, so we replicate it here:
    // depth_vk = (clip_z/clip_w + 1)/2 = (clip_z + clip_w) / (2*clip_w)
    pos.z = (pos.z + pos.w) * 0.5;

    gl_Position = pos;
}
