#pragma once

#include <cstdint>
#include "declarations.hpp"

class Communicator {
public:
    int32_t communicatorIndex;
    /**
     * Receives the next bit from this communicator.
     * Must be called from the simulation thread only!
     */
    virtual bool receive() noexcept = 0;
    /**
     * Transmits the next bit to this communicator.
     * Must be called from the simulation thread only!
     */
    virtual void transmit(bool) noexcept {}
    /**
     * Reset temporary data in the communicator.  This is called by Simulator::compile()
     * Must be synchronized with all other method calls.
     */
    virtual void refresh() noexcept {}
    /**
     * Reset the communicator state.  This is called by StateManager::resetSimulator()
     * Must be synchronized with all other method calls.
     */
    virtual void reset() noexcept {}
};
