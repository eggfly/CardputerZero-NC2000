/* port_lvgl/main_lvgl.cpp
 *
 * LVGL port for CardputerZero NC2000. Builds a dlopen'd shared library
 * implementing the cz_app.h ABI so the app loads into the desktop emulator
 * and, unchanged, into APPLaunch on the device.
 *
 * Core approach:
 *   - app_main(parent): allocate an lv_canvas sized at the scaled WQX display,
 *     spin up an LVGL timer that advances the NC2000 CPU + copies its lcd_buf
 *     into the canvas pixels.
 *   - Keyboard: the host emulator hands us every SDL keyboard event through
 *     lv_sdl_keyboard_handler. We translate them to NC2000's 8×8 key matrix
 *     via handle_key_wayback (the same path the SDL variant uses).
 */
#include <cz_app.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>

#include <SDL2/SDL.h>   /* stub in port_fb/SDL2/ — provides SDLK_* enums */

/* Upstream NC2000 core symbols (C++ headers). */
#include "comm.h"
#include "nc2000.h"
#include "state.h"
#include "display.h"
#include "settings.h"
#include "key_new.h"

extern nc2k_states_t nc2k_states;
extern uint32_t SLICE_INTERVAL;
extern uint32_t LCD_OUTER_REFRESH_INTERVAL;
extern bool nc2000mode;
extern WqxRom nc2k_rom;

/* Upstream globals port_fb provided (we take over their role). */
uint8_t lcd_buf[SCREEN_WIDTH * SCREEN_HEIGHT / 8 * 2];
unsigned char *lcd_effect_buffer = nullptr;
void init_lcd_stripe() { /* stub — no LCD stripe decoration in LVGL mode */ }

/* key_new.cpp references `extern SDL_Window *window` when the user toggles
 * fast-forward / pro-key (to update the window title). We have no SDL window
 * here; define the symbol to satisfy the linker — SDL_SetWindowTitle is a
 * no-op in the stubbed SDL.h. */
SDL_Window *window = nullptr;

/* ----- Palette ----- */
static const uint16_t palette_rgb565[4] = {
    0xD6F4, /* light green-white */
    0x9D11, /* mid-light */
    0x6B49, /* mid-dark */
    0x1861, /* near-black */
};

/* ----- Scaling ----- */
static const int SCALE = 2;
static const int WQX_W = SCREEN_WIDTH;   /* 160 */
static const int WQX_H = SCREEN_HEIGHT;  /* 80 */
static const int DISP_W = WQX_W * SCALE; /* 320 */
static const int DISP_H = WQX_H * SCALE; /* 160 */

/* ----- App state ----- */
static lv_obj_t  *g_canvas = NULL;
static uint16_t  *g_cbuf   = NULL;  /* WQX scaled buffer, RGB565 */
static lv_timer_t *g_timer = NULL;
static uint64_t   g_last_ns = 0;    /* CLOCK_MONOTONIC_RAW ns at previous slice */

/* Real wall-clock time in nanoseconds. We can't use lv_tick_get() here — the
 * host calls lv_tick_inc(5) per render loop, but vsync stretches each loop to
 * ~16ms on a 60Hz display, so LVGL's internal clock runs ~3x slower than the
 * wall. Using it to drive RunTimeSlice throttled the 5.12MHz NC2000 CPU down
 * to ~1.5MHz — the "slower than CM0" symptom. */
static inline uint64_t mono_ns() {
    struct timespec ts;
#if defined(CLOCK_MONOTONIC_RAW)
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void write_scaled_pixels(uint8_t *lcd)
{
    if (!g_cbuf) return;
    const bool grey = is_grey_mode();
    if (!grey) {
        /* 1 bit per pixel */
        for (int i = 0; i < WQX_W * WQX_H / 8; i++) {
            for (int j = 0; j < 8; j++) {
                bool pixel = (lcd[i] & (1 << (7 - j))) != 0;
                uint16_t color = pixel ? palette_rgb565[3] : palette_rgb565[0];
                int pos = i * 8 + j;
                int sy = pos / WQX_W;
                int sx = pos % WQX_W;
                int dx = sx * SCALE;
                int dyb = sy * SCALE;
                for (int py = 0; py < SCALE; py++) {
                    uint16_t *row = g_cbuf + (dyb + py) * DISP_W;
                    for (int px = 0; px < SCALE; px++) row[dx + px] = color;
                }
            }
        }
    } else {
        /* 2 bits per pixel */
        for (int i = 0; i < WQX_W * WQX_H / 8 * 2; i++) {
            for (int j = 0; j < 4; j++) {
                uint8_t v = (lcd[i] >> (6 - j * 2)) & 0x03;
                uint16_t color = palette_rgb565[v];
                int pos = (i * 8 + j * 2) / 2;
                int sy = pos / WQX_W;
                int sx = pos % WQX_W;
                int dx = sx * SCALE;
                int dyb = sy * SCALE;
                for (int py = 0; py < SCALE; py++) {
                    uint16_t *row = g_cbuf + (dyb + py) * DISP_W;
                    for (int px = 0; px < SCALE; px++) row[dx + px] = color;
                }
            }
        }
    }
}

static void nc2k_tick_cb(lv_timer_t *t)
{
    (void)t;
    /* Advance the CPU by *real* wall-clock elapsed time. Cap at 50ms so a
     * paused window / backgrounded tab doesn't flood us with emulation work
     * the moment we resume. */
    uint64_t now_ns = mono_ns();
    uint64_t elapsed_ns = now_ns - g_last_ns;
    g_last_ns = now_ns;
    if (elapsed_ns == 0) return;
    uint32_t elapsed_ms = (uint32_t)(elapsed_ns / 1000000ULL);
    if (elapsed_ms == 0) elapsed_ms = 1;
    if (elapsed_ms > 50) elapsed_ms = 50;
    RunTimeSlice(elapsed_ms, false);

    /* Throttle screen refresh via the upstream LCD_OUTER_REFRESH_INTERVAL.
     * Use ms-wall-clock, not lv_tick, for the same reason as the slice above. */
    static uint64_t last_render_ms = 0;
    uint64_t now_ms = now_ns / 1000000ULL;
    if (now_ms / LCD_OUTER_REFRESH_INTERVAL == last_render_ms / LCD_OUTER_REFRESH_INTERVAL)
        return;
    last_render_ms = now_ms;

    /* Check LCD on/off. */
    bool lcd_on = true;
    if (nc2000mode || nc1020mode) {
        uint8_t *ram_io = nc2k_states.ram_io;
        if (nc2000mode) lcd_on = (nc2k_states.lcden && nc2k_states.lcdon);
        if (nc1020mode) lcd_on = nc2k_states.lcdon;
        if (ram_io[0x05] >> 5 == 7) lcd_on = false;
    }

    if (!lcd_on) {
        memset(lcd_buf, 0, sizeof(lcd_buf));
    } else if (!CopyLcdBuffer(lcd_buf)) {
        return;
    }
    write_scaled_pixels(lcd_buf);
    lv_obj_invalidate(g_canvas);
}

/* ----- cz_app.h ABI ----- */
extern "C" CZ_APP_EXPORT void app_main(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x202A2E), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    g_cbuf = (uint16_t*)calloc(DISP_W * DISP_H, sizeof(uint16_t));
    if (!g_cbuf) { printf("[nc2000-lvgl] OOM canvas\n"); return; }

    g_canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(g_canvas, g_cbuf, DISP_W, DISP_H, LV_COLOR_FORMAT_RGB565);
    lv_obj_align(g_canvas, LV_ALIGN_CENTER, 0, 0);

    /* Seed a "booting..." banner so the user sees something instantly even if
     * ROM loading takes a moment. */
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, "NC2000 booting…");
    lv_obj_set_style_text_color(label, lv_color_hex(0x00E5FF), 0);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -2);

    /* Upstream init. The core needs ROM/NAND/NOR file paths — settings.cpp
     * defaults to "roms/<model>.*" relative to CWD. On desktop the emulator
     * is launched from AppBuilder/emulator/build/ which doesn't have ROMs;
     * give it a hand by chdir'ing to NC2000_ROM_DIR if provided. Device
     * installs (/usr/share/nc2000) follow a separate codepath in port_fb
     * and are unaffected. */
    const char *rom_dir = getenv("NC2000_ROM_DIR");
    if (rom_dir && rom_dir[0]) {
        if (chdir(rom_dir) != 0) {
            printf("[nc2000-lvgl] chdir(%s) failed; ROM files may not load\n", rom_dir);
        } else {
            printf("[nc2000-lvgl] cwd → %s\n", rom_dir);
        }
    }

    /* Upstream's process_args() is what normally sets rom paths. We don't
     * parse argv here — set nc2000 mode + ROM paths directly so LoadNC2k()
     * finds them. Device installs use a launcher script that chdirs to
     * /usr/share/nc2000/ where the same filenames live. */
    nc2000mode = true;
    nc2k_rom.nandFlashPath = "roms/nc2000.nand";
    nc2k_rom.nand0Path     = "roms/nc2000.nand0";
    nc2k_rom.norFlashPath  = "roms/nc2000.nor";

    init_parameters();
    init_keyitems();         /* builds sdl_to_item map used by handle_key_wayback */
    LoadNC2k();

    g_last_ns = mono_ns();
    /* Fire the tick cb as fast as LVGL will let us (1ms). The callback itself
     * computes real-elapsed time so the CPU stays in step with wall clock
     * regardless of the host's LVGL period. In practice the host's render
     * vsync pins this to ~16ms/frame, which means we hand the CPU ~16ms of
     * work (≈82k cycles at 5.12MHz) per slice — comfortably under the CM0
     * per-slice budget. */
    g_timer = lv_timer_create(nc2k_tick_cb, 1, NULL);

    printf("[nc2000-lvgl] app_main running (canvas %dx%d)\n", DISP_W, DISP_H);
}

extern "C" CZ_APP_EXPORT void app_event(int type, void *data)
{
    (void)data;
    if (type == CZ_EV_EXIT_REQUEST) {
        if (g_timer) { lv_timer_delete(g_timer); g_timer = NULL; }
        if (g_canvas) { lv_obj_delete(g_canvas); g_canvas = NULL; }
        if (g_cbuf) { free(g_cbuf); g_cbuf = NULL; }
    }
}

/* Current emulator (pin 318527b) dlsyms `ui_init`. Thunk it to app_main so
 * existing hosts keep working; future hosts that dlsym `app_main` directly
 * will prefer it. */
extern "C" CZ_APP_EXPORT void ui_init(void) { app_main(lv_screen_active()); }

/* ============================================================
 * Host keyboard hooks
 *
 * The emulator's main.cpp hands us every SDL_KEYDOWN / SDL_KEYUP / SDL_TEXTINPUT
 * event through g_kbd_handler (the symbol `lv_sdl_keyboard_handler` we export
 * below). We dispatch KEYDOWN/KEYUP into the NC2000 core's handle_key_wayback,
 * which writes the 8×8 keypadmatrix the emulated CPU scans in io_new.cpp /
 * NekoDriverIO.cpp.
 *
 * We still also create a dummy LVGL keypad indev so the LVGL side of things
 * doesn't warn about a missing read_cb.
 * ============================================================ */

/* We intentionally don't pull in real <SDL2/SDL.h> (the build's include path
 * is wired up to resolve it to the port_fb stub, which has no SDL_Event
 * layout). We only need two things from the event pointer the host passes us:
 *   - the event type  (Uint32 at offset 0)
 *   - .key.keysym.sym (SDL_Keycode == Sint32 at offset 20 of SDL_Event)
 * Those offsets are stable across SDL2 2.0.x.
 *   SDL_Event union → SDL_KeyboardEvent key
 *     offset  0  Uint32 type
 *     offset  4  Uint32 timestamp
 *     offset  8  Uint32 windowID
 *     offset 12  Uint8  state
 *     offset 13  Uint8  repeat
 *     offset 14  Uint8  padding2
 *     offset 15  Uint8  padding3
 *     offset 16  SDL_Keysym keysym
 *       offset 16  SDL_Scancode scancode   (enum → 4 bytes)
 *       offset 20  SDL_Keycode  sym        (Sint32)
 */
enum {
    SDL_KEYDOWN_VAL = 0x300,
    SDL_KEYUP_VAL   = 0x301,
};

static void _nc2k_kbd_read(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;
    data->state = LV_INDEV_STATE_RELEASED;
    data->key = 0;
}

extern "C" CZ_APP_EXPORT lv_indev_t *lv_sdl_keyboard_create(void) {
    lv_indev_t *kb = lv_indev_create();
    lv_indev_set_type(kb, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(kb, _nc2k_kbd_read);
    return kb;
}

extern "C" CZ_APP_EXPORT void lv_sdl_keyboard_handler(void *ev) {
    if (!ev) return;
    const uint8_t *bytes = (const uint8_t *)ev;
    uint32_t type;
    int32_t  sym;
    memcpy(&type, bytes + 0,  sizeof(type));
    memcpy(&sym,  bytes + 20, sizeof(sym));
    if (type == SDL_KEYDOWN_VAL) {
        handle_key_wayback((signed int)sym, true);
    } else if (type == SDL_KEYUP_VAL) {
        handle_key_wayback((signed int)sym, false);
    }
    /* SDL_TEXTINPUT is ignored — NC2000 samples the raw matrix from keycode
     * presses, and the character-input path (input_text) is only useful for
     * the debug console which isn't wired up in LVGL. */
}
