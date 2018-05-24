#include "simulator.hpp"

#include <memory>
#include <utility>
#include <thread>
#include <chrono>


Simulator::~Simulator() {
    if (running())stop();
}


void Simulator::compile(const GameState& gameState) {
    // Note: (TODO) for now it seems that we are modifying the innards of tmpState, but when we do proper compilation we will need another data structure instead, not just plain GameState.
    GameState tmpState;
    tmpState.dataMatrix = typename GameState::matrix_t(gameState.dataMatrix.width(), gameState.dataMatrix.height());
    for (int32_t y = 0; y != gameState.dataMatrix.height(); ++y) {
        for (int32_t x = 0; x != gameState.dataMatrix.width(); ++x) {
            tmpState.dataMatrix[{x, y}] = gameState.dataMatrix[{x, y}];
        }
    }

    // Note: no atomics required here because the simulation thread has not started, and starting the thread automatically does synchronization.
    latestCompleteState = std::make_shared<GameState>(std::move(tmpState));
}


void Simulator::start() {
    // Unset the 'stopping' flag
    // note:  // std::memory_order_relaxed, because when starting the thread, the std::thread constructor automatically does synchronization.
    simStopping.store(false, std::memory_order_relaxed);
    
    // Spawn the simulator thread
    simThread = std::thread([this]() {
        this->run();
    });
}



void Simulator::stop() {
    // Set the 'stopping' flag, and flush it so that the simulation thread can see it
    simStopping.store(true, std::memory_order_release);

    // Wait for the simulation thread to be done
    simThread.join();
    
    // Free up resources used by the std::thread object by assigning an empty std::thread
    simThread = std::thread();
}



GameState Simulator::takeSnapshot() const {
    // Note (TODO): will be replaced with the proper compiled graph representation
    GameState returnState;
    const std::shared_ptr<GameState> gameState = std::atomic_load_explicit(&latestCompleteState, std::memory_order_acquire);
    returnState.dataMatrix = typename GameState::matrix_t(gameState->dataMatrix.width(), gameState->dataMatrix.height());
    for (int32_t y = 0; y != gameState->dataMatrix.height(); ++y) {
        for (int32_t x = 0; x != gameState->dataMatrix.width(); ++x) {
            returnState.dataMatrix[{x, y}] = gameState->dataMatrix[{x, y}];
        }
    }
    return returnState;
}



// To be invoked from the simulator thread only!
void Simulator::run() {
    while (true) {
        // note: we just use the member field 'latestCompleteState' directly without any synchronization,
        // because when the simulator thread is running, no other thread will modify 'latestCompleteState'.
        const GameState& oldState = *latestCompleteState;

        // holds the new state after simulating this step
        GameState newState;
        newState.dataMatrix = typename GameState::matrix_t(oldState.dataMatrix.width(), oldState.dataMatrix.height());

        // TODO: change the double for-loop below to actually process one step of the simulation
        for (int32_t y = 0; y != oldState.dataMatrix.height(); ++y) {
            for (int32_t x = 0; x != oldState.dataMatrix.width(); ++x) {
                newState.dataMatrix[{x, y}] = oldState.dataMatrix[{x, y}];
            }
        }

        // now we are done with the simulation, check if we are being asked to stop
        // if we are being asked to stop, we should discared this step,
        // so the UI doesn't change from the time the user pressed the stop button.
        if (simStopping.load(std::memory_order_acquire)) {
            break;
        }

        // we aren't asked to stop.
        // so commit the new state (i.e. replace 'latestCompleteState' with the new state).
        // also, std::memory_order_release to flush the changes so that the main thread can see them
        std::atomic_store_explicit(&latestCompleteState, std::make_shared<GameState>(std::move(newState)), std::memory_order_release);

        // sleep for 500 milliseconds
        // TODO: this will become configurable, or to not sleep at all.
        // TODO: make it such that if the main thread calls stop(), it will wake a sleeping simulation thread, so that the main thread won't freeze up waiting for the simulation thread to stop.
        using namespace std::literals::chrono_literals; // <-- placed here because the sleeping for fixed amount of time is a temporary thing
        std::this_thread::sleep_for(500ms);
    }
}