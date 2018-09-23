/*
 * Circuit Sandbox
 * Copyright 2018 National University of Singapore <enterprise@nus.edu.sg>
 *
 * This file is part of Circuit Sandbox.
 * Circuit Sandbox is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 * Circuit Sandbox is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with Circuit Sandbox.  If not, see <https://www.gnu.org/licenses/>.
 */

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
