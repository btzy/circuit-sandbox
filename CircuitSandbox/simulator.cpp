#include "simulator.hpp"
#include "simulator_compile.hpp"
#include "elements.hpp"
#include "tag_tuple.hpp"
#include "visitor.hpp"

#include <memory>
#include <utility>
#include <thread>
#include <chrono>
#include <type_traits>
#include <stack>
#include <numeric>
#include <cassert>


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
            compilerStaticData.pixels[pt].isRelay = isRelayElement(gameState[pt]);
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
                                if (!componentIsUseful && isFloodfillableSourceOrSignalElement(gameState[currPt])) {
                                    componentIsUseful = true;
                                }

                                // if its not insulated wire, push the opposite direction
                                if (!std::holds_alternative<InsulatedWire>(gameState[currPt]) && !visited[currPt][1 - currDir]) {
                                    floodStack.emplace(currPt, 1 - currDir);
                                }

                                // visit the adjacent pixels
                                using positive_one_t = std::integral_constant<int32_t, 1>;
                                using negative_one_t = std::integral_constant<int32_t, -1>;
                                using directions_t = ext::tag_tuple<negative_one_t, positive_one_t>;
                                directions_t::for_each([&](auto direction_tag_t, auto) {
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

    // next, we populate the sources
    for (int32_t y = 0; y != gameState.height(); ++y) {
        for (int32_t x = 0; x != gameState.width(); ++x) {
            ext::point pt{ x, y };
            if (std::holds_alternative<Source>(gameState[pt])) {
                auto& source = compilerStaticData.sources.data.emplace_back();
                source.outputComponent = compilerStaticData.pixels[pt].index[0]; // for sources, index 0 and 1 should be the same
                assert(source.outputComponent >= 0 && source.outputComponent < compilerStaticData.components.size());
            }
        }
    }

    // next, we populate the logic gates
    for (int32_t y = 0; y != gameState.height(); ++y) {
        for (int32_t x = 0; x != gameState.width(); ++x) {
            ext::point pt{ x, y };
            std::visit([&](const auto& element) {
                using ElementType = std::decay_t<decltype(element)>;
                if constexpr (std::is_base_of_v<LogicGate, ElementType>) {
                    int32_t outputComponent = compilerStaticData.pixels[pt].index[0];
                    assert(outputComponent >= 0 && outputComponent < compilerStaticData.components.size());
                    // Note: can optimize if heap allocation for the std::vector is slow
                    std::vector<int32_t> inputComponents;
                    using up = integral_pair<int32_t, 0, -1>;
                    using down = integral_pair<int32_t, 0, 1>;
                    using left = integral_pair<int32_t, -1, 0>;
                    using right = integral_pair<int32_t, 1, 0>;
                    using directions_t = ext::tag_tuple<up, down, left, right>;
                    directions_t::for_each([&](auto direction_tag_t, auto) {
                        ext::point newPt = pt;
                        newPt.x += decltype(direction_tag_t)::type::first;
                        newPt.y += decltype(direction_tag_t)::type::second;
                        if (gameState.contains(newPt) && isSignal(gameState[newPt])) {
                            assert(compilerStaticData.pixels[newPt].index[0] >= 0 && compilerStaticData.pixels[newPt].index[0] < compilerStaticData.components.size());
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
                    using up = integral_pair<int32_t, 0, -1>;
                    using down = integral_pair<int32_t, 0, 1>;
                    using left = integral_pair<int32_t, -1, 0>;
                    using right = integral_pair<int32_t, 1, 0>;
                    using directions_t = ext::tag_tuple<up, down, left, right>;
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
                                assert(compilerStaticData.pixels[newPt].index[dir] >= 0 && compilerStaticData.pixels[newPt].index[dir] < compilerStaticData.components.size());
                                relayPixel.adjComponents[relayPixel.numAdjComponents++] = compilerStaticData.pixels[newPt].index[dir];
                                compilerStaticData.components[compilerStaticData.pixels[newPt].index[dir]].adjRelayPixels.emplace_back(outputRelayPixelIndex);
                            }
                            else if (isRelayElement(element)) {
                                // special case where two relays are adjacent
                                // spawn a new component between them, if the other component is already processed
                                if (visited[newPt][0]) { // this ensures that we only spawn the new component once per pair of adjacent relays
                                    assert(compilerStaticData.pixels[newPt].index[0] >= 0 && compilerStaticData.pixels[newPt].index[0] <= compilerStaticData.relayPixels.size());
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


    // === generate the fixed (packed) representation from the unpacked representation ===
    staticData.sources.data.update(compilerStaticData.sources.data);

    compilerStaticData.logicGates.forEachGate(staticData.logicGates, [&](auto& gate, const auto& compilerGate) {
        using indices_t = ext::tag_tuple<std::integral_constant<int32_t, 0>, std::integral_constant<int32_t, 1>, std::integral_constant<int32_t, 2>, std::integral_constant<int32_t, 3>, std::integral_constant<int32_t, 4>>;
        indices_t::for_each([&](const auto index_tag, auto) {
            constexpr int32_t Index = decltype(index_tag)::type::value;
            std::get<Index>(gate.data).update(std::get<Index>(compilerGate.data));
        });
    });
    
    compilerStaticData.relays.forEachRelay(staticData.relays, [&](auto& relay, const auto& compilerRelay) {
        using indices_t = ext::tag_tuple<std::integral_constant<int32_t, 0>, std::integral_constant<int32_t, 1>, std::integral_constant<int32_t, 2>, std::integral_constant<int32_t, 3>, std::integral_constant<int32_t, 4>>;
        indices_t::for_each([&](const auto index_tag, auto) {
            constexpr int32_t Index = decltype(index_tag)::type::value;
            std::get<Index>(relay.data).update(std::get<Index>(compilerRelay.data));
        });
    });

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

    staticData.relayPixels.update(compilerStaticData.relayPixels);

    staticData.pixels = std::move(compilerStaticData.pixels);


    // === dynamic state ===
    // sets everything to false by default
    DynamicData dynamicData(staticData.components.size, staticData.relayPixels.size);

    // fill from all the currently sources and logic gates,
    // and fill the conductive state from the relays
    for (int32_t y = 0; y != gameState.height(); ++y) {
        for (int32_t x = 0; x != gameState.width(); ++x) {
            ext::point pt{ x, y };
            std::visit([&](const auto& element) {
                // TODO: Have to fix save format to store whether *relays* are conductive
                // Other elements should not store any transient state
                using ElementType = std::decay_t<decltype(element)>;
                if constexpr (std::is_same_v<Source, ElementType>) {
                    int32_t outputComponent = staticData.pixels[pt].index[0];
                    assert(outputComponent >= 0 && outputComponent < staticData.components.size);
                    dynamicData.componentLogicLevels[outputComponent] = true;
                }
                else if constexpr (std::is_base_of_v<LogicGate, ElementType>) {
                    if (element.getLogicLevel()) {
                        int32_t outputComponent = staticData.pixels[pt].index[0];
                        assert(outputComponent >= 0 && outputComponent < staticData.components.size);
                        dynamicData.componentLogicLevels[outputComponent] = true;
                    }
                }
                else if constexpr(std::is_base_of_v<Relay, ElementType>) {
                    if (element.getLogicLevel()) {
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
    DynamicData newState(staticData.components.size, staticData.relayPixels.size);

    // calculate the new state
    calculate(staticData, oldState, newState);

    // save the new state (again without synchronization because simulator thread is not running).
    latestCompleteState = std::make_shared<DynamicData>(std::move(newState));
}



void Simulator::takeSnapshot(CanvasState& returnState) const {
    // Note (TODO): will be replaced with the proper compiled graph representation
    const std::shared_ptr<DynamicData> dynamicData = std::atomic_load_explicit(&latestCompleteState, std::memory_order_acquire);
    for (int32_t y = 0; y != returnState.dataMatrix.height(); ++y) {
        for (int32_t x = 0; x != returnState.dataMatrix.width(); ++x) {
            ext::point pt{ x, y };
            std::visit(visitor{
                [](std::monostate) {},
                [&](auto& element) {
                    element.setLogicLevel(staticData.pixels[pt].logicLevel(*dynamicData));
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
        DynamicData newState(staticData.components.size, staticData.relayPixels.size);

        // calculate the new state
        calculate(staticData, oldState, newState);

        // now we are done with the simulation, check if we are being asked to stop
        // if we are being asked to stop, we should discared this step,
        // so the UI doesn't change from the time the user pressed the stop button.
        if (simStopping.load(std::memory_order_acquire)) {
            break;
        }

        // we aren't asked to stop.
        // so commit the new state (i.e. replace 'latestCompleteState' with the new state).
        // also, std::memory_order_release to flush the changes so that the main thread can see them
        std::atomic_store_explicit(&latestCompleteState, std::make_shared<DynamicData>(std::move(newState)), std::memory_order_release);

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
    for (const SimulatorSource& source : staticData.sources.data) {
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
    bool& output = newData.relayPixelIsConductive[this->outputRelayPixel];
    // hopefully compilers will unroll the loop
    for (size_t i = 0; i != NumInputs; ++i) {
        if (oldData.componentLogicLevels[this->inputComponents[i]]) {
            output = true;
            return;
        }
    }
    output = false;
}
template <size_t NumInputs>
inline void Simulator::SimulatorNegativeRelay<NumInputs>::operator()(const DynamicData& oldData, DynamicData& newData) const noexcept {
    bool& output = newData.relayPixelIsConductive[this->outputRelayPixel];
    // hopefully compilers will unroll the loop
    for (size_t i = 0; i != NumInputs; ++i) {
        if (!oldData.componentLogicLevels[this->inputComponents[i]]) {
            output = true;
            return;
        }
    }
    output = false;
}
inline bool Simulator::StaticData::DisplayedPixel::logicLevel(const DynamicData& execData) const noexcept {
    if (!isRelay) {
        return (index[0] != -1 ? execData.componentLogicLevels[index[0]] : false) || (index[1] != -1 ? execData.componentLogicLevels[index[1]] : false);
        
    }
    else {
        return (index[0] != -1 ? execData.relayPixelLogicLevels[index[0]] : false) || (index[1] != -1 ? execData.relayPixelLogicLevels[index[1]] : false);
    }
}
