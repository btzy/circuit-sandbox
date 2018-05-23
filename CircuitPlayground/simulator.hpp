#pragma once

/**
 * Simulator class, which runs the simulation (on a different thread)
 * GameState class should invoke this class with the heap_matrix of underlying data (?)
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
     * Compiles and starts (runs) the simulation using the given game state matrix
     * If there is already a compiled simulation, do not recompile the gamestate
     */
    void startOrResume(GameState::matrix_t gameState);
    
    /**
     * Pauses the simulation (but does not destroy the compiled simulation)
     */
    void pause();

    /**
     * Stops and destroys the simulation.
     * Used when the gamestate was changed by the user, and we definitely need to recompile the simulation anyway.
     */
    void stop();
};