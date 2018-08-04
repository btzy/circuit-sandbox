#pragma once

#include "declarations.hpp"

namespace WebResource {
    using url_char_t = char;
    constexpr inline const url_char_t* USER_MANUAL = "https://btzy.github.io/circuit-sandbox/manuals/" CIRCUIT_SANDBOX_VERSION_STRING ".pdf";

    /**
     * Launch a web page.  Returns true if launch was successful.
     */
    bool launch(const url_char_t* url);
}
