/*
 * Circuit Sandbox
 * Copyright 2018 National University of Singapore <enterprise@nus.edu.sg>
 *
 * This file is part of Circuit Sandbox.
 * Circuit Sandbox is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 * Circuit Sandbox is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with Circuit Sandbox.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <memory>
#include <utility>
#include <thread>
#include <chrono>
#include <type_traits>
#include <stack>
#include <unordered_map>
#include <algorithm>
#include <numeric>
#include <cassert>

#include "simulator.hpp"
#include "simulator_compile.hpp"
#include "elements.hpp"
#include "tag_tuple.hpp"
#include "visitor.hpp"
#include "screencommunicator.hpp"
#include "fileinputcommunicator.hpp"
#include "fileoutputcommunicator.hpp"

Simulator::~Simulator() {
    if (running())stop();
}


void Simulator::compile(CanvasState& gameState) {
    // temporary compiler data (unpacked representation)
    CompilerStaticData compilerStaticData;
    compilerStaticData.pixels = ext::heap_matrix<Simulator::StaticData::DisplayedPixel>(gameState.size());

    // flags to tell us whether each pixel has been processed (originally everything is unprocessed)
    // horizontal followed by vertical
    ext::heap_matrix<std::array<bool, 2>> visited(gameState.size());
    visited.fill({ false, false });

    // store whether each point is a relay
    for (int32_t y = 0; y != gameState.height(); ++y) {
        for (int32_t x = 0; x != gameState.width(); ++x) {
            ext::point pt{ x, y };
            std::visit([&](const auto& element) {
                using ElementType = std::decay_t<decltype(element)>;
                if constexpr(std::is_base_of_v<Relay, ElementType>) {
                    compilerStaticData.pixels[pt].type = StaticData::DisplayedPixel::PixelType::RELAY;
                }
                else if constexpr(std::is_base_of_v<CommunicatorElement, ElementType>) {
                    compilerStaticData.pixels[pt].type = StaticData::DisplayedPixel::PixelType::COMMUNICATOR;
                }
                else if constexpr(std::is_base_of_v<Element, ElementType>) {
                    compilerStaticData.pixels[pt].type = StaticData::DisplayedPixel::PixelType::COMPONENT;
                }
                else {
                    compilerStaticData.pixels[pt].type = StaticData::DisplayedPixel::PixelType::EMPTY;
                }
            }, gameState[pt]);
            compilerStaticData.pixels[pt].index[0] = compilerStaticData.pixels[pt].index[1] = -1;
        }
    }

    // populate all the components first
    for (int32_t y = 0; y != gameState.height(); ++y) {
        for (int32_t x = 0; x != gameState.width(); ++x) {
            ext::point pt{ x, y };

            if (isFloodfillableElement(gameState[pt])) {
                for (int dir = 0; dir < 2; ++dir) {
                    if (!visited[pt][dir]) {
                        // since we have not visited this pixel yet, we do a flood fill algorithm to find all pixels that are connected

                        // whether it might be useful to process this element (an optimization to remove useless components)
                        bool componentIsUseful = false;

                        std::vector<std::pair<ext::point, int>> updatedPixels;

                        // flood fill
                        {
                            std::stack<std::pair<ext::point, int>> floodStack;
                            floodStack.emplace(pt, dir);

                            while (!floodStack.empty()) {
                                // retrieve topmost point
                                auto[currPt, currDir] = floodStack.top();
                                floodStack.pop();

                                // ignore if already processed
                                if (visited[currPt][currDir]) continue;

                                // set visited
                                visited[currPt][currDir] = true;

                                // set its component index
                                updatedPixels.emplace_back(currPt, currDir);

                                // check if component is useful
                                if (!componentIsUseful && isFloodfillableUsefulElement(gameState[currPt])) {
                                    componentIsUseful = true;
                                }

                                // if its not insulated wire, push the opposite direction
                                if (!std::holds_alternative<InsulatedWire>(gameState[currPt]) && !visited[currPt][currDir ^ 1]) {
                                    floodStack.emplace(currPt, currDir ^ 1);
                                }

                                // visit the adjacent pixels
                                using positive_one_t = std::integral_constant<int32_t, 1>;
                                using negative_one_t = std::integral_constant<int32_t, -1>;
                                using directions_t = ext::tag_tuple<negative_one_t, positive_one_t>;
                                // verbose capture due to clang 6.0.0 issue with structured bindings
                                directions_t::for_each([&, &currPt = currPt, &currDir = currDir](auto direction_tag_t, auto) {
                                    ext::point newPt = currPt;
                                    if (currDir == 0) {
                                        newPt.x += decltype(direction_tag_t)::type::value;
                                    }
                                    else {
                                        newPt.y += decltype(direction_tag_t)::type::value;
                                    }
                                    if (gameState.contains(newPt) && !visited[newPt][currDir] &&
                                        isFloodfillableElement(gameState[newPt]) &&
                                        !(isSignalReceiver(gameState[newPt]) && isSignal(gameState[currPt])) &&
                                        !(isSignal(gameState[newPt]) && isSignalReceiver(gameState[currPt]))) {
                                        floodStack.emplace(newPt, currDir);
                                    }
                                    if (!componentIsUseful && gameState.contains(newPt) && isRelayElement(gameState[newPt])) {
                                        componentIsUseful = true;
                                    }
                                });
                                
                            }
                        }

                        if (componentIsUseful) {
                            // assign it the next component index
                            int32_t componentIndex = compilerStaticData.components.size();
                            // commit the changes
                            for (auto [currPt, currDir] : updatedPixels) {
                                compilerStaticData.pixels[currPt].index[currDir] = componentIndex;
                            }
                            // register this component
                            compilerStaticData.components.emplace_back();
                        }
                    }
                }
            }
        }
    }

    // directions for flood fill
    using up = integral_pair<int32_t, 0, -1>;
    using down = integral_pair<int32_t, 0, 1>;
    using left = integral_pair<int32_t, -1, 0>;
    using right = integral_pair<int32_t, 1, 0>;
    using directions_t = ext::tag_tuple<up, down, left, right>;

    // next, we populate the sources and logic gates
    for (int32_t y = 0; y != gameState.height(); ++y) {
        for (int32_t x = 0; x != gameState.width(); ++x) {
            ext::point pt{ x, y };
            std::visit([&](const auto& element) {
                using ElementType = std::decay_t<decltype(element)>;
                if constexpr(std::is_same_v<Source, ElementType>) {
                    auto& source = compilerStaticData.sources.emplace_back();
                    source.outputComponent = compilerStaticData.pixels[pt].index[0]; // for sources, index 0 and 1 should be the same
                    assert(source.outputComponent >= 0 && source.outputComponent < static_cast<int32_t>(compilerStaticData.components.size()));
                }
                else if constexpr (std::is_base_of_v<LogicGate, ElementType>) {
                    int32_t outputComponent = compilerStaticData.pixels[pt].index[0];
                    assert(outputComponent >= 0 && outputComponent < static_cast<int32_t>(compilerStaticData.components.size()));
                    // Note: can optimize if heap allocation for the std::vector is slow
                    std::vector<int32_t> inputComponents;
                    
                    directions_t::for_each([&](auto direction_tag_t, auto) {
                        ext::point newPt = pt;
                        newPt.x += decltype(direction_tag_t)::type::first;
                        newPt.y += decltype(direction_tag_t)::type::second;
                        if (gameState.contains(newPt) && isSignal(gameState[newPt])) {
                            assert(compilerStaticData.pixels[newPt].index[0] >= 0 && compilerStaticData.pixels[newPt].index[0] < static_cast<int32_t>(compilerStaticData.components.size()));
                            inputComponents.emplace_back(compilerStaticData.pixels[newPt].index[0]);
                        }
                    });
                    compilerStaticData.logicGates.emplace<ElementType>(inputComponents, outputComponent);
                }
            }, gameState[pt]);
        }
    }

    // next, we populate the relays (and spawn new components if two relays are adjacent)
    for (int32_t y = 0; y != gameState.height(); ++y) {
        for (int32_t x = 0; x != gameState.width(); ++x) {
            ext::point pt{ x, y };
            visited[pt] = { true, true };
            std::visit([&](const auto& element) {
                using ElementType = std::decay_t<decltype(element)>;
                if constexpr (std::is_base_of_v<Relay, ElementType>) {
                    int32_t outputRelayPixelIndex = compilerStaticData.relayPixels.size();
                    auto& relayPixel = compilerStaticData.relayPixels.emplace_back();
                    relayPixel.numAdjComponents = 0;
                    // TODO: optimize if heap allocation is slow
                    std::vector<int32_t> inputComponents;
                    directions_t::for_each([&](auto direction_tag_t, auto) {
                        ext::point newPt = pt;
                        int32_t dir = (decltype(direction_tag_t)::type::second != 0);
                        newPt.x += decltype(direction_tag_t)::type::first;
                        newPt.y += decltype(direction_tag_t)::type::second;
                        if (gameState.contains(newPt)) {
                            auto& element = gameState[newPt];
                            if (isSignal(element)) {
                                inputComponents.emplace_back(compilerStaticData.pixels[newPt].index[0]);
                            }
                            else if (isFloodfillableElement(element)) {
                                assert(compilerStaticData.pixels[newPt].index[dir] >= 0 && compilerStaticData.pixels[newPt].index[dir] < static_cast<int32_t>(compilerStaticData.components.size()));
                                relayPixel.adjComponents[relayPixel.numAdjComponents++] = compilerStaticData.pixels[newPt].index[dir];
                                compilerStaticData.components[compilerStaticData.pixels[newPt].index[dir]].adjRelayPixels.emplace_back(outputRelayPixelIndex);
                            }
                            else if (isRelayElement(element)) {
                                // special case where two relays are adjacent
                                // spawn a new component between them, if the other component is already processed
                                if (visited[newPt][0]) { // this ensures that we only spawn the new component once per pair of adjacent relays
                                    assert(compilerStaticData.pixels[newPt].index[0] >= 0 && compilerStaticData.pixels[newPt].index[0] < static_cast<int32_t>(compilerStaticData.relayPixels.size()));
                                    int32_t componentIndex = compilerStaticData.components.size();
                                    auto& newComponent = compilerStaticData.components.emplace_back();
                                    newComponent.adjRelayPixels.emplace_back(outputRelayPixelIndex);
                                    relayPixel.adjComponents[relayPixel.numAdjComponents++] = componentIndex;
                                    newComponent.adjRelayPixels.emplace_back(compilerStaticData.pixels[newPt].index[0]);
                                    auto& otherRelayPixel = compilerStaticData.relayPixels[compilerStaticData.pixels[newPt].index[0]];
                                    otherRelayPixel.adjComponents[otherRelayPixel.numAdjComponents++] = componentIndex;
                                }
                            }
                        }
                    });
                    compilerStaticData.relays.emplace<ElementType>(inputComponents, outputRelayPixelIndex);
                    compilerStaticData.pixels[pt].index[0] = compilerStaticData.pixels[pt].index[1] = outputRelayPixelIndex;
                }
            }, gameState[pt]);
        }
    }

    /*
    === Filling in the communicators ===
    Makes adjacent communicator elements share the same backing communicator object, attempting to reuse existing objects where possible.
    Algorithm attempts to make the assignments as intuitive as possible to the user.

    How the algorithm works:
    ("communicator" refers to the actual ScreenCommunicator, not a pixel)
    ("component" is a set of pixels that are of the same pixel type and are connected to each other)

    1) For each communicator that is split between multiple components:
    1.1) Select the component with the largest number of pixels of this communicator
    1.2) Nullify pixels from other components that are from this communicator
    2) For each component whose pixels are not all null (not bound to any communicator):
    2.1) Set all pixels of that component to the communicator with the largest number of pixels in that component

    Algorithm is expected to run in O(N) where N is the number of pixels on the canvas (with some constant factor for hashing).
    */

    // types of communicators that we recognize:
    using CommunicatorTypes_t = ext::tag_tuple<ScreenCommunicator, FileInputCommunicator, FileOutputCommunicator>;

    // Describes the communicator index of each pixel (if it is a communicator)
    ext::heap_matrix<int32_t> communicatorComponentIndices(gameState.size());
    communicatorComponentIndices.fill(-1); // no assigned communicator for all pixels at first
    
    // the cumulative number of communicators used
    int32_t communicatorTypeComponentOffset = 0;

    CommunicatorTypes_t::for_each([&](auto CommunicatorTypeTag, auto) {
        using CommunicatorType = typename decltype(CommunicatorTypeTag)::type;

        if constexpr(std::is_same_v<ScreenCommunicator, CommunicatorType>) {
            staticData.screenCommunicatorStartIndex = communicatorTypeComponentOffset;
        }

        // the number of communicators of this particular type
        int32_t communicatorComponentCount = 0;

        // pair<communicator component index, count> , keep sorted by communicator component index, expected to be quite small so we use vector
        std::unordered_map<std::shared_ptr<CommunicatorType>, std::vector<std::pair<int32_t, int32_t>>> communicatorMapToCommunicatorComponent;

        // First, we extract all the connected communicator components and assign each component a unique index
        for (int32_t y = 0; y != gameState.height(); ++y) {
            for (int32_t x = 0; x != gameState.width(); ++x) {
                ext::point pt{ x, y };
                std::visit([&](const auto& element) {
                    using ElementType = std::decay_t<decltype(element)>;
                    if constexpr (std::is_same_v<typename CommunicatorType::element_t, ElementType>) {
                        if (communicatorComponentIndices[pt] == -1) {
                            // this is a new communicator

                            // register the communicator (get an index for it)
                            int32_t communicatorIndex = communicatorComponentCount++;

                            // use flood fill to assign all adjacent communicators (of the same type) the same index
                            std::stack<ext::point> floodStack;
                            floodStack.emplace(pt);
                            while (!floodStack.empty()) {
                                // retrieve topmost point
                                ext::point currPt = floodStack.top();
                                floodStack.pop();

                                // ignore if already processed
                                if (communicatorComponentIndices[currPt] != -1) continue;

                                // assign communicator index
                                communicatorComponentIndices[currPt] = communicatorIndex;

                                // store the communicator in the map
                                std::vector<std::pair<int32_t, int32_t>>& mapEntry = communicatorMapToCommunicatorComponent.emplace(element.communicator, std::vector<std::pair<int32_t, int32_t>>()).first->second;
                                auto vectorEntry = std::lower_bound(mapEntry.begin(), mapEntry.end(), communicatorIndex, [](const std::pair<int32_t, int32_t>& entry, const int32_t& searchValue) {
                                    return entry.first < searchValue;
                                });
                                if (vectorEntry != mapEntry.end() && vectorEntry->first == communicatorIndex) {
                                    vectorEntry->second++;
                                }
                                else {
                                    mapEntry.emplace(vectorEntry, communicatorIndex, 1);
                                }

                                // submit unprocessed adjacent communicators to stack
                                directions_t::for_each([&](auto direction_tag_t, auto) {
                                    ext::point newPt = currPt;
                                    newPt.x += decltype(direction_tag_t)::type::first;
                                    newPt.y += decltype(direction_tag_t)::type::second;
                                    if (gameState.contains(newPt) && std::holds_alternative<ElementType>(gameState[newPt])) {
                                        floodStack.emplace(newPt);
                                    }
                                });
                            }
                        }
                    }
                }, gameState[pt]);
            }
        }

        // stores the most voted communicator for each communicator component
        // pair<communicator, vote count>
        std::unique_ptr<std::pair<std::shared_ptr<CommunicatorType>, int32_t>[]> communicatorComponents = std::make_unique<std::pair<std::shared_ptr<CommunicatorType>, int32_t>[]>(communicatorComponentCount);
        // originally, vote count is zero
        std::fill_n(communicatorComponents.get(), communicatorComponentCount, std::make_pair(nullptr, 0));

        // Second, keep on the communicator component with maximum pixels from each communicator
        // Third, elect a communicator for each communicator component, or create a new communicator if there are none to choose from (interleaved with second step)
        for (auto& mapEntry : communicatorMapToCommunicatorComponent) {
            auto& vect = mapEntry.second;

            // sort by the counts
            sort(vect.begin(), vect.end(), [](const std::pair<int32_t, int32_t>& a, const std::pair<int32_t, int32_t>& b) {
                return a.second < b.second;
            });

            // can we outvote the current leader?
            if (communicatorComponents[vect.front().first].second < vect.front().second) {
                // outvoted! keep the new component instead
                communicatorComponents[vect.front().first] = std::make_pair(mapEntry.first, vect.front().second);
            }
        }

        // Fourth, create communicator objects for null communicators, and assign communicator indices
        compilerStaticData.communicators.reserve(communicatorTypeComponentOffset + communicatorComponentCount);
        for (int32_t index = 0; index != communicatorComponentCount; ++index) {
            if (communicatorComponents[index].first == nullptr) {
                communicatorComponents[index].first = std::make_shared<CommunicatorType>();
            }
            communicatorComponents[index].first->communicatorIndex = communicatorTypeComponentOffset + index;
            communicatorComponents[index].first->refresh();
            // spawn the communicator in the static data
            auto& comm = compilerStaticData.communicators.emplace_back();
            comm.communicator = communicatorComponents[index].first.get();
        }

        // Fifth, fill in the input and output components for the compiler static data
        for (int32_t y = 0; y != gameState.height(); ++y) {
            for (int32_t x = 0; x != gameState.width(); ++x) {
                ext::point pt{ x, y };
                std::visit([&](auto& element) {
                    using ElementType = std::decay_t<decltype(element)>;
                    if constexpr (std::is_same_v<typename CommunicatorType::element_t, ElementType>) {
                        int32_t outputComponent = compilerStaticData.pixels[pt].index[0];
                        assert(outputComponent >= 0 && outputComponent < static_cast<int32_t>(compilerStaticData.components.size()));
                        int32_t typeLocalCommunicatorIndex = communicatorComponentIndices[pt];
                        auto& communicatorObj = compilerStaticData.communicators[communicatorTypeComponentOffset + typeLocalCommunicatorIndex];

                        // set the output components
                        communicatorObj.outputComponent = outputComponent; // will be overwritten many times, but it's always the same outputComponent.
                        if (element.communicator != communicatorComponents[typeLocalCommunicatorIndex].first) {
                            // currently the element is storing the wrong communicator, so we update it
                            element.communicator = communicatorComponents[typeLocalCommunicatorIndex].first;
                        }

                        // add all the input components for this communicator
                        // note that there might be duplicate input components, because each communicator spans multiple pixels
                        // we de-duplicate it later
                        directions_t::for_each([&](auto direction_tag_t, auto) {
                            ext::point newPt = pt;
                            newPt.x += decltype(direction_tag_t)::type::first;
                            newPt.y += decltype(direction_tag_t)::type::second;
                            if (gameState.contains(newPt) && isSignal(gameState[newPt])) {
                                assert(compilerStaticData.pixels[newPt].index[0] >= 0 && compilerStaticData.pixels[newPt].index[0] < compilerStaticData.components.size());
                                communicatorObj.inputComponents.emplace_back(compilerStaticData.pixels[newPt].index[0]);
                            }
                        });
                    }
                }, gameState[pt]);
            }
        }

        // Sixth, update the number of communicators of this type
        communicatorTypeComponentOffset += communicatorComponentCount;

        if constexpr(std::is_same_v<ScreenCommunicator, CommunicatorType>) {
            staticData.screenCommunicatorEndIndex = communicatorTypeComponentOffset;
        }
    });

    assert(communicatorTypeComponentOffset == static_cast<int32_t>(compilerStaticData.communicators.size()));

    // Seventh, de-duplicate the communicators' input components
    for (SimulatorCommunicator& comm : compilerStaticData.communicators) {
        auto& inputs = comm.inputComponents;
        // sort the input components
        std::sort(inputs.begin(), inputs.end());
        // erase the consecutive duplicates
        inputs.erase(std::unique(inputs.begin(), inputs.end()), inputs.end());
        // free unnecessary memory (because this vector will be *moved* to the real static data later)
        inputs.shrink_to_fit();
    }

    // Eighth, prepare the received data storage and clear the input queue
    screenInputQueue.clear();

    // === generate the fixed (packed) representation from the unpacked representation ===

    // sources
    staticData.sources.update(std::move(compilerStaticData.sources));

    // logic gates
    compilerStaticData.logicGates.forEachGate(staticData.logicGates, [&](auto& gate, const auto& compilerGate) {
        using indices_t = ext::tag_tuple<std::integral_constant<int32_t, 0>, std::integral_constant<int32_t, 1>, std::integral_constant<int32_t, 2>, std::integral_constant<int32_t, 3>, std::integral_constant<int32_t, 4>>;
        indices_t::for_each([&](const auto index_tag, auto) {
            constexpr int32_t Index = decltype(index_tag)::type::value;
            std::get<Index>(gate.data).update(std::move(std::get<Index>(compilerGate.data)));
        });
    });
    
    // relays
    compilerStaticData.relays.forEachRelay(staticData.relays, [&](auto& relay, const auto& compilerRelay) {
        using indices_t = ext::tag_tuple<std::integral_constant<int32_t, 0>, std::integral_constant<int32_t, 1>, std::integral_constant<int32_t, 2>, std::integral_constant<int32_t, 3>, std::integral_constant<int32_t, 4>>;
        indices_t::for_each([&](const auto index_tag, auto) {
            constexpr int32_t Index = decltype(index_tag)::type::value;
            std::get<Index>(relay.data).update(std::move(std::get<Index>(compilerRelay.data)));
        });
    });

    // communicators
    staticData.communicators.update(std::move(compilerStaticData.communicators));

    // components
    int32_t adjComponentListSize = std::accumulate(compilerStaticData.components.begin(), compilerStaticData.components.end(), 0, [](const int32_t& prev, const CompilerComponent& curr) {
        return prev + static_cast<int32_t>(curr.adjRelayPixels.size());
    });

    staticData.adjComponentList.resize(adjComponentListSize);

    staticData.components.resize(compilerStaticData.components.size());
    int32_t offset = 0;
    std::transform(compilerStaticData.components.begin(), compilerStaticData.components.end(), staticData.components.begin(), [&](const CompilerComponent& old) {
        int32_t newBegin = offset;
        std::copy(old.adjRelayPixels.begin(), old.adjRelayPixels.end(), staticData.adjComponentList.begin() + newBegin);
        offset += static_cast<int32_t>(old.adjRelayPixels.size());
        return Simulator::Component{ newBegin, offset };
    });

    staticData.relayPixels.update(std::move(compilerStaticData.relayPixels));

    staticData.pixels = std::move(compilerStaticData.pixels);


    // === dynamic state ===
    // sets everything to false by default
    DynamicData dynamicData(staticData.components.size, staticData.relayPixels.size, staticData.communicators.size);

    // fill from all the currently sources and logic gates,
    // and fill the conductive state from the relays
    for (int32_t y = 0; y != gameState.height(); ++y) {
        for (int32_t x = 0; x != gameState.width(); ++x) {
            ext::point pt{ x, y };
            std::visit([&](const auto& element) {
                using ElementType = std::decay_t<decltype(element)>;
                if constexpr (std::is_same_v<Source, ElementType>) {
                    int32_t outputComponent = staticData.pixels[pt].index[0];
                    assert(outputComponent >= 0 && outputComponent < staticData.components.size);
                    dynamicData.componentLogicLevels[outputComponent] = true;
                }
                if constexpr (std::is_base_of_v<LogicLevelElement, ElementType>) {
                    if (element.logicLevel) {
                        int32_t outputComponent = staticData.pixels[pt].index[0];
                        assert(outputComponent >= 0 && outputComponent < staticData.components.size);
                        dynamicData.componentLogicLevels[outputComponent] = true;
                    }
                }
                if constexpr (std::is_base_of_v<CommunicatorElement, ElementType>) {
                    if (element.transmitState) {
                        int32_t outputCommunicator = element.communicator->communicatorIndex;
                        assert(outputCommunicator >= 0 && outputCommunicator < staticData.communicators.size);
                        dynamicData.communicatorTransmitStates[outputCommunicator] = true;
                    }
                }
                if constexpr(std::is_base_of_v<Relay, ElementType>) {
                    if (element.conductiveState) {
                        int32_t outputRelayPixel = staticData.pixels[pt].index[0];
                        assert(outputRelayPixel >= 0 && outputRelayPixel < staticData.relayPixels.size);
                        dynamicData.relayPixelIsConductive[outputRelayPixel] = true;
                    }
                }
            }, gameState[pt]);
        }
    }

    // flood fill to ensure that the current state is a valid simulation state
    floodFill(staticData, dynamicData);

    // Note: no atomics required here because the simulation thread has not started, and starting the thread automatically does synchronization.
    latestCompleteState = std::make_shared<DynamicData>(std::move(dynamicData));

    // take a snapshot (with the immediate propagation done)
    takeSnapshot(gameState);
}


void Simulator::reset(CanvasState& gameState) {
    // Reset the transient state to the starting state
    for (auto& element : gameState.dataMatrix) {
        resetLogicLevel(element);
    }

    // compile
    compile(gameState);

    // reset the communicators
    for (auto& simComm : staticData.communicators) {
        simComm.communicator->reset();
    }
}


void Simulator::start() {
    // Unset the 'stopping' flag
    // note:  // std::memory_order_relaxed, because when starting the thread, the std::thread constructor automatically does synchronization.
    simStopping.store(false, std::memory_order_relaxed);

    // Spawn the simulator thread
    simThread = std::thread([this]() {
        run();
    });
}


void Simulator::stop() {
    // Set the 'stopping' flag, and flush it so that the simulation thread can see it
    {
        std::lock_guard<std::mutex> lock(simSleepMutex);
        simStopping.store(true, std::memory_order_relaxed);
    }
    simSleepCV.notify_one();

    // Wait for the simulation thread to be done
    simThread.join();

    // Free up resources used by the std::thread object by assigning an empty std::thread
    simThread = std::thread();
}


void Simulator::step() {
    // this runs the simulator in the main thread, since we want to block until the step is complete.
    // since the simulator is stopped, we can read from latestCompleteState without synchronization.
    const DynamicData& oldState = *latestCompleteState;
    DynamicData newState(staticData.components.size, staticData.relayPixels.size, staticData.communicators.size);

    // calculate the new state
    calculate(staticData, oldState, newState);

    // save the new state (again without synchronization because simulator thread is not running).
    latestCompleteState = std::make_shared<DynamicData>(std::move(newState));
}



void Simulator::takeSnapshot(CanvasState& returnState) const {
    const std::shared_ptr<DynamicData> dynamicData = std::atomic_load_explicit(&latestCompleteState, std::memory_order_acquire);
    for (int32_t y = 0; y != returnState.dataMatrix.height(); ++y) {
        for (int32_t x = 0; x != returnState.dataMatrix.width(); ++x) {
            ext::point pt{ x, y };
            std::visit([&](auto& element) {
                using ElementType = std::decay_t<decltype(element)>;
                if constexpr(std::is_base_of_v<RenderLogicLevelElement, ElementType>) {
                    element.logicLevel = staticData.pixels[pt].logicLevel(*dynamicData);
                }
                if constexpr(std::is_base_of_v<CommunicatorElement, ElementType>) {
                    element.transmitState = dynamicData->communicatorTransmitStates[element.communicator->communicatorIndex];
                }
                if constexpr(std::is_base_of_v<Relay, ElementType>) {
                    element.conductiveState = dynamicData->relayPixelIsConductive[staticData.pixels[pt].index[0]];
                }
            }, returnState[pt]);
        }
    }
}



// To be invoked from the simulator thread only!
void Simulator::run() {
    // set the next step time to now
    nextStepTime = std::chrono::steady_clock::now();

    while (true) {
        // note: we just use the member field 'latestCompleteState' directly without any synchronization,
        // because when the simulator thread is running, no other thread will modify 'latestCompleteState'.
        const DynamicData& oldState = *latestCompleteState;
        DynamicData newState(staticData.components.size, staticData.relayPixels.size, staticData.communicators.size);

        // calculate the new state
        calculate(staticData, oldState, newState);

        // now we are done with the simulation, we save the new state.
        // so commit the new state (i.e. replace 'latestCompleteState' with the new state).
        // note that we need to commit this state even if we have already been asked to stop, otherwise the communicators will skip a step.
        // also, std::memory_order_release to flush the changes so that the main thread can see them
        std::atomic_store_explicit(&latestCompleteState, std::make_shared<DynamicData>(std::move(newState)), std::memory_order_release);

        // check if we are being asked to stop.
        if (simStopping.load(std::memory_order_acquire)) {
            break;
        }

        // sleep for an amount of time given by `period`, if the time is not already used up
        // this code will account for the time spent calculating the simulation, as long as it is less than `period`
        std::chrono::steady_clock::duration period(period_rep.load(std::memory_order_acquire));
        // special case when period is zero to avoid getting the current time (getting current time is an expensive operation).
        if (period.count() != 0) {
            nextStepTime += period;
            std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
            if (nextStepTime > now) {
                // still can sleep awhile
                std::unique_lock<std::mutex> lock(simSleepMutex);
                simSleepCV.wait_until(lock, nextStepTime, [this] {
                    return simStopping.load(std::memory_order_relaxed);
                });
                // break immediately if we got woken up due to simulator stopping.
                if (simStopping.load(std::memory_order_relaxed)) {
                    break;
                }
            }
            else {
                // the next step is overdue, so we don't sleep
                // if we come here, it means that the period is too fast for the simulator
                nextStepTime = now;
            }
        }
    }
}


void Simulator::calculate(const StaticData& staticData, const DynamicData& oldState, DynamicData& newState) {

    // invoke all the sources
    for (const SimulatorSource& source : staticData.sources) {
        source(oldState, newState);
    }

    // invoke all the logic gates
    staticData.logicGates.forEach([&](const auto& x) {
        x.forEach([&](const auto& y) {
            for (const auto& gate : y) {
                gate(oldState, newState);
            }
        });
    });
    
    // invoke all the relays
    staticData.relays.forEach([&](const auto& x) {
        x.forEach([&](const auto& y) {
            for (const auto& relay : y) {
                relay(oldState, newState);
            }
        });
    });

    // receive all the data for screen communicators
    pullCommunicatorReceivedData();

    // invoke all the communicators
    for (int32_t i = 0; i != staticData.communicators.size; ++i) {
        staticData.communicators.data[i](oldState, newState, newState.communicatorTransmitStates[i]);
    }

    // flood fill all the components
    floodFill(staticData, newState);
}


void Simulator::floodFill(const StaticData& staticData, DynamicData& dynamicData) {
    // first field of pair is true if it is a relay instead of a component
    std::stack<std::pair<bool, int32_t>> componentStack;
    for (int32_t i = 0; i != staticData.components.size; ++i) {
        if (dynamicData.componentLogicLevels[i]) {
            componentStack.emplace(false, i);
            dynamicData.componentLogicLevels[i] = false; // will be turned on again in the flood fill algorithm
        }
    }
    while (!componentStack.empty()) {
        auto [isRelay, i] = componentStack.top();
        componentStack.pop();

        if (!isRelay) {
            // ignore if already on
            if (dynamicData.componentLogicLevels[i]) continue;

            // turn the component on
            dynamicData.componentLogicLevels[i] = true;

            // flood to neighbours
            Component& component = staticData.components.data[i];
            for (int32_t j = component.adjRelayPixelsBegin; j != component.adjRelayPixelsEnd; ++j) {
                int32_t relayIndex = staticData.adjComponentList.data[j];
                if (dynamicData.relayPixelIsConductive[relayIndex] && !dynamicData.relayPixelLogicLevels[relayIndex]) {
                    componentStack.emplace(true, relayIndex);
                }
            }
        }
        else {
            // ignore if already on
            if (dynamicData.relayPixelLogicLevels[i]) continue;

            // turn the component on
            dynamicData.relayPixelLogicLevels[i] = true;

            // flood to neighbours
            RelayPixel& relayPixel = staticData.relayPixels.data[i];
            for (int32_t j = 0; j != relayPixel.numAdjComponents; ++j) {
                if (!dynamicData.componentLogicLevels[relayPixel.adjComponents[j]]) {
                    componentStack.emplace(false, relayPixel.adjComponents[j]);
                }
            }
        }
    }
}

void Simulator::pullCommunicatorReceivedData() {
    ScreenInputCommunicatorEvent commEvent;
    while (screenInputQueue.pop(commEvent)) {
        const bool turnOn = commEvent.turnOn;
        const int32_t commIndex = commEvent.communicatorIndex;
        assert(dynamic_cast<ScreenCommunicator*>(staticData.communicators[commIndex].communicator) != nullptr);
        auto& screenCommunicator = static_cast<ScreenCommunicator&>(*staticData.communicators[commIndex].communicator);
        screenCommunicator.insertEvent(turnOn);
    }
}


// simulator sepecific stuff
inline void Simulator::SimulatorSource::operator()(const DynamicData& oldData, DynamicData& newData) const noexcept {
    newData.componentLogicLevels[this->outputComponent] = true;
}
template <size_t NumInputs>
inline void Simulator::SimulatorAndGate<NumInputs>::operator()(const DynamicData& oldData, DynamicData& newData) const noexcept {
    bool& output = newData.componentLogicLevels[this->outputComponent];
    if (!output) {
        bool ans = true;
        // hopefully compilers will unroll the loop
        for (size_t i = 0; i != NumInputs; ++i) {
            if (!oldData.componentLogicLevels[this->inputComponents[i]]) {
                ans = false;
                break;
            }
        }
        output = ans;
    }
}
template <size_t NumInputs>
inline void Simulator::SimulatorOrGate<NumInputs>::operator()(const DynamicData& oldData, DynamicData& newData) const noexcept {
    bool& output = newData.componentLogicLevels[this->outputComponent];
    if (!output) {
        // hopefully compilers will unroll the loop
        for (size_t i = 0; i != NumInputs; ++i) {
            if (oldData.componentLogicLevels[this->inputComponents[i]]) {
                output = true;
                break;
            }
        }
    }
}
template <size_t NumInputs>
inline void Simulator::SimulatorNandGate<NumInputs>::operator()(const DynamicData& oldData, DynamicData& newData) const noexcept {
    bool& output = newData.componentLogicLevels[this->outputComponent];
    if (!output) {
        // hopefully compilers will unroll the loop
        for (size_t i = 0; i != NumInputs; ++i) {
            if (!oldData.componentLogicLevels[this->inputComponents[i]]) {
                output = true;
                break;
            }
        }
    }
}
template <size_t NumInputs>
inline void Simulator::SimulatorNorGate<NumInputs>::operator()(const DynamicData& oldData, DynamicData& newData) const noexcept {
    bool& output = newData.componentLogicLevels[this->outputComponent];
    if (!output) {
        bool ans = true;
        // hopefully compilers will unroll the loop
        for (size_t i = 0; i != NumInputs; ++i) {
            if (oldData.componentLogicLevels[this->inputComponents[i]]) {
                ans = false;
                break;
            }
        }
        output = ans;
    }
}

template <size_t NumInputs>
inline void Simulator::SimulatorPositiveRelay<NumInputs>::operator()(const DynamicData& oldData, DynamicData& newData) const noexcept {
    // hopefully compilers will unroll the loop
    for (size_t i = 0; i != NumInputs; ++i) {
        if (oldData.componentLogicLevels[this->inputComponents[i]]) {
            newData.relayPixelIsConductive[this->outputRelayPixel] = true;
            return;
        }
    }
}
template <size_t NumInputs>
inline void Simulator::SimulatorNegativeRelay<NumInputs>::operator()(const DynamicData& oldData, DynamicData& newData) const noexcept {
    // hopefully compilers will unroll the loop
    for (size_t i = 0; i != NumInputs; ++i) {
        if (!oldData.componentLogicLevels[this->inputComponents[i]]) {
            newData.relayPixelIsConductive[this->outputRelayPixel] = true;
            return;
        }
    }
}
inline void Simulator::SimulatorCommunicator::operator()(const DynamicData& oldData, DynamicData& newData, bool& transmitOutput) const noexcept {
    // hopefully compilers will unroll the loop
    for (int32_t inputComponent : inputComponents) {
        if (oldData.componentLogicLevels[inputComponent]) {
            transmitOutput = true;
            break;
        }
    }

    communicator->transmit(transmitOutput);

    bool& receiveOutput = newData.componentLogicLevels[outputComponent];
    receiveOutput = communicator->receive() || receiveOutput;
}
inline bool Simulator::StaticData::DisplayedPixel::logicLevel(const DynamicData& execData) const noexcept {
    switch (type) {
    case PixelType::COMPONENT: [[fallthrough]];
    case PixelType::COMMUNICATOR:
        return (index[0] != -1 ? execData.componentLogicLevels[index[0]] : false) || (index[1] != -1 ? execData.componentLogicLevels[index[1]] : false);
    case PixelType::RELAY:
        return execData.relayPixelLogicLevels[index[0]];
    case PixelType::EMPTY:
        break;
    }
    assert(false);
#ifdef NDEBUG
#ifdef _MSC_VER // MSVC
    __assume(0);
#endif
#ifdef __GNUC__ // GCC or Clang
    __builtin_unreachable();
#endif
#endif // NDEBUG
}
