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

class GameState {
private:

    // the possible elements that a pixel can represent
    // std::monostate is a 'default' state, which represents an empty pixel
    using element_tags_t = extensions::tag_tuple<std::monostate, ConductiveWire, InsulatedWire, Signal, Source, PositiveRelay, NegativeRelay, AndGate, OrGate, NandGate, NorGate>;

    using element_variant_t = element_tags_t::instantiate<std::variant>;
    using matrix_t = extensions::heap_matrix<element_variant_t>;

    matrix_t dataMatrix;

    // stuff needed to implement selection
    // we need to update gameState even before the selection actually merges with dataMatrix (so that the simulation looks correct)
    matrix_t selection; // stores the selection
    matrix_t base; // the 'base' layer is a copy of dataMatrix minus selection at the time the selection was made
    int32_t selectionX = 0, selectionY = 0; // position of selection in dataMatrix's coordinate system
    int32_t baseX = 0, baseY = 0; // position of base in dataMatrix's coordinate system

    bool changed = false; // whether dataMatrix changed since the last write to the undo stack
    extensions::point deltaTrans{ 0, 0 }; // difference in viewport translation from previous gamestate (TODO: move this into a proper UndoDelta class)

    friend class StateManager;
    friend class Simulator;

    /**
    * Modifies the matrix so that the x and y will be within it
    * Returns the translation that should be applied on {x,y}.
    */
    extensions::point prepareMatrixForAddition(matrix_t& matrix, int32_t x, int32_t y) {
        if (matrix.empty()) {
            // special case for empty matrix
            matrix = matrix_t(1, 1);
            return { -x, -y };
        }

        if (0 <= x && x < matrix.width() && 0 <= y && y < matrix.height()) { // check if inside the matrix
            return { 0, 0 }; // no preparation or translation needed
        }

        int32_t x_min = std::min(x, 0);
        int32_t x_max = std::max(x + 1, matrix.width());
        int32_t x_translation = -x_min;
        int32_t new_width = x_max + x_translation;

        int32_t y_min = std::min(y, 0);
        int32_t y_max = std::max(y + 1, matrix.height());
        int32_t y_translation = -y_min;
        int32_t new_height = y_max + y_translation;

        matrix_t new_matrix(new_width, new_height);
        extensions::move_range(matrix, new_matrix, 0, 0, x_translation, y_translation, matrix.width(), matrix.height());

        matrix = std::move(new_matrix);

        return { x_translation, y_translation };
    }

    /**
     * Shrinks the matrix to the minimal bounding rectangle.
     * As an optimization, will only shrink if {x,y} is along a border.
     * Returns the translation that should be applied on {x,y}.
     */
    extensions::point shrinkMatrix(matrix_t& matrix, int32_t x, int32_t y) {
        if (x > 0 && x + 1 < matrix.width() && y > 0 && y + 1 < matrix.height()) { // check if not on the border
            return { 0, 0 }; // no preparation or translation needed
        }
        return shrinkMatrix(matrix);
    }

    /**
     * Shrink with no optimization.
     */
    extensions::point shrinkMatrix(matrix_t& matrix) {
        // we simply iterate the whole matrix to get the min and max values
        int32_t x_min = std::numeric_limits<int32_t>::max();
        int32_t x_max = std::numeric_limits<int32_t>::min();
        int32_t y_min = std::numeric_limits<int32_t>::max();
        int32_t y_max = std::numeric_limits<int32_t>::min();

        // iterate height before width for cache-friendliness
        for (int32_t i = 0; i != matrix.height(); ++i) {
            for (int32_t j = 0; j != matrix.width(); ++j) {
                if (!std::holds_alternative<std::monostate>(matrix[{j, i}])) {
                    x_min = std::min(x_min, j);
                    x_max = std::max(x_max, j);
                    y_min = std::min(y_min, i);
                    y_max = std::max(y_max, i);
                }
            }
        }

        if (x_min == 0 && x_max + 1 == matrix.width() && y_min == 0 && y_max + 1 == matrix.height()) {
            // no resizing needed
            return { 0, 0 };
        }

        if (x_min > x_max) {
            // if this happens, it means the game state is totally empty
            matrix = matrix_t();

            // if the matrix is totally empty, returning a translation makes no sense
            // so we just return {0,0}
            return { 0, 0 };
        }

        int32_t new_width = x_max + 1 - x_min;
        int32_t new_height = y_max + 1 - y_min;

        matrix_t new_matrix(new_width, new_height);
        extensions::move_range(matrix, new_matrix, x_min, y_min, 0, 0, new_width, new_height);

        matrix = std::move(new_matrix);

        return { -x_min, -y_min };
    }

public:

    /**
     * Change the state of a pixel.
     * 'Element' should be one of the elements in 'element_variant_t', or std::monostate for the eraser
     * Returns the net translation change that the viewport should apply (such that the viewport will be at the 'same' position)
     */
    template <typename Element>
    extensions::point changePixelState(int32_t x, int32_t y) {
        extensions::point translation{ 0, 0 };

        // note that this function performs linearly to the size of the matrix.  But since this is limited by how fast the user can click, it should be good enough

        if constexpr (std::is_same_v<std::monostate, Element>) {
            if (x >= 0 && x < dataMatrix.width() && y >= 0 && y < dataMatrix.height()) {
                // flag the gamestate as having changed
                if (!std::holds_alternative<Element>(dataMatrix[{x, y}])) {
                    changed = true;
                }
                dataMatrix[{x, y}] = Element{};

                // Element is std::monostate, so we have to see if we can shrink the matrix size
                translation = shrinkMatrix(dataMatrix, x, y);
            }
        }
        else {
            // Element is not std::monostate, so we might need to expand the matrix size first
            translation = prepareMatrixForAddition(dataMatrix, x, y);
            x += translation.x;
            y += translation.y;

            // flag the gamestate as having changed
            if (!std::holds_alternative<Element>(dataMatrix[{x, y}])) {
                changed = true;
            }
            dataMatrix[{x, y}] = Element{};
        }

        deltaTrans.x += translation.x;
        deltaTrans.y += translation.y;
        return translation;
    }

    /**
     * Move elements within selectionRect from dataMatrix to selection.
     */
    void selectRect(SDL_Rect selectionRect);

    /**
     * Check if (x, y) has been selected.
     */
    bool pointInSelection(int32_t x, int32_t y);

    /**
     * Clear the selection and merge it with dataMatrix.
     */
    void clearSelection();

    /**
     * Merge base and selection. The result is stored in dataMatrix.
     * Pre: selectionX/Y and baseX/Y are correct so dataMatrix does not have to be further resized
     */
    void mergeSelection();

    /**
     * Move the selection. Gamestate may resize and shift so this returns the translation.
     */
    extensions::point moveSelection(int32_t dx, int32_t dy);
};
