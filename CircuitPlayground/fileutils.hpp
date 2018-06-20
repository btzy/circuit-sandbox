#pragma once

#include <algorithm>

constexpr const char* CCPG_FILE_MAGIC = "CCPG";
constexpr const char* CCPG_FILE_EXTENSION = "ccpg";

/**
 * Add extension to the given file name if no extension is specified
 * buffer have space for at least (strlen(given) + 6) characters (including trailing null character)
 */
inline const char* addExtensionIfNecessary(const char* given, char* buffer) {
    const char* givenEnd = given;
    while (*givenEnd != '\0') {
        ++givenEnd;
    }
    const char* simpleNameStart = givenEnd;
    while (simpleNameStart != given && *(simpleNameStart - 1) != '/' && *(simpleNameStart - 1) != '\\') {
        --simpleNameStart;
    }
    const char* dot = simpleNameStart;
    while (dot != givenEnd && *dot != '.') {
        ++dot;
    }
    if (dot != givenEnd) {
        // no need to add extension
        return given;
    }
    else {
        // add extension
        char* bufferEnd = std::copy(given, givenEnd, buffer);
        *bufferEnd++ = '.';
        bufferEnd = std::copy(CCPG_FILE_EXTENSION, CCPG_FILE_EXTENSION + 4, bufferEnd);
        *bufferEnd = '\0';
        return buffer;
    }
}