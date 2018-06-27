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

    // TODO: all these virtual methods should come from EventHook instead of from Action.
    // expects the mouse to be in the playarea
    /*virtual inline ActionEventResult processMouseHover(const SDL_MouseMotionEvent&) {
        return ActionEventResult::UNPROCESSED;
    }
    virtual inline ActionEventResult processMouseLeave() {
        return ActionEventResult::UNPROCESSED;
    }
    // expects the mouse to be in the playarea
    virtual inline ActionEventResult processMouseButtonDown(const SDL_MouseButtonEvent&) {
        return ActionEventResult::UNPROCESSED;
    }
    // might be outside the playarea if the mouse was dragged out
    virtual inline ActionEventResult processMouseDrag(const SDL_MouseMotionEvent&) {
        return ActionEventResult::UNPROCESSED;
    }
    // might be outside the playarea if the mouse was dragged out
    virtual inline ActionEventResult processMouseButtonUp() {
        return ActionEventResult::UNPROCESSED;
    }

    // expects the mouse to be in the playarea
    virtual inline ActionEventResult processMouseWheel(const SDL_MouseWheelEvent&) {
        return ActionEventResult::UNPROCESSED;
    }*/


    // rendering functions
    /**
     * Render directly using the SDL_Renderer (it is in window coordinates).
     */
    virtual void render(SDL_Renderer*) const {}
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
};
