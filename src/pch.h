#pragma once

#include <iostream>
#include <vector>
#include <tuple>
#include <stdint.h>
#include <string>
#include <stdio.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/vector_angle.hpp>

#define SR_TRACE(...)     \
    printf("[SR-TRACE]"); \
    printf(__VA_ARGS__);  \
    printf("\n")
