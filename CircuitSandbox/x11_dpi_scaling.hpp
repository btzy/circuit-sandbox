#pragma once

#include "declarations.hpp"
#include <SDL.h>

namespace DpiScaling {
    
    /**
     * Get DPI scaling.  Returns true if query was successful.
     * window might be null.
     */
    bool getDpi(SDL_Window* window, int display_index, float& out);
}
