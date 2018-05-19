#include <type_traits>
#include <variant>
#include <tuple>

#include <SDL.h>

#include "playarea.hpp"
#include "mainwindow.hpp"
#include "elements.hpp"

PlayArea::PlayArea(MainWindow& main_window) : mainWindow(main_window) {};


void PlayArea::updateDpi() {
    // do nothing, because play area works fully in physical pixels and there are no hardcoded logical pixels constants
}


void PlayArea::render(SDL_Renderer* renderer) const {
    // TODO
}


void PlayArea::processMouseMotionEvent(const SDL_MouseMotionEvent& event) {
    // offset relative to top-left of toolbox (in physical size; both event and renderArea are in physical size units)
    int offsetX = event.x - renderArea.x;
    int offsetY = event.y - renderArea.y;

    // TODO: maybe some mouseover effects like draw a white rectangle around the pixel being hovered, if the zoom level is high enough?
}


void PlayArea::processMouseButtonDownEvent(const SDL_MouseButtonEvent& event) {
    // offset relative to top-left of toolbox (in physical size; both event and renderArea are in physical size units)
    int offsetX = event.x - renderArea.x;
    int offsetY = event.y - renderArea.y;

    // translation:
    offsetX -= translationX;
    offsetY -= translationY;
    // TODO: proper transformation of coordinates for scaling

    MainWindow::tool_tags::get(mainWindow.selectedToolIndex, [this, offsetX, offsetY](const auto tool_tag) {
        // 'Element' is the type of element (e.g. ConductiveWire)
        using Tool = typename decltype(tool_tag)::type;

        if constexpr (std::is_base_of_v<Pencil, Tool>) {
            int32_t deltaTransX, deltaTransY;

            // if it is a Pencil, forward the drawing to the gamestate
            if constexpr (std::is_base_of_v<Eraser, Tool>) {
                std::tie(deltaTransX, deltaTransY) = gameState.changePixelState<std::monostate>(offsetX, offsetY); // special handling for the eraser
            }
            else {
                std::tie(deltaTransX, deltaTransY) = gameState.changePixelState<Tool>(offsetX, offsetY); // forwarding for the normal elements
            }

            translationX -= deltaTransX;
            translationY -= deltaTransY;
        }
        else if constexpr (std::is_base_of_v<Selector, Tool>) {
            // it is a Selector.
            // TODO.
        }
        else {
            // if we get here, we will get a compiler error
            static_assert(std::negation_v<std::is_same<Tool, Tool>>, "Invalid tool type!");
        }
    });

    // TODO
}


void PlayArea::processMouseLeave() {
    // TODO
}
