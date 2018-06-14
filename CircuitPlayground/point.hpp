#pragma once
/**
 * A very simple point class
 */

#include <cstddef>
#include <algorithm>

#include "integral_division.hpp"

namespace extensions {
    struct point {
        int32_t x, y;

        // + and - compound assignment operators
        point& operator+=(const point& other) noexcept {
            x += other.x;
            y += other.y;
            return *this;
        }
        point& operator-=(const point& other) noexcept {
            x -= other.x;
            y -= other.y;
            return *this;
        }
        point& operator*=(const int32_t& scale) noexcept {
            x *= scale;
            y *= scale;
            return *this;
        }
        point& operator/=(const int32_t& scale) noexcept {
            x /= scale;
            y /= scale;
            return *this;
        }

        // arithmetic operators
        point operator+(const point& other) const noexcept {
            point ret = *this;
            ret += other;
            return ret;
        }
        point operator-(const point& other) const noexcept {
            point ret = *this;
            ret -= other;
            return ret;
        }
        point operator*(const int32_t& scale) const noexcept {
            point ret = *this;
            ret *= scale;
            return ret;
        }
        point operator/(const int32_t& scale) const noexcept {
            point ret = *this;
            ret *= scale;
            return ret;
        }

        // unary operators
        point operator+() const noexcept {
            return *this;
        }
        point operator-() const noexcept {
            return { -x, -y };
        }

        // equality operators
        bool operator==(const point& other) const noexcept {
            return x == other.x && y == other.y;
        }
        bool operator!=(const point& other) const noexcept {
            return !(*this == other);
        }

    };

    // integral division functions
    inline point div_floor(point pt, int32_t scale) noexcept {
        return { extensions::div_floor(pt.x, scale), extensions::div_floor(pt.y, scale) };
    }
    inline point div_ceil(point pt, int32_t scale) noexcept {
        return { extensions::div_ceil(pt.x, scale), extensions::div_ceil(pt.y, scale) };
    }

    inline point min(const point& pt1, const point& pt2) noexcept {
        return { std::min(pt1.x, pt2.x), std::min(pt1.y, pt2.y) };
    }
    inline point max(const point& pt1, const point& pt2) noexcept {
        return { std::max(pt1.x, pt2.x), std::max(pt1.y, pt2.y) };
    }
}
