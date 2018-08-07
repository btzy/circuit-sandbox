#pragma once

#include <cstdint>
#include <optional>
#include <memory>

#include <SDL.h>

#include "declarations.hpp"
#include "control.hpp"
#include "statemanager.hpp"
#include "point.hpp"
#include "playareaactionmanager.hpp"
#include "sdl_automatic.hpp"
#include "elementdescriptionutils.hpp"
#include "notificationdisplay.hpp"

/**
 * Represents the play area - the part of the window where the user can draw on.
 * This class handles the drawing onto the game canvas, including all the necessary translation and scaling (due to the adjustable zoom level and panning).
 * This class owns the StateManager object (which stores the 2d current drawing state, including HIGH/LOW voltage state), and the CommandManager object.
 */

class PlayArea final : public Control {
private:
    // owner window
    MainWindow& mainWindow;

    // translation (in physical pixels)
    ext::point translation{ 0, 0 };

    // length (in physical pixels) of each element; changes with zoom level
    int32_t scale;

    std::array<int32_t, 2> savedScale{ 1, 20 }; // saved zoom levels to toggle between
    size_t savedScaleIndex = 0; // index used by toggleZoom()
    NotificationDisplay::UniqueNotification saveZoomNotification;
    NotificationDisplay::UniqueNotification toggleZoomNotification;

    // variables used for zoom animations
    // if there's animation going on, `scale` and `translation` always refers to the end (i.e. target) values.
    Drawable::RenderClock::time_point zoomAnimationStartTime = Drawable::RenderClock::time_point::max();
    int32_t zoomScaleStart; // scale at the start of zoom animation
    ext::point zoomTranslationStart; // translation at the start of zoom animation

    // point that is being mouseovered
    // (in display coordinates, so that it will still work if the user zooms without moving the mouse)
    std::optional<ext::point> mouseoverPoint;

    // copy of element being mouseovered, for description in button bar
    Description::ElementVariant_t mouseoverElement;

    std::optional<ext::point> panOrigin; // the last position the mouse was dragged to (nullopt if we are not currently panning)

    // the texture used to render the canvas pixels on.
    // This is in canvas coordinates - the area that is visible on the play area.  Updated during layoutComponents(), or when the scale changes.
    // Note: depending on the translation, there might be one row/column of pixels at the right or bottom that is totally hidden from view.
    UniqueTexture pixelTexture;
    uint32_t pixelFormat;
    ext::point pixelTextureSize;

    bool defaultView = false; // whether default view (instead of live view) is being rendered
    NotificationDisplay::UniqueNotification defaultViewNotification;

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
     * Called once, by mainwindow, before calling any functions in playarea.
     */
    void initScale();

    /**
     * Renders this play area on the given area of the renderer.
     * This method is called by MainWindow
     * This method is non-const because the cached state might be updated from the simulator when rendering.
     * @pre renderer must not be null.
     */
    void render(SDL_Renderer* renderer) override;
private:
    void render(SDL_Renderer* renderer, StateManager& stateManager);

    /**
     * Prepares the texture for use.
     * This method must be called is the dimensions of the canvas might have changed (e.g. zoom in/out, resize window).
     * @pre renderer must not be null.
     */
    void prepareTexture(SDL_Renderer*);
    void prepareTexture(SDL_Renderer*, int32_t textureScale);

public:
    /**
     * Currently this just calls prepareTexture();
     * @pre renderer must not be null.
     */
    void layoutComponents(SDL_Renderer*) override;

    /**
     * Check if default view is being used
     */
    inline bool isDefaultView() const noexcept {
        return defaultView;
    }

    /**
     * Sets mouseoverElement field and updates description in button bar if necessary.
     */
    void changeMouseoverElement(const CanvasState::element_variant_t& newElement);

    /**
     * Save and toggle between two zoom levels.
     */
    void saveZoom();
    void toggleZoom();

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


