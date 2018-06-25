#pragma once

/**
 * Base class for drawable objects
 */

#include <SDL.h>

#include "eventreceiver.hpp"


class Drawable : public EventReceiver {
public:
    SDL_Rect renderArea; // this is a physical size

    virtual void render(SDL_Renderer*) = 0;
};

