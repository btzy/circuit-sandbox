#include <variant>
#include <iostream>
#include <fstream>
#include <chrono>

#include "statemanager.hpp"
#include "visitor.hpp"
#include "sdl_fast_maprgb.hpp"

StateManager::StateManager(Simulator::period_t period) {
    // compile the empty stateManager, so that simulator won't be empty
    simulator.compile(defaultState);
    simulator.setPeriod(period);
}

StateManager::~StateManager() {
}

void StateManager::fillSurface(bool useDefaultView, uint32_t* pixelBuffer, uint32_t pixelFormat, const SDL_Rect& surfaceRect, int32_t pitch) {
    if (simulator.running()) {
        updateDefaultState();
    }

    // this function has been optimized, as it is a bottleneck for large screens when zoomed out
    invoke_RGB_format(pixelFormat, [&](const auto format) {
        using FormatType = decltype(format);
        invoke_bool(useDefaultView, [&](const auto defaultView_tag) {
            using DefaultViewType = decltype(defaultView_tag);

            uint32_t* row = pixelBuffer;
            for (int32_t y = surfaceRect.y; y != surfaceRect.y + surfaceRect.h; ++y, row += pitch) {
                uint32_t* pixel = row;
                for (int32_t x = surfaceRect.x; x != surfaceRect.x + surfaceRect.w; ++x, ++pixel) {
                    const ext::point canvasPt{ x, y };

                    // check if the requested pixel is inside the buffer
                    if (defaultState.contains(canvasPt)) {
                        std::visit(visitor{
                            [pixel](std::monostate) {
                            *pixel = 0;
                        },
                            [pixel, useDefaultView](const auto& element) {
                            *pixel = fast_MapRGB<FormatType::value>(element.template computeDisplayColor<DefaultViewType::value>());
                        },
                            }, defaultState.dataMatrix[canvasPt]);
                    }
                    else {
                        *pixel = 0;
                    }
                }
            }
        });
    });
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
        simulator.compile(defaultState);
        if (simulatorRunning) simulator.start();
    }
}

bool StateManager::saveToHistory() {
    // check if changed if necessary
    evaluateChangedState();

    // don't write to the undo stack if defaultState did not change
    if (!changed) return false;

    historyManager.saveToHistory(defaultState, deltaTrans);

    changed = false;
    deltaTrans = { 0, 0 };
    return true;
}

void StateManager::startSimulatorUnchecked() {
    simulator.start();
}

void StateManager::startOrStopSimulator() {
    bool simulatorRunning = simulator.running();
    if (simulatorRunning) {
        stopSimulatorUnchecked();
    }
    else {
        startSimulatorUnchecked();
    }
}

void StateManager::stopSimulatorUnchecked() {
    simulator.stop();
    updateDefaultState();
}

void StateManager::stepSimulatorUnchecked() {
    simulator.step();
    updateDefaultState();
}

void StateManager::startSimulator() {
    if (!simulator.running()) {
        startSimulatorUnchecked();
    }
}

void StateManager::stopSimulator() {
    if (simulator.running()) {
        stopSimulatorUnchecked();
    }
}

void StateManager::stepSimulator() {
    if (!simulator.running()) {
        stepSimulatorUnchecked();
    }
}

bool StateManager::simulatorRunning() const {
    return simulator.running();
}

void StateManager::resetSimulator() {
    bool simulatorRunning = simulator.running();
    if (simulatorRunning) simulator.stop();
    simulator.reset(defaultState);
    if (simulatorRunning) simulator.start();
}

void StateManager::updateDefaultState() {
    simulator.takeSnapshot(defaultState);
}

CanvasState::element_variant_t StateManager::getElementAtPoint(const ext::point& pt) const {
    if (defaultState.contains(pt)) {
        return defaultState[pt];
    }
    else return std::monostate{};
}
