#include <chrono>
#include "clipboardmanager.hpp"
#include "notificationdisplay.hpp"

using namespace std::literals;

CanvasState ClipboardManager::read(int32_t index) {
    // overwrite the default clipboard too, unless the selected clipboard is empty
    CanvasState state = storage.read(index);
    if (index && !state.empty()) {
        notificationDisplay.add(NotificationFlags::DEFAULT, 5s, NotificationDisplay::Data{ { "Pasted from clipboard", NotificationDisplay::TEXT_COLOR_ACTION }, { ' ' + std::to_string(index), NotificationDisplay::TEXT_COLOR_KEY } });
        storage.write(0, state);
    }
    return state;
}

void ClipboardManager::write(const CanvasState& state, int32_t index) {
    storage.write(index, state);
    if (index) {
        // also write to the default clipboard
        storage.write(0, state);
        notificationDisplay.add(NotificationFlags::DEFAULT, 5s, NotificationDisplay::Data{ { "Saved to clipboard", NotificationDisplay::TEXT_COLOR_ACTION }, { ' ' + std::to_string(index), NotificationDisplay::TEXT_COLOR_KEY } });
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
