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

#include "mainwindow.hpp"
#include "keyboardeventreceiver.hpp"
#include "control.hpp"

/**
 * Helper classes for letting actions receive MainWindow events.
 */

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
        win.registrar.push<KeyboardEventReceiver>(this);
    }
    ~KeyboardEventHook() {
        win.registrar.pop<KeyboardEventReceiver>(this);
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

class MainWindowEventHook : public Control {
private:
    MainWindow& win;
public:
    MainWindowEventHook(MainWindow& mainWindow, const SDL_Rect& renderArea) : win(mainWindow) {
        this->renderArea = renderArea;
        win.registrar.push<Drawable, Control, KeyboardEventReceiver>(this);
    }
    ~MainWindowEventHook() {
        if (win.currentLocationTarget == this) {
            win.currentLocationTarget = nullptr;
        }
        if (win.currentEventTarget == this) {
            win.currentEventTarget = nullptr;
        }
        win.registrar.pop<Drawable, Control, KeyboardEventReceiver>(this);
    }

    void processMouseHover(const SDL_MouseMotionEvent& event) override {
        forwardEvent([&]() {
            return processWindowMouseHover(event);
        }, [&]() {
            win.currentAction.reset();
        });
    }

    void processMouseLeave() override {
        forwardEvent([&]() {
            return processWindowMouseLeave();
        }, [&]() {
            win.currentAction.reset();
        });
    }

    bool processMouseButtonDown(const SDL_MouseButtonEvent& event) override {
        return forwardEvent([&]() {
            return processWindowMouseButtonDown(event);
        }, [&]() {
            win.currentAction.reset();
        });
    }

    void processMouseDrag(const SDL_MouseMotionEvent& event) override {
        forwardEvent([&]() {
            return processWindowMouseDrag(event);
        }, [&]() {
            win.currentAction.reset();
        });
    }

    void processMouseButtonUp() override {
        forwardEvent([&]() {
            return processWindowMouseButtonUp();
        }, [&]() {
            win.currentAction.reset();
        });
    }

    bool processMouseWheel(const SDL_MouseWheelEvent& event) override {
        return forwardEvent([&]() {
            return processWindowMouseWheel(event);
        }, [&]() {
            win.currentAction.reset();
        });
    }

    bool processKeyboard(const SDL_KeyboardEvent& event) override {
        return forwardEvent([&]() {
            return processWindowKeyboard(event);
        }, [&]() {
            win.currentAction.reset();
        });
    }

    bool processTextInput(const SDL_TextInputEvent& event) override {
        return forwardEvent([&]() {
            return processWindowTextInput(event);
        }, [&]() {
            win.currentAction.reset();
        });
    }

    // expects the mouse to be in the renderArea
    virtual ActionEventResult processWindowMouseHover(const SDL_MouseMotionEvent&) {
        return ActionEventResult::PROCESSED;
    }
    virtual ActionEventResult processWindowMouseLeave() {
        return ActionEventResult::PROCESSED;
    }
    // expects the mouse to be in the renderArea
    virtual ActionEventResult processWindowMouseButtonDown(const SDL_MouseButtonEvent&) {
        return ActionEventResult::PROCESSED;
    }
    // might be outside the renderArea if the mouse was dragged out
    virtual ActionEventResult processWindowMouseDrag(const SDL_MouseMotionEvent&) {
        return ActionEventResult::PROCESSED;
    }
    // might be outside the renderArea if the mouse was dragged out
    virtual ActionEventResult processWindowMouseButtonUp() {
        return ActionEventResult::PROCESSED;
    }

    // expects the mouse to be in the renderArea
    virtual ActionEventResult processWindowMouseWheel(const SDL_MouseWheelEvent&) {
        return ActionEventResult::PROCESSED;
    }
    // mouse might be anywhere, so shortcut keys are not dependent on the mouse position
    virtual ActionEventResult processWindowKeyboard(const SDL_KeyboardEvent&) {
        return ActionEventResult::PROCESSED;
    }
    // mouse might be anywhere, so shortcut keys are not dependent on the mouse position
    virtual ActionEventResult processWindowTextInput(const SDL_TextInputEvent&) {
        return ActionEventResult::PROCESSED;
    }

    // renderer
    void render(SDL_Renderer* renderer) override {}
};
