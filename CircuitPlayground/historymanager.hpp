#pragma once

#include <stack>
#include <optional>
#include <utility>
#include "canvasstate.hpp"
#include "point.hpp"

struct HistoryManager {
private:
    // fields for undo/redo stack
    std::stack<std::pair<CanvasState, ext::point>> undoStack; // the undo stack stores entire CanvasStates (with accompanying deltaTrans) for now
    std::stack<std::pair<CanvasState, ext::point>> redoStack; // the redo stack stores entire CanvasStates (with accompanying deltaTrans) for now
    CanvasState currentHistoryState; // the canvas state treated as 'current' by the history manager; this is either the state when saveToHistory() was last called, or the state after an undo/redo operation.

    // distance of currentHistoryState from the last saved state in history
    // 0 if currentHistoryState is the same as the last saved state
    // std::nullopt if it's not possible to reach the last saved state using undo/redo
    std::optional<size_t> saveDistance = 0;

public:
    void saveToHistory(const CanvasState& state, const ext::point& deltaTrans) {
        // note: we save the inverse translation
        undoStack.emplace(std::move(currentHistoryState), -deltaTrans);
        // save a snapshot of the defaultState (which should be in sync with the simulator)
        currentHistoryState = state;

        if (saveDistance) {
            if (*saveDistance < 0) {
                // it's impossible to reach the original state
                saveDistance = std::nullopt;
            }
            else {
                ++*saveDistance;
            }
        }

        // flush the redoStack
        while (!redoStack.empty()) redoStack.pop();
    }

    /**
     * Undo/redo. Returns the translation to apply to center viewport.
     * Assumes that relevant undoStack/redoStack is not empty.
     */
    ext::point undo(CanvasState& state) {
        auto[canvasState, tmpDeltaTrans] = std::move(undoStack.top());
        undoStack.pop();

        // note: we save the inverse of the undo translation into the redo stack
        redoStack.emplace(std::move(currentHistoryState), -tmpDeltaTrans);
        state = currentHistoryState = std::move(canvasState);

        if (saveDistance) --*saveDistance;

        return tmpDeltaTrans;
    }

    ext::point redo(CanvasState& state) {
        auto[canvasState, tmpDeltaTrans] = std::move(redoStack.top());
        redoStack.pop();

        // note: we save the inverse of the redo translation into the undo stack
        undoStack.emplace(std::move(currentHistoryState), -tmpDeltaTrans);
        state = currentHistoryState = std::move(canvasState);

        if (saveDistance) ++*saveDistance;

        return tmpDeltaTrans;
    }

    bool canUndo() const {
        return !undoStack.empty();
    }

    bool canRedo() const {
        return !redoStack.empty();
    }

    /**
     * adjust the current state without adding to the undo stack
     */
    void imbue(const CanvasState& state) {
        currentHistoryState = state;
    }

    /**
     * returns true if saveToHistory() has never been called.
     */
    bool empty() const {
        return redoStack.empty() && undoStack.empty();
    }

    const CanvasState& currentState() const {
        return currentHistoryState;
    }

    /**
     * returns whether currentHistoryState has changed since the game was last saved
     */
    bool changedSinceLastSave() const {
        return saveDistance != 0;
    }

    /**
     * inform the history manager that the gamestate was saved to disk
     */
    void setSaved() {
        saveDistance = 0;
    }
};
