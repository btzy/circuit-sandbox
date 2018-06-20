#pragma once

/**
 * Stores all stuff that is global to the whole program, and are not associated with anything else.
 */

#include <SDL.h>

#include "tag_tuple.hpp"
#include "elements.hpp"

class MainWindow;
class Toolbox;
class PlayArea;


// compile-time type tag which stores the list of available elements
using tool_tags_t = extensions::tag_tuple<Selector, Panner, Eraser, ConductiveWire, InsulatedWire, Signal, Source, PositiveRelay, NegativeRelay, AndGate, OrGate, NandGate, NorGate>;


// compile-time type tag which stores the list of available actions
class BaseAction;
template <typename> class PencilAction;
class SelectionAction;
class HistoryAction;
class FileNewAction;
class FileOpenAction;
class FileSaveAction;
using action_tags_t = extensions::tag_tuple<BaseAction, SelectionAction, HistoryAction, FileNewAction, FileOpenAction, FileSaveAction, PencilAction<Eraser>, PencilAction<ConductiveWire>, PencilAction<InsulatedWire>, PencilAction<Signal>, PencilAction<Source>, PencilAction<PositiveRelay>, PencilAction<NegativeRelay>, PencilAction<AndGate>, PencilAction<OrGate>, PencilAction<NandGate>, PencilAction<NorGate>>;


/**
 * The number of distinct "input handles": 5 (SDL2 supports this many mouse buttons) + 1 (for touch input)
 */
constexpr static size_t NUM_INPUT_HANDLES = 6;

/**
 * Resolve the mouse button event to the input handle index (used with 'selectedToolIndices');
 * Uses fields 'which' and 'button'
 */
inline size_t resolveInputHandleIndex(const SDL_MouseButtonEvent& event) {
    if (event.which == SDL_TOUCH_MOUSEID) {
        return 0;
    }
    else {
        switch (event.button) {
        case SDL_BUTTON_LEFT:
            return 1;
        case SDL_BUTTON_MIDDLE:
            return 2;
        case SDL_BUTTON_RIGHT:
            return 3;
        case SDL_BUTTON_X1:
            return 4;
        case SDL_BUTTON_X2:
            return 5;
        default:
#if defined(__GNUC__)
            __builtin_unreachable();
#elif defined(_MSC_VER)
            __assume(0);
#endif
        }
    }
}