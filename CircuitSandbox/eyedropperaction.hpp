#pragma once

#include <type_traits>
#include <variant>

#include <SDL.h>
#include "point.hpp"
#include "playareaaction.hpp"
#include "playarea.hpp"
#include "mainwindow.hpp"
#include "visitor.hpp"

class EyedropperAction final : public PlayAreaAction {
private:

    template <typename Element>
    void bindToolFromElement(size_t inputHandleIndex, Element element) {
        tool_tags_t::for_each([this, inputHandleIndex](const auto tool_tag, const auto index_tag){
            // 'Tool' is the type of tool (e.g. ConductiveWire)
            using Tool = typename decltype(tool_tag)::type;
            if constexpr(std::is_same_v<Element, Tool>) {
                // 'index' is the index of this element inside the tool_tags_t
                constexpr size_t index = decltype(index_tag)::value;
                mainWindow.bindTool(inputHandleIndex, index);
            }
        });
    }

public:
    EyedropperAction(MainWindow& mainWindow) : PlayAreaAction(mainWindow) {}

    ~EyedropperAction() {}

    static inline ActionEventResult startWithPlayAreaMouseButtonDown(const SDL_MouseButtonEvent& event, MainWindow& mainWindow, PlayArea& playArea, const ActionStarter& starter) {
        ext::point canvasOffset = playArea.canvasFromWindowOffset(event);
        size_t inputHandleIndex = resolveInputHandleIndex(event);

        SDL_Keymod modifiers = SDL_GetModState();
        if (modifiers & KMOD_CTRL) {
            auto& action = starter.start<EyedropperAction>(mainWindow);
            std::visit(visitor{
                [](std::monostate) {},
                [&](const auto& element) {
                    action.bindToolFromElement(inputHandleIndex, element);
                }
            }, action.canvas()[canvasOffset]);
            return ActionEventResult::COMPLETED;
        }
        return ActionEventResult::UNPROCESSED;
    }
};
