#include <inttypes.h>
#include <brender.h>
#include <brddi.h>
#include <priminfo.h>
#include <brsdl2dev.h>
#include <stdio.h>
#include <SDL.h>
#include <assert.h>

#define SOFTCUBE_16BIT 1

void BR_CALLBACK _BrBeginHook(void)
{
    struct br_device *BR_EXPORT BrDrv1SoftPrimBegin(char *arguments);
    struct br_device *BR_EXPORT BrDrv1SoftRendBegin(char *arguments);

    BrDevAddStatic(NULL, BrDrv1SoftPrimBegin, NULL);
    BrDevAddStatic(NULL, BrDrv1SoftRendBegin, NULL);
    //BrDevAddStatic(NULL, BrDrv1SDL2Begin, NULL);
}

void BR_CALLBACK _BrEndHook(void)
{
}

static char primitive_heap[1500 * 1024];

static void draw_info(br_pixelmap *screen, br_material *mat)
{

    br_uint_16 font_height = 0;
    brp_block *block;
}

int main(int argc, char **argv)
{
    br_pixelmap *screen = NULL, *colour_buffer = NULL, *depth_buffer = NULL;
    br_actor    *world, *camera, *cube, *cube2, *light;
    int          ret = 1;
    br_uint_64   ticks_last, ticks_now;
    br_colour    clear_colour;
    br_error     err;

    int load_from_file = 0;

    uint8_t file_buf[640 * 480];
    // file_buf[-1] = 0;
    int px_idx = 0;

    if(load_from_file) {
        FILE *f = fopen("/Users/jeff/Downloads/carma/out.txt", "r");

        while(1) {
            int res = fgetc(f);
            if(res == EOF) {
                break;
            }
            char c = (char)res;
            if(c != ' ' && c != '\x0d' && c != '\x0a') {
                file_buf[px_idx] = c - 48;
                px_idx++;
            }
        }
    }

    BrBegin();

    //BrLogSetLevel(BR_LOG_DEBUG);

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
	{
		printf("sdl_init panic! (%s)\n", SDL_GetError());
		return -1;
	}

	SDL_Window *window = SDL_CreateWindow(
		"brenSDL",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		640, 480,
		SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL)
    {
        printf("renderer panic! (%s)\n", SDL_GetError());
        return -1;
    }

    BrBegin();
    BrZbBegin(BR_PMT_INDEX_8, BR_PMT_DEPTH_16);

    //err = BrDevBegin(&screen, "SDL2");
    screen = BrPixelmapAllocate(BR_PMT_INDEX_8, 640, 480, NULL, BR_PMAF_NORMAL);

    if(err != BRE_OK) {
        //BrLogError("APP", "BrDevBeginVar() failed");
        goto create_fail;
    }

    {
#if defined(SOFTCUBE_16BIT)
        //BrLogInfo("APP", "Running at 16-bpp");
        colour_buffer = BrPixelmapMatchTyped(screen, BR_PMMATCH_OFFSCREEN, BR_PMT_INDEX_8);
        ;
        clear_colour = BR_COLOUR_565(66, 0, 66);

#else
        //BrLogInfo("APP", "Running at 24-bpp");
        colour_buffer = BrPixelmapMatchTyped(screen, BR_PMMATCH_OFFSCREEN, BR_PMT_RGB_888);
        ;
        clear_colour = BR_COLOUR_RGB(66, 66, 66);
#endif
    }

    if(colour_buffer == NULL) {
        //BrLogError("APP", "BrPixelmapAllocate() failed");
        goto create_fail;
    }

    if((depth_buffer = BrPixelmapMatch(colour_buffer, BR_PMMATCH_DEPTH_16)) == NULL) {
        //BrLogError("APP", "BrPixelmapMatch() failed");
        goto create_fail;
    }

    SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(colour_buffer->pixels, 640, 480, 8, 640, 0, 0, 0, 0);

    BrPixelmapFill(depth_buffer, 0xFFFFFFFF);

    colour_buffer->origin_x = depth_buffer->origin_x = colour_buffer->width >> 1;
    colour_buffer->origin_y = depth_buffer->origin_y = colour_buffer->height >> 1;

    br_pixelmap *pal_std;
    if((pal_std = BrPixelmapLoad("/opt/CARMA/DATA/REG/PALETTES/DRRENDER.PAL")) == NULL) {
        //Error("APP", "Error loading std.pal");
        goto create_fail;
    }

    SDL_Color *cols = pal_std->pixels;
    for (int i = 0; i < 256; i++)
    {
        int r = cols[i].r;
        cols[i].r = cols[i].b;
        cols[i].b = r;
    }

    //BrPixelmapPaletteSet(colour_buffer, pal_std);
    SDL_SetPaletteColors(surface->format->palette, pal_std->pixels, 0, 256);

    //BrRendererBegin(colour_buffer, NULL, NULL, primitive_heap, sizeof(primitive_heap));

    world = BrActorAllocate(BR_ACTOR_NONE, NULL);

    {
        br_camera *camera_data;

        camera         = BrActorAdd(world, BrActorAllocate(BR_ACTOR_CAMERA, NULL));
        camera->t.type = BR_TRANSFORM_MATRIX34;
        BrMatrix34Translate(&camera->t.t.mat, BR_SCALAR(0.0), BR_SCALAR(0.0), BR_SCALAR(1.5));

        camera_data           = (br_camera *)camera->type_data;
        camera_data->aspect   = BR_DIV(BR_SCALAR(colour_buffer->width), BR_SCALAR(colour_buffer->height));
        camera_data->hither_z = 0.1f;
    }

    BrModelFindHook(BrModelFindFailedLoad);
    BrMapFindHook(BrMapFindFailedLoad);
    BrMaterialFindHook(BrMaterialFindFailedLoad);

    br_pixelmap *pm2[1000];
    int          count = BrPixelmapLoadMany("/opt/CARMA/DATA/PIXELMAP/GASPUMP.PIX", pm2, 1000);
    BrMapAddMany(pm2, count);
    br_material *mat2[1000];
    count = BrMaterialLoadMany("/opt/CARMA/DATA/MATERIAL/GASPUMP.MAT", mat2, 1000);
    BrMaterialAddMany(mat2, count);
    for(int i = 0; i < count; i++) {
        mat2[i]->flags |= BR_MATF_PERSPECTIVE;
        mat2[i]->flags |= BR_MATF_DITHER;
        mat2[i]->flags |= BR_MATF_SMOOTH;
    }

    br_pixelmap *pm3[1000];
    count = BrPixelmapLoadMany("/opt/CARMA/DATA/PIXELMAP/SCREWIE.PIX", pm3, 1000);
    BrMapAddMany(pm3, count);
    br_material *mat3[1000];
    count = BrMaterialLoadMany("/opt/CARMA/DATA/MATERIAL/SCREWIE.MAT", mat3, 1000);
    BrMaterialAddMany(mat3, count);

    br_pixelmap *pm = BrPixelmapLoad("/Users/jeff/code/CrocDE-BRender/examples/dat/checkerboard8.pix");
    BrMapAdd(pm);

    br_model *mod1[1000];
    count = BrModelLoadMany("/opt/CARMA/DATA/MODELS/SCREWIE.DAT", mod1, 1000);
    BrModelAddMany(mod1, count);

cube = BrActorLoad("/opt/CARMA/DATA/ACTORS/SCREWIE.ACT");
    BrActorAdd(world, cube);
    //  cube           = BrActorAdd(world, BrActorAllocate(BR_ACTOR_MODEL, NULL));
    // cube->t.type = BR_TRANSFORM_MATRIX34;
    // cube->model    = BrModelFind("/Users/jeff/code/CrocDE-BRender/examples/dat/cube.dat");
    // BrModelUpdate(cube->model, BR_MODU_ALL);
    // cube->material = BrMaterialLoad("/Users/jeff/code/CrocDE-BRender/examples/dat/checkerboard8.mat");
    // BrMapUpdate(cube->material->colour_map, BR_MAPU_ALL);
    // BrMaterialUpdate(cube->material, BR_MATU_ALL);

//   cube2           = BrActorAdd(world, BrActorAllocate(BR_ACTOR_MODEL, NULL));
//     cube2->t.type = BR_TRANSFORM_MATRIX34;
//     cube2->model    = BrModelFind("/Users/jeff/code/CrocDE-BRender/examples/dat/cube.dat");
//     cube2->material = BrMaterialLoad("/Users/jeff/code/CrocDE-BRender/examples/dat/checkerboard8.mat");
//     BrMapUpdate(cube2->material->colour_map, BR_MAPU_ALL);
//     BrMaterialUpdate(cube2->material, BR_MATU_ALL);
    // cube->render_style = BR_RSTYLE_EDGES;


#if defined(SOFTCUBE_16BIT)

#else
    cube->material = BrMaterialLoad("checkerboard24.mat");
#endif

    // cube->material->flags |= BR_MATF_PERSPECTIVE; // Perspective-correct texture mapping. Doesn't actually work.
    // cube->material->flags |= BR_MATF_DITHER;      // Dithering.
    // cube->material->flags |= BR_MATF_SMOOTH; // Makes lighting look _much_ better.
    // cube->material->flags |= BR_MATF_DISABLE_COLOUR_KEY;  // Not supported by software.
    // cube->material->opacity = 255; // < 255 selects screendoor renderer
    // cube->render_style = BR_RSTYLE_EDGES;

//BrMatrix34Translate(&cube->t.t.mat, 0, 0.0, -30);
    BrMatrix34RotateX(&cube->t.t.mat, BR_ANGLE_DEG(-20));
    BrMatrix34PostRotateY(&cube->t.t.mat, BR_ANGLE_DEG(-100));
    //BrMatrix34RotateX(&cube2->t.t.mat, BR_ANGLE_DEG(-20));

    light = BrActorAdd(world, BrActorAllocate(BR_ACTOR_LIGHT, NULL));
    BrLightEnable(light);

    ticks_last = SDL_GetTicks64();

    Uint32 totalFrameTicks = 0;
    Uint32 totalFrames     = 0;

    for(SDL_Event evt;;) {
        float dt;

        ticks_now  = SDL_GetTicks64();
        dt         = (float)(ticks_now - ticks_last) / 1000.0f;
        ticks_last = ticks_now;

        while(SDL_PollEvent(&evt) > 0) {
            switch(evt.type) {
                case SDL_QUIT:
                    goto done;

                case SDL_KEYUP:
                    if (evt.key.keysym.sym == SDLK_a) {
                        BrMatrix34PostRotateY(&cube->t.t.mat, BR_ANGLE_DEG(BR_SCALAR(-3)));
                    } else if (evt.key.keysym.sym == SDLK_s) {
                        BrMatrix34PostRotateY(&cube->t.t.mat, BR_ANGLE_DEG(BR_SCALAR(3)));
                    }
                    break;

            }
        }

        totalFrames++;
        Uint32 startTicks = SDL_GetTicks();
        Uint64 startPerf  = SDL_GetPerformanceCounter();

        if(load_from_file) {

            colour_buffer->origin_x = 0;
            colour_buffer->origin_y = 0;
            for(int y = 0; y < 480; y++) {
                memcpy(colour_buffer->pixels + y * colour_buffer->row_bytes, &file_buf[y * 480], 640);
            }
            // memcpy(colour_buffer->pixels, file_buf, 640 * 480);

        } else {

           BrMatrix34PostRotateY(&cube->t.t.mat, BR_ANGLE_DEG(BR_SCALAR(60) * BR_SCALAR(dt)));

            BrRendererFrameBegin();
            BrPixelmapFill(colour_buffer, 10);
            BrPixelmapFill(depth_buffer, 0xFFFFFFFF);

            BrZbSceneRender(world, camera, colour_buffer, depth_buffer);

            BrRendererFrameEnd();
        }

        // End frame timing
        Uint32 endTicks  = SDL_GetTicks();
        Uint64 endPerf   = SDL_GetPerformanceCounter();
        Uint64 framePerf = endPerf - startPerf;
        float  frameTime = (endTicks - startTicks) / 1000.0f;
        totalFrameTicks += endTicks - startTicks;

        // Strings to display
        int fps = 1.0f / frameTime;
        int avg = 1000.0f / ((float)totalFrameTicks / totalFrames);

        // font_height = BrPixelmapTextHeight(screen, BrFontProp7x9);

        BrPixelmapTextF(colour_buffer, -320, -200, BR_COLOUR_RGBA(255, 255, 0, 255), BrFontProp7x9,
                        "Current FPS: %d, average: %d", fps, avg);

        if(totalFrames % 60 == 0) {
            printf("Current FPS: %d, average: %d\n", fps, avg);
        }

        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
        SDL_DestroyTexture(texture);

        //BrPixelmapDoubleBuffer(screen, colour_buffer);
    }

done:
    ret = 0;

    BrRendererEnd();

create_fail:

    if(depth_buffer != NULL)
        BrPixelmapFree(depth_buffer);

    if(colour_buffer != NULL)
        BrPixelmapFree(colour_buffer);

    BrEnd();

    return ret;
}
