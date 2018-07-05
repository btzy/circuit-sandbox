#include "playareaactionmanager.hpp"
#include "eyedropperaction.hpp"
#include "selectionaction.hpp"
#include "historyaction.hpp"
#include "pencilaction.hpp"


bool PlayAreaActionManager::processPlayAreaMouseButtonDown(const SDL_MouseButtonEvent& event) {
    return forwardEvent(data, [&, this]() {
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
    return forwardEvent(data, [&, this]() {
        return data->processPlayAreaMouseDrag(event);
    });
}

bool PlayAreaActionManager::processPlayAreaMouseButtonUp() {
    return forwardEvent(data, [&, this]() {
        return data->processPlayAreaMouseButtonUp();
    });
}

// should we expect the mouse to be in the playarea?
// returns true if the event was consumed, false otherwise
bool PlayAreaActionManager::processPlayAreaMouseWheel(const SDL_MouseWheelEvent& event) {
    return forwardEvent(data, [&, this]() {
        return data->processPlayAreaMouseWheel(event);
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
