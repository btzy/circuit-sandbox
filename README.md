<div align="center">
  <img src="https://btzy.github.io/orbital/Circuit%20Sandbox.png"><br/><br/>
</div>

**Circuit Sandbox** is a digital circuit sandbox game where users can create arbitrarily complex circuits out of simple components.  Circuit Sandbox is designed from the ground up to support large and complex simulations efficiently.

Circuit Sandbox is currently in development.

## Download binaries

Pre-built binaries are [available for download](https://github.com/btzy/circuit-sandbox/releases) for Windows 10.  They should run straight out of the box, no installation required.

If you are on Windows 8.1, you might need to install the [Windows 10 Universal C Runtime](https://www.microsoft.com/en-us/download/details.aspx?id=48234) before running Circuit Sandbox.

For other operating systems, you'll have to build it from source.

## How to play

Read the [user manual](https://btzy.github.io/circuit-sandbox/manuals/latest.pdf) for an introduction to Circuit Sandbox.

There are also sample circuits in the `samples` folder of this repository.

## Build from source

A reasonably conforming C++17 compiler is required to compile Circuit Sandbox.

Circuit Sandbox has been tested to compile with Visual Studio 2017 (MSVC 15.7) on Windows and GCC 7.3 on Linux.  It might also work with MinGW-w64 on Windows, but a few tweaks might be needed to get it use newer versions of `Windows.h`.

It probably also compiles on Mac OSX with Clang, but this is untested.

*Note: The C runtime has to be linked dynamically due to how Native File Dialog works.*

### Dependencies

* [Boost libraries](https://www.boost.org/)
  * Headers: Tribool, Process, Endian
  * Link Libraries: System, Filesystem
* [SDL2](https://www.libsdl.org/download-2.0.php)
* [SDL2_ttf](https://www.libsdl.org/projects/SDL_ttf/)
* [Native File Dialog](https://github.com/mlabbe/nativefiledialog)

Remember to add the relevant compiler and linker settings so that the `#include`s and link libraries are discoverable.
