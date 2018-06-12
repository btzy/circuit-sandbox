#pragma once

/**
* Represents the state of the saveable game data.
* All the game state that needs to be saved to disk should reside in this class.
* Exposes functions for resizing and editing the canvas
*/

#include <utility> // for std::pair
#include <variant> // for std::variant
#include <type_traits>
#include <algorithm> // for std::min and std::max
#include <cstdint> // for int32_t and uint32_t
#include <limits>

#include "heap_matrix.hpp"
#include "elements.hpp"
#include "point.hpp"
#include "tag_tuple.hpp"

class CanvasState {
private:

    // the possible elements that a pixel can represent
    // std::monostate is a 'default' state, which represents an empty pixel
    using element_tags_t = extensions::tag_tuple<std::monostate, ConductiveWire, InsulatedWire, Signal, Source, PositiveRelay, NegativeRelay, AndGate, OrGate, NandGate, NorGate>;

    using element_variant_t = element_tags_t::instantiate<std::variant>;

    using matrix_t = extensions::heap_matrix<element_variant_t>;

    matrix_t dataMatrix;

    friend class StateManager;
    friend class Simulator;

    /**
    * Modifies the dataMatrix so that the x and y will be within the matrix.
    * Returns the translation that should be applied on {x,y}.
    */
    extensions::point prepareDataMatrixForAddition(int32_t x, int32_t y) {
        if (dataMatrix.empty()) {
            // special case for empty matrix
            dataMatrix = matrix_t(1, 1);
            return { -x, -y };
        }

        if (0 <= x && x < dataMatrix.width() && 0 <= y && y < dataMatrix.height()) { // check if inside the matrix
            return { 0, 0 }; // no preparation or translation needed
        }

        int32_t x_min = std::min(x, 0);
        int32_t x_max = std::max(x + 1, dataMatrix.width());
        int32_t x_translation = -x_min;
        int32_t new_width = x_max + x_translation;

        int32_t y_min = std::min(y, 0);
        int32_t y_max = std::max(y + 1, dataMatrix.height());
        int32_t y_translation = -y_min;
        int32_t new_height = y_max + y_translation;

        matrix_t new_matrix(new_width, new_height);
        extensions::move_range(dataMatrix, new_matrix, 0, 0, x_translation, y_translation, dataMatrix.width(), dataMatrix.height());

        dataMatrix = std::move(new_matrix);

        return { x_translation, y_translation };
    }

    /**
    * Shrinks the dataMatrix to the minimal bounding rectangle.
    * As an optimization, will only shrink if {x,y} is along a border.
    * Returns the translation that should be applied on {x,y}.
    */
    extensions::point shrinkDataMatrix(int32_t x, int32_t y) {

        if (x > 0 && x + 1 < dataMatrix.width() && y > 0 && y + 1 < dataMatrix.height()) { // check if not on the border
            return { 0, 0 }; // no preparation or translation needed
        }

        return shrinkDataMatrix();
    }

    /**
     * Shrink with no optimization.
     */
    extensions::point shrinkDataMatrix() {

        // we simply iterate the whole matrix to get the min and max values
        int32_t x_min = std::numeric_limits<int32_t>::max();
        int32_t x_max = std::numeric_limits<int32_t>::min();
        int32_t y_min = std::numeric_limits<int32_t>::max();
        int32_t y_max = std::numeric_limits<int32_t>::min();

        // iterate height before width for cache-friendliness
        for (int32_t i = 0; i != dataMatrix.height(); ++i) {
            for (int32_t j = 0; j != dataMatrix.width(); ++j) {
                if (!std::holds_alternative<std::monostate>(dataMatrix[{j, i}])) {
                    x_min = std::min(x_min, j);
                    x_max = std::max(x_max, j);
                    y_min = std::min(y_min, i);
                    y_max = std::max(y_max, i);
                }
            }
        }

        if (x_min == 0 && x_max + 1 == dataMatrix.width() && y_min == 0 && y_max + 1 == dataMatrix.height()) {
            // no resizing needed
            return { 0, 0 };
        }

        if (x_min > x_max) {
            // if this happens, it means the game state is totally empty
            dataMatrix = matrix_t();

            // if the matrix is totally empty, returning a translation makes no sense
            // so we just return {0,0}
            return { 0, 0 };
        }

        int32_t new_width = x_max + 1 - x_min;
        int32_t new_height = y_max + 1 - y_min;

        matrix_t new_matrix(new_width, new_height);
        extensions::move_range(dataMatrix, new_matrix, x_min, y_min, 0, 0, new_width, new_height);

        dataMatrix = std::move(new_matrix);

        return { -x_min, -y_min };
    }

public:

    

    /**
    * Change the state of a pixel.
    * 'Element' should be one of the elements in 'element_variant_t', or std::monostate for the eraser
    * Returns a pair describing whether the canvas was actually modified, and the net translation change that the viewport should apply (such that the viewport will be at the 'same' position)
    */
    template <typename Element>
    std::pair<bool, extensions::point> changePixelState(int32_t x, int32_t y) {
        
        // note that this function performs linearly to the size of the matrix.  But since this is limited by how fast the user can click, it should be good enough

        if constexpr (std::is_same_v<std::monostate, Element>) {
            if (x >= 0 && x < dataMatrix.width() && y >= 0 && y < dataMatrix.height()
                    && !std::holds_alternative<Element>(dataMatrix[{x, y}])) {
                dataMatrix[{x, y}] = Element{};

                // Element is std::monostate, so we have to see if we can shrink the matrix size
                // Inform the caller that the canvas was changed, and the translation required
                return { true, shrinkDataMatrix(x, y) };

            }
            else {
                return { false, { 0, 0 } };
            }
        }
        else {
            // Element is not std::monostate, so we might need to expand the matrix size first
            extensions::point translation = prepareDataMatrixForAddition(x, y);
            x += translation.x;
            y += translation.y;

            if (!std::holds_alternative<Element>(dataMatrix[{x, y}])) {
                dataMatrix[{x, y}] = Element{};
                return { true, translation };
            }
            else {

                // note: if we reach here, `translation` is guaranteed to be {0,0} as well

                return { false, { 0, 0 } };
            }
        }
    }


    /**
     * Moves elements from an area of dataMatrix to a new CanvasState and returns them.
     * The area becomes empty on the current dataMatrix.
     * Return matrix will be of the exact width and height; it will not be shrinked.
     * @pre Assumes that the parameters are within the bounds of this canvas state
     */
    CanvasState splice(int32_t x, int32_t y, int32_t width, int32_t height) {
        CanvasState newState;
        newState.dataMatrix = matrix_t(width, height);
        extensions::swap_range(dataMatrix, newState.dataMatrix, x, y, 0, 0, width, height);
        return newState;
    }


    /**
     * Merges a CanvasState with another, potentially modifying both of them.
     * Returns a matrix (not shrinked) and the translation rerquired.
     * @pre Assumes that the parameters are within the bounds of this canvas state
     */
    static std::pair<CanvasState, extensions::point> merge(CanvasState&& first, const extensions::point& firstTrans, CanvasState&& second, const extensions::point& secondTrans) {
        int32_t new_xmin = std::min(secondTrans.x, firstTrans.x);
        int32_t new_ymin = std::min(secondTrans.y, firstTrans.y);
        int32_t new_xmax = std::max(secondTrans.x + second.width(), firstTrans.x + first.width());
        int32_t new_ymax = std::max(secondTrans.y + second.height(), firstTrans.y + first.height());

        // TODO: can optimize if the base totally contains the selection.  Then don't need to construct a new matrix.

        CanvasState newState;
        newState.dataMatrix = matrix_t(new_xmax - new_xmin, new_ymax - new_ymin);

        // move base to dataMatrix
        extensions::move_range(first.dataMatrix, newState.dataMatrix, 0, 0, firstTrans.x - new_xmin, firstTrans.y - new_ymin, first.width(), first.height());


        // move non-monostate elements from selection to dataMatrix
        for (int32_t y = 0; y < second.height(); ++y) {
            for (int32_t x = 0; x < second.width(); ++x) {
                if (!std::holds_alternative<std::monostate>(second.dataMatrix[{x, y}])) {
                    newState.dataMatrix[{x + secondTrans.x - new_xmin, y + secondTrans.y - new_ymin}] = std::move(second.dataMatrix[{x, y}]);
                }
            }
        }

        return { std::move(newState), {-new_xmin, -new_ymin} };
    }


    /**
     * returns true if the matrix is empty (i.e. has no width and height)
     */
    bool empty() const noexcept {
        return dataMatrix.empty();
    }

    /**
     * returns the width of the matrix
     */
    int32_t width() const noexcept {
        return dataMatrix.width();
    }

    /**
     * returns the height of the matrix
     */
    int32_t height() const noexcept {
        return dataMatrix.height();
    }
};
