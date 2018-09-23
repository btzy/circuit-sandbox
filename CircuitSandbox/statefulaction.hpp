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
#include "action.hpp"
#include "mainwindow.hpp"
#include "statemanager.hpp"

class StatefulAction : public Action {
protected:
    MainWindow& mainWindow;

public:
    StatefulAction(MainWindow& mainWindow) :mainWindow(mainWindow) {}

protected:
    StateManager& stateManager() const {
        return mainWindow.stateManager;
    }

    /**
     * Gets the canvas for this action to make edits on.
     * All actions may modify this safely at any time during the action, but modifications will be rendered to screen immediately.
     */
    CanvasState& canvas() const {
        return stateManager().defaultState;
    }

    /**
     * Gets the tribool flag for whether the canvas was changed.  This is used for the saveToHistory() call in the destructor of EditAction.
     * All EditActions may modify this safely at any time during the action.  Only the latest set state will be used.
     * By default this is boost::indeterminate, so changing this flag is only an optimization.
     */
    boost::tribool& changed() const {
        return stateManager().changed;
    }
};