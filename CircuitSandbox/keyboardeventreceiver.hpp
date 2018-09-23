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

/**
 * Base class for classes that can receive the keyboard events.
 */

class KeyboardEventReceiver {
public:
    // return 'true' to indicate that the event should not be propagated to other EventReceivers
    virtual bool processKeyboard(const SDL_KeyboardEvent&) { return false; } // fires when a keyboard button is pressed
    virtual bool processTextInput(const SDL_TextInputEvent&) { return false; } // fires when text was inputted
};
