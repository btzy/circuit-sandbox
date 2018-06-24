#pragma once

#include <SDL.h>
#include "point.hpp"
#include "visitor.hpp"
#include "drawing.hpp"
#include "canvasaction.hpp"
#include "playarea.hpp"
#include "mainwindow.hpp"


/**
 * Action that represents selection.
 */

class SelectionAction final : public CanvasAction<SelectionAction> {

private:
    enum class State {
        SELECTING,
        SELECTED,
        MOVING
    };

    State state;

    // in State::SELECTING, selectionOrigin is the point where the mouse went down, and selectionEnd is the current point of the mouse
    extensions::point selectionOrigin; // in canvas coordinates
    extensions::point selectionEnd; // in canvas coordinates

    // when entering State::SELECTED these will be set; after that these will not be modified
    CanvasState selection; // stores the selection

    // in State::SELECTED and State::MOVING, these are the translations of selection and base, relative to defaultState
    extensions::point selectionTrans; // in canvas coordinates
    extensions::point baseTrans; // in canvas coordinates

    // in State::MOVING, this was the previous point of the mouse
    extensions::point moveOrigin; // in canvas coordinates

    inline static CanvasState clipboard; // the single clipboard; will be changed to support multi-clipboard

    /**
     * Prepare base, selection, baseTrans, selectionTrans from selectionOrigin and selectionEnd. Returns true if the selection is not empty.
     */
    bool selectRect() {
        CanvasState& base = canvas();

        // normalize supplied points
        extensions::point topLeft = extensions::min(selectionOrigin, selectionEnd);
        extensions::point bottomRight = extensions::max(selectionOrigin, selectionEnd) + extensions::point{ 1, 1 };

        // restrict selectionRect to the area within base
        topLeft = extensions::max(topLeft, { 0, 0 });
        bottomRight = extensions::min(bottomRight, base.size());
        extensions::point selectionSize = bottomRight - topLeft;

        // no elements in the selection rect
        if (selectionSize.x <= 0 || selectionSize.y <= 0) return false;

        // splice out the selection from the base
        selection = base.splice(topLeft.x, topLeft.y, selectionSize.x, selectionSize.y);

        // shrink the selection
        extensions::point selectionShrinkTrans = selection.shrinkDataMatrix();

        // no elements in the selection rect
        if (selection.empty()) return false;

        selectionTrans = topLeft - selectionShrinkTrans;

        baseTrans = -base.shrinkDataMatrix();

        return true;
    }

public:
    SelectionAction(PlayArea& playArea, State state) :CanvasAction<SelectionAction>(playArea), state(state) {}

    // destructor, called to finish the action immediately
    ~SelectionAction() override {
        if (state != State::SELECTING) {
            auto[tmpDefaultState, translation] = CanvasState::merge(std::move(canvas()), baseTrans, std::move(selection), selectionTrans);
            canvas() = std::move(tmpDefaultState);
            deltaTrans = std::move(translation);
        }
    }

    // TODO: refractor tool resolution out of SelectionAction and PencilAction, because its getting repetitive
    // TODO: refractor calculation of canvasOffset from event somewhere else as well

    // check if we need to start selection action from dragging/clicking the playarea
    static inline ActionEventResult startWithMouseButtonDown(const SDL_MouseButtonEvent& event, PlayArea& playArea, const ActionStarter& starter) {
        size_t inputHandleIndex = resolveInputHandleIndex(event);
        size_t currentToolIndex = playArea.mainWindow.selectedToolIndices[inputHandleIndex];

        return tool_tags_t::get(currentToolIndex, [&event, &playArea, &starter](const auto tool_tag) {
            // 'Tool' is the type of tool (e.g. Selector)
            using Tool = typename decltype(tool_tag)::type;

            if constexpr (std::is_base_of_v<Selector, Tool>) {
                // start selection action from dragging/clicking the playarea
                auto& action = starter.start<SelectionAction>(playArea, State::SELECTING);
                extensions::point physicalOffset = extensions::point{ event.x, event.y } -extensions::point{ playArea.renderArea.x, playArea.renderArea.y };
                extensions::point canvasOffset = playArea.computeCanvasCoords(physicalOffset);
                action.selectionEnd = action.selectionOrigin = canvasOffset;
                return ActionEventResult::PROCESSED;
            }
            else {
                return ActionEventResult::UNPROCESSED;
            }
        }, ActionEventResult::UNPROCESSED);
    }

    // check if we need to start selection action from Ctrl-A or Ctrl-V
    static inline ActionEventResult startWithKeyboard(const SDL_KeyboardEvent& event, PlayArea& playArea, const ActionStarter& starter) {
        if (event.type == SDL_KEYDOWN) {
            SDL_Keymod modifiers = SDL_GetModState();
            switch (event.keysym.scancode) {
            case SDL_SCANCODE_A:
                if (modifiers & KMOD_CTRL) {
                    auto& action = starter.start<SelectionAction>(playArea, State::SELECTED);
                    action.selection = std::move(action.canvas());
                    action.selectionTrans = action.baseTrans = { 0, 0 };
                    action.canvas() = CanvasState(); // clear defaultState
                    return ActionEventResult::PROCESSED;
                }
            case SDL_SCANCODE_V:
                if (modifiers & KMOD_CTRL) {
                    auto& action = starter.start<SelectionAction>(playArea, State::SELECTED);

                    extensions::point physicalOffset;
                    SDL_GetMouseState(&physicalOffset.x, &physicalOffset.y);
                    extensions::point canvasOffset = physicalOffset - playArea.translation;
                    canvasOffset = extensions::div_floor(canvasOffset, playArea.scale);

                    if (!point_in_rect(physicalOffset, playArea.renderArea)) {
                        // since the mouse is not in the play area, we paste in the middle of the play area
                        canvasOffset = extensions::div_floor(extensions::point{ playArea.renderArea.w/2, playArea.renderArea.h/2 } - playArea.translation, playArea.scale);
                    }
                    action.selection = clipboard; // have to make a copy, so that we don't mess up the clipboard
                    action.selectionTrans = canvasOffset - action.selection.size() / 2; // set the selection offset as the current offset
                    action.baseTrans = { 0, 0 };
                    return ActionEventResult::PROCESSED;
                }
            default:
                return ActionEventResult::UNPROCESSED;
            }
        }
        return ActionEventResult::UNPROCESSED;
    }

    ActionEventResult processCanvasMouseDrag(const extensions::point&, const SDL_MouseMotionEvent& event) {
        // compute the canvasOffset here and restrict it to the visible area
        extensions::point canvasOffset;
        canvasOffset.x = std::clamp(event.x, playArea.renderArea.x, playArea.renderArea.x + playArea.renderArea.w);
        canvasOffset.y = std::clamp(event.y, playArea.renderArea.y, playArea.renderArea.y + playArea.renderArea.h);
        canvasOffset = playArea.computeCanvasCoords(canvasOffset);

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
                selectionTrans += canvasOffset - moveOrigin;
                moveOrigin = canvasOffset;
            }
            return ActionEventResult::PROCESSED;
        }
        return ActionEventResult::UNPROCESSED;
    }

    ActionEventResult processCanvasMouseButtonDown(const extensions::point& canvasOffset, const SDL_MouseButtonEvent& event) {
        size_t inputHandleIndex = resolveInputHandleIndex(event);
        size_t currentToolIndex = playArea.mainWindow.selectedToolIndices[inputHandleIndex];

        return tool_tags_t::get(currentToolIndex, [this, &canvasOffset](const auto tool_tag) {
            // 'Tool' is the type of tool (e.g. Selector)
            using Tool = typename decltype(tool_tag)::type;

            if constexpr (std::is_base_of_v<Selector, Tool>) {
                switch (state) {
                case State::SELECTED:
                    if (!selection.contains(canvasOffset - selectionTrans)) {
                        // end selection
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

    ActionEventResult processCanvasMouseButtonUp() {
        switch (state) {
            case State::SELECTING:
                state = State::SELECTED;
                if (selectRect()) {
                    return ActionEventResult::PROCESSED;
                } else {
                    selectionTrans = baseTrans = { 0, 0 }; // set these values, because selectRect won't set if it returns false
                    return ActionEventResult::COMPLETED;
                }
            case State::MOVING:
                state = State::SELECTED;
                return ActionEventResult::PROCESSED;
            default:
                return ActionEventResult::UNPROCESSED;
        }
    }

    inline ActionEventResult processKeyboard(const SDL_KeyboardEvent& event) override {
        if (event.type == SDL_KEYDOWN) {
            SDL_Keymod modifiers = SDL_GetModState();
            switch (event.keysym.scancode) {
            case SDL_SCANCODE_H:
                selection.flipHorizontal();
                return ActionEventResult::PROCESSED;
            case SDL_SCANCODE_V:
                if (!(modifiers & KMOD_CTRL)) {
                    selection.flipVertical();
                    return ActionEventResult::PROCESSED;
                }
                return ActionEventResult::UNPROCESSED;
            case SDL_SCANCODE_LEFTBRACKET: // [
                selection.rotateCounterClockwise();
                return ActionEventResult::PROCESSED;
            case SDL_SCANCODE_RIGHTBRACKET: // ]
                selection.rotateClockwise();
                return ActionEventResult::PROCESSED;
            case SDL_SCANCODE_D:
                [[fallthrough]];
            case SDL_SCANCODE_DELETE:
            {
                selection = CanvasState(); // clear the selection
                return ActionEventResult::COMPLETED; // tell playarea to end this action
            }
            case SDL_SCANCODE_C:
                if (modifiers & KMOD_CTRL) {
                    clipboard = selection;
                    return ActionEventResult::PROCESSED;
                }
            case SDL_SCANCODE_X:
                if (modifiers & KMOD_CTRL) {
                    clipboard = std::move(selection);
                    selection = CanvasState(); // clear the selection
                    return ActionEventResult::COMPLETED; // tell playarea to end this action
                }
            default:
                return ActionEventResult::UNPROCESSED;
            }
        }
        return ActionEventResult::UNPROCESSED;
    }


    // disable default rendering when not SELECTING
    bool disableDefaultRender() const override {
        return state != State::SELECTING;
    }

    // rendering function, render the selection (since base will already be rendered)
    void renderSurface(uint32_t* pixelBuffer, const SDL_Rect& renderRect) const override {
        if (state != State::SELECTING) {
            bool defaultView = playArea.isDefaultView();

            for (int32_t y = renderRect.y; y != renderRect.y + renderRect.h; ++y) {
                for (int32_t x = renderRect.x; x != renderRect.x + renderRect.w; ++x) {
                    SDL_Color computedColor{ 0, 0, 0, 0 };
                    const extensions::point canvasPt{ x, y };

                    // draw base, check if the requested pixel inside the buffer
                    if (canvas().contains(canvasPt - baseTrans)) {
                        std::visit(visitor{
                            [](std::monostate) {},
                            [&](const auto& element) {
                                computedColor = computeDisplayColor(element, defaultView);
                            },
                            }, canvas()[canvasPt - baseTrans]);
                    }

                    // draw selection, check if the requested pixel inside the buffer
                    if (selection.contains(canvasPt - selectionTrans)) {
                        std::visit(visitor{
                            [](std::monostate) {},
                            [&](const auto& element) {
                                computedColor = computeDisplayColor(element, defaultView);
                                computedColor.b = 0xFF; // colour the selection blue
                            },
                            }, selection[canvasPt - selectionTrans]);
                    }

                    uint32_t color = computedColor.r | (computedColor.g << 8) | (computedColor.b << 16);
                    *pixelBuffer++ = color;
                }
            }
        }
    }

    // rendering function, render the selection rectangle if it exists
    void renderDirect(SDL_Renderer* renderer) const override {
        switch (state) {
        case State::SELECTING:
            {
                // normalize supplied points
                extensions::point topLeft = extensions::min(selectionOrigin, selectionEnd);
                extensions::point bottomRight = extensions::max(selectionOrigin, selectionEnd) + extensions::point{ 1, 1 };

                SDL_Rect selectionArea{
                    topLeft.x * playArea.scale + playArea.translation.x,
                    topLeft.y * playArea.scale + playArea.translation.y,
                    (bottomRight.x - topLeft.x) * playArea.scale,
                    (bottomRight.y - topLeft.y) * playArea.scale
                };

                SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
                extensions::renderDrawDashedRect(renderer, &selectionArea);
            }
            break;
        case State::SELECTED:
            [[fallthrough]]
        case State::MOVING:
            {
                SDL_Rect selectionArea{
                    selectionTrans.x * playArea.scale + playArea.translation.x,
                    selectionTrans.y * playArea.scale + playArea.translation.y,
                    selection.width() * playArea.scale,
                    selection.height() * playArea.scale
                };

                SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
                extensions::renderDrawDashedRect(renderer, &selectionArea);
            }
            break;
        }
    }
};
