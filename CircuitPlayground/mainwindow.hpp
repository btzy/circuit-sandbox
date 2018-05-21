#pragma once

/**
 * The main window.
 */

#include <variant>
#include <optional>

#include <SDL.h>

#include "declarations.hpp"
#include "toolbox.hpp"
#include "playarea.hpp"
#include "tag_tuple.hpp"
#include "elements.hpp"

class MainWindow {
public:

    // compile-time type tag which stores the list of available elements
    using tool_tags = extensions::tag_tuple<Selector, Panner, Eraser, ConductiveWire, InsulatedWire>;

    // logical units
    constexpr static int LOGICAL_TOOLBOX_WIDTH = 128;

    // physical units
    int TOOLBOX_WIDTH = LOGICAL_TOOLBOX_WIDTH;

private:
    bool closing; // whether the user has pressed the close button

    // Render-able components in the window:
    Toolbox toolbox;
    PlayArea playArea;

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
     * Resolve the mouse button event to the input handle index (used with 'selectedToolIndices');
     * Uses fields 'which' and 'button'
     */
    static inline size_t resolveInputHandleIndex(const SDL_MouseButtonEvent& event) {
        if (event.which == SDL_TOUCH_MOUSEID) {
            return 0;
        }
        else {
            switch (event.button) {
            case SDL_BUTTON_LEFT:
                return 1;
            case SDL_BUTTON_MIDDLE:
                return 2;
            case SDL_BUTTON_RIGHT:
                return 3;
            case SDL_BUTTON_X1:
                return 4;
            case SDL_BUTTON_X2:
                return 5;
            default:
#if defined(__GNUC__)
                __builtin_unreachable();
#elif defined(_MSC_VER)
                __assume(0);
#endif
            }
        }
    }

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
