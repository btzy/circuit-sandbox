#pragma once

/**
 * Represents the toolbox - the place where all the element buttons are located.
 */

#include <tuple>

#include <SDL.h>

#include "elements.hpp"

#include "tag_tuple.hpp"


class Toolbox {
private:
    // compile-time type tag which stores the list of available elements
    using element_tags = extensions::tag_tuple<ConductiveWire, InsulatedWire>;

public:

    /**
     * Renders this toolbox on the given area of the renderer.
     * This method is called by MainWindow
     * @pre renderer must not be null.
     */
    void render(SDL_Renderer* renderer, const SDL_Rect& render_area);
};

