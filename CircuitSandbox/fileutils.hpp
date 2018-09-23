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

#include <algorithm>

#define CCSB_FILE_MAGIC "CCPG"
#define CCSB_FILE_EXTENSION "ccsb"
#define CCSB_FILE_FRIENDLY_NAME "Circuit Sandbox save"

/**
 * Returns a pointer to the first character after the last '/' or '\\'
 */
inline const char* getFileName(const char* given) {
    const char* givenEnd = given;
    while (*givenEnd != '\0') {
        ++givenEnd;
    }
    const char* simpleNameStart = givenEnd;
    while (simpleNameStart != given && *(simpleNameStart - 1) != '/' && *(simpleNameStart - 1) != '\\') {
        --simpleNameStart;
    }
    return simpleNameStart;
}
