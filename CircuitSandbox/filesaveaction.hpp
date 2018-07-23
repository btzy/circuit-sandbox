#pragma once

#include <fstream>
#include <cstring>
#include <type_traits>

#include <boost/process/spawn.hpp>
#include <SDL.h>
#include <nfd.h>

#include "visitor.hpp"
#include "action.hpp"
#include "playarea.hpp"
#include "elements.hpp"
#include "canvasstate.hpp"
#include "fileutils.hpp"

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

        char* properPath = nullptr;
        char* outPath = nullptr;
        if (filePath == nullptr) {
            nfdresult_t result = NFD_SaveDialog(CCSB_FILE_EXTENSION, nullptr, &outPath);
            mainWindow.suppressMouseUntilNextDown();
            if (result == NFD_OKAY) {
                properPath = new char[std::strlen(outPath) + 6];
                filePath = addExtensionIfNecessary(outPath, properPath);
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
                break;
            case CanvasState::WriteResult::IO_ERROR:
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Cannot Save File", "This file cannot be written to.", mainWindow.window);
                break;
            }
        }

        // start the simulator if necessary
        if (simulatorRunning) mainWindow.stateManager.simulator.start();

        // free the memory
        if (outPath != nullptr) {
            free(outPath); // note: freeing memory that was acquired by NFD library
        }
        if (properPath != nullptr) {
            delete[] properPath;
        }
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
