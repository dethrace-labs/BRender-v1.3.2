##ifdef GL_ES
#version 300 es
precision mediump float;
precision mediump int;
precision lowp usampler2D;
##endif
##ifdef GL_CORE
#version 140
##endif

#extension GL_ARB_explicit_attrib_location : require

in vec3 colour;
in vec2 uv;

uniform sampler2D uSampler;

layout (location = 0) out vec4 mainColour;

void main()
{
    float flip = 1.0f;
    mainColour = texture(uSampler, vec2(uv.x, abs(flip - uv.y))) * vec4(colour.rgb, 1.0);
}
