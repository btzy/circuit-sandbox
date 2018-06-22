#pragma once

#include "editaction.hpp"
#include "playarea.hpp"
#include "mainwindow.hpp"

/**
 * Represents an EditAction that can be saved to history
 */

class SaveableAction : public EditAction {

public:

    SaveableAction(PlayArea& playArea) : EditAction(playArea) {};

    ~SaveableAction() override {
        // amend the translation for stateManager
        playArea.stateManager.deltaTrans += deltaTrans;
        // save to history when this action ends
        playArea.stateManager.saveToHistory();
    }
};
