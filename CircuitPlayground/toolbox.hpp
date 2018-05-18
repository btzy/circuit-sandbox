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

    // logical units
    constexpr static int LOGICAL_BUTTON_HEIGHT = 20;
    constexpr static int LOGICAL_PADDING_HORIZONTAL = 8;
    constexpr static int LOGICAL_PADDING_VERTICAL = 8;

    // physical units
    int BUTTON_HEIGHT = LOGICAL_BUTTON_HEIGHT;
    int PADDING_HORIZONTAL = LOGICAL_PADDING_HORIZONTAL;
    int PADDING_VERTICAL = LOGICAL_PADDING_VERTICAL;
public:

    Toolbox(MainWindow&);

    /**
    * Informs the toolbox that the dpi has been updated, so the toolbox should set its physical unit fields.
    */
    void updateDpi();

    /**
     * Renders this toolbox on the given area of the renderer.
     * This method is called by MainWindow
     * @pre renderer must not be null.
     */
    void render(SDL_Renderer* renderer);

    /**
    * Processing of events.
    */
    void processMouseMotionEvent(const SDL_MouseMotionEvent&);
    void processMouseButtonDownEvent(const SDL_MouseButtonEvent&);


    /**
    * This is called to reset all the hovering of buttons, because mousemove events are only triggered when the mouse is still on the canvas
    */
    void processMouseLeave();
};

