/*
 * Circuit Sandbox
 * Copyright 2018 National University of Singapore <enterprise@nus.edu.sg>
 *
 * This file is part of Circuit Sandbox.
 * Circuit Sandbox is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 * Circuit Sandbox is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with Circuit Sandbox.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <chrono>
#include "clipboardmanager.hpp"

using namespace std::literals;

CanvasState ClipboardManager::read(int32_t index) {
    // overwrite the default clipboard too, unless the selected clipboard is empty
    CanvasState state = storage.read(index);
    if (index) {
        if (state.empty()) {
            emptyNotification = notificationDisplay.uniqueAdd(NotificationFlags::DEFAULT, 5s, NotificationDisplay::Data{ { "Clipboard empty", NotificationDisplay::TEXT_COLOR_ERROR } });
        }
        else {
            NotificationDisplay::Data notificationData{
                { "Pasted", NotificationDisplay::TEXT_COLOR_ACTION },
                { " from clipboard", NotificationDisplay::TEXT_COLOR },
                { ' ' + std::to_string(index), NotificationDisplay::TEXT_COLOR_KEY }
            };
            notificationDisplay.add(NotificationFlags::DEFAULT, 5s, notificationData);
            storage.write(0, state);
        }
    }
    return state;
}

void ClipboardManager::write(const CanvasState& state, int32_t index) {
    storage.write(index, state);
    if (index) {
        // also write to the default clipboard
        storage.write(0, state);
        NotificationDisplay::Data notificationData {
            { "Copied", NotificationDisplay::TEXT_COLOR_ACTION },
            { " to clipboard", NotificationDisplay::TEXT_COLOR },
            { ' ' + std::to_string(index), NotificationDisplay::TEXT_COLOR_KEY }
        };
        notificationDisplay.add(NotificationFlags::DEFAULT, 5s, notificationData);
    }
}

std::array<size_t, NUM_CLIPBOARDS> ClipboardManager::getOrder() const {
    std::array<size_t, NUM_CLIPBOARDS> order;
    for (int i = 0; i < NUM_CLIPBOARDS; ++i) order[i] = i;
    return order;
}

SDL_Texture* ClipboardManager::getThumbnail(int32_t index) {
    return storage.getThumbnail(index);
}

void ClipboardManager::setRenderer(SDL_Renderer* renderer) {
    storage.setRenderer(renderer);
}
