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
#include "communicator.hpp"

class ScreenCommunicator final : public Communicator {
public:
    using element_t = ScreenCommunicatorElement;
    struct Input {
        // this is a bit field
        // state : queue of states for communicator (up to 5 in queue)
        // count : number of states in queue excluding the first (live) one
        uint8_t state : 5, count : 3;
    } input; // prepared and used by simulator thread only!
    void insertEvent(bool value) noexcept {
        if (input.count < 4) {
            input.state |= (value << (++input.count));
        }
        else {
            input.state |= (value << 4);
        }
    }
    bool receive() noexcept override {
        if (input.count > 0) {
            input.state >>= 1;
            --input.count;
        }
        return input.state & 1;
    }
    void refresh() noexcept override {
        input.state = 0;
        input.count = 0;
    }
};

struct ScreenInputCommunicatorEvent {
    int32_t communicatorIndex;
    bool turnOn;
};
