#pragma once

#include <SDL.h>

#include "declarations.hpp"
#include "elements.hpp"

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

    // expects the mouse to be in the playarea
    virtual inline ActionEventResult processMouseButtonDown(const SDL_MouseButtonEvent&) {
        return ActionEventResult::UNPROCESSED;
    }
    // might be outside the playarea if the mouse was dragged out
    virtual inline ActionEventResult processMouseDrag(const SDL_MouseMotionEvent&) {
        return ActionEventResult::UNPROCESSED;
    }// might be outside the playarea if the mouse was dragged out
    virtual inline ActionEventResult processMouseButtonUp() {
        return ActionEventResult::UNPROCESSED;
    }

    // expects the mouse to be in the playarea
    virtual inline ActionEventResult processMouseWheel(const SDL_MouseWheelEvent&) {
        return ActionEventResult::UNPROCESSED;
    }

    // mouse might be anywhere, so shortcut keys are not dependent on the mouse position
    virtual inline ActionEventResult processKeyboard(const SDL_KeyboardEvent&) {
        return ActionEventResult::UNPROCESSED;
    }




    // static methods to create actions, writes to the ActionVariant& to start it (and destroy the previous action, if any)
    // returns true if an action was started, false otherwise
    static inline ActionEventResult startWithMouseButtonDown(const SDL_MouseButtonEvent&, PlayArea&, const ActionStarter&) {
        return ActionEventResult::UNPROCESSED;
    }
    static inline ActionEventResult startWithMouseDrag(const SDL_MouseMotionEvent&, PlayArea&, const ActionStarter&) {
        return ActionEventResult::UNPROCESSED;
    }
    static inline ActionEventResult startWithMouseButtonUp(PlayArea&, const ActionStarter&) {
        return ActionEventResult::UNPROCESSED;
    }
    static inline ActionEventResult startWithMouseWheel(const SDL_MouseWheelEvent&, PlayArea&, const ActionStarter&) {
        return ActionEventResult::UNPROCESSED;
    }
    static inline ActionEventResult startWithKeyboard(const SDL_KeyboardEvent&, PlayArea&, const ActionStarter&) {
        return ActionEventResult::UNPROCESSED;
    }



    // rendering functions
    /**
     * Disable the default rendering function, which draws everything from StateManager::defaultState.
     * Use this if you have modified StateManager::defaultState and don't want to show the modifications,
     * or you want to draw something that covers the whole playarea.
     */
    virtual bool disableDefaultRender() const {
        return false;
    }
    /**
     * Render on a surface (in canvas units) using its pixel buffer.
     * `renderRect` is the part of the canvas that should be drawn onto this surface.
     */
    virtual void renderSurface(uint32_t* pixelBuffer, const SDL_Rect& renderRect) const {}
    /**
     * Render directly using the SDL_Renderer (it is in playarea coordinates).
     */
    virtual void renderDirect(SDL_Renderer*) const {}
};

class ActionStarter {
private:
    std::unique_ptr<Action>& data;
    explicit ActionStarter(std::unique_ptr<Action>& data) :data(data) {}
    friend class ActionManager;
public:
    template <typename T, typename... Args>
    T& start(Args&&... args) const {
        data = nullptr; // destroy the old action
        data = std::make_unique<T>(std::forward<Args>(args)...); // construct the new action
        return static_cast<T&>(*data);
    }
};
