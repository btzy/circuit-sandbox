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

#include "editaction.hpp"
#include "mainwindow.hpp"

class HistoryAction final : public EditAction {
public:

    HistoryAction(MainWindow& mainWindow) : EditAction(mainWindow) {
        changed() = false; // so that HistoryManager::saveToHistory() will not be called
    };

    ~HistoryAction() override {
        // reset the translation for playArea and stateManager
        stateManager().deltaTrans = { 0, 0 };
    }

    static inline void startByUndoing(MainWindow& mainWindow, const ActionStarter& starter) {
        if (mainWindow.stateManager.historyManager.canUndo()) {
            auto& action = starter.start<HistoryAction>(mainWindow);
            action.deltaTrans = mainWindow.stateManager.historyManager.undo(action.canvas());
            starter.reset(); // terminate the action immediately
        }
        else {
            mainWindow.noUndoNotification = mainWindow.getNotificationDisplay().uniqueAdd(NotificationFlags::DEFAULT, 5s, NotificationDisplay::Data{ { "No change to undo", NotificationDisplay::TEXT_COLOR_ERROR } });
        }
    }

    static inline void startByRedoing(MainWindow& mainWindow, const ActionStarter& starter) {
        if (mainWindow.stateManager.historyManager.canRedo()) {
            auto& action = starter.start<HistoryAction>(mainWindow);
            action.deltaTrans = mainWindow.stateManager.historyManager.redo(action.canvas());
            starter.reset(); // terminate the action immediately
        }
        else {
            mainWindow.noRedoNotification = mainWindow.getNotificationDisplay().uniqueAdd(NotificationFlags::DEFAULT, 5s, NotificationDisplay::Data{ { "No change to redo", NotificationDisplay::TEXT_COLOR_ERROR } });
        }
    }
};
