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

# --- install rules: used by packaging/build_deb.sh -----------------
# Layout inside the resulting .deb:
#   /usr/share/nc2000/{nc2000, nc2000-launch.sh, wqxhex.bin, resource/, roms/}
#   /usr/share/APPLaunch/applications/nc2000.desktop
#   /usr/share/APPLaunch/share/images/nc2000_icon.png
install(TARGETS nc2000 RUNTIME DESTINATION share/nc2000)
install(FILES wqxhex.bin DESTINATION share/nc2000)
install(DIRECTORY resource DESTINATION share/nc2000)
install(DIRECTORY roms     DESTINATION share/nc2000)
install(PROGRAMS cardputer/nc2000-launch.sh DESTINATION share/nc2000)
install(FILES cardputer/nc2000.desktop
        DESTINATION share/APPLaunch/applications)
install(FILES packaging/nc2000_icon.png
        DESTINATION share/APPLaunch/share/images)
