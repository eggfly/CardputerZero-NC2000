#include "comm.h"
#include "nc2000.h"
#include "display.h"
#include "sound.h"
#include "settings.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <time.h>
#include <signal.h>

// From display_fb.cpp
bool display_fb_init();
void display_fb_shutdown();

// From input_fb.cpp
void input_fb_init(bool enable_evdev, bool enable_stdin, bool enable_tcp, int port);
void input_fb_shutdown();

static volatile bool running = true;

static void signal_handler(int sig) {
    (void)sig;
    running = false;
}

static uint64_t get_ticks_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void main_loop() {
    uint64_t start_tick = get_ticks_ms();
    uint64_t expected_tick = 0;

    while (running) {
        RunTimeSlice(SLICE_INTERVAL, false);
        Render(expected_tick);

        expected_tick += SLICE_INTERVAL;
        uint64_t actual_tick = get_ticks_ms() - start_tick;

        if (actual_tick > expected_tick + 300)
            expected_tick = actual_tick - 300;

        if (actual_tick < expected_tick) {
            usleep((expected_tick - actual_tick) * 1000);
        }
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    process_args(argc, argv);
    init_parameters();

    printf("=== WQX Emulator (Framebuffer Port) ===\n");
    printf("Mode: %s\n", nc2000mode ? "NC2000" : (nc1020mode ? "NC1020" : "unknown"));

    LoadNC2k();

    if (!display_fb_init()) {
        fprintf(stderr, "Failed to init framebuffer display\n");
        return 1;
    }

    // Enable all input paths: evdev (physical kb) + TCP (remote)
    // stdin disabled by default (would interfere with terminal)
    input_fb_init(true, false, true, 9527);

    printf("Running... (Ctrl+C to quit, TCP key server on port 9527)\n");
    main_loop();

    printf("Shutting down...\n");
    if (save_flash_on_exit) save_flash("");
    if (save_state_on_exit) save_state("");

    input_fb_shutdown();
    display_fb_shutdown();

    return 0;
}
