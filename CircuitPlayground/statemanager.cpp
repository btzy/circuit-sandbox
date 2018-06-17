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

void StateManager::fillSurface(bool useDefaultView, uint32_t* pixelBuffer, int32_t left, int32_t top, int32_t width, int32_t height) const {

    // This is kinda yucky, but I can't think of a better way such that:
    // 1) If useDefaultView is false, we don't make a *copy* of defaultState
    // 2) If useDefaultView is true, we take a snapshot from the simulator and use it
    auto lambda = [this, &pixelBuffer, left, top, width, height, useDefaultView](const CanvasState& renderGameState) {
        for (int32_t y = top; y != top + height; ++y) {
            for (int32_t x = left; x != left + width; ++x) {
                SDL_Color computedColor{ 0, 0, 0, 0 };

                // check if the requested pixel inside the buffer
                if (x >= 0 && x < renderGameState.width() && y >= 0 && y < renderGameState.height()) {
                    if (!hasSelection || (x >= baseTrans.x && x < baseTrans.x + base.width() &&
                                          y >= baseTrans.y && y < baseTrans.y + base.height() &&
                                          !std::holds_alternative<std::monostate>(base.dataMatrix[{x - baseTrans.x, y - baseTrans.y}]))) {
                        std::visit(visitor{
                            [](std::monostate) {},
                            [&computedColor, useDefaultView](const auto& element) {
                                computedColor = computeDisplayColor(element, useDefaultView);
                            },
                        }, renderGameState.dataMatrix[{x, y}]);
                    }
                }

                if (hasSelection) {
                    int32_t select_x = x - selectionTrans.x;
                    int32_t select_y = y - selectionTrans.y;
                    if (select_x >= 0 && select_x < selection.width() && select_y >= 0 && select_y < selection.height()) {
                        std::visit(visitor{
                            [](std::monostate) {},
                            [&computedColor](const auto& element) {
                                computedColor = std::decay_t<decltype(element)>::displayColor;
                                computedColor.b = 0xFF;
                            },
                        }, selection.dataMatrix[{select_x, select_y}]);
                    }
                }
                uint32_t color = computedColor.r | (computedColor.g << 8) | (computedColor.b << 16);
                *pixelBuffer++ = color;
            }
        }
    };

    if (useDefaultView) {
        lambda(defaultState);
    }
    else {
        lambda(simulator.takeSnapshot());
    }

}

bool StateManager::evaluateChangedState() {
    // return immediately if it isn't indeterminate
    if (changed != boost::indeterminate) {
        return changed;
    }

    if (defaultState.width() != currentHistoryState.width() ||
        defaultState.height() != currentHistoryState.height()) {

        changed = true;
        return true;
    }
    for (int32_t y = 0; y < defaultState.height(); ++y) {
        for (int32_t x = 0; x < defaultState.width(); ++x) {
            if (defaultState.dataMatrix[{x, y}].index() != currentHistoryState.dataMatrix[{x, y}].index()) {
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

    // note: we save the inverse translation
    undoStack.emplace(std::move(currentHistoryState), -deltaTrans);
    // save a snapshot of the defaultState (which should be in sync with the simulator)
    currentHistoryState = defaultState;
    deltaTrans = { 0, 0 };

    changed = false;

    // flush the redoStack
    while (!redoStack.empty()) redoStack.pop();
}

extensions::point StateManager::undo() {
    if (undoStack.empty()) return { 0, 0 };

    auto [canvasState, tmpDeltaTrans] = std::move(undoStack.top());
    undoStack.pop();

    // note: we save the inverse of the undo translation into the redo stack
    redoStack.emplace(std::move(currentHistoryState), -tmpDeltaTrans);
    defaultState = currentHistoryState = std::move(canvasState);
    deltaTrans = { 0, 0 };

    reloadSimulator();

    return tmpDeltaTrans;
}

extensions::point StateManager::redo() {
    if (redoStack.empty()) return { 0, 0 };

    auto[canvasState, tmpDeltaTrans] = std::move(redoStack.top());
    redoStack.pop();

    // note: we save the inverse of the redo translation into the undo stack
    undoStack.emplace(std::move(currentHistoryState), -tmpDeltaTrans);
    defaultState = currentHistoryState = std::move(canvasState);
    deltaTrans = { 0, 0 };

    reloadSimulator();

    return tmpDeltaTrans;
}

void StateManager::startSimulator() {
    simulator.start();
}

void StateManager::startOrStopSimulator() {
    if (simulator.holdsSimulation()) { // TODO: remove this, because now simulator will never be empty
        bool simulatorRunning = simulator.running();
        if (simulatorRunning) {
            simulator.stop();
        }
        else {
            simulator.start();
        }
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

void StateManager::readSave() {
    std::ifstream saveFile(savePath, std::ios::binary);
    if (!saveFile.is_open()) return;

    int32_t matrixWidth, matrixHeight;
    saveFile.read(reinterpret_cast<char*>(&matrixWidth), sizeof matrixWidth);
    saveFile.read(reinterpret_cast<char*>(&matrixHeight), sizeof matrixHeight);

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

    CanvasState snapshot = simulator.takeSnapshot();
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

bool StateManager::selectRect(const extensions::point& pt1, const extensions::point& pt2) {
    // make a copy of defaultState
    base = defaultState;

    // normalize supplied points
    extensions::point topLeft = extensions::min(pt1, pt2);
    extensions::point bottomRight = extensions::max(pt1, pt2) + extensions::point{1, 1};

    // TODO: proper rectangle clamping functions
    // restrict selectionRect to the area within base
    int32_t sx_min = std::max(topLeft.x, 0);
    int32_t sy_min = std::max(topLeft.y, 0);
    int32_t sx_max = std::min(bottomRight.x, base.width());
    int32_t sy_max = std::min(bottomRight.y, base.height());
    int32_t swidth = sx_max - sx_min;
    int32_t sheight = sy_max - sy_min;

    // no elements in the selection rect
    if (swidth <= 0 || sheight <= 0) return false;

    // splice out the selection from the base
    selection = base.splice(sx_min, sy_min, swidth, sheight);

    // shrink the selection
    extensions::point selectionShrinkTrans = selection.shrinkDataMatrix();

    // no elements in the selection rect
    if (selection.empty()) return false;

    hasSelection = true;

    selectionTrans = extensions::point{ sx_min, sy_min } - selectionShrinkTrans;

    baseTrans = -base.shrinkDataMatrix();

    return true;
}

void StateManager::selectAll() {
    finishSelection();
    selection = defaultState;
    hasSelection = true;
}

bool StateManager::pointInSelection(extensions::point pt) const {
    pt -= selectionTrans;
    return pt.x >= 0 && pt.x < selection.width() && pt.y >= 0 && pt.y < selection.height();
}

void StateManager::finishSelection() {
    selection = CanvasState();
    selectionTrans = { 0, 0 };
    base = CanvasState();
    baseTrans = { 0, 0 };
    hasSelection = false;
}

extensions::point StateManager::commitSelection() {
    extensions::point ans = mergeSelection();
    finishSelection();
    return ans;
}

extensions::point StateManager::mergeSelection() {
    if (!hasSelection) return { 0, 0 };

    auto [tmpDefaultState, translation] = CanvasState::merge(std::move(base), baseTrans, std::move(selection), selectionTrans);
    defaultState = std::move(tmpDefaultState);
    deltaTrans += translation;

    changed = boost::indeterminate; // is there's a possibility that the merge does nothing?

    return translation;
}

void StateManager::moveSelection(int32_t dx, int32_t dy) {
    selectionTrans.x += dx;
    selectionTrans.y += dy;
}

extensions::point StateManager::deleteSelection() {
    defaultState = std::move(base);

    changed = true; // since the selection can never be empty

    extensions::point translation = -baseTrans;

    deltaTrans += translation;

    finishSelection();

    return translation;
}

void StateManager::copySelectionToClipboard() {
    clipboard = selection;
}

extensions::point StateManager::cutSelectionToClipboard() {
    clipboard = std::move(selection);
    extensions::point translation = deleteSelection();

    return translation;
}

void StateManager::pasteSelectionFromClipboard(int32_t x, int32_t y) {
    base = defaultState;
    selection = clipboard;
    selectionTrans = { x, y };
    hasSelection = true;
}
