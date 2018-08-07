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

    // A steady clock used for rendering.
    using RenderClock = std::chrono::steady_clock;

    // The current time (used for rendering any animations).
    // Set by MainWindow only, but used anywhere that needs animations.
    static inline RenderClock::time_point renderTime;

    virtual void render(SDL_Renderer*) = 0;
    virtual void layoutComponents(SDL_Renderer*) {}

};
