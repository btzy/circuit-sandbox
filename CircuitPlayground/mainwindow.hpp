#pragma once

/**
 * The main window.
 */

#include <variant>
#include <optional>
#include <array>

#include <SDL.h>

#include "declarations.hpp"
#include "toolbox.hpp"
#include "playarea.hpp"
#include "drawable.hpp"


class MainWindow {
public:

    

    // logical units
    constexpr static int LOGICAL_TOOLBOX_WIDTH = 128;

    // physical units
    int TOOLBOX_WIDTH = LOGICAL_TOOLBOX_WIDTH;

private:
    bool closing; // whether the user has pressed the close button

    // Render-able components in the window:
    Toolbox toolbox;
    PlayArea playArea;

    const std::array<Drawable*, 2> drawables{ &toolbox, &playArea };
    Drawable* currentEventTarget; // the Drawable that the mouse was pressed down from
    Drawable* currentLocationTarget; // the Drawable that the mouse is currently inside

    // High DPI stuff:
    int physicalMultiplier = 1; // physical size = size in real monitor pixels
    int logicalMultiplier = 1; // logical size = size in device-independent virtual pixels
    // (physical size) = (logical size) * physicalMultiplier / logicalMultiplier

    /**
     * Process the event that has occurred (called by start())
     */
    void processEvent(const SDL_Event&);
    void processWindowEvent(const SDL_WindowEvent&);
    void processMouseMotionEvent(const SDL_MouseMotionEvent&);
    void processMouseButtonEvent(const SDL_MouseButtonEvent&);
    void processMouseWheelEvent(const SDL_MouseWheelEvent&);
    void processKeyboardEvent(const SDL_KeyboardEvent&);

    /**
     * Renders everything to the screen
     */
    void render();
    #ifdef _WIN32
    // hack for issue with window resizing on Windows not giving live events
    friend int resizeEventForwarder(void* main_window_void_ptr, SDL_Event* event);
    #endif // _WIN32

    /**
     * Recalculate all the 'renderArea' for all the Drawables (call after resizing)
     * Also does DPI recalculation (SDL2 will send a resize event when the dpi changes)
     */
    void layoutComponents();


    /**
    * Update the dpi fields.  Returns true if the dpi got changed
    */
    bool updateDpiFields(bool useWindow = true);

public:

    // SDL and window stuff:
    SDL_Window* window;
    SDL_Renderer* renderer;




    /**
     * Stores the element that is selected by the toolbox.
     */
    size_t selectedToolIndices[NUM_INPUT_HANDLES];

    constexpr static size_t EMPTY_INDEX = static_cast<size_t>(-1);

    constexpr static SDL_Color backgroundColor{0, 0, 0, 0xFF}; // black background

    /**
     * Initializes SDL and the window (it will be hidden)
     */
    MainWindow();

    /**
     * Destroys the window and quits SDL
     */
    ~MainWindow();

    /**
     * Shows the window to the user, and runs the event loop.
     * This function will block until the window is closed.
     */
    void start();

    /**
     * DPI conversion functions
     */
    inline int logicalToPhysicalSize(int logicalSize) const {
        return logicalSize * physicalMultiplier / logicalMultiplier;
    }
    inline int physicalToLogicalSize(int physicalSize) const {
        return physicalSize * logicalMultiplier / physicalMultiplier;
    }
};
