#pragma once

#include "baseaction.hpp"

/**
 * Action that represents a pencil that can draw an element (or eraser).
 */

template <typename PlayArea>
class PencilAction : public CanvasAction<PencilAction<PlayArea>, PlayArea> {

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
            deltaTrans = playArea.stateManager.changePixelState<std::monostate>(pt.x, pt.y); // special handling for the eraser
        }
        else {
            deltaTrans = playArea.stateManager.changePixelState<Tool>(pt.x, pt.y); // forwarding for the normal elements
        }

        playArea.translation -= deltaTrans * playArea.scale;
    }


public:

    PencilAction(PlayArea& playArea, size_t toolIndex) :CanvasAction<PencilAction<PlayArea>, PlayArea>(playArea), toolIndex(toolIndex) {}


    ~PencilAction() {
        // nothing to do to clean up
    }

    template <typename ActionVariant>
    static inline bool startWithMouseButtonEvent(const SDL_MouseButtonEvent& event, PlayArea& playArea, ActionVariant& output) {
        size_t inputHandleIndex = MainWindow::resolveInputHandleIndex(event);
        size_t currentToolIndex = playArea.mainWindow.selectedToolIndices[inputHandleIndex];

        return MainWindow::tool_tags_t::get(currentToolIndex, [&event, &playArea, &output, &currentToolIndex](const auto tool_tag) {
            // 'Tool' is the type of tool (e.g. Selector)
            using Tool = typename decltype(tool_tag)::type;

            if constexpr (std::is_base_of_v<Pencil, Tool>) {
                if (event.type == SDL_MOUSEBUTTONDOWN) {
                    // create the new action
                    auto& action = output.emplace<PencilAction<PlayArea>>(playArea, currentToolIndex);

                    // draw the element at the current location
                    extensions::point physicalOffset = extensions::point{ event.x, event.y } -extensions::point{ playArea.renderArea.x, playArea.renderArea.y };
                    extensions::point canvasOffset = playArea.computeCanvasCoords(physicalOffset);
                    action.processDrawingTool<Tool>(canvasOffset);
                    return true;
                }
            }

            return false;
        });
    }

    ActionEventResult processCanvasMouseMotionEvent(const extensions::point& canvasOffset, const SDL_MouseMotionEvent& event) {
        return MainWindow::tool_tags_t::get(toolIndex, [this, &canvasOffset](const auto tool_tag) {
            // 'Tool' is the type of tool (e.g. Selector)
            using Tool = typename decltype(tool_tag)::type;

            if constexpr (std::is_base_of_v<Pencil, Tool>) {
                processDrawingTool<Tool>(canvasOffset);
                return ActionEventResult::PROCESSED;
            }
            else {
                // this will never happen
                return ActionEventResult::UNPROCESSED;
            }
        });
    }

    ActionEventResult processCanvasMouseButtonEvent(const extensions::point& canvasOffset, const SDL_MouseButtonEvent& event) {
        size_t inputHandleIndex = MainWindow::resolveInputHandleIndex(event);
        size_t currentToolIndex = playArea.mainWindow.selectedToolIndices[inputHandleIndex];

        // If another drawing tool is pressed, exit this action
        if (currentToolIndex != toolIndex) {
            return ActionEventResult::UNPROCESSED;
        }

        MainWindow::tool_tags_t::get(toolIndex, [this, &event](const auto tool_tag) {
            // 'Tool' is the type of tool (e.g. Selector)
            using Tool = typename decltype(tool_tag)::type;

            if constexpr (std::is_base_of_v<Pencil, Tool>) {
                if (event.type == SDL_MOUSEBUTTONUP) {
                    // we are done with this action, so tell the playarea to destroy this action
                    return ActionEventResult::COMPLETED;
                }
                else {
                    // weird state we are in!
                    return ActionEventResult::UNPROCESSED;
                }
            }
            else {
                // this will never happen
                return ActionEventResult::UNPROCESSED;
            }
        });
    }

};