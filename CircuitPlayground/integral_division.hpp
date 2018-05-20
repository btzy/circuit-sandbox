#pragma once

#include <cstdlib>

namespace extensions {

    // division towards negative infinity
    template <typename T>
    inline T div_floor(T numerator, T divisor) {
        auto div_result = std::div(numerator, divisor);
        T ans = div_result.quot;
        if (div_result.rem < 0)--ans;
        return ans;
    }

    // division towards positive infinity
    template <typename T>
    inline T div_ceil(T numerator, T divisor) {
        auto div_result = std::div(numerator, divisor);
        T ans = div_result.quot;
        if (div_result.rem > 0)++ans;
        return ans;
    }

}
