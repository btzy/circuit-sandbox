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
    }
    ~InitGuard() {
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
            main_window.loadFile(givenFilePath);
        }
        main_window.start(); // this method will block until the window closes (or some exception is thrown)
    }
    catch (const std::exception& err) {
        std::cerr << "Error thrown out of MainWindow:  " << err.what() << std::endl;
        return 1;
    }

    return 0;
}
