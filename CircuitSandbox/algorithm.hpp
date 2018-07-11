#pragma once

/*
 * Additional general-purpose algorithms.
 */

namespace ext {
    // tests if `target` is in the range [low, high)
    template <typename T>
    inline bool contains(const T& low, const T& high, const T& target) {
        return low <= target && target < high;
    }

    // tests if range [low2, high2) overlaps the range [low1, high1)
    template <typename T>
    inline bool overlaps(const T& low1, const T& high1, const T& low2, const T& high2) {
        return low1 < high2 && low2 < high1;
    }
}
