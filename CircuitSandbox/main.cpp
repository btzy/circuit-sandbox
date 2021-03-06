/*
 * Circuit Sandbox
 * Copyright 2018 National University of Singapore <enterprise@nus.edu.sg>
 *
 * This file is part of Circuit Sandbox.
 * Circuit Sandbox is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 * Circuit Sandbox is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with Circuit Sandbox.  If not, see <https://www.gnu.org/licenses/>.
 */

#if defined(_WIN32) && defined(_MSC_VER)
// this enables the visual styles for the popup dialog boxes
// see https://msdn.microsoft.com/en-us/library/windows/desktop/bb773175.aspx
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#include <exception>
#include <stdexcept>
#include <iostream>
#include <string>

#include <SDL.h>
#include <SDL_ttf.h>
#include <nfd.h>

#include "mainwindow.hpp"


using namespace std::literals::string_literals; // gives the 's' suffix for strings

// guard object used to initialize and de-initialize program-wide stuff
struct InitGuard {
    InitGuard() {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            throw std::runtime_error("SDL_Init() failed:  "s + SDL_GetError());
        }

        if (TTF_Init() != 0) {
            throw std::runtime_error("TTF_Init() failed:  "s + TTF_GetError());
        }

        if (NFD_Init() != NFD_OKAY) {
            throw std::runtime_error("NFD_Init() failed:  "s + NFD_GetError());
            // Note 1: NFD_Init() does `gtk_init_check(nullptr, nullptr)` on Linux, without which UTF-8 characters in the title bar won't work.
            // Node 2: NFD_Init() does `CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);` on Windows, which might be required for launching the browser using ShellExecuteA().
        }
    }
    ~InitGuard() {
        // quit everything in the reverse order of initialization
        NFD_Quit();
        TTF_Quit();
        SDL_Quit();
    }
};

int main(int argc, char* argv[]) {
    try {
        InitGuard init_guard; // this ensures that all the program-wide init and de-init works even if exceptions are thrown
        MainWindow main_window(argv[0]);
        if (argc >= 2) {
            // argv[1] is the file name (if it exists)]
            char* givenFilePath = argv[1];
            main_window.start(givenFilePath); // start... with given file path
        }
        else {
            main_window.start(); // this method will block until the window closes (or some exception is thrown)
        }
    }
    catch (const std::exception& err) {
        std::cerr << "Error thrown out of MainWindow:  " << err.what() << std::endl;
        return 1;
    }

    return 0;
}
