#include <variant>
#include <iostream>
#include <fstream>
#include <chrono>

#include "statemanager.hpp"
#include "visitor.hpp"
#include "sdl_fast_maprgb.hpp"
#include "buttonbar.hpp"
#include "fileutils.hpp"

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
                        *pixel = fast_MapRGB<FormatType::value>(computeDisplayColor(element, useDefaultView));
                    },
                        }, defaultState.dataMatrix[canvasPt]);
                }
                else {
                    *pixel = 0;
                }
            }
        }
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

void StateManager::setButtonBarDescription(ButtonBar& buttonBar, const ext::point& pt) {
    if (!defaultState.contains(pt)) {
        buttonBar.clearDescription();
        return;
    }
    std::visit([&](const auto& element) {
        using ElementType = std::decay_t<decltype(element)>;
        if constexpr(std::is_base_of_v<LogicGate, ElementType> || std::is_base_of_v<ScreenCommunicatorElement, ElementType>) {
            if (element.getLogicLevel()) {
                buttonBar.setDescription(ElementType::displayName, ElementType::displayColor, " [HIGH]", SDL_Color{ 0x66, 0xFF, 0x66, 0xFF });
            }
            else {
                buttonBar.setDescription(ElementType::displayName, ElementType::displayColor, " [LOW]", SDL_Color{ 0x66, 0x66, 0x66, 0xFF });
            }
        }
        else if constexpr(std::is_base_of_v<Relay, ElementType>) {
            // TODO: make canvasstate and savefile store whether relays are conductive or not
            // currently we're just using the logic level
            if (element.getLogicLevel()) {
                buttonBar.setDescription(ElementType::displayName, ElementType::displayColor, " [Conductive]", SDL_Color{ 0xFF, 0xFF, 0x66, 0xFF });
            }
            else {
                buttonBar.setDescription(ElementType::displayName, ElementType::displayColor, " [Insulator]", SDL_Color{ 0x66, 0x66, 0x66, 0xFF });
            }
        }
        else if constexpr(std::is_base_of_v<FileInputCommunicatorElement, ElementType>) {
            std::string filePathDescription;
            const char* logicDescription;
            SDL_Color filePathColor;
            SDL_Color logicColor;
            if (element.communicator && !element.communicator->getFile().empty()) {
                filePathDescription = " [" + std::string(getFileName(element.communicator->getFile().c_str())) + "]";
                filePathColor = SDL_Color{ 0xFF, 0xFF, 0xFF, 0xFF };
            }
            else {
                filePathDescription = " [No file]";
                filePathColor = SDL_Color{ 0x66, 0x66, 0x66, 0xFF };
            }
            if (element.getLogicLevel()) {
                logicDescription = " [HIGH]";
                logicColor = SDL_Color{ 0x66, 0xFF, 0x66, 0xFF };
            }
            else {
                logicDescription = " [LOW]";
                logicColor = SDL_Color{ 0x66, 0x66, 0x66, 0xFF };
            }
            buttonBar.setDescription(ElementType::displayName, ElementType::displayColor, logicDescription, logicColor, filePathDescription.c_str(), filePathColor);
        }
        else if constexpr(std::is_base_of_v<Element, ElementType>) {
            buttonBar.setDescription(ElementType::displayName, ElementType::displayColor);
        }
        else {
            buttonBar.clearDescription();
        }
    }, defaultState[pt]);
}
