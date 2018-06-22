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
    static_assert(element_tags_t::size <= (static_cast<size_t>(1) << (std::numeric_limits<uint8_t>::digits - 2)), "Number of elements cannot exceed number of available bits in file format.");

    using element_variant_t = element_tags_t::instantiate<std::variant>;

    using matrix_t = extensions::heap_matrix<element_variant_t>;

    matrix_t dataMatrix;

    friend class StateManager;
    friend class Simulator;
    friend class FileOpenAction;
    friend class FileSaveAction;

    /**
     * Modifies the dataMatrix so that the x and y will be within the matrix.
     * Returns the translation that should be applied on {x,y}.
     */
    extensions::point prepareDataMatrixForAddition(const extensions::point& pt) {
        if (dataMatrix.empty()) {
            // special case for empty matrix
            dataMatrix = matrix_t(1, 1);
            return -pt;
        }

        if (dataMatrix.contains(pt)) { // check if inside the matrix
            return { 0, 0 }; // no preparation or translation needed
        }

        extensions::point min_pt = extensions::min(pt, { 0, 0 });
        extensions::point max_pt = extensions::min(pt + extensions::point{ 1, 1 }, dataMatrix.size());
        extensions::point translation = -min_pt;
        extensions::point new_size = max_pt + translation;

        matrix_t new_matrix(new_size);
        extensions::move_range(dataMatrix, new_matrix, 0, 0, translation.x, translation.y, dataMatrix.width(), dataMatrix.height());

        dataMatrix = std::move(new_matrix);

        return translation;
    }

    /**
     * Shrinks the dataMatrix to the minimal bounding rectangle.
     * As an optimization, will only shrink if {x,y} is along a border.
     * Returns the translation that should be applied on {x,y}.
     */
    extensions::point shrinkDataMatrix(const extensions::point& pt) {

        if (pt.x > 0 && pt.x + 1 < dataMatrix.width() && pt.y > 0 && pt.y + 1 < dataMatrix.height()) { // check if not on the border
            return { 0, 0 }; // no preparation or translation needed
        }

        return shrinkDataMatrix();
    }

public:

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

    /**
     * Change the state of a pixel.
     * 'Element' should be one of the elements in 'element_variant_t', or std::monostate for the eraser
     * Returns a pair describing whether the canvas was actually modified, and the net translation change that the viewport should apply (such that the viewport will be at the 'same' position)
     */
    template <typename Element>
    std::pair<bool, extensions::point> changePixelState(extensions::point pt) {
        // note that this function performs linearly to the size of the matrix.  But since this is limited by how fast the user can click, it should be good enough

        if constexpr (std::is_same_v<std::monostate, Element>) {
            if (dataMatrix.contains(pt) && !std::holds_alternative<Element>(dataMatrix[pt])) {
                dataMatrix[pt] = Element{};

                // Element is std::monostate, so we have to see if we can shrink the matrix size
                // Inform the caller that the canvas was changed, and the translation required
                return { true, shrinkDataMatrix(pt) };

            }
            else {
                return { false, { 0, 0 } };
            }
        }
        else {
            // Element is not std::monostate, so we might need to expand the matrix size first
            extensions::point translation = prepareDataMatrixForAddition(pt);
            pt += translation;

            if (!std::holds_alternative<Element>(dataMatrix[pt])) {
                dataMatrix[pt] = Element{};
                return { true, translation };
            }
            else {
                // note: if we reach here, `translation` is guaranteed to be {0,0} as well
                return { false, { 0, 0 } };
            }
        }
    }

    /**
     * indices is a pair of {x,y}
     * @pre indices must be within the bounds of width and height
     * For a version that does bounds checking and expansion/contraction of the underlying matrix, use changePixelState
     */
    element_variant_t& operator[](const extensions::point& indices) noexcept {
        return dataMatrix[indices];
    }

    /**
     * indices is a pair of {x,y}
     * @pre indices must be within the bounds of width and height
     * For a version that does bounds checking and expansion/contraction of the underlying matrix, use changePixelState
     */
    const element_variant_t& operator[](const extensions::point& indices) const noexcept {
        return dataMatrix[indices];
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

    /**
     * returns the size of the matrix
     */
    extensions::point size() const noexcept {
        return dataMatrix.size();
    }

    /**
    * returns true if the point is within the bounds of the matrix
    */
    bool contains(const extensions::point& pt) const noexcept {
        return dataMatrix.contains(pt);
    }

    /**
     * grow the underlying matrix to a larger size
     */
    extensions::point extend(const extensions::point& topLeft, const extensions::point& bottomRight) {
        extensions::point newTopLeft = empty() ? topLeft : min(topLeft, { 0, 0 });
        extensions::point newBottomRight = empty() ? bottomRight : max(bottomRight, size());
        extensions::point newSize = newBottomRight - newTopLeft;
        matrix_t newMatrix(newSize);
        if(!empty()) extensions::move_range(dataMatrix, newMatrix, 0, 0, -newTopLeft.x, -newTopLeft.y, width(), height());
        dataMatrix = std::move(newMatrix);
        return -newTopLeft;
    }


    /**
     * Moves elements from an area of dataMatrix to a new CanvasState and returns them.
     * The area becomes empty on the current dataMatrix.
     * Returned matrix will be of the exact width and height; it will not be shrinked.
     * @pre Assumes that the parameters are within the bounds of this canvas state
     */
    CanvasState splice(int32_t x, int32_t y, int32_t width, int32_t height) {
        CanvasState newState;
        newState.dataMatrix = matrix_t(width, height);
        extensions::swap_range(dataMatrix, newState.dataMatrix, x, y, 0, 0, width, height);
        return newState;
    }


    void flipHorizontal() {
        dataMatrix.flipHorizontal();
    }

    void flipVertical() {
        dataMatrix.flipVertical();
    }

    void rotateClockwise() {
        matrix_t newDataMatrix = matrix_t(dataMatrix.height(), dataMatrix.width());
        for (int32_t x = 0; x != dataMatrix.width(); ++x) {
            for (int32_t y = 0; y != dataMatrix.height(); ++y) {
                newDataMatrix[{dataMatrix.height()-y-1, x}] = dataMatrix[{x, y}];
            }
        }
        dataMatrix = std::move(newDataMatrix);
    }

    void rotateCounterClockwise() {
        matrix_t newDataMatrix = matrix_t(dataMatrix.height(), dataMatrix.width());
        for (int32_t x = 0; x != dataMatrix.width(); ++x) {
            for (int32_t y = 0; y != dataMatrix.height(); ++y) {
                newDataMatrix[{y, dataMatrix.width()-x-1}] = dataMatrix[{x, y}];
            }
        }
        dataMatrix = std::move(newDataMatrix);
    }

    /**
     * Merges a CanvasState with another, potentially modifying both of them.
     * Elements from the second CanvasState will be written over those from the first in the output.
     * Returns a matrix (not shrinked) and the translation required.
     * @pre Assumes that the parameters are within the bounds of this canvas state
     */
    static std::pair<CanvasState, extensions::point> merge(CanvasState&& first, const extensions::point& firstTrans, CanvasState&& second, const extensions::point& secondTrans) {
        // special cases if we are merging with something empty
        if (first.empty()) return { std::move(second), -secondTrans };
        if (second.empty()) return { std::move(first), -firstTrans };

        int32_t new_xmin = std::min(secondTrans.x, firstTrans.x);
        int32_t new_ymin = std::min(secondTrans.y, firstTrans.y);
        int32_t new_xmax = std::max(secondTrans.x + second.width(), firstTrans.x + first.width());
        int32_t new_ymax = std::max(secondTrans.y + second.height(), firstTrans.y + first.height());

        // TODO: can optimize if the base totally contains the selection.  Then don't need to construct a new matrix.

        CanvasState newState;
        newState.dataMatrix = matrix_t(new_xmax - new_xmin, new_ymax - new_ymin);

        // move first to newstate
        extensions::move_range(first.dataMatrix, newState.dataMatrix, 0, 0, firstTrans.x - new_xmin, firstTrans.y - new_ymin, first.width(), first.height());

        // move non-monostate elements from second to newstate
        for (int32_t y = 0; y < second.height(); ++y) {
            for (int32_t x = 0; x < second.width(); ++x) {
                if (!std::holds_alternative<std::monostate>(second.dataMatrix[{x, y}])) {
                    newState.dataMatrix[{x + secondTrans.x - new_xmin, y + secondTrans.y - new_ymin}] = std::move(second.dataMatrix[{x, y}]);
                }
            }
        }

        return { std::move(newState), {-new_xmin, -new_ymin} };
    }
};
