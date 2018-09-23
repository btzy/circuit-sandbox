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
