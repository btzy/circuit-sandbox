#pragma once

#include <SDL.h>
#include "point.hpp"
#include "visitor.hpp"
#include "drawing.hpp"
#include "saveableaction.hpp"
#include "playarea.hpp"
#include "mainwindow.hpp"
#include "eventhook.hpp"


/**
 * Action that represents selection.
 */

class SelectionAction final : public SaveableAction, public KeyboardEventHook {

private:
    enum class State {
        SELECTING,
        SELECTED,
        MOVING
    };

    State state;

    // in State::SELECTING, selectionOrigin is the point where the mouse went down, and selectionEnd is the current point of the mouse
    ext::point selectionOrigin; // in canvas coordinates
    ext::point selectionEnd; // in canvas coordinates

    // when entering State::SELECTED these will be set; after that these will not be modified
    CanvasState selection; // stores the selection

    // in State::SELECTED and State::MOVING, these are the translations of selection and base, relative to defaultState
    ext::point selectionTrans; // in canvas coordinates
    ext::point baseTrans; // in canvas coordinates

    // in State::MOVING, this was the previous point of the mouse
    ext::point moveOrigin; // in canvas coordinates

    static CanvasState clipboard; // the single clipboard; will be changed to support multi-clipboard

    /**
     * Prepare base, selection, baseTrans, selectionTrans from selectionOrigin and selectionEnd. Returns true if the selection is not empty.
     */
    bool selectRect() {
        CanvasState& base = canvas();

        // normalize supplied points
        ext::point topLeft = ext::min(selectionOrigin, selectionEnd);
        ext::point bottomRight = ext::max(selectionOrigin, selectionEnd) + ext::point{ 1, 1 };

        // restrict selectionRect to the area within base
        topLeft = ext::max(topLeft, { 0, 0 });
        bottomRight = ext::min(bottomRight, base.size());
        ext::point selectionSize = bottomRight - topLeft;

        // no elements in the selection rect
        if (selectionSize.x <= 0 || selectionSize.y <= 0) return false;

        // splice out the selection from the base
        selection = base.splice(topLeft.x, topLeft.y, selectionSize.x, selectionSize.y);

        // shrink the selection
        ext::point selectionShrinkTrans = selection.shrinkDataMatrix();

        // no elements in the selection rect
        if (selection.empty()) return false;

        selectionTrans = topLeft - selectionShrinkTrans;

        baseTrans = -base.shrinkDataMatrix();

        return true;
    }

public:
    SelectionAction(MainWindow& mainWindow, State state) :SaveableAction(mainWindow), KeyboardEventHook(mainWindow), state(state) {}

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
    static inline ActionEventResult startWithPlayAreaMouseButtonDown(const SDL_MouseButtonEvent& event, MainWindow& mainWindow, PlayArea& playArea, const ActionStarter& starter) {
        size_t inputHandleIndex = resolveInputHandleIndex(event);
        size_t currentToolIndex = mainWindow.selectedToolIndices[inputHandleIndex];

        return tool_tags_t::get(currentToolIndex, [&](const auto tool_tag) {
            // 'Tool' is the type of tool (e.g. Selector)
            using Tool = typename decltype(tool_tag)::type;

            if constexpr (std::is_base_of_v<Selector, Tool>) {
                // start selection action from dragging/clicking the playarea
                auto& action = starter.start<SelectionAction>(mainWindow, State::SELECTING);
                ext::point canvasOffset = playArea.canvasFromWindowOffset(event);
                action.selectionEnd = action.selectionOrigin = canvasOffset;
                return ActionEventResult::PROCESSED;
            }
            else {
                return ActionEventResult::UNPROCESSED;
            }
        }, ActionEventResult::UNPROCESSED);
    }

    static inline void startBySelectingAll(MainWindow& mainWindow, const ActionStarter& starter) {
        if (mainWindow.stateManager.defaultState.empty()) return;

        auto& action = starter.start<SelectionAction>(mainWindow, State::SELECTED);
        action.selection = std::move(action.canvas());
        action.selectionTrans = action.baseTrans = { 0, 0 };
        action.canvas() = CanvasState(); // clear defaultState
    }

    static inline void startByPasting(MainWindow& mainWindow, PlayArea& playArea, const ActionStarter& starter) {
        if (clipboard.empty()) return;
        
        auto& action = starter.start<SelectionAction>(mainWindow, State::SELECTED);

        ext::point windowOffset;
        SDL_GetMouseState(&windowOffset.x, &windowOffset.y);

        if (!point_in_rect(windowOffset, playArea.renderArea)) {
            // since the mouse is not in the play area, we paste in the middle of the play area
            windowOffset = ext::point{ playArea.renderArea.x + playArea.renderArea.w / 2, playArea.renderArea.y + playArea.renderArea.h / 2 };
        }
        ext::point canvasOffset = playArea.canvasFromWindowOffset(windowOffset);

        action.selection = clipboard; // have to make a copy, so that we don't mess up the clipboard
        action.selectionTrans = canvasOffset - action.selection.size() / 2; // set the selection offset as the current offset
        action.baseTrans = { 0, 0 };
    }

    ActionEventResult processPlayAreaMouseDrag(const SDL_MouseMotionEvent& event) override {
        // compute the canvasOffset here and restrict it to the visible area
        ext::point canvasOffset = playArea().canvasFromWindowOffset(ext::restrict_to_rect(event, playArea().renderArea));

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

    ActionEventResult processPlayAreaMouseButtonDown(const SDL_MouseButtonEvent& event) override {
        size_t inputHandleIndex = resolveInputHandleIndex(event);
        size_t currentToolIndex = mainWindow.selectedToolIndices[inputHandleIndex];

        ext::point canvasOffset = playArea().canvasFromWindowOffset(event);

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

    ActionEventResult processPlayAreaMouseButtonUp() {
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

    ActionEventResult processWindowKeyboard(const SDL_KeyboardEvent& event) override {
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
    bool disablePlayAreaDefaultRender() const override {
        return state != State::SELECTING;
    }

    // rendering function, render the selection (since base will already be rendered)
    void renderPlayAreaSurface(uint32_t* pixelBuffer, const SDL_Rect& renderRect) const override {
        if (state != State::SELECTING) {
            bool defaultView = playArea().isDefaultView();

            for (int32_t y = renderRect.y; y != renderRect.y + renderRect.h; ++y) {
                for (int32_t x = renderRect.x; x != renderRect.x + renderRect.w; ++x) {
                    SDL_Color computedColor{ 0, 0, 0, 0 };
                    const ext::point canvasPt{ x, y };

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
    void renderPlayAreaDirect(SDL_Renderer* renderer) const override {
        switch (state) {
        case State::SELECTING:
            {
                // normalize supplied points
                ext::point topLeft = ext::min(selectionOrigin, selectionEnd);
                ext::point bottomRight = ext::max(selectionOrigin, selectionEnd) + ext::point{ 1, 1 };

                ext::point windowTopLeft = playArea().windowFromCanvasOffset(topLeft);
                ext::point windowBottomRight = playArea().windowFromCanvasOffset(bottomRight);
                
                SDL_Rect selectionArea{
                    windowTopLeft.x,
                    windowTopLeft.y,
                    windowBottomRight.x - windowTopLeft.x,
                    windowBottomRight.y - windowTopLeft.y
                };

                SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
                SDL_RenderDrawRect(renderer, &selectionArea);
                SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
                ext::renderDrawDashedRect(renderer, &selectionArea);
            }
            break;
        case State::SELECTED:
            [[fallthrough]];
        case State::MOVING:
            {
                ext::point windowTopLeft = playArea().windowFromCanvasOffset(selectionTrans);
            
                SDL_Rect selectionArea{
                    windowTopLeft.x,
                    windowTopLeft.y,
                    selection.width() * playArea().getScale(),
                    selection.height() * playArea().getScale()
                };

                SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
                SDL_RenderDrawRect(renderer, &selectionArea);
                SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
                ext::renderDrawDashedRect(renderer, &selectionArea);
            }
            break;
        }
    }
};
