#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL_assert.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "../libvirtterm.h"
#include "colors.h"

/* We will use this ren to draw into this window every frame. */
static SDL_Window   *window = NULL;
static SDL_Renderer *ren = NULL;
static SDL_Texture  *font = NULL;
static VT           *vt = NULL;

#define FONT_W 8
#define FONT_H 15
#define ZOOM 2.f
#define BORDER 16

void vt_callback(VT* vt, VTEvent* e)
{
    (void) vt;
    (void) e;
}


SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    (void) appstate;
    (void) argc;
    (void) argv;

    SDL_assert(SDL_Init(SDL_INIT_VIDEO));
    SDL_assert(SDL_CreateWindowAndRenderer("libvirtterm example", 640, 480, SDL_WINDOW_RESIZABLE, &window, &ren));

    char* bmp_path;
    SDL_asprintf(&bmp_path, "%stoshiba.bmp", SDL_GetBasePath());
    SDL_Surface* font_sf = SDL_LoadBMP(bmp_path);
    if (!font_sf) {
        SDL_Log("Couldn't load bitmap: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_free(bmp_path);

    SDL_SetSurfaceColorKey(font_sf, true, 0);

    font = SDL_CreateTextureFromSurface(ren, font_sf);
    SDL_assert(font);
    SDL_SetTextureScaleMode(font, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(font, SDL_BLENDMODE_BLEND);

    SDL_DestroySurface(font_sf);

    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    vt = vt_new((h - BORDER*2) / FONT_H / ZOOM, (w - BORDER*2) / FONT_W / ZOOM, vt_callback, NULL);

    return SDL_APP_CONTINUE;
}


SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    (void) appstate;

    switch (event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}


static void draw_char(size_t row, size_t column)
{
    VTChar chr = vt->matrix[row * vt->columns + column];
    SDL_FRect origin = { chr.ch % 32 * FONT_W, chr.ch / 32 * FONT_H, FONT_W, FONT_H };
    SDL_FRect dest = { column * FONT_W * ZOOM + BORDER, row * FONT_H * ZOOM + BORDER, FONT_W * ZOOM, FONT_H * ZOOM };

    // draw bg
    SDL_Color bg = terminal_colors[chr.attrib.bg_color];
    SDL_SetRenderDrawColor(ren, bg.r, bg.g, bg.b, SDL_ALPHA_OPAQUE);
    SDL_RenderFillRect(ren, &dest);

    // draw character
    if (chr.ch != 0 && chr.ch != ' ') {
        SDL_Color fg = terminal_colors[chr.attrib.fg_color];
        SDL_SetTextureColorMod(font, fg.r, fg.g, fg.b);
        SDL_RenderTexture(ren, font, &origin, &dest);
    }
}


SDL_AppResult SDL_AppIterate(void *appstate)
{
    (void) appstate;

    for (size_t row = 0; row < vt->rows; ++row) {
        for (size_t column = 0; column < vt->columns; ++column) {
            draw_char(row, column);
        }
    }

    SDL_RenderPresent(ren);
    return SDL_APP_CONTINUE;
}


void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void) appstate;
    (void) result;
}

