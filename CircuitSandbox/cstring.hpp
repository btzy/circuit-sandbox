#pragma once

/**
 * Additional utilities for C-strings.
 */

namespace ext {
    /**
     * Returns a pointer to the first space on or after the given pointer, or a pointer to the end of the string if no space can be found.
     */
    inline const char* next_space(const char* given) noexcept {
        for (; *given; ++given) {
            if (*given == ' ') {
                return given;
            }
        }
        return given;
    }

    /**
     * Returns a pointer to the first non-space on or after the given pointer, or a pointer to the end of the string if no non-space can be found.
     */
    inline const char* next_non_space(const char* given) noexcept {
        for (; *given; ++given) {
            if (*given != ' ') {
                return given;
            }
        }
        return given;
    }
}