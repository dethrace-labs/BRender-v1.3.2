#include <inttypes.h>
#include <brender.h>
#include <brddi.h>
#include <priminfo.h>
//#include <brsdl2dev.h>
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


int main(int argc, char **argv)
{
    br_pixelmap *screen = NULL, *colour_buffer = NULL, *depth_buffer = NULL;
    br_actor    *world, *camera, *cube, *cube2, *light;
    int          ret = 1;
    br_uint_64   ticks_last, ticks_now;
    br_colour    clear_colour;
    br_error     err;

    BrBegin();

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
	{
		printf("sdl_init panic! (%s)\n", SDL_GetError());
		return -1;
	}

	SDL_Window *window = SDL_CreateWindow(
		"BRender v1.3.2 software renderer",
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
        BrMatrix34Translate(&camera->t.t.mat, BR_SCALAR(0.0), BR_SCALAR(0), BR_SCALAR(1.4));
        // BrMatrix34Translate(&camera->t.t.mat, BR_SCALAR(0.0), BR_SCALAR(0.5), BR_SCALAR(1));

        camera_data           = (br_camera *)camera->type_data;
        camera_data->aspect   = BR_DIV(BR_SCALAR(colour_buffer->width), BR_SCALAR(colour_buffer->height));
        camera_data->hither_z = 0.1f;
    }

    BrModelFindHook(BrModelFindFailedLoad);
    BrMapFindHook(BrMapFindFailedLoad);
    BrMaterialFindHook(BrMaterialFindFailedLoad);

    char* pm_names[] = {
        "/opt/CARMA/DATA/PIXELMAP/GASPUMP.PIX",
        "/opt/CARMA/DATA/PIXELMAP/TRAFCLIT.PIX",
        "/opt/CARMA/DATA/PIXELMAP/EAGREDL.PIX",
        "/opt/CARMA/DATA/PIXELMAP/SCREWIE.PIX",
        "/opt/CARMA/DATA/PIXELMAP/TASSLE.PIX",
    };
    char* mat_names[] = {
        "/opt/CARMA/DATA/MATERIAL/GASPUMP.MAT",
        "/opt/CARMA/DATA/MATERIAL/TRFCLITE.MAT",
        "/opt/CARMA/DATA/MATERIAL/EAGLE.MAT",
        "/opt/CARMA/DATA/MATERIAL/SCREWIE.MAT",
        "/opt/CARMA/DATA/MATERIAL/TASSLE.MAT",
        "/opt/CARMA/DATA/REG/MATERIAL/SIMPMAT.MAT"
    };
    char *mdl_names[] = {
        "/opt/CARMA/DATA/MODELS/&00GAS.DAT",
        "/opt/CARMA/DATA/MODELS/&03TRAFF.DAT",
        "/opt/CARMA/DATA/MODELS/EAGLE.DAT",
        "/opt/CARMA/DATA/MODELS/EAGLEX.DAT",
        "/opt/CARMA/DATA/MODELS/SCREWIE.DAT",
        "/opt/CARMA/DATA/MODELS/TASSLE.DAT",
    };

    br_pixelmap *fog = BrPixelmapLoad("/opt/CARMA/DATA/SHADETAB/STAAAAAA.TAB");
    BrMapAdd(fog);

    for (int i = 0; i < sizeof(pm_names) / sizeof(char*); i++) {
        br_pixelmap *tmp[1000];
        int          count = BrPixelmapLoadMany(pm_names[i], tmp, 1000);
        BrMapAddMany(tmp, count);
    }
    for (int i = 0; i < sizeof(mat_names) / sizeof(char*); i++) {
        br_material *tmp[1000];
        int          count = BrMaterialLoadMany(mat_names[i], tmp, 1000);
        for (int j = 0; j < count; j++) {
            if (tmp[j]->colour_map != NULL) {
                // tmp[j]->flags |= BR_MATF_PRELIT | BR_MATF_SMOOTH;
                // tmp[j]->flags &= ~BR_MATF_LIGHT;
                // tmp[j]->index_shade = fog;
                // tmp[j]->ka = 0.5f;
                //BrMaterialUpdate(tmp[j], BR_MATU_ALL);
            }
        }
        int loaded  = BrMaterialAddMany(tmp, count);
    }

    for (int i = 0; i < sizeof(mdl_names) / sizeof(char*); i++) {
        br_model *tmp[1000];
        int          count = BrModelLoadMany(mdl_names[i], tmp, 1000);
        for (int j = 0; j < count; j++) {
            for (int k = 0; k < tmp[j]->nvertices; k++) {
                tmp[j]->vertices[k].index = 0;
            }
        }
        BrModelAddMany(tmp, count);
    }

    // cube = BrActorLoad("/opt/DBG/DATA/ACTORS/&00GAS.ACT");
    // cube = BrActorLoad("/opt/DBG/DATA/ACTORS/&03TRAFF.ACT");
    // cube = BrActorLoad("/opt/DBG/DATA/ACTORS/EAGLE.ACT");
    // cube = BrActorLoad("/opt/DBG/DATA/ACTORS/SCREWIE.ACT");
    cube = BrActorLoad("/opt/CARMA/DATA/ACTORS/EAGLE.ACT");

    //cube2 = BrActorLoad("/opt/DBG/DATA/ACTORS/&00GAS.ACT");
    //BrMatrix34Translate(&cube2->t.t.mat, 0, 0.3, 0.2);
    //cube->render_style = BR_RSTYLE_EDGES;

    // only wheel
    // cube->children[0].prev = NULL;
    // cube->children[0].parent = NULL;
    // BrActorAdd(world, &cube->children[0]);

    // cube->children = NULL;
    BrActorAdd(world, cube);
    //BrActorAdd(world, cube2);



    BrMatrix34RotateX(&cube->t.t.mat, BR_ANGLE_DEG(-20));
    BrMatrix34PostRotateY(&cube->t.t.mat, BR_ANGLE_DEG(150));
    //BrMatrix34RotateX(&cube2->t.t.mat, BR_ANGLE_DEG(-20));

    light = BrActorAdd(world, BrActorAllocate(BR_ACTOR_LIGHT, NULL));
    BrLightEnable(light);

    ticks_last = SDL_GetTicks64();

    Uint32 totalFrameTicks = 0;
    Uint32 totalFrames     = 0;
    int show_depth_buffer = 0;

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
                    if (evt.key.keysym.sym == SDLK_d) {
                        show_depth_buffer = !show_depth_buffer;
                    }
                    break;

            }
        }

        totalFrames++;
        Uint32 startTicks = SDL_GetTicks();
        Uint64 startPerf  = SDL_GetPerformanceCounter();

        //BrMatrix34PostRotateY(&cube->t.t.mat, BR_ANGLE_DEG(BR_SCALAR(60) * BR_SCALAR(dt)));

        BrRendererFrameBegin();
        BrPixelmapFill(colour_buffer, 0);
        BrPixelmapFill(depth_buffer, 0xFFFFFFFF);

        BrZbSceneRender(world, camera, colour_buffer, depth_buffer);

        BrRendererFrameEnd();


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
        if (show_depth_buffer) {
            uint16_t *dp = depth_buffer->pixels;
            uint8_t *p = colour_buffer->pixels;
            for (int y = 0; y < 480; y++) {
                for (int x = 0; x < 640; x++) {
                    if (*dp != 0xFFFF) {
                        *p = 255;
                    }
                    dp++;
                    p++;
                }
            }
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
