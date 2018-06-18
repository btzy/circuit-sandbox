#pragma once

#include <cstdint> // for int32_t and uint32_t
#include <string>
#include <stack>

#include <boost/logic/tribool.hpp>

#include "declarations.hpp"
#include "canvasstate.hpp"
#include "simulator.hpp"


/**
 * Manages the gamestate, rendering the gamestate, and invoking the simulator.
 */

class StateManager {
private:
    CanvasState defaultState; // stores a cache of the simulator state.  this is guaranteed to be updated if the simulator is not running.
    Simulator simulator; // stores the 'live' states and has methods to compile and run the simulation

    // fields for undo/redo stack
    std::stack<std::pair<CanvasState, extensions::point>> undoStack; // the undo stack stores entire CanvasStates (with accompanying deltaTrans) for now
    std::stack<std::pair<CanvasState, extensions::point>> redoStack; // the redo stack stores entire CanvasStates (with accompanying deltaTrans) for now
    CanvasState currentHistoryState; // the canvas state treated as 'current' by the history manager; this is either the state when saveToHistory() was last called, or the state after an undo/redo operation.
    boost::tribool changed = false; // whether canvasstate changed since the last write to the undo stack
    extensions::point deltaTrans{ 0, 0 }; // difference in viewport translation from previous gamestate (TODO: move this into a proper UndoDelta class)

    bool hasSelection = false; // whether selection/base contain meaningful data (neccessary to prevent overwriting defaultState)

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
