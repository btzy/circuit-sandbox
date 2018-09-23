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

    EditAction(MainWindow& mainWindow) : PlayAreaAction(mainWindow), deltaTrans(ext::point{ 0, 0 }), simulatorRunning(stateManager().simulator.running()) {
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
