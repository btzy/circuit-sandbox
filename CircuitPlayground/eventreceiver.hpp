#pragma once

#include <SDL.h>

/**
 * Base class for classes that can receive the keyboard/mouse events
 */

class EventReceiver {
public:
    virtual void processMouseHover(const SDL_MouseMotionEvent&) {} // fires when mouse is inside the renderArea
    virtual void processMouseLeave() {} // fires once when mouse goes out of the renderArea

    virtual void processMouseButtonDown(const SDL_MouseButtonEvent&) {} // fires when the mouse is pressed down, must be inside the renderArea
    virtual void processMouseDrag(const SDL_MouseMotionEvent&) {} // fires when mouse is moved, and was previously pressed down
    virtual void processMouseButtonUp(const SDL_MouseButtonEvent&) {} // fires when the mouse is lifted up, might be outside the renderArea

    virtual void processMouseWheel(const SDL_MouseWheelEvent&) {} // fires when the mouse wheel is rotated while the mouse is in the renderArea
    virtual void processKeyboard(const SDL_KeyboardEvent&) {} // fires when a keyboard button is pressed
};
