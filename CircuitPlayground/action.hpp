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
    

public:
    // disable all the copy and move constructors and assignment operators, because this class is intended to be a 'policy' type class
    

    Action(PlayArea& playArea) : playArea(playArea) {}

    ~Action() {}


    void reset() {
        data.emplace<DummyAction>();
    }


    // expects the mouse to be in the playarea
    inline ActionEventResult processMouseMotionEvent(const SDL_MouseMotionEvent& event) {
        return std::visit([&event](auto& action) {
            return action.processMouseMotionEvent(event);
        }, data);
    }

    // expects the mouse to be in the playarea
    inline ActionEventResult processMouseButtonEvent(const SDL_MouseButtonEvent& event) {
        return std::visit([&event](auto& action) {
            return action.processMouseButtonEvent(event);
        }, data);
    }

    // should we expect the mouse to be in the playarea?
    inline ActionEventResult processMouseWheelEvent(const SDL_MouseWheelEvent& event) {
        return std::visit([&event](auto& action) {
            return action.processMouseWheelEvent(event);
        }, data);
    }

    inline ActionEventResult processKeyboardEvent(const SDL_KeyboardEvent& event) {
        return std::visit([&event](auto& action) {
            return action.processKeyboardEvent(event);
        }, data);
    }

    // expects to be called when the mouse leaves the playarea
    inline ActionEventResult processMouseLeave() {
        return std::visit([](auto& action) {
            return action.processMouseLeave();
        }, data);
    }

    // action creators, returns true if action was constructed
    inline bool startWithMouseMotionEvent(const SDL_MouseMotionEvent& event) {
        return actions_tag_t::for_each([this, &event](auto action_tag, auto) {
            using action_t = typename decltype(action_tag)::type;
            return action_t::startWithMouseMotionEvent(event, playArea, data);
        });
    }
    inline bool startWithMouseButtonEvent(const SDL_MouseButtonEvent& event) {
        return actions_tag_t::for_each([this, &event](auto action_tag, auto) {
            using action_t = typename decltype(action_tag)::type;
            return action_t::startWithMouseButtonEvent(event, playArea, data);
        });
    }
    inline bool startWithMouseWheelEvent(const SDL_MouseWheelEvent& event) {
        return actions_tag_t::for_each([this, &event](auto action_tag, auto) {
            using action_t = typename decltype(action_tag)::type;
            return action_t::startWithMouseWheelEvent(event, playArea, data);
        });
    }
    inline bool startWithKeyboardEvent(const SDL_KeyboardEvent& event) {
        return actions_tag_t::for_each([this, &event](auto action_tag, auto) {
            using action_t = typename decltype(action_tag)::type;
            return action_t::startWithKeyboardEvent(event, playArea, data);
        });
    }
    inline bool startWithMouseLeave() {
        return actions_tag_t::for_each([this](auto action_tag, auto) {
            using action_t = typename decltype(action_tag)::type;
            return action_t::startWithMouseLeave(playArea, data);
        });
    }

    // renderer
    void render(SDL_Renderer* renderer) const {
        std::visit([&renderer](const auto& action) {
            action.render(renderer);
        }, data);
    }
};