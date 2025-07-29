#include <brddi.h>
#include <brender.h>
#include <inttypes.h>
#include <priminfo.h>
// #include <brsdl2dev.h>
#include <SDL.h>
#include <assert.h>
#include <stdio.h>


br_pixelmap *screen = NULL, *colour_buffer = NULL, *depth_buffer = NULL;
br_actor *world, *camera;
SDL_Window* window;
br_uint_64 ticks_last, ticks_now;

// software renderer
br_device_virtualfb_callback_procs virtualfb_callbacks;
SDL_Texture *screen_texture;
uint32_t converted_palette[256];
SDL_Renderer* renderer;

// gl renderer
br_device_gl_callback_procs gl_callbacks;
SDL_GLContext* gl_context;

const int width = 640;
const int height = 480;

void BR_CALLBACK _BrBeginHook(void) {
    struct br_device* BR_EXPORT BrDrv1SoftPrimBegin(char* arguments);
    struct br_device* BR_EXPORT BrDrv1SoftRendBegin(char* arguments);
    struct br_device* BR_EXPORT BrDrv1VirtualFramebufferBegin(char* arguments);
    struct br_device* BR_EXPORT BrDrv1GLBegin(char* arguments);

    BrDevAddStatic(NULL, BrDrv1SoftPrimBegin, NULL);
    BrDevAddStatic(NULL, BrDrv1SoftRendBegin, NULL);
    BrDevAddStatic(NULL, BrDrv1VirtualFramebufferBegin, NULL);
    BrDevAddStatic(NULL, BrDrv1GLBegin, NULL);
}

void BR_CALLBACK _BrEndHook(void) {
}

static void software_palette_changed(br_colour entries[256]) {
    for (int i = 0; i < 256; i++) {
        converted_palette[i] = (0xff << 24 | BR_RED(entries[i]) << 16 | BR_GRN(entries[i]) << 8 | BR_BLU(entries[i]));
    }
}

static void software_renderer_swap(br_pixelmap* back_buffer) {
    uint8_t* src_pixels = back_buffer->pixels;
    uint32_t* dest_pixels;
    int dest_pitch;

    SDL_LockTexture(screen_texture, NULL, (void**)&dest_pixels, &dest_pitch);
    for (int i = 0; i < back_buffer->height * back_buffer->width; i++) {
        *dest_pixels = converted_palette[*src_pixels];
        dest_pixels++;
        src_pixels++;
    }
    SDL_UnlockTexture(screen_texture);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, screen_texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}


static void gl_renderer_swap(br_pixelmap* back_buffer) {
    SDL_GL_SwapWindow(window);
}

static void gl_get_viewport(int* x, int* y, float* width_m, float* height_m) {
    int window_width, window_height;
    int vp_width, vp_height;
    SDL_GetWindowSize(window, &window_width, &window_height);

    *x = 0;
    *y = 0;
    *width_m = 1; //window_width;
    *height_m = 1; //window_height;
}


static int init_software_renderer() {
    window = SDL_CreateWindow(
        "BRender v1.3.2 software renderer",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    virtualfb_callbacks.palette_changed = software_palette_changed;
    virtualfb_callbacks.swap_buffers = software_renderer_swap;

    BrDevBeginVar(&screen, "virtualframebuffer",
        BRT_WIDTH_I32, width,
        BRT_HEIGHT_I32, height,
        BRT_VIRTUALFB_CALLBACKS_P, &virtualfb_callbacks,
        BR_NULL_TOKEN);

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        printf("Failed to create SDL renderer: %s\n", SDL_GetError());
        exit(1);
    }
    screen_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
    return 0;
}

static int init_opengl_renderer() {
    window = SDL_CreateWindow(
        "BRender v1.3.2 gl renderer",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    gl_context = SDL_GL_CreateContext(window);

    if (gl_context == NULL) {
        printf("Failed to create OpenGL core profile: %s. Trying OpenGLES...\n", SDL_GetError());
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        gl_context = SDL_GL_CreateContext(window);
    }
    if (gl_context == NULL) {
        printf("Failed to create OpenGL context: %s\n", SDL_GetError());
        exit(1);
    }
    SDL_GL_SetSwapInterval(1);

    gl_callbacks.get_proc_address = SDL_GL_GetProcAddress;
    gl_callbacks.swap_buffers = gl_renderer_swap;
    gl_callbacks.get_viewport = gl_get_viewport;
    BrDevBeginVar(&screen, "glrend",
        BRT_WIDTH_I32, width,
        BRT_HEIGHT_I32, height,
        BRT_OPENGL_CALLBACKS_P, &gl_callbacks,
        BRT_PIXEL_TYPE_U8, BR_PMT_RGB_565,
        BR_NULL_TOKEN);

    if (screen == NULL) {
        printf("Failed to create driver: %s\n", SDL_GetError());
        exit(1);
    }
    return 0;
}

int main(int argc, char** argv) {


    BrBegin();

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("sdl_init panic! (%s)\n", SDL_GetError());
        return -1;
    }

    // init_software_renderer();
    init_opengl_renderer();

    colour_buffer = BrPixelmapMatch(screen, BR_PMMATCH_OFFSCREEN);
    depth_buffer = BrPixelmapMatch(colour_buffer, BR_PMMATCH_DEPTH_16);
    if (depth_buffer == NULL || colour_buffer == NULL) {
        printf("Failed to create colour_buffer or depth_buffer\n");
        exit(1);
    }

    colour_buffer->origin_x = depth_buffer->origin_x = colour_buffer->width >> 1;
    colour_buffer->origin_y = depth_buffer->origin_y = colour_buffer->height >> 1;

    BrZbBegin(colour_buffer->type, depth_buffer->type);


    world = BrActorAllocate(BR_ACTOR_NONE, NULL);

    // add camera
    br_camera* camera_data;
    camera = BrActorAdd(world, BrActorAllocate(BR_ACTOR_CAMERA, NULL));
    camera->t.type = BR_TRANSFORM_MATRIX34;
    BrMatrix34Translate(&camera->t.t.mat, BR_SCALAR(-0.0f), BR_SCALAR(5.3f), BR_SCALAR(50.5));
    camera_data = (br_camera*)camera->type_data;
    camera_data->type = BR_CAMERA_PERSPECTIVE_FOV;
    camera_data->field_of_view = BrDegreeToAngle(55.55f);
    camera_data->aspect = BR_DIV(BR_SCALAR(colour_buffer->width), BR_SCALAR(colour_buffer->height));
    camera_data->hither_z = 0.1f;
    camera_data->yon_z = 300;

    // load palette
    br_pixelmap* pal_std = BrPixelmapLoad("../dat/gamelet.pal");
    BrDevPaletteSetOld(pal_std);

    // load scene
    br_pixelmap *pixmaps[100];
    int pixmap_count = BrPixelmapLoadMany("../dat/miniskin.pix", pixmaps, 100);
    BrMapAddMany(pixmaps, pixmap_count);
    br_material *materials[100];
    int material_count = BrMaterialLoadMany("../dat/minibod.mat", materials, 100);
    for (int i = 0; i < material_count; i++) {
        materials[i]->flags |= BR_MATF_PERSPECTIVE;
        materials[i]->flags &= ~BR_MATF_LIGHT;
        materials[i]->flags &= ~BR_MATF_PRELIT;
    }
    BrMaterialAddMany(materials, material_count);
    br_model *models[100];
    int model_count = BrModelLoadMany("../dat/minibody.dat", models, 100);
    BrModelAddMany(models, model_count);

    br_actor *mini = BrActorLoad("../dat/minibody.act");

    BrActorAdd(world, mini);

    ticks_last = SDL_GetTicks64();

    for (SDL_Event evt;;) {
        float dt;

        ticks_now = SDL_GetTicks64();
        dt = (float)(ticks_now - ticks_last) / 1000.0f;
        ticks_last = ticks_now;

        while (SDL_PollEvent(&evt) > 0) {
            switch (evt.type) {
            case SDL_QUIT:
                exit(0);
            }
        }

        BrMatrix34PreRotateY(&mini->t.t.mat, BR_ANGLE_DEG(BR_SCALAR(60) * BR_SCALAR(dt)));

        BrPixelmapFill(colour_buffer, 0);
        BrPixelmapFill(depth_buffer, 0xFFFFFFFF);

        BrZbSceneRender(world, camera, colour_buffer, depth_buffer);
        BrPixelmapDoubleBuffer(screen, colour_buffer);
    }
}
