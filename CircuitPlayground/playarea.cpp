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

    // render the selection rectangle if it exists
    if (selectorState == Selector::SELECTING) {
        SDL_Rect selectionArea {
            selectionRect.x * scale + translation.x,
            selectionRect.y * scale + translation.y,
            selectionRect.w * scale,
            selectionRect.h * scale
        };

        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0x44);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
        SDL_RenderFillRect(renderer, &selectionArea);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }

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
    // offset relative to top-left of toolbox (in physical size; both event and renderArea are in physical size units)
    extensions::point physicalOffset{ event.x - renderArea.x, event.y - renderArea.y };

    SDL_Point position{event.x, event.y};

    // this check is necessary as PlayArea receives mousemotion events even if the cursor is outside its render area
    if (SDL_PointInRect(&position, &renderArea)) {
        extensions::point offset = computeCanvasCoords(physicalOffset);
        if (drawingIndex) {
            MainWindow::tool_tags_t::get(mainWindow.selectedToolIndices[*drawingIndex], [this, offset](const auto tool_tag) {
                using Tool = typename decltype(tool_tag)::type;
                if constexpr (std::is_base_of_v<Pencil, Tool>) {
                    processDrawingTool<Tool>(offset);
                }
            });
        } else if (selectorState == Selector::SELECTING) {
            selectionRect = getRect(selectionOrigin, offset);
        } else if (selectorState == Selector::MOVING) {
            if (moveOrigin != offset) {
                stateManager.moveSelection(offset.x - moveOrigin.x, offset.y - moveOrigin.y);
                moveOrigin = offset;
            }
        }
    }

    // update translation if panning
    if (panning && mouseoverPoint) {
        translation += physicalOffset - *mouseoverPoint;
    }

    // store the new mouseover point
    mouseoverPoint = physicalOffset;
}


void PlayArea::processMouseButtonEvent(const SDL_MouseButtonEvent& event) {
    // offset relative to top-left of toolbox (in physical size; both event and renderArea are in physical size units)
    extensions::point physicalOffset = extensions::point{ event.x, event.y } - extensions::point{renderArea.x, renderArea.y};

    size_t inputHandleIndex = MainWindow::resolveInputHandleIndex(event);
    MainWindow::tool_tags_t::get(mainWindow.selectedToolIndices[inputHandleIndex], [this, event, physicalOffset, inputHandleIndex](const auto tool_tag) {
        // 'Tool' is the type of tool (e.g. Selector)
        using Tool = typename decltype(tool_tag)::type;

        if constexpr (std::is_base_of_v<Pencil, Tool>) {
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                // If another drawing tool is in use, break that action and start a new one
                finishAction();
                drawingIndex = inputHandleIndex;
                processDrawingTool<Tool>(computeCanvasCoords(physicalOffset));
            } else {
                // Break the current drawing action if the mouseup is from that tool
                if (inputHandleIndex == drawingIndex) {
                    finishAction();
                }
            }
        }
        else if constexpr (std::is_base_of_v<Selector, Tool>) {
            // it is a Selector.
            extensions::point offset = computeCanvasCoords(physicalOffset);
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (selectorState == Selector::SELECTED) {
                    if (!stateManager.pointInSelection(offset)) {
                        finishAction();
                        offset = computeCanvasCoords(physicalOffset);
                    } else {
                        selectorState = Selector::MOVING;
                        moveOrigin = offset;
                    }
                }
                if (selectorState == Selector::INACTIVE) {
                    selectorState = Selector::SELECTING;
                    selectionOrigin = offset;
                    selectionRect = { selectionOrigin.x, selectionOrigin.y, 1, 1 };
                }
            } else {
                if (selectorState == Selector::SELECTING) {
                    selectorState = Selector::SELECTED;
                    SDL_Point position{ event.x, event.y };
                    // TODO: consider using viewports to eliminate these checks
                    if (SDL_PointInRect(&position, &renderArea)) {
                        selectionRect = getRect(selectionOrigin, offset);
                        stateManager.selectRect(selectionRect);
                    }
                } else if (selectorState == Selector::MOVING) {
                    selectorState = Selector::SELECTED;
                }
            }
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
        // note: "scale / 2" added so that the division will round to the nearest integer instead of floor
        extensions::point offset = *mouseoverPoint - translation + extensions::point{ scale/2, scale/2 };
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

void PlayArea::processMouseLeave() {
    mouseoverPoint = std::nullopt;
}

void PlayArea::processKeyboardEvent(const SDL_KeyboardEvent& event) {
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
        case SDL_SCANCODE_D:
        case SDL_SCANCODE_DELETE:
            {
                extensions::point deltaTrans = stateManager.deleteSelection();
                translation -= deltaTrans * scale;
                finishAction();
            }
            break;
        case SDL_SCANCODE_A:
            if (modifiers & KMOD_CTRL) {
                stateManager.selectAll();
                selectorState = Selector::SELECTED;
            }
            break;
        case SDL_SCANCODE_C:
            if (modifiers & KMOD_CTRL) {
                stateManager.copySelectionToClipboard();
            }
            break;
        case SDL_SCANCODE_X:
            if (modifiers & KMOD_CTRL) {
                extensions::point deltaTrans = stateManager.cutSelectionToClipboard();
                translation -= deltaTrans * scale;
                finishAction();
            }
            break;
        case SDL_SCANCODE_V:
            if (modifiers & KMOD_CTRL) {
                finishAction();
                selectorState = Selector::SELECTED;

                extensions::point physicalOffset;
                SDL_GetMouseState(&physicalOffset.x, &physicalOffset.y);
                extensions::point offset = physicalOffset - translation;
                offset = extensions::div_floor(offset, scale);

                SDL_Point position{ physicalOffset.x, physicalOffset.y };
                if (!SDL_PointInRect(&position, &renderArea)) {
                    offset = { 0, 0 };
                }
                stateManager.pasteSelectionFromClipboard(offset.x, offset.y);
            }
            break;
        case SDL_SCANCODE_Y:
            if (modifiers & KMOD_CTRL) {
                finishAction();
                extensions::point deltaTrans = stateManager.redo();
                translation -= deltaTrans * scale;
            }
            break;
        case SDL_SCANCODE_Z:
            if (modifiers & KMOD_CTRL) {
                finishAction();
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
    } else if (event.type == SDL_KEYUP) {
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

void PlayArea::finishAction() {
    drawingIndex = std::nullopt;
    selectorState = Selector::INACTIVE;
    extensions::point deltaTrans = stateManager.commitSelection();
    translation -= deltaTrans * scale;
    stateManager.saveToHistory(); // TODO: check drawingIndex and selectorState to tell if state definitely hasn't changed.
}

SDL_Rect PlayArea::getRect(extensions::point p, extensions::point q) {
    return SDL_Rect {
        std::min(p.x, q.x),
        std::min(p.y, q.y),
        std::abs(p.x - q.x) + 1,
        std::abs(p.y - q.y) + 1,
    };
}
