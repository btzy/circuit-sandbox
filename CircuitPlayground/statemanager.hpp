#pragma once

#include <cstdint> // for int32_t and uint32_t
#include <string>
#include <vector>

#include "gamestate.hpp"
#include "simulator.hpp"


/**
 * Manages the gamestate, rendering the gamestate, and invoking the simulator.
 */

class StateManager {

private:
    GameState gameState; // stores the 'default' states, which is the state that can be saved to disk
    Simulator simulator; // stores the 'live' states and has methods to compile and run the simulation
    std::vector<GameState> history; // the undo stack stores entire GameStates for now
    size_t historyIndex = -1; // index of current gamestate in history (constructor will initialise it to 0)

public:

    StateManager();
    ~StateManager();

    /**
     * Change the state of a pixel.
     * Currently forwards the invocation to gameState.
     */
    template <typename Element>
    extensions::point changePixelState(int32_t x, int32_t y) {
        if (simulator.holdsSimulation()) {
            // If the simulator current holds a simulation, then we need to update it too.
            if (simulator.running()) simulator.stop();
            GameState simState = simulator.takeSnapshot();
            simState.changePixelState<Element>(x, y);
            simulator.compile(simState);
            simulator.start();
        }
        return gameState.changePixelState<Element>(x, y);
    }

    /**
     * Draw a rectangle of elements onto a pixel buffer supplied by PlayArea.
     * Pixel format: pixel = R | (G << 8) | (B << 16)
     * useLiveView: whether we want to render the live view (instead of the default view)
     */
    void fillSurface(bool useLiveView, uint32_t* pixelBuffer, int32_t x, int32_t y, int32_t width, int32_t height) const;

    /**
     * Explicitly scans the current gamestate to determine if it changed. Updates 'changed'.
     * This should only be used if no faster alternative exists.
     */
    bool checkIfChanged();

    /**
     * Take a snapshot of the gamestate and save it in the history
     */
    void saveToHistory();

    /**
     * Undo/redo. Returns the translation to apply to center viewport.
     * The gamestate will not change if it is already the oldest/newest state.
     */
    extensions::point undo();
    extensions::point redo();

    /**
     * Reset the transient (live) states to the default states for all elements
     */
    void resetLiveView();

    /**
     * Starts the simulator
     */
    void startSimulator();

    /**
     * Stops the simulator
     */
    void stopSimulator();

    /**
     * Clear the transient (live) states
     */
    void clearLiveView();

    /**
     * Read/write the save file. The path is hardcoded for now.
     * Save format: [int32_t matrixWidth] [int32_t matrixHeight] (matrixWidth*matrixHeight)x [size_t index]
     */
    std::string savePath = "circuitplayground.sav";
    void readSave();
    void writeSave();

    /**
     * These invocations are forwarded to gameState.
     */
    void selectRect(SDL_Rect selectionRect);
    bool pointInSelection(int32_t x, int32_t y);
    void clearSelection();
    extensions::point moveSelection(int32_t dx, int32_t dy);
    extensions::point deleteSelection();
};
