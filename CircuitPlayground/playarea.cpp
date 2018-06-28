#include <type_traits>
#include <tuple>
#include <cstdint>

#include <SDL.h>

#include "playarea.hpp"
#include "mainwindow.hpp"
#include "elements.hpp"
#include "integral_division.hpp"
#include "point.hpp"
#include "playareaaction.hpp"

PlayArea::PlayArea(MainWindow& main_window) : mainWindow(main_window), currentAction(mainWindow.currentAction, mainWindow, *this) {};


void PlayArea::render(SDL_Renderer* renderer) {
    render(renderer, mainWindow.stateManager);
}
void PlayArea::render(SDL_Renderer* renderer, StateManager& stateManager) {
    // calculate the rectangle (in gamestate coordinates) that we will be drawing:
    SDL_Rect surfaceRect;
    surfaceRect.x = ext::div_floor(-translation.x, scale);
    surfaceRect.y = ext::div_floor(-translation.y, scale);
    surfaceRect.w = ext::div_ceil(renderArea.w - translation.x, scale) - surfaceRect.x;
    surfaceRect.h = ext::div_ceil(renderArea.h - translation.y, scale) - surfaceRect.y;

    // render the gamestate
    SDL_Surface* surface = SDL_CreateRGBSurface(0, surfaceRect.w, surfaceRect.h, 32, 0x000000FFu, 0x0000FF00u, 0x00FF0000u, 0);
    if (!currentAction.disablePlayAreaDefaultRender()) {
        stateManager.fillSurface(defaultView, reinterpret_cast<uint32_t*>(surface->pixels), surfaceRect.x, surfaceRect.y, surfaceRect.w, surfaceRect.h);
    }
    // ask current action to render pixels to the surface if necessary
    currentAction.renderPlayAreaSurface(reinterpret_cast<uint32_t*>(surface->pixels), surfaceRect);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
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
        int32_t gameStateX = ext::div_floor(mouseoverPoint->x - translation.x, scale);
        int32_t gameStateY = ext::div_floor(mouseoverPoint->y - translation.y, scale);

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

    // ask current action to render itself directly if necessary
    currentAction.renderPlayAreaDirect(renderer);

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
}


void PlayArea::processMouseHover(const SDL_MouseMotionEvent& event) {

    ext::point physicalOffset = ext::point{ event.x, event.y } -ext::point{ renderArea.x, renderArea.y };

    // store the new mouseover point
    mouseoverPoint = physicalOffset;
}

void PlayArea::processMouseLeave() {
    mouseoverPoint = std::nullopt;
}

bool PlayArea::processMouseButtonDown(const SDL_MouseButtonEvent& event) {

    if (!currentAction.processPlayAreaMouseButtonDown(event)) {
        // at this point, no actions are able to handle this event, so we do the default for playarea

        // offset relative to top-left of toolbox (in physical size; both event and renderArea are in physical size units)
        ext::point physicalOffset = ext::point{ event.x, event.y } -ext::point{ renderArea.x, renderArea.y };

        size_t inputHandleIndex = resolveInputHandleIndex(event);
        tool_tags_t::get(mainWindow.selectedToolIndices[inputHandleIndex], [this, &event, &physicalOffset](const auto tool_tag) {
            // 'Tool' is the type of tool (e.g. Selector)
            using Tool = typename decltype(tool_tag)::type;

            if constexpr (std::is_base_of_v<Panner, Tool>) {
                if (event.clicks == 1) {
                    // if single click, set pan origin
                    panOrigin = physicalOffset;
                }
                else if (event.clicks == 2) {
                    // if double click, center viewport at clicked position
                    translation += (ext::div_floor(ext::point{ renderArea.w/2, renderArea.h/2 }, scale) - ext::div_floor(physicalOffset, scale)) * scale;
                }
            }
        });

    }
    return true;

}

void PlayArea::processMouseDrag(const SDL_MouseMotionEvent& event) {

    if (!currentAction.processPlayAreaMouseDrag(event)) {
        // at this point, no actions are able to handle this event, so we do the default for playarea

        // update translation if panning
        if (panOrigin) {
            ext::point physicalOffset = ext::point{ event.x, event.y } - ext::point{ renderArea.x, renderArea.y };
            translation += physicalOffset - *panOrigin;
            panOrigin = physicalOffset;
        }
    }

}


void PlayArea::processMouseButtonUp() {

    if (!currentAction.processPlayAreaMouseButtonUp()) {
        // at this point, no actions are able to handle this event, so we do the default for playarea

        if (panOrigin) { // the panner is active
            panOrigin = std::nullopt;
        }
    }

}

bool PlayArea::processMouseWheel(const SDL_MouseWheelEvent& event) {

    if (!currentAction.processPlayAreaMouseWheel(event)) {
        // at this point, no actions are able to handle this event, so we do the default for playarea

        if (mouseoverPoint) {
            // change the scale factor,
            // and adjust the translation so that the scaling pivots on the pixel that the mouse is over
            int32_t scrollAmount = (event.direction == SDL_MOUSEWHEEL_NORMAL) ? (event.y) : (-event.y);
            // note: "scale / 2" added so that the division will round to the nearest integer instead of floor
            ext::point offset = *mouseoverPoint - translation + ext::point{ scale / 2, scale / 2 };
            offset = ext::div_floor(offset, scale);

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
    return true;
}

bool PlayArea::processKeyboard(const SDL_KeyboardEvent& event) {

    if (event.type == SDL_KEYDOWN) {
        //SDL_Keymod modifiers = SDL_GetModState();
        switch (event.keysym.scancode) { // using the scancode layout so that keys will be in the same position if the user has a non-qwerty keyboard
        case SDL_SCANCODE_T:
            // default view is active while T is held
            defaultView = true;
            return true;
        default:
            break;
        }
    }
    else if (event.type == SDL_KEYUP) {
        switch (event.keysym.scancode) {
        case SDL_SCANCODE_T:
            // default view is active while T is held
            defaultView = false;
            return true;
        default:
            break;
        }
    }
    return false;
}