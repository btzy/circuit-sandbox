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