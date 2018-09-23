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

#include <type_traits>
#include <utility>
#include <array>
#include <cstddef>
#include <cassert>

/**
 * An implementation of unrolled linked list queue,
 * which minimizes memory allocation and reuses old buffers where possible.
 * It is not thread safe.
 * BufferSize = number of elements in each block of memory.
 */

namespace ext {
    template <typename T, size_t NodeSize>
    class unrolled_linked_list_queue {
    private:
        static_assert(NodeSize > 0);

        struct node {
            std::array<std::aligned_storage_t<sizeof(T), alignof(T)>, NodeSize> data;
            node* next;
        };

        // the next place to pop
        node* front_node;
        size_t front_index; // in range [0, NodeSize)

        // the next place to push (currently empty)
        node* back_node;
        size_t back_index; // in range [0, NodeSize)

        // empty buffers
        node* unused_node;

        inline node* get_new_or_unused_node() {
            if (unused_node != nullptr) {
                return std::exchange(unused_node, unused_node->next);
            }
            else {
                return new node;
            }
        }

        inline void keep_unused_node(node* obj) noexcept {
            obj->next = unused_node;
            unused_node = obj;
        }

    public:
        unrolled_linked_list_queue() {
            back_node = front_node = new node;
            back_index = front_index = 0;
            unused_node = nullptr;
        }

        unrolled_linked_list_queue(const unrolled_linked_list_queue&) = delete;
        unrolled_linked_list_queue& operator=(const unrolled_linked_list_queue&) = delete;

        unrolled_linked_list_queue(unrolled_linked_list_queue&&) = delete;
        unrolled_linked_list_queue& operator=(unrolled_linked_list_queue&&) = delete;

        ~unrolled_linked_list_queue() {
            while (!empty()) {
                pop();
            }
            assert(front_node == back_node);
            delete front_node;
            while (unused_node != nullptr) {
                delete std::exchange(unused_node, unused_node->next);
            }
        }

        template <typename... Args>
        T& emplace(Args&&... args) {
            T* element = new (&(back_node->data[back_index])) T(std::forward<Args>(args)...);
            ++back_index;
            if (back_index == NodeSize) {
                node* new_node = get_new_or_unused_node();
                back_node->next = new_node;
                back_node = new_node;
                back_index = 0;
            }
            return *element;
        }

        void push(const T& t) {
            emplace(t);
        }

        void push(T&& t) {
            emplace(std::move(t));
        }

        void pop() {
            assert(!empty());
            T& element = reinterpret_cast<T&>(front_node->data[front_index]);
            element.~T();
            ++front_index;
            if (front_index == NodeSize) {
                node* new_node = front_node->next;
                keep_unused_node(front_node);
                front_node = new_node;
                front_index = 0;
            }
        }

        T& front() noexcept {
            assert(!empty());
            return reinterpret_cast<T&>(front_node->data[front_index]);
        }

        const T& front() const noexcept {
            assert(!empty());
            return reinterpret_cast<const T&>(front_node->data[front_index]);
        }

        bool empty() const noexcept {
            return front_index == back_index && front_node == back_node;
        }
    };
}
