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
 *   - We reuse the existing NC2K_CORE (cpu, memory, io, ROM loader) but not
 *     port_fb — that one writes to /dev/fb0 directly which we don't want.
 *
 * v0 scope: graphics only, no keyboard input wiring. That can be layered on
 * by grabbing lv_indev keypad events in the timer callback.
 */
#include <cz_app.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

/* Upstream NC2000 core symbols (C++ headers). */
#include "comm.h"
#include "nc2000.h"
#include "state.h"
#include "display.h"
#include "settings.h"

extern nc2k_states_t nc2k_states;
extern uint32_t SLICE_INTERVAL;
extern uint32_t LCD_OUTER_REFRESH_INTERVAL;

/* Upstream globals port_fb provided (we take over their role). */
uint8_t lcd_buf[SCREEN_WIDTH * SCREEN_HEIGHT / 8 * 2];
unsigned char *lcd_effect_buffer = nullptr;
void init_lcd_stripe() { /* stub — no LCD stripe decoration in LVGL mode */ }

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
static uint64_t   g_tick_ms = 0;

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
    /* Advance emulation one slice. RunTimeSlice takes a uint32_t ms amount. */
    RunTimeSlice(SLICE_INTERVAL, false);
    g_tick_ms += SLICE_INTERVAL;

    /* Throttle screen refresh via the upstream LCD_OUTER_REFRESH_INTERVAL. */
    static uint64_t last_render = 0;
    if (g_tick_ms / LCD_OUTER_REFRESH_INTERVAL == last_render / LCD_OUTER_REFRESH_INTERVAL)
        return;
    last_render = g_tick_ms;

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

    /* Upstream init. LoadNC2k() reads ROM/NAND/NOR from files whose paths are
     * set via process_args; without them, defaults are used and the core
     * still runs (just with empty storage). */
    init_parameters();
    LoadNC2k();

    g_timer = lv_timer_create(nc2k_tick_cb, SLICE_INTERVAL, NULL);

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

/* The emulator also looks for lv_sdl_keyboard_create / _handler. Without them
 * it creates a keypad indev with no read_cb → LVGL log spam. Provide a
 * minimal indev with a no-op read_cb until real keypad support lands. */
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
extern "C" CZ_APP_EXPORT void lv_sdl_keyboard_handler(void *ev) { (void)ev; }
