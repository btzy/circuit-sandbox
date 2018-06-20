#include <cassert>
#include <variant>
#include <iostream>
#include <fstream>

#include "statemanager.hpp"
#include "visitor.hpp"

StateManager::StateManager() {
    // read defaultState from disk if a savefile exists
    readSave();
    simulator.compile(defaultState, false);
    
}

StateManager::~StateManager() {
    // autosave defaultState to disk
    writeSave();
}

void StateManager::fillSurface(bool useDefaultView, uint32_t* pixelBuffer, int32_t left, int32_t top, int32_t width, int32_t height) {
    if (simulator.running()) {
        updateDefaultState();
    }
    for (int32_t y = top; y != top + height; ++y) {
        for (int32_t x = left; x != left + width; ++x) {
            uint32_t color = 0;
            const extensions::point canvasPt{ x, y };

            // check if the requested pixel is inside the buffer
            if (defaultState.contains(canvasPt)) {
                SDL_Color computedColor{ 0, 0, 0, 0 };
                std::visit(visitor{
                    [](std::monostate) {},
                    [&computedColor, useDefaultView](const auto& element) {
                        computedColor = computeDisplayColor(element, useDefaultView);
                    },
                }, defaultState.dataMatrix[{x, y}]);
                color = computedColor.r | (computedColor.g << 8) | (computedColor.b << 16);
            }
            *pixelBuffer++ = color;
        }
    }
}

bool StateManager::evaluateChangedState() {
    // return immediately if it isn't indeterminate
    if (changed != boost::indeterminate) {
        return changed;
    }

    const CanvasState& currentState = historyManager.currentState();
    if (defaultState.width() != currentState.width() ||
        defaultState.height() != currentState.height()) {

        changed = true;
        return true;
    }
    for (int32_t y = 0; y < defaultState.height(); ++y) {
        for (int32_t x = 0; x < defaultState.width(); ++x) {
            if (defaultState[{x, y}].index() != currentState[{x, y}].index()) {
                changed = true;
                return true;
            }
        }
    }
    changed = false;
    return false;
}

void StateManager::reloadSimulator() {
    if (simulator.holdsSimulation()) {
        bool simulatorRunning = simulator.running();
        if (simulatorRunning) simulator.stop();
        simulator.compile(defaultState, false);
        if (simulatorRunning) simulator.start();
    }
}

void StateManager::saveToHistory() {
    // check if changed if necessary
    evaluateChangedState();

    // don't write to the undo stack if defaultState did not change
    if (!changed) return;

    historyManager.saveToHistory(defaultState, deltaTrans);

    changed = false;
    deltaTrans = { 0, 0 };
}

void StateManager::startSimulator() {
    simulator.start();
}

void StateManager::startOrStopSimulator() {
    bool simulatorRunning = simulator.running();
    if (simulatorRunning) {
        simulator.stop();
    }
    else {
        simulator.start();
    }
}

void StateManager::stopSimulator() {
    simulator.stop();
}

void StateManager::resetSimulator() {
    bool simulatorRunning = simulator.running();
    if (simulatorRunning) simulator.stop();
    simulator.compile(defaultState, true);
    if (simulatorRunning) simulator.start();
}

void StateManager::updateDefaultState() {
    simulator.takeSnapshot(defaultState);
}

void StateManager::readSave() {
    std::ifstream saveFile(savePath, std::ios::binary);
    if (!saveFile.is_open()) return;

    int32_t matrixWidth, matrixHeight;
    saveFile.read(reinterpret_cast<char*>(&matrixWidth), sizeof matrixWidth);
    saveFile.read(reinterpret_cast<char*>(&matrixHeight), sizeof matrixHeight);

    // TODO: make this 'safe', meaning that it checks matrixWidth and matrixHeight to be sound values

    defaultState.dataMatrix = typename CanvasState::matrix_t(matrixWidth, matrixHeight);

    for (int32_t y = 0; y != defaultState.height(); ++y) {
        for (int32_t x = 0; x != defaultState.width(); ++x) {
            size_t element_index;
            bool logicLevel;
            bool defaultLogicLevel;
            saveFile.read(reinterpret_cast<char*>(&element_index), sizeof element_index);
            saveFile.read(reinterpret_cast<char*>(&logicLevel), sizeof logicLevel);
            saveFile.read(reinterpret_cast<char*>(&defaultLogicLevel), sizeof defaultLogicLevel);

            CanvasState::element_variant_t element;
            CanvasState::element_tags_t::get(element_index, [&element, logicLevel, defaultLogicLevel](const auto element_tag) {
                using Element = typename decltype(element_tag)::type;
                if constexpr (!std::is_same<Element, std::monostate>::value) {
                    element = Element(logicLevel, defaultLogicLevel);
                }
            });
            defaultState.dataMatrix[{x, y}] = element;
        }
    }

    changed = true;
}

void StateManager::writeSave() {
    std::ofstream saveFile(savePath, std::ios::binary);
    if (!saveFile.is_open()) return;

    simulator.takeSnapshot(defaultState);
    CanvasState& snapshot = defaultState;
    int32_t matrixWidth = snapshot.width();
    int32_t matrixHeight = snapshot.height();
    saveFile.write(reinterpret_cast<char*>(&matrixWidth), sizeof matrixWidth);
    saveFile.write(reinterpret_cast<char*>(&matrixHeight), sizeof matrixHeight);

    for (int32_t y = 0; y != snapshot.height(); ++y) {
        for (int32_t x = 0; x != snapshot.width(); ++x) {
            CanvasState::element_variant_t element = snapshot.dataMatrix[{x, y}];
            bool logicLevel = false;
            bool defaultLogicLevel = false;
            std::visit(visitor{
                [](std::monostate) {},
                [&logicLevel, &defaultLogicLevel](const auto& element) {
                    logicLevel = element.getLogicLevel();
                    defaultLogicLevel = element.getDefaultLogicLevel();
                },
            }, element);
            CanvasState::element_tags_t::for_each([&element, &saveFile, &logicLevel, &defaultLogicLevel](const auto element_tag, const auto index_tag) {
                size_t index = element.index();
                if (index == decltype(index_tag)::value) {
                    saveFile.write(reinterpret_cast<char*>(&index), sizeof index);
                    saveFile.write(reinterpret_cast<char*>(&logicLevel), sizeof logicLevel);
                    saveFile.write(reinterpret_cast<char*>(&defaultLogicLevel), sizeof defaultLogicLevel);
                }
            });
        }
    }
}
