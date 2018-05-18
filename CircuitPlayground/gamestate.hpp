#pragma once

#include <utility> // for std::pair
#include <variant> // for std::variant

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

public:

    GameState() {
        // TODO
    }

    template <typename Element>
    void addElement(size_t x, size_t y) {
        // TODO
    }
};
