#pragma once

#include <cstdint> // for int32_t and uint32_t
#include <string>

#include <boost/logic/tribool.hpp>

#include "declarations.hpp"
#include "canvasstate.hpp"
#include "simulator.hpp"
#include "historymanager.hpp"


/**
 * Manages the gamestate, rendering the gamestate, and invoking the simulator.
 */

class StateManager {
private:
    CanvasState defaultState; // stores a cache of the simulator state.  this is guaranteed to be updated if the simulator is not running.
    Simulator simulator; // stores the 'live' states and has methods to compile and run the simulation

    boost::tribool changed = false; // whether canvasstate changed since the last write to the undo stack
    extensions::point deltaTrans{ 0, 0 }; // difference in viewport translation from previous gamestate (TODO: move this into a proper UndoDelta class)

    bool hasSelection = false; // whether selection/base contain meaningful data (neccessary to prevent overwriting defaultState)

    HistoryManager historyManager; // stores the undo/redo stack

    /**
     * Explicitly scans the current gamestate to determine if it changed. Updates 'changed'.
     * This should only be used if no faster alternative exists.
     */
    bool evaluateChangedState();

    /**
     * If a current simulator exists, stop and recompiles the simulator.
     */
    void reloadSimulator();

    friend class EditAction;
    friend class HistoryAction;

public:

    StateManager();
    ~StateManager();

    /**
     * Draw a rectangle of elements onto a pixel buffer supplied by PlayArea.
     * Pixel format: pixel = R | (G << 8) | (B << 16)
     * useDefaultView: whether we want to render the default view (instead of live view)
     */
    void fillSurface(bool useDefaultView, uint32_t* pixelBuffer, int32_t x, int32_t y, int32_t width, int32_t height);

    /**
     * Take a snapshot of the gamestate and save it in the history.
     * Will check if the state is actually changed, before attempting to save.
     */
    void saveToHistory();

    /**
     * Starts the simulator
     */
    void startSimulator();

    /**
     * Toggle between running and pausing the simulator.
     */
    void startOrStopSimulator();

    /**
     * Stops the simulator
     */
    void stopSimulator();

    /**
     * Reset all elements in the simulation to their default state.
     */
    void resetSimulator();

    /**
     * Read/write the save file. The path is hardcoded for now.
     * Save format: [int32_t matrixWidth] [int32_t matrixHeight] (matrixWidth*matrixHeight)x [size_t index, bool logicLevel, bool defaultLogicLevel]
     */
    std::string savePath = "circuitplayground.sav";
    void readSave();
    void writeSave();
};
