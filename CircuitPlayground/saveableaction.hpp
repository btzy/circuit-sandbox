#pragma once

#include "editaction.hpp"

/**
 * Represents an EditAction that can be saved to history
 */

class SaveableAction : public EditAction {

public:

    SaveableAction(MainWindow& mainWindow) : EditAction(mainWindow) {};

    ~SaveableAction() override {
        // amend the translation for stateManager
        stateManager().deltaTrans += deltaTrans;
        // save to history when this action ends
        stateManager().saveToHistory();
    }
};
