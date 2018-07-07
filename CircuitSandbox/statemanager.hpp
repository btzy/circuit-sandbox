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
    ext::point deltaTrans{ 0, 0 }; // difference in viewport translation from previous gamestate (TODO: move this into a proper UndoDelta class)

    bool hasSelection = false; // whether selection/base contain meaningful data (neccessary to prevent overwriting defaultState)

    HistoryManager historyManager; // stores the undo/redo stack

    /**
     * Explicitly scans the current gamestate to determine if it changed. Updates 'changed'.
     * This should only be used if no faster alternative exists.
     */
    bool evaluateChangedState();

    /**
     * If a current simulator exists, stop and recompiles the simulator.
     * Note: this function is not used at all.
     */
    void reloadSimulator();

    friend class StatefulAction;
    friend class EditAction;
    friend class SaveableAction;
    friend class HistoryAction;
    friend class FileOpenAction;
    friend class FileSaveAction;
    friend class SelectionAction;
    friend class ChangeSimulationSpeedAction;

public:

    StateManager(Simulator::period_t);
    ~StateManager();

    /**
     * Draw a rectangle of elements onto a pixel buffer supplied by PlayArea.
     * Pixel format: pixel = R | (G << 8) | (B << 16)
     * useDefaultView: whether we want to render the default view (instead of live view)
     */
    void fillSurface(bool useDefaultView, uint32_t* pixelBuffer, uint32_t pixelFormat, const SDL_Rect& surfaceRect, int32_t pitch);

    /**
     * Take a snapshot of the gamestate and save it in the history.
     * Will check if the state is actually changed, before attempting to save.
     */
    void saveToHistory();

    /**
     * Starts the simulator.
     * @pre simulator is stopped
     */
    void startSimulatorUnchecked();

    /**
     * Toggle between running and pausing the simulator.
     */
    void startOrStopSimulator();

    /**
     * Stops the simulator.
     * @pre simulator is running
     */
    void stopSimulatorUnchecked();

    /**
     * Steps the simulator.
     * @pre simulator is stopped
     */
    void stepSimulatorUnchecked();

    /**
     * Starts the simulator if it is currently stopped.
     */
    void startSimulator();

    /**
    * Stops the simulator if it is currently running.
    */
    void stopSimulator();

    /**
     * Steps the simulator if it is currently stopped.
     */
    void stepSimulator();

    /**
    * Whether the simulator is running.
    */
    bool simulatorRunning() const;

    /**
     * Reset all elements in the simulation to their default state.
     * This will work even if simulator is running.
     */
    void resetSimulator();

    /**
     * Update defaultState with a new snapshot from the simulator.
     */
    void updateDefaultState();
};
