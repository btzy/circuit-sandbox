#pragma once

#include <boost/logic/tribool.hpp>
#include "action.hpp"
#include "playarea.hpp"

/**
 * Represents an action that needs to stop the simulator, make edits, then restart the simulator.
 * An edit action is the fundamental unit of change that the user can make on the canvas state.
 * When an edit action is started, the simulator will stop.
 * An edit action may modify defaultState and deltaTrans arbitrarily during the action,
 * but it should take care that rendering is done properly, because by default rendering uses defaultState.
 * It is guaranteed that defaultState and deltaTrans will not be used by any other code (apart from rendering) when an action is in progress.
 */

class EditAction : public Action {

protected:
    ext::point deltaTrans;
    PlayArea& playArea;
    bool const simulatorRunning;

public:

    EditAction(PlayArea& playArea) :deltaTrans({ 0, 0 }), playArea(playArea), simulatorRunning(playArea.stateManager.simulator.running()) {
        // stop the simulator if running
        if (simulatorRunning) playArea.stateManager.stopSimulatorUnchecked();
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
        // amend the translation for playArea
        playArea.translation -= deltaTrans * playArea.scale;
        // update window title
        playArea.mainWindow.setUnsaved(playArea.stateManager.historyManager.changedSinceLastSave());
        // recompile the simulator
        playArea.stateManager.simulator.compile(canvas(), false);
        // start the simulator if its supposed to be running
        if (simulatorRunning) playArea.stateManager.startSimulator();
    }
};
