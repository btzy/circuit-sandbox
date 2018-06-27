#pragma once

#include "mainwindow.hpp"

class KeyboardEventHook {
private:
    MainWindow& win;
    class Receiver : public KeyboardEventReceiver {
    private:
        KeyboardEventHook& hook;
    public:
        bool processKeyboard(const SDL_KeyboardEvent& event) override {
            ActionEventResult result = hook.processKeyboard(event);
            switch (result) {
            case ActionEventResult::COMPLETED:
                hook.win.currentAction.reset();
                [[fallthrough]];
            case ActionEventResult::PROCESSED:
                return true;
            case ActionEventResult::CANCELLED:
                hook.win.currentAction.reset();
                [[fallthrough]];
            case ActionEventResult::UNPROCESSED:
                return false;
            }
        }
        Receiver(KeyboardEventHook& hook) : hook(hook) {}
    } receiver;
public:
    KeyboardEventHook(MainWindow& mainWindow) : win(mainWindow), receiver(*this) {
        win.keyboardEventReceivers.emplace_back(&receiver);
    }
    ~KeyboardEventHook() {
        win.keyboardEventReceivers.pop_back();
    }

    // mouse might be anywhere, so shortcut keys are not dependent on the mouse position
    virtual inline ActionEventResult processKeyboard(const SDL_KeyboardEvent&) {
        return ActionEventResult::UNPROCESSED;
    }
};
