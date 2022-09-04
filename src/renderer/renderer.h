#pragma once

#if WIN32
    #ifdef RENDERER_EXPORT
        #define R_API __declspec(dllexport)
    #else
        #define R_API __declspec(dllimport)
    #endif
#else
#define R_API
#endif


#include "freetype/freetype.h"

#define GL_WITH_GLAD
#include "freetype-gl/freetype-gl.h"


#include "rendering_interface.h"