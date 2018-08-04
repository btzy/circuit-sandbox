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


#else


// browser launching not supported :(
// TODO: add support for other platforms
namespace WebResource {
    bool launch(const url_char_t* url) {
        return false;
    }
}


#endif
