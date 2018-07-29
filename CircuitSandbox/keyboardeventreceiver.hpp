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
