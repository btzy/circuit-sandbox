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
#include "fileoutputcommunicator.hpp"

class FileCommunicatorSelectAction final : public PlayAreaAction {
public:
    template <typename FileCommunicator>
    FileCommunicatorSelectAction(MainWindow& mainWindow, FileCommunicator& comm) :PlayAreaAction(mainWindow) {
        // select file
        char* outPath = nullptr;
        nfdresult_t result;
        if constexpr(std::is_same_v<FileInputCommunicator, FileCommunicator>) {
            result = NFD_OpenDialog(nullptr, nullptr, &outPath);
        }
        else if constexpr(std::is_same_v<FileOutputCommunicator, FileCommunicator>) {
            result = NFD_SaveDialog(nullptr, nullptr, &outPath);
        }
        else {
            static_assert(std::is_same_v<FileCommunicator, FileCommunicator>, "Unrecognized communicator type");
        }
        mainWindow.suppressMouseUntilNextDown();
        if (result != NFD_OKAY) {
            outPath = nullptr;
        }
        if (outPath != nullptr) {
            comm.setFile(outPath);

            mainWindow.getNotificationDisplay().add(NotificationFlags::DEFAULT, 5s, NotificationDisplay::Data{
                { "Communicator linked to ", NotificationDisplay::TEXT_COLOR },
                { getFileName(outPath), NotificationDisplay::TEXT_COLOR_FILE }
            });

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
                        if constexpr(std::is_same_v<FileInputCommunicatorElement, ElementType> || std::is_same_v<FileOutputCommunicatorElement, ElementType>) {
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
