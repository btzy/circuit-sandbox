#pragma once

/**
 * This header file contains the definitions for all the elements in the game.
 */

/**
 * All elements "Element" must satisfy the following:
 * 1. Element::displayColor is a SDL_Color (the display color when drawn on screen)
 * 2. Element::displayName is a const char* (the name displayed in the toolbox)
 */

struct ConductiveWire {
    static constexpr SDL_Color displayColor{0x99, 0x99, 0x99, 0xFF};
    static constexpr const char* displayName = "Conductive Wire";
};

struct InsulatedWire {
    static constexpr SDL_Color displayColor{0xCC, 0x99, 0x66, 0xFF};
    static constexpr const char* displayName = "Insulated Wire";
};
