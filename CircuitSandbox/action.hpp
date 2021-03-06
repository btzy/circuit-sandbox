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

#include <memory>
#include <SDL.h>

/**
 * An action stores the context for a group of related functionality involves multiple events.
 * Actions span an arbitrary amount of time.
 * The Action object should not be destroyed until the game is quitting.
 * ActionManager guarantees that only one action can be active at any moment.
 */

/**
 * ActionEventResult represents the result of all the event handlers in actions.
 * PROCESSED: This action successfully used the event, so PlayArea should not use it.  The action should remain active.
 * UNPROCESSED: This action did not use the event, so PlayArea should use it accordingly, possibly exiting (destroying) this action.
 * COMPLETED: This action successfully used the event, so PlayArea should not use it.  PlayArea should exit (destroy) this action immediately, and must not call any other methods on this action.
 * CANCELLED: This action did not use the event, so PlayArea should use it accordingly.  PlayArea should exit (destroy) this action immediately, and must not call any other methods on this action.
 */
enum class ActionEventResult {
    PROCESSED,
    UNPROCESSED,
    COMPLETED,
    CANCELLED
};

class ActionStarter;


/**
 * This is the base class for all actions.
 * All derived classes may assume that mouse button/drag actions happen in this order:
 * * MouseButtonDown --> MouseDrag (any number of times, possibly zero) --> MouseUp
 * Furthermore, two different mouse buttons cannot be pressed at the same time
 * All derived classes may assume that rendering happens in this order:
 * * disableDefaultRender --> (default rendering, if not disabled) --> renderSurface --> renderDirect
 * For MouseButtonDown and MouseWheel, it is guaranteed that the mouse is inside the playarea.
 */
class Action {

public:
    // prevent copy/move
    Action() = default;
    Action(Action&&) = delete;
    Action& operator=(Action&&) = delete;
    Action(const Action&) = delete;
    Action& operator=(const Action&) = delete;

    virtual ~Action() {}
};

class ActionStarter {
private:
    std::unique_ptr<Action>& data;
    explicit ActionStarter(std::unique_ptr<Action>& data) :data(data) {}
    friend class ActionManager;
public:
    template <typename T, typename... Args>
    T& start(Args&&... args) const {
        reset(); // destroy the old action
        data = std::make_unique<T>(std::forward<Args>(args)...); // construct the new action
        return static_cast<T&>(*data);
    }
    void reset() const {
        data = nullptr;
    }

    // for transferring ownership of actions
    using Handle = std::unique_ptr<Action>;
    Handle steal() const noexcept {
        return std::move(data);
    }

    void imbue(Handle&& handle) const {
        data = std::move(handle);
    }
};
