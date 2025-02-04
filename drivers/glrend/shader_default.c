#include "drv.h"
#include "brassert.h"
#include "default.frag.glsl.h"
#include "default.vert.glsl.h"


br_boolean VIDEOI_CompileDefaultShader(HVIDEO hVideo) {
    GLuint vert = VIDEOI_CreateAndCompileShader(GL_VERTEX_SHADER, DEFAULT_VERT_GLSL, sizeof(DEFAULT_VERT_GLSL));
    if (!vert)
        return BR_FALSE;
    GLuint frag = VIDEOI_CreateAndCompileShader(GL_FRAGMENT_SHADER, DEFAULT_FRAG_GLSL, sizeof(DEFAULT_FRAG_GLSL));
    if (!frag) {
        glDeleteShader(vert);
        return BR_FALSE;
    }
    hVideo->defaultProgram.program = VIDEOI_CreateAndCompileProgram(vert, frag);
    glDeleteShader(vert);
    glDeleteShader(frag);
    if (hVideo->defaultProgram.program) {
        hVideo->defaultProgram.aPosition = glGetAttribLocation(hVideo->defaultProgram.program, "aPosition");
        hVideo->defaultProgram.aColour = glGetAttribLocation(hVideo->defaultProgram.program, "aColour");
        hVideo->defaultProgram.aUV = glGetAttribLocation(hVideo->defaultProgram.program, "aUV");

        hVideo->defaultProgram.uSampler = glGetUniformLocation(hVideo->defaultProgram.program, "uSampler");
        hVideo->defaultProgram.uMVP = glGetUniformLocation(hVideo->defaultProgram.program, "uMVP");
        glUseProgram(hVideo->defaultProgram.program);
    }

    UASSERT(glGetError() == 0);
    return hVideo->defaultProgram.program != 0;
}
