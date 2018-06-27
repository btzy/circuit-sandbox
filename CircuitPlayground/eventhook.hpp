#pragma once

#include "mainwindow.hpp"

namespace {
    template <typename Executor, typename Resetter>
    inline bool forwardEvent(Executor&& executor, Resetter&& resetter) {
        ActionEventResult result = std::forward<Executor>(executor)();
        switch (result) {
        case ActionEventResult::COMPLETED:
            std::forward<Resetter>(resetter)();
            [[fallthrough]];
        case ActionEventResult::PROCESSED:
            return true;
        case ActionEventResult::CANCELLED:
            std::forward<Resetter>(resetter)();
            [[fallthrough]];
        case ActionEventResult::UNPROCESSED:
            return false;
        }
        return false;
    }
}

class KeyboardEventHook : public KeyboardEventReceiver {
private:
    MainWindow& win;
public:
    KeyboardEventHook(MainWindow& mainWindow) : win(mainWindow) {
        win.keyboardEventReceivers.emplace_back(this);
    }
    ~KeyboardEventHook() {
        win.keyboardEventReceivers.pop_back();
    }

    bool processKeyboard(const SDL_KeyboardEvent& event) override {
        return forwardEvent([&]() {
            return processWindowKeyboard(event);
        }, [&]() {
            win.currentAction.reset();
        });
    }

    // mouse might be anywhere, so shortcut keys are not dependent on the mouse position
    virtual inline ActionEventResult processWindowKeyboard(const SDL_KeyboardEvent&) {
        return ActionEventResult::UNPROCESSED;
    }
};
