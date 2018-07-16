#pragma once

#include <SDL.h>
#include <algorithm>
#include <variant>
#include <memory>
#include <atomic>
#include "declarations.hpp"

/**
 * This header file contains the definitions for all the elements in the game.
 */


struct Pencil {}; // base class for normal elements and the eraser
struct Element : public Pencil {
public:
    constexpr bool isSignal() const noexcept {
        return false;
    }

    constexpr bool isSignalReceiver() const noexcept {
        return false;
    }

}; // base class for elements

struct RenderLogicLevelElement {
protected:
    RenderLogicLevelElement(bool logicLevel, bool startingLogicLevel) noexcept : logicLevel(logicLevel), startingLogicLevel(startingLogicLevel) {}

public:
    // the current logic level (for display/rendering purposes only!)
    bool logicLevel; // true = HIGH, false = LOW
    bool startingLogicLevel;

    void reset() noexcept {
        logicLevel = startingLogicLevel;
    }

    template <bool StartingState = false>
    bool getLogicLevel() const noexcept {
        if constexpr (StartingState) {
            return startingLogicLevel;
        }
        else {
            return logicLevel;
        }
    }
};

struct SignalReceivingElement : public Element {
public:
    constexpr bool isSignalReceiver() const noexcept {
        return true;
    }
};

// elements that use logic levels for compilation (meaning that it is not only for display purposes)
struct LogicLevelElement : public RenderLogicLevelElement {
    LogicLevelElement(bool logicLevel, bool startingLogicLevel) noexcept : RenderLogicLevelElement(logicLevel, startingLogicLevel) {}
};

struct LogicGate : public SignalReceivingElement, public LogicLevelElement {
protected:
    LogicGate(bool logicLevel, bool startingLogicLevel) noexcept : LogicLevelElement(logicLevel, startingLogicLevel) {}
};
struct Relay : public SignalReceivingElement, public RenderLogicLevelElement {
protected:
    Relay(bool logicLevel, bool startingLogicLevel, bool conductiveState, bool startingConductiveState) noexcept : RenderLogicLevelElement(logicLevel, startingLogicLevel), conductiveState(conductiveState), startingConductiveState(startingConductiveState) {}
public:
    bool conductiveState;
    bool startingConductiveState;

    void reset() noexcept {
        conductiveState = startingConductiveState;
        RenderLogicLevelElement::reset();
    }

    template <bool StartingState = false>
    bool getConductiveState() const noexcept {
        if constexpr (StartingState) {
            return startingConductiveState;
        }
        else {
            return conductiveState;
        }
    }
};
struct CommunicatorElement : public SignalReceivingElement, public LogicLevelElement {
protected:
    CommunicatorElement(bool logicLevel, bool startingLogicLevel, bool transmitState) : LogicLevelElement(logicLevel, startingLogicLevel), transmitState(transmitState) {}
public:
    bool transmitState; // filled in by Simulator::takeSnapshot()
};


// convenience functions
template <typename ElementVariant>
constexpr inline bool isSignal(const ElementVariant& v) {
    return std::visit([](const auto& element) {
        if constexpr(std::is_base_of_v<Element, std::decay_t<decltype(element)>>) {
            return element.isSignal();
        }
        else return false;
    }, v);
}

template <typename ElementVariant>
constexpr inline bool isSignalReceiver(const ElementVariant& v) {
    return std::visit([](const auto& element) {
        if constexpr(std::is_base_of_v<Element, std::decay_t<decltype(element)>>) {
            return element.isSignalReceiver();
        }
        else return false;
    }, v);
}

/**
 * Reset the this element (usually this will reset the logic levels to starting state).
 */
template <typename ElementVariant>
constexpr inline void resetLogicLevel(ElementVariant& v) {
    std::visit([](auto& element) {
        if constexpr(std::is_base_of_v<Element, std::decay_t<decltype(element)>>) {
            element.reset();
        }
    }, v);
}



/**
 * All tools "Tool" must satisfy the following:
 * 1. Tool::displayColor is a SDL_Color (the display color when drawn on screen)
 * 2. Tool::displayName is a const char* (the name displayed in the toolbox)
 */

struct Selector {
    static constexpr SDL_Color displayColor{ 0xFF, 0xFF, 0xFF, 0xFF };
    static constexpr const char* displayName = "Selector";
};

struct Panner {
    static constexpr SDL_Color displayColor{ 0xFF, 0xFF, 0xFF, 0xFF };
    static constexpr const char* displayName = "Panner";
};

struct Interactor {
    static constexpr SDL_Color displayColor{ 0xFF, 0xFF, 0xFF, 0xFF };
    static constexpr const char* displayName = "Interactor";
};

struct Eraser : public Pencil {
    static constexpr SDL_Color displayColor{ 0xFF, 0xFF, 0xFF, 0xFF };
    static constexpr const char* displayName = "Eraser";
};

struct ConductiveWire : public Element, public RenderLogicLevelElement {
    static constexpr SDL_Color displayColor{0x99, 0x99, 0x99, 0xFF};
    static constexpr const char* displayName = "Conductive Wire";

    ConductiveWire(bool logicLevel = false, bool startingLogicLevel = false) noexcept : RenderLogicLevelElement(logicLevel, startingLogicLevel) {}
};

struct InsulatedWire : public Element, public RenderLogicLevelElement {
    // previously was: {0xCC, 0x99, 0x66, 0xFF};
    static constexpr SDL_Color displayColor{0, 0x66, 0x44, 0xFF};
    static constexpr const char* displayName = "Insulated Wire";

    InsulatedWire(bool logicLevel = false, bool startingLogicLevel = false) noexcept : RenderLogicLevelElement(logicLevel, startingLogicLevel) {}
};


struct Signal : public Element, public RenderLogicLevelElement {
    static constexpr SDL_Color displayColor{ 0xFF, 0xFF, 0, 0xFF };
    static constexpr const char* displayName = "Signal";

    Signal(bool logicLevel = false, bool startingLogicLevel = false) noexcept : RenderLogicLevelElement(logicLevel, startingLogicLevel) {}

    constexpr bool isSignal() const {
        return true;
    }
};


struct Source : public Element {
    static constexpr SDL_Color displayColor{ 0, 0xFF, 0, 0xFF };
    static constexpr const char* displayName = "Source";

    Source() noexcept {}

    template <bool StartingState = false>
    bool getLogicLevel() const noexcept {
        return true;
    }

    void reset() const noexcept {}
};


struct AndGate : public LogicGate {
    static constexpr SDL_Color displayColor{ 0xFF, 0x0, 0xFF, 0xFF };
    static constexpr const char* displayName = "AND Gate";

    AndGate(bool logicLevel = false, bool startingLogicLevel = false) noexcept : LogicGate(logicLevel, startingLogicLevel) {}
};


struct OrGate : public LogicGate {
    static constexpr SDL_Color displayColor{ 0x99, 0x0, 0xFF, 0xFF };
    static constexpr const char* displayName = "OR Gate";

    OrGate(bool logicLevel = false, bool startingLogicLevel = false) noexcept : LogicGate(logicLevel, startingLogicLevel) {}
};


struct NandGate : public LogicGate {
    static constexpr SDL_Color displayColor{ 0x66, 0x88, 0xFF, 0xFF };
    static constexpr const char* displayName = "NAND Gate";

    NandGate(bool logicLevel = false, bool startingLogicLevel = false) noexcept : LogicGate(logicLevel, startingLogicLevel) {}
};


struct NorGate : public LogicGate {
    static constexpr SDL_Color displayColor{ 0x0, 0x88, 0xFF, 0xFF };
    static constexpr const char* displayName = "NOR Gate";

    NorGate(bool logicLevel = false, bool startingLogicLevel = false) noexcept : LogicGate(logicLevel, startingLogicLevel) {}
};


struct PositiveRelay : public Relay {
    static constexpr SDL_Color displayColor{ 0xFF, 0x99, 0, 0xFF };
    static constexpr const char* displayName = "Positive Relay";

    PositiveRelay(bool logicLevel = false, bool startingLogicLevel = false, bool conductiveState = false, bool startingConductiveState = false) noexcept : Relay(logicLevel, startingLogicLevel, conductiveState, startingConductiveState) {}
};


struct NegativeRelay : public Relay {
    static constexpr SDL_Color displayColor{ 0xFF, 0x33, 0x0, 0xFF };
    static constexpr const char* displayName = "Negative Relay";

    NegativeRelay(bool logicLevel = false, bool startingLogicLevel = false, bool conductiveState = false, bool startingConductiveState = false) noexcept : Relay(logicLevel, startingLogicLevel, conductiveState, startingConductiveState) {}
};

struct ScreenCommunicatorElement : public CommunicatorElement {
    static constexpr SDL_Color displayColor{ 0xFF, 0, 0, 0xFF };
    static constexpr const char* displayName = "Screen I/O";

    // shared communicator instance
    // after action is ended, this should point to a valid communicator instance (filled in by Simulator::compile())
    std::shared_ptr<ScreenCommunicator> communicator;

    ScreenCommunicatorElement(bool logicLevel = false, bool startingLogicLevel = false, bool transmitState = false) noexcept : CommunicatorElement(logicLevel, startingLogicLevel, transmitState) {}

    SDL_Color computeDisplayColor() const noexcept {
        if (transmitState) {
            return SDL_Color{ static_cast<Uint8>(0xFF - (0xFF - ScreenCommunicatorElement::displayColor.r) * 1 / 3), static_cast<Uint8>(0xFF - (0xFF - ScreenCommunicatorElement::displayColor.g) * 1 / 3), static_cast<Uint8>(0xFF - (0xFF - ScreenCommunicatorElement::displayColor.b) * 1 / 3), ScreenCommunicatorElement::displayColor.a };
        }
        else {
            return SDL_Color{ static_cast<Uint8>(ScreenCommunicatorElement::displayColor.r * 1 / 3), static_cast<Uint8>(ScreenCommunicatorElement::displayColor.g * 1 / 3), static_cast<Uint8>(ScreenCommunicatorElement::displayColor.b * 1 / 3), ScreenCommunicatorElement::displayColor.a };
        }
    }
};

struct FileInputCommunicatorElement : public CommunicatorElement {
    static constexpr SDL_Color displayColor{ 0xFF, 0, 0, 0xFF };
    static constexpr const char* displayName = "File Input";

    std::shared_ptr<FileInputCommunicator> communicator;

    FileInputCommunicatorElement(bool logicLevel = false, bool startingLogicLevel = false, bool transmitState = false) noexcept : CommunicatorElement(logicLevel, startingLogicLevel, transmitState) {}

    SDL_Color computeDisplayColor() const noexcept {
        if (transmitState) {
            return SDL_Color{ static_cast<Uint8>(0xFF - (0xFF - FileInputCommunicatorElement::displayColor.r) * 2 / 3), static_cast<Uint8>(0xFF - (0xFF - FileInputCommunicatorElement::displayColor.g) * 2 / 3), static_cast<Uint8>(0xFF - (0xFF - FileInputCommunicatorElement::displayColor.b) * 2 / 3), FileInputCommunicatorElement::displayColor.a };
        }
        else {
            return SDL_Color{ static_cast<Uint8>(FileInputCommunicatorElement::displayColor.r * 2 / 3), static_cast<Uint8>(FileInputCommunicatorElement::displayColor.g * 2 / 3), static_cast<Uint8>(FileInputCommunicatorElement::displayColor.b * 2 / 3), FileInputCommunicatorElement::displayColor.a };
        }
    }
};




// display colour functions
template <bool StartingState, typename Element>
constexpr inline SDL_Color computeDisplayColor(const Element& element) noexcept {
    if constexpr(std::is_base_of_v<CommunicatorElement, Element>) {
        return element.computeDisplayColor();
    }
    else {
        if (element.getLogicLevel<StartingState>()) {
            return SDL_Color{ static_cast<Uint8>(0xFF - (0xFF - Element::displayColor.r) * 2 / 3), static_cast<Uint8>(0xFF - (0xFF - Element::displayColor.g) * 2 / 3), static_cast<Uint8>(0xFF - (0xFF - Element::displayColor.b) * 2 / 3), Element::displayColor.a };
        }
        else {
            return SDL_Color{ static_cast<Uint8>(Element::displayColor.r * 2 / 3), static_cast<Uint8>(Element::displayColor.g * 2 / 3), static_cast<Uint8>(Element::displayColor.b * 2 / 3), Element::displayColor.a };
        }
    }
}
