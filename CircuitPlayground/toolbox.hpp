#pragma once

/**
 * Represents the toolbox - the place where all the element buttons are located.
 */

#include <tuple>
#include <optional>
#include <variant>

#include <SDL.h>

#include "declarations.hpp"
#include "drawable.hpp"
#include "elements.hpp"

#include "tag_tuple.hpp"


class Toolbox : public Drawable {
private:
    // owner window
    MainWindow& mainWindow;

    constexpr static int BUTTON_HEIGHT = 20;
    constexpr static int PADDING_HORIZONTAL = 8;
    constexpr static int PADDING_VERTICAL = 8;
public:

    Toolbox(MainWindow&);

    /**
     * Renders this toolbox on the given area of the renderer.
     * This method is called by MainWindow
     * @pre renderer must not be null.
     */
    void render(SDL_Renderer* renderer);

    void processMouseMotionEvent(const SDL_MouseMotionEvent&);
    void processMouseButtonDownEvent(const SDL_MouseButtonEvent&);
};

