#pragma once

#include <utility>
#include <fstream>
#include <algorithm>
#include <limits>

#include <boost/endian/conversion.hpp>
#include <boost/process/spawn.hpp>
#include <SDL.h>
#include <nfd.h>

#include "action.hpp"
#include "mainwindow.hpp"
#include "canvasstate.hpp"
#include "fileutils.hpp"

class FileOpenAction final : public Action {
private:
    enum class ReadResult : char {
        OK,
        OUTDATED, // file is saved in a newer format
        CORRUPTED, // file is corrupted
        IO_ERROR // file cannot be opened or read
    };

    ReadResult readSave(CanvasState& state, const char* filePath) {
        std::ifstream saveFile(filePath, std::ios::binary);
        if (!saveFile.is_open()) return ReadResult::IO_ERROR;

        // read the magic sequence
        char data[4];
        saveFile.read(data, 4);
        if (!std::equal(data, data + 4, CCPG_FILE_MAGIC)) return ReadResult::CORRUPTED;

        // read the version number
        int32_t version;
        saveFile.read(reinterpret_cast<char*>(&version), sizeof version);
        boost::endian::little_to_native_inplace(version);
        if (version != 0) return ReadResult::OUTDATED;

        // read the width and height
        int32_t matrixWidth, matrixHeight;
        saveFile.read(reinterpret_cast<char*>(&matrixWidth), sizeof matrixWidth);
        saveFile.read(reinterpret_cast<char*>(&matrixHeight), sizeof matrixHeight);
        boost::endian::little_to_native_inplace(matrixWidth);
        boost::endian::little_to_native_inplace(matrixHeight);
        if (matrixWidth < 0 || matrixHeight < 0 || static_cast<int64_t>(matrixWidth) * matrixHeight > std::numeric_limits<int32_t>::max()) return ReadResult::CORRUPTED;

        // create the matrix
        CanvasState::matrix_t canvasData(matrixWidth, matrixHeight);

        for (int32_t y = 0; y != canvasData.height(); ++y) {
            for (int32_t x = 0; x != canvasData.width(); ++x) {
                uint8_t elementData;
                saveFile.read(reinterpret_cast<char*>(&elementData), 1);
                size_t element_index = elementData >> 2;
                bool logicLevel = elementData & 0b10;
                bool defaultLogicLevel = elementData & 0b01;

                CanvasState::element_variant_t& element = canvasData[{x, y}];
                if (!CanvasState::element_tags_t::get(element_index, [&](const auto element_tag) {
                    using Element = typename decltype(element_tag)::type;
                    if constexpr (!std::is_same<Element, std::monostate>::value) {
                        element.emplace<Element>(logicLevel, defaultLogicLevel);
                    }
                    return true;
                }, false)) {
                    return ReadResult::OUTDATED; // maybe the new save format contains more elements?
                }
            }
        }

        // only overwriting state.dataMatrix here ensures it is only modified if we return ReadResult::OK.
        state.dataMatrix = std::move(canvasData);
        return ReadResult::OK;
    }

public:
    FileOpenAction(MainWindow& mainWindow, PlayArea& playArea, const char* filePath = nullptr) {

        // stop the simulator if running
        bool simulatorRunning = mainWindow.stateManager.simulator.running();
        if (simulatorRunning) mainWindow.stateManager.simulator.stop();


        // show the file dialog if necessary
        char* outPath = nullptr;
        if (filePath == nullptr) {
            nfdresult_t result = NFD_OpenDialog(CCPG_FILE_EXTENSION, nullptr, &outPath);
            mainWindow.suppressMouseUntilNextDown();
            if (result == NFD_OKAY) {
                filePath = outPath;
            }
        }

        if (filePath != nullptr) { // means that the user wants to open filePath
            if (mainWindow.stateManager.historyManager.empty() && !mainWindow.hasFilePath()) {
                // read from filePath
                ReadResult result = readSave(mainWindow.stateManager.defaultState, filePath);
                switch (result) {
                case ReadResult::OK:
                    // reset the translations
                    mainWindow.stateManager.deltaTrans = { 0, 0 };
                    // TODO: some intelligent translation/scale depending on dimensions of canvasstate
                    playArea.translation = { 0, 0 };
                    playArea.scale = 20;
                    // imbue the history
                    mainWindow.stateManager.historyManager.imbue(mainWindow.stateManager.defaultState);
                    mainWindow.setUnsaved(false);
                    mainWindow.setFilePath(filePath);
                    // recompile the simulator
                    mainWindow.stateManager.simulator.compile(mainWindow.stateManager.defaultState, false);
                    simulatorRunning = false; // don't restart the simulator
                    break;
                case ReadResult::OUTDATED:
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Cannot Open File", "This file was created by a newer version of Circuit Playground, and cannot be opened here.  Please update Circuit Playground and try again.", mainWindow.window);
                    break;
                case ReadResult::CORRUPTED:
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Cannot Open File", "File is corrupted.", mainWindow.window);
                    break;
                case ReadResult::IO_ERROR:
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
