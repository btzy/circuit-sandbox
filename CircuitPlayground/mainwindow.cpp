#include <stdexcept>
#include <string>

#include <SDL.h>
#include <SDL_ttf.h>

#include "mainwindow.hpp"


using namespace std::literals::string_literals; // gives the 's' suffix for strings

#ifdef _WIN32
// hack for issue with window resizing on Windows not giving live events
int resizeEventForwarder(void* main_window_void_ptr, SDL_Event* event) {
    if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_RESIZED) {
        SDL_Window* event_window = SDL_GetWindowFromID(event->window.windowID);
        MainWindow* main_window = static_cast<MainWindow*>(main_window_void_ptr);
        if (event_window == main_window->window) {
            main_window->render();
        }
    }
    return 0;
}
#endif // _WIN32


MainWindow::MainWindow() : closing(false) {

    // TODO: allow high DPI with SDL_WINDOW_ALLOW_HIGHDPI flag and test whether it changes anything:
    window = SDL_CreateWindow("Circuit Playground", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
    if (window == nullptr) {
        throw std::runtime_error("SDL_CreateWindow() failed:  "s + SDL_GetError());
    }

    #ifdef _WIN32
    // On Windows, when the user is resizing the window, we don't get any events until the resize is complete.
    // This tries to fix this
    SDL_AddEventWatch(resizeEventForwarder, static_cast<void*>(this));
    #endif // _WIN32

    // Create the renderer.  SDL_RENDERER_PRESENTVSYNC turns on the monitor refresh rate synchronization.
    // For some reason my SDL2 doesn't have any software renderer, so I can't do "SDL_RENDERER_SOFTWARE | SDL_RENDERER_ACCELERATED"
    // So we try creating hardware renderer, and if it doesn't work then we try creating software renderer.
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE | SDL_RENDERER_PRESENTVSYNC);
    }
    if (renderer == nullptr) {
        throw std::runtime_error("SDL_CreateRenderer() failed:  "s + SDL_GetError());
    }
}


MainWindow::~MainWindow() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}


void MainWindow::start() {

    // Clear the window with a black background
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    // Show the window to the user
    SDL_ShowWindow(window);

    // event/drawing loop:
    while (true) {
        SDL_Event event;

        // get the next event to process, if any
        while(SDL_PollEvent(&event) != 0) {
            processEvent(event);
            if (closing) break;
        }

        // immediately break out of the loop if the user pressed the close button
        if (closing) break;

        // draw everything onto the screen
        render();
    }
}


void MainWindow::processEvent(const SDL_Event& event) {
    switch (event.type) {
    case SDL_WINDOWEVENT:
        processWindowEvent(event.window);
        break;
    }
}


void MainWindow::processWindowEvent(const SDL_WindowEvent& event) {
    switch (event.event) {
    case SDL_WINDOWEVENT_CLOSE: // close button was pressed (or some other close command like Alt-F4)
        closing = true;
        break;
    }
}


// TODO: needs some way to use the old data when resizing, for consistency?
void MainWindow::render() {
    // get the size of the render target
    int pixelWidth, pixelHeight;
    SDL_GetRendererOutputSize(renderer, &pixelWidth, &pixelHeight);

    // Clear the window with a black background
    SDL_SetRenderDrawColor(renderer, backgroundColor.r, backgroundColor.g, backgroundColor.b, 255);
    SDL_RenderClear(renderer);

    // TODO: draw everything to the screen - buttons, status info, play area, etc.
    toolbox.render(renderer, SDL_Rect{pixelWidth - 128, 0, 128, pixelHeight});

    // Then display to the user
    SDL_RenderPresent(renderer);
}

