/*
 * Circuit Sandbox
 * Copyright 2018 National University of Singapore <enterprise@nus.edu.sg>
 *
 * This file is part of Circuit Sandbox.
 * Circuit Sandbox is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 * Circuit Sandbox is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with Circuit Sandbox.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

/**
 * Encapsulates a heap_matrix that is resizeable
 */

#include <type_traits>
#include <algorithm> // for std::min and std::max
#include <cstdint> // for int32_t and uint32_t
#include <limits>
#include "heap_matrix.hpp"

namespace ext {

    class expandable_bool_matrix {
    private:

        using matrix_t = ext::heap_matrix<bool>;

        matrix_t dataMatrix;

        /**
        * Modifies the dataMatrix so that the x and y will be within the matrix.
        * Returns the translation that should be applied on {x,y}.
        */
        ext::point prepareDataMatrixForAddition(const ext::point& pt) {
            if (dataMatrix.empty()) {
                // special case for empty matrix
                dataMatrix = matrix_t(1, 1);
                dataMatrix[{0, 0}] = false;
                return -pt;
            }

            if (dataMatrix.contains(pt)) { // check if inside the matrix
                return { 0, 0 }; // no preparation or translation needed
            }

            ext::point min_pt = ext::min(pt, { 0, 0 });
            ext::point max_pt = ext::max(pt + ext::point{ 1, 1 }, dataMatrix.size());
            ext::point translation = -min_pt;
            ext::point new_size = max_pt + translation;

            matrix_t new_matrix(new_size);
            new_matrix.fill(false);
            ext::move_range(dataMatrix, new_matrix, 0, 0, translation.x, translation.y, dataMatrix.width(), dataMatrix.height());

            dataMatrix = std::move(new_matrix);

            return translation;
        }

        /**
        * Shrinks the dataMatrix to the minimal bounding rectangle.
        * As an optimization, will only shrink if {x,y} is along a border.
        * Returns the translation that should be applied on {x,y}.
        */
        ext::point shrinkDataMatrix(const ext::point& pt) {

            if (pt.x > 0 && pt.x + 1 < dataMatrix.width() && pt.y > 0 && pt.y + 1 < dataMatrix.height()) { // check if not on the border
                return { 0, 0 }; // no preparation or translation needed
            }

            return shrinkDataMatrix();
        }

        /**
        * Shrink with no optimization.
        */
        ext::point shrinkDataMatrix() {

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
            ext::move_range(dataMatrix, new_matrix, x_min, y_min, 0, 0, new_width, new_height);

            dataMatrix = std::move(new_matrix);

            return { -x_min, -y_min };
        }

    public:

        /**
        * Change the state of a pixel.
        * 'Element' should be one of the elements in 'element_variant_t', or std::monostate for the eraser
        * Returns a pair describing whether the canvas was actually modified, and the net translation change that the viewport should apply (such that the viewport will be at the 'same' position)
        */
        std::pair<bool, ext::point> changePixelState(ext::point pt, bool newValue) {
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
                ext::point translation = prepareDataMatrixForAddition(pt);
                pt += translation;

                if (dataMatrix[pt] != newValue) {
                    dataMatrix[pt] = newValue;
                    return { true, translation };
                }
                else {
                    // note: if we reach here, `translation` is guaranteed to be {0,0} as well
                    return { false, { 0, 0 } };
                }
            }
        }

        /**
        * Change the state of a rectangle of pixels.
        * Note that `bottomRight` is actually past-the-end, for consistency usual expectations and with CanvasState::extend()
        * 'Element' should be one of the elements in 'element_variant_t', or std::monostate for the eraser
        * Returns a pair describing whether the canvas was actually modified, and the net translation change that the viewport should apply (such that the viewport will be at the 'same' position)
        */
        std::pair<bool, ext::point> changeRectState(ext::point topLeft, ext::point bottomRight, bool newValue) {
            bool changed = false;

            if (!newValue) {
                if(dataMatrix.overlaps(topLeft, bottomRight)) {
                    return { false, { 0, 0 } };
                }

                topLeft = max(topLeft, { 0, 0 });
                bottomRight = min(bottomRight, dataMatrix.size());
                int32_t width = bottomRight.x - topLeft.x + 1;
                int32_t height = topLeft.y - bottomRight.y + 1;

                for (int32_t y = 0; y < height; ++y) {
                    for (int32_t x = 0; x < width; ++x) {
                        ext::point pt = topLeft + ext::point{ x, y };
                        if (dataMatrix[pt] != newValue) {
                            dataMatrix[pt] = newValue;
                            changed = true;
                        }
                    }
                }
                return { changed, shrinkDataMatrix() };
            }
            else {
                ext::point translation = prepareDataMatrixForAddition(topLeft);
                bottomRight += translation;
                translation += prepareDataMatrixForAddition(bottomRight - ext::point{ 1, 1 });
                topLeft += translation;
                int32_t width = bottomRight.x - topLeft.x;
                int32_t height = topLeft.y - bottomRight.y;

                for (int32_t y = 0; y < height; ++y) {
                    for (int32_t x = 0; x < width; ++x) {
                        ext::point pt = topLeft + ext::point{ x, y };
                        if (dataMatrix[pt] != newValue) {
                            dataMatrix[pt] = newValue;
                            changed = true;
                        }
                    }
                }
                return { changed, changed ? translation : ext::point{ 0, 0 } };
            }
        }

        /**
        * indices is a pair of {x,y}
        * @pre indices must be within the bounds of width and height
        * For a version that does bounds checking and expansion/contraction of the underlying matrix, use changePixelState
        */
        bool& operator[](const ext::point& indices) noexcept {
            return dataMatrix[indices];
        }

        /**
        * indices is a pair of {x,y}
        * @pre indices must be within the bounds of width and height
        * For a version that does bounds checking and expansion/contraction of the underlying matrix, use changePixelState
        */
        const bool& operator[](const ext::point& indices) const noexcept {
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
        ext::point size() const noexcept {
            return dataMatrix.size();
        }

    };

}
