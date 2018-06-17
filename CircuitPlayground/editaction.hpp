#pragma once

#include "baseaction.hpp"
#include "playarea.hpp"

/**
 * Represents an action that needs to stop the simulator, make edits, then restart the simulator
 */

class EditAction : public BaseAction {
    
protected:
    PlayArea& playArea;
    bool const simulatorRunning;
    
public:

    EditAction(PlayArea& playArea) :playArea(playArea), simulatorRunning(playArea.stateManager.simulator.running()) {
        // stop the simulator if running
        if (simulatorRunning) playArea.stateManager.simulator.stop();
        canvas() = playArea.stateManager.simulator.takeSnapshot();
    };

    CanvasState& canvas() const {
        return playArea.stateManager.defaultState;
    }

    ~EditAction() override {
        // save to history when this action ends
        playArea.stateManager.saveToHistory();
        // recompile the simulator
        playArea.stateManager.simulator.compile(canvas(), false);
        // start the simulator if its supposed to be running
        if (simulatorRunning) playArea.stateManager.simulator.start();
    }
};
