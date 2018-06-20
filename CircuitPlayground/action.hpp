#pragma once

#include <memory>

#include <SDL.h>

#include "tag_tuple.hpp"
#include "point.hpp"

#include "declarations.hpp"

#include "baseaction.hpp"


/**
 * This class encapsulates an 'action', which is the fundamental unit of change that the user can make on the canvas state.
 * When an action is started, the simulator will stop.
 * Actions span an arbitrary amount of time.
 * The Action object should not be destroyed until the game is quitting.
 * An action may modify defaultState and deltaTrans arbitrarily during the action,
 * but it should take care that rendering is done properly, because by default rendering uses defaultState.
 * It is guaranteed that defaultState and deltaTrans will not be used by any other code (apart from rendering) when an action is in progress.
 */

class Action {

private:

    PlayArea& playArea;

    std::unique_ptr<BaseAction> data;

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

    Action(PlayArea& playArea) : playArea(playArea), data(std::make_unique<BaseAction>()) {}

    ~Action() {}

    void reset() {
        start<BaseAction>();
    }

    template <typename T, typename... Args>
    T& start(Args&&... args) {
        data = nullptr; // destroy the old action
        data = std::make_unique<T>(std::forward<Args>(args)...); // construct the new action
        return static_cast<T&>(*data);
    }

    // requirements here follow those in BaseAction.
    bool processMouseButtonDown(const SDL_MouseButtonEvent&);
    bool processMouseDrag(const SDL_MouseMotionEvent&);
    bool processMouseButtonUp(const SDL_MouseButtonEvent&);

    bool processMouseWheel(const SDL_MouseWheelEvent&);
    bool processKeyboard(const SDL_KeyboardEvent&);

    // renderer
    bool disableDefaultRender() const;
    void renderSurface(uint32_t* pixelBuffer, const SDL_Rect& renderRect) const;
    void renderDirect(SDL_Renderer* renderer) const;
};
