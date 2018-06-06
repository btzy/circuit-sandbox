#include <variant>
#include <iostream>
#include <fstream>

#include "statemanager.hpp"
#include "visitor.hpp"

StateManager::StateManager() {
    // read dataMatrix from disk if a savefile exists, otherwise it is initialized to std::monostate by default
    readSave();
    saveToHistory();
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
                            SDL_Color computedColor = computeDisplayColor(element);
                            color = computedColor.r | (computedColor.g << 8) | (computedColor.b << 16);
                        },
                    }, renderGameState.dataMatrix[{x, y}]);

                    int32_t select_x = x - gameState.selectionX;
                    int32_t select_y = y - gameState.selectionY;
                    if (select_x >= 0 && select_x < gameState.selection.width() && select_y >= 0 && select_y < gameState.selection.height()) {
                        if (!std::holds_alternative<std::monostate>(gameState.selection[{select_x, select_y}])) {
                            color |= 0xFF0000;
                        }
                    }
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

bool StateManager::checkIfChanged() {
    if (history.size() == 0 ||
        gameState.dataMatrix.width() != history[historyIndex].dataMatrix.width() ||
        gameState.dataMatrix.height() != history[historyIndex].dataMatrix.height()) {
        gameState.changed = true;
        return true;
    }
    for (int32_t y = 0; y < gameState.dataMatrix.height(); ++y) {
        for (int32_t x = 0; x < gameState.dataMatrix.width(); ++x) {
            if (gameState.dataMatrix[{x, y}].index() != history[historyIndex].dataMatrix[{x, y}].index()) {
                gameState.changed = true;
                return true;
            }
        }
    }
    gameState.changed = false;
    return false;
}

void StateManager::saveToHistory() {
    // don't write to the undo stack if the gameState did not change
    if (!gameState.changed) return;

    if (historyIndex+1 < history.size()) {
        history.resize(historyIndex+1);
    }
    gameState.changed = false;
    history.push_back(gameState);
    gameState.deltaTrans = { 0, 0 };
    historyIndex++;
}

extensions::point StateManager::undo() {
    if (historyIndex == 0) return { 0, 0 };
    extensions::point deltaTrans = history[historyIndex].deltaTrans;

    // apply the inverse translation
    deltaTrans.x = -deltaTrans.x;
    deltaTrans.y = -deltaTrans.y;

    historyIndex--;
    gameState = history[historyIndex];
    if (simulator.holdsSimulation()) {
        if (simulator.running()) simulator.stop();
        simulator.compile(gameState);
        simulator.start();
    }
    gameState.deltaTrans = { 0, 0 };
    return deltaTrans;
}

extensions::point StateManager::redo() {
    if (historyIndex == history.size() - 1) return { 0, 0 };
    historyIndex++;
    gameState = history[historyIndex];
    if (simulator.holdsSimulation()) {
        if (simulator.running()) simulator.stop();
        simulator.compile(gameState);
        simulator.start();
    }
    extensions::point deltaTrans = gameState.deltaTrans;
    gameState.deltaTrans = { 0, 0 };
    return deltaTrans;
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
            GameState::element_tags_t::get(element_index, [&element](const auto element_tag) {
                using Element = typename decltype(element_tag)::type;
                element = Element();
            });
            gameState.dataMatrix[{x, y}] = element;
        }
    }
    gameState.changed = true;
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
            GameState::element_tags_t::for_each([&element, &saveFile](const auto element_tag, const auto index_tag) {
                size_t index = element.index();
                if (index == decltype(index_tag)::value) {
                    saveFile.write(reinterpret_cast<char*>(&index), sizeof index);
                }
            });
        }
    }
}

void StateManager::selectRect(SDL_Rect selectionRect) {
    gameState.selectRect(selectionRect);
}

void StateManager::selectAll() {
    gameState.selectAll();
}

bool StateManager::pointInSelection(int32_t x, int32_t y) {
    return gameState.pointInSelection(x, y);
}

void StateManager::clearSelection() {
    // TODO: try to reduce these calls
    checkIfChanged();
    gameState.clearSelection();
}

extensions::point StateManager::moveSelection(int32_t dx, int32_t dy) {
    extensions::point translation = gameState.moveSelection(dx, dy);

    // update the simulator after moving
    if (simulator.holdsSimulation()) {
        if (simulator.running()) simulator.stop();
        simulator.compile(gameState);
        simulator.start();
    }

    return translation;
}

extensions::point StateManager::deleteSelection() {
    extensions::point translation = gameState.deleteSelection();

    // update the simulator after deleting
    if (simulator.holdsSimulation()) {
        if (simulator.running()) simulator.stop();
        simulator.compile(gameState);
        simulator.start();
    }

    return translation;
}

void StateManager::copy() {
    gameState.copySelectionToClipboard();
}

extensions::point StateManager::cut() {
    extensions::point translation = gameState.deleteSelection();
    gameState.copySelectionToClipboard();

    if (simulator.holdsSimulation()) {
        if (simulator.running()) simulator.stop();
        simulator.compile(gameState);
        simulator.start();
    }

    return translation;
}

extensions::point StateManager::paste(int32_t x, int32_t y) {
    extensions::point translation = gameState.pasteSelection(x, y);

    if (simulator.holdsSimulation()) {
        if (simulator.running()) simulator.stop();
        simulator.compile(gameState);
        simulator.start();
    }

    return translation;
}
