#pragma once

/**
 * Collection adapter that reverses the order of elemenets.
 * https://stackoverflow.com/a/28139075/1021959
 */

#include <iterator>

namespace ext {

    template <typename T>
    struct reversion_wrapper {
        T& iterable;
    };

    template <typename T>
    auto begin(reversion_wrapper<T> w) {
        return std::rbegin(w.iterable);
    }

    template <typename T>
    auto end(reversion_wrapper<T> w) {
        return std::rend(w.iterable);
    }

    template <typename T>
    reversion_wrapper<T> reverse(T&& iterable) {
        return { iterable };
    }

}
