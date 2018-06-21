#include "simulator.hpp"
#include "elements.hpp"
#include "tag_tuple.hpp"
#include "visitor.hpp"

#include <memory>
#include <utility>
#include <thread>
#include <chrono>
#include <type_traits>
#include <stack>


Simulator::~Simulator() {
    if (running())stop();
}


void Simulator::compile(const CanvasState& gameState, bool resetLogicLevel) {
    // Note: (TODO) for now it seems that we are modifying the innards of tmpState, but when we do proper compilation we will need another data structure instead, not just plain CanvasState.
    CanvasState tmpState;
    tmpState.dataMatrix = typename CanvasState::matrix_t(gameState.dataMatrix.width(), gameState.dataMatrix.height());
    for (int32_t y = 0; y != gameState.dataMatrix.height(); ++y) {
        for (int32_t x = 0; x != gameState.dataMatrix.width(); ++x) {
            tmpState.dataMatrix[{x, y}] = gameState.dataMatrix[{x, y}];
            if (resetLogicLevel) {
                std::visit(visitor{
                    [](std::monostate) {},
                    [](auto& element) {
                        element.resetLogicLevel();
                    }
                }, tmpState.dataMatrix[{x, y}]);
            }
        }
    }

    // Note: no atomics required here because the simulation thread has not started, and starting the thread automatically does synchronization.
    latestCompleteState = std::make_shared<CanvasState>(std::move(tmpState));
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
    const CanvasState& oldState = *latestCompleteState;
    CanvasState newState;
    
    // calculate the new state
    calculate(oldState, newState);

    // save the new state (again without synchronization because simulator thread is not running).
    latestCompleteState = std::make_shared<CanvasState>(std::move(newState));
}



void Simulator::takeSnapshot(CanvasState& returnState) const {
    // Note (TODO): will be replaced with the proper compiled graph representation
    const std::shared_ptr<CanvasState> gameState = std::atomic_load_explicit(&latestCompleteState, std::memory_order_acquire);
    for (int32_t y = 0; y != gameState->dataMatrix.height(); ++y) {
        for (int32_t x = 0; x != gameState->dataMatrix.width(); ++x) {
            returnState[{x, y}] = gameState->dataMatrix[{x, y}];
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
        const CanvasState& oldState = *latestCompleteState;
        CanvasState newState;

        // calculate the new state
        calculate(oldState, newState);

        // now we are done with the simulation, check if we are being asked to stop
        // if we are being asked to stop, we should discared this step,
        // so the UI doesn't change from the time the user pressed the stop button.
        if (simStopping.load(std::memory_order_acquire)) {
            break;
        }

        // we aren't asked to stop.
        // so commit the new state (i.e. replace 'latestCompleteState' with the new state).
        // also, std::memory_order_release to flush the changes so that the main thread can see them
        std::atomic_store_explicit(&latestCompleteState, std::make_shared<CanvasState>(std::move(newState)), std::memory_order_release);

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


void Simulator::calculate(const CanvasState& oldState, CanvasState& newState) {

    // holds the new state after simulating this step
    extensions::heap_matrix<ElementState> intermediateState(oldState.dataMatrix.width(), oldState.dataMatrix.height());

    // TODO: consider having an actual element type represent an 'Empty' element, so we don't need to keep checking for std::monostate everywhere
    for (int32_t y = 0; y != oldState.dataMatrix.height(); ++y) {
        for (int32_t x = 0; x != oldState.dataMatrix.width(); ++x) {

            using positive_one_t = std::integral_constant<int32_t, 1>;
            using zero_t = std::integral_constant<int32_t, 0>;
            using negative_one_t = std::integral_constant<int32_t, -1>;
            using directions_t = extensions::tag_tuple<std::pair<zero_t, negative_one_t>, std::pair<positive_one_t, zero_t>, std::pair<zero_t, positive_one_t>, std::pair<negative_one_t, zero_t>>;
            // populate the adjacent environment for this pixel
            AdjacentEnvironment env;
            directions_t::for_each([&oldState, &env, x, y](auto direction_tag_t, auto index_t) {
                int32_t adjX = x + decltype(direction_tag_t)::type::first_type::value;
                int32_t adjY = y + decltype(direction_tag_t)::type::second_type::value;
                auto& adjInput = env.inputs[decltype(index_t)::value];
                adjInput = AdjacentInput{ AdjacentElementType::EMPTY, false };
                if (0 <= adjX && adjX < oldState.dataMatrix.width() && 0 <= adjY && adjY < oldState.dataMatrix.height()) {
                    const auto& adjacentElement = oldState.dataMatrix[{adjX, adjY}];
                    std::visit(visitor{
                        [](std::monostate) {},
                        [&adjInput](auto element) {
                            adjInput.type = element.isSignal() ? AdjacentElementType::SIGNAL : AdjacentElementType::WIRE;
                            adjInput.logicLevel = element.getLogicLevel();
                        }
                    }, adjacentElement);
                }
            });
            intermediateState[{x, y}] = std::visit(visitor{
                [](std::monostate) {
                    return ElementState::INSULATOR;
                },
                [&env](auto element) {
                    return element.processStep(env);
                }
            }, oldState.dataMatrix[{x, y}]);
        }
    }

    // holds the new state after simulating this step
    newState.dataMatrix = typename CanvasState::matrix_t(oldState.dataMatrix.width(), oldState.dataMatrix.height());

    // clone all the element types, but set them all to LOW
    for (int32_t y = 0; y != oldState.dataMatrix.height(); ++y) {
        for (int32_t x = 0; x != oldState.dataMatrix.width(); ++x) {
            newState.dataMatrix[{x, y}] = std::visit(visitor{
                [](std::monostate) {
                    return CanvasState::element_variant_t{};
                },
                [](auto element) {
                    using Element = decltype(element);
                    return CanvasState::element_variant_t{ Element(false, element.getDefaultLogicLevel()) };
                }
            }, oldState.dataMatrix[{x, y}]);
        }
    }



    struct Visited {
        bool dir[2] = { false, false }; // {horizontal, vertical}
    };

    extensions::heap_matrix<Visited> visitedMatrix(oldState.dataMatrix.width(), oldState.dataMatrix.height());


    // run a flood fill algorithm
    {

        // reset the visitedMatrix
        for (int32_t y = 0; y != oldState.dataMatrix.height(); ++y) {
            for (int32_t x = 0; x != oldState.dataMatrix.width(); ++x) {
                if (intermediateState[{x, y}] == ElementState::SOURCE) {
                    visitedMatrix[{x, y}] = { false,false };
                }
            }
        }

        std::stack<std::pair<extensions::point, int>> pendingVisit;

        // insert all the sources into the stack
        for (int32_t y = 0; y != oldState.dataMatrix.height(); ++y) {
            for (int32_t x = 0; x != oldState.dataMatrix.width(); ++x) {
                if (intermediateState[{x, y}] == ElementState::SOURCE) {
                    pendingVisit.emplace(std::pair<extensions::point, int>{ {x, y}, 0});
                    pendingVisit.emplace(std::pair<extensions::point, int>{ {x, y}, 1});
                }
            }
        }

        // flood fill
        while (!pendingVisit.empty()) {
            auto[currentPoint, axis] = pendingVisit.top();
            pendingVisit.pop();

            // check if we have already processed this location
            if (visitedMatrix[currentPoint].dir[axis]) {
                continue; // if it's already set to HIGH, it means we already processed it
            }

            // set the logic level to HIGH
            visitedMatrix[currentPoint].dir[axis] = true;

            // if its conductive wire, push the same location with the complementary axis into the stack (if it's still not yet visited)
            if (intermediateState[currentPoint] == ElementState::CONDUCTIVE_WIRE && !visitedMatrix[currentPoint].dir[1 - axis]) {
                pendingVisit.emplace(currentPoint, 1 - axis);
            }

            // put all the unvisited neighbours into the stack
            using positive_one_t = std::integral_constant<int32_t, 1>;
            using negative_one_t = std::integral_constant<int32_t, -1>;
            using directions_t = extensions::tag_tuple<negative_one_t, positive_one_t>;
            directions_t::for_each([&pendingVisit, &oldState, &intermediateState, &visitedMatrix, currentPoint, axis](auto direction_tag_t, auto) {
                int32_t adjX = currentPoint.x;
                int32_t adjY = currentPoint.y;
                if (axis == 0) {
                    adjX += decltype(direction_tag_t)::type::value;
                }
                else {
                    adjY += decltype(direction_tag_t)::type::value;
                }

                extensions::point newPoint{ adjX, adjY };

                // TODO: probably we should make heap_matrix have a within_bounds() method
                if (0 <= newPoint.x && newPoint.x < oldState.dataMatrix.width() && 0 <= newPoint.y && newPoint.y < oldState.dataMatrix.height()
                    && intermediateState[newPoint] != ElementState::INSULATOR &&
                    !(isSignal(oldState.dataMatrix[currentPoint]) && isSignalReceiver(oldState.dataMatrix[newPoint])) &&
                    !(isSignalReceiver(oldState.dataMatrix[currentPoint]) && isSignal(oldState.dataMatrix[newPoint])) &&
                    !visitedMatrix[newPoint].dir[axis]) {
                    pendingVisit.emplace(newPoint, axis);
                }
            });
        }
    }

    // set the logic state of the new elements
    for (int32_t y = 0; y != oldState.dataMatrix.height(); ++y) {
        for (int32_t x = 0; x != oldState.dataMatrix.width(); ++x) {
            std::visit(visitor{
                [](std::monostate) {},
                [&vis = visitedMatrix[{x,y}]](auto& element) {
                    element.setLogicLevel(vis.dir[0] || vis.dir[1]);
                }
            }, newState.dataMatrix[{x, y}]);
        }
    }
}