#pragma once

#include <utility>
#include <array>
#include "canvasstate.hpp"
#include "point.hpp"
#include "sdl_automatic.hpp"
#include "declarations.hpp"

struct ClipboardManager {
private:
    struct Clipboard {
        CanvasState state;
        UniqueTexture thumbnail;
    };

    CanvasState defaultClipboard;
    std::array<Clipboard, NUM_CLIPBOARDS> clipboards;

    /**
     * Generate the thumbnail to be used in the clipboard action interface.
     */
    void generateThumbnail(int32_t index, SDL_Renderer* renderer);

public:
    /**
     * Read from a clipboard. Use the default clipboard if index is not given.
     */
    CanvasState read() const;
    CanvasState read(int32_t index);

    /**
     * Write to a clipboard. Use the default clipboard if index is not given.
     */
    void write(const CanvasState& state);
    void write(const CanvasState& state, int32_t index, SDL_Renderer* renderer);

    /**
     * Return a permutation of clipboard indices representing the order they should be displayed in.
     */
    std::array<size_t, NUM_CLIPBOARDS> getOrder() const;

    SDL_Texture* getThumbnail(int32_t index) const;
};
