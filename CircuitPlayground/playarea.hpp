#pragma once

#include <cstdint>
#include <optional>

#include <SDL.h>

#include "declarations.hpp"
#include "drawable.hpp"
#include "statemanager.hpp"
#include "point.hpp"
#include "action.hpp"

/**
 * Represents the play area - the part of the window where the user can draw on.
 * This class handles the drawing onto the game canvas, including all the necessary translation and scaling (due to the adjustable zoom level and panning).
 * This class owns the StateManager object (which stores the 2d current drawing state, including HIGH/LOW voltage state), and the CommandManager object.
 */

class PlayArea final : public Drawable {
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

    std::optional<extensions::point> panOrigin; // the last position the mouse was dragged to (nullopt if we are not currently panning)


    bool defaultView = false; // whether default view (instead of live view) is being rendered

    Action currentAction;


    friend class EditAction;
    template <typename T> friend class PencilAction;
    friend class SelectionAction;



public:
    PlayArea(MainWindow&);

    /**
     * Informs the play area that the dpi has been updated, so the play area should set its physical unit fields.
     */
    void updateDpi();

    /**
     * Renders this play area on the given area of the renderer.
     * This method is called by MainWindow
     * This method is non-const because the cached state might be updated from the simulator when rendering.
     * @pre renderer must not be null.
     */
    void render(SDL_Renderer* renderer);

    /**
     * Check if default view is being used
     */
    inline bool isDefaultView() const noexcept {
        return defaultView;
    }

    /**
     * Processing of events.
     */
    void processMouseHover(const SDL_MouseMotionEvent&) override; // fires when mouse is inside the renderArea
    void processMouseLeave() override; // fires once when mouse goes out of the renderArea

    void processMouseButtonDown(const SDL_MouseButtonEvent&) override; // fires when the mouse is pressed down, must be inside the renderArea
    void processMouseDrag(const SDL_MouseMotionEvent&) override; // fires when mouse is moved, and was previously pressed down
    void processMouseButtonUp(const SDL_MouseButtonEvent&) override; // fires when the mouse is lifted up, might be outside the renderArea

    virtual void processMouseWheel(const SDL_MouseWheelEvent&) override;
    virtual void processKeyboard(const SDL_KeyboardEvent&) override;

    extensions::point computeCanvasCoords(extensions::point physicalOffset) const {

        // translation:
        extensions::point offset = physicalOffset - translation;

        // scaling:
        return extensions::div_floor(offset, scale);
    }
};


