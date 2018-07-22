#pragma once

#include <functional> // for std::reference_wrapper
#include <SDL.h>
#include "point.hpp"
#include "drawing.hpp"
#include "saveableaction.hpp"
#include "playarea.hpp"
#include "mainwindow.hpp"
#include "eventhook.hpp"
#include "sdl_fast_maprgb.hpp"
#include "expandable_matrix.hpp"
#include "clipboardaction.hpp"
#include "clipboardmanager.hpp"


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

    // in State::SELECTED and State::MOVING, these are the translations of selection, relative to defaultState
    ext::point selectionTrans = { 0, 0 }; // in canvas coordinates

    // in State::MOVING, this was the previous point of the mouse
    ext::point moveOrigin; // in canvas coordinates

    friend class ClipboardAction;

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
            auto tmpBase = CanvasState::merge(std::move(unselected), topLeft - unselectedShrinkTrans, std::move(base), { 0, 0 }).first;
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
    template <bool UseExtendedComponent>
    bool selectConnectedComponent(const ext::point& pt, bool subtract) {
        CanvasState& base = canvas();

        // splice out the connected component
        auto [connectedComponent, componentTrans] = spliceConnectedComponent<UseExtendedComponent>(pt);

        if (subtract) {
            // merge the component with base
            auto tmpBase = CanvasState::merge(std::move(base), { 0, 0 }, std::move(connectedComponent), componentTrans).first;
            base = std::move(tmpBase);
        }
        else {
            // merge the component with selection
            auto [tmpSelection, translation] = CanvasState::merge(std::move(connectedComponent), componentTrans, std::move(selection), selectionTrans);
            selection = std::move(tmpSelection);
            selectionTrans = -std::move(translation);
        }

        // shrink the selection
        selectionTrans -= selection.shrinkDataMatrix();

        return !selection.empty();
    }


    /**
    * Moves logically connected elements from base and selection to a new CanvasState and returns them.
    * Returns the new CanvasState and the translation relative to the base.
    * When ExtendedComponent is true, the connected component contains everything adjacent (ignoring std::monostate).
    * This method will resize neither the base nor the selection.  The spliced elements are replaced by std::monostate.
    * Both base and selection will not be shrinked.
    */
    template <bool UseExtendedComponent>
    std::pair<CanvasState, ext::point> spliceConnectedComponent(const ext::point& origin) {
        CanvasState& base = canvas();

        if (!base.contains(origin) ||
            (std::holds_alternative<std::monostate>(base[origin]) &&
            (!selection.contains(origin - selectionTrans) || std::holds_alternative<std::monostate>(selection[origin - selectionTrans])))) {
            return { CanvasState{}, { 0, 0 } };
        }

        CanvasState newState;

        ext::point minPt = ext::point::max();
        ext::point maxPt = ext::point::min();
        std::vector<std::pair<ext::point, std::reference_wrapper<CanvasState::element_variant_t>>> componentData;

        // get the min and max bounds
        floodFillConnectedComponent<UseExtendedComponent>(origin, [&](const ext::point& pt, CanvasState::element_variant_t& element) {
            minPt = min(minPt, pt);
            maxPt = max(maxPt, pt);
            assert(element.index() > 0 && element.index() != std::variant_npos);
            componentData.emplace_back(pt, element);
        });

        ext::point spliceSize = maxPt - minPt + ext::point{ 1, 1 };

        newState.dataMatrix = CanvasState::matrix_t(spliceSize);

        // swap the data over to the new state
        while (!componentData.empty()) {
            auto& [pt, element] = componentData.back();

            using std::swap;
            // swap will give correct results because matrix_t is initialized to std::monostate elements
            swap(element.get(), newState.dataMatrix[pt - minPt]);

            componentData.pop_back();
        }

        return { std::move(newState), minPt };
    }

    /**
    * Runs a floodfill (customized by the ExtendedComponent policy).
    * Calls callback(ext::point) for each point being visited (including the origin).
    */
    template <bool UseExtendedComponent, typename Callback>
    void floodFillConnectedComponent(const ext::point& origin, Callback callback) {
        CanvasState& base = canvas();

        if constexpr(!UseExtendedComponent) {
            // using double-click
            struct Visited {
                bool dir[2] = { false, false }; // { 0: horizontal, 1: vertical }
            };
            ext::heap_matrix<Visited> visitedMatrix(base.width(), base.height());

            std::stack<std::pair<ext::point, int>> pendingVisit;
            // flood fill from origin along both axes; guaranteed to never visit monostate
            pendingVisit.emplace(origin, 0);
            pendingVisit.emplace(origin, 1);

            while (!pendingVisit.empty()) {
                auto[pt, axis] = pendingVisit.top();
                pendingVisit.pop();

                if (visitedMatrix[pt].dir[axis]) continue;
                visitedMatrix[pt].dir[axis] = true;

                auto& currElement = selection.contains(pt - selectionTrans) && !std::holds_alternative<std::monostate>(selection[pt - selectionTrans]) ? selection[pt - selectionTrans] : base[pt];

                assert(!std::holds_alternative<std::monostate>(currElement));

                // call the callback (only if this is the first direction added, so the callback won't get duplicate points)
                if (visitedMatrix[pt].dir[0] ^ visitedMatrix[pt].dir[1]) {
                    callback(pt, currElement);
                }

                // visit other axis unless this is an insulated wire
                if (!visitedMatrix[pt].dir[axis ^ 1] && !std::holds_alternative<InsulatedWire>(currElement)) {
                    pendingVisit.emplace(pt, axis ^ 1);
                }

                using positive_one_t = std::integral_constant<int32_t, 1>;
                using negative_one_t = std::integral_constant<int32_t, -1>;
                using directions_t = ext::tag_tuple<negative_one_t, positive_one_t>;
                directions_t::for_each([&](auto direction_tag_t, auto) {
                    auto[x, y] = pt;
                    if (axis == 0) {
                        x += decltype(direction_tag_t)::type::value;
                    }
                    else {
                        y += decltype(direction_tag_t)::type::value;
                    }
                    ext::point nextPt{ x, y };

                    if (base.contains(nextPt) && !visitedMatrix[nextPt].dir[axis]) {
                        const auto& nextElement = selection.contains(nextPt - selectionTrans) && !std::holds_alternative<std::monostate>(selection[nextPt - selectionTrans]) ? selection[nextPt - selectionTrans] : base[nextPt];

                        if ((std::holds_alternative<ConductiveWire>(currElement) || std::holds_alternative<InsulatedWire>(currElement)) &&
                            (std::holds_alternative<ConductiveWire>(nextElement) || std::holds_alternative<InsulatedWire>(nextElement))) {
                            pendingVisit.emplace(nextPt, axis);
                        }
                        else if (std::holds_alternative<Signal>(currElement) &&
                            (std::holds_alternative<AndGate>(nextElement) || std::holds_alternative<OrGate>(nextElement) || std::holds_alternative<NandGate>(nextElement) ||
                                std::holds_alternative<NorGate>(nextElement) || std::holds_alternative<PositiveRelay>(nextElement) || std::holds_alternative<NegativeRelay>(nextElement))) {
                            pendingVisit.emplace(nextPt, axis);
                        }
                        else if ((std::holds_alternative<AndGate>(currElement) || std::holds_alternative<OrGate>(currElement) || std::holds_alternative<NandGate>(currElement) ||
                            std::holds_alternative<NorGate>(currElement) || std::holds_alternative<PositiveRelay>(currElement) || std::holds_alternative<NegativeRelay>(currElement)) &&
                            std::holds_alternative<Signal>(nextElement)) {
                            pendingVisit.emplace(nextPt, axis);
                        }
                        else if (std::holds_alternative<ScreenCommunicatorElement>(currElement) &&
                            std::holds_alternative<ScreenCommunicatorElement>(nextElement)) {
                            pendingVisit.emplace(nextPt, axis);
                        }
                        else if (std::holds_alternative<FileInputCommunicatorElement>(currElement) &&
                            std::holds_alternative<FileInputCommunicatorElement>(nextElement)) {
                            pendingVisit.emplace(nextPt, axis);
                        }
                        else if (std::holds_alternative<FileOutputCommunicatorElement>(currElement) &&
                            std::holds_alternative<FileOutputCommunicatorElement>(nextElement)) {
                            pendingVisit.emplace(nextPt, axis);
                        }
                    }
                });
            }
        }
        else {
            // using triple-click
            ext::heap_matrix<bool> visitedMatrix(base.width(), base.height());
            visitedMatrix.fill(false);

            std::stack<ext::point> pendingVisit;
            // flood fill from origin along both axes; guaranteed to never visit monostate
            pendingVisit.emplace(origin);

            while (!pendingVisit.empty()) {
                auto pt = pendingVisit.top();
                pendingVisit.pop();

                if (visitedMatrix[pt]) continue;
                visitedMatrix[pt] = true;

                auto& currElement = selection.contains(pt - selectionTrans) && !std::holds_alternative<std::monostate>(selection[pt - selectionTrans]) ? selection[pt - selectionTrans] : base[pt];

                assert(!std::holds_alternative<std::monostate>(currElement));

                // call the callback
                callback(pt, currElement);

                using positive_one_t = std::integral_constant<int32_t, 1>;
                using zero_t = std::integral_constant<int32_t, 0>;
                using negative_one_t = std::integral_constant<int32_t, -1>;
                using directions_t = ext::tag_tuple<std::pair<zero_t, negative_one_t>, std::pair<positive_one_t, zero_t>, std::pair<zero_t, positive_one_t>, std::pair<negative_one_t, zero_t>>;
                directions_t::for_each([&](auto direction_tag_t, auto) {
                    
                    ext::point nextPt = pt;
                    nextPt.x += decltype(direction_tag_t)::type::first_type::value;
                    nextPt.y += decltype(direction_tag_t)::type::second_type::value;
                    if (base.contains(nextPt) && !visitedMatrix[nextPt]) {
                        const auto& nextElement = selection.contains(nextPt - selectionTrans) && !std::holds_alternative<std::monostate>(selection[nextPt - selectionTrans]) ? selection[nextPt - selectionTrans] : base[nextPt];
                        if (!std::holds_alternative<std::monostate>(nextElement)) {
                            pendingVisit.emplace(nextPt);
                        }
                    }
                });
            }
        }
    }

public:
    SelectionAction(MainWindow& mainWindow, State state) :SaveableAction(mainWindow), KeyboardEventHook(mainWindow), state(state) {}

    // destructor, called to finish the action immediately
    ~SelectionAction() override {
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
        action.selectionTrans = { 0, 0 };
        // action.selectionOrigin = { 0, 0 };
        // action.selectionEnd = action.selection.size() - ext::point{ 1, 1 };
    }

    static inline void startByPasting(MainWindow& mainWindow, PlayArea& playArea, const ActionStarter& starter, std::optional<int32_t> index = std::nullopt) {
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

    ActionEventResult processPlayAreaMouseButtonUp() {
        switch (state) {
            case State::SELECTING:
                state = State::SELECTED;
                if (selectRect(SDL_GetModState() & KMOD_ALT)) {
                    return ActionEventResult::PROCESSED;
                } else {
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
            case SDL_SCANCODE_RIGHT:
                state = State::MOVED;
                if (modifiers & KMOD_SHIFT) {
                    selectionTrans += { 4, 0 };
                }
                else {
                    selectionTrans += { 1, 0 };
                }
                return ActionEventResult::PROCESSED;
            case SDL_SCANCODE_LEFT:
                state = State::MOVED;
                if (modifiers & KMOD_SHIFT) {
                    selectionTrans += { -4, 0 };
                }
                else {
                    selectionTrans += { -1, 0 };
                }
                return ActionEventResult::PROCESSED;
            case SDL_SCANCODE_UP:
                state = State::MOVED;
                if (modifiers & KMOD_SHIFT) {
                    selectionTrans += { 0, -4 };
                }
                else {
                    selectionTrans += { 0, -1 };
                }
                return ActionEventResult::PROCESSED;
            case SDL_SCANCODE_DOWN:
                state = State::MOVED;
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
                state = State::MOVED;
                selection = CanvasState(); // clear the selection
                return ActionEventResult::COMPLETED; // tell playarea to end this action
            case SDL_SCANCODE_C:
                if (modifiers & KMOD_CTRL) {
                    if (modifiers & KMOD_SHIFT) {
                        ClipboardAction::startCopyDialog(mainWindow, mainWindow.renderer, mainWindow.currentAction.getStarter(), selection);
                        return ActionEventResult::PROCESSED;
                    }
                    else {
                        mainWindow.clipboard.write(mainWindow.renderer, selection);
                        return ActionEventResult::PROCESSED;
                    }
                }
            case SDL_SCANCODE_X:
                if (modifiers & KMOD_CTRL) {
                    state = State::MOVED;
                    mainWindow.clipboard.write(mainWindow.renderer, std::move(selection));
                    selection = CanvasState(); // clear the selection
                    return ActionEventResult::COMPLETED; // tell playarea to end this action
                }
            case SDL_SCANCODE_ESCAPE:
                return ActionEventResult::COMPLETED;
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

    const char* getStatus() const {
        return selection.empty() || state == State::MOVED || state == State::MOVING ? nullptr : "Hold Shift to add to the selection, Alt to subtract from it";
    }
};
