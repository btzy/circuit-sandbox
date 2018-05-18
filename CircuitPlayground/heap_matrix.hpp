#pragma once

#include <utility>

/**
 * Represents a generic dynamically-allocated 2D array.
 * The size of the 2d array can be set at runtime, but it is not growable.
 */

namespace extensions {

    template <typename T>
    class heap_matrix {

    private:
        T* buffer; // pointer to the buffer that stores all the data; each row is a continguous range of elements
        size_t _width;
        size_t _height;

    public:

        friend inline void swap(heap_matrix& a, heap_matrix& b) {
            using std::swap;

            swap(a.buffer, b.buffer);
            swap(a._width, b._width);
            swap(a._height, b._height);
        }

        heap_matrix() : buffer(nullptr), _width(0), _height(0) {}

        // don't think these are needed, but can easily be written if needed
        heap_matrix(const heap_matrix&) = delete;
        heap_matrix& operator=(const heap_matrix&) = delete;

        heap_matrix(heap_matrix&& other) :buffer(other.buffer), _width(other.width), _height(other.height) {
            other.buffer = nullptr;
            other._width = 0;
            other._height = 0;
        }
        heap_matrix& operator=(heap_matrix&& other) {
            swap(*this, other);
            return *this;
        }

        ~heap_matrix() {
            if (buffer != nullptr) {
                delete[] buffer; // delete the heap array if it isn't nullptr
            }
        }

        heap_matrix(size_t width, size_t height): _width(width), _height(height) {
            buffer = new T[_width * _height]; // reserve the heap array
        }

        /**
        * returns true if the matrix is empty (i.e. has no width and height)
        */
        bool empty() const {
            return buffer == nullptr;
        }

        /**
        * returns the width of the matrix
        */
        /*size_t width() const {
            return _width;
        }*/

        /**
         * indices is a pair of {x,y}
         * @pre indices must be within the bounds of width and height
         */
        T& operator[](const std::pair<size_t, size_t>& indices) {
            return buffer[indices.second * _width + indices.first];
        }

        /**
        * indices is a pair of {x,y}
        * @pre indices must be within the bounds of width and height
        */
        const T& operator[](const std::pair<size_t, size_t>& indices) const {
            return buffer[indices.second * _width + indices.first];
        }
    };
}
