#include "playareaactionmanager.hpp"
#include "selectionaction.hpp"
#include "historyaction.hpp"
#include "pencilaction.hpp"
#include "screeninputaction.hpp"
#include "filecommunicatorselectaction.hpp"


bool PlayAreaActionManager::processPlayAreaMouseButtonDown(const SDL_MouseButtonEvent& event) {
    return mouseDownResult = forwardEvent(data, [&, this]() {
        return data->processPlayAreaMouseButtonDown(event);
    }, [&, this]() {
        ActionEventResult res;
        playarea_action_tags_t::for_each([&, this](auto action_tag, auto) {
            using action_t = typename decltype(action_tag)::type;
            res = action_t::startWithPlayAreaMouseButtonDown(event, mainWindow, playArea, getStarter());
            return res == ActionEventResult::PROCESSED || res == ActionEventResult::COMPLETED;
        });
        return res;
    });
}

bool PlayAreaActionManager::processPlayAreaMouseDrag(const SDL_MouseMotionEvent& event) {
    if (mouseDownResult) {
        return forwardEvent(data, [&, this]() {
            return data->processPlayAreaMouseDrag(event);
        });
    }
    else return false;
}

bool PlayAreaActionManager::processPlayAreaMouseButtonUp() {
    if (mouseDownResult) {
        return forwardEvent(data, [&, this]() {
            return data->processPlayAreaMouseButtonUp();
        });
    }
    else return false;
}

// should we expect the mouse to be in the playarea?
// returns true if the event was consumed, false otherwise
bool PlayAreaActionManager::processPlayAreaMouseWheel(const SDL_MouseWheelEvent& event) {
    return forwardEvent(data, [&, this]() {
        return data->processPlayAreaMouseWheel(event);
    });
}

bool PlayAreaActionManager::processPlayAreaMouseHover(const SDL_MouseMotionEvent& event) {
    return forwardEvent(data, [&, this]() {
        return data->processPlayAreaMouseHover(event);
    });
}

bool PlayAreaActionManager::processPlayAreaMouseLeave() {
    return forwardEvent(data, [&, this]() {
        return data->processPlayAreaMouseLeave();
    });
}

bool PlayAreaActionManager::disablePlayAreaDefaultRender() const {
    return data ? data->disablePlayAreaDefaultRender() : false;
}
void PlayAreaActionManager::renderPlayAreaSurface(uint32_t* pixelBuffer, uint32_t pixelFormat, const SDL_Rect& renderRect, int32_t pitch) const {
    if (data) data->renderPlayAreaSurface(pixelBuffer, pixelFormat, renderRect, pitch);
}
void PlayAreaActionManager::renderPlayAreaDirect(SDL_Renderer* renderer) const {
    if (data) data->renderPlayAreaDirect(renderer);
}
