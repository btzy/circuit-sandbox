#pragma once

#include <memory>

#include <SDL.h>

#include "declarations.hpp"

#include "action.hpp"
#include "drawable.hpp"

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

    // renderer
    void render(SDL_Renderer* renderer) const;
};
