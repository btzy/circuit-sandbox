#pragma once

#include <atomic>

class Communicator {
public:
    int32_t communicatorIndex;
};

class ScreenCommunicator final : public Communicator {};

struct ScreenInputCommunicatorEvent {
    int32_t communicatorIndex;
    bool turnOn;
};
