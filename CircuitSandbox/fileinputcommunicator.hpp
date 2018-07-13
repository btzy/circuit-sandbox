#pragma once

#include <atomic>
#include <fstream>
#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <cstdint>
#include <cstddef>
#include <condition_variable>
#include "declarations.hpp"
#include "communicator.hpp"
#include "flushable_fixed_queue.hpp"

class FileInputCommunicator final : public Communicator {
    /**
     * There are three threads that interacts with this class:
     * 1. The UI thread
     * 2. The simulation thread
     * 3. The file reading thread
     * Most of the methods of this class should only be called by one of those threads.
     * The file reading thread is owned by this class.
     */
private:
    std::thread fileThread;
    std::atomic<bool> stoppingFlag;
    std::mutex sigMutex;
    std::condition_variable sigCV;
    std::ifstream inputStream;
    std::string inputFilePath;

    // used by simulator thread only
    std::queue<uint8_t> transmittedCommands; // TODO: allocation-minimized queue implementation
    bool suppressEnded = true; // whether the we need to read a byte first (prevents zero-length files)

    // used by simulator threaed only
    uint8_t currentTransmitChunk; // bitmask
    uint8_t currentTransmitCount;
    uint16_t currentReceiveChunk; // bitmask
    uint8_t currentReceiveCount;

    // shared between simulator and file threads
    constexpr static size_t BufSize = 65536;
    ext::flushable_fixed_queue<std::byte, BufSize> fileInputQueue;

    /**
     * Wakes up the file reading thread if it is sleeping.
     */
    void notifyFileThread() {
        {
            std::scoped_lock<std::mutex> lck(sigMutex);
        }
        sigCV.notify_one();
    }

    /**
     * Joins the file reading thread if it is joinable.
     */
    void joinFileThread() noexcept {
        if (fileThread.joinable()) {
            {
                std::scoped_lock<std::mutex> lock(sigMutex);
                stoppingFlag.store(true, std::memory_order_relaxed);
            }
            sigCV.notify_one();
            fileThread.join();
        }
    }

    bool loadFile(const char* filePath) {
        // stop the current thread if any
        joinFileThread();
        fileThread = std::thread();

        // close and clear the old file if any
        inputStream.close();
        inputStream.clear();

        // flush any bytes that are still in the buffer
        fileInputQueue.flush();

        if (filePath != nullptr) {
            // open the new file
            inputStream.open(filePath);
        }

        bool success = inputStream.is_open();

        // launch a new file thread
        if (success) {
            stoppingFlag.store(false, std::memory_order_relaxed);
            fileThread = std::thread([this]() {
                run();
            });
        }
        else {
            inputStream.close();
            inputStream.clear();
        }

        return success;
    }

public:
    using element_t = FileInputCommunicatorElement;

    ~FileInputCommunicator() {
        joinFileThread();
    }

    /**
     * Receives the next bit from this communicator.
     * Must be called from the simulation thread only!
     */
    bool receive() noexcept override {
        if (currentReceiveCount == 0) {
            if (!transmittedCommands.empty()) {
                uint8_t command = transmittedCommands.front();
                std::byte out;
                switch (command) {
                case 0b001:
                    // byte request
                    if (!suppressEnded) {
                        fileInputQueue.discard();
                    }
                    if (fileInputQueue.peek(out)) {
                        bool producerNeedsSignal = fileInputQueue.pop_testproducerneedssignal();
                        currentReceiveChunk = (static_cast<uint16_t>(out) << 3) | 0b001;
                        currentReceiveCount = 11;
                        transmittedCommands.pop();
                        suppressEnded = false;
                        if (producerNeedsSignal) {
                            notifyFileThread();
                        }
                    }
                    break;
                case 0b101:
                    // byte available in file?
                    if (!suppressEnded && (fileInputQueue.discard() || fileInputQueue.ended())) {
                        currentReceiveChunk = 0b0101;
                        currentReceiveCount = 4;
                        transmittedCommands.pop();
                        suppressEnded = true;
                    }
                    else if (suppressEnded || (!fileInputQueue.ended() && fileInputQueue.peek(out))) {
                        currentReceiveChunk = 0b1101;
                        currentReceiveCount = 4;
                        transmittedCommands.pop();
                        suppressEnded = true;
                    }
                    break;
                default:
                    // unknown command, discard it
                    transmittedCommands.pop();
                }
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
        currentTransmitChunk |= (static_cast<uint16_t>(value) << currentTransmitCount);
        if (currentTransmitChunk != 0) {
            ++currentTransmitCount;
            if (currentTransmitCount >= 3) {
                transmittedCommands.emplace(currentTransmitChunk);
                currentTransmitChunk = currentTransmitCount = 0;
            }
        }
    }

    /**
     * Set the file path
     * Must be called from the UI thread only!
     * filePath must be non-null
     */
    void setFile(const char* filePath) {
        inputFilePath = filePath;
        loadFile(filePath);
    }

    /**
     * Clear the file path
     * Must be called from the UI thread only!
     */
    void clearFile() {
        inputFilePath.clear();
        loadFile(nullptr);
    }

    /**
     * Load the file given by a previous setFilePath() call.
     */
    void reset() noexcept override {
        // stop the current thread if any
        joinFileThread();
        fileThread = std::thread();

        // close and clear the old file if any
        inputStream.close();
        inputStream.clear();

        while(!transmittedCommands.empty()) transmittedCommands.pop();
        suppressEnded = true;
        currentTransmitCount = 0;
        currentTransmitChunk = 0;
        currentReceiveCount = 0;
        currentReceiveChunk = 0;
        fileInputQueue.clear();

        if (!inputFilePath.empty()) {
            // open the new file
            inputStream.open(inputFilePath);
        }

        bool success = inputStream.is_open();

        // launch a new file thread
        if (success) {
            stoppingFlag.store(false, std::memory_order_relaxed);
            fileThread = std::thread([this]() {
                run();
            });
        }
    }

private:

    /**
     * This is the file reading thread function.
     * Must be called from the file reading thread only!
     */
    void run() {
        bool fileEnded = false;
        while (!stoppingFlag.load(std::memory_order_acquire) && !fileEnded) {
            size_t space;
            while (!stoppingFlag.load(std::memory_order_acquire) && (space = fileInputQueue.space()) > 0) {
                // try to read as many bytes as possible from the file
                std::byte bytes[BufSize];
                inputStream.read(reinterpret_cast<char*>(bytes), BufSize);
                auto byteCount = inputStream.gcount();
                fileInputQueue.push(bytes, bytes + byteCount);
                if (!inputStream.good()) {
                    fileEnded = true;
                    break;
                }
            }
            if (!stoppingFlag.load(std::memory_order_acquire) && !fileEnded) {
                // if we get here, it means the file hasn't ended but the buffer is full. so we sleep until notified.
                std::unique_lock<std::mutex> lck(sigMutex);
                sigCV.wait(lck, [this]() {
                    return stoppingFlag.load(std::memory_order_relaxed) || fileInputQueue.space() > 0;
                });
            }
        }
        fileInputQueue.end();
    }
};
