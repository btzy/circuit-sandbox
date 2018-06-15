#include "action.hpp"
#include "selectionaction.hpp"
#include "pencilaction.hpp"

// returns true if the event was consumed, false otherwise
bool Action::processMouseMotionEvent(const SDL_MouseMotionEvent& event) {
    return forwardEvent([&, this]() {
        return data->processMouseMotionEvent(event);
    }, [&, this]() {
        return action_tags_t::for_each([&, this](auto action_tag, auto) {
            using action_t = typename decltype(action_tag)::type;
            return action_t::startWithMouseMotionEvent(event, playArea, data);
        });
    });
}

// expects the mouse to be in the playarea, unless it is a mouseup
// returns true if the event was consumed, false otherwise
bool Action::processMouseButtonEvent(const SDL_MouseButtonEvent& event) {
    return forwardEvent([&, this]() {
        return data->processMouseButtonEvent(event);
    }, [&, this]() {
        return action_tags_t::for_each([&, this](auto action_tag, auto) {
            using action_t = typename decltype(action_tag)::type;
            return action_t::startWithMouseButtonEvent(event, playArea, data);
        });
    });
}

// should we expect the mouse to be in the playarea?
// returns true if the event was consumed, false otherwise
bool Action::processMouseWheelEvent(const SDL_MouseWheelEvent& event) {
    return forwardEvent([&, this]() {
        return data->processMouseWheelEvent(event);
    }, [&, this]() {
        return action_tags_t::for_each([&, this](auto action_tag, auto) {
            using action_t = typename decltype(action_tag)::type;
            return action_t::startWithMouseWheelEvent(event, playArea, data);
        });
    });
}

// returns true if the event was consumed, false otherwise
bool Action::processKeyboardEvent(const SDL_KeyboardEvent& event) {
    return forwardEvent([&, this]() {
        return data->processKeyboardEvent(event);
    }, [&, this]() {
        return action_tags_t::for_each([&, this](auto action_tag, auto) {
            using action_t = typename decltype(action_tag)::type;
            return action_t::startWithKeyboardEvent(event, playArea, data);
        });
    });
}

// expects to be called when the mouse leaves the playarea
// returns true if the event was consumed, false otherwise
bool Action::processMouseLeave() {
    return forwardEvent([this]() {
        return data->processMouseLeave();
    }, [this]() {
        return action_tags_t::for_each([this](auto action_tag, auto) {
            using action_t = typename decltype(action_tag)::type;
            return action_t::startWithMouseLeave(playArea, data);
        });
    });
}