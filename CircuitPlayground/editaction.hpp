#pragma once

#include <boost/logic/tribool.hpp>
#include "baseaction.hpp"
#include "playarea.hpp"
#include "mainwindow.hpp"

/**
 * Represents an action that needs to stop the simulator, make edits, then restart the simulator
 */

class EditAction : public BaseAction {
    
protected:
    extensions::point deltaTrans;
    PlayArea& playArea;
    bool const simulatorRunning;
    
public:

    EditAction(PlayArea& playArea) :deltaTrans({ 0, 0 }), playArea(playArea), simulatorRunning(playArea.stateManager.simulator.running()) {
        // stop the simulator if running
        if (simulatorRunning) playArea.stateManager.simulator.stop();
        playArea.stateManager.updateDefaultState();
        changed() = boost::indeterminate;
    };

    /**
     * Gets the canvas for this action to make edits on.
     * All actions may modify this safely at any time during the action, but modifications will be rendered to screen immediately.
     */
    CanvasState& canvas() const {
        return playArea.stateManager.defaultState;
    }

    /**
     * Gets the tribool flag for whether the canvas was changed.  This is used for the saveToHistory() call in the destructor of EditAction.
     * All EditActions may modify this safely at any time during the action.  Only the latest set state will be used.
     * By default this is boost::indeterminate, so changing this flag is only an optimization.
     */
    boost::tribool& changed() const {
        return playArea.stateManager.changed;
    }

    ~EditAction() override {
        // amend the translation for playArea and stateManager
        playArea.stateManager.deltaTrans += deltaTrans;
        playArea.translation -= deltaTrans * playArea.scale;
        // save to history when this action ends
        playArea.stateManager.saveToHistory();
        playArea.mainWindow.setUnsaved(true);
        // recompile the simulator
        playArea.stateManager.simulator.compile(canvas(), false);
        // start the simulator if its supposed to be running
        if (simulatorRunning) playArea.stateManager.simulator.start();
    }
};
