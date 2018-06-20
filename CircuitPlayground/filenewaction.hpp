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
        boost::process::spawn(playArea.mainWindow.processName);
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