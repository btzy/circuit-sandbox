#pragma once

/**
 * Automatic memory management for SDL pointers.
 */

#include <memory>
#include <SDL.h>

struct TextureDeleter {
    void operator()(SDL_Texture* ptr) const noexcept {
        SDL_DestroyTexture(ptr);
    }
};

using UniqueTexture = std::unique_ptr<SDL_Texture, TextureDeleter>;
