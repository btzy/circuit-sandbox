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

#include <utility>
#include <algorithm> // for std::copy, std::move, std::reverse, std::swap_ranges

#include "algorithm.hpp"
#include "point.hpp"

/**
 * Represents a generic dynamically-allocated 2D array.
 * The size of the 2d array can be set at runtime, but it is not growable.
 */

namespace ext {

    template <typename T>
    class heap_matrix {

    private:
        T* buffer; // pointer to the buffer that stores all the data; each row is a continguous range of elements
        int32_t _width;
        int32_t _height;

    public:

        friend inline void swap(heap_matrix& a, heap_matrix& b) noexcept {
            using std::swap;

            swap(a.buffer, b.buffer);
            swap(a._width, b._width);
            swap(a._height, b._height);
        }

        heap_matrix() noexcept : buffer(nullptr), _width(0), _height(0) {}

        heap_matrix(const heap_matrix& other) : heap_matrix(other._width, other._height) {
            copy_range(other, *this, 0, 0, 0, 0, _width, _height);
        }
        heap_matrix& operator=(const heap_matrix& other) {
            if (_width != other._width || _height != other._height) {
                if (!empty()) {
                    delete[] buffer;
                }
                _width = other._width;
                _height = other._height;
                if (!other.empty()) {
                    buffer = new T[_width * _height];
                }
                else {
                    buffer = nullptr;
                }
            }
            copy_range(other, *this, 0, 0, 0, 0, _width, _height);
            return *this;
        }

        // noexcept is used to enforce move semantics when heap_matrix is used with STL containers
        heap_matrix(heap_matrix&& other) noexcept :buffer(other.buffer), _width(other._width), _height(other._height) {
            other.buffer = nullptr;
            other._width = 0;
            other._height = 0;
        }
        heap_matrix& operator=(heap_matrix&& other) noexcept {
            swap(*this, other);
            return *this;
        }

        ~heap_matrix() noexcept {
            if (buffer != nullptr) {
                delete[] buffer; // delete the heap array if it isn't nullptr
            }
        }

        heap_matrix(int32_t width, int32_t height): _width(width), _height(height) {
            if (_width == 0 || _height == 0) {
                buffer = nullptr;
                _width = 0;
                _height = 0;
            }
            else {
                buffer = new T[_width * _height]; // reserve the heap array
            }
        }

        heap_matrix(const ext::point& size) : heap_matrix(size.x, size.y) {}

        /**
         * returns true if the matrix is empty (i.e. has no width and height)
         */
        bool empty() const noexcept {
            return buffer == nullptr;
        }

        /**
         * returns the width of the matrix
         */
        int32_t width() const noexcept {
            return _width;
        }

        /**
         * returns the height of the matrix
         */
        int32_t height() const noexcept {
            return _height;
        }

        /**
         * fills all elements in the matrix with the same value
         */
        void fill(const T& value) {
            std::fill_n(buffer, _width * _height, value);
        }

        /**
         * returns the size of the matrix
         */
        ext::point size() const noexcept {
            return { _width, _height };
        }

        /**
         * returns true if the point is within the bounds of the matrix
         */
        bool contains(const ext::point& pt) const noexcept {
            return ext::contains(0, _width, pt.x) && ext::contains(0, _height, pt.y);
        }

        /**
         * returns true if rectangle overlaps with the matrix
         * note that bottomRight is past-the-end
         * assumes that the rectangle specified has a non-negative size
         */
        bool overlaps(const ext::point& topLeft, const ext::point& bottomRight) const noexcept {
            return ext::overlaps(0, _width, topLeft.x, bottomRight.x) && ext::overlaps(0, _height, topLeft.y, bottomRight.y);
        }

        /**
         * indices is a pair of {x,y}
         * @pre indices must be within the bounds of width and height
         */
        T& operator[](const point& indices) noexcept {
            return buffer[indices.y * _width + indices.x];
        }

        /**
         * indices is a pair of {x,y}
         * @pre indices must be within the bounds of width and height
         */
        const T& operator[](const point& indices) const noexcept {
            return buffer[indices.y * _width + indices.x];
        }

        /**
         * Flip about a vertical line in the middle of the matrix
         */
        void flipHorizontal() {
            const T* const end = buffer + _width * _height;
            for (T* start = buffer; start != end; start += _width) {
                std::reverse(start, start + _width);
            }
        }

        /**
         * Flip about a horizontal line in the middle of the matrix
         */
        void flipVertical() {
            const T* const end = buffer + _width * (_height - 1);
            const T* const mid = buffer + _width * (_height / 2);
            for (T* start = buffer; start != mid; start+=_width) {
                std::swap_ranges(start, start + _width, end - start + buffer);
            }
        }

        T* begin() noexcept {
            return buffer;
        }

        const T* begin() const noexcept {
            return buffer;
        }

        T* end() noexcept {
            return buffer + _width * _height;
        }

        const T* end() const noexcept {
            return buffer + _width * _height;
        }

        template <typename TSrc, typename TDest>
        friend inline void copy_range(const heap_matrix<TSrc>& src, heap_matrix<TDest>& dest, int32_t src_x, int32_t src_y, int32_t dest_x, int32_t dest_y, int32_t width, int32_t height);

        template <typename TSrc, typename TDest>
        friend inline void move_range(heap_matrix<TSrc>& src, heap_matrix<TDest>& dest, int32_t src_x, int32_t src_y, int32_t dest_x, int32_t dest_y, int32_t width, int32_t height);

        template <typename TSrc, typename TDest>
        friend inline void swap_range(heap_matrix<TSrc>& src, heap_matrix<TDest>& dest, int32_t src_x, int32_t src_y, int32_t dest_x, int32_t dest_y, int32_t width, int32_t height);
    };

    /**
     * Copies all the data in a rectangle in the source matrix to a rectangle in the destination matrix
     * @pre the rectangles should be within the bounds of their respective matrices
     */
    template <typename TSrc, typename TDest>
    inline void copy_range(const heap_matrix<TSrc>& src, heap_matrix<TDest>& dest, int32_t src_x, int32_t src_y, int32_t dest_x, int32_t dest_y, int32_t width, int32_t height) {
        const TSrc* src_buffer = src.buffer + src_y * src._width;
        TDest* dest_buffer = dest.buffer + dest_y * dest._width;
        for (int32_t i = 0; i < height; ++i) {
            std::copy(src_buffer + src_x, src_buffer + src_x + width, dest_buffer + dest_x);
            src_buffer += src._width;
            dest_buffer += dest._width;
        }
    }

    /**
     * Moves all the data in a rectangle in the source matrix to a rectangle in the destination matrix
     * @pre the rectangles should be within the bounds of their respective matrices
     */
    template <typename TSrc, typename TDest>
    inline void move_range(heap_matrix<TSrc>& src, heap_matrix<TDest>& dest, int32_t src_x, int32_t src_y, int32_t dest_x, int32_t dest_y, int32_t width, int32_t height) {
        TSrc* src_buffer = src.buffer + src_y * src._width;
        TDest* dest_buffer = dest.buffer + dest_y * dest._width;
        for (int32_t i = 0; i < height; ++i) {
            std::move(src_buffer + src_x, src_buffer + src_x + width, dest_buffer + dest_x);
            src_buffer += src._width;
            dest_buffer += dest._width;
        }
    }

    template <typename TSrc, typename TDest>
    inline void swap_range(heap_matrix<TSrc>& src, heap_matrix<TDest>& dest, int32_t src_x, int32_t src_y, int32_t dest_x, int32_t dest_y, int32_t width, int32_t height) {
        TSrc* src_buffer = src.buffer + src_y * src._width;
        TDest* dest_buffer = dest.buffer + dest_y * dest._width;
        for (int32_t i = 0; i < height; ++i) {
            std::swap_ranges(src_buffer + src_x, src_buffer + src_x + width, dest_buffer + dest_x);
            src_buffer += src._width;
            dest_buffer += dest._width;
        }
    }
}
