cmake_minimum_required(VERSION 3.15)
project(wqx_fb)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-O3 -g -pthread -DHANDYPSP -Wno-deprecated-declarations")

# NC2000 source root
set(NC2K_SRC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Our stub headers MUST come first so fake SDL.h / lcdpainter.h are found
# before the real ones in NC2K_SRC
include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${NC2K_SRC})

# Core emulation files (no SDL dependency)
set(CORE_SOURCES
    ${NC2K_SRC}/nc2000.cpp
    ${NC2K_SRC}/cpu.cpp
    ${NC2K_SRC}/cpu_loop.cpp
    ${NC2K_SRC}/cpu_loop_new.cpp
    ${NC2K_SRC}/mem.cpp
    ${NC2K_SRC}/io.cpp
    ${NC2K_SRC}/io_new.cpp
    ${NC2K_SRC}/rom.cpp
    ${NC2K_SRC}/nor.cpp
    ${NC2K_SRC}/nand.cpp
    ${NC2K_SRC}/ram.cpp
    ${NC2K_SRC}/comm.cpp
    ${NC2K_SRC}/cmd.cpp
    ${NC2K_SRC}/settings.cpp
    ${NC2K_SRC}/NekoDriverIO.cpp
    ${NC2K_SRC}/disassembler.cpp
    ${NC2K_SRC}/disassembler_new.cpp
    ${NC2K_SRC}/iv_uart.cpp
    ${NC2K_SRC}/udp_server.cpp
    ${NC2K_SRC}/dsp/dsp.cpp
    ${NC2K_SRC}/ansi/w65c02cpu.cpp
    ${NC2K_SRC}/ansi/w65c02op.cpp
    ${NC2K_SRC}/compare/c6502.cpp
    ${NC2K_SRC}/compare/pc1000bus.cpp
    ${NC2K_SRC}/lcdstripe/json.cpp
    ${NC2K_SRC}/key.cpp
)

# Framebuffer port files (replace SDL-dependent code)
set(PORT_SOURCES
    main_fb.cpp
    display_fb.cpp
    sound_stub.cpp
    input_fb.cpp
    console_stub.cpp
    lcdpainter_stub.cpp
)

add_executable(wqx_fb ${CORE_SOURCES} ${PORT_SOURCES})
target_link_libraries(wqx_fb pthread)
