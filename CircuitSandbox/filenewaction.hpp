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

#include <boost/process/spawn.hpp>
#include <SDL.h>

#include "statefulaction.hpp"
#include "mainwindow.hpp"

class FileNewAction final : public Action {
public:
    FileNewAction(MainWindow& mainWindow) {
        boost::process::spawn(mainWindow.processName);
    };

    static inline void start(MainWindow& mainWindow, const ActionStarter& starter) {
        starter.start<FileNewAction>(mainWindow);
        starter.reset();
    }
};
