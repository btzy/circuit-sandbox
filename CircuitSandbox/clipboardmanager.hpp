#pragma once

#include <utility>
#include <array>
#include "canvasstate.hpp"
#include "point.hpp"
#include "sdl_automatic.hpp"
#include "declarations.hpp"
#include "clipboardstore.hpp"
#include "declarations.hpp"
#include "notificationdisplay.hpp"

struct ClipboardManager {
private:
    struct Clipboard {
        CanvasState state;
        UniqueTexture thumbnail;
    };

    ClipboardStore<NUM_CLIPBOARDS> storage;

    // for displaying notifications
    NotificationDisplay& notificationDisplay;

    NotificationDisplay::UniqueNotification emptyNotification;

public:
    ClipboardManager(NotificationDisplay& notificationDisplay) : notificationDisplay(notificationDisplay) {}

    /**
     * Read from a clipboard. Use the default clipboard if index is not given.
     */
    CanvasState read(int32_t index = 0);

    /**
     * Write to a clipboard. Use the default clipboard if index is not given.
     */
    void write(const CanvasState& state, int32_t index = 0);

    /**
     * Return a permutation of clipboard indices representing the order they should be displayed in.
     */
    std::array<size_t, NUM_CLIPBOARDS> getOrder() const;

    SDL_Texture* getThumbnail(int32_t index);

    void setRenderer(SDL_Renderer* renderer);
};
