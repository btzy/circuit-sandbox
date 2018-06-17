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

public:
    CanvasState defaultState; // stores the 'default' states, which is the state that can be saved to disk
    Simulator simulator; // stores the 'live' states and has methods to compile and run the simulation

private:
    
    // fields for undo/redo stack
    std::stack<std::pair<CanvasState, extensions::point>> undoStack; // the undo stack stores entire CanvasStates (with accompanying deltaTrans) for now
    std::stack<std::pair<CanvasState, extensions::point>> redoStack; // the redo stack stores entire CanvasStates (with accompanying deltaTrans) for now
    CanvasState currentHistoryState; // the canvas state treated as 'current' by the history manager; this is either the state when saveToHistory() was last called, or the state after an undo/redo operation.
    boost::tribool changed = false; // whether canvasstate changed since the last write to the undo stack
    extensions::point deltaTrans{ 0, 0 }; // difference in viewport translation from previous gamestate (TODO: move this into a proper UndoDelta class)

    // fields for selection mechanism
    CanvasState selection; // stores the selection
    CanvasState base; // the 'base' layer is a copy of defaultState minus selection at the time the selection was made
    extensions::point selectionTrans{ 0, 0 }; // position of selection in defaultState's coordinate system
    extensions::point baseTrans{ 0, 0 }; // position of base in defaultState's coordinate system
    bool hasSelection = false; // whether selection/base contain meaningful data (neccessary to prevent overwriting defaultState)

    CanvasState clipboard;

    /**
     * Explicitly scans the current gamestate to determine if it changed. Updates 'changed'.
     * This should only be used if no faster alternative exists.
     */
    bool evaluateChangedState();

    /**
     * If a current simulator exists, stop and recompiles the simulator.
     */
    void reloadSimulator();



public:

    StateManager();
    ~StateManager();

    /**
     * Draw a rectangle of elements onto a pixel buffer supplied by PlayArea.
     * Pixel format: pixel = R | (G << 8) | (B << 16)
     * useDefaultView: whether we want to render the default view (instead of live view)
     */
    void fillSurface(bool useDefaultView, uint32_t* pixelBuffer, int32_t x, int32_t y, int32_t width, int32_t height) const;

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

    /**
     * Copy elements within selectionRect from defaultState to selection. Returns true if the selection is not empty.
     */
    bool selectRect(const extensions::point& pt1, const extensions::point& pt2);

    /**
     * Copy all elements in defaultState to selection.
     */
    void selectAll();

    /**
     * Check if (x, y) is within selection.
     */
    bool pointInSelection(extensions::point pt) const;

    /**
     * Clear the selection and merge it with defaultState, then exits selection mode.
     * This method will update the live simulation if necessary.
     * It is okay for selectionTrans and baseTrans to be nonzero.
     */
    extensions::point commitSelection();

    /**
     * Merge base and selection. The result is stored in defaultState.
     * This method will update the live simulation if necessary.
     * It is okay for selectionTrans and baseTrans to be nonzero.
     * WARNING: `base` and `selection` will be left in an indeterminate state! Use finishSelection() after mergeSelection() to clear them.
     */
    extensions::point mergeSelection();

    /**
     * Move the selection.
     */
    void moveSelection(int32_t dx, int32_t dy);

    /**
     * Delete the elements in the selection, then exit selection mode.
     * NOTE: This method and clearSelection() have potential to get mixed up.
     * This method will update the live simulation if necessary.
     */
    extensions::point deleteSelection();

    /**
     * Copy the contents of the clipboard to the selection.
     */
    extensions::point pasteSelection(int32_t x, int32_t y);

    /**
     * Exit selection mode without saving anything.
     */
    void finishSelection();

    /**
     * Cut the selection into the clipboard.
     * This method will update the live simulation if necessary.
     */
    extensions::point cutSelectionToClipboard();

    /**
     * Copy the selection into the clipboard.
     */
    void copySelectionToClipboard();

    /**
     * Paste the selection from the clipboard.
     */
    void pasteSelectionFromClipboard(int32_t x, int32_t y);
};
