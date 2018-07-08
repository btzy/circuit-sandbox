#pragma once

#include <SDL.h>
#include "point.hpp"
#include "drawing.hpp"
#include "saveableaction.hpp"
#include "playarea.hpp"
#include "mainwindow.hpp"
#include "eventhook.hpp"
#include "sdl_fast_maprgb.hpp"
#include "expandable_matrix.hpp"


/**
 * Action that represents selection.
 */

class SelectionAction final : public SaveableAction, public KeyboardEventHook {

private:
    enum class State {
        SELECTING,
        SELECTED,
        MOVING,
        MOVED
    };

    State state;

    // in State::SELECTING, selectionOrigin is the point where the mouse went down, and selectionEnd is the current point of the mouse
    ext::point selectionOrigin; // in canvas coordinates
    ext::point selectionEnd; // in canvas coordinates

    // when entering State::SELECTED these will be set; after that these will not be modified
    CanvasState selection; // stores the selection

    // in State::SELECTED and State::MOVING, these are the translations of selection and base, relative to defaultState
    ext::point selectionTrans = { 0, 0 }; // in canvas coordinates
    ext::point baseTrans = { 0, 0 }; // in canvas coordinates

    // in State::MOVING, this was the previous point of the mouse
    ext::point moveOrigin; // in canvas coordinates

    static CanvasState clipboard; // the single clipboard; will be changed to support multi-clipboard

    /**
     * Prepare base, selection, baseTrans, selectionTrans from selectionOrigin and selectionEnd. Returns true if the selection is not empty.
     * When this method is called, selection is guaranteed to lie within base since base is only shrunk on destruction.
     * If subtract is true, elements in the selection rect are removed from selection.
     */
    bool selectRect(bool subtract) {
        CanvasState& base = canvas();

        // normalize supplied points
        ext::point topLeft = ext::min(selectionOrigin, selectionEnd); // inclusive
        ext::point bottomRight = ext::max(selectionOrigin, selectionEnd) + ext::point{ 1, 1 }; // exclusive

        if (subtract) {
            if (selection.empty()) return false;

            // restrict selectionRect to the area within working selection
            topLeft = ext::max(topLeft, selectionTrans);
            bottomRight = ext::min(bottomRight, selectionTrans + selection.size());
            ext::point selectionSize = bottomRight - topLeft;

            // no elements in the selection rect
            if (selectionSize.x <= 0 || selectionSize.y <= 0) return !selection.empty();

            // splice out a part of the working selection
            CanvasState unselected = selection.splice(topLeft.x - selectionTrans.x, topLeft.y - selectionTrans.y, selectionSize.x, selectionSize.y);
            selectionTrans -= selection.shrinkDataMatrix();

            // shrink the unselected part
            ext::point unselectedShrinkTrans = unselected.shrinkDataMatrix();

            // merge the unselected part with base
            auto tmpBase = CanvasState::merge(std::move(unselected), topLeft - unselectedShrinkTrans, std::move(base), baseTrans).first;
            base = std::move(tmpBase);
        }
        else {
            // restrict selectionRect to the area within base
            topLeft = ext::max(topLeft, { 0, 0 });
            bottomRight = ext::min(bottomRight, base.size());
            ext::point selectionSize = bottomRight - topLeft;

            // no elements in the selection rect
            if (selectionSize.x <= 0 || selectionSize.y <= 0) return !selection.empty();

            // splice out the newest selection from the base
            CanvasState newSelection = base.splice(topLeft.x, topLeft.y, selectionSize.x, selectionSize.y);

            // shrink the newest selection
            ext::point selectionShrinkTrans = newSelection.shrinkDataMatrix();

            // merge the newest selection with selection
            auto [tmpSelection, translation] = CanvasState::merge(std::move(newSelection), topLeft - selectionShrinkTrans, std::move(selection), selectionTrans);
            selection = std::move(tmpSelection);
            selectionTrans = -std::move(translation);
        }

        return !selection.empty();
    }

    /**
     * Select all elements that are logically connected to the element at pt.
     * When this method is called, selection is guaranteed to lie within base since base is only shrunk on destruction.
     * If subtract is true, elements in the selection rect are removed from selection.
     */
    bool selectConnectedComponent(ext::point pt, bool subtract) {
        CanvasState& base = canvas();

        if (subtract) {
            // if pt contains an element, it is guaranteed to be in base from the first click
            // move the origin of selection back to selection - it will be moved back later
            // it is ok if there is no element at pt
            if (!base.contains(pt)) return !selection.empty();
            CanvasState origin = base.splice(pt.x, pt.y, 1, 1);
            auto [newSelection, translation] = CanvasState::merge(std::move(origin), pt, std::move(selection), selectionTrans);
            selection = std::move(newSelection);
            selectionTrans = -translation;

            auto [unselected, unselectedTrans] = selection.spliceConnectedComponent(pt - selectionTrans);
            unselectedTrans += selectionTrans;
            selectionTrans -= selection.shrinkDataMatrix();

            // merge the unselected part with base
            auto tmpBase = CanvasState::merge(std::move(unselected), unselectedTrans, std::move(base), baseTrans).first;
            base = std::move(tmpBase);
        }
        else {
            // if pt contains an element, it is guaranteed to be in selection from the first click
            // move the origin of selection back to base - it will be moved back later
            // it is ok if there is no element at pt
            if (!selection.contains(pt - selectionTrans)) return !selection.empty();
            CanvasState origin = selection.splice(pt.x - selectionTrans.x, pt.y - selectionTrans.y, 1, 1);
            base = std::move(CanvasState::merge(std::move(origin), pt, std::move(base), baseTrans).first);

            auto [newSelection, newSelectionTrans] = base.spliceConnectedComponent(pt);

            // merge the newest selection with selection
            auto [tmpSelection, translation] = CanvasState::merge(std::move(newSelection), newSelectionTrans, std::move(selection), selectionTrans);
            selection = std::move(tmpSelection);
            selectionTrans = -std::move(translation);
        }
        return !selection.empty();
    }

public:
    SelectionAction(MainWindow& mainWindow, State state) :SaveableAction(mainWindow), KeyboardEventHook(mainWindow), state(state) {}

    // destructor, called to finish the action immediately
    ~SelectionAction() override {
        // note that base is only shrunk on destruction
        baseTrans -= canvas().shrinkDataMatrix();
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
        action.selection = std::move(action.canvas().splice(0, 0, action.canvas().width(), action.canvas().height()));
        action.selectionTrans = action.baseTrans = { 0, 0 };
        // action.selectionOrigin = { 0, 0 };
        // action.selectionEnd = action.selection.size() - ext::point{ 1, 1 };
    }

    static inline void startByPasting(MainWindow& mainWindow, PlayArea& playArea, const ActionStarter& starter) {
        if (clipboard.empty()) return;

        auto& action = starter.start<SelectionAction>(mainWindow, State::MOVED);

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
            // this situation only happens if the mouse is held down but not dragged past moveOrigin
            // once it is dragged past moveOrigin, the selection can no longer be modified
            if (moveOrigin != canvasOffset) {
                selectionTrans += canvasOffset - moveOrigin;
                moveOrigin = canvasOffset;
                state = State::MOVING;
            }
            return ActionEventResult::PROCESSED;
        case State::MOVED:
            // something's wrong -- if the mouse is being held down, we shouldn't be in MOVED state
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

        return tool_tags_t::get(currentToolIndex, [this, event, &canvasOffset](const auto tool_tag) {
            // 'Tool' is the type of tool (e.g. Selector)
            using Tool = typename decltype(tool_tag)::type;

            if constexpr (std::is_base_of_v<Selector, Tool>) {
                switch (state) {
                case State::SELECTED:
                {
                    SDL_Keymod modifiers = SDL_GetModState();
                    if (event.clicks == 1) {
                        // check if only one of shift or alt is held down
                        if (!!(modifiers & KMOD_SHIFT) ^ !!(modifiers & KMOD_ALT)) {
                            state = State::SELECTING;
                            selectionEnd = selectionOrigin = canvasOffset;
                            return ActionEventResult::PROCESSED;
                        }
                        if (!selection.contains(canvasOffset - selectionTrans)) {
                            // end selection
                            return ActionEventResult::CANCELLED; // this is on purpose, so that we can enter a new selection action with the same event.
                        }
                        // do not transition to MOVING yet because we want to check for double clicks
                    }
                    else if (event.clicks == 2) {
                        // select connected components if clicking outside the current selection or if shift/alt is held
                        if ((selection.width() == 1 && selection.height() == 1) || (!!(modifiers & KMOD_SHIFT) ^ !!(modifiers & KMOD_ALT))) {
                            selectConnectedComponent(canvasOffset, modifiers & KMOD_ALT);
                        }
                    }
                    moveOrigin = canvasOffset;
                    return ActionEventResult::PROCESSED;
                }
                case State::MOVED:
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
                if (selectRect(SDL_GetModState() & KMOD_ALT)) {
                    return ActionEventResult::PROCESSED;
                } else {
                    selectionTrans = baseTrans = { 0, 0 }; // set these values, because selectRect won't set if it returns false
                    return ActionEventResult::COMPLETED;
                }
            case State::MOVING:
                state = State::MOVED;
                return ActionEventResult::PROCESSED;
            default:
                return ActionEventResult::UNPROCESSED;
        }
    }

    ActionEventResult processWindowKeyboard(const SDL_KeyboardEvent& event) override {
        if (event.type == SDL_KEYDOWN) {
            SDL_Keymod modifiers = static_cast<SDL_Keymod>(event.keysym.mod);
            switch (event.keysym.scancode) {
            case SDL_SCANCODE_H:
                state = State::MOVED;
                selection.flipHorizontal();
                return ActionEventResult::PROCESSED;
            case SDL_SCANCODE_V:
                if (!(modifiers & KMOD_CTRL)) {
                    state = State::MOVED;
                    selection.flipVertical();
                    return ActionEventResult::PROCESSED;
                }
                return ActionEventResult::UNPROCESSED;
            case SDL_SCANCODE_LEFTBRACKET: // [
                state = State::MOVED;
                selection.rotateCounterClockwise();
                return ActionEventResult::PROCESSED;
            case SDL_SCANCODE_RIGHTBRACKET: // ]
                state = State::MOVED;
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
        return !selection.empty();
    }

    // rendering function, render the selection (since base will already be rendered)
    void renderPlayAreaSurface(uint32_t* pixelBuffer, uint32_t pixelFormat, const SDL_Rect& renderRect, int32_t pitch) const override {
        if (!selection.empty()) {
            bool defaultView = playArea().isDefaultView();

            invoke_RGB_format(pixelFormat, [&](const auto format) {
                using FormatType = decltype(format);
                for (int32_t y = renderRect.y; y != renderRect.y + renderRect.h; ++y, pixelBuffer += pitch) {
                    uint32_t* pixel = pixelBuffer;
                    for (int32_t x = renderRect.x; x != renderRect.x + renderRect.w; ++x, ++pixel) {
                        const ext::point canvasPt{ x, y };
                        uint32_t color = 0;
                        // draw base, check if the requested pixel inside the buffer
                        if (canvas().contains(canvasPt - baseTrans)) {
                            std::visit(visitor{
                                [](std::monostate) {},
                                [&color, defaultView](const auto& element) {
                                    color = fast_MapRGB<FormatType::value>(computeDisplayColor(element, defaultView));
                                },
                            }, canvas()[canvasPt - baseTrans]);
                        }

                        // draw selection, check if the requested pixel inside the buffer
                        if (selection.contains(canvasPt - selectionTrans)) {
                            std::visit(visitor{
                                [](std::monostate) {},
                                [this, &color, defaultView](const auto& element) {
                                    alignas(uint32_t) SDL_Color computedColor = computeDisplayColor(element, defaultView);
                                    if (state == State::SELECTING || state == State::SELECTED) {
                                        computedColor.b = 0xFF; // colour the selection blue if it can still be modified
                                    } else {
                                        computedColor.r = 0xFF; // otherwise colour the selection red
                                    }
                                    color = fast_MapRGB<FormatType::value>(computedColor);
                                },
                            }, selection[canvasPt - selectionTrans]);
                        }
                        *pixel = color;
                    }
                }
            });
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
            [[fallthrough]];
        case State::SELECTED:
            [[fallthrough]];
        case State::MOVING:
            [[fallthrough]];
        case State::MOVED:
            if (!selection.empty()) {
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
