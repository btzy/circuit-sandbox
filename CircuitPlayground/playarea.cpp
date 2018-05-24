#include <type_traits>
#include <variant>
#include <tuple>
#include <cstdint>

#include <SDL.h>
#include <variant>

#include "playarea.hpp"
#include "mainwindow.hpp"
#include "elements.hpp"
#include "integral_division.hpp"
#include "point.hpp"

PlayArea::PlayArea(MainWindow& main_window) : mainWindow(main_window) {};


void PlayArea::updateDpi() {
    // do nothing, because play area works fully in physical pixels and there are no hardcoded logical pixels constants
}


void PlayArea::render(SDL_Renderer* renderer) const {
    // calculate the rectangle (in gamestate coordinates) that we will be drawing:
    SDL_Rect surfaceRect;
    surfaceRect.x = extensions::div_floor(-translationX, scale);
    surfaceRect.y = extensions::div_floor(-translationY, scale);
    surfaceRect.w = extensions::div_ceil(renderArea.w - translationX, scale) - surfaceRect.x;
    surfaceRect.h = extensions::div_ceil(renderArea.h - translationY, scale) - surfaceRect.y;

    // render the gamestate
    SDL_Surface* surface = SDL_CreateRGBSurface(0, surfaceRect.w, surfaceRect.h, 32, 0x000000FFu, 0x0000FF00u, 0x00FF0000u, 0);
    stateManager.fillSurface(liveView, reinterpret_cast<uint32_t*>(surface->pixels), surfaceRect.x, surfaceRect.y, surfaceRect.w, surfaceRect.h);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    // set clip rect to clip off parts of the surface outside renderArea
    SDL_RenderSetClipRect(renderer, &renderArea);
    // scale and translate the surface according to the the pan and zoom level
    // the section of the surface enclosed within surfaceRect is mapped to dstRect
    const SDL_Rect dstRect {
        renderArea.x + surfaceRect.x * scale + translationX,
        renderArea.y + surfaceRect.y * scale + translationY,
        surfaceRect.w * scale,
        surfaceRect.h * scale
    };
    SDL_RenderCopy(renderer, texture, nullptr, &dstRect);
    SDL_DestroyTexture(texture);

    // render a mouseover rectangle (if the mouseoverPoint is non-empty)
    if (mouseoverPoint) {
        int32_t gameStateX = extensions::div_floor(mouseoverPoint->x - translationX, scale);
        int32_t gameStateY = extensions::div_floor(mouseoverPoint->y - translationY, scale);

        SDL_Rect mouseoverRect{
            gameStateX * scale + translationX,
            gameStateY * scale + translationY,
            scale,
            scale
        };

        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0x44);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
        SDL_RenderFillRect(renderer, &mouseoverRect);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }

    // draw a square at the top left to denote the live view (TODO: make it a border or something nicer)
    if (liveView) {
        SDL_SetRenderDrawColor(renderer, 0, 0xFF, 0xFF, SDL_ALPHA_OPAQUE);
        SDL_Rect squareBox{
            renderArea.x,
            renderArea.y,
            mainWindow.logicalToPhysicalSize(10),
            mainWindow.logicalToPhysicalSize(10)
        };
        SDL_RenderFillRect(renderer, &squareBox);
    }

    // reset the clip rect
    SDL_RenderSetClipRect(renderer, nullptr);
}


void PlayArea::processMouseMotionEvent(const SDL_MouseMotionEvent& event) {
    // offset relative to top-left of toolbox (in physical size; both event and renderArea are in physical size units)
    int physicalOffsetX = event.x - renderArea.x;
    int physicalOffsetY = event.y - renderArea.y;

    SDL_Point position{event.x, event.y};

    if (drawingIndex && SDL_PointInRect(&position, &renderArea)) {
        int offsetX = physicalOffsetX - translationX;
        int offsetY = physicalOffsetY - translationY;
        offsetX = extensions::div_floor(offsetX, scale);
        offsetY = extensions::div_floor(offsetY, scale);
        MainWindow::tool_tags::get(mainWindow.selectedToolIndices[*drawingIndex], [this, offsetX, offsetY](const auto tool_tag) {
            using Tool = typename decltype(tool_tag)::type;
            if constexpr (std::is_base_of_v<Pencil, Tool>) {
                processDrawingTool<Tool>(offsetX, offsetY);
            }
        });
    }

    // update translation if panning
    if (panning && mouseoverPoint) {
        translationX += physicalOffsetX - mouseoverPoint->x;
        translationY += physicalOffsetY - mouseoverPoint->y;
    }

    // store the new mouseover point
    mouseoverPoint = extensions::point{ physicalOffsetX, physicalOffsetY };
}


void PlayArea::processMouseButtonEvent(const SDL_MouseButtonEvent& event) {
    // offset relative to top-left of toolbox (in physical size; both event and renderArea are in physical size units)
    int physicalOffsetX = event.x - renderArea.x;
    int physicalOffsetY = event.y - renderArea.y;

    // translation:
    int offsetX = physicalOffsetX - translationX;
    int offsetY = physicalOffsetY - translationY;

    // scaling:
    offsetX = extensions::div_floor(offsetX, scale);
    offsetY = extensions::div_floor(offsetY, scale);

    size_t inputHandleIndex = MainWindow::resolveInputHandleIndex(event);
    MainWindow::tool_tags::get(mainWindow.selectedToolIndices[inputHandleIndex], [this, event, offsetX, offsetY, inputHandleIndex](const auto tool_tag) {
        // 'Tool' is the type of tool (e.g. Selector)
        using Tool = typename decltype(tool_tag)::type;

        if constexpr (std::is_base_of_v<Pencil, Tool>) {
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                // If another drawing tool is in use, break that action and start a new one
                if (drawingIndex) {
                    stateManager.saveToHistory();
                }
                drawingIndex = inputHandleIndex;
                processDrawingTool<Tool>(offsetX, offsetY);
            } else {
                // Break the current drawing action if the mouseup is from that tool
                if (inputHandleIndex == drawingIndex) {
                    drawingIndex = std::nullopt;
                    stateManager.saveToHistory();
                }
            }
        }
        else if constexpr (std::is_base_of_v<Selector, Tool>) {
            // it is a Selector.
            // TODO.
        }
        else if constexpr (std::is_base_of_v<Panner, Tool>) {
            // it is a Panner.
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                panning = true;
            } else {
                panning = false;
            }
        }
        else {
            // if we get here, we will get a compiler error
            static_assert(std::negation_v<std::is_same<Tool, Tool>>, "Invalid tool type!");
        }
    });

    // TODO
}

void PlayArea::processMouseWheelEvent(const SDL_MouseWheelEvent& event) {
    if (mouseoverPoint) {
        // change the scale factor,
        // and adjust the translation so that the scaling pivots on the pixel that the mouse is over
        int32_t scrollAmount = (event.direction == SDL_MOUSEWHEEL_NORMAL) ? (event.y) : (-event.y);
        int32_t offsetX = extensions::div_floor(mouseoverPoint->x - translationX + scale / 2, scale); // note: "scale / 2" added so that the division will round to the nearest integer instead of floor
        int32_t offsetY = extensions::div_floor(mouseoverPoint->y - translationY + scale / 2, scale);

        if (scrollAmount > 0) {
            scale++;
            translationX -= offsetX;
            translationY -= offsetY;
        }
        else if (scrollAmount < 0 && scale > 1) {
            scale--;
            translationX += offsetX;
            translationY += offsetY;
        }
    }
}


void PlayArea::processMouseLeave() {
    mouseoverPoint = std::nullopt;
}



void PlayArea::processKeyboardEvent(const SDL_KeyboardEvent& event) {
    // TODO: have a proper UI for toggling views and for live view interactions (start/stop, press button, etc.)
    if (event.type == SDL_KEYDOWN) {
        switch (event.keysym.scancode) { // using the scancode layout so that keys will be in the same position if the user has a non-qwerty keyboard
        case SDL_SCANCODE_T:
            // the 'T' key, used to toggle default/live view
            if (!liveView) {
                liveView = true;
                // if we changed to liveView, recompile the state and start the simulator (TODO: change to something more appropriate after discussions)
                stateManager.resetLiveView();
                stateManager.startSimulator();
            }
            else {
                liveView = false;
                // TODO: change to something more appropriate
                stateManager.stopSimulator();
                stateManager.clearLiveView();
            }
            break;
        }
    }
}