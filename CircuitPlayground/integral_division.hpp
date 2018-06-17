#pragma once

#include <cstdlib>

namespace extensions {

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
