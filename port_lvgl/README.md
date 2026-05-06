# port_lvgl — NC2000 as a CardputerZero LVGL app

Builds NC2000 as a dlopen'd shared library implementing the `cz_app.h`
ABI. The resulting `libnc2000.dylib/.so/.dll` loads into the desktop
CardputerZero emulator and, unchanged, into APPLaunch on the device.

This is additive — it does **not** affect the existing `port_fb/` build.
Both targets coexist and can be built together.

## Status (v0)

- [x] Graphics path: canvas → LVGL display; 160×80 WQX scaled 2× to 320×160
- [x] LCD on/off handling, 1bpp and 4-grey modes
- [ ] Keyboard input (TODO — wire LVGL indev keypad events into the NC2000 key table)
- [ ] Sound (TODO — SDL_mixer on desktop, ALSA on device)

## How to build

Enable the port with a CMake option:

```bash
cmake -S . -B build -DENABLE_PORT_LVGL=ON \
      -DCZ_SDK_DIR=/path/to/CardputerZero-AppBuilder/sdk \
      -DCZ_LVGL_DIR=/path/to/CardputerZero-AppBuilder/emulator/lib/lvgl
cmake --build build --target nc2000_lvgl
```

Or, from the AppBuilder side (once `apps/nc2000` points here as a
submodule):

```bash
czdev run apps/nc2000
```

## Layout

`port_lvgl/main_lvgl.cpp` reuses `NC2K_CORE` (CPU, memory, IO, ROM) from
upstream NC2000 and adds:

- `write_scaled_pixels(lcd_buf)` — converts the 160×80 mono/grey WQX
  framebuffer into RGB565 suitable for an LVGL canvas
- `nc2k_tick_cb` — an LVGL timer firing at `SLICE_INTERVAL`; advances the
  core one slice and invalidates the canvas at the throttled refresh rate
- `app_main` / `app_event` — the cz_app.h entry points
