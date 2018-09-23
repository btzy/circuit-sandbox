/*
 * Circuit Sandbox
 * Copyright 2018 National University of Singapore <enterprise@nus.edu.sg>
 *
 * This file is part of Circuit Sandbox.
 * Circuit Sandbox is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 * Circuit Sandbox is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with Circuit Sandbox.  If not, see <https://www.gnu.org/licenses/>.
 */

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
