#pragma once

/**
 * The main window.
 */

#include <SDL.h>

#include "toolbox.hpp"

class MainWindow {
private:
    // SDL and window stuff:
    SDL_Window* window;
    SDL_Renderer* renderer;
    bool closing; // whether the user has pressed the close button

    // Render-able components in the window:
    Toolbox toolbox;

    /**
     * Process the event that has occurred (called by start())
     */
    void processEvent(const SDL_Event&);
    void processWindowEvent(const SDL_WindowEvent&);

    /**
     * Renders everything to the screen
     */
    void render();
    #ifdef _WIN32
    // hack for issue with window resizing on Windows not giving live events
    friend int resizeEventForwarder(void* main_window_void_ptr, SDL_Event* event);
    #endif // _WIN32

public:

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
};
