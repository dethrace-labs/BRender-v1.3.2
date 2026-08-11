/*
 * VIDEO structures
 */
#ifndef VIDEO_H_
#define VIDEO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "shader_data.h"

typedef struct _VIDEO {
    GLint maxUniformBlockSize;
    GLint maxUniformBufferBindings;
    GLint maxVertexUniformBlocks;
    GLint maxFragmentUniformBlocks;
    GLint maxSamples;
    GLfloat maxAnisotropy;

    struct {
        GLuint program;
        GLint aPosition;     /* Position, vec3 */
        GLint aColour;       /* Colour, vec3 */
        GLint aUV;           /* UV, vec2 */
        GLint uSampler;      /* Sampler, sampler2D */
        GLint uMVP;          /* Model-View-Projection Matrix, mat4 */
        GLint uFlipVertically;
        GLint uDiscardPurplePixels;
    } defaultProgram;

    struct {
        GLuint program;

        struct {
            GLint aPosition; /* Vectex Position, vec3 */
            GLint aUV;       /* UV, vec2 */
            GLint aNormal;   /* Vertex Normal, vec3 */
            GLint aColour;   /* Vertex colour, vec4 */
        } attributes;

        struct {
            GLint main_texture; /* sampler2D */
        } uniforms;

        GLuint uboScene;
        GLuint blockIndexScene;
        GLuint blockBindingScene;

        GLuint uboModel;
        GLuint blockIndexModel;
        GLuint blockBindingModel;

        GLint mainTextureBinding;
    } brenderProgram;
} VIDEO, *HVIDEO;

#ifdef __cplusplus
};
#endif
#endif /* VIDEO_H_ */
