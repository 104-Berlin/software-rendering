cmake_minimum_required(VERSION 3.15)
project(software-rendering)

set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

add_subdirectory(src/renderer)
add_subdirectory(deps/glfw)

add_executable(SoftwareRendering main.cpp)
target_link_libraries(SoftwareRendering srRenderer glfw)

set_target_properties(SoftwareRendering
                        PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
