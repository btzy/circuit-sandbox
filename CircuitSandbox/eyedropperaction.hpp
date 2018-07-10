#pragma once

#include <type_traits>
#include <variant>

#include <SDL.h>
#include "point.hpp"
#include "playareaaction.hpp"
#include "playarea.hpp"
#include "mainwindow.hpp"
#include "visitor.hpp"

class EyedropperAction final : public PlayAreaAction, public KeyboardEventHook {
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
    EyedropperAction(MainWindow& mainWindow, SDL_Renderer* renderer) : PlayAreaAction(mainWindow), KeyboardEventHook(mainWindow) {}

    ~EyedropperAction() {}

    static inline void start(MainWindow& mainWindow, SDL_Renderer* renderer, const ActionStarter& starter) {
        starter.start<EyedropperAction>(mainWindow, renderer);
    }

    ActionEventResult processPlayAreaMouseButtonDown(const SDL_MouseButtonEvent& event) {
        ext::point canvasOffset = playArea().canvasFromWindowOffset(event);
        size_t inputHandleIndex = resolveInputHandleIndex(event);

        return std::visit(visitor{
            [](std::monostate) {
                return ActionEventResult::PROCESSED;
            },
            [&, this](const auto& element) {
                bindToolFromElement(inputHandleIndex, element);
                return ActionEventResult::COMPLETED;
            }
        }, canvas()[canvasOffset]);
    }

    ActionEventResult processWindowKeyboard(const SDL_KeyboardEvent& event) override {
        if (event.type == SDL_KEYUP) {
            if (event.keysym.scancode == SDL_SCANCODE_E) {
                return ActionEventResult::COMPLETED;
            }
        }
        return ActionEventResult::PROCESSED;
    }

    const char* getStatus() const override {
        return "Eyedropper active";
    }
};
