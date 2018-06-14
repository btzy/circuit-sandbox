#pragma once

#include <SDL.h>

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


    // expects the mouse to be in the playarea
    inline ActionEventResult processMouseMotionEvent(const SDL_MouseMotionEvent&) {
        return ActionEventResult::UNPROCESSED;
    }

    // expects the mouse to be in the playarea
    inline ActionEventResult processMouseButtonEvent(const SDL_MouseButtonEvent&) {
        return ActionEventResult::UNPROCESSED;
    }

    // should we expect the mouse to be in the playarea?
    inline ActionEventResult processMouseWheelEvent(const SDL_MouseWheelEvent&) {
        return ActionEventResult::UNPROCESSED;
    }

    inline ActionEventResult processKeyboardEvent(const SDL_KeyboardEvent&) {
        return ActionEventResult::UNPROCESSED;
    }

    // expects to be called when the mouse leaves the playarea
    inline ActionEventResult processMouseLeave() {
        return ActionEventResult::UNPROCESSED;
    }



    // static methods to create actions, writes to the ActionVariant& to start it (and destroy the previous action, if any)
    // returns true if an action was started, false otherwise
    template <typename ActionVariant>
    static inline bool startWithMouseMotionEvent(const SDL_MouseMotionEvent&, PlayArea&, ActionVariant&) {
        return false;
    }
    template <typename ActionVariant>
    static inline bool startWithMouseButtonEvent(const SDL_MouseButtonEvent&, PlayArea&, ActionVariant&) {
        return false;
    }
    template <typename ActionVariant>
    static inline bool startWithMouseWheelEvent(const SDL_MouseWheelEvent&, PlayArea&, ActionVariant&) {
        return false;
    }
    template <typename ActionVariant>
    static inline bool startWithKeyboardEvent(const SDL_KeyboardEvent&, PlayArea&, ActionVariant&) {
        return false;
    }
    template <typename ActionVariant>
    static inline bool startWithMouseLeave(PlayArea&, ActionVariant&) {
        return false;
    }



    // rendering function
    void render(SDL_Renderer*) const {}
};



// a helper class that abstracts out converting mouse coordinates to canvas offset
template <typename T, typename PlayArea>
class CanvasAction : public BaseAction {
protected:
    PlayArea& playArea;

public:

    CanvasAction(PlayArea& playArea) :playArea(playArea) {};


    // save to history when this action ends
    ~CanvasAction() {
        playArea.stateManager.saveToHistory();
    }



    // resolve the canvas offset then forward
    ActionEventResult processMouseMotionEvent(const SDL_MouseMotionEvent& event) {
        extensions::point physicalOffset = extensions::point{ event.x, event.y } -extensions::point{ playArea.renderArea.x, playArea.renderArea.y };
        extensions::point canvasOffset = playArea.computeCanvasCoords(physicalOffset);
        return static_cast<T*>(this)->processCanvasMouseMotionEvent(canvasOffset, event);
    }

    // resolve the canvas offset then forward
    ActionEventResult processMouseButtonEvent(const SDL_MouseButtonEvent& event) {
        extensions::point physicalOffset = extensions::point{ event.x, event.y } -extensions::point{ playArea.renderArea.x, playArea.renderArea.y };
        extensions::point canvasOffset = playArea.computeCanvasCoords(physicalOffset);
        return static_cast<T*>(this)->processCanvasMouseButtonEvent(canvasOffset, event);
    }
};