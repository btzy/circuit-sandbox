#pragma once

#include <utility>
#include <algorithm> // for std::copy and std::move

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
            if (_width == 0 || _height == 0) {
                buffer = nullptr;
                _width = 0;
                _height = 0;
            }
            else {
                buffer = new T[_width * _height]; // reserve the heap array
            }
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
        size_t width() const {
            return _width;
        }

        /**
        * returns the height of the matrix
        */
        size_t height() const {
            return _height;
        }

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

        template <typename TSrc, typename TDest>
        friend inline void copy_range(const heap_matrix<TSrc>& src, heap_matrix<TDest>& dest, size_t src_x, size_t src_y, size_t dest_x, size_t dest_y, size_t width, size_t height);

        template <typename TSrc, typename TDest>
        friend inline void move_range(heap_matrix<TSrc>& src, heap_matrix<TDest>& dest, size_t src_x, size_t src_y, size_t dest_x, size_t dest_y, size_t width, size_t height);
    };

    /**
    * Copies all the data in a rectangle in the source matrix to a rectangle in the destination matrix
    * @pre the rectangles should be within the bounds of their respective matrices
    */
    template <typename TSrc, typename TDest>
    inline void copy_range(const heap_matrix<TSrc>& src, heap_matrix<TDest>& dest, size_t src_x, size_t src_y, size_t dest_x, size_t dest_y, size_t width, size_t height) {
        const TSrc* src_buffer = src.buffer;
        TDest* dest_buffer = dest.buffer;
        for (size_t i = 0; i < height; ++i) {
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
    inline void move_range(heap_matrix<TSrc>& src, heap_matrix<TDest>& dest, size_t src_x, size_t src_y, size_t dest_x, size_t dest_y, size_t width, size_t height) {
        TSrc* src_buffer = src.buffer + src_y * src._width;
        TDest* dest_buffer = dest.buffer + dest_y * dest._width;
        for (size_t i = 0; i < height; ++i) {
            std::move(src_buffer + src_x, src_buffer + src_x + width, dest_buffer + dest_x);
            src_buffer += src._width;
            dest_buffer += dest._width;
        }
    }
}
