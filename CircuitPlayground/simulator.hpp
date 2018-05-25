#pragma once

/**
 * Simulator class, which runs the simulation (on a different thread)
 * StateManager class should invoke this class with the heap_matrix of underlying data (?)
 * All communication with the simulation engine should use this class
 * TODO: For now, no compilation actually happens, but we intend to compile to a graph before running the simulation.
 */

#include <thread>
#include <memory>
#include <atomic>

#include "gamestate.hpp"
#include "heap_matrix.hpp"


class Simulator {
private:
    // The thread on which the simulation will run.
    std::thread simThread;

    // The last GameState that is completely calculated.  This object might be accessed by the UI thread (for rendering purposes), but is updated (atomically) by the simulation thread.
    std::shared_ptr<GameState> latestCompleteState; // note: in C++20 this should be changed to std::atomic<std::shared_ptr<GameState>>.
    std::atomic<bool> simStopping; // flag for the UI thread to tell the simulation thread to stop.

    /**
     * Method that actually runs the simulator.
     * Must be invoked from the simulator thread only!
     * This method does not return until the simulation is stopped.
     * @pre simulation has been compiled.
     */
    void run();

public:

    ~Simulator();

    /**
     * Compiles the given gamestate and save the compiled simulation state (but does not start running the simulation).
     * @pre simulation is currently stopped.
     */
    void compile(const GameState& gameState);

    /**
     * Start running the simulation.
     * @pre simulation is currently stopped.
     */
    void start();

    /**
     * Stops (i.e. pauses) the simulation.
     * @pre simulation is currently running.
     */
    void stop();

    /**
     * Returns true if the simulation is currently running, false otherwise.
     */
    bool running() const {
        return simThread.joinable();
    }

    /**
     * Returns true is the simulator currently holds a compiled simulation.
     */
    bool holdsSimulation() const {
        std::shared_ptr<GameState> tmpState = std::atomic_load_explicit(&latestCompleteState, std::memory_order_acquire);
        return tmpState != nullptr;
    }

    /**
     * Clears any active simulation.
     * @pre simulation is currently stopped.
     */
    void clear() {
        latestCompleteState = nullptr;
    }

    /**
     * Take a "snapshot" of the current simulation state, and write it to the supplied argument.
     * This works regardless whether the simulation is running or stopped.
     */
    GameState takeSnapshot() const;
};
