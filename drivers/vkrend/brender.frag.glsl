#version 450

#define MAX_LIGHTS                   48
#define MAX_CLIP_PLANES              6

#define ENABLE_GAMMA_CORRECTION      0
#define ENABLE_SIMULATE_8BIT_COLOUR  0
#define ENABLE_SIMULATE_16BIT_COLOUR 0

#define UV_SOURCE_MODEL              0
#define UV_SOURCE_ENV_L              1
#define UV_SOURCE_ENV_I              2

#define FOGGING_EMULATE_3DFX_DENSITY_MULTIPLIER 80

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

layout(location = 0) in vec4 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 normal;
layout(location = 3) in vec4 colour;

layout(location = 4) in vec3 rawPosition;
layout(location = 5) in vec3 rawNormal;

layout(location = 6) in vec3 v_frag_pos;
layout(location = 7) in float v_view_z;

layout(location = 0) out vec4 fragColour;

layout(binding = 0) uniform sampler2D main_texture;

vec3 adjustBrightness(in vec3 colour, in float brightness)
{
    return colour + brightness;
}

vec3 adjustContrast(in vec3 colour, in float contrast)
{
    return 0.5 + (1.0 + contrast) * (colour - 0.5);
}

vec3 adjustExposure(in vec3 colour, in float exposure)
{
    return vec3(1.0) - exp(-colour * exposure);
}

vec3 adjustSaturation(in vec3 colour, in float saturation)
{
    const vec3 luminosityFactor = vec3(0.2126, 0.7152, 0.0722);
    vec3 grayscale = vec3(dot(colour, luminosityFactor));

    return mix(grayscale, colour, 1.0 + saturation);
}

vec2 SurfaceMapEnvironment(in vec3 eye, in vec3 normal, in mat4 model_to_environment) {
    vec3 r;
    vec4 wr;
    float d, cu, cv;

    r = reflect(eye, normalize(normal));

    wr = model_to_environment * vec4(r, 0.0);
    vec3 wr2 = normalize(wr.xyz);

    cu = 0.5 + atan(wr2.x, -wr2.z) * 0.159154943091895;
    cv = 0.5 + -wr2.y * 0.5;

    return vec2(cu, cv);
}

vec2 SurfaceMap(in vec3 position, in vec3 normal, in vec2 uv)
{
    if(uv_source == UV_SOURCE_ENV_L) {
        vec3 eye = normalize(eye_m.xyz - position);
        uv = SurfaceMapEnvironment(eye, normal, environment);
    } else if(uv_source == UV_SOURCE_ENV_I) {
        vec3 eye = normalize(eye_m.xyz);
        uv = SurfaceMapEnvironment(eye, normal, environment);
    } else {
    }

    return (map_transform * vec4(uv, 1.0, 0.0)).xy;
}

void processClipPlanes() {
    for(uint i = 0u; i < num_clip_planes; i++) {
        float d = dot(clip_planes[i], projection_brender * model_view * vec4(rawPosition.xyz, 1));
        if (d < 0.0) {
            discard;
        }
    }
}

void doDistanceFog() {
    if (fog_enabled) {
        float linear_depth = -v_view_z;

        float linear_depth_normalized = linear_depth / yon_z;
        float density = 1/fog_max * FOGGING_EMULATE_3DFX_DENSITY_MULTIPLIER;
        float fogging_factor = 1.0 - exp(-density * linear_depth_normalized * linear_depth_normalized);
        fragColour.rgb = mix(fragColour.rgb, fog_colour.rgb, clamp(fogging_factor, 0.0, 1.0));
    }
}

void main() {
    vec4 textureColour;

    processClipPlanes();

    vec2 mappedUV = SurfaceMap(rawPosition, rawNormal, uv);

    if (disable_texture) {
        textureColour = surface_colour;
    } else {
        textureColour = texture(main_texture, mappedUV);
        // full black pixel is transparent
        if (!disable_colour_key && textureColour.rgb == vec3(0)) {
            discard;
        }
    }

    if (lighting == 1u) {
        fragColour.rgb = colour.rgb * textureColour.rgb;
    } else {
        fragColour.rgb = textureColour.rgb;
    }

    fragColour.a = alpha;

    if (lighting == 1u && prelit == 0u) {
        fragColour.rgb *= vec3(ka, ka, ka);
    }

    doDistanceFog();
}
