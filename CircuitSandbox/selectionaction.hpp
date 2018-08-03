#pragma once

#include <functional> // for std::reference_wrapper
#include <SDL.h>
#include "point.hpp"
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

    NotificationDisplay::UniqueNotification notification;
    bool notificationActive = false;

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

    // methods to set notifications:
    void showAddAndSubtractNotification();
    void hideNotification();
    void leaveSelectableState(State state);

public:
    SelectionAction(MainWindow& mainWindow, State state);

    // destructor, called to finish the action immediately
    ~SelectionAction() override;

    // TODO: refractor tool resolution out of SelectionAction and PencilAction, because its getting repetitive
    // TODO: refractor calculation of canvasOffset from event somewhere else as well

    // check if we need to start selection action from dragging/clicking the playarea
    static ActionEventResult startWithPlayAreaMouseButtonDown(const SDL_MouseButtonEvent& event, MainWindow& mainWindow, PlayArea& playArea, const ActionStarter& starter);

    static void startBySelectingAll(MainWindow& mainWindow, const ActionStarter& starter);

    static void startByPasting(MainWindow& mainWindow, PlayArea& playArea, const ActionStarter& starter, std::optional<int32_t> index = std::nullopt);

    ActionEventResult processPlayAreaMouseDrag(const SDL_MouseMotionEvent& event) override;

    ActionEventResult processPlayAreaMouseButtonDown(const SDL_MouseButtonEvent& event) override;

    ActionEventResult processPlayAreaMouseButtonUp() override;

    ActionEventResult processWindowKeyboard(const SDL_KeyboardEvent& event) override;


    // disable default rendering when not SELECTING
    bool disablePlayAreaDefaultRender() const override;

    // rendering function, render the selection (since base will already be rendered)
    void renderPlayAreaSurface(uint32_t* pixelBuffer, uint32_t pixelFormat, const SDL_Rect& renderRect, int32_t pitch) const override;

    // rendering function, render the selection rectangle if it exists
    void renderPlayAreaDirect(SDL_Renderer* renderer) const override;
};
