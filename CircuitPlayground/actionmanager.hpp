#pragma once

#include <memory>

#include <SDL.h>

#include "declarations.hpp"

#include "action.hpp"

/**
 * ActionManager holds an action and ensures that only one action can be active at any moment.
 * When a new action is started, the previous is destroyed.
 */

class ActionManager {

private:

    PlayArea& playArea;

    std::unique_ptr<Action> data;

    /**
     * returns true if event is consumed, false otherwise
     */
    template<typename ProcessCallback, typename StartWithCallback>
    inline bool forwardEvent(ProcessCallback&& process, StartWithCallback&& startWith) {
        ActionEventResult result = std::forward<ProcessCallback>(process)();
        switch (result) {
        case ActionEventResult::COMPLETED:
            reset();
            [[fallthrough]];
        case ActionEventResult::PROCESSED:
            return true;
        case ActionEventResult::CANCELLED:
            reset();
            [[fallthrough]];
        case ActionEventResult::UNPROCESSED:
            break;
        }

        // have to ask all the actions if they would like to start
        ActionEventResult startResult = std::forward<StartWithCallback>(startWith)();
        switch (startResult) {
        case ActionEventResult::COMPLETED:
            reset();
            [[fallthrough]];
        case ActionEventResult::PROCESSED:
            return true;
        case ActionEventResult::CANCELLED:
            reset();
            [[fallthrough]];
        case ActionEventResult::UNPROCESSED:
            break;
        }

        return false;
    }

public:
    // disable all the copy and move constructors and assignment operators, because this class is intended to be a 'policy' type class

    ActionManager(PlayArea& playArea) : playArea(playArea), data(std::make_unique<Action>()) {}

    ~ActionManager() {}

    void reset() {
        start<Action>();
    }

    template <typename T, typename... Args>
    T& start(Args&&... args) {
        data = nullptr; // destroy the old action
        data = std::make_unique<T>(std::forward<Args>(args)...); // construct the new action
        return static_cast<T&>(*data);
    }

    // requirements here follow those in Action.
    bool processMouseButtonDown(const SDL_MouseButtonEvent&);
    bool processMouseDrag(const SDL_MouseMotionEvent&);
    bool processMouseButtonUp();

    bool processMouseWheel(const SDL_MouseWheelEvent&);
    bool processKeyboard(const SDL_KeyboardEvent&);

    // renderer
    bool disableDefaultRender() const;
    void renderSurface(uint32_t* pixelBuffer, const SDL_Rect& renderRect) const;
    void renderDirect(SDL_Renderer* renderer) const;
};
