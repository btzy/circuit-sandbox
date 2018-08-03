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
            mainWindow.getNotificationDisplay().add(NotificationFlags::DEFAULT, 5s, NotificationDisplay::Data{ { "No change to undo", NotificationDisplay::TEXT_COLOR_ERROR } });
        }
    }

    static inline void startByRedoing(MainWindow& mainWindow, const ActionStarter& starter) {
        if (mainWindow.stateManager.historyManager.canRedo()) {
            auto& action = starter.start<HistoryAction>(mainWindow);
            action.deltaTrans = mainWindow.stateManager.historyManager.redo(action.canvas());
            starter.reset(); // terminate the action immediately
        }
        else {
            mainWindow.getNotificationDisplay().add(NotificationFlags::DEFAULT, 5s, NotificationDisplay::Data{ { "No change to redo", NotificationDisplay::TEXT_COLOR_ERROR } });
        }
    }
};
