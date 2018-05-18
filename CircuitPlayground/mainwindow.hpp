#pragma once

/**
 * The main window.
 */

#include <variant>
#include <optional>

#include <SDL.h>

#include "declarations.hpp"
#include "toolbox.hpp"
#include "tag_tuple.hpp"
#include "elements.hpp"

class MainWindow {
public:

    // compile-time type tag which stores the list of available elements
    using element_tags = extensions::tag_tuple<ConductiveWire, InsulatedWire>;

private:
    bool closing; // whether the user has pressed the close button

    // Render-able components in the window:
    Toolbox toolbox;

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
    void processMouseButtonDownEvent(const SDL_MouseButtonEvent&);

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
     * Stores all the data necessary to maintain the context in which the user is interacting.
     */
    struct InteractionContext {
        // selection state: (it is a std::variant of tag<Element>)
        size_t selectedElementIndex;

        // element being moused over (so we can show something on the UI)
        size_t mouseoverElementIndex;

        constexpr static size_t EMPTY_INDEX = static_cast<size_t>(-1);
    };

    InteractionContext context;

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
    inline int logicalToPhysicalSize(int logicalSize) {
        return logicalSize * physicalMultiplier / logicalMultiplier;
    }
    inline int physicalToLogicalSize(int physicalSize) {
        return physicalSize * logicalMultiplier / physicalMultiplier;
    }
};
