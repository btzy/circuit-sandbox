#pragma once

#include <SDL.h>
#include "keyboardeventreceiver.hpp"

/**
 * Base class for classes that can receive the keyboard/mouse events
 */

class EventReceiver : public KeyboardEventReceiver {
public:
    // return 'true' to indicate that the event should not be propagated to other EventReceivers
   
    virtual void processMouseHover(const SDL_MouseMotionEvent&) {} // fires when mouse is inside the renderArea
    virtual void processMouseLeave() {} // fires once when mouse goes out of the renderArea

    virtual bool processMouseButtonDown(const SDL_MouseButtonEvent&) { return false; } // fires when the mouse is pressed down, must be inside the renderArea
    virtual void processMouseDrag(const SDL_MouseMotionEvent&) {} // fires when mouse is moved, and was previously pressed down
    virtual void processMouseButtonUp() {} // fires when the mouse is lifted up, might be outside the renderArea

    virtual bool processMouseWheel(const SDL_MouseWheelEvent&) { return false; } // fires when the mouse wheel is rotated while the mouse is in the renderArea
};
