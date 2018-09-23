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
