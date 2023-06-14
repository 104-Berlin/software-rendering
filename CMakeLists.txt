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


add_executable(SoftwareRendering    main.cpp
                            deps/imgui/backends/imgui_impl_opengl3.cpp
                            deps/imgui/backends/imgui_impl_sdl.cpp)
                            
target_link_libraries(SoftwareRendering srRenderer SDL2-static ImGui)
target_include_directories(SoftwareRendering SYSTEM PUBLIC deps/sdl2/include)

set_target_properties(SoftwareRendering
                PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})


if (DEFINED EMSCRIPTEN)
    #set(CMAKE_EXECUTABLE_SUFFIX ".html")
    add_compile_definitions("__EMSCRIPTEN__")
    target_link_options(SoftwareRendering PRIVATE
        "-sMODULARIZE=1"
        "-sEXPORT_NAME='Main'"
        "-sINVOKE_RUN=0"
        "-sDISABLE_DEPRECATED_FIND_EVENT_TARGET_BEHAVIOR=0"
        "-sEXPORTED_FUNCTIONS=['_main']"
        "-sEXPORTED_RUNTIME_METHODS=['callMain', 'ccall', 'cwrap']"
        "-sFORCE_FILESYSTEM=1"
        "-sUSE_SDL=2"
        "-sALLOW_MEMORY_GROWTH=1" 
        "-sASYNCIFY"
        "-O2"
    )
    add_custom_command(
        TARGET srRenderer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_SOURCE_DIR}/index.html
                ${CMAKE_BINARY_DIR}/index.html
        )
else()
endif()


target_include_directories(ImGui PUBLIC deps/imgui)