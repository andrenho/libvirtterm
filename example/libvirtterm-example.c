#define _XOPEN_SOURCE 600
#include <pty.h>
#include <unistd.h>
#include <fcntl.h>
#include <utmp.h>

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL_assert.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "../libvirtterm.h"
#include "colors.h"

// #define DEBUG 1

/* We will use this ren to draw into this window every frame. */
static SDL_Window   *window = NULL;
static SDL_Renderer *ren = NULL;
static SDL_AudioStream *stream = NULL;
static Uint8 *wav_data = NULL;
static Uint32 wav_data_len = 0;
static SDL_Texture  *font = NULL;
static VT           *vt = NULL;
static int          master_pty = -1;
static bool         play_beep = false;

#define FONT_W 8
#define FONT_H 15
#define ZOOM 1.f
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

    //
    // initialize SDL
    //
    SDL_assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO));
    SDL_assert(SDL_CreateWindowAndRenderer("libvirtterm example", 1024, 768, SDL_WINDOW_RESIZABLE, &window, &ren));
    SDL_StartTextInput(window);

    //
    // initialize font
    //
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

    //
    // initialize audio (for beep)
    //
    SDL_AudioSpec spec;
    char* wav_path;
    SDL_asprintf(&wav_path, "%sbeep.wav", SDL_GetBasePath());
    if (!SDL_LoadWAV(wav_path, &spec, &wav_data, &wav_data_len)) {
        SDL_Log("Couldn't load .wav file: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!stream) {
        SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_ResumeAudioStreamDevice(stream);

    //
    // initialize libvirtterm
    //
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    VTConfig config = VT_DEFAULT_CONFIG;
    vt = vt_new((h - BORDER*2) / FONT_H / ZOOM, (w - BORDER*2) / FONT_W / ZOOM, vt_callback, &config, NULL);

    //
    // open shell
    //
    char pty_name[1024];
    struct winsize ws = { vt->rows, vt->columns, w - (BORDER * 2), h - (BORDER * 2) };
    pid_t pid = forkpty(&master_pty, pty_name, NULL, &ws);
    if (pid == 0) {
        setenv("TERM", "xterm", 1);
        char *shell_path = getenv("SHELL");
        if (shell_path)
            execl(shell_path, shell_path, NULL);
        else
            execl("/bin/sh", "sh", NULL);
        perror("execl");
        exit(1);
    }
    int flags = fcntl(master_pty, F_GETFL, 0);
    fcntl(master_pty, F_SETFL, flags | O_NONBLOCK);

    printf("Shell session started at %s.\n", pty_name);

    return SDL_APP_CONTINUE;
}


static uint16_t translate_key(SDL_Keycode key)
{
    switch (key) {
        case SDLK_F1:                     return VT_F1;
        case SDLK_F2:                     return VT_F2;
        case SDLK_F3:                     return VT_F3;
        case SDLK_F4:                     return VT_F4;
        case SDLK_F5:                     return VT_F5;
        case SDLK_F6:                     return VT_F6;
        case SDLK_F7:                     return VT_F7;
        case SDLK_F8:                     return VT_F8;
        case SDLK_F9:                     return VT_F9;
        case SDLK_F10:                    return VT_F10;
        case SDLK_F11:                    return VT_F11;
        case SDLK_F12:                    return VT_F12;
        case SDLK_INSERT:                 return VT_INSERT;
        case SDLK_HOME:                   return VT_HOME;
        case SDLK_PAGEUP:                 return VT_PAGE_UP;
        case SDLK_END:                    return VT_END;
        case SDLK_PAGEDOWN:               return VT_PAGE_DOWN;
        case SDLK_RIGHT:                  return VT_ARROW_RIGHT;
        case SDLK_LEFT:                   return VT_ARROW_LEFT;
        case SDLK_DOWN:                   return VT_ARROW_DOWN;
        case SDLK_UP:                     return VT_ARROW_UP;
        case SDLK_BACKSPACE:              return VT_BACKSPACE;
        case SDLK_TAB:                    return VT_TAB;
        case SDLK_KP_ENTER:               return '\r';
        default:
            return key < 0xff ? key : 0;
    }
}


SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    (void) appstate;

    switch (event->type) {
        case SDL_EVENT_KEY_DOWN: {
            SDL_Keycode keycode = SDL_GetKeyFromScancode(event->key.scancode, event->key.mod, false);
            uint16_t key = translate_key(keycode);
            char buf[16];
            int n = vt_translate_key(vt, key, event->key.mod & SDL_KMOD_SHIFT, event->key.mod & SDL_KMOD_CTRL, buf, sizeof buf);
            if (buf[0] != 0) {
#ifdef DEBUG
                for (int i = 0; i < n; ++i)
                    printf("< %c    %d 0x%02X\n", buf[i], buf[i], buf[i]);
                printf("------\n");
#endif
                if (write(master_pty, buf, n) == 0)
                    exit(0);
            }
            break;
        }
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}


static void draw_char(size_t row, size_t column)
{
    VTChar chr = vt_char(vt, row, column);
    SDL_FRect origin = { chr.ch % 32 * FONT_W, chr.ch / 32 * FONT_H, FONT_W, FONT_H };
    SDL_FRect dest = { column * FONT_W * ZOOM + BORDER, row * FONT_H * ZOOM + BORDER, FONT_W * ZOOM, FONT_H * ZOOM };

    // draw bg
    SDL_Color bg = terminal_colors[chr.attrib.bg_color];
    SDL_SetRenderDrawColor(ren, bg.r, bg.g, bg.b, SDL_ALPHA_OPAQUE);
    SDL_RenderFillRect(ren, &dest);

    // draw character
    if (chr.ch != 0 && chr.ch != ' ') {
        SDL_Color fg = terminal_colors[chr.attrib.fg_color];
        if (chr.attrib.dim) { fg.r *= 0.6; fg.g *= 0.6; fg.b *= 0.6; }
        SDL_SetTextureColorMod(font, fg.r, fg.g, fg.b);
        SDL_RenderTexture(ren, font, &origin, &dest);

        if (chr.attrib.bold) {
            dest.x += 1;
            SDL_RenderTexture(ren, font, &origin, &dest);
        }

        if (chr.attrib.underline) {
            SDL_FRect r = { dest.x, dest.y + ((FONT_H - 2) * ZOOM), dest.w, ZOOM };
            SDL_SetRenderDrawColor(ren, fg.r, fg.g, fg.b, SDL_ALPHA_OPAQUE);
            SDL_RenderFillRect(ren, &r);
        }
    }
}


SDL_AppResult SDL_AppIterate(void *appstate)
{
    (void) appstate;

    //
    // continue playing beep
    //
    if (play_beep) {
        while (SDL_GetAudioStreamQueued(stream) < (int)wav_data_len) {
            SDL_PutAudioStreamData(stream, wav_data, wav_data_len);
        }
        play_beep = false;
    }

    //
    // process PTY
    //
    char buf[256];
    int n = read(master_pty, buf, sizeof(buf));
    if (n == 0) {
        exit(0);
    } else if (n > 0) {
#ifdef DEBUG
        for (int i = 0; i < n; ++i)
            printf("> %c    %d 0x%02X\n", buf[i], buf[i], buf[i]);
        printf("------\n");
#endif
        vt_write(vt, buf, n);
    }

    //
    // render screen
    //
    SDL_SetRenderDrawColor(ren, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(ren);

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

