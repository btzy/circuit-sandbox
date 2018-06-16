#pragma once

#include "point.hpp"
#include "canvasaction.hpp"
#include "playarea.hpp"
#include "mainwindow.hpp"


/**
 * Action that represents a pencil that can draw an element (or eraser).
 */

class PencilAction : public CanvasAction<PencilAction> {

private:
    // index of this tool on the toolbox
    size_t toolIndex;

    /**
     * Use a drawing tool on (x, y)
     */
    template <typename Tool>
    void processDrawingTool(extensions::point pt) {
        extensions::point deltaTrans;

        // if it is a Pencil, forward the drawing to the gamestate
        if constexpr (std::is_base_of_v<Eraser, Tool>) {
            deltaTrans = this->playArea.stateManager.changePixelState<std::monostate>(pt.x, pt.y); // special handling for the eraser
        }
        else {
            deltaTrans = this->playArea.stateManager.changePixelState<Tool>(pt.x, pt.y); // forwarding for the normal elements
        }

        this->playArea.translation -= deltaTrans * this->playArea.scale;
    }


public:

    PencilAction(PlayArea& playArea, size_t toolIndex) :CanvasAction<PencilAction>(playArea), toolIndex(toolIndex) {}


    ~PencilAction() override {
        // nothing to do to clean up
    }

    static inline bool startWithMouseButtonDown(const SDL_MouseButtonEvent& event, PlayArea& playArea, std::unique_ptr<BaseAction>& actionPtr) {
        size_t inputHandleIndex = resolveInputHandleIndex(event);
        size_t currentToolIndex = playArea.mainWindow.selectedToolIndices[inputHandleIndex];

        return tool_tags_t::get(currentToolIndex, [&event, &playArea, &actionPtr, &currentToolIndex](const auto tool_tag) {
            // 'Tool' is the type of tool (e.g. Selector)
            using Tool = typename decltype(tool_tag)::type;

            if constexpr (std::is_base_of_v<Pencil, Tool>) {
                // create the new action
                actionPtr = std::make_unique<PencilAction>(playArea, currentToolIndex);
                auto& action = static_cast<PencilAction&>(*actionPtr);

                // draw the element at the current location
                extensions::point physicalOffset = extensions::point{ event.x, event.y } -extensions::point{ playArea.renderArea.x, playArea.renderArea.y };
                extensions::point canvasOffset = playArea.computeCanvasCoords(physicalOffset);
                action.processDrawingTool<Tool>(canvasOffset);
                return true;
            }

            return false;
        }, false);
    }

    ActionEventResult processCanvasMouseDrag(const extensions::point& canvasOffset, const SDL_MouseMotionEvent& event) {
        return tool_tags_t::get(toolIndex, [this, &event, &canvasOffset](const auto tool_tag) {
            // 'Tool' is the type of tool (e.g. Selector)
            using Tool = typename decltype(tool_tag)::type;

            if constexpr (std::is_base_of_v<Pencil, Tool>) {
                SDL_Point position{ event.x, event.y };
                if (SDL_PointInRect(&position, &playArea.renderArea)) {
                    // the if-statement here ensures that the pencil does not draw outside the visible part of the canvas
                    processDrawingTool<Tool>(canvasOffset);
                }
                // we claim to have processed the event, event if the mouse was outside the visible area
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
        size_t currentToolIndex = this->playArea.mainWindow.selectedToolIndices[inputHandleIndex];

        // If another drawing tool is pressed, exit this action
        if (currentToolIndex != toolIndex) {
            return ActionEventResult::UNPROCESSED;
        }
        else {
            // we are done with this action, so tell the playarea to destroy this action
            return ActionEventResult::COMPLETED;
        }
    }

};
