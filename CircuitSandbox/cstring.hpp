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