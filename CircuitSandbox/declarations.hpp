#pragma once

/**
 * Stores all stuff that is global to the whole program, and are not associated with anything else.
 */

#include <SDL.h>

#include "tag_tuple.hpp"

// drawables
class MainWindow;
class Toolbox;
class PlayArea;
class ButtonBar;
class NotificationDisplay;

// all the available actions
class Action;
class PlayAreaAction;
template <typename> class PencilAction;
class SelectionAction;
class HistoryAction;
class FileNewAction;
class FileOpenAction;
class FileSaveAction;
class ScreenInputAction;
class FileCommunicatorSelectAction;

// all the tools and elements
struct Selector;
struct Panner;
struct Interactor;
struct Eraser;
struct ConductiveWire;
struct InsulatedWire;
struct Signal;
struct Source;
struct PositiveRelay;
struct NegativeRelay;
struct AndGate;
struct OrGate;
struct NandGate;
struct NorGate;
struct ScreenCommunicatorElement;
struct FileInputCommunicatorElement;
struct FileOutputCommunicatorElement;

// compile-time type tag which stores the list of available elements
using tool_tags_t = ext::tag_tuple<Selector, Panner, Interactor, Eraser, ConductiveWire, InsulatedWire, Signal, Source, PositiveRelay, NegativeRelay, AndGate, OrGate, NandGate, NorGate, ScreenCommunicatorElement, FileInputCommunicatorElement, FileOutputCommunicatorElement>;

// list of actions that have a static startWithPlayAreaMouseButtonDown(const SDL_MouseButtonEvent&, MainWindow&, PlayArea& const ActionStarter&);
using playarea_action_tags_t = ext::tag_tuple<SelectionAction, ScreenInputAction, FileCommunicatorSelectAction, PencilAction<Eraser>, PencilAction<ConductiveWire>, PencilAction<InsulatedWire>, PencilAction<Signal>, PencilAction<Source>, PencilAction<PositiveRelay>, PencilAction<NegativeRelay>, PencilAction<AndGate>, PencilAction<OrGate>, PencilAction<NandGate>, PencilAction<NorGate>, PencilAction<ScreenCommunicatorElement>, PencilAction<FileInputCommunicatorElement>, PencilAction<FileOutputCommunicatorElement>>;

// communicators
class ScreenCommunicator;
class FileInputCommunicator;
class FileOutputCommunicator;

// colors
constexpr static SDL_Color BLACK          { 0x1d, 0x1e, 0x21, 0xFF };
constexpr static SDL_Color RED            { 0xa5, 0x20, 0x20, 0xFF };
constexpr static SDL_Color GREEN          { 0x71, 0x94, 0x40, 0xFF };
constexpr static SDL_Color YELLOW         { 0xea, 0x87, 0x15, 0xFF };
constexpr static SDL_Color BLUE           { 0x44, 0x75, 0x9d, 0xFF };
constexpr static SDL_Color MAGENTA        { 0x79, 0x4e, 0x87, 0xFF };
constexpr static SDL_Color CYAN           { 0x43, 0x86, 0x7e, 0xFF };
constexpr static SDL_Color WHITE          { 0xbd, 0xc1, 0xbf, 0xFF };
constexpr static SDL_Color BRIGHT_BLACK   { 0x43, 0x48, 0x4f, 0xFF };
constexpr static SDL_Color BRIGHT_RED     { 0xf1, 0x6e, 0x6e, 0xFF };
constexpr static SDL_Color BRIGHT_GREEN   { 0xa4, 0xe5, 0x45, 0xFF };
constexpr static SDL_Color BRIGHT_YELLOW  { 0xfa, 0xd3, 0x35, 0xFF };
constexpr static SDL_Color BRIGHT_BLUE    { 0xa7, 0xc2, 0xeb, 0xFF };
constexpr static SDL_Color BRIGHT_MAGENTA { 0xd6, 0x8e, 0xeb, 0xFF };
constexpr static SDL_Color BRIGHT_CYAN    { 0x62, 0xdd, 0xd7, 0xFF };
constexpr static SDL_Color BRIGHT_WHITE   { 0xed, 0xf0, 0xee, 0xFF };

// strings
// we use compiler #defines so we can have adjacent string concatenation
#define CIRCUIT_SANDBOX_STRING "Circuit Sandbox"
#define CIRCUIT_SANDBOX_VERSION_STRING "v0.3" // change this when there is a new update
#define WINDOW_TITLE_STRING CIRCUIT_SANDBOX_STRING " " CIRCUIT_SANDBOX_VERSION_STRING

/**
 * The number of distinct "input handles": 5 (SDL2 supports this many mouse buttons) + 1 (for touch input)
 */
constexpr static size_t NUM_INPUT_HANDLES = 6;

// number of clipboards (excluding the default clipboard)
constexpr static size_t NUM_CLIPBOARDS = 10;

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
