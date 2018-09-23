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

#include <cstdlib>

namespace ext {

    // division towards negative infinity
    // requires divisor to be positive
    template <typename T>
    inline T div_floor(T numerator, T divisor) noexcept {
        auto div_result = std::div(numerator, divisor);
        T ans = div_result.quot;
        if (div_result.rem < 0)--ans;
        return ans;
    }

    // division towards positive infinity
    // requires divisor to be positive
    template <typename T>
    inline T div_ceil(T numerator, T divisor) noexcept {
        auto div_result = std::div(numerator, divisor);
        T ans = div_result.quot;
        if (div_result.rem > 0)++ans;
        return ans;
    }

    // division with rounding (0.5 rounds toward positive infinity)
    // requires divisor to be positive
    template <typename T>
    inline T div_round(T numerator, T divisor) noexcept {
        numerator += divisor / 2;
        return div_floor(numerator, divisor);
    }

}
