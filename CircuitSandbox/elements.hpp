#pragma once

#include <SDL.h>
#include <algorithm>
#include <variant>
#include <memory>
#include <atomic>
#include <string>
#include "declarations.hpp"
#include "fileutils.hpp"

using namespace std::literals;

/**
 * This header file contains the definitions for all the elements in the game.
 */


struct Pencil {}; // base class for normal elements and the eraser
struct Element : public Pencil {}; // base class for elements

template <typename T>
struct ElementBase : public Element {
public:
    constexpr bool isSignal() const noexcept {
        return false;
    }

    constexpr bool isSignalReceiver() const noexcept {
        return false;
    }

    template <bool StartingState = false>
    SDL_Color computeDisplayColor() const noexcept {
        if (static_cast<const T&>(*this).template getLogicLevel<StartingState>()) {
            return SDL_Color{ static_cast<Uint8>(0xFF - (0xFF - T::displayColor.r) * 2 / 3), static_cast<Uint8>(0xFF - (0xFF - T::displayColor.g) * 2 / 3), static_cast<Uint8>(0xFF - (0xFF - T::displayColor.b) * 2 / 3), T::displayColor.a };
        }
        else {
            return SDL_Color{ static_cast<Uint8>(T::displayColor.r * 2 / 3), static_cast<Uint8>(T::displayColor.g * 2 / 3), static_cast<Uint8>(T::displayColor.b * 2 / 3), T::displayColor.a };
        }
    }

    bool operator==(const T& other) const noexcept {
        return true;
    }

    bool operator!=(const T& other) const noexcept {
        return !(*this == other);
    }

    template <typename Callback>
    void setDescription(Callback&& callback) const {
        std::forward<Callback>(callback)(T::displayName, T::displayColor);
    }
};

struct RenderLogicLevelElement {};

template <typename T>
struct RenderLogicLevelElementBase : public RenderLogicLevelElement {
protected:
    RenderLogicLevelElementBase(bool logicLevel, bool startingLogicLevel) noexcept : logicLevel(logicLevel), startingLogicLevel(startingLogicLevel) {}

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

struct SignalReceivingElement {};

template <typename T>
struct SignalReceivingElementBase : public ElementBase<T> {
public:
    constexpr bool isSignalReceiver() const noexcept {
        return true;
    }
};

// elements that use logic levels for compilation (meaning that it is not only for display purposes)
struct LogicLevelElement {};

template <typename T>
struct LogicLevelElementBase :public LogicLevelElement, public RenderLogicLevelElementBase<T> {
    LogicLevelElementBase(bool logicLevel, bool startingLogicLevel) noexcept : RenderLogicLevelElementBase<T>(logicLevel, startingLogicLevel) {}

    template <typename Callback>
    void setDescription(Callback&& callback) const {
        if (static_cast<const T&>(*this).getLogicLevel()) {
            std::forward<Callback>(callback)(T::displayName, T::displayColor, " [HIGH]", SDL_Color{ 0x66, 0xFF, 0x66, 0xFF });
        }
        else {
            std::forward<Callback>(callback)(T::displayName, T::displayColor, " [LOW]", SDL_Color{ 0x66, 0x66, 0x66, 0xFF });
        }
    }

    bool operator==(const T& other) const noexcept {
        return this->logicLevel == other.logicLevel && this->startingLogicLevel == other.startingLogicLevel;
    }

    bool operator!=(const T& other) const noexcept {
        return !(*this == other);
    }
};

struct LogicGate {};

template <typename T>
struct LogicGateBase : public LogicGate, public SignalReceivingElementBase<T>, public LogicLevelElementBase<T> {
protected:
    LogicGateBase(bool logicLevel, bool startingLogicLevel) noexcept : LogicLevelElementBase<T>(logicLevel, startingLogicLevel) {}

public:
    bool operator==(const T& other) const noexcept {
        return static_cast<const LogicLevelElementBase<T>&>(*this) == other;
    }

    bool operator!=(const T& other) const noexcept {
        return !(*this == other);
    }

    using LogicLevelElementBase<T>::setDescription;
};

struct Relay {};

template <typename T>
struct RelayBase : public Relay, public SignalReceivingElementBase<T>, public RenderLogicLevelElementBase<T> {
protected:
    RelayBase(bool logicLevel, bool startingLogicLevel, bool conductiveState, bool startingConductiveState) noexcept : RenderLogicLevelElementBase<T>(logicLevel, startingLogicLevel), conductiveState(conductiveState), startingConductiveState(startingConductiveState) {}
public:
    bool conductiveState;
    bool startingConductiveState;

    void reset() noexcept {
        conductiveState = startingConductiveState;
        RenderLogicLevelElementBase<T>::reset();
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

    bool operator==(const T& other) const noexcept {
        return conductiveState == other.conductiveState && startingConductiveState == other.startingConductiveState;
    }

    bool operator!=(const T& other) const noexcept {
        return !(*this == other);
    }

    template <typename Callback>
    void setDescription(Callback&& callback) const {
        if (static_cast<const T&>(*this).getConductiveState()) {
            std::forward<Callback>(callback)(T::displayName, T::displayColor, " [Conductive]", SDL_Color{ 0xFF, 0xFF, 0x66, 0xFF });
        }
        else {
            std::forward<Callback>(callback)(T::displayName, T::displayColor, " [Insulator]", SDL_Color{ 0x66, 0x66, 0x66, 0xFF });
        }
    }
};

struct CommunicatorElement{};

template <typename T, typename TCommunicator>
struct CommunicatorElementBase : public CommunicatorElement, public SignalReceivingElementBase<T>, public LogicLevelElementBase<T> {
protected:
    CommunicatorElementBase(bool logicLevel, bool startingLogicLevel, bool transmitState) : LogicLevelElementBase<T>(logicLevel, startingLogicLevel), transmitState(transmitState) {}
public:
    bool transmitState; // filled in by Simulator::takeSnapshot()

    // shared communicator instance
    // after action is ended, this should point to a valid communicator instance (filled in by Simulator::compile())
    std::shared_ptr<TCommunicator> communicator;

    bool operator==(const T& other) const noexcept {
        return static_cast<const LogicLevelElementBase<T>&>(*this) == other;
    }

    bool operator!=(const T& other) const noexcept {
        return !(*this == other);
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

struct ConductiveWire : public ElementBase<ConductiveWire>, public RenderLogicLevelElementBase<ConductiveWire> {
    static constexpr SDL_Color displayColor{0x99, 0x99, 0x99, 0xFF};
    static constexpr const char* displayName = "Conductive Wire";

    ConductiveWire(bool logicLevel = false, bool startingLogicLevel = false) noexcept : RenderLogicLevelElementBase<ConductiveWire>(logicLevel, startingLogicLevel) {}
};

struct InsulatedWire : public ElementBase<InsulatedWire>, public RenderLogicLevelElementBase<InsulatedWire> {
    // previously was: {0xCC, 0x99, 0x66, 0xFF};
    static constexpr SDL_Color displayColor{0, 0x66, 0x44, 0xFF};
    static constexpr const char* displayName = "Insulated Wire";

    InsulatedWire(bool logicLevel = false, bool startingLogicLevel = false) noexcept : RenderLogicLevelElementBase<InsulatedWire>(logicLevel, startingLogicLevel) {}
};


struct Signal : public ElementBase<Signal>, public RenderLogicLevelElementBase<Signal> {
    static constexpr SDL_Color displayColor{ 0xFF, 0xFF, 0, 0xFF };
    static constexpr const char* displayName = "Signal";

    Signal(bool logicLevel = false, bool startingLogicLevel = false) noexcept : RenderLogicLevelElementBase<Signal>(logicLevel, startingLogicLevel) {}

    constexpr bool isSignal() const {
        return true;
    }
};


struct Source : public ElementBase<Source> {
    static constexpr SDL_Color displayColor{ 0, 0xFF, 0, 0xFF };
    static constexpr const char* displayName = "Source";

    Source() noexcept {}

    template <bool StartingState = false>
    bool getLogicLevel() const noexcept {
        return true;
    }

    void reset() const noexcept {}
};


struct AndGate : public LogicGateBase<AndGate> {
    static constexpr SDL_Color displayColor{ 0xFF, 0x0, 0xFF, 0xFF };
    static constexpr const char* displayName = "AND Gate";

    AndGate(bool logicLevel = false, bool startingLogicLevel = false) noexcept : LogicGateBase<AndGate>(logicLevel, startingLogicLevel) {}
};


struct OrGate : public LogicGateBase<OrGate> {
    static constexpr SDL_Color displayColor{ 0x99, 0x0, 0xFF, 0xFF };
    static constexpr const char* displayName = "OR Gate";

    OrGate(bool logicLevel = false, bool startingLogicLevel = false) noexcept : LogicGateBase<OrGate>(logicLevel, startingLogicLevel) {}
};


struct NandGate : public LogicGateBase<NandGate> {
    static constexpr SDL_Color displayColor{ 0x66, 0x88, 0xFF, 0xFF };
    static constexpr const char* displayName = "NAND Gate";

    NandGate(bool logicLevel = false, bool startingLogicLevel = false) noexcept : LogicGateBase<NandGate>(logicLevel, startingLogicLevel) {}
};


struct NorGate : public LogicGateBase<NorGate> {
    static constexpr SDL_Color displayColor{ 0x0, 0x88, 0xFF, 0xFF };
    static constexpr const char* displayName = "NOR Gate";

    NorGate(bool logicLevel = false, bool startingLogicLevel = false) noexcept : LogicGateBase<NorGate>(logicLevel, startingLogicLevel) {}
};


struct PositiveRelay : public RelayBase<PositiveRelay> {
    static constexpr SDL_Color displayColor{ 0xFF, 0x99, 0, 0xFF };
    static constexpr const char* displayName = "Positive Relay";

    PositiveRelay(bool logicLevel = false, bool startingLogicLevel = false, bool conductiveState = false, bool startingConductiveState = false) noexcept : RelayBase<PositiveRelay>(logicLevel, startingLogicLevel, conductiveState, startingConductiveState) {}
};


struct NegativeRelay : public RelayBase<NegativeRelay> {
    static constexpr SDL_Color displayColor{ 0xFF, 0x33, 0x0, 0xFF };
    static constexpr const char* displayName = "Negative Relay";

    NegativeRelay(bool logicLevel = false, bool startingLogicLevel = false, bool conductiveState = false, bool startingConductiveState = false) noexcept : RelayBase<NegativeRelay>(logicLevel, startingLogicLevel, conductiveState, startingConductiveState) {}
};

struct ScreenCommunicatorElement : public CommunicatorElementBase<ScreenCommunicatorElement, ScreenCommunicator> {
    static constexpr SDL_Color displayColor{ 0xFF, 0, 0, 0xFF };
    static constexpr const char* displayName = "Screen I/O";

    ScreenCommunicatorElement(bool logicLevel = false, bool startingLogicLevel = false, bool transmitState = false) noexcept : CommunicatorElementBase<ScreenCommunicatorElement, ScreenCommunicator>(logicLevel, startingLogicLevel, transmitState) {}

    template <bool StartingState = false>
    SDL_Color computeDisplayColor() const noexcept {
        if (transmitState) {
            return SDL_Color{ static_cast<Uint8>(0xFF - (0xFF - ScreenCommunicatorElement::displayColor.r) * 1 / 3), static_cast<Uint8>(0xFF - (0xFF - ScreenCommunicatorElement::displayColor.g) * 1 / 3), static_cast<Uint8>(0xFF - (0xFF - ScreenCommunicatorElement::displayColor.b) * 1 / 3), ScreenCommunicatorElement::displayColor.a };
        }
        else {
            return SDL_Color{ static_cast<Uint8>(ScreenCommunicatorElement::displayColor.r * 1 / 3), static_cast<Uint8>(ScreenCommunicatorElement::displayColor.g * 1 / 3), static_cast<Uint8>(ScreenCommunicatorElement::displayColor.b * 1 / 3), ScreenCommunicatorElement::displayColor.a };
        }
    }
};

struct FileInputCommunicatorElement : public CommunicatorElementBase<FileInputCommunicatorElement, FileInputCommunicator> {
    static constexpr SDL_Color displayColor{ 0xFF, 0, 0, 0xFF };
    static constexpr const char* displayName = "File Input";

    FileInputCommunicatorElement(bool logicLevel = false, bool startingLogicLevel = false, bool transmitState = false) noexcept : CommunicatorElementBase<FileInputCommunicatorElement, FileInputCommunicator>(logicLevel, startingLogicLevel, transmitState) {}

    template <bool StartingState = false>
    SDL_Color computeDisplayColor() const noexcept {
        if (transmitState) {
            return SDL_Color{ static_cast<Uint8>(0xFF - (0xFF - displayColor.r) * 2 / 3), static_cast<Uint8>(0xFF - (0xFF - displayColor.g) * 2 / 3), static_cast<Uint8>(0xFF - (0xFF - displayColor.b) * 2 / 3), displayColor.a };
        }
        else {
            return SDL_Color{ static_cast<Uint8>(displayColor.r * 2 / 3), static_cast<Uint8>(displayColor.g * 2 / 3), static_cast<Uint8>(displayColor.b * 2 / 3), displayColor.a };
        }
    }
};
