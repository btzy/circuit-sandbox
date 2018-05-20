#pragma once

#include <utility> // for std::pair
#include <variant> // for std::variant
#include <type_traits>
#include <algorithm> // for std::min and std::max
#include <cstdint> // for int32_t and uint32_t
#include <limits>

#include "heap_matrix.hpp"
#include "elements.hpp"


/**
 * Represents the state of the saveable game data.
 * All the game state that needs to be saved to disk should reside in this class.
 * Exposes functions for resizing and editing the canvas
 */

class GameState {

private:
    // std::monostate is a 'default' state, which represents an empty pixel
    using element_variant_t = std::variant<std::monostate, ConductiveWire, InsulatedWire>;
    extensions::heap_matrix<element_variant_t> dataMatrix;

    /**
     * Modifies the dataMatrix so that the x and y will be within the matrix.
     * Returns the translation that should be applied on {x,y}.
     */
    std::pair<int32_t, int32_t> prepareDataMatrixForAddition(int32_t x, int32_t y) {
        if (dataMatrix.empty()) {
            // special case for empty matrix
            dataMatrix = extensions::heap_matrix<element_variant_t>(1, 1);
            return { -x, -y };
        }

        if (0 >= x && x < dataMatrix.width() && 0 >= y && y < dataMatrix.height()) { // check if inside the matrix
            return { 0, 0 }; // no preparation or translation needed
        }

        int32_t x_min = std::min(x, 0);
        int32_t x_max = std::max(x + 1, static_cast<int32_t>(dataMatrix.width()));
        int32_t x_translation = -x_min;
        int32_t new_width = x_max + x_translation;

        int32_t y_min = std::min(y, 0);
        int32_t y_max = std::max(y + 1, static_cast<int32_t>(dataMatrix.height()));
        int32_t y_translation = -y_min;
        int32_t new_height = y_max + y_translation;

        extensions::heap_matrix<element_variant_t> new_matrix(new_width, new_height);
        extensions::move_range(dataMatrix, new_matrix, 0, 0, x_translation, y_translation, dataMatrix.width(), dataMatrix.height());

        dataMatrix = std::move(new_matrix);

        return { x_translation, y_translation };
    }

    /**
    * Shrinks the dataMatrix to the minimal bounding rectangle.
    * As an optimization, will only shrink if {x,y} is along a border.
    * Returns the translation that should be applied on {x,y}.
    */
    std::pair<int32_t, int32_t> shrinkDataMatrix(int32_t x, int32_t y) {
        if (x > 0 && x + 1 < dataMatrix.width() && y > 0 && y + 1 < dataMatrix.height()) { // check if not on the border
            return { 0, 0 }; // no preparation or translation needed
        }


        // otherwise, we simply iterate the whole matrix to get the min and max values
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
            dataMatrix = extensions::heap_matrix<element_variant_t>();

            // if the matrix is totally empty, returning a translation makes no sense
            // so we just return {0,0}
            return { 0, 0 };
        }

        int32_t new_width = x_max + 1 - x_min;
        int32_t new_height = y_max + 1 - y_min;

        extensions::heap_matrix<element_variant_t> new_matrix(new_width, new_height);
        extensions::move_range(dataMatrix, new_matrix, x_min, y_min, 0, 0, new_width, new_height);

        dataMatrix = std::move(new_matrix);

        return { -x_min, -y_min };
    }

public:

    GameState() {
        // nothing to initialize; 'dataMatrix' is default initialized to the std::monostate
    }

    /**
     * Change the state of a pixel.
     * 'Element' should be one of the elementis in 'element_variant_t', or std::monostate for the eraser
     * Returns the net translation change that the viewport should apply (such that the viewport will be at the 'same' position)
     */
    template <typename Element>
    std::pair<int32_t, int32_t> changePixelState(int32_t x, int32_t y) {
        std::pair<int32_t, int32_t> translation{ 0, 0 };

        // note that this function performs linearly to the size of the matrix.  But since this is limited by how fast the user can click, it should be good enough

        if constexpr (!(std::is_same_v<std::monostate, Element>)) {
            // Element is not std::monostate, so we might need to expand the matrix size first
            translation = prepareDataMatrixForAddition(x, y);
            x += translation.first;
            y += translation.second;
        }

        dataMatrix[{x, y}] = Element{};

        if constexpr(std::is_same_v<std::monostate, Element>) {
            // Element is std::monostate, so we have to see if we can shrink the matrix size
            translation = shrinkDataMatrix(x, y);

            // note that shrinkDataMatrix() and prepareDataMatrixForAddition() cannot both be called.  At most one of them will be called only.
        }

        return translation;
    }

    /**
     * Draw a rectangle of elements onto a pixel buffer supplied by PlayArea.
     * Pixel format: pixel = R | (G << 8) | (B << 16)
     */
    void fillSurface(uint32_t* pixelBuffer, int32_t x, int32_t y, int32_t width, int32_t height) const;
};
