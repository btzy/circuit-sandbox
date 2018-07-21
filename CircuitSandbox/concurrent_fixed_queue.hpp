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
#include <cassert>

namespace ext {
    /**
     * T = type of elements in the queue
     * Size = buffer size = max num of elements + 1
     */
    template <typename T, size_t Size>
    class concurrent_fixed_queue {
    private:
        std::array<std::aligned_storage_t<sizeof(T), alignof(T)>, Size> buffer;
        std::atomic<size_t> pushIndex, popIndex;

        inline static size_t space(size_t tmp_pushIndex, size_t tmp_popIndex) noexcept {
            if (tmp_popIndex <= tmp_pushIndex) tmp_popIndex += Size;
            return tmp_popIndex - tmp_pushIndex - 1;
        }

        inline static size_t available(size_t tmp_pushIndex, size_t tmp_popIndex) noexcept {
            if (tmp_pushIndex < tmp_popIndex) tmp_pushIndex += Size;
            return tmp_pushIndex - tmp_popIndex;
        }

    public:
        concurrent_fixed_queue() {
            pushIndex.store(0, std::memory_order_relaxed);
            popIndex.store(0, std::memory_order_relaxed);
        }
        ~concurrent_fixed_queue() {}

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
            size_t tmp_popIndex = popIndex.load(std::memory_order_relaxed);
            while (tmp_popIndex != tmp_pushIndex) {
                T& obj = reinterpret_cast<T&>(buffer[tmp_popIndex]);
                obj.~T();
                ++tmp_popIndex;
                if (tmp_popIndex == Size) tmp_popIndex -= Size;
            }
            popIndex.store(tmp_popIndex, std::memory_order_release);
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
            new (&buffer[tmp_pushIndex]) T(std::forward<Args>(args)...);
            pushIndex.store((tmp_pushIndex + 1) % Size, std::memory_order_release);
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
                new (&buffer[tmp_pushIndex]) T(*begin);
                ++tmp_pushIndex;
                if (tmp_pushIndex == Size) tmp_pushIndex -= Size;
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
            T& obj = reinterpret_cast<T&>(buffer[tmp_popIndex]);
            out = std::move(obj);
            obj.~T();
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
                T& obj = reinterpret_cast<T&>(buffer[tmp_popIndex]);
                *begin = std::move(obj);
                obj.~T();
                ++tmp_popIndex;
                if (tmp_popIndex == Size) tmp_popIndex -= Size;
            }
            popIndex.store(tmp_popIndex, std::memory_order_release);
        }

        /**
         * Pops some elements from the front of the queue, discarding them.
         * Assumes that there is enough elements available (use available() to check for availability).
         */
        inline void pop(size_t amount) noexcept {
            size_t tmp_popIndex = popIndex.load(std::memory_order_relaxed);
            for (; amount > 0; --amount) {
                T& obj = reinterpret_cast<T&>(buffer[tmp_popIndex]);
                obj.~T();
                ++tmp_popIndex;
                if (tmp_popIndex == Size) tmp_popIndex -= Size;
            }
            popIndex.store(tmp_popIndex, std::memory_order_release);
        }

        /**
        * Peeks some elements from the front of the queue.
        * Assumes that there is enough elements available (use available() to check for availability).
        */
        template <typename InBegin, typename InEnd>
        inline void peek(InBegin begin, InEnd end) const {
            size_t tmp_popIndex = popIndex.load(std::memory_order_relaxed);
            for (; begin != end; ++begin) {
                const T& obj = reinterpret_cast<const T&>(buffer[tmp_popIndex]);
                *begin = obj;
                ++tmp_popIndex;
                if (tmp_popIndex == Size) tmp_popIndex -= Size;
            }
        }

        /**
         * Peeks the element from the front of the queue if exists.
         * Returns true if there was an element to remove.
         */
        inline bool peek(T& out) const {
            if (available() == 0) {
                return false;
            }
            size_t tmp_popIndex = popIndex.load(std::memory_order_relaxed);
            T& obj = reinterpret_cast<T&>(buffer[tmp_popIndex]);
            out = obj;
            return true;
        }

        /**
        * Removes the element from the front of the queue.
        * Assumes that there is at least one element in the queue (use after peek()).
        */
        inline void pop() {
            size_t tmp_popIndex = popIndex.load(std::memory_order_relaxed);
            T& obj = reinterpret_cast<T&>(buffer[tmp_popIndex]);
            obj.~T();
            popIndex.store((tmp_popIndex + 1) % Size, std::memory_order_release);
        }

        /**
        * Removes the element from the front of the queue.
        * Returns 
        * Assumes that there is at least one element in the queue (use after peek()).
        */
        inline bool pop_testproducerneedssignal() {
            size_t tmp_popIndex = popIndex.load(std::memory_order_relaxed);
            T& obj = reinterpret_cast<T&>(buffer[tmp_popIndex]);
            obj.~T();
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
            new (&buffer[tmp_pushIndex]) T(std::forward<Args>(args)...);
            tmp_pushIndex = (tmp_pushIndex + 1) % Size;
            pushIndex.store(tmp_pushIndex, std::memory_order_release);
            return available(tmp_pushIndex, popIndex.load(std::memory_order_acquire)) <= 1;
        }
    };
}
