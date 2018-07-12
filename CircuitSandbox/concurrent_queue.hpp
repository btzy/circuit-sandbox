#pragma once

/*
 * A single-producer single-consumer queue where the producer and consumer can run from different threads.
 * All push() calls must be synchronized with one another, and all pop() calls must be synchronized with one another,
 * but push() and pop() calls do not need to synchronize with each other.
 * This is implemented using a linked list instead of a circular buffer, so there is no size restriction in the queue.
 * pop() never frees any memory.
 */

#include <atomic>
#include <utility>
#include <type_traits>
#include <cassert>

namespace ext {
    template <typename T>
    class concurrent_queue {
        /**
         * The trailing object in the list is always empty.
         * * Test if next == nullptr to tell if the data is empty.
         * This class never releases memory until destroyed.
         */
    private:
        struct node {
            std::aligned_storage_t<sizeof(T), alignof(T)> data;
            std::atomic<node*> next;
            inline node* getNext() const noexcept {
                return next.load(std::memory_order_acquire);
            }
            inline void setNext(node* ptr) noexcept {
                next.store(ptr, std::memory_order_release);
            }
            inline void resetRelaxed() noexcept {
                next.store(nullptr, std::memory_order_relaxed);
            }
        };
        node* head;
        node* tail;
        node* buffer_head;
        node* buffer_tail;
    public:
        concurrent_queue() {
            head = tail = new node;
            head->resetRelaxed();
            buffer_head = buffer_tail = new node;
            buffer_head->resetRelaxed();
        }
        ~concurrent_queue() {
            // make sure we free all the memory we own:

            // clear the buffer
            {
                node* tmp_buffer_head;
                while ((tmp_buffer_head = buffer_head->getNext()) != nullptr) {
                    delete buffer_head;
                    buffer_head = tmp_buffer_head;
                }
                assert(buffer_head == buffer_tail && buffer_head->getNext() == nullptr);
                delete buffer_head;
            }

            // clear the real linked list
            {
                node* tmp_head;
                while ((tmp_head = head->getNext()) != nullptr) {
                    {
                        T& obj = reinterpret_cast<T&>(head->data);
                        obj.~T();
                    }
                    delete head;
                    head = tmp_head;
                }
                assert(head == tail && head->getNext() == nullptr);
                delete head;
            }
        }

        /**
         * Calling this method must be synchronized with pop().
         * In other words, you cannot call pop() from other threads while clearing the queue.
         */
        inline void clear() {
            node* tmp_head;
            while ((tmp_head = head->getNext()) != nullptr) {
                {
                    T& obj = reinterpret_cast<T&>(head->data);
                    obj.~T();
                }
                head->resetRelaxed();
                buffer_tail->setNext(head);
                buffer_tail = head;
                head = tmp_head;
            }
        }

        /**
         * Emplace a new element to the back of the queue.
         */
        template <typename... Args>
        inline void emplace(Args&&... args) {
            new (&tail->data) T(std::forward<Args>(args)...);
            node* tmp_buffer_head = buffer_head->getNext();
            if (tmp_buffer_head != nullptr) {
                buffer_head->resetRelaxed();
                tail->setNext(buffer_head);
                tail = buffer_head;
                buffer_head = tmp_buffer_head;
            }
            else {
                node* new_object = new node;
                new_object->resetRelaxed();
                tail->setNext(new_object);
                tail = new_object;
            }
        }

        /**
         * Push a new element to the back of the queue.
         */
        template <typename Arg>
        inline void push(Arg&& arg) {
            emplace(std::forward<Arg>(arg));
        }

        /**
         * Gets and removes the element from the front of the queue if exists.
         * Returns true if there was an element to remove.
         */
        inline bool pop(T& out) {
            node* tmp_head = head->getNext();
            if (tmp_head == nullptr) {
                return false;
            }
            else {
                T& obj = reinterpret_cast<T&>(head->data);
                out = std::move(obj);
                obj.~T();
                head->resetRelaxed();
                buffer_tail->setNext(head);
                buffer_tail = head;
                head = tmp_head;
                return true;
            }
        }
    };
}
