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
    gameState.fillSurface(reinterpret_cast<uint32_t*>(surface->pixels), surfaceRect.x, surfaceRect.y, surfaceRect.w, surfaceRect.h);
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

    // reset the clip rect
    SDL_RenderSetClipRect(renderer, nullptr);
}


void PlayArea::processMouseMotionEvent(const SDL_MouseMotionEvent& event) {
    // offset relative to top-left of toolbox (in physical size; both event and renderArea are in physical size units)
    int physicalOffsetX = event.x - renderArea.x;
    int physicalOffsetY = event.y - renderArea.y;

    
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

    MainWindow::tool_tags::get(mainWindow.selectedToolIndex, [this, event, offsetX, offsetY, physicalOffsetX, physicalOffsetY](const auto tool_tag) {
        // 'Element' is the type of element (e.g. ConductiveWire)
        using Tool = typename decltype(tool_tag)::type;

        if constexpr (std::is_base_of_v<Pencil, Tool>) {
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                extensions::point deltaTrans;

                // if it is a Pencil, forward the drawing to the gamestate
                if constexpr (std::is_base_of_v<Eraser, Tool>) {
                    deltaTrans = gameState.changePixelState<std::monostate>(offsetX, offsetY); // special handling for the eraser
                }
                else {
                    deltaTrans = gameState.changePixelState<Tool>(offsetX, offsetY); // forwarding for the normal elements
                }

                translationX -= deltaTrans.x * scale;
                translationY -= deltaTrans.y * scale;
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
    int32_t scrollAmount = (event.direction == SDL_MOUSEWHEEL_NORMAL) ? (event.y) : (-event.y);
    if (scrollAmount > 0) {
        scale++;
    }
    else if (scrollAmount < 0) {
        scale--;
    }
}


void PlayArea::processMouseLeave() {
    mouseoverPoint = std::nullopt;
}
