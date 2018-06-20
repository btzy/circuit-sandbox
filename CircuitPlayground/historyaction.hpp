#pragma once

#include "baseaction.hpp"
#include "playarea.hpp"

class HistoryAction final : public BaseAction {

protected:
    extensions::point deltaTrans;
    PlayArea& playArea;
    bool const simulatorRunning;

public:

    HistoryAction(PlayArea& playArea) :deltaTrans({ 0, 0 }), playArea(playArea), simulatorRunning(playArea.stateManager.simulator.running()) {
        // stop the simulator if running
        if (simulatorRunning) playArea.stateManager.simulator.stop();
        playArea.stateManager.updateDefaultState();
    };

    CanvasState& canvas() const {
        return playArea.stateManager.defaultState;
    }

    ~HistoryAction() override {
        // amend the translation for playArea and stateManager
        playArea.stateManager.deltaTrans = { 0, 0 };
        playArea.translation -= deltaTrans * playArea.scale;
        // recompile the simulator
        playArea.stateManager.simulator.compile(canvas(), false);
        // start the simulator if its supposed to be running
        if (simulatorRunning) playArea.stateManager.simulator.start();
    }

    // check if we need to start selection action from Ctrl-Z or Ctrl-Y
    static inline ActionEventResult startWithKeyboard(const SDL_KeyboardEvent& event, PlayArea& playArea, const ActionStarter& starter) {
        if (event.type == SDL_KEYDOWN) {
            SDL_Keymod modifiers = SDL_GetModState();
            switch (event.keysym.scancode) {
            case SDL_SCANCODE_Y:
                if (modifiers & KMOD_CTRL) {
                    if (playArea.stateManager.historyManager.canRedo()) {
                        auto& action = starter.start<HistoryAction>(playArea);
                        action.deltaTrans = playArea.stateManager.historyManager.redo(action.canvas());
                        return ActionEventResult::COMPLETED; // request to destroy this action immediately
                    }
                }
                break;
            case SDL_SCANCODE_Z:
                if (modifiers & KMOD_CTRL) {
                    if (playArea.stateManager.historyManager.canUndo()) {
                        auto& action = starter.start<HistoryAction>(playArea);
                        action.deltaTrans = playArea.stateManager.historyManager.undo(action.canvas());
                        return ActionEventResult::COMPLETED; // request to destroy this action immediately
                    }
                }
                break;
            default:
                return ActionEventResult::UNPROCESSED;
            }
        }
        return ActionEventResult::UNPROCESSED;
    }
};
