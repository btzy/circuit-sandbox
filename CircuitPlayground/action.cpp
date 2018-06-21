#include "action.hpp"
#include "selectionaction.hpp"
#include "historyaction.hpp"
#include "pencilaction.hpp"
#include "filenewaction.hpp"
#include "fileopenaction.hpp"
#include "filesaveaction.hpp"


bool Action::processMouseButtonDown(const SDL_MouseButtonEvent& event) {
    return forwardEvent([&, this]() {
        return data->processMouseButtonDown(event);
    }, [&, this]() {
        ActionEventResult res;
        action_tags_t::for_each([&, this](auto action_tag, auto) {
            using action_t = typename decltype(action_tag)::type;
            res = action_t::startWithMouseButtonDown(event, playArea, ActionStarter(data));
            return res == ActionEventResult::PROCESSED || res == ActionEventResult::COMPLETED;
        });
        return res;
    });
}

bool Action::processMouseDrag(const SDL_MouseMotionEvent& event) {
    return forwardEvent([&, this]() {
        return data->processMouseDrag(event);
    }, [&, this]() {
        ActionEventResult res;
        action_tags_t::for_each([&, this](auto action_tag, auto) {
            using action_t = typename decltype(action_tag)::type;
            res = action_t::startWithMouseDrag(event, playArea, ActionStarter(data));
            return res == ActionEventResult::PROCESSED || res == ActionEventResult::COMPLETED;
        });
        return res;
    });
}

bool Action::processMouseButtonUp() {
    return forwardEvent([&, this]() {
        return data->processMouseButtonUp();
    }, [&, this]() {
        ActionEventResult res;
        action_tags_t::for_each([&, this](auto action_tag, auto) {
            using action_t = typename decltype(action_tag)::type;
            res = action_t::startWithMouseButtonUp(playArea, ActionStarter(data));
            return res == ActionEventResult::PROCESSED || res == ActionEventResult::COMPLETED;
        });
        return res;
    });
}

// should we expect the mouse to be in the playarea?
// returns true if the event was consumed, false otherwise
bool Action::processMouseWheel(const SDL_MouseWheelEvent& event) {
    return forwardEvent([&, this]() {
        return data->processMouseWheel(event);
    }, [&, this]() {
        ActionEventResult res;
        action_tags_t::for_each([&, this](auto action_tag, auto) {
            using action_t = typename decltype(action_tag)::type;
            res = action_t::startWithMouseWheel(event, playArea, ActionStarter(data));
            return res == ActionEventResult::PROCESSED || res == ActionEventResult::COMPLETED;
        });
        return res;
    });
}

// returns true if the event was consumed, false otherwise
bool Action::processKeyboard(const SDL_KeyboardEvent& event) {
    return forwardEvent([&, this]() {
        return data->processKeyboard(event);
    }, [&, this]() {
        ActionEventResult res;
        action_tags_t::for_each([&, this](auto action_tag, auto) {
            using action_t = typename decltype(action_tag)::type;
            res = action_t::startWithKeyboard(event, playArea, ActionStarter(data));
            return res == ActionEventResult::PROCESSED || res == ActionEventResult::COMPLETED;
        });
        return res;
    });
}

bool Action::disableDefaultRender() const {
    return data->disableDefaultRender();
}
void Action::renderSurface(uint32_t* pixelBuffer, const SDL_Rect& renderRect) const {
    data->renderSurface(pixelBuffer, renderRect);
}
void Action::renderDirect(SDL_Renderer* renderer) const {
    data->renderDirect(renderer);
}
