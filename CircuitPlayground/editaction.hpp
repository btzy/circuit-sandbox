#pragma once

#include "baseaction.hpp"
#include "playarea.hpp"

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
    };

    CanvasState& canvas() const {
        return playArea.stateManager.defaultState;
    }

    ~EditAction() override {
        // amend the translation for playArea and stateManager
        playArea.stateManager.deltaTrans += deltaTrans;
        playArea.translation -= deltaTrans * playArea.scale;
        // save to history when this action ends
        playArea.stateManager.saveToHistory();
        // recompile the simulator
        playArea.stateManager.simulator.compile(canvas(), false);
        // start the simulator if its supposed to be running
        if (simulatorRunning) playArea.stateManager.simulator.start();
    }
};
