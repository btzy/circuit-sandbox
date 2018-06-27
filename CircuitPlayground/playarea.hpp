#pragma once

#include <cstdint>
#include <optional>

#include <SDL.h>

#include "declarations.hpp"
#include "drawable.hpp"
#include "statemanager.hpp"
#include "point.hpp"
#include "playareaactionmanager.hpp"

/**
 * Represents the play area - the part of the window where the user can draw on.
 * This class handles the drawing onto the game canvas, including all the necessary translation and scaling (due to the adjustable zoom level and panning).
 * This class owns the StateManager object (which stores the 2d current drawing state, including HIGH/LOW voltage state), and the CommandManager object.
 */

class PlayArea final : public Drawable {
private:
    // owner window
    MainWindow& mainWindow;

    // translation (in physical pixels)
    ext::point translation{ 0, 0 };

    // length (in physical pixels) of each element; changes with zoom level
    int32_t scale = 20;

    // point that is being mouseovered
    // (in display coordinates, so that it will still work if the user zooms without moving the mouse)
    std::optional<ext::point> mouseoverPoint;

    std::optional<ext::point> panOrigin; // the last position the mouse was dragged to (nullopt if we are not currently panning)

    bool defaultView = false; // whether default view (instead of live view) is being rendered

    PlayAreaActionManager currentAction; // current action that receives playarea events, might be nullptr

    friend class PlayAreaAction;
    friend class EditAction;
    friend class FileOpenAction;

    /*friend class EditAction;
    friend class SaveableAction;
    template <typename T> friend class PencilAction;
    friend class EyedropperAction;
    friend class SelectionAction;
    friend class HistoryAction;
    friend class FileOpenAction;
    friend class FileSaveAction;
    friend class FileNewAction;*/

public:
    PlayArea(MainWindow&);

    /**
     * Informs the play area that the dpi has been updated, so the play area should set its physical unit fields.
     */
    void updateDpiFields();

    /**
     * Renders this play area on the given area of the renderer.
     * This method is called by MainWindow
     * This method is non-const because the cached state might be updated from the simulator when rendering.
     * @pre renderer must not be null.
     */
    void render(SDL_Renderer* renderer) override;
private:
    void render(SDL_Renderer* renderer, StateManager& stateManager);

public:
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

    bool processMouseButtonDown(const SDL_MouseButtonEvent&) override; // fires when the mouse is pressed down, must be inside the renderArea
    void processMouseDrag(const SDL_MouseMotionEvent&) override; // fires when mouse is moved, and was previously pressed down
    void processMouseButtonUp() override; // fires when the mouse is lifted up, might be outside the renderArea

    bool processMouseWheel(const SDL_MouseWheelEvent&) override;
    bool processKeyboard(const SDL_KeyboardEvent&) override;

    inline ext::point canvasFromWindowOffset(const ext::point& windowOffset) const {

        ext::point physicalOffset = windowOffset - ext::point(renderArea.x, renderArea.y);

        // translation:
        ext::point offset = physicalOffset - translation;

        // scaling:
        return ext::div_floor(offset, scale);
    }

    inline ext::point windowFromCanvasOffset(const ext::point& canvasOffset) const {
        return canvasOffset * scale + translation + ext::point(renderArea.x, renderArea.y);
    }

    inline int32_t getScale() const {
        return scale;
    }
};


