#include "clipboardmanager.hpp"

void ClipboardManager::generateThumbnail(int32_t index) {
}

CanvasState ClipboardManager::read() const {
    return defaultClipboard;
}

CanvasState ClipboardManager::read(int32_t index) {
    // overwrite the default clipboard too, unless the selected clipboard is empty
    if (!clipboards[index].state.empty()) {
        defaultClipboard = clipboards[index].state;
    }
    return clipboards[index].state;
}

void ClipboardManager::write(const CanvasState& state) {
    defaultClipboard = state;
}

void ClipboardManager::write(const CanvasState& state, int32_t index) {
    defaultClipboard = clipboards[index].state = state;
    generateThumbnail(index);
}

std::array<size_t, NUM_CLIPBOARDS> ClipboardManager::getOrder() const {
    std::array<size_t, NUM_CLIPBOARDS> order;
    for (int i = 0; i < NUM_CLIPBOARDS; ++i) order[i] = i;
    return order;
}
