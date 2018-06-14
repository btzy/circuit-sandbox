#pragma once

#include "baseaction.hpp"

/**
 * Action that represents selection.
 */

template <typename PlayArea>
class SelectionAction : public CanvasAction<SelectionAction<PlayArea>, PlayArea> {

private:
    enum class State {
        SELECTING,
        SELECTED,
        MOVING
    };

    State state;

    extensions::point selectionOrigin; // in canvas coordinates
    extensions::point selectionEnd; // in canvas coordinates
    extensions::point moveOrigin; // in canvas coordinates


public:

    SelectionAction(PlayArea& playArea, State state) :CanvasAction<SelectionAction<PlayArea>, PlayArea>(playArea), state(state) {}


    // destructor, called to finish the action immediately
    ~SelectionAction() {
        extensions::point deltaTrans = playArea.stateManager.commitSelection();
        playArea.translation -= deltaTrans * playArea.scale;
    }

    // TODO: refractor tool resolution out of SelectionAction and PencilAction, because its getting repetitive
    // TODO: refractor calculation of canvasOffset from event somewhere else as well

    // check if we need to start selection action from dragging/clicking the playarea
    template <typename ActionVariant>
    static inline bool startWithMouseButtonEvent(const SDL_MouseButtonEvent& event, PlayArea& playArea, ActionVariant& output) {
        size_t inputHandleIndex = MainWindow::resolveInputHandleIndex(event);
        size_t currentToolIndex = playArea.mainWindow.selectedToolIndices[inputHandleIndex];

        return MainWindow::tool_tags_t::get(currentToolIndex, [&event, &playArea, &output](const auto tool_tag) {
            // 'Tool' is the type of tool (e.g. Selector)
            using Tool = typename decltype(tool_tag)::type;

            if constexpr (std::is_base_of_v<Selector, Tool>) {
                if (event.type == SDL_MOUSEBUTTONDOWN) {
                    // start selection action from dragging/clicking the playarea
                    auto& action = output.emplace<SelectionAction<PlayArea>>(playArea, State::SELECTING);
                    extensions::point physicalOffset = extensions::point{ event.x, event.y } -extensions::point{ playArea.renderArea.x, playArea.renderArea.y };
                    extensions::point canvasOffset = playArea.computeCanvasCoords(physicalOffset);
                    action.selectionEnd = action.selectionOrigin = canvasOffset;
                    return true;
                }
            }

            return false;
        });
    }

    // check if we need to start selection action from Ctrl-A or Ctrl-V
    template <typename ActionVariant>
    static inline bool startWithKeyboardEvent(const SDL_KeyboardEvent& event, PlayArea& playArea, ActionVariant& output) {
        if (event.type == SDL_KEYDOWN) {
            SDL_Keymod modifiers = SDL_GetModState();
            switch (event.keysym.scancode) {
            case SDL_SCANCODE_A:
                if (modifiers & KMOD_CTRL) {
                    auto& action = output.emplace<SelectionAction<PlayArea>>(playArea, State::SELECTED);
                    playArea.stateManager.selectAll();
                    return true;
                }
            case SDL_SCANCODE_V:
                if (modifiers & KMOD_CTRL) {
                    auto& action = output.emplace<SelectionAction<PlayArea>>(playArea, State::SELECTED);

                    extensions::point physicalOffset;
                    SDL_GetMouseState(&physicalOffset.x, &physicalOffset.y);
                    extensions::point canvasOffset = physicalOffset - playArea.translation;
                    canvasOffset = extensions::div_floor(canvasOffset, playArea.scale);

                    SDL_Point position{ physicalOffset.x, physicalOffset.y };
                    if (!SDL_PointInRect(&position, &playArea.renderArea)) {
                        canvasOffset = { 0, 0 }; // TODO: shouldn't we paste at (0,0) in playarea window coordinates, rather than canvas coordinates?
                    }
                    playArea.stateManager.pasteSelectionFromClipboard(canvasOffset.x, canvasOffset.y);

                    return true;
                }
            }
        }
        return false;
    }

    ActionEventResult processCanvasMouseMotionEvent(const extensions::point& canvasOffset, const SDL_MouseMotionEvent& event) {
        switch (state) {
        case State::SELECTING:
            // save the new ending location
            selectionEnd = canvasOffset;
            return ActionEventResult::PROCESSED;
        case State::SELECTED:
            // do nothing, but claim that we have already used the event so no one else gets to process it
            return ActionEventResult::PROCESSED;
        case State::MOVING:
            // move the selection if necessary
            if (moveOrigin != canvasOffset) {
                playArea.stateManager.moveSelection(canvasOffset.x - moveOrigin.x, canvasOffset.y - moveOrigin.y);
                moveOrigin = canvasOffset;
            }
            return ActionEventResult::PROCESSED;
        }
        return ActionEventResult::UNPROCESSED;
    }

    ActionEventResult processCanvasMouseButtonEvent(const extensions::point& canvasOffset, const SDL_MouseButtonEvent& event) {
        size_t inputHandleIndex = MainWindow::resolveInputHandleIndex(event);
        size_t currentToolIndex = playArea.mainWindow.selectedToolIndices[inputHandleIndex];

        return MainWindow::tool_tags_t::get(currentToolIndex, [this, &event, &canvasOffset](const auto tool_tag) {
            // 'Tool' is the type of tool (e.g. Selector)
            using Tool = typename decltype(tool_tag)::type;

            if constexpr (std::is_base_of_v<Selector, Tool>) {
                if (event.type == SDL_MOUSEBUTTONDOWN) {
                    switch (state) {
                    case State::SELECTED:
                        if (!playArea.stateManager.pointInSelection(canvasOffset)) {
                            // end selection
                            extensions::point deltaTrans = playArea.stateManager.commitSelection();
                            playArea.translation -= deltaTrans * playArea.scale;
                            return ActionEventResult::CANCELLED; // this is on purpose, so that we can enter a new selection action with the same event.
                        }
                        else {
                            // enter moving state
                            state = State::MOVING;
                            moveOrigin = canvasOffset;
                            return ActionEventResult::PROCESSED;
                        }
                    default:
                        // this is not possible! some bug happened?
                        // let playarea decide what to do (possibly destroy this action and start a new one)
                        return ActionEventResult::UNPROCESSED;
                    }
                }
                else {
                    switch (state) {
                    case State::SELECTING:
                        state = State::SELECTED;
                        {
                            SDL_Point position{ event.x, event.y };
                            // TODO: consider using viewports to eliminate these checks
                            if (SDL_PointInRect(&position, &playArea.renderArea)) {
                                selectionEnd = canvasOffset;
                                playArea.stateManager.selectRect(selectionOrigin, selectionEnd);
                            }
                        }
                        return ActionEventResult::PROCESSED;
                    case State::MOVING:
                        state = State::SELECTED;
                        return ActionEventResult::PROCESSED;
                    default:
                        return ActionEventResult::UNPROCESSED;
                    }
                }
            }
            else {
                // wrong tool, return UNPROCESSED (maybe PlayArea will destroy this action)
                return ActionEventResult::UNPROCESSED;
            }
        });
    }

    inline ActionEventResult processKeyboardEvent(const SDL_KeyboardEvent& event) {
        if (event.type == SDL_KEYDOWN) {
            SDL_Keymod modifiers = SDL_GetModState();
            switch (event.keysym.scancode) {
            case SDL_SCANCODE_D:
            case SDL_SCANCODE_DELETE:
            {
                extensions::point deltaTrans = playArea.stateManager.deleteSelection();
                playArea.translation -= deltaTrans * playArea.scale;
                return ActionEventResult::COMPLETED;
            }
            case SDL_SCANCODE_C:
                if (modifiers & KMOD_CTRL) {
                    playArea.stateManager.copySelectionToClipboard();
                    return ActionEventResult::PROCESSED;
                }
            case SDL_SCANCODE_X:
                if (modifiers & KMOD_CTRL) {
                    extensions::point deltaTrans = playArea.stateManager.cutSelectionToClipboard();
                    playArea.translation -= deltaTrans * playArea.scale;
                    return ActionEventResult::COMPLETED;
                }
            }
        }
        return ActionEventResult::UNPROCESSED;
    }


    // rendering function, render the selection rectangle if it exists
    void render(SDL_Renderer* renderer) const {
        if (state == State::SELECTING) {
            // normalize supplied points
            extensions::point topLeft = extensions::min(selectionOrigin, selectionEnd);
            extensions::point bottomRight = extensions::max(selectionOrigin, selectionEnd) + extensions::point{ 1, 1 };


            SDL_Rect selectionArea{
                topLeft.x * playArea.scale + playArea.translation.x,
                topLeft.y * playArea.scale + playArea.translation.y,
                (bottomRight.x - topLeft.x) * playArea.scale,
                (bottomRight.y - topLeft.y) * playArea.scale
            };

            SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0x44);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
            SDL_RenderFillRect(renderer, &selectionArea);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }
    }
};
