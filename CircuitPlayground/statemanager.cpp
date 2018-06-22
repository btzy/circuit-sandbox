#include <variant>
#include <iostream>
#include <fstream>
#include <chrono>

#include "statemanager.hpp"
#include "visitor.hpp"

StateManager::StateManager() {
    // compile the empty stateManager, so that simulator won't be empty
    simulator.compile(defaultState, false);
    using namespace std::chrono_literals;
    simulator.setPeriod(200ms);
}

StateManager::~StateManager() {
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
    if (!boost::indeterminate(changed)) {
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
        stopSimulator();
    }
    else {
        startSimulator();
    }
}

void StateManager::stopSimulator() {
    simulator.stop();
    updateDefaultState();
}

void StateManager::stepSimulator() {
    simulator.step();
    updateDefaultState();
}

void StateManager::stepSimulatorIfPossible() {
    if (!simulator.running()) {
        stepSimulator();
    }
}

void StateManager::resetSimulator() {
    bool simulatorRunning = simulator.running();
    if (simulatorRunning) simulator.stop();
    simulator.compile(defaultState, true);
    simulator.takeSnapshot(defaultState);
    if (simulatorRunning) simulator.start();
}

void StateManager::updateDefaultState() {
    simulator.takeSnapshot(defaultState);
}
