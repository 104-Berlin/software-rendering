cmake_minimum_required(VERSION 3.15)
project(software-rendering)

set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

add_subdirectory(src/renderer)
add_subdirectory(deps/sdl2)

## IMGUI
add_library(ImGui STATIC    deps/imgui/imgui.cpp
                            deps/imgui/imgui_demo.cpp
                            deps/imgui/imgui_draw.cpp
                            deps/imgui/imgui_widgets.cpp
                            deps/imgui/imgui_tables.cpp)


target_include_directories(ImGui PUBLIC deps/imgui)