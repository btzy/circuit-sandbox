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

#include <variant>
#include <SDL.h>
#include <nfd.hpp>
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
        NFD::UniquePath outPath;
        nfdresult_t result;
        if constexpr(std::is_same_v<FileInputCommunicator, FileCommunicator>) {
            result = NFD::OpenDialog(outPath);
        }
        else if constexpr(std::is_same_v<FileOutputCommunicator, FileCommunicator>) {
            result = NFD::SaveDialog(outPath);
        }
        else {
            static_assert(std::is_same_v<FileCommunicator, FileCommunicator>, "Unrecognized communicator type");
        }
        mainWindow.suppressMouseUntilNextDown();
        if (outPath) {
            comm.setFile(outPath.get());

            mainWindow.getNotificationDisplay().add(NotificationFlags::DEFAULT, 5s, NotificationDisplay::Data{
                { "Communicator linked to ", NotificationDisplay::TEXT_COLOR },
                { getFileName(outPath.get()), NotificationDisplay::TEXT_COLOR_FILE }
            });

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
