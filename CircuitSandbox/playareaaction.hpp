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

#include "statefulaction.hpp"
#include "playarea.hpp"

class PlayAreaAction : public StatefulAction {
public:
    PlayAreaAction(MainWindow& mainWindow) : StatefulAction(mainWindow) {
		// register for playarea events
        playArea().currentAction.setAction(this);
	}

    ~PlayAreaAction() override {
        playArea().currentAction.setAction(nullptr);
    }

protected:
    PlayArea& playArea() const {
        return mainWindow.playArea;
    }

public:
	// all the events below are called by PlayArea

    // expects the mouse to be in the playarea
    virtual inline ActionEventResult processPlayAreaMouseButtonDown(const SDL_MouseButtonEvent&) {
        return ActionEventResult::UNPROCESSED;
    }
    // might be outside the playarea if the mouse was dragged out
    virtual inline ActionEventResult processPlayAreaMouseDrag(const SDL_MouseMotionEvent&) {
        return ActionEventResult::UNPROCESSED;
    }
    virtual inline ActionEventResult processPlayAreaMouseButtonUp() {
        return ActionEventResult::UNPROCESSED;
    }

    // expects the mouse to be in the playarea
    virtual inline ActionEventResult processPlayAreaMouseWheel(const SDL_MouseWheelEvent&) {
        return ActionEventResult::UNPROCESSED;
    }

    // expects the mouse to be in the playarea
    virtual inline ActionEventResult processPlayAreaMouseHover(const SDL_MouseMotionEvent&) {
        return ActionEventResult::UNPROCESSED;
    }
    virtual inline ActionEventResult processPlayAreaMouseLeave() {
        return ActionEventResult::UNPROCESSED;
    }

    // rendering functions
	// these functions will only be called if the playarea needs to render, i.e. disableDefaultRender() returns false
    /**
     * Disable the default rendering function, which draws everything from StateManager::defaultState.
     * Use this if you have modified StateManager::defaultState and don't want to show the modifications,
     * or you want to draw something that covers the whole playarea.
     */
    virtual bool disablePlayAreaDefaultRender() const {
        return false;
    }
    /**
     * Render on a surface (in canvas units) using its pixel buffer.
     * `renderRect` is the part of the canvas that should be drawn onto this surface.
     */
    virtual void renderPlayAreaSurface(uint32_t* pixelBuffer, uint32_t pixelFormat, const SDL_Rect& renderRect, int32_t pitch) const {}
    /**
     * Render directly using the SDL_Renderer (it is in window coordinates).
     */
    virtual void renderPlayAreaDirect(SDL_Renderer*) const {}
};
