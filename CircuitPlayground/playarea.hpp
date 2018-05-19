#pragma once

#include <cstdint>

#include <SDL.h>

#include "declarations.hpp"
#include "drawable.hpp"
#include "gamestate.hpp"

/**
 * Represents the play area - the part of the window where the user can draw on.
 * This class handles the drawing onto the game canvas, including all the necessary translation and scaling (due to the adjustable zoom level and panning).
 * This class owns the GameState object (which stores the 2d current drawing state, including HIGH/LOW voltage state), and the CommandManager object.
 */

class PlayArea : public Drawable {
private:
    // owner window
    MainWindow& mainWindow;

    // game state
    GameState gameState;

    // transformation
    int32_t translationX = 0;
    int32_t translationY = 0;

public:
    PlayArea(MainWindow&);

    /**
    * Informs the play area that the dpi has been updated, so the play area should set its physical unit fields.
    */
    void updateDpi();

    /**
    * Renders this play area on the given area of the renderer.
    * This method is called by MainWindow
    * @pre renderer must not be null.
    */
    void render(SDL_Renderer* renderer) const;

    /**
    * Processing of events.
    */
    void processMouseMotionEvent(const SDL_MouseMotionEvent&);
    void processMouseButtonDownEvent(const SDL_MouseButtonEvent&);


    /**
    * This is called to reset all the hovering of stuff, because mousemove events are only triggered when the mouse is still on the canvas
    */
    void processMouseLeave();

};
