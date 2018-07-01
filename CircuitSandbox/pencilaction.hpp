#pragma once

#include <cmath>
#include <type_traits>
#include <variant>

#include <SDL.h>
#include "point.hpp"
#include "saveableaction.hpp"
#include "statemanager.hpp"
#include "playarea.hpp"
#include "mainwindow.hpp"
#include "canvasstate.hpp"
#include "elements.hpp"
#include "expandable_matrix.hpp"


/**
 * Action that represents a pencil that can draw an element (or eraser).
 */

// anonymous namespace to put helper functions that won't leak out to other translation units
namespace {
    // calls callback(pt) for every point between `from` and `to`, excluding `from` but including `to`.
    template <typename Callback>
    void interpolate(const ext::point& from, const ext::point& to, Callback&& callback) {
        int32_t dx = to.x - from.x;
        int32_t dy = to.y - from.y;
        if (dx == 0 && dy == 0) {
            return;
        }
        else if (dx >= std::abs(dy)) {
            for (int32_t x = from.x + 1; x <= to.x; ++x) {
                int32_t y = ext::div_round(dy * (x - from.x), dx) + from.y;
                callback(ext::point{ x, y });
            }
        }
        else if (-dx >= std::abs(dy)) {
            for (int32_t x = from.x - 1; x >= to.x; --x) {
                int32_t y = ext::div_round(dy * (from.x - x), -dx) + from.y;
                callback(ext::point{ x, y });
            }
        }
        else if (dy >= std::abs(dx)) {
            for (int32_t y = from.y + 1; y <= to.y; ++y) {
                int32_t x = ext::div_round(dx * (y - from.y), dy) + from.x;
                callback(ext::point{ x, y });
            }
        }
        else if (-dy >= std::abs(dx)) {
            for (int32_t y = from.y - 1; y >= to.y; --y) {
                int32_t x = ext::div_round(dx * (from.y - y), -dy) + from.x;
                callback(ext::point{ x, y });
            }
        }
    }

    template <typename T, typename = void>
    struct CanvasStateVariantElement;

    template <typename T>
    struct CanvasStateVariantElement<T, std::enable_if_t<std::is_base_of_v<Element, T>>>{
        using type = T;
    };

    template <typename T>
    struct CanvasStateVariantElement<T, std::enable_if_t<std::is_same_v<Eraser, T>>> {
        using type = std::monostate;
    };

    template <typename T>
    using CanvasStateVariantElement_t = typename CanvasStateVariantElement<T>::type;
}

template <typename PencilType>
class PencilAction final : public SaveableAction {

private:

    // previous mouse position in canvas units (used to do interpolation for dragging)
    ext::point mousePos;
// storage for the things drawn by the current action, and the transformation relative to defaultState
    ext::expandable_bool_matrix actionState;
    ext::point actionTrans;

    /**
     * Use a drawing tool on (x, y) (in defaultState coordinates)
     */
    void changePixelState(const ext::point& pt, bool newValue) {
        ext::point translation = actionState.changePixelState(pt - actionTrans, newValue).second;
        actionTrans -= translation;
    }


public:

    PencilAction(MainWindow& mainWindow) :SaveableAction(mainWindow), actionTrans({ 0, 0 }) {}


    ~PencilAction() override {
        // commit the state
        CanvasState& outputState = this->canvas();
        this->deltaTrans = outputState.extend(actionTrans, actionTrans + actionState.size()); // this is okay because actionState is guaranteed to be non-empty

        // whether we actually changed anything
        bool hasChanges = false;

        // check if the PencilAction was a single click instead of a drag
        // clicking on a gate/relay with the same tool draws a signal instead
        if (actionState.width() == 1 && actionState.height() == 1) {
            auto& element = outputState[actionTrans + this->deltaTrans];
            if (!std::holds_alternative<CanvasStateVariantElement_t<PencilType>>(element)) {
                element = CanvasStateVariantElement_t<PencilType>{};
                hasChanges = true;
            } else if constexpr (std::is_same_v<PencilType, AndGate> || std::is_same_v<PencilType, OrGate> || std::is_same_v<PencilType, NandGate> || std::is_same_v<PencilType, NorGate> || std::is_same_v<PencilType, PositiveRelay> || std::is_same_v<PencilType, NegativeRelay>) {
                if (std::holds_alternative<CanvasStateVariantElement_t<PencilType>>(element)) {
                    element = Signal{};
                    hasChanges = true;
                }
            }
        } else {
            // note: eraser can be optimized to not extend the canvas first
            for (int32_t y = 0; y < actionState.height(); ++y) {
                for (int32_t x = 0; x < actionState.width(); ++x) {
                    ext::point pt{ x, y };
                    auto& element = outputState[actionTrans + this->deltaTrans + pt];
                    if (actionState[pt] && !std::holds_alternative<CanvasStateVariantElement_t<PencilType>>(element)) {
                        element = CanvasStateVariantElement_t<PencilType>{};
                        hasChanges = true;
                    }
                }
            }
        }

        if (std::is_same_v<PencilType, Eraser>) {
            this->deltaTrans += outputState.shrinkDataMatrix();
        }

        // set changed flag
        this->changed() = hasChanges;
    }

    static inline ActionEventResult startWithPlayAreaMouseButtonDown(const SDL_MouseButtonEvent& event, MainWindow& mainWindow, PlayArea& playArea, const ActionStarter& starter) {
        size_t inputHandleIndex = resolveInputHandleIndex(event);
        size_t currentToolIndex = mainWindow.selectedToolIndices[inputHandleIndex];

        return tool_tags_t::get(currentToolIndex, [&](const auto tool_tag) {
            // 'Tool' is the type of tool (e.g. Selector)
            using Tool = typename decltype(tool_tag)::type;

            if constexpr (std::is_same_v<PencilType, Tool>) {
                // create the new action
                auto& action = starter.start<PencilAction>(mainWindow);

                // draw the element at the current location
                ext::point canvasOffset = playArea.canvasFromWindowOffset(event);
                action.changePixelState(canvasOffset, true);
                action.mousePos = canvasOffset;
                return ActionEventResult::PROCESSED;
            }
            else {
                return ActionEventResult::UNPROCESSED;
            }
        }, ActionEventResult::UNPROCESSED);
    }

    ActionEventResult processPlayAreaMouseDrag(const SDL_MouseMotionEvent& event) {
        // note: probably can be optimized to don't keep doing expansion/contraction checking when interpolating, but this is probably not going to be noticeably slow
        ext::point canvasOffset = playArea().canvasFromWindowOffset(event);
        // interpolate by drawing a straight line from the previous point to the current point
        interpolate(mousePos, canvasOffset, [&](const ext::point& pt) {
            if (ext::point_in_rect(event, this->playArea().renderArea)) {
                // the if-statement here ensures that the pencil does not draw outside the visible part of the canvas
                changePixelState(pt, true);
            }
        });
        mousePos = canvasOffset;
        // we claim to have processed the event, even if the mouse was outside the visible area
        // because if the user drags the mouse back into the visible area, we want to continue drawing
        return ActionEventResult::PROCESSED;
    }

    ActionEventResult processPlayAreaMouseButtonUp() {
        // we are done with this action, so tell the playarea to destroy this action (destruction will automatically commit)
        return ActionEventResult::COMPLETED;
    }


    // rendering function, render the elements that are being drawn in this action
    void renderPlayAreaSurface(uint32_t* pixelBuffer, const SDL_Rect& renderRect) const override {
        for (int32_t y = 0; y != actionState.height(); ++y) {
            for (int32_t x = 0; x != actionState.width(); ++x) {
                ext::point actionPt{ x, y };
                ext::point canvasPt = actionPt + actionTrans;
                if (actionState[actionPt] && point_in_rect(canvasPt, renderRect)) {
                    constexpr SDL_Color color = PencilType::displayColor;
                    pixelBuffer[(canvasPt.y - renderRect.y) * renderRect.w + (canvasPt.x - renderRect.x)] = color.r | (color.g << 8) | (color.b << 16);
                }
            }
        }
    }

};
