#pragma once

#include <SDL.h>

struct TextureDeleter {
    void operator()(SDL_Texture* ptr) const noexcept {
        SDL_DestroyTexture(ptr);
    }
};
