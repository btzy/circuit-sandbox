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
