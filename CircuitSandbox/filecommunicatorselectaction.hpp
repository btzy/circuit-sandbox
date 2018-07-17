#pragma once

#include <variant>
#include <SDL.h>
#include <nfd.h>
#include "point.hpp"
#include "playarea.hpp"
#include "mainwindow.hpp"
#include "playareaaction.hpp"
#include "elements.hpp"
#include "fileinputcommunicator.hpp"

class FileCommunicatorSelectAction final : public PlayAreaAction {
public:
    FileCommunicatorSelectAction(MainWindow& mainWindow, FileInputCommunicator& comm) :PlayAreaAction(mainWindow) {
        // select file
        char* outPath = nullptr;
        nfdresult_t result = NFD_OpenDialog(nullptr, nullptr, &outPath);
        mainWindow.suppressMouseUntilNextDown();
        if (result != NFD_OKAY) {
            outPath = nullptr;
        }
        if (outPath != nullptr) {
            comm.setFile(outPath);
            free(outPath);

            // save to history, but never recompile
            changed() = true;
            stateManager().saveToHistory();
        }
    }
    ~FileCommunicatorSelectAction() override {}

    // check if we need to start interaction by clicking the playarea
    static inline ActionEventResult startWithPlayAreaMouseButtonDown(const SDL_MouseButtonEvent& event, MainWindow& mainWindow, PlayArea& playArea, const ActionStarter& starter) {
        size_t inputHandleIndex = resolveInputHandleIndex(event);
        size_t currentToolIndex = mainWindow.selectedToolIndices[inputHandleIndex];

        return tool_tags_t::get(currentToolIndex, [&](const auto tool_tag) {
            // 'Tool' is the type of tool (e.g. Selector)
            using Tool = typename decltype(tool_tag)::type;

            if constexpr (std::is_base_of_v<Interactor, Tool>) {
                ext::point canvasOffset = playArea.canvasFromWindowOffset(event);
                if (mainWindow.stateManager.defaultState.contains(canvasOffset)){
                    return std::visit([&](auto& element) {
                        using ElementType = std::decay_t<decltype(element)>;
                        if constexpr(std::is_same_v<FileInputCommunicatorElement, ElementType>) {
                            // Only start the file input action if the mouse was pressed down over a File Input Communicator.
                            starter.start<FileCommunicatorSelectAction>(mainWindow, *element.communicator);
                            return ActionEventResult::PROCESSED;
                        }
                        else {
                            return ActionEventResult::UNPROCESSED;
                        }
                    }, mainWindow.stateManager.defaultState[canvasOffset]);
                }
                else {
                    return ActionEventResult::UNPROCESSED;
                }
            }
            else {
                return ActionEventResult::UNPROCESSED;
            }
        }, ActionEventResult::UNPROCESSED);
    }
};
