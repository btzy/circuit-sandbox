/*
 * Circuit Sandbox
 * Copyright 2018 National University of Singapore <enterprise@nus.edu.sg>
 *
 * This file is part of Circuit Sandbox.
 * Circuit Sandbox is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 * Circuit Sandbox is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with Circuit Sandbox.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "launch_browser.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>

namespace WebResource {
    bool launch(const url_char_t* url) {
        auto ret = ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOW);
        return reinterpret_cast<int&>(ret) > 32;
    }
}


#elif defined(__APPLE__)
// don't do anything, because theres an objective-c++ file for apple bindings
#else


// browser launching not supported :(
// TODO: add support for other platforms
namespace WebResource {
    bool launch(const url_char_t* url) {
        return false;
    }
}


#endif
