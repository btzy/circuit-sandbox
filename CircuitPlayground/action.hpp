#pragma once

#include <variant>
#include <optional>

#include <SDL.h>

#include "tag_tuple.hpp"
#include "point.hpp"

#include "baseaction.hpp"


/**
 * This class encapsulates an 'action', which is the fundamental unit of change that the user can make on the canvas state.
 * When an action is started, the simulator will stop.
 * Actions span an arbitrary amount of time.
 * The action object should not be destroyed until the game is quitting.
 */




class DummyAction : public BaseAction {};



template <typename PlayArea, template <typename> typename... Actions>
class Action {

private:

    PlayArea& playArea;

    using actions_tag_t = typename extensions::tag_tuple<DummyAction, Actions<PlayArea>...>;
    using variant_t = typename actions_tag_t::template instantiate<std::variant>;
    variant_t data;
    
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

            // have to ask all the actions if they would like to start
            if (std::forward<StartWithCallback>(startWith)()) {
                return true;
            }

        }

        return false;
    }

public:
    // disable all the copy and move constructors and assignment operators, because this class is intended to be a 'policy' type class
    

    Action(PlayArea& playArea) : playArea(playArea) {}

    ~Action() {}


    void reset() {
        data.emplace<DummyAction>();
    }



    // returns true if the event was consumed, false otherwise
    inline bool processMouseMotionEvent(const SDL_MouseMotionEvent& event) {
        return forwardEvent([&, this]() {
            return std::visit([&](auto& action) {
                return action.processMouseMotionEvent(event);
            }, data);
        }, [&, this]() {
            return actions_tag_t::for_each([&, this](auto action_tag, auto) {
                using action_t = typename decltype(action_tag)::type;
                return action_t::startWithMouseMotionEvent(event, playArea, data);
            });
        });
    }

    // expects the mouse to be in the playarea, unless it is a mouseup
    // returns true if the event was consumed, false otherwise
    inline bool processMouseButtonEvent(const SDL_MouseButtonEvent& event) {
        return forwardEvent([&, this]() {
            return std::visit([&](auto& action) {
                return action.processMouseButtonEvent(event);
            }, data);
        }, [&, this]() {
            return actions_tag_t::for_each([&, this](auto action_tag, auto) {
                using action_t = typename decltype(action_tag)::type;
                return action_t::startWithMouseButtonEvent(event, playArea, data);
            });
        });
    }

    // should we expect the mouse to be in the playarea?
    // returns true if the event was consumed, false otherwise
    inline bool processMouseWheelEvent(const SDL_MouseWheelEvent& event) {
        return forwardEvent([&, this]() {
            return std::visit([&](auto& action) {
                return action.processMouseWheelEvent(event);
            }, data);
        }, [&, this]() {
            return actions_tag_t::for_each([&, this](auto action_tag, auto) {
                using action_t = typename decltype(action_tag)::type;
                return action_t::startWithMouseWheelEvent(event, playArea, data);
            });
        });
    }

    // returns true if the event was consumed, false otherwise
    inline bool processKeyboardEvent(const SDL_KeyboardEvent& event) {
        return forwardEvent([&, this]() {
            return std::visit([&](auto& action) {
                return action.processKeyboardEvent(event);
            }, data);
        }, [&, this]() {
            return actions_tag_t::for_each([&, this](auto action_tag, auto) {
                using action_t = typename decltype(action_tag)::type;
                return action_t::startWithKeyboardEvent(event, playArea, data);
            });
        });
    }

    // expects to be called when the mouse leaves the playarea
    // returns true if the event was consumed, false otherwise
    inline bool processMouseLeave() {
        return forwardEvent([this]() {
            return std::visit([](auto& action) {
                return action.processMouseLeave();
            }, data);
        }, [this]() {
            return actions_tag_t::for_each([this](auto action_tag, auto) {
                using action_t = typename decltype(action_tag)::type;
                return action_t::startWithMouseLeave(playArea, data);
            });
        });
    }

    // renderer
    void render(SDL_Renderer* renderer) const {
        std::visit([&renderer](const auto& action) {
            action.render(renderer);
        }, data);
    }
};