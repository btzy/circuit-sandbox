#include <type_traits>
#include <tuple>
#include <cstdint>
#include <stdexcept>
#include <variant>

#include <SDL.h>

#include "playarea.hpp"
#include "mainwindow.hpp"
#include "elements.hpp"
#include "integral_division.hpp"
#include "point.hpp"
#include "playareaaction.hpp"
#include "sdl_fast_maprgb.hpp"
#include "notificationdisplay.hpp"

PlayArea::PlayArea(MainWindow& main_window) : mainWindow(main_window), currentAction(mainWindow.currentAction, mainWindow, *this) {}


void PlayArea::render(SDL_Renderer* renderer, Drawable::RenderClock::time_point) {
    render(renderer, mainWindow.stateManager);
}
void PlayArea::render(SDL_Renderer* renderer, StateManager& stateManager) {
    // calculate the rectangle (in canvas coordinates) that we will be drawing:
    SDL_Rect surfaceRect;
    surfaceRect.x = ext::div_floor(-translation.x, scale);
    surfaceRect.y = ext::div_floor(-translation.y, scale);
    surfaceRect.w = pixelTextureSize.x;
    surfaceRect.h = pixelTextureSize.y;

    // lock the texture so we can start drawing onto it
    uint32_t* pixelData;
    int32_t pitch;
    SDL_LockTexture(pixelTexture.get(), nullptr, &reinterpret_cast<void*&>(pixelData), &pitch);
    pitch >>= 2; // divide by 4, because uint32_t is 4 bytes

    // render the gamestate
    if (!currentAction.disablePlayAreaDefaultRender()) {
        stateManager.fillSurface(defaultView, pixelData, pixelFormat, surfaceRect, pitch);
    }
    // ask current action to render pixels to the surface if necessary
    currentAction.renderPlayAreaSurface(pixelData, pixelFormat, surfaceRect, pitch);

    // unlock the texture
    SDL_UnlockTexture(pixelTexture.get());

    // scale and translate the surface according to the the pan and zoom level
    // the section of the surface enclosed within surfaceRect is mapped to dstRect
    const SDL_Rect dstRect {
        renderArea.x + surfaceRect.x * scale + translation.x,
        renderArea.y + surfaceRect.y * scale + translation.y,
        surfaceRect.w * scale,
        surfaceRect.h * scale
    };
    SDL_RenderCopy(renderer, pixelTexture.get(), nullptr, &dstRect);

    
    if (mouseoverPoint) {
        ext::point canvasPoint = canvasFromWindowOffset(*mouseoverPoint);

        // render a mouseover rectangle (if the mouseoverPoint is non-empty)
        SDL_Rect mouseoverRect{
            canvasPoint.x * scale + translation.x,
            canvasPoint.y * scale + translation.y,
            scale,
            scale
        };
        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0x44);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
        SDL_RenderFillRect(renderer, &mouseoverRect);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        // set the element description
        changeMouseoverElement(mainWindow.stateManager.getElementAtPoint(canvasPoint));
    }

    // ask current action to render itself directly if necessary
    currentAction.renderPlayAreaDirect(renderer);
}

void PlayArea::prepareTexture(SDL_Renderer* renderer) {
    // free the old texture first (if any)
    pixelTexture.reset(nullptr);
    
    // calculate the required texture size - the maximum size necessary for *any* possible translation.
    pixelTextureSize.x = (renderArea.w - 2) / scale + 2;
    pixelTextureSize.y = (renderArea.h - 2) / scale + 2;
    
    pixelTexture.reset(create_fast_texture(renderer, SDL_TEXTUREACCESS_STREAMING, pixelTextureSize, pixelFormat));
    if (pixelTexture == nullptr) {
        throw std::runtime_error("Renderer does not support any 32-bit ARGB textures!");
    }
}

void PlayArea::layoutComponents(SDL_Renderer* renderer) {
    // reset the description
    changeMouseoverElement(std::monostate{});
    // prepare a new backing texture
    prepareTexture(renderer);
}

void PlayArea::initScale() {
    scale = mainWindow.logicalToPhysicalSize(20);
};


void PlayArea::changeMouseoverElement(const CanvasState::element_variant_t& newElement) {
    Description::ElementVariant_t descElement = Description::fromElementVariant(newElement);
    if (mouseoverElement != descElement) {
        mouseoverElement = std::move(descElement);
        std::visit([&](const auto& element) {
            if constexpr (std::is_base_of_v<Element, std::decay_t<decltype(element)>>) {
                element.setDescription([&](auto&&... args) {
                    mainWindow.buttonBar.setDescription(std::forward<decltype(args)>(args)...);
                });
            }
            else {
                mainWindow.buttonBar.clearDescription();
            }
        }, mouseoverElement);
    }
}


void PlayArea::processMouseHover(const SDL_MouseMotionEvent& event) {

    ext::point physicalOffset = ext::point{ event.x, event.y } -ext::point{ renderArea.x, renderArea.y };

    // store the new mouseover point
    mouseoverPoint = physicalOffset;
    currentAction.processPlayAreaMouseHover(event);
}

void PlayArea::processMouseLeave() {
    mouseoverPoint = std::nullopt;

    currentAction.processPlayAreaMouseLeave();
}

bool PlayArea::processMouseButtonDown(const SDL_MouseButtonEvent& event) {

    if (!currentAction.processPlayAreaMouseButtonDown(event)) {
        // at this point, no actions are able to handle this event, so we do the default for playarea

        // offset relative to top-left of toolbox (in physical size; both event and renderArea are in physical size units)
        ext::point physicalOffset = ext::point{ event.x, event.y } -ext::point{ renderArea.x, renderArea.y };

        size_t inputHandleIndex = resolveInputHandleIndex(event);
        return tool_tags_t::get(mainWindow.selectedToolIndices[inputHandleIndex], [this, &event, &physicalOffset](const auto tool_tag) {
            // 'Tool' is the type of tool (e.g. Selector)
            using Tool = typename decltype(tool_tag)::type;

            if constexpr (std::is_base_of_v<Panner, Tool>) {
                if (event.clicks == 1) {
                    // if single click, set pan origin
                    panOrigin = physicalOffset;
                }
                else if (event.clicks == 2) {
                    // if double click, center viewport at clicked position
                    translation += ext::point{ renderArea.w / 2, renderArea.h / 2 } - physicalOffset;
                }
                return true;
            }
            else {
                return false;
            }
        }, false);
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
                prepareTexture(mainWindow.renderer);
            }
            else if (scrollAmount < 0 && scale > 1) {
                scale--;
                translation += offset;
                prepareTexture(mainWindow.renderer);
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
            if (!event.repeat) {
                // default view is active while T is held
                defaultView = true;
                defaultViewNotification = mainWindow.getNotificationDisplay().uniqueAdd(NotificationFlags::DEFAULT, NotificationDisplay::Data{ { "Viewing starting state", NotificationDisplay::TEXT_COLOR_STATE } });
            }
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
            defaultViewNotification.reset();
            return true;
        default:
            break;
        }
    }
    return false;
}
