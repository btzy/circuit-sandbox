#include <variant>

#include "statemanager.hpp"
#include "visitor.hpp"

void StateManager::fillSurface(bool useLiveView, uint32_t* pixelBuffer, int32_t left, int32_t top, int32_t width, int32_t height) const {

    // This is kinda yucky, but I can't think of a better way such that:
    // 1) If useLiveView is false, we don't make a *copy* of gameState
    // 2) If useLiveView is true, we take a snapshot from the simulator and use it
    auto lambda = [this, &pixelBuffer, left, top, width, height](const GameState& renderGameState) {
        for (int32_t y = top; y != top + height; ++y) {
            for (int32_t x = left; x != left + width; ++x) {
                uint32_t color = 0;

                // check if the requested pixel inside the buffer
                if (x >= 0 && x < renderGameState.dataMatrix.width() && y >= 0 && y < renderGameState.dataMatrix.height()) {
                    std::visit(visitor{
                        [&color](std::monostate) {},
                        [&color](const auto& element) {
                            // 'Element' is the type of tool (e.g. ConductiveWire)
                            //using Element = decltype(element);

                            SDL_Color computedColor = computeDisplayColor(element);
                            color = computedColor.r | (computedColor.g << 8) | (computedColor.b << 16);
                        },
                    }, renderGameState.dataMatrix[{x, y}]);
                }
                *pixelBuffer++ = color;
            }
        }
    };

    if (useLiveView) {
        lambda(simulator.takeSnapshot());
    }
    else {
        lambda(gameState);
    }


}

void StateManager::saveToHistory() {
    // TODO
}



void StateManager::resetLiveView() {
    simulator.compile(gameState);
}


void StateManager::startSimulator() {
    simulator.start();
}


void StateManager::stopSimulator() {
    simulator.stop();
}


void StateManager::clearLiveView() {
    simulator.clear();
}
