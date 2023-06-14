#pragma once

#if 0
#ifdef RENDERER_EXPORT
#define R_API __declspec(dllexport)
#else
#define R_API __declspec(dllimport)
#endif
#else
#define R_API
#endif

#define GL_WITH_GLAD

#include <unordered_map>

#include <freetype/freetype.h>

#include "rendering_interface.h"