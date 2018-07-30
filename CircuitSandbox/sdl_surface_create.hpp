#pragma once

#include <cstdint>
#include <SDL.h>

inline SDL_Surface* create_alpha_surface(int32_t width, int32_t height) {
    // copied from https://wiki.libsdl.org/SDL_CreateRGBSurface

    Uint32 rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    rmask = 0xff000000;
    gmask = 0x00ff0000;
    bmask = 0x0000ff00;
    amask = 0x000000ff;
#else
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = 0xff000000;
#endif
    return SDL_CreateRGBSurface(0, width, height, 32, rmask, gmask, bmask, amask);
}

inline SDL_Surface* create_surface(int32_t width, int32_t height) {
    return SDL_CreateRGBSurface(0, width, height, 32, 0, 0, 0, 0);
}
