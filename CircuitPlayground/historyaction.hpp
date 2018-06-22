#pragma once

#include "baseaction.hpp"
#include "playarea.hpp"

class HistoryAction final : public EditAction {

public:

    HistoryAction(PlayArea& playArea) : EditAction(playArea) {
        changed() = false; // so that HistoryManager::saveToHistory() will not be called
    };

    ~HistoryAction() override {
        // reset the translation for playArea and stateManager
        playArea.stateManager.deltaTrans = { 0, 0 };
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
