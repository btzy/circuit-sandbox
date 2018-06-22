#pragma once

#include "saveableaction.hpp"
#include "playarea.hpp"

// a helper class that abstracts out converting mouse coordinates to canvas offset
template <typename T>
class CanvasAction : public SaveableAction {
public:
    CanvasAction(PlayArea& playArea) :SaveableAction(playArea) {};

    // dummy stubs, so derived classes do not need to define these functions if they don't use them
    ActionEventResult processCanvasMouseButtonDown(const extensions::point&, const SDL_MouseButtonEvent&) {
        return ActionEventResult::UNPROCESSED;
    }
    ActionEventResult processCanvasMouseDrag(const extensions::point&, const SDL_MouseMotionEvent&) {
        return ActionEventResult::UNPROCESSED;
    }
    ActionEventResult processCanvasMouseButtonUp() {
        return ActionEventResult::UNPROCESSED;
    }

    // resolve the canvas offset then forward
    ActionEventResult processMouseButtonDown(const SDL_MouseButtonEvent& event) override {
        extensions::point physicalOffset = extensions::point{ event.x, event.y } -extensions::point{ playArea.renderArea.x, playArea.renderArea.y };
        extensions::point canvasOffset = playArea.computeCanvasCoords(physicalOffset);
        return static_cast<T*>(this)->processCanvasMouseButtonDown(canvasOffset, event);
    }

    // resolve the canvas offset then forward
    ActionEventResult processMouseDrag(const SDL_MouseMotionEvent& event) override {
        extensions::point physicalOffset = extensions::point{ event.x, event.y } -extensions::point{ playArea.renderArea.x, playArea.renderArea.y };
        extensions::point canvasOffset = playArea.computeCanvasCoords(physicalOffset);
        return static_cast<T*>(this)->processCanvasMouseDrag(canvasOffset, event);
    }

    // does nothing but forward for consistency
    ActionEventResult processMouseButtonUp() override {
        return static_cast<T*>(this)->processCanvasMouseButtonUp();
    }
};
