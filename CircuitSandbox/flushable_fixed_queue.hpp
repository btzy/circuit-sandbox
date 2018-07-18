#pragma once

/*
 * A single-producer single-consumer queue where the producer and consumer can run from different threads.
 * All push() calls must be synchronized with one another, and all pop() calls must be synchronized with one another,
 * but push() and pop() calls do not need to synchronize with each other.
 * This is implemented using a circular buffer, with a compile-time specified size restriction.
 */

#include <atomic>
#include <utility>
#include <type_traits>
#include <array>
#include <limits>
#include <cassert>

namespace ext {
    /**
     * T = type of elements in the queue
     * expects T to be trivially constructible and trivially destructible
     * Size = buffer size = max num of elements + 1
     */
    template <typename T, size_t Size>
    class flushable_fixed_queue {
    private:
        std::array<T, Size> buffer;
        std::atomic<size_t> pushIndex, popIndex;
        std::atomic<size_t> endIndex, flushIndex;

        inline static size_t space(size_t tmp_pushIndex, size_t tmp_popIndex) noexcept {
            if (tmp_popIndex <= tmp_pushIndex) tmp_popIndex += Size;
            return tmp_popIndex - tmp_pushIndex - 1;
        }

        inline static size_t available(size_t tmp_pushIndex, size_t tmp_popIndex) noexcept {
            if (tmp_pushIndex < tmp_popIndex) tmp_pushIndex += Size;
            return tmp_pushIndex - tmp_popIndex;
        }

    public:
        flushable_fixed_queue() {
            pushIndex.store(0, std::memory_order_relaxed);
            popIndex.store(0, std::memory_order_relaxed);
            endIndex.store(std::numeric_limits<size_t>::max(), std::memory_order_relaxed);
            flushIndex.store(std::numeric_limits<size_t>::max(), std::memory_order_relaxed);
        }
        ~flushable_fixed_queue() {}

        /**
         * Calling this method must be synchronized with push().
         * Check how many new elements we can insert now.
         */
        inline size_t space() const noexcept {
            size_t tmp_pushIndex = pushIndex.load(std::memory_order_relaxed);
            size_t tmp_popIndex = popIndex.load(std::memory_order_acquire);
            return space(tmp_pushIndex, tmp_popIndex);
        }

        /**
         * Calling this method must be synchronized with pop().
         * Check how many new elements we can pop now.
         */
        inline size_t available() const noexcept {
            size_t tmp_popIndex = popIndex.load(std::memory_order_relaxed);
            size_t tmp_pushIndex = pushIndex.load(std::memory_order_acquire);
            return available(tmp_pushIndex, tmp_popIndex);
        }

        /**
         * Calling this method must be synchronized with pop().
         * In other words, you cannot call pop() from other threads while clearing the queue.
         */
        inline void clear() noexcept {
            size_t tmp_pushIndex = pushIndex.load(std::memory_order_acquire);
            popIndex.store(tmp_pushIndex, std::memory_order_release);
            endIndex.store(std::numeric_limits<size_t>::max(), std::memory_order_relaxed);
            flushIndex.store(std::numeric_limits<size_t>::max(), std::memory_order_relaxed);
        }

        /**
         * Calling this method must be synchronized with push().
         */
        inline void end() noexcept {
            endIndex.store(pushIndex.load(std::memory_order_relaxed), std::memory_order_release);
        }

        /**
        * Calling this method must be synchronized with pop().
        */
        inline bool ended() const noexcept {
            return endIndex.load(std::memory_order_acquire) == popIndex.load(std::memory_order_acquire);
        }

        /**
         * Informs the consumer that existing queue items may be discarded.
         * Calling this method must be synchronized with push().
         * Must have called end() immediately before this.
         */
        inline void flush() noexcept {
            flushIndex.store(pushIndex.load(std::memory_order_relaxed), std::memory_order_release);
        }

        /**
         * Discard discardable existing queue items.
         * Returns true if flush() was called by the producer.
         * Calling this method must be synchronized with pop().
         */
        inline bool discard() noexcept {
            size_t tmp_flushIndex = flushIndex.load(std::memory_order_acquire);
            size_t tmp_pushIndex = pushIndex.load(std::memory_order_acquire);
            size_t tmp_popIndex = popIndex.load(std::memory_order_relaxed);
            if (tmp_popIndex == tmp_flushIndex || tmp_flushIndex == std::numeric_limits<size_t>::max() || available(tmp_pushIndex, tmp_popIndex) < available(tmp_flushIndex, tmp_popIndex)) return false;
            popIndex.store(tmp_flushIndex, std::memory_order_release);
            return true;
        }

        /**
         * Emplace a new element to the back of the queue.
         */
        template <typename... Args>
        inline bool try_emplace(Args&&... args) {
            if (space() == 0) {
                return false;
            }
            size_t tmp_pushIndex = pushIndex.load(std::memory_order_relaxed);
            buffer[tmp_pushIndex] = T(std::forward<Args>(args)...);
            tmp_pushIndex = (tmp_pushIndex + 1) % Size;
            if (endIndex.load(std::memory_order_relaxed) == tmp_pushIndex) {
                endIndex.store(std::numeric_limits<size_t>::max(), std::memory_order_release);
            }
            if (flushIndex.load(std::memory_order_relaxed) == tmp_pushIndex) {
                flushIndex.store(std::numeric_limits<size_t>::max(), std::memory_order_release);
            }
            pushIndex.store(tmp_pushIndex, std::memory_order_release);
            return true;
        }

        /**
         * Push a new element to the back of the queue.
         */
        template <typename Arg>
        inline bool try_push(Arg&& arg) {
            return try_emplace(std::forward<Arg>(arg));
        }

        /**
         * Push some elements to the back of the queue.
         * Assumes that there is enough space (use space() to check for space).
         */
        template <typename InBegin, typename InEnd>
        inline void push(InBegin begin, InEnd end) {
            size_t tmp_pushIndex = pushIndex.load(std::memory_order_relaxed);
            for (; begin != end; ++begin) {
                buffer[tmp_pushIndex] = std::move(*begin);
                ++tmp_pushIndex;
                if (tmp_pushIndex == Size) tmp_pushIndex -= Size;
                if (endIndex.load(std::memory_order_relaxed) == tmp_pushIndex) {
                    endIndex.store(std::numeric_limits<size_t>::max(), std::memory_order_release);
                }
                if (flushIndex.load(std::memory_order_relaxed) == tmp_pushIndex) {
                    flushIndex.store(std::numeric_limits<size_t>::max(), std::memory_order_release);
                }
            }
            pushIndex.store(tmp_pushIndex, std::memory_order_release);
        }

        /**
         * Gets and removes the element from the front of the queue if exists.
         * Returns true if there was an element to remove.
         */
        inline bool try_pop(T& out) {
            if (available() == 0) {
                return false;
            }
            size_t tmp_popIndex = popIndex.load(std::memory_order_relaxed);
            out = std::move(buffer[tmp_popIndex]);
            popIndex.store((tmp_popIndex + 1) % Size, std::memory_order_release);
            return true;
        }
        
        /**
         * Pops some elements from the front of the queue.
         * Assumes that there is enough elements available (use available() to check for availability).
         */
        template <typename InBegin, typename InEnd>
        inline void pop(InBegin begin, InEnd end) {
            size_t tmp_popIndex = popIndex.load(std::memory_order_relaxed);
            for (; begin != end; ++begin) {
                *begin = std::move(buffer[tmp_popIndex]);
                ++tmp_popIndex;
                if (tmp_popIndex == Size) tmp_popIndex -= Size;
            }
            popIndex.store(tmp_popIndex, std::memory_order_release);
        }

        /**
         * Peeks the element from the front of the queue if exists.
         * Returns true if there was an element to remove.
         */
        inline bool peek(T& out) {
            if (available() == 0) {
                return false;
            }
            size_t tmp_popIndex = popIndex.load(std::memory_order_relaxed);
            out = buffer[tmp_popIndex];
            return true;
        }

        /**
        * Removes the element from the front of the queue.
        * Assumes that there is at least one element in the queue (use after peek()).
        */
        inline void pop() {
            size_t tmp_popIndex = popIndex.load(std::memory_order_relaxed);
            popIndex.store((tmp_popIndex + 1) % Size, std::memory_order_release);
        }

        /**
        * Removes the element from the front of the queue.
        * Returns true if the producer needs signal.
        * Assumes that there is at least one element in the queue (use after peek()).
        */
        inline bool pop_testproducerneedssignal() {
            size_t tmp_popIndex = popIndex.load(std::memory_order_relaxed);
            tmp_popIndex = (tmp_popIndex + 1) % Size;
            popIndex.store(tmp_popIndex, std::memory_order_release);
            return space(pushIndex.load(std::memory_order_acquire), tmp_popIndex) <= 1;
        }

        /**
         * Removes the element from the front of the queue.
         * Returns true if the producer needs signal.
         * Assumes that there is at least one element in the queue (use after peek()).
         */
        template <typename... Args>
        inline bool emplace_testconsumerneedssignal(Args&&... args) {
            size_t tmp_pushIndex = pushIndex.load(std::memory_order_relaxed);
            buffer[tmp_pushIndex] = T(std::forward<Args>(args)...);
            tmp_pushIndex = (tmp_pushIndex + 1) % Size;
            if (endIndex.load(std::memory_order_relaxed) == tmp_pushIndex) {
                endIndex.store(std::numeric_limits<size_t>::max(), std::memory_order_release);
            }
            if (flushIndex.load(std::memory_order_relaxed) == tmp_pushIndex) {
                flushIndex.store(std::numeric_limits<size_t>::max(), std::memory_order_release);
            }
            pushIndex.store(tmp_pushIndex, std::memory_order_release);
            return available(tmp_pushIndex, popIndex.load(std::memory_order_acquire)) <= 1;
        }
    };
}
