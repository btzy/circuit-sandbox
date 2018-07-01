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
#include <mutex>
#include <condition_variable>
#include <chrono>

#include "canvasstate.hpp"


class Simulator {
public:
    using period_t = std::chrono::steady_clock::duration;
private:
    // The thread on which the simulation will run.
    std::thread simThread;

    // The last CanvasState that is completely calculated.  This object might be accessed by the UI thread (for rendering purposes), but is updated (atomically) by the simulation thread.
    std::shared_ptr<CanvasState> latestCompleteState; // note: in C++20 this should be changed to std::atomic<std::shared_ptr<CanvasState>>.
    std::atomic<bool> simStopping; // flag for the UI thread to tell the simulation thread to stop.

    // synchronization stuff to wake the simulator thread if its sleeping
    std::mutex simSleepMutex;
    std::condition_variable simSleepCV;

    // minimum time between successive simulation steps (zero = as fast as possible)
    std::atomic<period_t::rep> period_rep; // it is stored using the underlying integer type so that we can use atomics
    std::chrono::steady_clock::time_point nextStepTime; // next time the simulator will be stepped

    /**
     * Method that actually runs the simulator.
     * Must be invoked from the simulator thread only!
     * This method does not return until the simulation is stopped.
     * @pre simulation has been compiled.
     */
    void run();

    /**
    * Method that calculates a single step of the simulation.
    * May be invoked in the simulator thread (by run()) or from the main thread (by step()).
    * This method is static, and hence is safe be called concurrently from multiple threads.
    * @pre simulation has been compiled.
    */
    static void calculate(const CanvasState& oldState, CanvasState& newState);

public:

    ~Simulator();

    /**
     * Compiles the given gamestate and save the compiled simulation state (but does not start running the simulation).
     * If resetLogicLevel is true, the default logic levels will be used.
     * @pre simulation is currently stopped.
     */
    void compile(const CanvasState& gameState, bool useDefaultLogicLevel);

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
    * Runs a single step of the simulation, then stops the simulation.
    * This method will block until the step is completed and the simulation is stopped.
    * @pre simulation is currently stopped.
    * @post simulation is stopped.
    */
    void step();

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
        std::shared_ptr<CanvasState> tmpState = std::atomic_load_explicit(&latestCompleteState, std::memory_order_acquire);
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
     * Take a "snapshot" of the current simulation state and writes it to the argument supplied.
     * This works regardless whether the simulation is running or stopped.
     * @pre the supplied canvas state has the correct element positions as the canvas state that was compiled.
     */
    void takeSnapshot(CanvasState&) const;

    /**
     * Gets this period of the simulation step.
     * This works regardless whether the simulation is running or stopped.
     */
    std::chrono::steady_clock::duration getPeriod() const {
        return std::chrono::steady_clock::duration(period_rep.load(std::memory_order_acquire));
    }

    /**
    * Gets this period of the simulation step.
    * This works regardless whether the simulation is running or stopped.
    */
    void setPeriod(const std::chrono::steady_clock::duration& period) {
        period_rep.store(period.count(), std::memory_order_release);
    }
};
