#pragma once

/**
 * Represents the toolbox - the place where all the element buttons are located.
 */

#include <SDL.h>

#include "declarations.hpp"
#include "drawable.hpp"




class Toolbox final : public Drawable {
private:
    // owner window
    MainWindow& mainWindow;

    // logical units
    constexpr static int LOGICAL_BUTTON_HEIGHT = 24;
    constexpr static int LOGICAL_PADDING_HORIZONTAL = 8;
    constexpr static int LOGICAL_PADDING_VERTICAL = 8;
    constexpr static int LOGICAL_BUTTON_PADDING = 2;
    constexpr static int LOGICAL_BUTTON_SPACING = 2;

    // physical units
    int BUTTON_HEIGHT = LOGICAL_BUTTON_HEIGHT;
    int PADDING_HORIZONTAL = LOGICAL_PADDING_HORIZONTAL;
    int PADDING_VERTICAL = LOGICAL_PADDING_VERTICAL;
    int BUTTON_PADDING = LOGICAL_BUTTON_PADDING;
    int BUTTON_SPACING = LOGICAL_BUTTON_SPACING;

    // the index of the element being mouseovered
    size_t mouseoverToolIndex;

    /**
     * The colour of the each handle
     */
    constexpr static SDL_Color selectorHandleColors[NUM_INPUT_HANDLES] = {
        { 0xFF, 0xFF, 0, SDL_ALPHA_OPAQUE },
        { 0xFF, 0, 0, SDL_ALPHA_OPAQUE },
        { 0, 0xFF, 0, SDL_ALPHA_OPAQUE },
        { 0x55, 0x55, 0xFF, SDL_ALPHA_OPAQUE },
        { 0, 0xCC, 0xFF, SDL_ALPHA_OPAQUE },
        { 0xCC, 0, 0xFF, SDL_ALPHA_OPAQUE }
    };

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
    void render(SDL_Renderer* renderer) const;

    /**
     * Processing of events.
     */
    void processMouseHover(const SDL_MouseMotionEvent&) override;
    void processMouseButtonDown(const SDL_MouseButtonEvent&) override;


    /**
     * This is called to reset all the hovering of buttons, because mousemove events are only triggered when the mouse is still on the canvas
     */
    void processMouseLeave();
};

