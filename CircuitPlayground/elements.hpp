#pragma once

#include <SDL.h>

/**
 * This header file contains the definitions for all the elements in the game.
 */

struct Pencil {}; // base class for normal elements and the eraser
struct Element : public Pencil {}; // base class for normal elements

/**
 * All tools "Tool" must satisfy the following:
 * 1. Tool::displayColor is a SDL_Color (the display color when drawn on screen)
 * 2. Tool::displayName is a const char* (the name displayed in the toolbox)
 */

struct Selector {
    static constexpr SDL_Color displayColor{ 0xFF, 0xFF, 0xFF, 0xFF };
    static constexpr const char* displayName = "Selector";
};

struct Eraser : public Pencil {
    static constexpr SDL_Color displayColor{ 0xFF, 0xFF, 0xFF, 0xFF };
    static constexpr const char* displayName = "Eraser";
};

struct ConductiveWire : public Element {
    static constexpr SDL_Color displayColor{0x99, 0x99, 0x99, 0xFF};
    static constexpr const char* displayName = "Conductive Wire";
};

struct InsulatedWire : public Element {
    static constexpr SDL_Color displayColor{0xCC, 0x99, 0x66, 0xFF};
    static constexpr const char* displayName = "Insulated Wire";
};
