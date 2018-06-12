#pragma once

#include <cstdint>
#include <optional>

#include <SDL.h>

#include "declarations.hpp"
#include "drawable.hpp"
#include "statemanager.hpp"
#include "point.hpp"

/**
 * Represents the play area - the part of the window where the user can draw on.
 * This class handles the drawing onto the game canvas, including all the necessary translation and scaling (due to the adjustable zoom level and panning).
 * This class owns the StateManager object (which stores the 2d current drawing state, including HIGH/LOW voltage state), and the CommandManager object.
 */

class PlayArea : public Drawable {
private:
    // owner window
    MainWindow& mainWindow;

    // game state
    StateManager stateManager;

    // translation (in physical pixels)
    extensions::point translation{ 0, 0 };

    // length (in physical pixels) of each element; changes with zoom level
    int32_t scale = 20;

    // point that is being mouseovered
    // (in display coordinates, so that it will still work if the user zooms without moving the mouse)
    std::optional<extensions::point> mouseoverPoint;

    std::optional<size_t> drawingIndex = std::nullopt; // input handle index of the active drawing tool
    bool panning = false; // whether panning is active

    int32_t selectionOriginX, selectionOriginY; // the first point of the selection rectangle
    int32_t moveOriginX, moveOriginY; // the reference point (in matrix units) when moving a selection
    SDL_Rect selectionRect;
    Selector::State selectorState = Selector::INACTIVE;

    bool liveView = false; // whether live view (instead of default view) is being rendered

    extensions::point computeCanvasCoords(extensions::point physicalOffset) const {
        
        // translation:
        extensions::point offset = physicalOffset - translation;

        // scaling:
        return extensions::div_floor(offset, scale);
    }


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
    void processMouseButtonEvent(const SDL_MouseButtonEvent&);
    void processMouseWheelEvent(const SDL_MouseWheelEvent&);
    void processKeyboardEvent(const SDL_KeyboardEvent&);

    /**
     * This is called to reset all the hovering of stuff, because mousemove events are only triggered when the mouse is still on the canvas
     */
    void processMouseLeave();

    /**
     * Finish the current action and write to the undo stack.
     */
    void finishAction();

    /**
     * Use a drawing tool on (x, y)
     */
    template <typename Tool>
    void processDrawingTool(extensions::point pt) {
        extensions::point deltaTrans;

        // if it is a Pencil, forward the drawing to the gamestate
        if constexpr (std::is_base_of_v<Eraser, Tool>) {
            deltaTrans = stateManager.changePixelState<std::monostate>(pt.x, pt.y); // special handling for the eraser
        }
        else {
            deltaTrans = stateManager.changePixelState<Tool>(pt.x, pt.y); // forwarding for the normal elements
        }

        translation -= deltaTrans * scale;
    }

    /**
     * Returns the rectangle given by two opposite corners.
     */
    SDL_Rect getRect(int32_t x1, int32_t y1, int32_t x2, int32_t y2);
};
