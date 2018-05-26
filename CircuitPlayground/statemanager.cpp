#include <variant>
#include <iostream>
#include <fstream>

#include "statemanager.hpp"
#include "visitor.hpp"

StateManager::StateManager() {
    // read dataMatrix from disk if a savefile exists, otherwise it is initialized to std::monostate by default
    readSave();
}

StateManager::~StateManager() {
    // autosave dataMatrix to disk
    writeSave();
}

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

void StateManager::readSave() {
    std::ifstream saveFile(savePath, std::ios::binary);
    if (!saveFile.is_open()) return;

    int32_t matrixWidth, matrixHeight;
    saveFile.read(reinterpret_cast<char*>(&matrixWidth), sizeof matrixWidth);
    saveFile.read(reinterpret_cast<char*>(&matrixHeight), sizeof matrixHeight);

    gameState.dataMatrix = typename GameState::matrix_t(matrixWidth, matrixHeight);

    for (int32_t y = 0; y != gameState.dataMatrix.height(); ++y) {
        for (int32_t x = 0; x != gameState.dataMatrix.width(); ++x) {
            size_t element_index;
            saveFile.read(reinterpret_cast<char*>(&element_index), sizeof element_index);

            GameState::element_variant_t element;
            GameState::element_tags::get(element_index, [&element](const auto element_tag) {
                using Element = typename decltype(element_tag)::type;
                element = Element();
            });
            gameState.dataMatrix[{x, y}] = element;
        }
    }
}

void StateManager::writeSave() {
    std::ofstream saveFile(savePath, std::ios::binary);
    if (!saveFile.is_open()) return;

    int32_t matrixWidth = gameState.dataMatrix.width();
    int32_t matrixHeight = gameState.dataMatrix.height();
    saveFile.write(reinterpret_cast<char*>(&matrixWidth), sizeof matrixWidth);
    saveFile.write(reinterpret_cast<char*>(&matrixHeight), sizeof matrixHeight);

    for (int32_t y = 0; y != gameState.dataMatrix.height(); ++y) {
        for (int32_t x = 0; x != gameState.dataMatrix.width(); ++x) {
            GameState::element_variant_t element = gameState.dataMatrix[{x, y}];
            GameState::element_tags::for_each([&element, &saveFile](const auto element_tag, const auto index_tag) {
                size_t index = element.index();
                if (index == decltype(index_tag)::value) {
                    saveFile.write(reinterpret_cast<char*>(&index), sizeof index);
                }
            });
        }
    }
}
