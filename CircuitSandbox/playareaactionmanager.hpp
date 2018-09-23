/*
 * Circuit Sandbox
 * Copyright 2018 National University of Singapore <enterprise@nus.edu.sg>
 *
 * This file is part of Circuit Sandbox.
 * Circuit Sandbox is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 * Circuit Sandbox is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with Circuit Sandbox.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <memory>

#include <SDL.h>

#include "declarations.hpp"

#include "actionmanager.hpp"
#include "action.hpp"

class PlayAreaActionManager {
private:
    ActionManager& actionManager;
    MainWindow& mainWindow;
    PlayArea& playArea;
    PlayAreaAction* data; // current action that receives playarea events, might be nullptr

    friend class PlayAreaAction;

    /**
     * returns true if event is consumed, false otherwise
     */
    template<typename Data, typename ProcessCallback>
    inline bool forwardEvent(const Data& data, ProcessCallback&& process) {
        if (!data) return false;
        ActionEventResult result = std::forward<ProcessCallback>(process)();
        switch (result) {
        case ActionEventResult::COMPLETED:
            actionManager.reset();
            [[fallthrough]];
        case ActionEventResult::PROCESSED:
            return true;
        case ActionEventResult::CANCELLED:
            actionManager.reset();
            [[fallthrough]];
        case ActionEventResult::UNPROCESSED:
            break;
        }
        return false;
    }

    /**
     * returns true if event is consumed, false otherwise
     */
    template<typename Data, typename ProcessCallback, typename StartWithCallback>
    inline bool forwardEvent(const Data& data, ProcessCallback&& process, StartWithCallback&& startWith) {
        if (forwardEvent(data, std::forward<ProcessCallback>(process))) return true;

        // have to ask all the actions if they would like to start
        ActionEventResult startResult = std::forward<StartWithCallback>(startWith)();
        switch (startResult) {
        case ActionEventResult::COMPLETED:
            actionManager.reset();
            [[fallthrough]];
        case ActionEventResult::PROCESSED:
            return true;
        case ActionEventResult::CANCELLED:
            actionManager.reset();
            [[fallthrough]];
        case ActionEventResult::UNPROCESSED:
            break;
        }

        return false;
    }

    void setAction(PlayAreaAction* action) {
        data = action;
    }

    bool mouseDownResult; // result of the last mousedown event processed by the current action

public:
    // disable all the copy and move constructors and assignment operators, because this class is intended to be a 'policy' type class

    PlayAreaActionManager(ActionManager& actionManager, MainWindow& mainWindow, PlayArea& playArea) : actionManager(actionManager), mainWindow(mainWindow), playArea(playArea), data(nullptr) {}

    ~PlayAreaActionManager() {}

    ActionStarter getStarter() {
        return actionManager.getStarter();
    }

    // requirements here follow those in Action.
    bool processPlayAreaMouseButtonDown(const SDL_MouseButtonEvent&);
    bool processPlayAreaMouseDrag(const SDL_MouseMotionEvent&);
    bool processPlayAreaMouseButtonUp();

    bool processPlayAreaMouseWheel(const SDL_MouseWheelEvent&);

    bool processPlayAreaMouseHover(const SDL_MouseMotionEvent& event);
    bool processPlayAreaMouseLeave();

    // renderer
    bool disablePlayAreaDefaultRender() const;
    void renderPlayAreaSurface(uint32_t* pixelBuffer, uint32_t pixelFormat, const SDL_Rect& renderRect, int32_t pitch) const;
    void renderPlayAreaDirect(SDL_Renderer* renderer) const;
};
