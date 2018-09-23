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

#include <fstream>
#include <cstring>
#include <type_traits>

#include <boost/process/spawn.hpp>
#include <SDL.h>
#include <nfd.hpp>

#include "visitor.hpp"
#include "action.hpp"
#include "playarea.hpp"
#include "elements.hpp"
#include "canvasstate.hpp"
#include "fileutils.hpp"
#include "notificationdisplay.hpp"

class FileSaveAction final : public Action {
private:

    CanvasState::WriteResult writeSave(const CanvasState& state, const char* filePath) {
        std::ofstream saveFile(filePath, std::ios::binary);
        if (!saveFile.is_open()) return CanvasState::WriteResult::IO_ERROR;

        return state.writeSave(saveFile);
    }

public:
    FileSaveAction(MainWindow& mainWindow, const char* filePath = nullptr) {

        // stop the simulator if running
        bool simulatorRunning = mainWindow.stateManager.simulator.running();
        if (simulatorRunning) mainWindow.stateManager.simulator.stop();

        NFD::UniquePath outPath;
        if (filePath == nullptr) {
            nfdfilteritem_t fileFilter{ CCSB_FILE_FRIENDLY_NAME, CCSB_FILE_EXTENSION };
            nfdresult_t result = NFD::SaveDialog(outPath, &fileFilter, 1, nullptr, "Circuit1." CCSB_FILE_EXTENSION);
            mainWindow.suppressMouseUntilNextDown();
            if (result == NFD_OKAY) {
                // no adding file extension; NFD::SaveDialog does that on supported platforms
                filePath = outPath.get();
            }
        }

        if (filePath != nullptr) { // means that the user wants to save to filePath
            mainWindow.stateManager.updateDefaultState();
            CanvasState::WriteResult result = writeSave(mainWindow.stateManager.defaultState, filePath);
            switch (result) {
            case CanvasState::WriteResult::OK:
                mainWindow.setUnsaved(false);
                mainWindow.setFilePath(filePath);
                mainWindow.stateManager.historyManager.setSaved();
                mainWindow.getNotificationDisplay().add(NotificationFlags::DEFAULT, 5s, NotificationDisplay::Data{ { "File saved", NotificationDisplay::TEXT_COLOR_ACTION } });
                break;
            case CanvasState::WriteResult::IO_ERROR:
                mainWindow.getNotificationDisplay().add(NotificationFlags::DEFAULT, 5s, NotificationDisplay::Data{ { "Error saving file: This file cannot be written to.", NotificationDisplay::TEXT_COLOR_ERROR } });
                break;
            }
        }

        // start the simulator if necessary
        if (simulatorRunning) mainWindow.stateManager.simulator.start();
    };

    static inline void start(MainWindow& mainWindow, const SDL_Keymod& modifiers, const ActionStarter& starter, const char* filePath = nullptr) {
        if (modifiers & KMOD_SHIFT) {
            // force "Save As" dialog
            starter.start<FileSaveAction>(mainWindow, nullptr);
        }
        else if (mainWindow.stateManager.historyManager.changedSinceLastSave()) {
            starter.start<FileSaveAction>(mainWindow, mainWindow.getFilePath());
        }
    }
};
