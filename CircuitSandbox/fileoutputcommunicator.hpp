#pragma once

#include <atomic>
#include <cstdio>
#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <cstdint>
#include <cstddef>
#include <condition_variable>
#include "declarations.hpp"
#include "communicator.hpp"
#include "concurrent_fixed_queue.hpp"
#include "unrolled_linked_list_queue.hpp"

class FileOutputCommunicator final : public Communicator {
    /**
     * There are three threads that interacts with this class:
     * 1. The UI thread
     * 2. The simulation thread
     * 3. The file writing thread
     * Most of the methods of this class should only be called by one of those threads.
     * The file writing thread is owned by this class.
     */
private:
    std::thread fileThread;
    std::atomic<bool> stoppingFlag;
    std::mutex sigMutex;
    std::condition_variable sigCV;
    std::FILE* outputHandle;
    std::string outputFilePath;

    // used by simulator thread only
    ext::unrolled_linked_list_queue<std::byte, 65536> writeQueue;
    std::atomic<size_t> acknowledged_bytes = 0;

    // used by simulator threaed only
    uint16_t currentTransmitChunk; // bitmask
    uint8_t currentTransmitCount;
    uint8_t currentReceiveChunk; // bitmask
    uint8_t currentReceiveCount;

    // shared between simulator and file threads
    constexpr static size_t BufSize = 65536;
    ext::concurrent_fixed_queue<std::byte, BufSize> fileOutputQueue;

    /**
     * Wakes up the file writing thread if it is sleeping.
     */
    void notifyFileThread() {
        {
            std::scoped_lock<std::mutex> lck(sigMutex);
        }
        sigCV.notify_one();
    }

    /**
     * Joins the file writing thread if it is joinable.
     */
    void unloadFile() noexcept {
        if (fileThread.joinable()) {
            {
                std::scoped_lock<std::mutex> lock(sigMutex);
                stoppingFlag.store(true, std::memory_order_relaxed);
            }
            sigCV.notify_one();
            fileThread.join();
        }
    }

    // must call unloadFile() before calling this!
    // returns true is load succeeded, false otherwise.
    bool loadFile() {
        if (!outputFilePath.empty() && (outputHandle = std::fopen(outputFilePath.c_str(), "wb")) != nullptr) {
            // disable output buffering
            std::setvbuf(outputHandle, nullptr, _IONBF, 0);
            
            // launch a new file thread
            stoppingFlag.store(false, std::memory_order_relaxed);
            fileThread = std::thread([this]() {
                run();
            });
            return true;
        }
        return false;
    }

public:
    using element_t = FileOutputCommunicatorElement;

    ~FileOutputCommunicator() {
        unloadFile();
    }

    /**
     * Receives the next bit from this communicator.
     * Must be called from the simulation thread only!
     */
    bool receive() noexcept override {
        if (currentReceiveCount == 0) {
            size_t currSize = acknowledged_bytes.load(std::memory_order_acquire);
            if (currSize > 0) {
                acknowledged_bytes.fetch_sub(1, std::memory_order_release);
                currentReceiveChunk = 0b001;
                currentReceiveCount = 3;
            }
        }
        if (currentReceiveCount != 0) {
            bool out = currentReceiveChunk & 1;
            currentReceiveChunk >>= 1;
            --currentReceiveCount;
            return out;
        }
        return false;
    }

    /**
     * Transmits the next bit to this communicator.
     * Must be called from the simulation thread only!
     */
    void transmit(bool value) noexcept override {
        // clear anything still in the write queue
        while (!writeQueue.empty()) {
            if (!fileOutputQueue.try_push(writeQueue.front())) {
                break;
            }
            writeQueue.pop();
        }
        currentTransmitChunk |= (static_cast<uint16_t>(value) << currentTransmitCount);
        if (currentTransmitChunk != 0) {
            ++currentTransmitCount;
            if (currentTransmitCount >= 3) {
                switch (currentTransmitChunk & 0b111) {
                case 0b001:
                    if (currentTransmitCount == 11) {
                        // write to fileOutputQueue if possible, instead of the writeQueue
                        std::byte tmp = static_cast<std::byte>(currentTransmitChunk >> 3);
                        bool consumerNeedsSignal = false;
                        if (writeQueue.empty() && fileOutputQueue.space() > 0) {
                            consumerNeedsSignal = fileOutputQueue.emplace_testconsumerneedssignal(tmp);
                        }
                        else {
                            writeQueue.push(tmp);
                        }
                        currentTransmitChunk = currentTransmitCount = 0;
                        if (consumerNeedsSignal) {
                            notifyFileThread();
                        }
                    }
                    break;
                default:
                    // unknown command
                    currentTransmitChunk = currentTransmitCount = 0;
                }
            }
        }
    }

    /**
     * Set the file path
     * Must be called from the UI thread only!
     * filePath must be non-null
     */
    void setFile(const char* filePath) {
        outputFilePath = filePath;
        unloadFile();

        loadFile();
    }

    /**
     * Set the file path
     * Must be called from the UI thread only!
     * filePath must be non-null
     */
    const std::string& getFile() const noexcept {
        return outputFilePath;
    }

    /**
     * Clear the file path
     * Must be called from the UI thread only!
     */
    void clearFile() {
        outputFilePath.clear();
        unloadFile();
    }

    /**
     * Load the file given by a previous setFilePath() call.
     */
    void reset() noexcept override {
        // stop the current thread if any
        unloadFile();

        while(!writeQueue.empty()) writeQueue.pop();
        currentTransmitCount = 0;
        currentTransmitChunk = 0;
        currentReceiveCount = 0;
        currentReceiveChunk = 0;
        fileOutputQueue.clear();

        // open the new file
        loadFile();
    }

private:

    /**
     * This is the file reading thread function.
     * Must be called from the file writing thread only!
     */
    void run() {
        while (!stoppingFlag.load(std::memory_order_acquire)) {
            size_t available;
            while (!stoppingFlag.load(std::memory_order_acquire) && (available = fileOutputQueue.available()) > 0) {
                // try to write as many bytes as possible to the file
                std::byte bytes[BufSize];
                // peek at the bytes at the front, but don't modify the queue yet in case the write fails
                fileOutputQueue.peek(bytes, bytes + available);
                auto commitCount = std::fwrite(reinterpret_cast<void*>(bytes), 1, available, outputHandle);

                // pop the bytes that were committed
                fileOutputQueue.pop(commitCount);

                // enqueue the acknowledgements
                acknowledged_bytes.fetch_add(commitCount, std::memory_order_release);

                // check if everything that was available were committed
                if (commitCount != available) {
                    // file broken for some reason
                    stoppingFlag.store(true, std::memory_order_relaxed);
                }
            }
            if (!stoppingFlag.load(std::memory_order_acquire)) {
                // if we get here, it means the file hasn't ended but the buffer is empty. so we sleep until notified.
                std::unique_lock<std::mutex> lck(sigMutex);
                sigCV.wait(lck, [this]() {
                    return stoppingFlag.load(std::memory_order_relaxed) || fileOutputQueue.available() > 0;
                });
            }
        }

        // close the file, because we are stopping (probably user requested to change the file)
        std::fclose(outputHandle);
    }
};
