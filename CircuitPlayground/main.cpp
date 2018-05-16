#include <exception>
#include <iostream>

#include "mainwindow.hpp"


int main( int argc, char * argv[] ) {
    try {
        MainWindow main_window;
        main_window.start(); // this method will block until the window closes (or some exception is thrown)
    }
    catch (const std::exception& err) {
        std::cerr << "Error thrown out of MainWindow:  " << err.what() << std::endl;
        return 1;
    }

    return 0;
}
