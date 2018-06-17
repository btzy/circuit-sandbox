#pragma once

/**
 * Encapsulates a heap_matrix that is resizeable
 */

#include <type_traits>
#include <algorithm> // for std::min and std::max
#include <cstdint> // for int32_t and uint32_t
#include <limits>
#include "heap_matrix.hpp"

namespace extensions {

    class expandable_bool_matrix {
    private:

        using matrix_t = extensions::heap_matrix<bool>;

        matrix_t dataMatrix;

        /**
        * Modifies the dataMatrix so that the x and y will be within the matrix.
        * Returns the translation that should be applied on {x,y}.
        */
        extensions::point prepareDataMatrixForAddition(const extensions::point& pt) {
            if (dataMatrix.empty()) {
                // special case for empty matrix
                dataMatrix = matrix_t(1, 1);
                dataMatrix[{0, 0}] = false;
                return -pt;
            }

            if (dataMatrix.contains(pt)) { // check if inside the matrix
                return { 0, 0 }; // no preparation or translation needed
            }

            extensions::point min_pt = extensions::min(pt, { 0, 0 });
            extensions::point max_pt = extensions::max(pt + extensions::point{ 1, 1 }, dataMatrix.size());
            extensions::point translation = -min_pt;
            extensions::point new_size = max_pt + translation;

            matrix_t new_matrix(new_size);
            new_matrix.fill(false);
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
                    if (dataMatrix[{j, i}]) {
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
        std::pair<bool, extensions::point> changePixelState(extensions::point pt, bool newValue) {
            // note that this function performs linearly to the size of the matrix.  But since this is limited by how fast the user can click, it should be good enough

            if (!newValue) {
                if (dataMatrix.contains(pt) && dataMatrix[pt] != newValue) {
                    dataMatrix[pt] = newValue;

                    // Element is std::monostate, so we have to see if we can shrink the matrix size
                    // Inform the caller that the canvas was changed, and the translation required
                    return { true, shrinkDataMatrix(pt) };

                }
                else {
                    return { false,{ 0, 0 } };
                }
            }
            else {
                // Element is not std::monostate, so we might need to expand the matrix size first
                extensions::point translation = prepareDataMatrixForAddition(pt);
                pt += translation;

                if (dataMatrix[pt] != newValue) {
                    dataMatrix[pt] = newValue;
                    return { true, translation };
                }
                else {
                    // note: if we reach here, `translation` is guaranteed to be {0,0} as well
                    return { false,{ 0, 0 } };
                }
            }
        }

        /**
        * indices is a pair of {x,y}
        * @pre indices must be within the bounds of width and height
        * For a version that does bounds checking and expansion/contraction of the underlying matrix, use changePixelState
        */
        bool& operator[](const extensions::point& indices) noexcept {
            return dataMatrix[indices];
        }

        /**
        * indices is a pair of {x,y}
        * @pre indices must be within the bounds of width and height
        * For a version that does bounds checking and expansion/contraction of the underlying matrix, use changePixelState
        */
        const bool& operator[](const extensions::point& indices) const noexcept {
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

    };

}