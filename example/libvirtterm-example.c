#include "../libvirtterm_pty.h"

#include <unistd.h>
#include <fcntl.h>
#include <utmp.h>

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL_assert.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "../libvirtterm.h"
#include "colors.h"

// #define DEBUG 1

/* We will use this ren to draw into this window every frame. */
static SDL_Window*      window = NULL;
static SDL_Renderer*    ren = NULL;
static SDL_AudioStream* stream = NULL;
static Uint8*           wav_data = NULL;
static Uint32           wav_data_len = 0;
static VT*              vt;
static VTPTY*           vtpty;
static SDL_Texture*     font;

#define FONT_W 8
#define FONT_H 15
#define ZOOM 1.f
#define BORDER 16
#define INPUT_BUFFER_SIZE (16*1024)

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
    SDL_asprintf(&wav_path, "%sbeep.wav", SDL_GetBasePath());
    SDL_asprintf(&wav_path, "%sbeep.wav", SDL_GetBasePath());
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
    config.debug = VT_DEBUG_ERRORS_ONLY;
    config.acs_chars[0x00] = 0x4;   // diamond
    config.acs_chars[0x01] = 0xb0;  // checkerbox
    config.acs_chars[0x06] = 0xf8;  // degree
    config.acs_chars[0x07] = 0xf1;  // plus-minus
    config.acs_chars[0x0a] = 0xd9;  // bottom right corner
    config.acs_chars[0x0b] = 0xbf;  // top right corner
    config.acs_chars[0x0c] = 0xda;  // top left corner
    config.acs_chars[0x0d] = 0xc0;  // bottom left corner
    config.acs_chars[0x0e] = 0xc5;  // cross
    config.acs_chars[0x0f] = 0xdf;  // upper line
    config.acs_chars[0x10] = 0xc4;  // middle to upper line
    config.acs_chars[0x11] = 0xc4;  // middle line
    config.acs_chars[0x12] = 0xc4;  // middle to lower line
    config.acs_chars[0x13] = '_';   // lower line
    config.acs_chars[0x14] = 0xc3;  // right tee
    config.acs_chars[0x15] = 0xb4;  // left tee
    config.acs_chars[0x16] = 0xc1;  // up tee
    config.acs_chars[0x17] = 0xc2;  // down tee
    config.acs_chars[0x18] = 0xb3;  // vertical bar
    config.acs_chars[0x19] = 0xf3;  // less than
    config.acs_chars[0x1a] = 0xf2;  // more than
    config.acs_chars[0x1b] = 0xe3;  // pi
    config.acs_chars[0x1c] = 0xf0;  // not equals
    config.acs_chars[0x1d] = 0x9c;  // pound sterling
    config.acs_chars[0x1e] = 0xfa;  // center dot
    vt = vt_new((h - BORDER*2) / FONT_H / ZOOM, (w - BORDER*2) / FONT_W / ZOOM, &config);

    //
    // initialize VTPTY
    //
    vtpty = vtpty_new(vt, INPUT_BUFFER_SIZE);
    printf("Shell session started at %s.\n", vtpty_name(vtpty));

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


static SDL_AppResult vtpty_do(VTPTYStatus status)
{
    switch (status) {
        case VTP_CLOSE:
            return SDL_APP_SUCCESS;
        case VTP_ERROR:
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "VTPTY Error", strerror(errno), window);
            return SDL_APP_FAILURE;
        default:
    }
    return SDL_APP_CONTINUE;
}


SDL_AppResult mouse_event(SDL_Event* event)
{
    VTMouseState mstate;

    float x, y;
    SDL_MouseButtonFlags bflags = SDL_GetMouseState(&x, &y);

    mstate.column = (x - BORDER) / FONT_W / ZOOM;
    mstate.row = (y - BORDER) / FONT_H / ZOOM;

    mstate.button[VTM_LEFT] = (bflags & SDL_BUTTON_LMASK) ? true : false;
    mstate.button[VTM_MIDDLE] = (bflags & SDL_BUTTON_MMASK) ? true : false;
    mstate.button[VTM_RIGHT] = (bflags & SDL_BUTTON_RMASK) ? true : false;
    mstate.button[VTM_SCROLL_DOWN] = event->type == SDL_EVENT_MOUSE_WHEEL && event->wheel.integer_y < 0;
    mstate.button[VTM_SCROLL_UP] = event->type == SDL_EVENT_MOUSE_WHEEL && event->wheel.integer_y > 0;

    SDL_Keymod kmod = SDL_GetModState();
    mstate.mod = 0;
    if (kmod & SDL_KMOD_SHIFT) mstate.mod |= VTM_SHIFT;
    if (kmod & SDL_KMOD_ALT) mstate.mod |= VTM_ALT;
    if (kmod & SDL_KMOD_CTRL) mstate.mod |= VTM_CTRL;

    return vtpty_do(vtpty_update_mouse_state(vtpty, mstate));
}


SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    (void) appstate;

    switch (event->type) {
        case SDL_EVENT_KEY_DOWN: {
            SDL_Keycode keycode = SDL_GetKeyFromScancode(event->key.scancode, event->key.mod, false);
            uint16_t key = translate_key(keycode);
            return vtpty_do(vtpty_keypress(vtpty, key, event->key.mod & SDL_KMOD_SHIFT, event->key.mod & SDL_KMOD_CTRL));
        }
        case SDL_EVENT_WINDOW_RESIZED: {
            int w = event->window.data1;
            int h = event->window.data2;
            vtpty_resize(vtpty, (h - BORDER*2) / FONT_H / ZOOM, (w - BORDER*2) / FONT_W / ZOOM);
            break;
        }
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_WHEEL:
            return mouse_event(event);
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}


static void draw_char(size_t row, size_t column)
{
    VTCell chr = vt_char(vt, row, column);
    SDL_FRect origin = { chr.ch % 32 * FONT_W, chr.ch / 32 * FONT_H, FONT_W, FONT_H };
    SDL_FRect dest = { column * FONT_W * ZOOM + BORDER, row * FONT_H * ZOOM + BORDER, FONT_W * ZOOM, FONT_H * ZOOM };

    // draw bg
    SDL_Color bg = terminal_colors[chr.attrib.bg_color];
    SDL_SetRenderDrawColor(ren, bg.r, bg.g, bg.b, SDL_ALPHA_OPAQUE);
    SDL_RenderFillRect(ren, &dest);

    // draw character
    if (chr.ch != 0) {
        SDL_Color fg = terminal_colors[chr.attrib.fg_color];
        if (chr.attrib.dim) { fg.r *= 0.6; fg.g *= 0.6; fg.b *= 0.6; }
        SDL_SetTextureColorMod(font, fg.r, fg.g, fg.b);
        SDL_RenderTexture(ren, font, &origin, &dest);

        /*
        if (chr.attrib.bold) {
            dest.x += 1;
            SDL_RenderTexture(ren, font, &origin, &dest);
        }
        */

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
    // process VT
    //
    VTEvent e;
    while (vt_next_event(vt, &e)) {
        if (e.type == VT_EVENT_BELL) {
            // play beep
            while (SDL_GetAudioStreamQueued(stream) < (int)wav_data_len)
                SDL_PutAudioStreamData(stream, wav_data, wav_data_len);
        } else if (e.type == VT_EVENT_TEXT_RECEIVED) {
            if (e.text_received.type == VTT_WINDOW_TITLE_UPDATED)
                SDL_SetWindowTitle(window, e.text_received.text);
            free((void *) e.text_received.text);
        }
    }

    //
    // process PTY
    //
    SDL_AppResult r = vtpty_do(vtpty_step(vtpty));

    //
    // render screen
    //
    SDL_SetRenderDrawColor(ren, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(ren);

    for (size_t row = 0; row < vt_rows(vt); ++row) {
        for (size_t column = 0; column < vt_columns(vt); ++column) {
            draw_char(row, column);
        }
    }

    SDL_RenderPresent(ren);
    return r;
}


void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void) appstate;
    (void) result;
}

