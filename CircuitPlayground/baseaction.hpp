#pragma once

#include <SDL.h>

#include "declarations.hpp"

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

class BaseAction {


public:

    // prevent copy/move
    BaseAction() = default;
    BaseAction(BaseAction&&) = delete;
    BaseAction& operator=(BaseAction&&) = delete;
    BaseAction(const BaseAction&) = delete;
    BaseAction& operator=(const BaseAction&) = delete;

    virtual ~BaseAction() {}

    // expects the mouse to be in the playarea
    virtual inline ActionEventResult processMouseButtonDown(const SDL_MouseButtonEvent&) {
        return ActionEventResult::UNPROCESSED;
    }
    // might be outside the playarea if the mouse was dragged out
    virtual inline ActionEventResult processMouseDrag(const SDL_MouseMotionEvent&) {
        return ActionEventResult::UNPROCESSED;
    }// might be outside the playarea if the mouse was dragged out
    virtual inline ActionEventResult processMouseButtonUp(const SDL_MouseButtonEvent&) {
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
    static inline bool startWithMouseButtonDown(const SDL_MouseButtonEvent&, PlayArea&, std::unique_ptr<BaseAction>&) {
        return false;
    }
    static inline bool startWithMouseDrag(const SDL_MouseMotionEvent&, PlayArea&, std::unique_ptr<BaseAction>&) {
        return false;
    }
    static inline bool startWithMouseButtonUp(const SDL_MouseButtonEvent&, PlayArea&, std::unique_ptr<BaseAction>&) {
        return false;
    }
    static inline bool startWithMouseWheel(const SDL_MouseWheelEvent&, PlayArea&, std::unique_ptr<BaseAction>&) {
        return false;
    }
    static inline bool startWithKeyboard(const SDL_KeyboardEvent&, PlayArea&, std::unique_ptr<BaseAction>&) {
        return false;
    }



    // rendering functions
    virtual void renderSurface(uint32_t* pixelBuffer, const SDL_Rect& renderRect) const {}
    virtual void renderDirect(SDL_Renderer*) const {}
};
