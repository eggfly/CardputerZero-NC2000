cmake_minimum_required(VERSION 3.15)
project(cardputer_zero_nc2000 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -g -pthread -DHANDYPSP -Wno-deprecated-declarations")

find_package(SDL2 REQUIRED)
message(STATUS "sdl includes: ${SDL2_INCLUDE_DIRS}")
message(STATUS "sdl libs:     ${SDL2_LIBRARIES}")

# All upstream NC2000 headers live under src/.
set(NC2K_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${SDL2_INCLUDE_DIRS} ${NC2K_SRC_DIR})

if(ENABLE_SERIAL_PORT)
    message(STATUS "serial port support enabled!")
    add_definitions(-DENABLE_SERIAL_PORT)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSERIALPORT REQUIRED libserialport)
    include_directories(${LIBSERIALPORT_INCLUDE_DIRS})
    link_directories(${LIBSERIALPORT_LIBDIR})
endif()

if(ENABLE_UDP_SERVER)
    message(STATUS "udp server support enabled!")
    add_definitions(-DENABLE_UDP_SERVER)
endif()

# Core emulation sources (shared by the SDL and framebuffer targets).
set(NC2K_CORE
    ${NC2K_SRC_DIR}/nc2000.cpp
    ${NC2K_SRC_DIR}/cpu.cpp
    ${NC2K_SRC_DIR}/cpu_loop.cpp
    ${NC2K_SRC_DIR}/cpu_loop_new.cpp
    ${NC2K_SRC_DIR}/mem.cpp
    ${NC2K_SRC_DIR}/io.cpp
    ${NC2K_SRC_DIR}/io_new.cpp
    ${NC2K_SRC_DIR}/rom.cpp
    ${NC2K_SRC_DIR}/nor.cpp
    ${NC2K_SRC_DIR}/nand.cpp
    ${NC2K_SRC_DIR}/ram.cpp
    ${NC2K_SRC_DIR}/comm.cpp
    ${NC2K_SRC_DIR}/cmd.cpp
    ${NC2K_SRC_DIR}/settings.cpp
    ${NC2K_SRC_DIR}/NekoDriverIO.cpp
    ${NC2K_SRC_DIR}/disassembler.cpp
    ${NC2K_SRC_DIR}/disassembler_new.cpp
    ${NC2K_SRC_DIR}/iv_uart.cpp
    ${NC2K_SRC_DIR}/udp_server.cpp
    ${NC2K_SRC_DIR}/dsp/dsp.cpp
    ${NC2K_SRC_DIR}/ansi/w65c02cpu.cpp
    ${NC2K_SRC_DIR}/ansi/w65c02op.cpp
    ${NC2K_SRC_DIR}/compare/c6502.cpp
    ${NC2K_SRC_DIR}/compare/pc1000bus.cpp
    ${NC2K_SRC_DIR}/lcdstripe/json.cpp
    ${NC2K_SRC_DIR}/key.cpp
)

# SDL target sources on top of the core.
set(NC2K_SDL_ONLY
    ${NC2K_SRC_DIR}/main.cpp
    ${NC2K_SRC_DIR}/sound.cpp
    ${NC2K_SRC_DIR}/display.cpp
    ${NC2K_SRC_DIR}/console.cpp
    ${NC2K_SRC_DIR}/key_new.cpp
    ${NC2K_SRC_DIR}/lcdstripe/lcdpainter.cpp
)

add_executable(nc2000 ${NC2K_CORE} ${NC2K_SDL_ONLY})
target_link_libraries(nc2000 PRIVATE ${SDL2_LIBRARIES})

if(ENABLE_UDP_SERVER AND WIN32)
    target_link_libraries(nc2000 PRIVATE wsock32 ws2_32)
endif()

if(ENABLE_SERIAL_PORT)
    target_compile_options(nc2000 PRIVATE ${LIBSERIALPORT_CFLAGS})
    target_link_libraries(nc2000 PRIVATE ${LIBSERIALPORT_LIBRARIES})
endif()

# ============================================================
# nc2000_fb — framebuffer port (no SDL, renders /dev/fb0 directly,
# reads /dev/input/event* via evdev). This is the variant that
# actually works on CardputerZero since libsdl2 2.32 on Debian
# trixie has no fbcon/fbdev backend and the LCD is a fbtft device
# that KMSDRM can't reach. Linux-only.
# ============================================================
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(NC2K_FB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/port_fb)
    set(NC2K_FB_PORT
        ${NC2K_FB_DIR}/main_fb.cpp
        ${NC2K_FB_DIR}/display_fb.cpp
        ${NC2K_FB_DIR}/sound_stub.cpp
        ${NC2K_FB_DIR}/input_fb.cpp
        ${NC2K_FB_DIR}/console_stub.cpp
        ${NC2K_FB_DIR}/lcdpainter_stub.cpp
    )
    add_executable(nc2000_fb ${NC2K_CORE} ${NC2K_FB_PORT})
    # Stub SDL.h / lcdpainter.h live in port_fb/ and must come before
    # the real headers sitting under src/.
    target_include_directories(nc2000_fb BEFORE PRIVATE ${NC2K_FB_DIR})
    target_link_libraries(nc2000_fb PRIVATE pthread)
endif()

# --- install rules: used by packaging/build_deb.sh -----------------
# Layout inside the resulting .deb:
#   /usr/share/nc2000/{nc2000_fb, nc2000-launch.sh, wqxhex.bin,
#                      resource/, roms/}
#   /usr/share/APPLaunch/applications/nc2000.desktop
#   /usr/share/APPLaunch/share/images/nc2000_icon.png
#
# Only the framebuffer binary goes into the .deb — the SDL variant is
# kept as a build target for dev machines (macOS/Linux desktops) but
# isn't useful on CardputerZero.
if(TARGET nc2000_fb)
    install(TARGETS nc2000_fb RUNTIME DESTINATION share/nc2000)
endif()
install(FILES ${NC2K_SRC_DIR}/wqxhex.bin DESTINATION share/nc2000)
install(DIRECTORY resource DESTINATION share/nc2000)
install(DIRECTORY roms     DESTINATION share/nc2000)
install(PROGRAMS cardputer/nc2000-launch.sh DESTINATION share/nc2000)
install(FILES cardputer/nc2000.desktop
        DESTINATION share/APPLaunch/applications)
install(FILES packaging/nc2000_icon.png
        DESTINATION share/APPLaunch/share/images)

# --- CardputerZero startup splash (320x170 keymap cheatsheet) -------
# Generated at configure time by packaging/gen_splash.py into
# ${CMAKE_CURRENT_BINARY_DIR}/splash.rgb565 (raw RGB565, little endian).
# Python3 + Pillow required on the build host; CI already has both.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
    set(SPLASH_OUT_PNG ${CMAKE_CURRENT_BINARY_DIR}/splash.png)
    set(SPLASH_OUT_RAW ${CMAKE_CURRENT_BINARY_DIR}/splash.rgb565)
    add_custom_command(
        OUTPUT ${SPLASH_OUT_PNG} ${SPLASH_OUT_RAW}
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/packaging/gen_splash.py
                ${SPLASH_OUT_PNG}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/packaging/gen_splash.py
        COMMENT "Generating CardputerZero startup splash"
    )
    add_custom_target(nc2000_splash ALL DEPENDS ${SPLASH_OUT_RAW})
    install(FILES ${SPLASH_OUT_RAW} DESTINATION share/nc2000)
else()
    message(WARNING "python3 not found; skipping splash.rgb565 generation")
endif()
