#include <SDL.h>

#include "playarea.hpp"

PlayArea::PlayArea(MainWindow& main_window) : mainWindow(main_window) {};


void PlayArea::updateDpi() {
    // do nothing, because play area works fully in physical pixels and there are no hardcoded logical pixels constants
}


void PlayArea::render(SDL_Renderer* renderer) {
    // TODO
}


void PlayArea::processMouseMotionEvent(const SDL_MouseMotionEvent& event) {
    // offset relative to top-left of toolbox (in physical size; both event and renderArea are in physical size units)
    int offsetX = event.x - renderArea.x;
    int offsetY = event.y - renderArea.y;

    // TODO
}


void PlayArea::processMouseButtonDownEvent(const SDL_MouseButtonEvent& event) {
    // offset relative to top-left of toolbox (in physical size; both event and renderArea are in physical size units)
    int offsetX = event.x - renderArea.x;
    int offsetY = event.y - renderArea.y;

    // TODO
}


void PlayArea::processMouseLeave() {
    // TODO
}
