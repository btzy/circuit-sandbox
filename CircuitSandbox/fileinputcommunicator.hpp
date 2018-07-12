#pragma once

#include <atomic>
#include <fstream>
#include <string>
#include <thread>
#include <mutex>
#include <cstdint>
#include <cstddef>
#include <condition_variable>
#include "declarations.hpp"
#include "communicator.hpp"
#include "concurrent_fixed_queue.hpp"

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
    size_t numBytesRequested;
    bool checkRequested;

    // used by simulator threaed only
    uint16_t currentTransmitChunk; // bitmask
    uint16_t currentTransmitCount;
    uint16_t currentReceiveChunk; // bitmask
    uint16_t currentReceiveCount;

    // shared between simulator and file threads
    constexpr static size_t BufSize = 65536;
    ext::concurrent_fixed_queue<std::byte, BufSize> fileInputQueue;
    std::atomic<bool> fileEnded;

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
            std::byte out;
            bool hasByte = fileInputQueue.peek(out);
            if (numBytesRequested > 0 && hasByte) {
                bool producerNeedsSignal = fileInputQueue.pop_testproducerneedssignal();
                currentReceiveChunk = (static_cast<uint16_t>(out) << 3) | 0b001;
                currentReceiveCount = 11;
                --numBytesRequested;
                if (producerNeedsSignal) {
                    notifyFileThread();
                }
            }
            else if (numBytesRequested == 0 && checkRequested) {
                if (hasByte) {
                    currentReceiveChunk = 0b1101;
                    currentReceiveCount = 4;
                    checkRequested = false;
                }
                else if (fileEnded.load(std::memory_order_acquire)) {
                    currentReceiveChunk = 0b0101;
                    currentReceiveCount = 4;
                    checkRequested = false;
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
                switch (currentTransmitChunk & 0b111) {
                case 0b001:
                    ++numBytesRequested;
                    currentTransmitChunk = currentTransmitCount = 0;
                    break;
                case 0b101:
                    checkRequested = true;
                    currentTransmitChunk = currentTransmitCount = 0;
                    break;
                default:
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
    void setFilePath(const char* filePath) {
        inputFilePath = filePath;
    }

    /**
     * Clear the file path
     * Must be called from the UI thread only!
     */
    void clearFilePath() {
        inputFilePath.clear();
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

        numBytesRequested = 0;
        checkRequested = false;
        currentTransmitCount = 0;
        currentTransmitChunk = 0;
        currentReceiveCount = 0;
        currentReceiveChunk = 0;
        fileInputQueue.clear();
        fileEnded.store(false, std::memory_order_relaxed);

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
        else {
            fileEnded.store(true, std::memory_order_release);
        }

        //return success;
    }

private:

    /**
     * This is the file reading thread function.
     * Must be called from the file reading thread only!
     */
    void run() {
        while (!stoppingFlag.load(std::memory_order_acquire) && !fileEnded.load(std::memory_order_relaxed)) {
            size_t space;
            while (!stoppingFlag.load(std::memory_order_acquire) && (space = fileInputQueue.space()) > 0) {
                // try to read as many bytes as possible from the file
                std::byte bytes[BufSize];
                inputStream.read(reinterpret_cast<char*>(bytes), BufSize);
                auto byteCount = inputStream.gcount();
                fileInputQueue.push(bytes, bytes + byteCount);
                if (!inputStream.good()) {
                    fileEnded.store(true, std::memory_order_release);
                    break;
                }
            }
            if (!stoppingFlag.load(std::memory_order_acquire) && !fileEnded.load(std::memory_order_relaxed)) {
                // if we get here, it means the file hasn't ended but the buffer is full. so we sleep until notified.
                std::unique_lock<std::mutex> lck(sigMutex);
                sigCV.wait(lck, [this]() {
                    return stoppingFlag.load(std::memory_order_relaxed) || fileInputQueue.space() > 0;
                });
            }
        }
    }
};
