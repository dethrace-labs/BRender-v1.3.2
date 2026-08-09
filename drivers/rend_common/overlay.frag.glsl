#version 450

layout(binding = 0) uniform sampler2D overlayTexture;

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(overlayTexture, fragUV);
    if (texColor.rgb == vec3(1.0, 0.0, 1.0)) {
        discard;
    }
    outColor.rgb = texColor.rgb;
    outColor.a = texColor.a;
}
