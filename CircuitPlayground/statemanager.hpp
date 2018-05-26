#pragma once

#include <cstdint> // for int32_t and uint32_t
#include <string>

#include "gamestate.hpp"
#include "simulator.hpp"


/**
 * Manages the gamestate, rendering the gamestate, and invoking the simulator.
 */

class StateManager {

private:
    GameState gameState; // stores the 'default' states, which is the state that can be saved to disk
    Simulator simulator; // stores the 'live' states and has methods to compile and run the simulation

public:

    StateManager();
    ~StateManager();

    /**
     * Change the state of a pixel.
     * Currently forwards the invocation to gameState.
     * TODO: Handle writing to the live simulation as well.
     */
    template <typename Element>
    extensions::point changePixelState(int32_t x, int32_t y) {
        if (simulator.holdsSimulation()) {
            // If the simulator current holds a simulation, then we need to update it too.
            if (simulator.running())simulator.stop();
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
     * Take a snapshot of the gamestate and save it in the history
     */
    void saveToHistory();

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
};
