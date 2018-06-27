#include "actionmanager.hpp"


/*bool ActionManager::processMouseButtonDown(const SDL_MouseButtonEvent& event) {
    return forwardEvent(data, [&, this]() {
        return data->processMouseButtonDown(event);
    });
}

bool ActionManager::processMouseDrag(const SDL_MouseMotionEvent& event) {
    return forwardEvent(data, [&, this]() {
        return data->processMouseDrag(event);
    });
}

bool ActionManager::processMouseButtonUp() {
    return forwardEvent(data, [&, this]() {
        return data->processMouseButtonUp();
    });
}

// should we expect the mouse to be in the playarea?
// returns true if the event was consumed, false otherwise
bool ActionManager::processMouseWheel(const SDL_MouseWheelEvent& event) {
    return forwardEvent(data, [&, this]() {
        return data->processMouseWheel(event);
    });
}

bool ActionManager::processKeyboard(const SDL_KeyboardEvent& event) {
    return forwardEvent(data, [&, this]() {
        return data->processKeyboard(event);
    });
}*/

/*bool ActionManager::disableDefaultRender() const {
    return data->disableDefaultRender();
}*/
void ActionManager::render(SDL_Renderer* renderer) const {
    data->render(renderer);
}
