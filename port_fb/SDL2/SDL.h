// Minimal SDL stub for framebuffer port — satisfies includes without real SDL
#ifndef _FAKE_SDL_H
#define _FAKE_SDL_H

#include <stdint.h>
#include <cstdio>
#include <cstring>

typedef int32_t Sint16;
typedef uint8_t Uint8;
typedef uint32_t SDL_Keycode;
typedef uint32_t SDL_AudioDeviceID;

// Keycodes used in key.cpp and key_new.cpp
enum {
    SDLK_RIGHT = 1073741903, SDLK_LEFT = 1073741904,
    SDLK_DOWN = 1073741905, SDLK_UP = 1073741906,
    SDLK_RETURN = 13, SDLK_SPACE = 32, SDLK_ESCAPE = 27,
    SDLK_PERIOD = 46, SDLK_MINUS = 45, SDLK_EQUALS = 61,
    SDLK_LEFTBRACKET = 91, SDLK_RIGHTBRACKET = 93, SDLK_BACKSLASH = 92,
    SDLK_COMMA = 44, SDLK_SLASH = 47, SDLK_BACKSPACE = 8,
    SDLK_SEMICOLON = 59, SDLK_QUOTE = 39, SDLK_BACKQUOTE = 96,
    SDLK_TAB = 9,
    SDLK_LSHIFT = 1073742049, SDLK_RSHIFT = 1073742053,
    SDLK_LCTRL = 1073742048, SDLK_RCTRL = 1073742052,
    SDLK_LALT = 1073742050, SDLK_RALT = 1073742054,
    SDLK_0 = 48, SDLK_1, SDLK_2, SDLK_3, SDLK_4,
    SDLK_5, SDLK_6, SDLK_7, SDLK_8, SDLK_9,
    SDLK_a = 97, SDLK_b, SDLK_c, SDLK_d, SDLK_e, SDLK_f,
    SDLK_g, SDLK_h, SDLK_i, SDLK_j, SDLK_k, SDLK_l,
    SDLK_m, SDLK_n, SDLK_o, SDLK_p, SDLK_q, SDLK_r,
    SDLK_s, SDLK_t, SDLK_u, SDLK_v, SDLK_w, SDLK_x,
    SDLK_y, SDLK_z,
    SDLK_F1 = 1073741882, SDLK_F2, SDLK_F3, SDLK_F4,
    SDLK_F5, SDLK_F6, SDLK_F7, SDLK_F8, SDLK_F9,
    SDLK_F10, SDLK_F11, SDLK_F12,
};

// Stub functions
inline uint64_t SDL_GetTicks64() { return 0; }
inline void SDL_LockAudioDevice(SDL_AudioDeviceID d) {}
inline void SDL_UnlockAudioDevice(SDL_AudioDeviceID d) {}
inline void SDL_memset(void *p, int v, int n) { memset(p, v, n); }

// Window stubs: key_new.cpp touches SDL_SetWindowTitle on the fast-forward /
// pro-key toggles. We have no SDL window here — silently swallow.
struct SDL_Window;
inline void SDL_SetWindowTitle(SDL_Window *, const char *) {}

#endif
