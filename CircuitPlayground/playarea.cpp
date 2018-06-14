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

PlayArea::PlayArea(MainWindow& main_window) : mainWindow(main_window), currentAction(*this) {};


void PlayArea::updateDpi() {
    // do nothing, because play area works fully in physical pixels and there are no hardcoded logical pixels constants
}


void PlayArea::render(SDL_Renderer* renderer) const {
    // calculate the rectangle (in gamestate coordinates) that we will be drawing:
    SDL_Rect surfaceRect;
    surfaceRect.x = extensions::div_floor(-translation.x, scale);
    surfaceRect.y = extensions::div_floor(-translation.y, scale);
    surfaceRect.w = extensions::div_ceil(renderArea.w - translation.x, scale) - surfaceRect.x;
    surfaceRect.h = extensions::div_ceil(renderArea.h - translation.y, scale) - surfaceRect.y;

    // render the gamestate
    SDL_Surface* surface = SDL_CreateRGBSurface(0, surfaceRect.w, surfaceRect.h, 32, 0x000000FFu, 0x0000FF00u, 0x00FF0000u, 0);
    stateManager.fillSurface(defaultView, reinterpret_cast<uint32_t*>(surface->pixels), surfaceRect.x, surfaceRect.y, surfaceRect.w, surfaceRect.h);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    // set clip rect to clip off parts of the surface outside renderArea
    SDL_RenderSetClipRect(renderer, &renderArea);
    // scale and translate the surface according to the the pan and zoom level
    // the section of the surface enclosed within surfaceRect is mapped to dstRect
    const SDL_Rect dstRect {
        renderArea.x + surfaceRect.x * scale + translation.x,
        renderArea.y + surfaceRect.y * scale + translation.y,
        surfaceRect.w * scale,
        surfaceRect.h * scale
    };
    SDL_RenderCopy(renderer, texture, nullptr, &dstRect);
    SDL_DestroyTexture(texture);

    // render a mouseover rectangle (if the mouseoverPoint is non-empty)
    if (mouseoverPoint) {
        int32_t gameStateX = extensions::div_floor(mouseoverPoint->x - translation.x, scale);
        int32_t gameStateY = extensions::div_floor(mouseoverPoint->y - translation.y, scale);

        SDL_Rect mouseoverRect{
            gameStateX * scale + translation.x,
            gameStateY * scale + translation.y,
            scale,
            scale
        };

        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0x44);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
        SDL_RenderFillRect(renderer, &mouseoverRect);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }

    // ask current action to render itself
    currentAction.render(renderer);
    

    // draw a square at the top left to denote default view (TODO: make it a border or something nicer)
    if (defaultView) {
        SDL_SetRenderDrawColor(renderer, 0, 0xFF, 0xFF, SDL_ALPHA_OPAQUE);
        SDL_Rect squareBox {
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
    
    extensions::point physicalOffset = extensions::point{ event.x, event.y } -extensions::point{ renderArea.x, renderArea.y };

    
    if (!currentAction.processMouseMotionEvent(event)) {
        // at this point, no actions are able to handle this event, so we do the default for playarea

        // update translation if panning
        if (panning && mouseoverPoint) {
            translation += physicalOffset - *mouseoverPoint;
        }
    }

    
    // store the new mouseover point
    mouseoverPoint = physicalOffset;
}


void PlayArea::processMouseButtonEvent(const SDL_MouseButtonEvent& event) {
    
    if (!currentAction.processMouseButtonEvent(event)) {
        // at this point, no actions are able to handle this event, so we do the default for playarea


        // offset relative to top-left of toolbox (in physical size; both event and renderArea are in physical size units)
        extensions::point physicalOffset = extensions::point{ event.x, event.y } -extensions::point{ renderArea.x, renderArea.y };

        size_t inputHandleIndex = MainWindow::resolveInputHandleIndex(event);
        MainWindow::tool_tags_t::get(mainWindow.selectedToolIndices[inputHandleIndex], [this, event, physicalOffset, inputHandleIndex](const auto tool_tag) {
            // 'Tool' is the type of tool (e.g. Selector)
            using Tool = typename decltype(tool_tag)::type;

            if constexpr (std::is_base_of_v<Panner, Tool>) {
                // it is a Panner.
                if (event.type == SDL_MOUSEBUTTONDOWN) {
                    panning = true;
                }
                else {
                    panning = false;
                }
            }
        });

    }

}

void PlayArea::processMouseWheelEvent(const SDL_MouseWheelEvent& event) {
    
    if (!currentAction.processMouseWheelEvent(event)) {
        // at this point, no actions are able to handle this event, so we do the default for playarea

        if (mouseoverPoint) {
            // change the scale factor,
            // and adjust the translation so that the scaling pivots on the pixel that the mouse is over
            int32_t scrollAmount = (event.direction == SDL_MOUSEWHEEL_NORMAL) ? (event.y) : (-event.y);
            // note: "scale / 2" added so that the division will round to the nearest integer instead of floor
            extensions::point offset = *mouseoverPoint - translation + extensions::point{ scale / 2, scale / 2 };
            offset = extensions::div_floor(offset, scale);

            if (scrollAmount > 0) {
                scale++;
                translation -= offset;
            }
            else if (scrollAmount < 0 && scale > 1) {
                scale--;
                translation += offset;
            }
        }

    }

    
}

void PlayArea::processMouseLeave() {
    
    if (!currentAction.processMouseLeave()) {
        // at this point, no actions are able to handle this event, so we do the default for playarea

        // no default mouseleave event

    }

    

    
    mouseoverPoint = std::nullopt;
}

void PlayArea::processKeyboardEvent(const SDL_KeyboardEvent& event) {
    
    if (!currentAction.processKeyboardEvent(event)) {
        // at this point, no actions are able to handle this event, so we do the default for playarea
        
        // TODO: have a proper UI for toggling views and for live view interactions (start/stop, press button, etc.)
        if (event.type == SDL_KEYDOWN) {
            SDL_Keymod modifiers = SDL_GetModState();
            switch (event.keysym.scancode) { // using the scancode layout so that keys will be in the same position if the user has a non-qwerty keyboard
            case SDL_SCANCODE_T:
                // default view is active while T is held
                defaultView = true;
                break;
            case SDL_SCANCODE_R:
                stateManager.resetSimulator();
                break;
            case SDL_SCANCODE_Y:
                if (modifiers & KMOD_CTRL) {
                    currentAction.reset();
                    extensions::point deltaTrans = stateManager.redo();
                    translation -= deltaTrans * scale;
                }
                break;
            case SDL_SCANCODE_Z:
                if (modifiers & KMOD_CTRL) {
                    currentAction.reset();
                    extensions::point deltaTrans = stateManager.undo();
                    translation -= deltaTrans * scale;
                }
                break;
            case SDL_SCANCODE_SPACE:
                stateManager.startOrStopSimulator();
                break;
            default:
                break;
            }
        }
        else if (event.type == SDL_KEYUP) {
            switch (event.keysym.scancode) {
            case SDL_SCANCODE_T:
                // default view is active while T is held
                defaultView = false;
                break;
            default:
                break;
            }
        }

    }
    
    
}
