#pragma once

#include <SDL.h>
#include <algorithm>
#include <variant>
#include <memory>
#include <atomic>
#include "communicator.hpp"

/**
 * This header file contains the definitions for all the elements in the game.
 */

enum struct AdjacentElementType : char {
    EMPTY,
    WIRE,
    SIGNAL
};

struct AdjacentInput {
    AdjacentElementType type;
    bool logicLevel;

    // operator so that the getLowSignalCounts and getHighSignal counts will work:
    friend inline bool operator==(const AdjacentInput& a, const AdjacentInput& b) {
        return a.type == b.type && a.logicLevel == b.logicLevel;
    }

    friend bool operator!=(const AdjacentInput& a, const AdjacentInput& b) {
        return !(a == b);
    }
};

struct AdjacentEnvironment {
    AdjacentInput inputs[4];
};

enum struct ElementState {
    INSULATOR,
    INSULATED_WIRE,
    CONDUCTIVE_WIRE,
    SOURCE
};


// preprocess an AdjacentEnvironment for the counts of the low and high signals respectively; useful for some element types
inline auto getLowSignalCounts(const AdjacentEnvironment& env) {
    return std::count(env.inputs, env.inputs + 4, AdjacentInput{ AdjacentElementType::SIGNAL, false });
}

inline auto getHighSignalCounts(const AdjacentEnvironment& env) {
    return  std::count(env.inputs, env.inputs + 4, AdjacentInput{ AdjacentElementType::SIGNAL, true });
}


struct Pencil {}; // base class for normal elements and the eraser
struct Element : public Pencil {
protected:
    // the current logic level (for display/rendering purposes only!)
    bool logicLevel; // true = HIGH, false = LOW
    bool defaultLogicLevel;

    Element(bool logicLevel, bool defaultLogicLevel) : logicLevel(logicLevel), defaultLogicLevel(defaultLogicLevel) {}

public:

    constexpr bool isSignal() const {
        return false;
    }

    constexpr bool isSignalReceiver() const {
        return false;
    }

    bool getLogicLevel() const {
        return logicLevel;
    }

    bool getDefaultLogicLevel() const {
        return defaultLogicLevel;
    }

    void setLogicLevel(bool level) {
        logicLevel = level;
    }

    void resetLogicLevel() {
        logicLevel = defaultLogicLevel;
    }
}; // base class for elements

struct SignalReceivingElement : public Element {
protected:
    SignalReceivingElement(bool logicLevel, bool defaultLogicLevel) :Element(logicLevel, defaultLogicLevel) {}
public:
    constexpr bool isSignalReceiver() const {
        return true;
    }
};

struct LogicGate : public SignalReceivingElement {
protected:
    LogicGate(bool logicLevel, bool defaultLogicLevel) :SignalReceivingElement(logicLevel, defaultLogicLevel) {}
};
struct Relay : public SignalReceivingElement {
protected:
    Relay(bool logicLevel, bool defaultLogicLevel) :SignalReceivingElement(logicLevel, defaultLogicLevel) {}
};
struct CommunicatorElement : public SignalReceivingElement {
protected:
    CommunicatorElement(bool logicLevel, bool defaultLogicLevel) :SignalReceivingElement(logicLevel, defaultLogicLevel) {}
};

struct ScreenCommunicatorElement : public CommunicatorElement {
    static constexpr SDL_Color displayColor{ 0xFF, 0, 0, 0xFF };
    static constexpr const char* displayName = "Screen I/O";

    using communicator_t = ScreenCommunicator;

    // shared communicator instance
    // after action is ended, this should point to a valid communicator instance
    std::shared_ptr<ScreenCommunicator> communicator;

    ScreenCommunicatorElement(bool logicLevel = false, bool defaultLogicLevel = false) :CommunicatorElement(logicLevel, defaultLogicLevel) {}

    SDL_Color computeDisplayColor() const noexcept {
        if (communicator && communicator->_transmit.load(std::memory_order_acquire)) {
            return SDL_Color{ static_cast<Uint8>(0xFF - (0xFF - ScreenCommunicatorElement::displayColor.r) * 1 / 3), static_cast<Uint8>(0xFF - (0xFF - ScreenCommunicatorElement::displayColor.g) * 1 / 3), static_cast<Uint8>(0xFF - (0xFF - ScreenCommunicatorElement::displayColor.b) * 1 / 3), ScreenCommunicatorElement::displayColor.a };
        }
        else {
            return SDL_Color{ static_cast<Uint8>(ScreenCommunicatorElement::displayColor.r * 1 / 3), static_cast<Uint8>(ScreenCommunicatorElement::displayColor.g * 1 / 3), static_cast<Uint8>(ScreenCommunicatorElement::displayColor.b * 1 / 3), ScreenCommunicatorElement::displayColor.a };
        }
    }
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
template <typename ElementVariant>
constexpr inline void resetLogicLevel(ElementVariant& v) {
    std::visit([](auto& element) {
        if constexpr(std::is_base_of_v<Element, std::decay_t<decltype(element)>>) {
            element.resetLogicLevel();
        }
    }, v);
}


// display colour functions
template <typename Element>
constexpr inline SDL_Color computeDisplayColor(const Element& element, bool useDefaultLogicLevel) {
    if constexpr(std::is_same_v<ScreenCommunicatorElement, Element>) {
        return element.computeDisplayColor();
    }
    else {
        if (useDefaultLogicLevel ? element.getDefaultLogicLevel() : element.getLogicLevel()) {
            return SDL_Color{ static_cast<Uint8>(0xFF - (0xFF - Element::displayColor.r) * 2 / 3), static_cast<Uint8>(0xFF - (0xFF - Element::displayColor.g) * 2 / 3), static_cast<Uint8>(0xFF - (0xFF - Element::displayColor.b) * 2 / 3), Element::displayColor.a };
        }
        else {
            return SDL_Color{ static_cast<Uint8>(Element::displayColor.r * 2 / 3), static_cast<Uint8>(Element::displayColor.g * 2 / 3), static_cast<Uint8>(Element::displayColor.b * 2 / 3), Element::displayColor.a };
        }
    }
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

struct ConductiveWire : public Element {
    static constexpr SDL_Color displayColor{0x99, 0x99, 0x99, 0xFF};
    static constexpr const char* displayName = "Conductive Wire";

    ConductiveWire(bool logicLevel = false, bool defaultLogicLevel = false) :Element(logicLevel, defaultLogicLevel) {}

    // process a step of the simulation
    ElementState processStep(const AdjacentEnvironment&) const {
        return ElementState::CONDUCTIVE_WIRE;
    }
};

struct InsulatedWire : public Element {
    // previously was: {0xCC, 0x99, 0x66, 0xFF};
    static constexpr SDL_Color displayColor{0, 0x66, 0x44, 0xFF};
    static constexpr const char* displayName = "Insulated Wire";

    InsulatedWire(bool logicLevel = false, bool defaultLogicLevel = false) :Element(logicLevel, defaultLogicLevel) {}

    // process a step of the simulation
    ElementState processStep(const AdjacentEnvironment&) const {
        return ElementState::INSULATED_WIRE;
    }
};


struct Signal : public Element {
    static constexpr SDL_Color displayColor{ 0xFF, 0xFF, 0, 0xFF };
    static constexpr const char* displayName = "Signal";

    Signal(bool logicLevel = false, bool defaultLogicLevel = false) :Element(logicLevel, defaultLogicLevel) {}

    // process a step of the simulation
    ElementState processStep(const AdjacentEnvironment&) const {
        return ElementState::CONDUCTIVE_WIRE;
    }

    constexpr bool isSignal() const {
        return true;
    }
};


struct Source : public SignalReceivingElement {
    static constexpr SDL_Color displayColor{ 0, 0xFF, 0, 0xFF };
    static constexpr const char* displayName = "Source";

    Source(bool logicLevel = true, bool defaultLogicLevel = true) :SignalReceivingElement(logicLevel, defaultLogicLevel) {}

    // process a step of the simulation
    ElementState processStep(const AdjacentEnvironment&) const {
        return ElementState::SOURCE;
    }
};


struct AndGate : public LogicGate {
    static constexpr SDL_Color displayColor{ 0xFF, 0x0, 0xFF, 0xFF };
    static constexpr const char* displayName = "AND Gate";

    AndGate(bool logicLevel = false, bool defaultLogicLevel = false) :LogicGate(logicLevel, defaultLogicLevel) {}

    // process a step of the simulation
    ElementState processStep(const AdjacentEnvironment& env) const {
        return getLowSignalCounts(env) == 0 ? ElementState::SOURCE : ElementState::CONDUCTIVE_WIRE;
    }
};


struct OrGate : public LogicGate {
    static constexpr SDL_Color displayColor{ 0x99, 0x0, 0xFF, 0xFF };
    static constexpr const char* displayName = "OR Gate";

    OrGate(bool logicLevel = false, bool defaultLogicLevel = false) :LogicGate(logicLevel, defaultLogicLevel) {}

    // process a step of the simulation
    ElementState processStep(const AdjacentEnvironment& env) const {
        return getHighSignalCounts(env) != 0 ? ElementState::SOURCE : ElementState::CONDUCTIVE_WIRE;
    }
};


struct NandGate : public LogicGate {
    static constexpr SDL_Color displayColor{ 0x66, 0x88, 0xFF, 0xFF };
    static constexpr const char* displayName = "NAND Gate";

    NandGate(bool logicLevel = false, bool defaultLogicLevel = false) :LogicGate(logicLevel, defaultLogicLevel) {}

    // process a step of the simulation
    ElementState processStep(const AdjacentEnvironment& env) const {
        return getLowSignalCounts(env) != 0 ? ElementState::SOURCE : ElementState::CONDUCTIVE_WIRE;
    }
};


struct NorGate : public LogicGate {
    static constexpr SDL_Color displayColor{ 0x0, 0x88, 0xFF, 0xFF };
    static constexpr const char* displayName = "NOR Gate";

    NorGate(bool logicLevel = false, bool defaultLogicLevel = false) :LogicGate(logicLevel, defaultLogicLevel) {}

    // process a step of the simulation
    ElementState processStep(const AdjacentEnvironment& env) const {
        return getHighSignalCounts(env) == 0 ? ElementState::SOURCE : ElementState::CONDUCTIVE_WIRE;
    }
};


struct PositiveRelay : public Relay {
    static constexpr SDL_Color displayColor{ 0xFF, 0x99, 0, 0xFF };
    static constexpr const char* displayName = "Positive Relay";

    PositiveRelay(bool logicLevel = false, bool defaultLogicLevel = false) :Relay(logicLevel, defaultLogicLevel) {}

    // process a step of the simulation
    ElementState processStep(const AdjacentEnvironment& env) const {
        return getHighSignalCounts(env) != 0 ? ElementState::CONDUCTIVE_WIRE : ElementState::INSULATOR;
    }
};


struct NegativeRelay : public Relay {
    static constexpr SDL_Color displayColor{ 0xFF, 0x33, 0x0, 0xFF };
    static constexpr const char* displayName = "Negative Relay";

    NegativeRelay(bool logicLevel = false, bool defaultLogicLevel = false) :Relay(logicLevel, defaultLogicLevel) {}

    // process a step of the simulation
    ElementState processStep(const AdjacentEnvironment& env) const {
        return getLowSignalCounts(env) != 0 ? ElementState::CONDUCTIVE_WIRE : ElementState::INSULATOR;
    }
};
