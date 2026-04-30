#include "comm.h"
#include "nc2000.h"
#include "display.h"
#include "state.h"
extern nc2k_states_t nc2k_states;
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>

uint8_t lcd_buf[SCREEN_WIDTH * SCREEN_HEIGHT / 8 * 2];
unsigned char *lcd_effect_buffer = nullptr;

static int fb_fd = -1;
static uint16_t *fb_mem = nullptr;
static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static int fb_width = 0;
static int fb_height = 0;
static int fb_stride = 0; // bytes per line

// WQX 160x80, scale 2x => 320x160, centered on 320x170 screen
static const int SCALE = 2;
static const int WQX_W = SCREEN_WIDTH;  // 160
static const int WQX_H = SCREEN_HEIGHT; // 80
static const int DISP_W = WQX_W * SCALE; // 320
static const int DISP_H = WQX_H * SCALE; // 160

// 4-level grayscale palette (RGB565)
// White → Light gray → Dark gray → Black (green-tinted like original LCD)
static const uint16_t palette_rgb565[4] = {
    0xD6F4, // ~(214,222,164) light green-white
    0x9D11, // ~(156,162,140) mid-light
    0x6B49, // ~(105,105,76)  mid-dark
    0x1861, // ~(24,12,12)    near-black
};

void init_lcd_stripe() {
    // stub - no LCD stripe decoration in fb mode
}

static bool fb_init() {
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        perror("open /dev/fb0");
        return false;
    }
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
    ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo);

    fb_width = vinfo.xres;
    fb_height = vinfo.yres;
    fb_stride = finfo.line_length;

    size_t fb_size = fb_stride * fb_height;
    fb_mem = (uint16_t *)mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_mem == MAP_FAILED) {
        perror("mmap fb");
        close(fb_fd);
        fb_fd = -1;
        return false;
    }

    printf("FB: %dx%d, bpp=%d, stride=%d\n", fb_width, fb_height, vinfo.bits_per_pixel, fb_stride);
    return true;
}

bool display_fb_init() {
    if (!fb_init()) return false;
    lcd_effect_buffer = new unsigned char[4]; // dummy, not used
    return true;
}

/* Blit a 320x170 raw RGB565 splash (little-endian) from a file to
 * fb0. Returns false if the file can't be opened / is wrong size;
 * caller should then fall through to emulator output. */
bool display_fb_show_splash(const char *path) {
    if (!fb_mem) return false;
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        printf("[splash] cannot open %s\n", path);
        return false;
    }
    const int SPW = 320;
    const int SPH = 170;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz != (long)(SPW * SPH * 2)) {
        printf("[splash] %s wrong size %ld (expected %d)\n", path, sz, SPW*SPH*2);
        fclose(fp);
        return false;
    }
    // Read row by row and blit into fb, centered (or clipped to fb size)
    int off_x = (fb_width  - SPW) / 2;  if (off_x < 0) off_x = 0;
    int off_y = (fb_height - SPH) / 2;  if (off_y < 0) off_y = 0;
    int copy_w = SPW;  if (off_x + copy_w > fb_width)  copy_w = fb_width  - off_x;
    int copy_h = SPH;  if (off_y + copy_h > fb_height) copy_h = fb_height - off_y;

    uint16_t row[SPW];
    for (int y = 0; y < SPH; ++y) {
        size_t got = fread(row, 2, SPW, fp);
        if ((int)got != SPW) break;
        if (y >= copy_h) continue;
        uint16_t *dst = fb_mem + ((off_y + y) * (fb_stride / 2)) + off_x;
        memcpy(dst, row, copy_w * 2);
    }
    fclose(fp);
    printf("[splash] drew %s\n", path);
    return true;
}

void display_fb_shutdown() {
    if (fb_mem && fb_mem != MAP_FAILED) {
        munmap(fb_mem, fb_stride * fb_height);
        fb_mem = nullptr;
    }
    if (fb_fd >= 0) {
        close(fb_fd);
        fb_fd = -1;
    }
}

static uint64_t last_render_tick = 0;

void Render(uint64_t tick) {
    if (tick / LCD_OUTER_REFRESH_INTERVAL == last_render_tick / LCD_OUTER_REFRESH_INTERVAL)
        return;
    last_render_tick = tick;

    if (fb_fd < 0) return;

    // Get LCD state
    unsigned char &lcden = nc2k_states.lcden;
    unsigned char &lcdon = nc2k_states.lcdon;
    bool lcd_on = true;
    if (nc2000mode || nc1020mode) {
        uint8_t *ram_io = nc2k_states.ram_io;
        if (nc2000mode) lcd_on = (lcden && lcdon);
        if (nc1020mode) lcd_on = lcdon;
        if (ram_io[0x05] >> 5 == 7) lcd_on = false;
    }

    if (!lcd_on) {
        memset(lcd_buf, 0, sizeof(lcd_buf));
    } else if (!CopyLcdBuffer(lcd_buf)) {
        return;
    }

    // Calculate centering offset
    int off_x = (fb_width - DISP_W) / 2;
    int off_y = (fb_height - DISP_H) / 2;
    if (off_x < 0) off_x = 0;
    if (off_y < 0) off_y = 0;

    uint16_t *line_base = (uint16_t *)((uint8_t *)fb_mem + off_y * fb_stride);

    if (!is_grey_mode()) {
        // Monochrome: 1-bit per pixel
        for (int i = 0; i < WQX_W * WQX_H / 8; i++) {
            for (int j = 0; j < 8; j++) {
                bool pixel = (lcd_buf[i] & (1 << (7 - j))) != 0;
                uint16_t color = pixel ? palette_rgb565[3] : palette_rgb565[0];

                int pos = i * 8 + j;
                int src_y = pos / WQX_W;
                int src_x = pos % WQX_W;

                int dst_x = off_x + src_x * SCALE;
                int dst_y_base = src_y * SCALE;

                for (int sy = 0; sy < SCALE; sy++) {
                    uint16_t *row = (uint16_t *)((uint8_t *)line_base + (dst_y_base + sy) * fb_stride);
                    for (int sx = 0; sx < SCALE; sx++) {
                        row[dst_x + sx] = color;
                    }
                }
            }
        }
    } else {
        // 4-level grayscale: 2 bits per pixel
        for (int i = 0; i < WQX_W * WQX_H / 8 * 2; i++) {
            for (int j = 0; j < 4; j++) {
                uint8_t value = (lcd_buf[i] >> (6 - j * 2)) & 0x03;
                uint16_t color = palette_rgb565[value];

                int pos = (i * 8 + j * 2) / 2;
                int src_y = pos / WQX_W;
                int src_x = pos % WQX_W;

                int dst_x = off_x + src_x * SCALE;
                int dst_y_base = src_y * SCALE;

                for (int sy = 0; sy < SCALE; sy++) {
                    uint16_t *row = (uint16_t *)((uint8_t *)line_base + (dst_y_base + sy) * fb_stride);
                    for (int sx = 0; sx < SCALE; sx++) {
                        row[dst_x + sx] = color;
                    }
                }
            }
        }
    }
}
