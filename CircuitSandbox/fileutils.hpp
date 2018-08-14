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
