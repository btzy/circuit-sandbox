#pragma once

#include <cmath>

#include <SDL.h>
#include "point.hpp"
#include "canvasaction.hpp"
#include "statemanager.hpp"
#include "playarea.hpp"
#include "mainwindow.hpp"
#include "canvasstate.hpp"
#include "expandable_matrix.hpp"


/**
 * Action that represents a pencil that can draw an element (or eraser).
 */

// anonymous namespace to put helper functions that won't leak out to other translation units
namespace {
    // calls callback(pt) for every point between `from` and `to`, excluding `from` but including `to`.
    template <typename Callback>
    void interpolate(const extensions::point& from, const extensions::point& to, Callback&& callback) {
        int32_t dx = to.x - from.x;
        int32_t dy = to.y - from.y;
        if (dx == 0 && dy == 0) {
            return;
        }
        else if (dx >= std::abs(dy)) {
            for (int32_t x = from.x + 1; x <= to.x; ++x) {
                int32_t y = extensions::div_round(dy * (x - from.x), dx) + from.y;
                callback(extensions::point{ x, y });
            }
        }
        else if (-dx >= std::abs(dy)) {
            for (int32_t x = from.x - 1; x >= to.x; --x) {
                int32_t y = extensions::div_round(dy * (from.x - x), -dx) + from.y;
                callback(extensions::point{ x, y });
            }
        }
        else if (dy >= std::abs(dx)) {
            for (int32_t y = from.y + 1; y <= to.y; ++y) {
                int32_t x = extensions::div_round(dx * (y - from.y), dy) + from.x;
                callback(extensions::point{ x, y });
            }
        }
        else if (-dy >= std::abs(dx)) {
            for (int32_t y = from.y - 1; y >= to.y; --y) {
                int32_t x = extensions::div_round(dx * (from.y - y), -dy) + from.x;
                callback(extensions::point{ x, y });
            }
        }
    }
}

class PencilAction : public CanvasAction<PencilAction> {

private:
    // index of this tool on the toolbox
    size_t toolIndex;

    // previous mouse position in canvas units (used to do interpolation for dragging)
    extensions::point mousePos;
// storage for the things drawn by the current action, and the transformation relative to defaultState
    extensions::expandable_bool_matrix actionState;
    extensions::point actionTrans;

    /**
     * Use a drawing tool on (x, y) (in defaultState coordinates)
     */
    void changePixelState(const extensions::point& pt, bool newValue) {
        actionTrans -= actionState.changePixelState(pt - actionTrans, newValue).second;
    }


public:

    PencilAction(PlayArea& playArea, size_t toolIndex) :CanvasAction<PencilAction>(playArea), toolIndex(toolIndex), actionTrans({ 0, 0 }) {}


    ~PencilAction() override {
        // commit the state
        tool_tags_t::get(toolIndex, [&](const auto tool_tag) {
            using Tool = typename decltype(tool_tag)::type;

            CanvasState& outputState = canvas();
            deltaTrans = outputState.extend(extensions::min({ 0, 0 }, actionTrans), extensions::max(outputState.size(), actionTrans + actionState.size()));
            
            for (int32_t y = 0; y < actionState.height(); ++y) {
                for (int32_t x = 0; x < actionState.width(); ++x) {
                    extensions::point pt{ x, y };
                    if (actionState[pt]) {
                        if constexpr(std::is_same_v<Tool, Eraser>) {
                            outputState[actionTrans + deltaTrans + pt] = std::monostate{};
                        }
                        else if constexpr(std::is_base_of_v<Element, Tool>) {
                            outputState[actionTrans + deltaTrans + pt] = Tool{};
                        }
                    }
                }
            }

            deltaTrans += outputState.shrinkDataMatrix();
        });
    }

    static inline bool startWithMouseButtonDown(const SDL_MouseButtonEvent& event, PlayArea& playArea, const ActionStarter& starter) {
        size_t inputHandleIndex = resolveInputHandleIndex(event);
        size_t currentToolIndex = playArea.mainWindow.selectedToolIndices[inputHandleIndex];

        return tool_tags_t::get(currentToolIndex, [&](const auto tool_tag) {
            // 'Tool' is the type of tool (e.g. Selector)
            using Tool = typename decltype(tool_tag)::type;

            if constexpr (std::is_base_of_v<Pencil, Tool>) {
                // create the new action
                auto& action = starter.start<PencilAction>(playArea, currentToolIndex);

                // draw the element at the current location
                extensions::point physicalOffset = extensions::point{ event.x, event.y } -extensions::point{ playArea.renderArea.x, playArea.renderArea.y };
                extensions::point canvasOffset = playArea.computeCanvasCoords(physicalOffset);
                action.changePixelState(canvasOffset, true);
                action.mousePos = canvasOffset; // TODO: processDrawingTool changes the saved offset; should translations only be changed at the end of an action?
                return true;
            }

            return false;
        }, false);
    }

    ActionEventResult processCanvasMouseDrag(const extensions::point& canvasOffset, const SDL_MouseMotionEvent& event) {
        return tool_tags_t::get(toolIndex, [&](const auto tool_tag) {
            // 'Tool' is the type of tool (e.g. Selector)
            using Tool = typename decltype(tool_tag)::type;

            if constexpr (std::is_base_of_v<Pencil, Tool>) {
                // note: probably can be optimized to don't keep doing expansion/contraction checking when interpolating, but this is probably not going to be noticeably slow
                // interpolate by drawing a straight line from the previous point to the current point
                interpolate(mousePos, canvasOffset, [&](const extensions::point& pt) {
                    if (extensions::point_in_rect(event, playArea.renderArea)) {
                        // the if-statement here ensures that the pencil does not draw outside the visible part of the canvas
                        changePixelState(pt, true);
                    }
                });
                mousePos = canvasOffset;
                // we claim to have processed the event, even if the mouse was outside the visible area
                // because if the user drags the mouse back into the visible area, we want to continue drawing
                return ActionEventResult::PROCESSED;
            }
            else {
                // this will never happen
                return ActionEventResult::UNPROCESSED;
            }
        }, ActionEventResult::UNPROCESSED);
    }

    ActionEventResult processMouseButtonUp(const SDL_MouseButtonEvent& event) {
        size_t inputHandleIndex = resolveInputHandleIndex(event);
        size_t currentToolIndex = playArea.mainWindow.selectedToolIndices[inputHandleIndex];

        // If another drawing tool is pressed, exit this action
        if (currentToolIndex != toolIndex) {
            return ActionEventResult::UNPROCESSED;
        }
        else {
            // we are done with this action, so tell the playarea to destroy this action (destruction will automatically commit)
            return ActionEventResult::COMPLETED;
        }
    }


    // rendering function, render the elements that are being drawn in this action
    void renderSurface(uint32_t* pixelBuffer, const SDL_Rect& renderRect) const override {
        tool_tags_t::get(toolIndex, [&](const auto tool_tag) {
            using Tool = typename decltype(tool_tag)::type;
            for (int32_t y = 0; y != actionState.height(); ++y) {
                for (int32_t x = 0; x != actionState.width(); ++x) {
                    extensions::point actionPt{ x, y };
                    extensions::point canvasPt = actionPt + actionTrans;
                    if (actionState[actionPt] && point_in_rect(canvasPt, renderRect)) {
                        constexpr SDL_Color color = Tool::displayColor;
                        pixelBuffer[(canvasPt.y - renderRect.y) * renderRect.w + (canvasPt.x - renderRect.x)] = color.r | (color.g << 8) | (color.b << 16);
                    }
                }
            }
        });
    }

};
