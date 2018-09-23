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
#include <numeric> // for std::reduce, std::accumulate
#include <utility> // for std::declval

namespace std {
    template <typename...> void reduce();
}

namespace ext {
    template <typename InputIt, typename T, typename BinaryOp, typename = std::void_t<>>
    struct has_std_reduce : std::false_type {};

    template <typename InputIt, typename T, typename BinaryOp>
    struct has_std_reduce<InputIt, T, BinaryOp, std::void_t<decltype(std::reduce(std::declval<InputIt>(), std::declval<InputIt>(), std::declval<T>(), std::declval<BinaryOp>()))>> : std::true_type {};

    template <typename InputIt, typename T, typename BinaryOp>
    inline T reduce(InputIt first, InputIt last, T init, BinaryOp&& binary_op) {
        if constexpr(has_std_reduce<InputIt, T, BinaryOp>::value) {
            return std::reduce(first, last, init, std::forward<BinaryOp>(binary_op));
        }
        else {
            return std::accumulate(first, last, init, std::forward<BinaryOp>(binary_op));
        }
    }
}
