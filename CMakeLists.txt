cmake_minimum_required(VERSION 3.20.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(DTC_OVERLAY_FILE ./app.overlay)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(synthesizer)
add_subdirectory(drivers)

file(GLOB sources src/*.c src/*.cpp src/*/*.c src/*/*.cpp)
target_sources(app PRIVATE ${sources})
