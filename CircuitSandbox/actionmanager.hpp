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

#include "action.hpp"
#include "control.hpp"

/**
 * ActionManager holds an action and ensures that only one action can be active at any moment.
 * When a new action is started, the previous is destroyed.
 */

class ActionManager {

private:

    MainWindow& mainWindow;

    std::unique_ptr<Action> data; // current action that receives playarea events, might be nullptr

public:
    // disable all the copy and move constructors and assignment operators, because this class is intended to be a 'policy' type class

    ActionManager(MainWindow& mainWindow) : mainWindow(mainWindow), data(nullptr) {}

    ~ActionManager() {}

    void reset() {
        data = nullptr;
    }

    ActionStarter getStarter() {
        return ActionStarter(data);
    }
};
