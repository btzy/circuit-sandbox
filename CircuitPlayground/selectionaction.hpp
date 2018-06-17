#pragma once

#include "point.hpp"
#include "canvasaction.hpp"
#include "playarea.hpp"
#include "mainwindow.hpp"


/**
 * Action that represents selection.
 */

class SelectionAction : public CanvasAction<SelectionAction> {

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

    SelectionAction(PlayArea& playArea, State state) :CanvasAction<SelectionAction>(playArea), state(state) {}


    // destructor, called to finish the action immediately
    ~SelectionAction() override {
        extensions::point deltaTrans = this->playArea.stateManager.commitSelection();
        this->playArea.translation -= deltaTrans * this->playArea.scale;
    }

    // TODO: refractor tool resolution out of SelectionAction and PencilAction, because its getting repetitive
    // TODO: refractor calculation of canvasOffset from event somewhere else as well

    // check if we need to start selection action from dragging/clicking the playarea=
    static inline bool startWithMouseButtonDown(const SDL_MouseButtonEvent& event, PlayArea& playArea, std::unique_ptr<BaseAction>& actionPtr) {
        size_t inputHandleIndex = resolveInputHandleIndex(event);
        size_t currentToolIndex = playArea.mainWindow.selectedToolIndices[inputHandleIndex];

        return tool_tags_t::get(currentToolIndex, [&event, &playArea, &actionPtr](const auto tool_tag) {
            // 'Tool' is the type of tool (e.g. Selector)
            using Tool = typename decltype(tool_tag)::type;

            if constexpr (std::is_base_of_v<Selector, Tool>) {
                // start selection action from dragging/clicking the playarea
                actionPtr = std::make_unique<SelectionAction>(playArea, State::SELECTING);
                auto& action = static_cast<SelectionAction&>(*actionPtr);
                extensions::point physicalOffset = extensions::point{ event.x, event.y } -extensions::point{ playArea.renderArea.x, playArea.renderArea.y };
                extensions::point canvasOffset = playArea.computeCanvasCoords(physicalOffset);
                action.selectionEnd = action.selectionOrigin = canvasOffset;
                return true;
            }

            return false;
        }, false);
    }

    // check if we need to start selection action from Ctrl-A or Ctrl-V
    static inline bool startWithKeyboard(const SDL_KeyboardEvent& event, PlayArea& playArea, std::unique_ptr<BaseAction>& actionPtr) {
        if (event.type == SDL_KEYDOWN) {
            SDL_Keymod modifiers = SDL_GetModState();
            switch (event.keysym.scancode) {
            case SDL_SCANCODE_A:
                if (modifiers & KMOD_CTRL) {
                    actionPtr = std::make_unique<SelectionAction>(playArea, State::SELECTED);
                    playArea.stateManager.selectAll();
                    return true;
                }
            case SDL_SCANCODE_V:
                if (modifiers & KMOD_CTRL) {
                    actionPtr = std::make_unique<SelectionAction>(playArea, State::SELECTED);

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
            default:
                return false;
            }
        }
        return false;
    }

    ActionEventResult processCanvasMouseDrag(const extensions::point& canvasOffset, const SDL_MouseMotionEvent&) {
        switch (state) {
        case State::SELECTING:
            // save the new ending location
            selectionEnd = canvasOffset;
            return ActionEventResult::PROCESSED;
        case State::SELECTED:
            // something's wrong -- if the mouse is being held down, we shouldn't be in SELECTED state
            // hopefully PlayArea can resolve this
            return ActionEventResult::UNPROCESSED;
        case State::MOVING:
            // move the selection if necessary
            if (moveOrigin != canvasOffset) {
                this->playArea.stateManager.moveSelection(canvasOffset.x - moveOrigin.x, canvasOffset.y - moveOrigin.y);
                moveOrigin = canvasOffset;
            }
            return ActionEventResult::PROCESSED;
        }
        return ActionEventResult::UNPROCESSED;
    }

    ActionEventResult processCanvasMouseButtonDown(const extensions::point& canvasOffset, const SDL_MouseButtonEvent& event) {
        size_t inputHandleIndex = resolveInputHandleIndex(event);
        size_t currentToolIndex = this->playArea.mainWindow.selectedToolIndices[inputHandleIndex];

        return tool_tags_t::get(currentToolIndex, [this, &canvasOffset](const auto tool_tag) {
            // 'Tool' is the type of tool (e.g. Selector)
            using Tool = typename decltype(tool_tag)::type;

            if constexpr (std::is_base_of_v<Selector, Tool>) {
                switch (state) {
                case State::SELECTED:
                    if (!this->playArea.stateManager.pointInSelection(canvasOffset)) {
                        // end selection
                        extensions::point deltaTrans = this->playArea.stateManager.commitSelection();
                        this->playArea.translation -= deltaTrans * this->playArea.scale;
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
                // wrong tool, return UNPROCESSED (maybe PlayArea will destroy this action)
                return ActionEventResult::UNPROCESSED;
            }
        }, ActionEventResult::UNPROCESSED);
    }

    ActionEventResult processCanvasMouseButtonUp(const extensions::point& canvasOffset, const SDL_MouseButtonEvent& event) {
        size_t inputHandleIndex = resolveInputHandleIndex(event);
        size_t currentToolIndex = this->playArea.mainWindow.selectedToolIndices[inputHandleIndex];

        return tool_tags_t::get(currentToolIndex, [this, &event, &canvasOffset](const auto tool_tag) {
            // 'Tool' is the type of tool (e.g. Selector)
            using Tool = typename decltype(tool_tag)::type;

            if constexpr (std::is_base_of_v<Selector, Tool>) {
                switch (state) {
                case State::SELECTING:
                    state = State::SELECTED;
                    selectionEnd = canvasOffset;
                    if (this->playArea.stateManager.selectRect(selectionOrigin, selectionEnd)) {
                        return ActionEventResult::PROCESSED;
                    } else {
                        return ActionEventResult::COMPLETED;
                    }
                case State::MOVING:
                    state = State::SELECTED;
                    return ActionEventResult::PROCESSED;
                default:
                    return ActionEventResult::UNPROCESSED;
                }
            }
            else {
                // wrong tool, return UNPROCESSED (maybe PlayArea will destroy this action)
                return ActionEventResult::UNPROCESSED;
            }
        }, ActionEventResult::UNPROCESSED);
    }

    inline ActionEventResult processKeyboard(const SDL_KeyboardEvent& event) override {
        if (event.type == SDL_KEYDOWN) {
            SDL_Keymod modifiers = SDL_GetModState();
            switch (event.keysym.scancode) {
            case SDL_SCANCODE_D:
            case SDL_SCANCODE_DELETE:
            {
                extensions::point deltaTrans = this->playArea.stateManager.deleteSelection();
                this->playArea.translation -= deltaTrans * this->playArea.scale;
                return ActionEventResult::COMPLETED;
            }
            case SDL_SCANCODE_C:
                if (modifiers & KMOD_CTRL) {
                    this->playArea.stateManager.copySelectionToClipboard();
                    return ActionEventResult::PROCESSED;
                }
            case SDL_SCANCODE_X:
                if (modifiers & KMOD_CTRL) {
                    extensions::point deltaTrans = this->playArea.stateManager.cutSelectionToClipboard();
                    this->playArea.translation -= deltaTrans * this->playArea.scale;
                    return ActionEventResult::COMPLETED;
                }
            default:
                return ActionEventResult::UNPROCESSED;
            }
        }
        return ActionEventResult::UNPROCESSED;
    }


    // rendering function, render the selection rectangle if it exists
    void render(SDL_Renderer* renderer) const override {
        if (state == State::SELECTING) {
            // normalize supplied points
            extensions::point topLeft = extensions::min(selectionOrigin, selectionEnd);
            extensions::point bottomRight = extensions::max(selectionOrigin, selectionEnd) + extensions::point{ 1, 1 };


            SDL_Rect selectionArea{
                topLeft.x * this->playArea.scale + this->playArea.translation.x,
                topLeft.y * this->playArea.scale + this->playArea.translation.y,
                (bottomRight.x - topLeft.x) * this->playArea.scale,
                (bottomRight.y - topLeft.y) * this->playArea.scale
            };

            SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0x44);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
            SDL_RenderFillRect(renderer, &selectionArea);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }
    }
};
