#include "selectionaction.hpp"
#include "drawing.hpp"
#include "sdl_fast_maprgb.hpp"
#include "expandable_matrix.hpp"
#include "clipboardaction.hpp"
#include "clipboardmanager.hpp"
#include "notificationdisplay.hpp"



SelectionAction::SelectionAction(MainWindow& mainWindow, State state) :SaveableAction(mainWindow), KeyboardEventHook(mainWindow), state(state) {
    if (state == State::SELECTED) {
        showAddAndSubtractNotification();
    }
}

// destructor, called to finish the action immediately
SelectionAction::~SelectionAction() {
    // note that base is only shrunk on destruction
    auto baseTrans = canvas().shrinkDataMatrix();
    if (state != State::SELECTING) {
        auto[tmpDefaultState, translation] = CanvasState::merge(std::move(canvas()), -baseTrans, std::move(selection), selectionTrans);
        canvas() = std::move(tmpDefaultState);
        deltaTrans = std::move(translation);
    }
    if (state != State::MOVED) {
        // request not to recompile the canvas state, because nothing changed
        changed() = false;
    }
}

// check if we need to start selection action from dragging/clicking the playarea
ActionEventResult SelectionAction::startWithPlayAreaMouseButtonDown(const SDL_MouseButtonEvent& event, MainWindow& mainWindow, PlayArea& playArea, const ActionStarter& starter) {
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

void SelectionAction::startBySelectingAll(MainWindow& mainWindow, const ActionStarter& starter) {
    if (mainWindow.stateManager.defaultState.empty()) return;

    auto& action = starter.start<SelectionAction>(mainWindow, State::SELECTED);
    action.selection = std::move(action.canvas().splice(0, 0, action.canvas().width(), action.canvas().height()));
    action.selectionTrans = { 0, 0 };
    // action.selectionOrigin = { 0, 0 };
    // action.selectionEnd = action.selection.size() - ext::point{ 1, 1 };
}

void SelectionAction::startByPasting(MainWindow& mainWindow, PlayArea& playArea, const ActionStarter& starter, std::optional<int32_t> index) {
    auto& action = starter.start<SelectionAction>(mainWindow, State::MOVED);
    action.selection = index ? mainWindow.clipboard.read(*index) : mainWindow.clipboard.read();
    if (action.selection.empty()) return;

    ext::point windowOffset;
    SDL_GetMouseState(&windowOffset.x, &windowOffset.y);

    if (!point_in_rect(windowOffset, playArea.renderArea)) {
        // since the mouse is not in the play area, we paste in the middle of the play area
        windowOffset = ext::point{ playArea.renderArea.x + playArea.renderArea.w / 2, playArea.renderArea.y + playArea.renderArea.h / 2 };
    }
    ext::point canvasOffset = playArea.canvasFromWindowOffset(windowOffset);

    action.selectionTrans = canvasOffset - action.selection.size() / 2; // set the selection offset as the current offset
}

ActionEventResult SelectionAction::processPlayAreaMouseDrag(const SDL_MouseMotionEvent& event) {
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
            leaveSelectableState(State::MOVING);
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

ActionEventResult SelectionAction::processPlayAreaMouseButtonDown(const SDL_MouseButtonEvent& event) {
    size_t inputHandleIndex = resolveInputHandleIndex(event);
    size_t currentToolIndex = mainWindow.selectedToolIndices[inputHandleIndex];

    ext::point canvasOffset = playArea().canvasFromWindowOffset(event);

    return tool_tags_t::get(currentToolIndex, [this, &event, &canvasOffset](const auto tool_tag) {
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
                else if (event.clicks >= 2) {
                    // select connected components if clicking outside the current selection or if shift/alt is held
                    if (event.clicks == 2) {
                        selectConnectedComponent<false>(canvasOffset, modifiers & KMOD_ALT);
                    }
                    else {
                        selectConnectedComponent<true>(canvasOffset, modifiers & KMOD_ALT);
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

ActionEventResult SelectionAction::processPlayAreaMouseButtonUp() {
    switch (state) {
    case State::SELECTING:
        state = State::SELECTED;
        if (selectRect(SDL_GetModState() & KMOD_ALT)) {
            showAddAndSubtractNotification();
            return ActionEventResult::PROCESSED;
        }
        else {
            // we get here if we didn't select anything
            selectionTrans = { 0, 0 }; // set these values, because selectRect won't set if it returns false
            return ActionEventResult::COMPLETED;
        }
    case State::MOVING:
        state = State::MOVED;
        return ActionEventResult::PROCESSED;
    default:
        return ActionEventResult::UNPROCESSED;
    }
}

ActionEventResult SelectionAction::processWindowKeyboard(const SDL_KeyboardEvent& event) {
    // TODO: follow style of MainWindow's keyboard handler, so that if 'Shift' is held then things like 'H' key will be disabled.
    if (event.type == SDL_KEYDOWN) {
        SDL_Keymod modifiers = static_cast<SDL_Keymod>(event.keysym.mod);
        switch (event.keysym.scancode) {
        case SDL_SCANCODE_H:
            leaveSelectableState(State::MOVED);
            selection.flipHorizontal();
            return ActionEventResult::PROCESSED;
        case SDL_SCANCODE_V:
            if (!(modifiers & KMOD_CTRL)) {
                leaveSelectableState(State::MOVED);
                selection.flipVertical();
                return ActionEventResult::PROCESSED;
            }
            return ActionEventResult::UNPROCESSED;
        case SDL_SCANCODE_LEFTBRACKET: // [
            leaveSelectableState(State::MOVED);
            selection.rotateCounterClockwise();
            return ActionEventResult::PROCESSED;
        case SDL_SCANCODE_RIGHTBRACKET: // ]
            leaveSelectableState(State::MOVED);
            selection.rotateClockwise();
            return ActionEventResult::PROCESSED;
        case SDL_SCANCODE_RIGHT:
            leaveSelectableState(State::MOVED);
            if (modifiers & KMOD_SHIFT) {
                selectionTrans += { 4, 0 };
            }
            else {
                selectionTrans += { 1, 0 };
            }
            return ActionEventResult::PROCESSED;
        case SDL_SCANCODE_LEFT:
            leaveSelectableState(State::MOVED);
            if (modifiers & KMOD_SHIFT) {
                selectionTrans += { -4, 0 };
            }
            else {
                selectionTrans += { -1, 0 };
            }
            return ActionEventResult::PROCESSED;
        case SDL_SCANCODE_UP:
            leaveSelectableState(State::MOVED);
            if (modifiers & KMOD_SHIFT) {
                selectionTrans += { 0, -4 };
            }
            else {
                selectionTrans += { 0, -1 };
            }
            return ActionEventResult::PROCESSED;
        case SDL_SCANCODE_DOWN:
            leaveSelectableState(State::MOVED);
            if (modifiers & KMOD_SHIFT) {
                selectionTrans += { 0, 4 };
            }
            else {
                selectionTrans += { 0, 1 };
            }
            return ActionEventResult::PROCESSED;
        case SDL_SCANCODE_D:
            [[fallthrough]];
        case SDL_SCANCODE_DELETE:
            leaveSelectableState(State::MOVED);
            selection = CanvasState(); // clear the selection
            return ActionEventResult::COMPLETED; // tell playarea to end this action
        case SDL_SCANCODE_C:
            if (modifiers & KMOD_CTRL) {
                if (modifiers & KMOD_SHIFT) {
                    ClipboardAction::startCopyDialog(mainWindow, mainWindow.renderer, mainWindow.currentAction.getStarter(), selection);
                    return ActionEventResult::PROCESSED;
                }
                else {
                    mainWindow.clipboard.write(selection);
                    return ActionEventResult::PROCESSED;
                }
            }
            return ActionEventResult::UNPROCESSED;
        case SDL_SCANCODE_X:
            if (modifiers & KMOD_CTRL) {
                leaveSelectableState(State::MOVED);
                mainWindow.clipboard.write(std::move(selection));
                selection = CanvasState(); // clear the selection
                return ActionEventResult::COMPLETED; // tell playarea to end this action
            }
            return ActionEventResult::UNPROCESSED;
        case SDL_SCANCODE_ESCAPE:
            return ActionEventResult::COMPLETED;
        default:
            return ActionEventResult::UNPROCESSED;
        }
    }
    return ActionEventResult::UNPROCESSED;
}



// disable default rendering when not SELECTING
bool SelectionAction::disablePlayAreaDefaultRender() const {
    return !selection.empty();
}

// rendering function, render the selection (since base will already be rendered)
void SelectionAction::renderPlayAreaSurface(uint32_t* pixelBuffer, uint32_t pixelFormat, const SDL_Rect& renderRect, int32_t pitch) const {
    if (!selection.empty()) {
        bool defaultView = playArea().isDefaultView();

        invoke_RGB_format(pixelFormat, [&](const auto format_tag) {
            using FormatType = decltype(format_tag);
            invoke_bool(defaultView, [&](const auto defaultView_tag) {
                using DefaultViewType = decltype(defaultView_tag);

                for (int32_t y = renderRect.y; y != renderRect.y + renderRect.h; ++y, pixelBuffer += pitch) {
                    uint32_t* pixel = pixelBuffer;
                    for (int32_t x = renderRect.x; x != renderRect.x + renderRect.w; ++x, ++pixel) {
                        const ext::point canvasPt{ x, y };
                        uint32_t color = 0;
                        // draw base, check if the requested pixel inside the buffer
                        if (canvas().contains(canvasPt)) {
                            std::visit(visitor{
                                [](std::monostate) {},
                                [&color](const auto& element) {
                                color = fast_MapRGB<FormatType::value>(element.template computeDisplayColor<DefaultViewType::value>());
                            },
                                }, canvas()[canvasPt]);
                        }

                        // draw selection, check if the requested pixel inside the buffer
                        if (selection.contains(canvasPt - selectionTrans)) {
                            std::visit(visitor{
                                [](std::monostate) {},
                                [this, &color](const auto& element) {
                                alignas(uint32_t) SDL_Color computedColor = element.template computeDisplayColor<DefaultViewType::value>();
                                if (state == State::SELECTING || state == State::SELECTED) {
                                    computedColor.b = 0xFF; // colour the selection blue if it can still be modified
                                }
                                else {
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
        });
    }
}

// rendering function, render the selection rectangle if it exists
void SelectionAction::renderPlayAreaDirect(SDL_Renderer* renderer) const {
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

void SelectionAction::showAddAndSubtractNotification() {
    // add notification
    notification = mainWindow.getNotificationDisplay().uniqueAdd(NotificationFlags::BEGINNER, NotificationDisplay::Data{
        { "Hold", NotificationDisplay::TEXT_COLOR },
        { " Shift", NotificationDisplay::TEXT_COLOR_KEY },
        { " to add to the selection or", NotificationDisplay::TEXT_COLOR },
        { " Alt", NotificationDisplay::TEXT_COLOR_KEY },
        { " to subtract from it;", NotificationDisplay::TEXT_COLOR },
        { " double-click", NotificationDisplay::TEXT_COLOR_KEY },
        { " to select logically connected components or", NotificationDisplay::TEXT_COLOR },
        { " triple-click", NotificationDisplay::TEXT_COLOR_KEY },
        { " to select all reachable elements", NotificationDisplay::TEXT_COLOR } });
}

void SelectionAction::hideNotification() {
    notification.reset();
}

void SelectionAction::leaveSelectableState(State newState) {
    // hides the add/subtract notification, then sets the state to something else
    hideNotification();
    state = newState;
}
