#pragma once

#include <cmath>
#include <type_traits>
#include <variant>
#include <vector>
#include <numeric>

#include <SDL.h>
#include "point.hpp"
#include "reduce.hpp"
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
    // calls callback(pt) for every point between `from` and `to`, excluding `from` and excluding `to`.
    // @pre: (from.x == to.x || from.y == to.y)
    template <typename Callback>
    inline void interpolate_orthogonal(const ext::point& from, const ext::point& to, Callback&& callback) {
        int32_t dx = to.x - from.x;
        int32_t dy = to.y - from.y;
        if (dx != 0) {
            if (dx > 0) {
                for (int32_t x = from.x + 1; x < to.x; ++x) {
                    callback(ext::point{ x, from.y });
                }
            }
            else {
                for (int32_t x = from.x - 1; x > to.x; --x) {
                    callback(ext::point{ x, from.y });
                }
            }
        }
        else if (dy != 0) {
            if (dy > 0) {
                for (int32_t y = from.y + 1; y < to.y; ++y) {
                    callback(ext::point{ from.x, y });
                }
            }
            else {
                for (int32_t y = from.y - 1; y > to.y; --y) {
                    callback(ext::point{ from.x, y });
                }
            }
        }
    }

    // Finds the closest point to mousePt that is either horizontal or vertical from targetPt.
    inline ext::point find_orthogonal_keypoint(const ext::point& mousePt, const ext::point& targetPt) {
        int32_t dx = std::abs(targetPt.x - mousePt.x);
        int32_t dy = std::abs(targetPt.y - mousePt.y);
        if (dx == 0 && dy == 0) {
            return targetPt;
        }
        else if (dx >= dy) {
            return { mousePt.x, targetPt.y };
        }
        else {
            return { targetPt.x, mousePt.y };
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
class PencilAction final : public SaveableAction, public KeyboardEventHook {

private:

    // previous mousedown position in canvas units
    ext::point mousePos;
    // storage for the key points in the current pencil action
    std::vector<ext::point> keyPoints;

    bool polylineMode;

    template <typename NewPencilType = PencilType, typename ElementVariant>
    bool change(ElementVariant& element) {
        if (!std::holds_alternative<CanvasStateVariantElement_t<NewPencilType>>(element)) {
            element = CanvasStateVariantElement_t<NewPencilType>{};
            return true;
        }
        return false;
    }

public:

    PencilAction(MainWindow& mainWindow, bool polylineMode) :SaveableAction(mainWindow), KeyboardEventHook(mainWindow), polylineMode(polylineMode) {}


    ~PencilAction() override {
        // commit the state
        CanvasState& outputState = this->canvas();
        ext::point minPt = ext::reduce(keyPoints.begin(), keyPoints.end(), ext::point::max(), [](const ext::point& pt1, const ext::point& pt2) {
            return min(pt1, pt2);
        });
        ext::point maxPt = ext::reduce(keyPoints.begin(), keyPoints.end(), ext::point::min(), [](const ext::point& pt1, const ext::point& pt2) {
            return max(pt1, pt2);
        }) + ext::point(1, 1); // extra (1, 1) for past-the-end required by outputState.extend()
        this->deltaTrans = outputState.extend(minPt, maxPt); // this is okay because actionState is guaranteed to be non-empty

        // whether we actually changed anything
        bool hasChanges = false;

        // check if the PencilAction was a single click instead of a drag
        // clicking on a gate/relay with the same tool draws a signal instead
        if (keyPoints.size() == 1) {
            auto& element = outputState[this->deltaTrans + keyPoints.front()];
            hasChanges = change(element);
            if constexpr (std::is_base_of_v<LogicGate, PencilType> || std::is_base_of_v<Relay, PencilType>) {
                if (!hasChanges) {
                    element = Signal{};
                    hasChanges = true;
                }
            }
        }
        else {
            // note: eraser can be optimized to not extend the canvas first
            auto prev = keyPoints.begin();
            hasChanges = change(outputState[this->deltaTrans + *prev]);
            auto curr = prev;
            // when drawing insulated wires, draw conductive wire at intermediate keypoints
            if constexpr (std::is_same_v<InsulatedWire, PencilType>) {
                for (++curr; curr != keyPoints.end(); prev = curr++) {
                    interpolate_orthogonal(*prev, *curr, [&](const ext::point& pt) {
                        hasChanges = change(outputState[this->deltaTrans + pt]) || hasChanges;
                    });
                }
                curr = prev = keyPoints.begin();
                for (++curr; curr + 1 != keyPoints.end(); prev = curr++) {
                    hasChanges = change<ConductiveWire>(outputState[this->deltaTrans + *curr]) || hasChanges;
                }
                hasChanges = change(outputState[this->deltaTrans + *curr]) || hasChanges;
            }
            else {
                for (++curr; curr != keyPoints.end(); prev = curr++) {
                    interpolate_orthogonal(*prev, *curr, [&](const ext::point& pt) {
                        hasChanges = change(outputState[this->deltaTrans + pt]) || hasChanges;
                    });
                    hasChanges = change(outputState[this->deltaTrans + *curr]) || hasChanges;
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
                bool polylineMode = SDL_GetModState() & KMOD_SHIFT;
                // create the new action
                auto& action = starter.start<PencilAction>(mainWindow, polylineMode);

                // draw the element at the current location
                action.mousePos = playArea.canvasFromWindowOffset(event);
                action.keyPoints.emplace_back(action.mousePos);
                return ActionEventResult::PROCESSED;
            }
            else {
                return ActionEventResult::UNPROCESSED;
            }
        }, ActionEventResult::UNPROCESSED);
    }

    ActionEventResult processPlayAreaMouseButtonDown(const SDL_MouseButtonEvent& event) override {

        size_t inputHandleIndex = resolveInputHandleIndex(event);
        size_t currentToolIndex = mainWindow.selectedToolIndices[inputHandleIndex];

        if (!tool_tags_t::get(currentToolIndex, [&](const auto tool_tag) {
            // 'Tool' is the type of tool (e.g. Selector)
            using Tool = typename decltype(tool_tag)::type;

            if constexpr (std::is_same_v<PencilType, Tool>) {
                return true;
            }
            else {
                return false;
            }
        }, false)) {
            // some other mouse button got pressed, so tell the playarea to destroy this action (destruction will automatically commit)
            return ActionEventResult::COMPLETED;
        }

        mousePos = playArea().canvasFromWindowOffset(event);

        // add new keypoint, if it isn't the same as the last point
        ext::point keyPoint = find_orthogonal_keypoint(mousePos, keyPoints.back());
        if (keyPoints.back() != keyPoint) {
            keyPoints.emplace_back(keyPoint);
        }

        SDL_Keymod modifiers = SDL_GetModState();
        if (modifiers & KMOD_SHIFT) {
            // user pressed SHIFT, so don't end action yet
            return ActionEventResult::PROCESSED;
        }
        else {
            // we are done with this action, so tell the playarea to destroy this action (destruction will automatically commit)
            return ActionEventResult::COMPLETED;
        }
    }

    ActionEventResult processPlayAreaMouseButtonUp() override {
        // if not drawing a polyline, mouseup creates the second keypoint and ends the action
        if (!polylineMode) {
            ext::point keyPoint = find_orthogonal_keypoint(mousePos, keyPoints.back());
            if (keyPoints.back() != keyPoint) {
                keyPoints.emplace_back(keyPoint);
            }
            return ActionEventResult::COMPLETED;
        }
        return ActionEventResult::PROCESSED;
    }

    ActionEventResult processPlayAreaMouseHover(const SDL_MouseMotionEvent& event) override {
        // save the mouse position
        mousePos = playArea().canvasFromWindowOffset(event);
        return ActionEventResult::PROCESSED;
    }


    ActionEventResult processWindowKeyboard(const SDL_KeyboardEvent& event) override {
        if (event.type == SDL_KEYDOWN) {
            SDL_Keymod modifiers = static_cast<SDL_Keymod>(event.keysym.mod);
            if (!(modifiers & KMOD_CTRL)) {
                switch (event.keysym.scancode) { // using the scancode layout so that keys will be in the same position if the user has a non-qwerty keyboard
                case SDL_SCANCODE_BACKSPACE: [[fallthrough]];
                case SDL_SCANCODE_KP_BACKSPACE: // Remove last point
                    if (keyPoints.size() > 1) {
                        keyPoints.pop_back();
                    }
                    return ActionEventResult::PROCESSED;
                case SDL_SCANCODE_ESCAPE:
                    return ActionEventResult::COMPLETED;
                default:
                    break;
                }
            }
        }
        return ActionEventResult::UNPROCESSED;
    }


    // rendering function, render the elements that are being drawn in this action
    void renderPlayAreaSurface(uint32_t* pixelBuffer, uint32_t pixelFormat, const SDL_Rect& renderRect, int32_t pitch) const override {
        invoke_RGB_format(pixelFormat, [&](const auto format) {
            using FormatType = decltype(format);
            auto prev = keyPoints.begin();
            uint32_t displayColor = fast_MapRGB<FormatType::value>(PencilType::displayColor);
            if (point_in_rect(*prev, renderRect)) {
                pixelBuffer[(prev->y - renderRect.y) * pitch + (prev->x - renderRect.x)] = displayColor;
            }
            auto curr = prev;
            for (++curr; curr != keyPoints.end(); prev = curr++) {
                interpolate_orthogonal(*prev, *curr, [&](const ext::point& pt) {
                    if (point_in_rect(pt, renderRect)) {
                        pixelBuffer[(pt.y - renderRect.y) * pitch + (pt.x - renderRect.x)] = displayColor;
                    }
                });
                if (point_in_rect(*curr, renderRect)) {
                    pixelBuffer[(curr->y - renderRect.y) * pitch + (curr->x - renderRect.x)] = displayColor;
                }
            }
        });
    }

    void renderPlayAreaDirect(SDL_Renderer* renderer) const override {
        ext::point newPos = find_orthogonal_keypoint(mousePos, keyPoints.back());

        ext::point topLeft = ext::min(keyPoints.back(), newPos);
        ext::point bottomRight = ext::max(keyPoints.back(), newPos) + ext::point{ 1, 1 };

        ext::point windowTopLeft = playArea().windowFromCanvasOffset(topLeft);
        ext::point windowBottomRight = playArea().windowFromCanvasOffset(bottomRight);
        
        SDL_Rect selectionArea{
            windowTopLeft.x,
            windowTopLeft.y,
            windowBottomRight.x - windowTopLeft.x,
            windowBottomRight.y - windowTopLeft.y
        };

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0xFF - (0xFF - PencilType::displayColor.r) / 2, 0xFF - (0xFF - PencilType::displayColor.g) / 2, 0xFF - (0xFF - PencilType::displayColor.b) / 2, 0x66);
        SDL_RenderFillRect(renderer, &selectionArea);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }
};
