#pragma once

#include <boost/logic/tribool.hpp>
#include "playareaaction.hpp"
#include "mainwindow.hpp"

/**
 * Represents an action that needs to stop the simulator, make edits, then restart the simulator.
 * An edit action is the fundamental unit of change that the user can make on the canvas state.
 * When an edit action is started, the simulator will stop.
 * An edit action may modify defaultState and deltaTrans arbitrarily during the action,
 * but it should take care that rendering is done properly, because by default rendering uses defaultState.
 * It is guaranteed that defaultState and deltaTrans will not be used by any other code (apart from rendering) when an action is in progress.
 */

class EditAction : public PlayAreaAction {

protected:
    ext::point deltaTrans;
    bool const simulatorRunning;
    bool needsRecompile = true;

public:

    EditAction(MainWindow& mainWindow) : PlayAreaAction(mainWindow), deltaTrans({ 0, 0 }), simulatorRunning(stateManager().simulator.running()) {
        // stop the simulator if running
        if (simulatorRunning) stateManager().stopSimulatorUnchecked();
        changed() = boost::indeterminate;
    };


    ~EditAction() override {
        // amend the translation for playArea
        playArea().translation -= deltaTrans * playArea().scale;
        if (needsRecompile) {
            // update window title
            mainWindow.setUnsaved(stateManager().historyManager.changedSinceLastSave());
            // recompile the simulator
            stateManager().simulator.compile(canvas());
        }
        // start the simulator if its supposed to be running
        if (simulatorRunning) stateManager().startSimulator();
    }
};
