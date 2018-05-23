#pragma once

/**
 * Simulator class, which runs the simulation (on a different thread)
 * StateManager class should invoke this class with the heap_matrix of underlying data (?)
 * All communication with the simulation engine should use this class
 */

#include <thread>

#include "gamestate.hpp"
#include "heap_matrix.hpp"

class Simulator {
private:
    std::thread simThread; // the thread on which the simulation will run

public:
    
    /**
     * Compiles the given gamestate and save the compiled simulation state (but does not start running the simulation).
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
        // TODO.
    }

    /**
     * Take a "snapshot" of the current simulation state, and write it to the supplied argument.
     * This works regardless whether the simulation is running or stopped.
     */
    GameState takeSnapshot() const;
};