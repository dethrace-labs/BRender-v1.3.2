/*
 * VIDEO methods
 */
#include "brassert.h"
#include "drv.h"

#include <stdio.h>
#include <string.h>

int glContextIsOpenGLES() {
    const char* version = (const char*)glGetString(GL_VERSION);
    if (version == NULL) {
        BR_FATAL("Failed to retrieve OpenGL version");
        return 0;
    }
    if (strstr(version, "OpenGL ES") || strstr(version, "GLES")) {
        return 1;
    }

    return 0;
}

// Quick n dirty shader pre-processor
// Wrap opengles only lines with ##ifdef GL_ES ... ##endif
// Wrap opengl core only lines with ##ifdef GL_CORE ... ##endif
// Note the double "##" to avoid collision with the standard glsl preprocessor
char* preprocessShader(char* shader, size_t size) {
    int i;
    char *processed;
    int line_i;
    char line[2048];
    int is_context_opengles;
    int filter_state;  // 0 - none, 1, only opengles, 2 only opengl core

    line_i = 0;
    filter_state = 0;
    is_context_opengles = glContextIsOpenGLES();
    processed = BrScratchAllocate(size);
    BrMemSet(processed, 0, sizeof(processed));

    for (i = 0; i < size; i++) {
        line[line_i] = shader[i];
        line_i++;
        if (shader[i] == '\n') {
            // we've captured a whole line
            if (strcmp(line, "##ifdef GL_ES\n") == 0) {
                filter_state = 1;
            } else if (strcmp(line, "##ifdef GL_CORE\n") == 0) {
                filter_state = 2;
            } else if (strcmp(line, "##endif\n") == 0) {
                filter_state = 0;
            } else {
                if (filter_state == 1 && is_context_opengles) {
                    strcat(processed, line);
                } else if (filter_state == 2 && !is_context_opengles) {
                    strcat(processed, line);
                } else if (filter_state == 0) {
                    strcat(processed, line);
                }
            }
            BrMemSet(line, 0, sizeof(line));
            line_i = 0;
        }
    }
    return processed;
}

GLuint VIDEOI_CreateAndCompileShader(const char *name, GLenum type, const char* shader, size_t size) {
    GLuint s;
    GLint _size, status;
    char *processed_shader;

    ASSERT(type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER);

    // processed_shader was alloc'd from scratch
    processed_shader = preprocessShader(shader, size);

    s = glCreateShader(type);
    _size = (GLint)size;
    glShaderSource(s, 1, &processed_shader, &_size);
    glCompileShader(s);

    BrScratchFree(processed_shader);

    status = GL_FALSE;
    glGetShaderiv(s, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        char errorBuffer[1024];
        GLint maxLength;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &maxLength);

        if (maxLength > sizeof(errorBuffer))
            maxLength = sizeof(errorBuffer);

        glGetShaderInfoLog(s, maxLength, &maxLength, errorBuffer);
        errorBuffer[maxLength - 1] = '\0';

        BR_FATAL2("VIDEO: Error compiling shader %s:\n%s", name, errorBuffer);
        glDeleteShader(s);
        return 0;
    }

    GL_CHECK_ERROR();
    return s;
}

GLuint VIDEOI_LoadAndCompileShader(GLenum type, const char* path, const char* default_data, size_t default_size) {
    GLchar* source;
    size_t size;
    GLuint shader;

    // if (path == NULL || (source = BrFileLoad(NULL, path, &size)) == NULL) {
    //     source = (GLchar*)default_data;
    //     size = default_size;
    // }

    source = (GLchar*)default_data;
    size = default_size;

    shader = VIDEOI_CreateAndCompileShader(path, type, source, size);

    if (source != default_data)
        BrResFree(source);

    return shader;
}

GLuint VIDEOI_CreateAndCompileProgram(GLuint vert, GLuint frag) {
    GLuint program;
    GLint status;

    if ((program = glCreateProgram()) == 0) {
        BR_FATAL("VIDEO: Error creating program.");
        return 0;
    }

    glAttachShader(program, vert);
    glAttachShader(program, frag);

    glLinkProgram(program);

    status = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        char errorBuffer[1024];
        GLint maxLength;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

        if (maxLength > sizeof(errorBuffer))
            maxLength = sizeof(errorBuffer);

        glGetProgramInfoLog(program, maxLength, &maxLength, errorBuffer);
        errorBuffer[maxLength - 1] = '\0';
        BR_FATAL1("VIDEO: Error linking program:\n%s", errorBuffer);

        glDetachShader(program, vert);
        glDetachShader(program, frag);
        glDeleteProgram(program);
        program = 0;
    }

    GL_CHECK_ERROR();
    return program;
}

HVIDEO VIDEO_Open(HVIDEO hVideo, const char* vertShader, const char* fragShader) {
    if (hVideo == NULL) {
        BR_FATAL("VIDEO: Invalid handle.");
        return NULL;
    }

    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &hVideo->maxUniformBlockSize);
    glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &hVideo->maxUniformBufferBindings);
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_BLOCKS, &hVideo->maxVertexUniformBlocks);
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_BLOCKS, &hVideo->maxFragmentUniformBlocks);
    glGetIntegerv(GL_MAX_SAMPLES, &hVideo->maxSamples);

    if (GLAD_GL_EXT_texture_filter_anisotropic) {
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &hVideo->maxAnisotropy);
    }

    if (!VIDEOI_CompileDefaultShader(hVideo)) {
        return NULL;
    }

    if (!VIDEOI_CompileBRenderShader(hVideo, vertShader, fragShader)) {
        glDeleteProgram(hVideo->defaultProgram.program);
        return NULL;
    }

    GL_CHECK_ERROR();
    return hVideo;
}

void VIDEO_Close(HVIDEO hVideo) {
    if (!hVideo)
        return;

    glUseProgram(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    if (hVideo->brenderProgram.blockIndexScene != GL_INVALID_INDEX)
        glDeleteBuffers(1, &hVideo->brenderProgram.uboScene);

    if (hVideo->brenderProgram.blockIndexModel != GL_INVALID_INDEX)
        glDeleteBuffers(0, &hVideo->brenderProgram.uboModel);

    glDeleteProgram(hVideo->defaultProgram.program);
}

br_error VIDEOI_BrPixelmapGetTypeDetails(br_uint_8 pmType, GLint* internalFormat, GLenum* format, GLenum* type,
    GLsizeiptr* elemBytes, br_boolean* blended) {
    br_boolean is_blended = BR_FALSE;
    switch (pmType) {
    case BR_PMT_RGB_555:
        *internalFormat = GL_RGB;
        *format = GL_BGRA;
        *type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
        *elemBytes = 2;
        break;
    case BR_PMT_RGB_565:
        *internalFormat = GL_RGB;
        *format = GL_RGB;
        *type = GL_UNSIGNED_SHORT_5_6_5;
        *elemBytes = 2;
        break;
    case BR_PMT_INDEX_8:
        *internalFormat = GL_RGBA;
        *format = GL_RGBA;
        *type = GL_UNSIGNED_BYTE;
        *elemBytes = 4;
        break;
    case BR_PMT_RGB_888:
        *internalFormat = GL_RGB;
#if BR_ENDIAN_LITTLE
        *format = GL_BGR;
#else
        *format = GL_RGB;
#endif
        *type = GL_UNSIGNED_BYTE;
        *elemBytes = 3;
        break;
    case BR_PMT_RGBX_888:
        *internalFormat = GL_RGB;
        *format = GL_BGRA;
        *type = GL_UNSIGNED_INT_8_8_8_8_REV;
        *elemBytes = 4;
        break;
    case BR_PMT_RGBA_8888:
        *internalFormat = GL_RGBA;
        *format = GL_BGRA;
        *type = GL_UNSIGNED_INT_8_8_8_8_REV;
        *elemBytes = 4;
        is_blended = BR_TRUE;
        break;
    case BR_PMT_BGR_555:
        *internalFormat = GL_RGB;
        *format = GL_BGR;
        *type = GL_UNSIGNED_SHORT_5_5_5_1;
        *elemBytes = 2;
        break;
    case BR_PMT_RGBA_4444:
        *internalFormat = GL_RGBA;
        *format = GL_RGBA;
        *type = GL_UNSIGNED_SHORT_4_4_4_4;
        *elemBytes = 2;
        is_blended = BR_TRUE;
        break;
    case BR_PMT_ARGB_4444:
        *internalFormat = GL_RGBA;
        *format = GL_BGRA;
        *type = GL_UNSIGNED_SHORT_4_4_4_4_REV;
        *elemBytes = 2;
        is_blended = BR_TRUE;
        break;
    case BR_PMT_RGB_332:
        *internalFormat = GL_RGB;
        *format = GL_RGB;
        *type = GL_UNSIGNED_BYTE_3_3_2;
        *elemBytes = 1;
        break;
    case BR_PMT_DEPTH_8:
        *internalFormat = GL_DEPTH_COMPONENT;
        *format = GL_DEPTH_COMPONENT;
        *type = GL_UNSIGNED_BYTE;
        *elemBytes = 1;
        break;
    case BR_PMT_DEPTH_16:
        *internalFormat = GL_DEPTH_COMPONENT16;
        *format = GL_DEPTH_COMPONENT;
        *type = GL_UNSIGNED_SHORT;
        *elemBytes = 2;
        break;
    default:
        BR_FATAL1("GLREND: Unsupported BRender texture format %d.", pmType);
        return BRE_FAIL;
    }

    if (blended != NULL)
        *blended = is_blended;

    return BRE_OK;
}

br_error VIDEOI_BrPixelmapToExistingTexture(GLuint tex, br_pixelmap* pm) {
    GLint internalFormat;
    GLenum format;
    GLenum type;
    GLsizeiptr elemBytes;
    br_error r;

    r = VIDEOI_BrPixelmapGetTypeDetails(pm->type, &internalFormat, &format, &type, &elemBytes, NULL);
    if (r != BRE_OK)
        return r;

    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, pm->width, pm->height, 0, format, type, pm->pixels);

    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);

    GL_CHECK_ERROR();
    return BRE_OK;
}

GLuint VIDEO_BrPixelmapToGLTexture(br_pixelmap* pm) {
    if (pm == NULL)
        return 0;

    GLuint tex;
    glGenTextures(1, &tex);

    if (VIDEOI_BrPixelmapToExistingTexture(tex, pm) != BRE_OK)
        return 0;

    return tex;
}

void VIDEOI_BrRectToGL(const br_pixelmap* pm, br_rectangle* r) {
    br_rectangle out;
    PixelmapRectangleClip(&out, r, pm);

    /* Flip the rect upside down to use (0, 0) at bottom-left. */
    *r = out;
    r->y = pm->height - r->h - r->y;
}

void VIDEOI_BrRectToUVs(const br_pixelmap* pm, const br_rectangle* r, float* x0, float* y0, float* x1, float* y1) {
    *x0 = (float)r->x / (float)pm->width;
    *y0 = (float)r->y / (float)pm->height;

    *x1 = (float)(r->x + r->w) / (float)pm->width;
    *y1 = (float)(r->y + r->h) / (float)pm->height;
}

br_matrix4* VIDEOI_D3DtoGLProjection(br_matrix4* m) {
    // Change the signs
    // https://cv4mar.blogspot.com.au/2009/03/transformation-matrices-between-opengl.html
    m->m[0][2] = -m->m[0][2];
    m->m[1][2] = -m->m[1][2];
    m->m[2][2] = -m->m[2][2];
    m->m[3][2] = -m->m[3][2];
    return m;
}
