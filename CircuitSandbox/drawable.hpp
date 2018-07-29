#pragma once

/**
 * Base class for drawable objects.
 * Drawables have a renderArea and can be rendered to screen.
 */

#include <chrono>

#include <SDL.h>

class Drawable {
public:
    SDL_Rect renderArea; // this is a physical size

    // a steady clock used for rendering
    using RenderClock = std::chrono::steady_clock;

    virtual void render(SDL_Renderer*, RenderClock::time_point) = 0;
    virtual void layoutComponents(SDL_Renderer*) {}

};
