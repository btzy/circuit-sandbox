#pragma once

#include <stack>
#include "canvasstate.hpp"
#include "point.hpp"

struct HistoryManager {
private:
    // fields for undo/redo stack
    std::stack<std::pair<CanvasState, extensions::point>> undoStack; // the undo stack stores entire CanvasStates (with accompanying deltaTrans) for now
    std::stack<std::pair<CanvasState, extensions::point>> redoStack; // the redo stack stores entire CanvasStates (with accompanying deltaTrans) for now
    CanvasState currentHistoryState; // the canvas state treated as 'current' by the history manager; this is either the state when saveToHistory() was last called, or the state after an undo/redo operation.

public:
    void saveToHistory(const CanvasState& state, const extensions::point& deltaTrans) {
        // note: we save the inverse translation
        undoStack.emplace(std::move(currentHistoryState), -deltaTrans);
        // save a snapshot of the defaultState (which should be in sync with the simulator)
        currentHistoryState = state;

        // flush the redoStack
        while (!redoStack.empty()) redoStack.pop();
    }

    /**
     * Undo/redo. Returns the translation to apply to center viewport.
     * Assumes that relevant undoStack/redoStack is not empty.
     */
    extensions::point undo(CanvasState& state) {
        auto[canvasState, tmpDeltaTrans] = std::move(undoStack.top());
        undoStack.pop();

        // note: we save the inverse of the undo translation into the redo stack
        redoStack.emplace(std::move(currentHistoryState), -tmpDeltaTrans);
        state = currentHistoryState = std::move(canvasState);

        return tmpDeltaTrans;
    }

    extensions::point redo(CanvasState& state) {
        auto[canvasState, tmpDeltaTrans] = std::move(redoStack.top());
        redoStack.pop();

        // note: we save the inverse of the redo translation into the undo stack
        undoStack.emplace(std::move(currentHistoryState), -tmpDeltaTrans);
        state = currentHistoryState = std::move(canvasState);

        return tmpDeltaTrans;
    }

    bool canUndo() const {
        return !undoStack.empty();
    }

    bool canRedo() const {
        return !redoStack.empty();
    }

    const CanvasState& currentState() const {
        return currentHistoryState;
    }
};
