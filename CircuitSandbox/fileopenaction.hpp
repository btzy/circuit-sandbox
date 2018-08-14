#pragma once

#include <utility>
#include <fstream>
#include <algorithm>
#include <limits>
#include <type_traits>

#include <boost/process/spawn.hpp>
#include <SDL.h>
#include <nfd.hpp>

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
        NFD::UniquePath outPath;
        if (filePath == nullptr) {
            // create (single) filter
            nfdfilteritem_t fileFilter{ CCSB_FILE_FRIENDLY_NAME, CCSB_FILE_EXTENSION };
            nfdresult_t result = NFD::OpenDialog(&fileFilter, 1, nullptr, outPath);
            mainWindow.suppressMouseUntilNextDown();
            if (result == NFD_OKAY) {
                filePath = outPath.get();
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
                    // intelligent translation/scale depending on dimensions of canvasstate:
                    //   place circuit in the centre of screen, at the largest possible size where the whole circuit can be seen (but clamped to reasonable bounds)
                    {
                        auto canvas_size = mainWindow.stateManager.defaultState.size();
                        auto render_size = ext::point{ playArea.renderArea.w, playArea.renderArea.h };

                        if (canvas_size.x != 0 && canvas_size.y != 0) {
                            playArea.scale = std::clamp(std::min(render_size.x / canvas_size.x, render_size.y / canvas_size.y), 1, mainWindow.logicalToPhysicalSize(20));
                            playArea.translation = (render_size - canvas_size * playArea.scale) / 2;
                        }
                        else {
                            playArea.translation = { 0, 0 };
                            playArea.scale = 20;
                        }
                        playArea.prepareTexture(mainWindow.renderer);
                    }
                    // imbue the history
                    mainWindow.stateManager.historyManager.imbue(mainWindow.stateManager.defaultState);
                    mainWindow.setUnsaved(false);
                    mainWindow.setFilePath(filePath);
                    // recompile the simulator (this will propagate all the logic levels properly for display)
                    mainWindow.stateManager.simulator.compile(mainWindow.stateManager.defaultState);
                    simulatorRunning = false; // don't restart the simulator
                    break;
                case CanvasState::ReadResult::OUTDATED:
                    mainWindow.getNotificationDisplay().add(NotificationFlags::DEFAULT, 5s, NotificationDisplay::Data{ { "Error opening file: This file was created by a newer version of " CIRCUIT_SANDBOX_STRING ", and cannot be opened here.  Please update " CIRCUIT_SANDBOX_STRING " and try again.", NotificationDisplay::TEXT_COLOR_ERROR } });
                    break;
                case CanvasState::ReadResult::CORRUPTED:
                    mainWindow.getNotificationDisplay().add(NotificationFlags::DEFAULT, 5s, NotificationDisplay::Data{ { "Error opening file: This file is corrupted.", NotificationDisplay::TEXT_COLOR_ERROR } });
                    break;
                case CanvasState::ReadResult::IO_ERROR:
                    mainWindow.getNotificationDisplay().add(NotificationFlags::DEFAULT, 5s, NotificationDisplay::Data{ { "Error opening file: This file cannot be accessed.", NotificationDisplay::TEXT_COLOR_ERROR } });
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

    };

    static inline void start(MainWindow& mainWindow, PlayArea& playArea, const ActionStarter& starter, const char* filePath = nullptr) {
        starter.start<FileOpenAction>(mainWindow, playArea, filePath);
        starter.reset();
    }
};
