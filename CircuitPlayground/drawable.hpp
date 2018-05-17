#pragma once

/**
 * Base class for drawable objects
 */

#include <tuple>

#include <SDL.h>

#include "elements.hpp"

#include "tag_tuple.hpp"


class Drawable {
public:
    SDL_Rect renderArea;
};

