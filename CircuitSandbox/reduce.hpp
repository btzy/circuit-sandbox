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
