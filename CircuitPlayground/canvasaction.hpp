#pragma once

#include "playarea.hpp"

// a helper class that abstracts out converting mouse coordinates to canvas offset
template <typename T>
class CanvasAction : public BaseAction {
protected:
    PlayArea& playArea;

public:

    CanvasAction(PlayArea& playArea) :playArea(playArea) {};


    // save to history when this action ends
    ~CanvasAction() override {
        playArea.stateManager.saveToHistory();
    }



    // resolve the canvas offset then forward
    ActionEventResult processMouseMotionEvent(const SDL_MouseMotionEvent& event) override {
        extensions::point physicalOffset = extensions::point{ event.x, event.y } -extensions::point{ playArea.renderArea.x, playArea.renderArea.y };
        extensions::point canvasOffset = playArea.computeCanvasCoords(physicalOffset);
        return static_cast<T*>(this)->processCanvasMouseMotionEvent(canvasOffset, event);
    }

    // resolve the canvas offset then forward
    ActionEventResult processMouseButtonEvent(const SDL_MouseButtonEvent& event) override {
        extensions::point physicalOffset = extensions::point{ event.x, event.y } -extensions::point{ playArea.renderArea.x, playArea.renderArea.y };
        extensions::point canvasOffset = playArea.computeCanvasCoords(physicalOffset);
        return static_cast<T*>(this)->processCanvasMouseButtonEvent(canvasOffset, event);
    }
};