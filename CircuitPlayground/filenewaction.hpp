#pragma once

#include <boost/process/spawn.hpp>
#include <SDL.h>

#include "baseaction.hpp"
#include "playarea.hpp"
#include "mainwindow.hpp"
#include "canvasstate.hpp"

class FileNewAction final : public BaseAction {
public:
    FileNewAction(PlayArea& playArea) {
        if (playArea.stateManager.historyManager.empty()) {
            // stop the simulator if running
            if (playArea.stateManager.simulator.running()) playArea.stateManager.simulator.stop();
            playArea.stateManager.defaultState = CanvasState();
            // reset the translations
            playArea.stateManager.deltaTrans = { 0, 0 };
            // TODO: some intelligent translation/scale depending on dimensions of canvasstate
            playArea.translation = { 0, 0 };
            playArea.scale = 20;
            // imbue the history
            playArea.stateManager.historyManager.imbue(playArea.stateManager.defaultState);
        }
        else {
            // launch a new instance
            boost::process::spawn(playArea.mainWindow.processName);
        }
    };

    // check if we need to start action from Ctrl-N
    static inline ActionEventResult startWithKeyboard(const SDL_KeyboardEvent& event, PlayArea& playArea, const ActionStarter& starter) {
        if (event.type == SDL_KEYDOWN) {
            SDL_Keymod modifiers = SDL_GetModState();
            switch (event.keysym.scancode) {
            case SDL_SCANCODE_N:
                if (modifiers & KMOD_CTRL) {
                    starter.start<FileNewAction>(playArea);
                    return ActionEventResult::COMPLETED;
                }
                break;
            default:
                break;
            }
        }
        return ActionEventResult::UNPROCESSED;
    }
};