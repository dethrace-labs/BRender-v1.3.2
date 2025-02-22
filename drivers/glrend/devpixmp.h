/*
 * Private device pixelmap structure
 */
#ifndef _DEVPIXMP_H_
#define _DEVPIXMP_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float x, y, z;
    float r, g, b;
    float u, v;
} br_device_pixelmap_gl_tri;

typedef struct br_device_pixelmap_gl_quad {
    br_device_pixelmap_gl_tri tris[4];
    GLuint defaultVao;
    GLuint textVao;
    GLuint buffers[2];
} br_device_pixelmap_gl_quad;

#ifdef BR_DEVICE_PIXELMAP_PRIVATE

/*
 * Private state of device pixelmap
 */
typedef struct br_device_pixelmap {

    /*
     * Dispatch table
     */
    const struct br_device_pixelmap_dispatch* dispatch;

    /*
     * Standard handle identifier
     */
    const char* pm_identifier;

    /** Standard pixelmap members (not including identifier) **/

    BR_PIXELMAP_MEMBERS

    /** End of br_pixelmap fields **/

    struct br_device* device;
    struct br_output_facility* output_facility;

    /*
     * Type of buffer (when matched)
     */
    br_token use_type;

    /*
     * No. MSAA samples
     */
    br_int_32 msaa_samples;

    /*
     * Pointer to renderer currently opened on this pixelmap (N.B. This is only set on the screen
     * pixelmap)
     */
    struct br_renderer* renderer;

    /*
     * Current screen pixelmap. Valid on ALL types. The screen points to itself.
     * NB: This is mainly used to retrieve the context-level OpenGL state.
     */
    struct br_device_pixelmap* screen;

    br_uint_16 parent_height;
    br_boolean sub_pixelmap;

    /* OpenGL crap */
    union {
        struct {
            /*
             * System-specific OpenGL function pointers.
             */
            br_device_gl_callback_procs callbacks;

            /*
             * Device-wide VIDEO instance.
             */
            VIDEO video;

            /*
             * OpenGL context
             */
            void* gl_context;

            const char* gl_version;
            const char* gl_vendor;
            const char* gl_renderer;

            GLint gl_num_extensions;
            char** gl_extensions;

            GLuint tex_white;
            GLuint tex_checkerboard;

            GLuint screen_buffer_vao, screen_buffer_ebo;

            br_int_32 num_refs;
        } asFront;
        struct {
            struct br_device_pixelmap* depthbuffer;
            GLuint glFbo;
            GLuint glTex;
            GLfloat clearColour[4];

            // This is to emulate the ability to lock the backbuffer and write to it as if it
            // is in main memory
            // Instead we create a separate writable texture and overlay it on top when double buffering
            void *lockedPixels;
            GLuint overlayTexture;
            int possiblyDirty;
            int locked;

        } asBack;
        struct {
            struct br_device_pixelmap* backbuffer;
            GLuint glDepth;
            GLfloat clearValue;
        } asDepth;
    };
    struct br_device_clut* clut;
} br_device_pixelmap;

void RenderFullScreenTextureToFrameBuffer(br_device_pixelmap* self, GLuint textureId, GLuint fb, int flipVertically, int discardPurplePixels);

#endif

#ifdef __cplusplus
};
#endif
#endif
