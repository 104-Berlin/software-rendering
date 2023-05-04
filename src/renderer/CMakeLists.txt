cmake_minimum_required(VERSION 3.15)
project(Renderer)

find_package(OpenGL REQUIRED)

file(GLOB RENDERER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

list(APPEND RENDERER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/glad/src/glad.c)
list(APPEND RENDERER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/freetype-gl/platform.c)
list(APPEND RENDERER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/freetype-gl/texture-atlas.c)
list(APPEND RENDERER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/freetype-gl/texture-font.c)
list(APPEND RENDERER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/freetype-gl/vector.c)
list(APPEND RENDERER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/freetype-gl/ftgl-utils.c)
list(APPEND RENDERER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/freetype-gl/distance-field.c)
list(APPEND RENDERER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/freetype-gl/utf8-utils.c)
list(APPEND RENDERER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/freetype-gl/edtaa3func.c)

add_subdirectory(freetype)

configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/freetype-gl/cmake/config.h.in"
        "${CMAKE_CURRENT_SOURCE_DIR}/freetype-gl/config.h"
)

add_library(srRenderer STATIC ${RENDERER_SRC})
target_compile_definitions(srRenderer PRIVATE RENDERER_EXPORT)
target_link_libraries(srRenderer ${OPENGL_LIBRARY} freetype)
target_include_directories(srRenderer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/glad/include)
target_include_directories(srRenderer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/glm)
target_include_directories(srRenderer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
