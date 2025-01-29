#include "drv.h"
#include "brassert.h"

static const GLchar g_DefaultVertexShader[] = "#version 150\n"
                                              "#extension GL_ARB_explicit_attrib_location:require\n"
                                              "layout (location = 0) in vec3 aPosition;\n"
                                              "layout (location = 1) in vec3 aColour;\n"
                                              "layout (location = 2) in vec2 aUV;\n"
                                              "out vec3 colour;\n"
                                              "out vec2 uv;\n"
                                              "\n"
                                              "uniform mat4 uMVP;\n"
                                              "\n"
                                              "void main()\n"
                                              "{\n"
                                              "	gl_Position = vec4(aPosition, 1.0);\n"
                                              "	colour = aColour;\n"
                                              "	uv = aUV;\n"
                                              "}\n";

static const GLchar g_DefaultFragmentShader
    []
    = "#version 150\n"
      "#extension GL_ARB_explicit_attrib_location : require\n"
      "\n"
      "in vec3 colour;\n"
      "in vec2 uv;\n"
      "uniform sampler2D uSampler;\n"
      "uniform float uVerticalFlip = 1.0f;\n"
      "\n"
      "layout (location = 0) out vec4 mainColour;\n"
      "\n"
      "void main()\n"
      "{\n"
      "	mainColour = texture(uSampler, vec2(uv.x, abs(uVerticalFlip - uv.y))) * vec4(colour.rgb, 1.0);\n"
      "}\n";
//  \n

br_boolean VIDEOI_CompileDefaultShader(HVIDEO hVideo) {
    GLuint vert = VIDEOI_CreateAndCompileShader(GL_VERTEX_SHADER, g_DefaultVertexShader, sizeof(g_DefaultVertexShader));
    if (!vert)
        return BR_FALSE;
UASSERT(glGetError() == 0);
    GLuint frag = VIDEOI_CreateAndCompileShader(GL_FRAGMENT_SHADER, g_DefaultFragmentShader, sizeof(g_DefaultFragmentShader));
    if (!frag) {
        glDeleteShader(vert);
        return BR_FALSE;
    }
UASSERT(glGetError() == 0);
    hVideo->defaultProgram.program = VIDEOI_CreateAndCompileProgram(vert, frag);
UASSERT(glGetError() == 0);
    glDeleteShader(vert);
    glDeleteShader(frag);
UASSERT(glGetError() == 0);
    if (hVideo->defaultProgram.program) {
        hVideo->defaultProgram.aPosition = glGetAttribLocation(hVideo->defaultProgram.program, "aPosition");
        hVideo->defaultProgram.aColour = glGetAttribLocation(hVideo->defaultProgram.program, "aColour");
        hVideo->defaultProgram.aUV = glGetAttribLocation(hVideo->defaultProgram.program, "aUV");

        hVideo->defaultProgram.uSampler = glGetUniformLocation(hVideo->defaultProgram.program, "uSampler");
        hVideo->defaultProgram.uMVP = glGetUniformLocation(hVideo->defaultProgram.program, "uMVP");
        hVideo->defaultProgram.uVerticalFlip = glGetUniformLocation(hVideo->defaultProgram.program, "uVerticalFlip");
        UASSERT(glGetError() == 0);
        // glBindFragDataLocation(hVideo->textProgram.program, 0, "mainColour");
        UASSERT(glGetError() == 0);
    }
UASSERT(glGetError() == 0);

    return hVideo->defaultProgram.program != 0;
}
