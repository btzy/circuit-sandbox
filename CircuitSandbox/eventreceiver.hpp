/*
 * Circuit Sandbox
 * Copyright 2018 National University of Singapore <enterprise@nus.edu.sg>
 *
 * This file is part of Circuit Sandbox.
 * Circuit Sandbox is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 * Circuit Sandbox is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with Circuit Sandbox.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <SDL.h>
#include "keyboardeventreceiver.hpp"

/**
 * Base class for classes that can receive the keyboard/mouse events.
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
