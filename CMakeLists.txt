cmake_minimum_required(VERSION 3.15)
project(cardputer_zero_nc2000 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -g -pthread -DHANDYPSP -Wno-deprecated-declarations")

find_package(SDL2 REQUIRED)
message(STATUS "sdl includes: ${SDL2_INCLUDE_DIRS}")
message(STATUS "sdl libs:     ${SDL2_LIBRARIES}")
include_directories(${SDL2_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

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

set(NC2K_SRC
    main.cpp
    nc2000.cpp
    disassembler.cpp
    disassembler_new.cpp
    cpu.cpp
    cpu_loop.cpp
    cpu_loop_new.cpp
    comm.cpp
    mem.cpp
    io.cpp
    io_new.cpp
    rom.cpp
    nor.cpp
    nand.cpp
    ram.cpp
    dsp/dsp.cpp
    sound.cpp
    cmd.cpp
    udp_server.cpp
    ansi/w65c02cpu.cpp
    ansi/w65c02op.cpp
    NekoDriverIO.cpp
    key.cpp
    key_new.cpp
    compare/c6502.cpp
    compare/pc1000bus.cpp
    settings.cpp
    display.cpp
    console.cpp
    iv_uart.cpp
    lcdstripe/json.cpp
    lcdstripe/lcdpainter.cpp
)

add_executable(nc2000 ${NC2K_SRC})
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
set(NC2K_FB_CORE
    nc2000.cpp
    cpu.cpp
    cpu_loop.cpp
    cpu_loop_new.cpp
    mem.cpp
    io.cpp
    io_new.cpp
    rom.cpp
    nor.cpp
    nand.cpp
    ram.cpp
    comm.cpp
    cmd.cpp
    settings.cpp
    NekoDriverIO.cpp
    disassembler.cpp
    disassembler_new.cpp
    iv_uart.cpp
    udp_server.cpp
    dsp/dsp.cpp
    ansi/w65c02cpu.cpp
    ansi/w65c02op.cpp
    compare/c6502.cpp
    compare/pc1000bus.cpp
    lcdstripe/json.cpp
    key.cpp
)
set(NC2K_FB_PORT
    ${NC2K_FB_DIR}/main_fb.cpp
    ${NC2K_FB_DIR}/display_fb.cpp
    ${NC2K_FB_DIR}/sound_stub.cpp
    ${NC2K_FB_DIR}/input_fb.cpp
    ${NC2K_FB_DIR}/console_stub.cpp
    ${NC2K_FB_DIR}/lcdpainter_stub.cpp
)
add_executable(nc2000_fb ${NC2K_FB_CORE} ${NC2K_FB_PORT})
# Stub SDL.h / lcdpainter.h live in port_fb/ and must come before the
# real headers sitting at the repo root.
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
install(FILES wqxhex.bin DESTINATION share/nc2000)
install(DIRECTORY resource DESTINATION share/nc2000)
install(DIRECTORY roms     DESTINATION share/nc2000)
install(PROGRAMS cardputer/nc2000-launch.sh DESTINATION share/nc2000)
install(FILES cardputer/nc2000.desktop
        DESTINATION share/APPLaunch/applications)
install(FILES packaging/nc2000_icon.png
        DESTINATION share/APPLaunch/share/images)
