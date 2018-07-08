#pragma once

#include <atomic>

class Communicator {
    // TODO: be careful about multithreading here
public:
    // Transmit a bit (should be called exactly once per step)
    virtual void transmit(bool) = 0;

    // Receive a bit (should be called exactly once per step)
    virtual bool receive() = 0;
};

class ScreenCommunicator final : public Communicator {
private:
    std::atomic<bool> _transmit = false, _receive = false;
    void setReceiver(bool value) {
        _receive.store(value, std::memory_order_release);
    }
    friend struct ScreenCommunicatorElement;
    friend class ScreenInputAction;
public:
    void transmit(bool value) override {
        _transmit.store(value, std::memory_order_release);
    }
    bool receive() override {
        return _receive.load(std::memory_order_acquire);
    }
};
