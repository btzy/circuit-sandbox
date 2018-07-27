#pragma once

#include <utility>
#include <fstream>
#include <algorithm>
#include <limits>
#include <type_traits>

#include <boost/process/spawn.hpp>
#include <SDL.h>
#include <nfd.h>

#include "action.hpp"
#include "mainwindow.hpp"
#include "elements.hpp"
#include "canvasstate.hpp"
#include "fileutils.hpp"

class FileOpenAction final : public Action {
private:

    static CanvasState::ReadResult readSave(CanvasState& state, const char* filePath) {
        std::ifstream saveFile(filePath, std::ios::binary);
        if (!saveFile.is_open()) return CanvasState::ReadResult::IO_ERROR;

        return state.loadSave(saveFile);
    }

public:
    FileOpenAction(MainWindow& mainWindow, PlayArea& playArea, const char* filePath = nullptr) {

        // stop the simulator if running
        bool simulatorRunning = mainWindow.stateManager.simulator.running();
        if (simulatorRunning) mainWindow.stateManager.simulator.stop();


        // show the file dialog if necessary
        char* outPath = nullptr;
        if (filePath == nullptr) {
            nfdresult_t result = NFD_OpenDialog(CCSB_FILE_EXTENSION, nullptr, &outPath);
            mainWindow.suppressMouseUntilNextDown();
            if (result == NFD_OKAY) {
                filePath = outPath;
            }
        }

        if (filePath != nullptr) { // means that the user wants to open filePath
            if (mainWindow.stateManager.historyManager.empty() && !mainWindow.hasFilePath()) {
                // read from filePath
                CanvasState::ReadResult result = readSave(mainWindow.stateManager.defaultState, filePath);
                switch (result) {
                case CanvasState::ReadResult::OK:
                    // reset the translations
                    mainWindow.stateManager.deltaTrans = { 0, 0 };
                    // TODO: some intelligent translation/scale depending on dimensions of canvasstate
                    playArea.translation = { 0, 0 };
                    playArea.scale = 20;
                    // imbue the history
                    mainWindow.stateManager.historyManager.imbue(mainWindow.stateManager.defaultState);
                    mainWindow.setUnsaved(false);
                    mainWindow.setFilePath(filePath);
                    // recompile the simulator (this will propagate all the logic levels properly for display)
                    mainWindow.stateManager.simulator.compile(mainWindow.stateManager.defaultState);
                    simulatorRunning = false; // don't restart the simulator
                    break;
                case CanvasState::ReadResult::OUTDATED:
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Cannot Open File", "This file was created by a newer version of " CIRCUIT_SANDBOX_STRING ", and cannot be opened here.  Please update " CIRCUIT_SANDBOX_STRING " and try again.", mainWindow.window);
                    break;
                case CanvasState::ReadResult::CORRUPTED:
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Cannot Open File", "File is corrupted.", mainWindow.window);
                    break;
                case CanvasState::ReadResult::IO_ERROR:
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Cannot Open File", "This file cannot be accessed.", mainWindow.window);
                    break;
                }
            }
            else {
                // launch a new instance
                boost::process::spawn(mainWindow.processName, filePath);
            }
        }

        // start the simulator if necessary
        if (simulatorRunning) mainWindow.stateManager.simulator.start();

        // free the memory
        if (outPath != nullptr) {
            free(outPath); // note: freeing memory that was acquired by NFD library
        }
    };

    static inline void start(MainWindow& mainWindow, PlayArea& playArea, const ActionStarter& starter, const char* filePath = nullptr) {
        starter.start<FileOpenAction>(mainWindow, playArea, filePath);
        starter.reset();
    }
};
