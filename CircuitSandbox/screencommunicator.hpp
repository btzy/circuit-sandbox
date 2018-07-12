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
