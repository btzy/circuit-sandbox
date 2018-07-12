#pragma once

#include <vector>
#include <variant>

#include "simulator.hpp"

/**
 * This header file contains the things that are only necessary for the simulation compiler
 */

template <typename Callback>
inline auto callback_as_template(int32_t x, Callback&& callback) {
    switch (x) {
    case 0:
        return std::forward<Callback>(callback)(std::integral_constant<int32_t, 0>{});
    case 1:
        return std::forward<Callback>(callback)(std::integral_constant<int32_t, 1>{});
    case 2:
        return std::forward<Callback>(callback)(std::integral_constant<int32_t, 2>{});
    case 3:
        return std::forward<Callback>(callback)(std::integral_constant<int32_t, 3>{});
    case 4:
        return std::forward<Callback>(callback)(std::integral_constant<int32_t, 4>{});
    }
}

template <template <size_t> typename Gate>
struct CompilerGatePack {
    std::tuple<std::vector<Gate<0>>, std::vector<Gate<1>>, std::vector<Gate<2>>, std::vector<Gate<3>>, std::vector<Gate<4>>> data;
    void emplace(const std::vector<int32_t>& inputComponents, int32_t outputComponent) {
        callback_as_template(inputComponents.size(), [&](auto integer_t) {
            auto& target = std::get<decltype(integer_t)::value>(data).emplace_back();
            std::copy(inputComponents.begin(), inputComponents.begin() + decltype(integer_t)::value, target.inputComponents.begin());
            target.outputComponent = outputComponent;
        });
    }
};
struct CompilerGates {
    CompilerGatePack<Simulator::SimulatorAndGate> andGate;
    CompilerGatePack<Simulator::SimulatorOrGate> orGate;
    CompilerGatePack<Simulator::SimulatorNandGate> nandGate;
    CompilerGatePack<Simulator::SimulatorNorGate> norGate;
    template <typename Element> void emplace(const std::vector<int32_t>& inputComponents, int32_t outputComponent) {
        if (std::is_same_v<AndGate, Element>) {
            andGate.emplace(inputComponents, outputComponent);
        }
        else if (std::is_same_v<OrGate, Element>) {
            orGate.emplace(inputComponents, outputComponent);
        }
        else if (std::is_same_v<NandGate, Element>) {
            nandGate.emplace(inputComponents, outputComponent);
        }
        else if (std::is_same_v<NorGate, Element>) {
            norGate.emplace(inputComponents, outputComponent);
        }
    }
    template <typename Callback>
    void forEachGate(Simulator::Gates& gates, Callback callback) {
        callback(gates.andGate, andGate);
        callback(gates.orGate, orGate);
        callback(gates.nandGate, nandGate);
        callback(gates.norGate, norGate);
    }
};
template <template <size_t> typename Relay>
struct CompilerRelayPack {
    std::tuple<std::vector<Relay<0>>, std::vector<Relay<1>>, std::vector<Relay<2>>, std::vector<Relay<3>>, std::vector<Relay<4>>> data;
    void emplace(const std::vector<int32_t>& inputComponents, int32_t outputRelayPixel) {
        callback_as_template(inputComponents.size(), [&](auto integer_t) {
            auto& target = std::get<decltype(integer_t)::value>(data).emplace_back();
            std::copy(inputComponents.begin(), inputComponents.begin() + decltype(integer_t)::value, target.inputComponents.begin());
            target.outputRelayPixel = outputRelayPixel;
        });
    }
};
struct CompilerRelays {
    CompilerRelayPack<Simulator::SimulatorPositiveRelay> positiveRelay;
    CompilerRelayPack<Simulator::SimulatorNegativeRelay> negativeRelay;
    template <typename Element> void emplace(const std::vector<int32_t>& inputComponents, int32_t outputRelayPixel) {
        if (std::is_same_v<PositiveRelay, Element>) {
            positiveRelay.emplace(inputComponents, outputRelayPixel);
        }
        else if (std::is_same_v<NegativeRelay, Element>) {
            negativeRelay.emplace(inputComponents, outputRelayPixel);
        }
    }
    template <typename Callback>
    void forEachRelay(Simulator::Relays& relays, Callback callback) {
        callback(relays.positiveRelay, positiveRelay);
        callback(relays.negativeRelay, negativeRelay);
    }
};
struct CompilerComponent {
    std::vector<int32_t> adjRelayPixels;
};
struct CompilerStaticData {
    // for all data that does not change after compilation

    // data about sources
    std::vector<Simulator::SimulatorSource> sources;

    // data about which component each gate maps to
    CompilerGates logicGates;

    // data about which RelayPixel each relay maps to
    CompilerRelays relays;

    // data about communicators
    std::vector<Simulator::SimulatorCommunicator> communicators;

    // list of components
    std::vector<CompilerComponent> components;
    // list of relay pixels (relay pixels have one-to-one correspondence to relays)
    std::vector<Simulator::RelayPixel> relayPixels;

    // state mapping
    ext::heap_matrix<Simulator::StaticData::DisplayedPixel> pixels;
};

template <typename ElementVariant>
inline bool isFloodfillableElement(ElementVariant&& element) noexcept {
    return std::visit([&](const auto& element) {
        using ElementType = std::decay_t<decltype(element)>;
        if constexpr (std::is_same_v<Signal, ElementType> || std::is_base_of_v<LogicGate, ElementType> || std::is_base_of_v<CommunicatorElement, ElementType> || std::is_same_v<ConductiveWire, ElementType> || std::is_same_v<InsulatedWire, ElementType> || std::is_same_v<Source, ElementType>) {
            return true;
        }
        else return false;
    }, std::forward<ElementVariant>(element));
}

template <typename ElementVariant>
inline bool isFloodfillableUsefulElement(ElementVariant&& element) noexcept {
    return std::visit([&](const auto& element) {
        using ElementType = std::decay_t<decltype(element)>;
        if constexpr (std::is_same_v<Signal, ElementType> || std::is_base_of_v<LogicGate, ElementType> || std::is_base_of_v<CommunicatorElement, ElementType> || std::is_same_v<Source, ElementType>) {
            return true;
        }
        else return false;
    }, std::forward<ElementVariant>(element));
}

template <typename ElementVariant>
inline bool isRelayElement(ElementVariant&& element) noexcept {
    return std::visit([&](const auto& element) {
        using ElementType = std::decay_t<decltype(element)>;
        if constexpr (std::is_base_of_v<Relay, ElementType>) {
            return true;
        }
        else return false;
    }, std::forward<ElementVariant>(element));
}

template <typename ElementVariant>
inline bool isLogicGateElement(ElementVariant&& element) noexcept {
    return std::visit([&](const auto& element) {
        using ElementType = std::decay_t<decltype(element)>;
        if constexpr (std::is_base_of_v<LogicGate, ElementType>) {
            return true;
        }
        else return false;
    }, std::forward<ElementVariant>(element));
}

template <typename T, T First, T Second>
struct integral_pair {
    static constexpr T first = First;
    static constexpr T second = Second;
};
